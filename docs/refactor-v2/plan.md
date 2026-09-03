# 协程运行时重构 V2 技术设计

## 架构总览

新运行时与现有 `fiber_lib/` 并行存在，根目录提供唯一 CMake 入口。运行时由以下组件组成：

```text
应用代码
   |
   +-- Coroutine / Scheduler API
   |       |
   |       +-- LoadBalancer：选择目标工作线程
   |       +-- Worker：本地队列、执行上下文、工作窃取
   |       +-- IngressQueue：按工作线程划分的无锁 MPMC 入口
   |       +-- MetricsCollector：周期性负载快照
   |       +-- CpuAffinity：可选线程亲和性绑定
   |
   +-- Hook API
           |
           +-- FdRegistry：文件描述符状态和用户超时
           +-- EventLoop：epoll 就绪/取消/超时事件
           +-- TimerQueue：单调时钟定时器
           +-- Coroutine wait/resume
```

调度器采用固定数量的工作线程。每个工作线程拥有一个只能由所属线程直接弹出、其他线程可以窃取的本地双端队列，以及一个只对应本线程的无锁 MPMC 入口。跨线程提交不直接修改目标线程的本地队列，而是进入目标工作线程的入口，再由目标线程转移到本地队列。工作线程依次处理本地任务、自己的入口和窃取任务，最终通过事件循环或条件变量等待。

协程只在挂起点之间迁移，不允许在协程正在执行时迁移。协程的最后运行线程记录在协程元数据中；恢复任务默认优先回到该线程，只有该线程超过亲和性阈值、已退出或无法接收任务时才由负载均衡器重新选择。

## 核心数据结构

### `RuntimeConfig`

运行时配置包含：

- `worker_count`：0 表示使用 `std::thread::hardware_concurrency()`；结果为 0 时回退为 1。
- `strategy`：`round_robin`、`least_load`、`affinity_first`、`adaptive`。
- `metrics_interval`：指标采集周期，默认 100 ms。
- `affinity_max_load`：亲和性线程允许的最大负载分数，默认 0.7。
- `overload_threshold` 和 `idle_threshold`：默认分别为 0.9 和 0.2。
- `queue_depth_weight`、`cpu_usage_weight`、`wait_time_weight`、`history_weight`、`affinity_bonus`：自适应策略权重，默认队列 0.40、CPU 0.30、等待时间 0.20、历史 0.10；亲和性奖励作为独立的分数扣减项。
- `stack_initial_size`、`stack_max_size`、`stack_pool_enabled`：协程栈初始大小、上限和栈池开关。
- `pin_workers`：是否尝试绑定工作线程到不同的在线 CPU。

配置解析只接受已知字段，非法值在启动时返回明确错误；未配置字段使用上述默认值。

### `Coroutine`

协程对象保存上下文、栈句柄、状态和调度元数据：

- 状态：`ready`、`running`、`waiting`、`finished`、`cancelled`。
- `last_worker`：上次执行的逻辑工作线程编号，-1 表示尚未运行。
- `wait_reason`：I/O、定时器或外部取消。
- `resume_generation`：防止同一次等待被就绪和超时路径重复恢复。
- `ucontext_t` 及栈分配器返回的栈内存。

协程上下文仅由当前拥有它的工作线程恢复；事件循环回调只生成一次可运行任务，不直接调用 `resume()`。

### `RunnableTask`

任务节点包含协程或一次性回调、目标线程提示、入队时间戳和取消标记。节点使用稳定地址，队列只传递节点指针；任务节点的所有权由队列取出后转移给执行线程，执行结束后释放。`Task` 是公开的可移动任务描述，内部转换为 `RunnableTask`；空任务提交返回 `invalid_task`。

### `WorkerLoadSnapshot`

每个工作线程维护可原子读取的指标：

- `worker_id`、`cpu_id`；
- `queue_depth`；
- `cpu_usage`，由工作线程的 CPU 纳秒计数和墙上时间增量计算，范围为 0 到 1；
- `completed_count`；
- `total_queue_wait_us`；
- `running`、`idle`、`last_update`。

指标更新使用原子计数器和双缓冲快照。采集线程不读取其他线程的 `RUSAGE_THREAD`；工作线程在执行循环中发布 CPU 时间，采集线程只负责按时间窗计算比例和复制快照。

### `FdContext` 与 `WaitRegistration`

`FdContext` 保存文件描述符是否为 socket、是否关闭、用户是否设置非阻塞、读写超时和当前注册事件。每一个等待操作对应一个 `WaitRegistration`，包含协程弱引用、事件类型、截止时间和原子完成状态。

同一文件描述符的读等待和写等待可以同时存在；同一方向重复注册返回错误。就绪、超时、关闭和取消路径通过原子状态竞争，只有获胜者安排协程恢复。

### `TimerEntry`

定时器使用单调时钟绝对截止时间、回调、周期、取消状态和序列号。定时器队列按截止时间排序，事件循环等待时间取最近截止时间与配置上限的较小值。

### `RuntimeMetrics`

运行时导出不可变快照：所有 `WorkerLoadSnapshot`、采样时间、完成总量、窃取次数、提交次数、上下文切换计数和等待中的 I/O 数量。调用者获得值拷贝，不持有运行时内部锁或指针。

## 核心接口

### `Scheduler`

以下是接口使用的结果和任务类型：

```cpp
enum class SubmitResult { accepted, stopped, invalid_worker, invalid_task, queue_full };
enum class SelectionError { empty_snapshot, invalid_configuration };
enum class Event { read, write };
enum class EventResult { accepted, duplicate, closed, invalid_fd };
enum class CancelReason { timeout, closed, shutdown, explicit_cancel };
using Duration = std::chrono::milliseconds;
using TimePoint = std::chrono::steady_clock::time_point;
using TimerHandle = uint64_t;
using TimerCallback = std::function<void()>;
struct SelectionResult {
    std::optional<size_t> worker;
    SelectionError error;
    explicit operator bool() const noexcept { return worker.has_value(); }
};
using Task = std::function<void()>;
struct SubmitOptions {
    std::optional<size_t> affinity_worker;
};
struct WaitRegistration {
    std::weak_ptr<Coroutine> coroutine;
    Event event;
    TimePoint deadline;
    std::shared_ptr<std::atomic<bool>> completed;
};
```

```cpp
class Scheduler {
public:
    explicit Scheduler(RuntimeConfig config = {});
    ~Scheduler();

    void start();
    void stop();
    bool is_running() const noexcept;

    SubmitResult submit(Task task, SubmitOptions options = {});
    SubmitResult submit_to(size_t worker, Task task);

    RuntimeConfig config() const;
    std::vector<WorkerLoadSnapshot> load_snapshot() const;
    RuntimeMetrics metrics_snapshot() const;
};
```

`submit` 根据任务的亲和性提示和策略选择工作线程；`submit_to` 仅用于测试和明确的调度场景。停止后提交返回 `stopped`，非法线程编号返回 `invalid_worker`。任务入队成功后唤醒目标线程或事件循环。

### `LoadBalancer`

```cpp
class LoadBalancer {
public:
    explicit LoadBalancer(RuntimeConfig config);
    SelectionResult select(const std::vector<WorkerLoadSnapshot>& snapshot,
                           std::optional<size_t> affinity_worker = std::nullopt) const;
    void update(RuntimeConfig config);
};
```

轮询使用原子索引；最少负载按归一化队列深度、CPU 使用率和等待时间选择最小分数；亲和性优先在首选线程低于阈值时直接选择，否则回退到最少负载；自适应按配置权重综合队列、CPU、等待时间、历史完成量和亲和性奖励。所有策略在空快照时返回 `SelectionError::empty_snapshot`，而非访问无效下标。

### `WorkStealingDeque`

```cpp
class WorkStealingDeque {
public:
    bool push(RunnableTask* task);       // 所属工作线程调用
    RunnableTask* pop();                 // 所属工作线程调用，LIFO
    RunnableTask* steal();               // 其他工作线程调用，FIFO
    size_t approximate_size() const noexcept;
};
```

队列使用固定容量分段的 Chase-Lev 双端队列：所有者操作底部，窃取者操作顶部；扩容段发布后不回收，直到队列销毁，以避免窃取者访问悬空内存。每个工作线程的入口使用无锁 MPMC 链式队列，节点在出队前保持有效，关闭时通过终止标记拒绝新节点；不存在单一的全局共享入口。

### `EventLoop` 与 `TimerQueue`

```cpp
class EventLoop {
public:
    explicit EventLoop(size_t worker_id);
    EventResult add(int fd, Event event, WaitRegistration registration);
    bool cancel(int fd, Event event, CancelReason reason);
    int run_once(std::chrono::milliseconds timeout);
    void wake();
};

class TimerQueue {
public:
    TimerHandle add(Duration delay, TimerCallback callback);
    bool cancel(TimerHandle handle);
    Duration next_timeout() const;
    size_t expire(TimePoint now, std::vector<TimerCallback>& callbacks);
};
```

`EventLoop` 使用 epoll 的边沿触发模式并维护每个 fd 的事件状态。`run_once` 先处理到期定时器，再处理就绪事件，将获胜等待注册转换为调度任务。`wake` 使用 eventfd 或 pipe 使阻塞的 epoll_wait 立即返回。

### Hook 接口

Hook 层保留 libc 兼容函数签名，通过 `dlsym(RTLD_NEXT, ...)` 保存原函数。以下调用统一进入 I/O 等待模板：`read/readv/recv/recvfrom/recvmsg`、`write/writev/send/sendto/sendmsg`、`connect/accept`。`poll` 将每个轮询项转换为读/写等待集合，并用单个截止时间协调多个注册。

睡眠 Hook 将协程注册到 `TimerQueue` 后挂起；剩余时间、`EINTR`、超时和取消按 POSIX 返回约定处理。未启用 Hook、当前线程不在运行时、普通文件描述符、用户显式设置非阻塞或原函数解析失败时调用原始函数。Hook 初始化失败不静默伪造成功，而是保留错误并回退到可报告的原始调用路径。

### `MetricsCollector`

```cpp
class MetricsCollector {
public:
    explicit MetricsCollector(Duration interval = 100ms);
    void start(const std::vector<Worker*>& workers);
    void stop();
    RuntimeMetrics snapshot() const;
};
```

采集器单独运行，每个周期读取工作线程原子计数和队列近似深度，计算 CPU 使用率、等待时间增量和负载标准差，发布不可变快照。停止流程先设置终止标记并唤醒采集器，再等待线程退出，保证析构时无后台访问。

### `CpuAffinity`

```cpp
class CpuAffinity {
public:
    static std::vector<int> online_cpus();
    static bool bind_current_thread(int cpu_id);
};
```

线程启动时按工作线程编号分配在线 CPU 并调用 `pthread_setaffinity_np`。权限不足或 CPU 不可用时记录警告并继续运行，不因可选亲和性失败而阻止服务启动；快照保留实际绑定结果。

## 模块设计

### 协程与栈管理

**职责：** 创建、恢复、挂起、完成和取消有栈协程；维护线程亲和性和等待代数。

**依赖：** `ucontext`、栈分配器、调度器的恢复接口。

**实现约束：** 所有上下文切换前后更新状态；协程函数异常必须转换为完成/失败状态并回到调度上下文，不能穿过 `swapcontext` 边界。M6 增加带 guard page 的动态栈和可选栈池，保留固定策略作为对照基线。

### 队列与调度器

**职责：** 任务分发、本地执行、工作窃取、唤醒和停止。

**依赖：** 协程、负载均衡器、线程封装、指标计数器。

**实现约束：** 正常提交路径不获取全局调度队列互斥锁；目标本地队列由共享入口转移填充。任务取出后先标记为运行中，再更新队列等待指标；停止时关闭入口、唤醒所有工作线程、排空可执行任务并等待线程退出。

### 负载均衡器

**职责：** 根据策略和快照选择工作线程。

**依赖：** `RuntimeConfig`、`WorkerLoadSnapshot`。

**实现约束：** 只读取快照值，不持有工作线程内部锁；所有指标先归一化并限制在有限范围，避免异常队列深度或等待时间导致数值溢出。负载选择与真正入队之间允许快照过期，入队失败时执行一次备用选择。

### 事件循环与定时器

**职责：** 管理 epoll、fd 事件、超时、取消和唤醒。

**依赖：** 协程、调度器、文件描述符注册表。

**实现约束：** 就绪和超时通过一次性注册状态竞争；关闭 fd 时先取消关联等待，再删除 epoll 注册，防止复用 fd 触发旧协程。事件循环只调度恢复任务，不直接恢复协程。

### Hook 与文件描述符注册表

**职责：** 保持 libc ABI，判断是否可挂起，并把 EAGAIN/EINPROGRESS 转成事件等待。

**依赖：** 原始 libc 函数、事件循环、定时器、当前协程上下文。

**实现约束：** 初始化和调用路径避免使用可能再次触发 Hook 的高层 I/O；日志使用已禁用 Hook 的路径。用户非阻塞语义不被改变，超时采用 fd 级读/写超时或调用参数规定的截止时间。

### 指标与基准工具

**职责：** 发布运行时快照、导出 JSON/CSV、运行基线和对比测试。

**依赖：** 运行时快照、Linux `perf`（可选）、压测客户端（可选）。

**实现约束：** 工具不可用时记录 `unavailable` 和原因，不以估算值冒充 perf 指标。每次结果包含主机 CPU、内核、编译器、提交号、命令、重复次数和原始输出。

## 模块交互

### 提交与执行

1. 调用者创建回调任务或协程任务并调用 `Scheduler::submit`。
2. 调度器读取当前负载快照和任务亲和性，调用 `LoadBalancer::select`。
3. 任务进入选定工作线程的共享入口，并通过 eventfd/条件变量唤醒该线程。
4. 工作线程先从本地双端队列取任务；为空则批量转移共享入口；仍为空则从其他工作线程窃取。
5. 工作线程记录开始时间、CPU 计数和 `last_worker`，恢复协程或运行回调。
6. 协程完成则释放任务节点；协程挂起则由 I/O 或定时器持有等待注册，恢复时重新进入调度入口并保留亲和性提示。

### Hook I/O 等待

1. Hook 检查线程 Hook 开关、fd 状态和用户非阻塞标志；不满足挂起条件时调用原函数。
2. 原函数返回 `EINTR` 时按调用约定重试或向上返回；返回 `EAGAIN/EWOULDBLOCK` 时创建等待注册。
3. 事件循环注册 fd 事件和可选超时定时器，然后当前协程转为 `waiting` 并切换回调度上下文。
4. 就绪、超时、关闭或取消中的第一个获胜路径完成注册，设置结果并提交协程恢复任务。
5. 协程恢复后取消另一条路径的定时器/事件，重试原函数或返回 `ETIMEDOUT`、`EBADF` 等标准错误。

### 关闭与失败路径

停止时禁止新提交，唤醒所有工作线程和指标采集线程；工作线程不再接收新任务，已等待的 I/O 被取消并以取消结果恢复，随后释放 fd 注册和协程栈。若线程亲和性绑定、perf 或外部压测工具不可用，功能测试仍可运行，报告明确标注受限指标。

## 需求追踪

| 需求 | 设计落点 | 验证方式 |
|---|---|---|
| F1、N1、N2、N7、N8 | 文件组织、CMake 决策、M0 | 干净 Linux 构建、警告构建、文档完整性和生成物检查 |
| F2 | `RuntimeConfig`、`CpuAffinity::online_cpus` | 默认值和显式线程数单测 |
| F3、F6、N3 | 每线程 Chase-Lev 队列、每线程 MPMC 入口、Worker | 队列并发、TSAN、窃取和唤醒集成测试 |
| F4、F5 | `LoadBalancer` 四种策略及归一化评分 | 受控快照策略单测 |
| F7 | `Coroutine::last_worker`、`SubmitOptions` | 亲和性保持和过载回退测试 |
| F8、F9 | Hook、`EventLoop`、`FdContext`、`WaitRegistration`、`TimerQueue` | 全部系统调用类别的成功、超时、关闭、取消和透传测试 |
| F10、F11 | `WorkerLoadSnapshot`、`RuntimeMetrics`、`MetricsCollector` | 周期采集、快照字段和导出测试 |
| F12 | `tests/unit`、`tests/integration`、`tests/stress` | CTest 全量运行 |
| F13、N5、N6 | `benchmarks`、`benchmarks.md`、`reports` | 同主机基线/对比报告，工具缺失明确标记 |
| F14 | 里程碑交付设计和提交流程 | 每阶段测试证据、提交号和远端分支核验 |
| N4 | `stack_allocator`、M6 容量基准 | 100,000 协程实例和内存报告 |

依赖关系保持单向：协程依赖栈分配器，调度器依赖协程/队列/负载均衡器，Hook 依赖事件循环/定时器/fd 注册表，指标和基准只读取运行时导出的快照。事件循环通过调度器提交恢复任务，不反向持有调度器执行锁，因此不存在模块级循环依赖。

## 文件组织

```text
coroutine-lib/
├── CMakeLists.txt                 # 顶层构建入口
├── CMakePresets.json              # Debug/Release/TSAN 预设
├── README.md                      # 新运行时构建、使用和基准入口
├── docs/
│   └── refactor-v2/
│       ├── spec.md                # 已批准的需求规格
│       ├── plan.md                # 本技术设计
│       ├── task.md                 # 实施任务分解
│       ├── checklist.md             # 验收清单
│       ├── benchmarks.md            # 指标、环境和命令说明
│       └── reports/                 # 版本化的原始/汇总报告
├── include/coroutine/
│   ├── config.h
│   ├── coroutine.h
│   ├── scheduler.h
│   ├── load_balancer.h
│   ├── metrics.h
│   └── hook.h
├── src/
│   ├── coroutine.cpp
│   ├── stack_allocator.cpp
│   ├── scheduler.cpp
│   ├── worker.cpp
│   ├── work_stealing_deque.cpp
│   ├── ingress_queue.cpp
│   ├── load_balancer.cpp
│   ├── metrics_collector.cpp
│   ├── event_loop.cpp
│   ├── timer_queue.cpp
│   ├── fd_registry.cpp
│   ├── hook.cpp
│   ├── poll_hook.cpp
│   ├── thread.cpp
│   └── cpu_affinity.cpp
├── tests/
│   ├── unit/
│   ├── integration/
│   ├── stress/
│   └── CMakeLists.txt
├── benchmarks/
│   ├── baseline_server.cpp
│   ├── scheduler_benchmark.cpp
│   ├── coroutine_capacity.cpp
│   ├── run_benchmarks.sh
│   └── CMakeLists.txt
└── fiber_lib/                      # 原有教学代码，保留不改
```

## 技术决策

| 决策 | 选择 | 原因 |
|---|---|---|
| 协程模型 | 继续使用有栈 `ucontext`，封装恢复边界 | 保持现有项目技术连续性，避免本次重构同时改变编程模型 |
| 构建系统 | 顶层 CMake + CMake Presets | 统一 Linux 构建、测试和 TSAN 配置，避免手写 `g++ *.cpp` 漏源文件 |
| 任务分发 | 每线程 Chase-Lev 队列 + 无锁 MPMC 共享入口 | 保留本地性并支持跨线程提交和工作窃取，消除单全局队列互斥锁 |
| 负载快照 | 原子计数器 + 周期性不可变快照 | 采集线程不读取其他线程专属的 `RUSAGE_THREAD`，避免错误 CPU 归属和锁阻塞 |
| 事件后端 | epoll + eventfd/pipe 唤醒 | 与现有 Linux 实现兼容，能统一 I/O、定时器和停止唤醒 |
| 时钟 | `steady_clock` | 系统时间回拨不影响 I/O 超时和睡眠语义 |
| Hook 范围 | libc 动态符号 Hook，普通 fd/用户非阻塞透传 | 兼容现有调用方式，避免改变普通文件和明确非阻塞调用的行为 |
| 线程亲和性 | 默认可关闭、失败可降级 | CPU 绑定依赖权限和容器配置，不能成为运行时启动硬依赖 |
| 栈优化 | M6 带 guard page 的动态栈/栈池原型 | 先建立可测试基线，再隔离高风险栈策略变化 |
| 性能验收 | 同主机同命令基线对比，工具缺失则标记不可用 | 防止把不可复现或内存指标误当作 CPU 性能结论 |

## 里程碑交付设计

| 里程碑 | 交付内容 | 完成门槛 |
|---|---|---|
| M0 | 统一构建骨架、环境探测、基线服务器和报告模板 | 构建通过，基线命令可重复执行并保存原始结果 |
| M1 | 配置、CPU 核数检测、四种负载均衡策略和快照测试 | 策略单测、线程数测试和快照导出测试通过 |
| M2 | 本地工作窃取队列、无锁共享入口和调度器压力测试 | 并发测试、TSAN（工具可用时）和窃取/唤醒集成测试通过 |
| M3 | epoll 事件循环、定时器、fd 注册表和完整 Hook | Hook 单测/集成测试及超时、关闭、取消路径通过 |
| M4 | 多线程调度器与 I/O 恢复任务集成 | echo/定时器端到端测试和停止流程测试通过 |
| M5 | 亲和性绑定、周期采集、负载反馈和均衡报告 | 负载快照、亲和性回退和均衡基准通过或明确记录环境限制 |
| M6 | 动态栈/栈池原型、100,000 协程容量测试、最终报告 | 容量测试、完整回归、性能对比和文档验收通过 |

每个里程碑完成后按“运行该阶段测试集 -> 检查工作树与生成物 -> 创建里程碑提交 -> 推送指定远端 -> 记录提交号和测试证据”的顺序执行。目标远端配置为 `https://github.com/Baiye-3000/coroutine-library`；若认证或网络不可用，保留本地提交并把推送失败作为明确阻塞项报告。

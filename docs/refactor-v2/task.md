# 协程运行时重构 V2 实施任务

## 文件清单

| 操作 | 文件 | 职责 |
|---|---|---|
| 新建 | `CMakeLists.txt` | 顶层构建、测试和基准入口 |
| 新建 | `CMakePresets.json` | Debug、Release、TSAN 构建预设 |
| 修改 | `README.md` | 新运行时的构建、测试和基准说明 |
| 新建 | `.gitignore` | 忽略新构建目录、二进制和性能原始输出 |
| 新建 | `include/coroutine/config.h` | 运行时配置和策略定义 |
| 新建 | `include/coroutine/coroutine.h` | 协程公共接口和状态 |
| 新建 | `include/coroutine/scheduler.h` | 调度器、提交结果和任务接口 |
| 新建 | `include/coroutine/load_balancer.h` | 负载快照和负载均衡器接口 |
| 新建 | `include/coroutine/metrics.h` | 指标快照和采集器接口 |
| 新建 | `include/coroutine/hook.h` | Hook 开关和公共 Hook 接口 |
| 新建 | `src/config.cpp` | 配置校验、默认线程数检测 |
| 新建 | `src/coroutine.cpp` | `ucontext` 协程生命周期和上下文切换 |
| 新建 | `src/stack_allocator.cpp` | 固定栈、动态栈和栈池分配 |
| 新建 | `src/scheduler.cpp` | 调度器生命周期、提交和停止 |
| 新建 | `src/worker.cpp` | 工作线程主循环、执行、唤醒和窃取 |
| 新建 | `src/work_stealing_deque.cpp` | Chase-Lev 本地双端队列 |
| 新建 | `src/ingress_queue.cpp` | 每工作线程 MPMC 入口队列 |
| 新建 | `src/load_balancer.cpp` | 四种选线程策略和评分 |
| 新建 | `src/metrics_collector.cpp` | 周期采集、快照和均衡统计 |
| 新建 | `src/event_loop.cpp` | epoll、eventfd 唤醒和事件派发 |
| 新建 | `src/timer_queue.cpp` | 单调时钟定时器 |
| 新建 | `src/fd_registry.cpp` | fd 状态和等待注册 |
| 新建 | `src/hook.cpp` | sleep、socket 和 I/O Hook |
| 新建 | `src/poll_hook.cpp` | `poll` Hook |
| 新建 | `src/thread.cpp` | Linux 线程工具和 CPU 时间发布 |
| 新建 | `src/cpu_affinity.cpp` | 在线 CPU 检测和线程绑定 |
| 新建 | `tests/CMakeLists.txt` | 测试目标注册 |
| 新建 | `tests/test_framework.h` | 无外部依赖的断言和测试注册 |
| 新建 | `tests/unit/config_test.cpp` | 配置和默认线程数测试 |
| 新建 | `tests/unit/load_balancer_test.cpp` | 四种策略和评分测试 |
| 新建 | `tests/unit/deque_test.cpp` | 本地队列所有者/窃取语义测试 |
| 新建 | `tests/unit/timer_test.cpp` | 定时器顺序和取消测试 |
| 新建 | `tests/unit/fd_registry_test.cpp` | fd 等待注册状态竞争测试 |
| 新建 | `tests/unit/metrics_test.cpp` | 指标采集和快照测试 |
| 新建 | `tests/unit/coroutine_test.cpp` | 创建、让出、恢复和异常收敛测试 |
| 新建 | `tests/integration/scheduler_test.cpp` | 提交、唤醒、窃取、亲和性和停止测试 |
| 新建 | `tests/integration/hook_test.cpp` | I/O、连接、接收、睡眠和透传测试 |
| 新建 | `tests/integration/poll_hook_test.cpp` | `poll` 就绪、超时、透传测试 |
| 新建 | `tests/integration/echo_server_test.cpp` | 多线程 echo 端到端测试 |
| 新建 | `tests/stress/scheduler_stress.cpp` | 并发提交和窃取压力测试 |
| 新建 | `benchmarks/CMakeLists.txt` | 基准测试目标注册 |
| 新建 | `benchmarks/baseline_server.cpp` | M0 单线程基线服务 |
| 新建 | `benchmarks/scheduler_benchmark.cpp` | 吞吐、延迟、均衡基准 |
| 新建 | `benchmarks/coroutine_capacity.cpp` | 协程栈容量和内存基准 |
| 新建 | `benchmarks/run_benchmarks.sh` | 固定环境信息和命令记录脚本 |
| 新建 | `docs/refactor-v2/benchmarks.md` | 指标定义、命令和可用性处理 |
| 新建 | `docs/refactor-v2/reports/m0-baseline.md` | M0 原始基线记录 |
| 新建 | `docs/refactor-v2/reports/m1-load-balancer.md` | M1 配置和策略测试记录 |
| 新建 | `docs/refactor-v2/reports/m2-scheduler.md` | M2 队列、压力和 TSAN 记录 |
| 新建 | `docs/refactor-v2/reports/m3-hook.md` | M3 Hook 覆盖和失败路径记录 |
| 新建 | `docs/refactor-v2/reports/m4-integration.md` | M4 I/O 调度和端到端记录 |
| 新建 | `docs/refactor-v2/reports/m5-load-balance.md` | M5 负载均衡报告 |
| 新建 | `docs/refactor-v2/reports/m6-final.md` | M6 最终对比报告 |

## M0：构建与基线

### T1：建立顶层 CMake 构建骨架

**文件：** `CMakeLists.txt`、`CMakePresets.json`、`.gitignore`

**依赖：** 无

**步骤：**

1. 要求 CMake 3.22+、C++17、pthread 和 Linux；禁止在非 Linux 平台配置运行时目标。
2. 添加 `coroutine_runtime` 静态库占位目标、`tests` 和 `benchmarks` 子目录开关，以及 `BUILD_TESTING` 开关。
3. 添加 Debug、Release 和 ThreadSanitizer 预设；TSAN 预设使用 `-fsanitize=thread`。
4. 添加 `build/`、`out/`、测试二进制、基准原始输出和 perf 数据的忽略规则，不覆盖现有用户 `.vscode/` 内容。

**验证：** 在 Ubuntu 22.04 环境运行 `cmake --preset debug && cmake --build --preset debug`；预期配置和构建均成功，且 `git status --short` 不出现构建产物。

### T2：建立测试框架和空测试入口

**文件：** `tests/CMakeLists.txt`、`tests/test_framework.h`、`tests/unit/config_test.cpp`

**依赖：** T1

**步骤：**

1. 实现无第三方依赖的测试注册器、断言宏和失败退出码。
2. 将单元、集成和压力测试注册到 CTest；测试可按目标独立执行。
3. 实现最小配置测试，验证测试框架会通过和会返回失败状态。

**验证：** 运行 `ctest --test-dir build/debug --output-on-failure`；预期所有已注册测试通过。

### T3：实现 M0 基线服务和采集脚本

**文件：** `benchmarks/CMakeLists.txt`、`benchmarks/baseline_server.cpp`、`benchmarks/run_benchmarks.sh`、`docs/refactor-v2/benchmarks.md`

**依赖：** T1

**步骤：**

1. 实现单线程 epoll HTTP echo/固定响应服务器，支持固定端口、连接数和持续时间参数。
2. 脚本记录主机 CPU、内核、编译器、Git 提交号、执行命令、`perf stat` 原始输出和压测客户端输出。
3. 文档定义 QPS、P99、上下文切换、Cache Miss、内存、CPU 标准差和负载比的采集方式；缺失工具必须记录为 `unavailable`。

**验证：** 运行 `benchmarks/run_benchmarks.sh --mode baseline`；预期生成包含环境、命令和原始输出位置的报告，工具不可用时具有明确原因。

### T4：记录 M0 基线并更新入口文档

**文件：** `docs/refactor-v2/reports/m0-baseline.md`、`README.md`

**依赖：** T2、T3

**步骤：**

1. 以固定命令至少运行三次基线，记录每次原始结果和汇总统计。
2. 记录可用工具、不可用工具以及任何环境限制，不填写推测性能值。
3. 在 README 增加新运行时的构建、测试、基准入口，并明确 `fiber_lib/` 是保留的旧版教学代码。

**验证：** 运行 `git diff --check`，检查报告包含三次运行、命令和环境字段；预期无新增格式错误，字段完整。

### T5：M0 门禁、提交和推送

**文件：** `docs/refactor-v2/reports/m0-baseline.md`

**依赖：** T4

**步骤：**

1. 在干净 Linux 构建目录运行 Debug 构建、CTest 和基线脚本。
2. 检查仅暂存 M0 新增/修改文件，绝不暂存用户现有的 `fiber_lib/6hook/main.cpp`、`fiber_lib/6hook/scheduler.h` 和 `.vscode/` 变更。
3. 将 `origin` 更新为 `https://github.com/Baiye-3000/coroutine-library`，确认远端和当前分支，再创建包含测试证据的 M0 提交。
4. 推送当前分支；把提交号、远端分支和实际测试命令追加到 M0 报告。

**验证：** 运行 `git show --stat --oneline HEAD` 和 `git ls-remote --heads origin <current-branch>`；预期提交仅含 M0 范围文件，远端存在相同分支头。

## M1：配置与负载均衡

### T6：实现运行时配置和 CPU 线程数选择

**文件：** `include/coroutine/config.h`、`src/config.cpp`、`tests/unit/config_test.cpp`

**依赖：** T5

**步骤：**

1. 定义 `RuntimeConfig`、策略枚举、权重和阈值默认值。
2. 使用 `std::thread::hardware_concurrency()` 检测默认工作线程数，并在返回 0 时回退为 1。
3. 校验显式线程数、采样间隔、权重范围和阈值；错误配置返回可断言的错误结果。

**验证：** 运行 `ctest --test-dir build/debug -R config_test --output-on-failure`；预期默认值、回退和显式配置测试通过。

### T7：实现负载快照和四种选择策略

**文件：** `include/coroutine/load_balancer.h`、`src/load_balancer.cpp`、`tests/unit/load_balancer_test.cpp`

**依赖：** T6

**步骤：**

1. 定义 `WorkerLoadSnapshot`、`SelectionResult` 和安全的分数归一化函数。
2. 实现轮询、最少负载、亲和性优先和自适应策略。
3. 对空快照、非法亲和性线程、过载首选线程和相同分数定义稳定行为。

**验证：** 运行 `ctest --test-dir build/debug -R load_balancer_test --output-on-failure`；预期四种策略、边界和空快照测试全部通过。

### T8：实现快照导出最小路径

**文件：** `include/coroutine/metrics.h`、`src/metrics_collector.cpp`、`tests/unit/metrics_test.cpp`

**依赖：** T6、T7

**步骤：**

1. 定义 `RuntimeMetrics` 和不可变的工作线程快照导出接口。
2. 实现由可注入工作线程计数器生成的快照，暂不启动真实后台采集线程。
3. 验证导出对象为值拷贝，且包含队列、完成量、等待时间和 CPU 字段。

**验证：** 运行 `ctest --test-dir build/debug -R metrics_test --output-on-failure`；预期快照字段和值拷贝测试通过。

### T9：M1 门禁、提交和推送

**文件：** `docs/refactor-v2/reports/m1-load-balancer.md` 及 M1 改动涉及的源文件、测试

**依赖：** T7、T8

**步骤：**

1. 运行 Debug 构建及 M1 单元测试。
2. 在 `m1-load-balancer.md` 记录测试命令、环境、原始输出位置、通过结果和将要创建的提交号。
3. 仅暂存 M1 范围文件，创建包含测试证据的 M1 提交并推送目标远端。

**验证：** 运行 `ctest --test-dir build/debug -R "config_test|load_balancer_test|metrics_test" --output-on-failure` 和远端分支核验；预期全部通过且远端分支包含 M1 提交。

## M2：无锁队列与工作窃取

### T10：实现本地工作窃取队列

**文件：** `src/work_stealing_deque.cpp`、`tests/unit/deque_test.cpp`

**依赖：** T9

**步骤：**

1. 实现分段 Chase-Lev 队列的所有者 `push/pop` 和窃取者 `steal`。
2. 确保扩容段在队列销毁前不释放，使用正确的原子内存序。
3. 覆盖空队列、单元素竞争、批量 LIFO、本地/窃取顺序和多窃取者竞争。

**验证：** 运行 `ctest --test-dir build/debug -R deque_test --output-on-failure`；预期功能和并发单测通过。

### T11：实现每工作线程无锁入口队列

**文件：** `src/ingress_queue.cpp`、`tests/unit/deque_test.cpp`

**依赖：** T10

**步骤：**

1. 实现 MPMC 链式入口队列、关闭标记和批量转移接口。
2. 为每个逻辑工作线程创建独立入口，禁止单一全局入口作为任务常规路径。
3. 覆盖多生产者提交、单消费者转移、关闭后拒绝和节点所有权释放。

**验证：** 运行 `ctest --test-dir build/debug -R deque_test --output-on-failure`；预期入口队列并发和关闭测试通过。

### T12：实现最小工作线程循环和调度器

**文件：** `include/coroutine/scheduler.h`、`src/scheduler.cpp`、`src/worker.cpp`、`tests/integration/scheduler_test.cpp`

**依赖：** T7、T8、T10、T11

**步骤：**

1. 实现启动、提交、每线程入口转移、本地执行、窃取、空闲等待、唤醒和停止。
2. 提交时调用负载均衡器，任务执行时维护队列深度、完成量和等待时间。
3. 通过测试接口验证空闲工作线程被唤醒、负载不均时完成窃取、停止后拒绝提交。

**验证：** 运行 `ctest --test-dir build/debug -R scheduler_test --output-on-failure`；预期提交、唤醒、窃取和停止场景通过。

### T13：加入并发压力与 TSAN 验证

**文件：** `tests/stress/scheduler_stress.cpp`、`CMakePresets.json`

**依赖：** T12

**步骤：**

1. 创建多生产者、多工作线程、高重复次数的提交/窃取压力程序。
2. 验证完成任务数与提交任务数一致，且在规定超时内退出。
3. 在工具链支持时配置并运行 TSAN；无法运行时记录明确原因，不跳过普通压力测试。

**验证：** 运行 `ctest --test-dir build/debug -R scheduler_stress --output-on-failure`，以及 `cmake --preset tsan && cmake --build --preset tsan && ctest --test-dir build/tsan -R scheduler_stress --output-on-failure`；预期普通压力通过，TSAN 可用时无报告。

### T14：M2 门禁、提交和推送

**文件：** `docs/refactor-v2/reports/m2-scheduler.md` 及 M2 改动涉及的源文件、测试

**依赖：** T13

**步骤：**

1. 运行 Debug 构建、队列/调度器测试、压力测试和可用的 TSAN。
2. 在 `m2-scheduler.md` 记录普通压力和 TSAN 的可用性及结果，检查没有把生成物或用户既有修改暂存。
3. 创建带验证证据的 M2 提交并推送目标远端。

**验证：** 运行 M2 测试集和远端分支核验；预期测试通过，远端含 M2 提交。

## M3：事件循环、定时器与 Hook

### T15：实现定时器与事件循环

**文件：** `src/timer_queue.cpp`、`src/event_loop.cpp`、`tests/unit/timer_test.cpp`

**依赖：** T14

**步骤：**

1. 实现基于 `steady_clock` 的定时器排序、取消、到期回调和最近等待时间计算。
2. 实现 epoll、eventfd/pipe 唤醒、事件注册、取消和运行一次接口。
3. 将就绪和定时器回调转换为调度任务，不在事件循环中直接恢复协程。

**验证：** 运行 `ctest --test-dir build/debug -R timer_test --output-on-failure`；预期顺序、取消和唤醒测试通过。

### T16：实现 fd 注册表和等待状态竞争

**文件：** `src/fd_registry.cpp`、`tests/unit/fd_registry_test.cpp`

**依赖：** T15

**步骤：**

1. 保存 socket 类型、关闭状态、用户非阻塞标记、读写超时和读/写等待注册。
2. 实现就绪、超时、关闭和显式取消的单获胜状态机。
3. 验证读写可并行注册、同方向重复注册返回错误、fd 关闭取消挂起等待。

**验证：** 运行 `ctest --test-dir build/debug -R fd_registry_test --output-on-failure`；预期全部状态竞争测试通过。

### T17：实现协程与 sleep Hook

**文件：** `include/coroutine/coroutine.h`、`src/coroutine.cpp`、`src/stack_allocator.cpp`、`include/coroutine/hook.h`、`src/hook.cpp`、`tests/unit/coroutine_test.cpp`、`tests/integration/hook_test.cpp`

**依赖：** T12、T15、T16

**步骤：**

1. 实现 `ucontext` 协程的创建、让出、恢复、完成和异常收敛，初期使用固定栈分配策略。
2. 实现 Hook 初始化、线程开关、`sleep`、`usleep`、`nanosleep` 的定时器等待和未启用模式透传。
3. 验证协程 sleep 不阻塞同一工作线程上的其他可运行任务，且异常不会跨上下文边界泄漏。

**验证：** 运行 `ctest --test-dir build/debug -R "coroutine_test|hook_test" --output-on-failure`；预期协程和睡眠 Hook 场景通过。

### T18：实现 socket 和读写 Hook

**文件：** `src/hook.cpp`、`tests/integration/hook_test.cpp`

**依赖：** T17

**步骤：**

1. Hook `socket`、`connect`、`accept` 以及 F8 中全部读写/收发调用。
2. 对 `EINTR`、`EAGAIN/EWOULDBLOCK`、`EINPROGRESS`、超时、关闭和用户非阻塞分别实现规定行为。
3. 未启用 Hook、非 socket fd、显式用户非阻塞和原函数解析失败时调用原始函数。

**验证：** 运行 `ctest --test-dir build/debug -R hook_test --output-on-failure`；预期连接、接收、发送、超时、关闭和透传用例通过。

### T19：实现 `poll` Hook

**文件：** `src/poll_hook.cpp`、`tests/integration/poll_hook_test.cpp`

**依赖：** T16、T17

**步骤：**

1. 将 `pollfd` 数组转换为读/写等待注册，使用一个调用级截止时间协调多 fd。
2. 聚合就绪 `revents`，正确返回超时、错误和被中断结果。
3. 对 Hook 禁用、零 fd、立即超时和用户非阻塞语义做透传或兼容处理。

**验证：** 运行 `ctest --test-dir build/debug -R poll_hook_test --output-on-failure`；预期就绪、超时和透传测试通过。

### T20：M3 门禁、提交和推送

**文件：** `docs/refactor-v2/reports/m3-hook.md` 及 M3 改动涉及的源文件、测试

**依赖：** T18、T19

**步骤：**

1. 运行 Debug 构建、协程、定时器、fd 注册表、Hook 和 poll Hook 测试。
2. 核查 F8 全部调用类别及 F9 失败路径均有测试用例，并在 `m3-hook.md` 记录命令、覆盖矩阵和结果。
3. 创建带验证证据的 M3 提交并推送目标远端。

**验证：** 运行 M3 CTest 正则测试集和远端分支核验；预期通过且远端含 M3 提交。

## M4：多线程 I/O 调度器集成

### T21：集成 I/O 恢复任务和线程本地事件循环

**文件：** `src/scheduler.cpp`、`src/worker.cpp`、`src/event_loop.cpp`、`tests/integration/scheduler_test.cpp`

**依赖：** T20

**步骤：**

1. 为每个工作线程创建事件循环，并把 I/O/定时器恢复任务送入该协程的亲和性入口。
2. 在亲和性线程过载或停止时允许调度器回退选择其他线程。
3. 验证停止时所有 I/O 等待以取消结果恢复，且不遗留 epoll 注册。

**验证：** 运行 `ctest --test-dir build/debug -R scheduler_test --output-on-failure`；预期 I/O 恢复、亲和性和停止用例通过。

### T22：实现多线程 echo 端到端测试

**文件：** `tests/integration/echo_server_test.cpp`、`README.md`

**依赖：** T21

**步骤：**

1. 编写本地回环多线程 echo 服务和多个并发客户端。
2. 验证每个客户端收到正确响应，连接关闭、超时和运行时停止均能在规定时间内完成。
3. 在 README 中记录端到端测试目标和运行命令。

**验证：** 运行 `ctest --test-dir build/debug -R echo_server_test --output-on-failure`；预期所有客户端响应正确且测试超时前退出。

### T23：M4 门禁、提交和推送

**文件：** `docs/refactor-v2/reports/m4-integration.md` 及 M4 改动涉及的源文件、测试

**依赖：** T22

**步骤：**

1. 运行 Debug 构建、全量单元测试、调度器测试、Hook 测试和 echo 端到端测试。
2. 在 `m4-integration.md` 记录测试结果，检查停止流程无卡住和无生成物暂存。
3. 创建带验证证据的 M4 提交并推送目标远端。

**验证：** 运行 `ctest --test-dir build/debug --output-on-failure` 和远端分支核验；预期全量通过且远端含 M4 提交。

## M5：亲和性、指标与反馈闭环

### T24：实现 Linux 线程亲和性与 CPU 时间发布

**文件：** `src/thread.cpp`、`src/cpu_affinity.cpp`、`src/worker.cpp`

**依赖：** T23

**步骤：**

1. 读取在线 CPU 列表并按工作线程编号尝试 `pthread_setaffinity_np`。
2. 记录实际绑定 CPU 或失败状态；绑定失败仅降级，不阻断运行时启动。
3. 让工作线程发布自身 CPU 纳秒计数，供采集器计算增量使用率。

**验证：** 运行 `ctest --test-dir build/debug -R "config_test|scheduler_test" --output-on-failure`；预期 CPU 映射和降级语义测试通过。

### T25：实现周期指标采集和快照

**文件：** `src/metrics_collector.cpp`、`include/coroutine/metrics.h`、`tests/unit/metrics_test.cpp`

**依赖：** T24

**步骤：**

1. 启动默认 100 ms 的采集线程，读取工作线程原子计数并计算 CPU 使用率、队列等待增量和负载统计。
2. 以双缓冲方式发布不可变快照，停止时唤醒并等待采集线程退出。
3. 测试至少两次更新、可配置采样周期、值拷贝和停止后无后台访问。

**验证：** 运行 `ctest --test-dir build/debug -R metrics_test --output-on-failure`；预期周期、快照和停止测试通过。

### T26：实现负载反馈和亲和性回退测试

**文件：** `src/load_balancer.cpp`、`src/scheduler.cpp`、`tests/integration/scheduler_test.cpp`

**依赖：** T25

**步骤：**

1. 让调度器使用采集快照选择新任务线程，并在恢复时评估 `affinity_max_load`。
2. 构造低负载首选线程与过载/不可用首选线程，验证分别保持亲和性和回退分发。
3. 构造短任务洪峰、长短任务混合和突发负载，验证无单一工作线程长期堆积。

**验证：** 运行 `ctest --test-dir build/debug -R scheduler_test --output-on-failure`；预期亲和性与负载反馈用例通过。

### T27：运行并记录 M5 均衡基准

**文件：** `benchmarks/scheduler_benchmark.cpp`、`docs/refactor-v2/reports/m5-load-balance.md`、`docs/refactor-v2/benchmarks.md`

**依赖：** T26

**步骤：**

1. 运行短任务洪峰、长短任务混合、突发流量和 CPU/I/O 混合基准，每项至少三次。
2. 记录每工作线程 CPU 使用率、队列深度、负载分数、最大/最小负载比和标准差。
3. 明确 N6 为通过、未通过或环境不具备足够并行度；不得把不可用数据标为通过。

**验证：** 运行 `benchmarks/run_benchmarks.sh --mode load-balance`；预期报告包含每场景原始数据、汇总结果和判定。

### T28：M5 门禁、提交和推送

**文件：** M5 改动涉及的源文件、测试、基准和报告

**依赖：** T27

**步骤：**

1. 运行 Debug 构建、全量 CTest、指标/亲和性测试和均衡基准。
2. 将环境限制、指标结果和测试证据记录到 M5 报告。
3. 创建带验证证据的 M5 提交并推送目标远端。

**验证：** 运行全量 CTest、均衡基准和远端分支核验；预期测试通过，远端含 M5 提交，报告有 N6 判定。

## M6：栈策略与最终验收

### T29：实现动态栈和可选栈池

**文件：** `src/stack_allocator.cpp`、`include/coroutine/config.h`、`tests/unit/coroutine_test.cpp`

**依赖：** T28

**步骤：**

1. 实现带 guard page 的动态栈分配、最大值校验和可选栈池复用。
2. 保留固定栈策略用于基线对比；在栈释放和复用前清理协程状态。
3. 覆盖初始大小、上限拒绝、池复用、析构释放和异常路径。

**验证：** 运行 `ctest --test-dir build/debug -R coroutine_test --output-on-failure`；预期栈策略和生命周期测试通过。

### T30：实现并运行 100,000 协程容量基准

**文件：** `benchmarks/coroutine_capacity.cpp`、`docs/refactor-v2/reports/m6-final.md`

**依赖：** T29

**步骤：**

1. 创建或复用至少 100,000 个协程，并测量峰值 RSS、创建/复用耗时和完成数。
2. 分别记录固定栈、动态栈和栈池（若启用）结果、配置和执行环境。
3. 对资源不足或内核限制明确记录失败原因，不省略数据。

**验证：** 运行 `benchmarks/run_benchmarks.sh --mode capacity --coroutines 100000`；预期报告显示完成数为 100,000 或记录明确资源限制。

### T31：运行最终性能对比并完成报告

**文件：** `benchmarks/scheduler_benchmark.cpp`、`docs/refactor-v2/reports/m6-final.md`、`README.md`

**依赖：** T30

**步骤：**

1. 按 M0 同一主机、同一命令、相同重复次数运行基线和新运行时对比。
2. 记录 QPS、P99、上下文切换、Cache Miss、内存和负载均衡，并计算 N5/N6 目标判定。
3. 在 README 链接最终报告，明确已达成、未达成和不可测指标及原因。

**验证：** 运行 `benchmarks/run_benchmarks.sh --mode comparison`；预期最终报告具有原始数据、计算公式和所有指标判定。

### T32：M6 最终门禁、提交和推送

**文件：** M6 改动涉及的源文件、全部报告和文档

**依赖：** T31

**步骤：**

1. 在干净构建目录运行 Release 构建、全量 CTest、压力测试、容量基准和最终对比基准；TSAN 可用时再运行压力测试。
2. 按 `checklist.md` 逐项执行验收，记录实际结果，不以预期代替证据。
3. 仅暂存 V2 新增/修改文件，创建包含最终测试和报告证据的 M6 提交，并推送目标远端。

**验证：** 运行 `ctest --test-dir build/release --output-on-failure`、容量/对比脚本、`git show --stat --oneline HEAD` 和远端分支核验；预期验收项有明确结果，远端含 M6 提交。

## 执行顺序

```text
T1 -> T2 -> T4 -> T5 -> T6 -> T7 -> T8 -> T9
  \-> T3 -/
T9 -> T10 -> T11 -> T12 -> T13 -> T14
T14 -> T15 -> T16 -> T17 -> T18 -> T20
                       \-> T19 -/
T20 -> T21 -> T22 -> T23 -> T24 -> T25 -> T26 -> T27 -> T28
T28 -> T29 -> T30 -> T31 -> T32
```

T2 与 T3 可并行；T7 与 T8 在 T6 后可并行；T18 与 T19 在依赖满足后可并行。每个 M0、M1、M2、M3、M4、M5 和 M6 门禁任务是下一阶段的前置依赖，未通过测试或未完成提交/推送记录时不得进入下一阶段。

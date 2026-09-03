# 协程运行时重构 V2 验收清单

> 每一项必须通过运行代码或观察实际结果验证。结果只能记录为通过、未通过或不可用；“不可用”必须给出工具或环境原因，不得以预期替代证据。

## 需求追踪

| 验收标准 | 对应清单章节 |
|---|---|
| AC1 | 实现完整性：CMake 构建与旧版代码保护；构建与测试：Debug/Release 构建 |
| AC2 | 实现完整性：CPU 默认线程数与显式配置 |
| AC3 | 实现完整性：无全局队列；集成与并发：压力测试和 TSAN |
| AC4 | 实现完整性：四种负载均衡策略和自适应评分 |
| AC5 | 集成与并发：工作窃取、唤醒和完成数核验 |
| AC6 | 实现完整性：亲和性保持和过载回退 |
| AC7 | Hook 与事件循环：全部 Hook 成功、超时/错误和透传验证 |
| AC8 | 实现完整性：周期指标采集和快照字段 |
| AC9 | 构建与测试：Debug/Release 全量 CTest |
| AC10 | 基准与性能：M0 基线、最终对比和 N5/N6 判定 |
| AC11 | 基准与性能：100,000 协程容量基准 |
| AC12 | 分阶段提交与推送：M0 至 M6 提交、推送和范围核验 |

## 实现完整性

- [ ] 新运行时通过根目录的 CMake 入口构建，且旧版 `fiber_lib/` 未被重构提交修改。（验证：在 Ubuntu 22.04 兼容环境运行 `cmake --preset debug && cmake --build --preset debug`；使用 `git diff --name-only <重构起点>..HEAD -- fiber_lib`，预期无输出。）
- [ ] Debug、Release 和 TSAN 预设存在且可配置；TSAN 不可用时报告编译器或运行环境原因。（验证：运行 `cmake --preset debug`、`cmake --preset release` 和 `cmake --preset tsan`；预期前两个成功，TSAN 成功或在报告中有明确失败原因。）
- [ ] 仓库不会跟踪新生成的构建目录、二进制或性能原始输出。（验证：构建和运行测试后执行 `git status --short`；预期无新增生成物，用户原有改动不在本项范围内。）
- [ ] 未配置工作线程数时使用在线 CPU 数或回退值 1；显式正数配置生效，非法配置被拒绝。（验证：运行 `ctest --test-dir build/debug -R config_test --output-on-failure`；预期通过。）
- [ ] 调度器具备本地队列、每线程无锁入口、工作窃取、唤醒和停止机制，常规路径没有单一全局互斥任务队列。（验证：运行 `ctest --test-dir build/debug -R "deque_test|scheduler_test|scheduler_stress" --output-on-failure`；检查架构测试和压力测试输出，预期通过。）
- [ ] 四种负载均衡策略均可选择，空快照和非法亲和性不会访问无效线程。（验证：运行 `ctest --test-dir build/debug -R load_balancer_test --output-on-failure`；预期全部策略和边界用例通过。）
- [ ] 自适应策略使用队列深度、CPU 使用率、等待时间、历史完成量和亲和性奖励，且权重、阈值可配置。（验证：运行 `ctest --test-dir build/debug -R "config_test|load_balancer_test" --output-on-failure`；预期受控快照选择符合断言。）
- [ ] 被定时器或 I/O 挂起的协程保留上次工作线程偏好；首选线程过载或不可用时回退。（验证：运行 `ctest --test-dir build/debug -R scheduler_test --output-on-failure`；预期亲和性保持和回退用例通过。）
- [ ] 每工作线程快照包含队列深度、CPU 使用率、完成量、等待时间、运行/空闲状态和更新时间；导出结果为值拷贝。（验证：运行 `ctest --test-dir build/debug -R metrics_test --output-on-failure`；预期字段和值拷贝断言通过。）
- [ ] 默认 100 ms 的指标采集器至少发布两次快照，并在停止时退出且不再访问工作线程。（验证：运行 `ctest --test-dir build/debug -R metrics_test --output-on-failure`；预期周期、停止和析构用例通过。）
- [ ] 线程 CPU 绑定按在线 CPU 尝试执行；无权限或 CPU 不可用时降级而不阻止运行时启动。（验证：运行 `ctest --test-dir build/debug -R "config_test|scheduler_test" --output-on-failure`；预期映射或降级状态被断言。）
- [ ] 协程支持创建、让出、恢复、完成、取消和异常收敛，且异常不会穿过上下文切换边界。（验证：运行 `ctest --test-dir build/debug -R coroutine_test --output-on-failure`；预期通过。）
- [ ] 栈分配支持固定策略、动态 guard page 策略和可选栈池；栈上限、复用和释放语义受测试覆盖。（验证：运行 `ctest --test-dir build/debug -R coroutine_test --output-on-failure`；预期通过。）

## Hook 与事件循环

- [ ] 事件循环使用 epoll 和唤醒 fd，定时器使用单调时钟；就绪和定时器回调只提交恢复任务，不在事件循环中直接恢复协程。（验证：运行 `ctest --test-dir build/debug -R timer_test --output-on-failure`；预期定时顺序、取消和唤醒用例通过。）
- [ ] fd 注册表支持读写并行等待、拒绝同方向重复注册，并使就绪、超时、关闭和取消只有一个获胜路径。（验证：运行 `ctest --test-dir build/debug -R fd_registry_test --output-on-failure`；预期状态竞争用例通过。）
- [ ] `sleep`、`usleep`、`nanosleep` 在 Hook 启用的协程中不阻塞工作线程，禁用时调用原始函数。（验证：运行 `ctest --test-dir build/debug -R hook_test --output-on-failure`；预期睡眠并发和透传用例通过。）
- [ ] `read`、`readv`、`recv`、`recvfrom`、`recvmsg`、`write`、`writev`、`send`、`sendto`、`sendmsg` 均能在 `EAGAIN/EWOULDBLOCK` 时挂起并在就绪后恢复。（验证：运行 `ctest --test-dir build/debug -R hook_test --output-on-failure`；预期每个读写类别有成功用例并通过。）
- [ ] `connect` 和 `accept` 在启用 Hook 时正确处理连接/接收、`EINPROGRESS`、超时和关闭；普通 fd、用户显式非阻塞及禁用 Hook 时透传。（验证：运行 `ctest --test-dir build/debug -R hook_test --output-on-failure`；预期相关用例通过。）
- [ ] `poll` Hook 支持多 fd 就绪聚合、超时、零 fd、禁用模式和错误返回语义。（验证：运行 `ctest --test-dir build/debug -R poll_hook_test --output-on-failure`；预期通过。）
- [ ] 所有 Hook 等待路径均覆盖 `EINTR`、超时、关闭和取消，且返回 POSIX 标准错误结果。（验证：运行 `ctest --test-dir build/debug -R "hook_test|poll_hook_test|fd_registry_test" --output-on-failure`；预期失败路径断言通过。）

## 集成与并发

- [ ] 多工作线程提交、唤醒、本地执行和停止在指定超时内完成。（验证：运行 `ctest --test-dir build/debug -R scheduler_test --output-on-failure`；预期通过。）
- [ ] 空闲工作线程从繁忙工作线程窃取并完成任务，完成数等于提交数。（验证：运行 `ctest --test-dir build/debug -R "scheduler_test|scheduler_stress" --output-on-failure`；预期计数相等且无超时。）
- [ ] 并发提交与窃取压力测试无死锁；TSAN 可用时没有数据竞争报告。（验证：运行 `ctest --test-dir build/debug -R scheduler_stress --output-on-failure`，再运行 `ctest --test-dir build/tsan -R scheduler_stress --output-on-failure`；预期普通测试通过，TSAN 通过或记录不可用原因。）
- [ ] I/O 和定时器恢复任务经线程入口重新调度，停止时取消等待且没有遗留 epoll 注册。（验证：运行 `ctest --test-dir build/debug -R scheduler_test --output-on-failure`；预期 I/O 恢复和停止场景通过。）
- [ ] 多线程本地回环 echo 服务为所有并发客户端返回正确响应，并在关闭、超时和停止场景中退出。（验证：运行 `ctest --test-dir build/debug -R echo_server_test --output-on-failure`；预期通过。）
- [ ] 每个公共调度、负载、协程、Hook 和指标接口至少有一个真实调用者。（验证：运行 `cmake --build --preset debug && ctest --test-dir build/debug --output-on-failure`；预期构建和全量测试通过。）

## 构建与测试

- [ ] Debug 构建无编译错误。（验证：`cmake --preset debug && cmake --build --preset debug`。）
- [ ] Release 构建无编译错误。（验证：`cmake --preset release && cmake --build --preset release`。）
- [ ] Debug 全量单元、集成和压力测试通过。（验证：`ctest --test-dir build/debug --output-on-failure`。）
- [ ] Release 全量测试通过。（验证：`ctest --test-dir build/release --output-on-failure`。）
- [ ] TSAN 压力测试在可用时通过，否则报告中记录不可用原因。（验证：`cmake --preset tsan && cmake --build --preset tsan && ctest --test-dir build/tsan -R scheduler_stress --output-on-failure`。）
- [ ] 源码、测试和文档无新增空白错误或未完成占位符。（验证：`git diff --check <重构起点>..HEAD`，并搜索 `T[O]DO|TB[D]|FIX[M]E`；预期无新增问题。）

## 基准与性能

- [ ] M0 基线报告包含至少三次同一命令运行、环境、工具可用性、原始输出位置和汇总值。（验证：检查 `docs/refactor-v2/reports/m0-baseline.md`；预期字段齐全且数据非推测。）
- [ ] 基准脚本记录主机 CPU、内核、编译器、Git 提交、命令、重复次数和原始输出位置。（验证：运行 `benchmarks/run_benchmarks.sh --mode baseline`；预期生成或更新报告。）
- [ ] 基准报告对 QPS、P99、上下文切换、Cache Miss、内存、CPU 使用率标准差和最大/最小负载比给出定义、采集命令和计算公式。（验证：检查 `docs/refactor-v2/benchmarks.md`；预期每项均有说明。）
- [ ] M5 覆盖短任务洪峰、长短任务混合、突发流量和 CPU/I/O 混合场景，每场景至少三次，并报告每工作线程原始负载。（验证：运行 `benchmarks/run_benchmarks.sh --mode load-balance`；检查 `m5-load-balance.md`，预期有场景、原始数据和汇总。）
- [ ] 在满足至少两个活跃工作线程和足够并行度时，M5/M6 报告明确 N6 是否达到 CPU 使用率标准差不高于 5% 和最大/最小负载比不高于 1.5；条件不满足时标记不可判定并说明原因。（验证：检查 M5、M6 报告中的判定和条件。）
- [ ] 容量基准创建或复用至少 100,000 个协程，并记录栈策略、完成数、峰值 RSS 和耗时。（验证：运行 `benchmarks/run_benchmarks.sh --mode capacity --coroutines 100000`；检查 `m6-final.md`，预期完成数为 100,000 或有明确资源限制。）
- [ ] 最终对比使用与 M0 相同主机、命令和重复次数，报告 QPS、P99、上下文切换、Cache Miss、内存和负载数据。（验证：运行 `benchmarks/run_benchmarks.sh --mode comparison`；检查 `m6-final.md`，预期原始数据和计算过程齐全。）
- [ ] N5 性能目标分别明确为通过、未通过或工具不可用：吞吐量提升至少 30%、P99 降低至少 20%、上下文切换减少至少 30%、Cache Miss 减少至少 10%。（验证：检查 `m6-final.md`，预期每项存在带基线分母的判定。）

## 分阶段提交与推送

- [ ] M0 测试和基线完成后创建 M0 提交，提交仅包含 M0 范围的新增/修改文件，且已推送至目标远端。（验证：检查 `m0-baseline.md` 的命令、提交号和远端分支；运行 `git show --stat --oneline <M0提交>` 和 `git ls-remote --heads origin <分支>`。）
- [ ] M1 测试完成后创建并推送 M1 提交，报告记录配置、策略和快照测试证据。（验证：检查 `m1-load-balancer.md`，核验提交和远端分支。）
- [ ] M2 测试完成后创建并推送 M2 提交，报告记录队列、压力和 TSAN 结果。（验证：检查 `m2-scheduler.md`，核验提交和远端分支。）
- [ ] M3 测试完成后创建并推送 M3 提交，报告记录 Hook 覆盖矩阵和失败路径结果。（验证：检查 `m3-hook.md`，核验提交和远端分支。）
- [ ] M4 测试完成后创建并推送 M4 提交，报告记录 I/O 调度和 echo 端到端结果。（验证：检查 `m4-integration.md`，核验提交和远端分支。）
- [ ] M5 测试和均衡基准完成后创建并推送 M5 提交，报告记录 N6 判定和环境限制。（验证：检查 `m5-load-balance.md`，核验提交和远端分支。）
- [ ] M6 全量验收、容量和最终对比完成后创建并推送 M6 提交，报告记录全部 N5/N6 判定。（验证：检查 `m6-final.md`，核验提交和远端分支。）
- [ ] 任一阶段暂存、提交和推送时均未包含用户既有的 `fiber_lib/6hook/main.cpp`、`fiber_lib/6hook/scheduler.h` 和 `.vscode/` 变更。（验证：对每个里程碑运行 `git show --name-only --format= <提交>`；预期无上述路径。）

## 端到端场景

- [ ] 调度端到端：多生产者提交混合短/长任务 -> 四个工作线程均完成任务 -> 空闲线程窃取积压任务 -> 完成数等于提交数。（验证：运行 `scheduler_stress` 和 `scheduler_benchmark` 混合负载场景；观察报告和断言。）
- [ ] I/O 端到端：协程发起连接、等待 `accept`/`recv`、收到 echo 响应 -> 连接关闭 -> 所有协程和工作线程在停止超时前退出。（验证：运行 `echo_server_test`；观察通过结果。）
- [ ] Hook 边界：在同一协程执行 `poll` 超时和 `read` 超时 -> 返回 `ETIMEDOUT` 或规定的超时结果 -> 其他工作线程仍可执行任务。（验证：运行 `hook_test` 与 `poll_hook_test`；观察错误码和并发断言。）
- [ ] 亲和性边界：协程在低负载首选线程挂起后恢复 -> 返回原线程；将该线程负载置为过载或停止 -> 恢复任务被调度到其他可用线程。（验证：运行 `scheduler_test` 亲和性用例；观察线程编号断言。）

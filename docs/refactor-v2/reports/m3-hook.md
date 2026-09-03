# M3 事件循环、定时器与 Hook 报告

## 状态

M3 的事件循环、单调时钟定时器、fd 等待状态、固定栈 `ucontext` 协程和异步 Hook 链路已完成。Debug 构建及全量测试通过，待提交推送。

事件循环只派发一次性恢复回调，不直接恢复协程；回调通过当前工作线程的调度入口提交协程恢复任务。跨线程亲和性回退和停止时统一取消等待仍属于 M4。

## 环境

- 平台：WSL2 Ubuntu 22.04
- 内核：`6.18.33.2-microsoft-standard-WSL2`
- 编译器：GCC 11.4.0
- 构建目录：`/tmp/coroutine-lib-build-m3`
- 分支：`feature-x`

## 验证命令

```sh
cmake -S . -B /tmp/coroutine-lib-build-m3 -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCOROUTINE_BUILD_TESTS=ON \
  -DCOROUTINE_BUILD_BENCHMARKS=OFF
cmake --build /tmp/coroutine-lib-build-m3
ctest --test-dir /tmp/coroutine-lib-build-m3 --output-on-failure
```

## 测试结果

```text
11/11 tests passed, 0 tests failed
```

覆盖目标：

| 测试 | 覆盖内容 | 结果 |
|---|---|---|
| `timer_test` | 定时器到期、取消、最近超时、eventfd 唤醒、epoll 读就绪 | 通过 |
| `fd_registry_test` | 读写并行注册、同方向重复注册、完成、关闭和关闭后拒绝 | 通过 |
| `coroutine_test` | 固定栈创建、resume、yield、完成、取消、异常收敛 | 通过 |
| `hook_test` | Hook 开关、协程 sleep、pipe EAGAIN 等待/恢复、socket 入口 | 通过 |
| `poll_hook_test` | poll 就绪、零 fd 和返回事件聚合入口 | 通过 |

其余 M0-M2 回归测试均通过。

## Hook 覆盖矩阵

| 类别 | 已提供入口 | 当前行为 |
|---|---|---|
| `read`/`write` | 是 | 协程 EAGAIN 注册 epoll、让出并恢复；用户非阻塞透传 |
| `recv`/`send` | 是 | 协程 EAGAIN 注册读写事件并重试 |
| `recvfrom`/`sendto`/向量调用 | 是 | 保留原始调用入口；完整地址/向量异步细化留待 M4 |
| `connect`/`accept`/`socket` | 是 | connect/accept 协程事件等待；socket 透传 |
| `poll` | 是 | 多 fd 注册、调用级定时器和就绪聚合 |
| `sleep/usleep/nanosleep` | 是 | 协程定时器让出；非运行时上下文透传 |

## 门禁

M3 测试完成。提交时仅纳入 M3 新增/修改文件，保留用户已有的 `fiber_lib/` 和 `.vscode/` 修改；提交号和远端分支将在推送后补录。

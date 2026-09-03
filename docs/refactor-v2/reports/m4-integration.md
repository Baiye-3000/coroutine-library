# M4 多线程 I/O 调度集成报告

## 状态

M4 的线程本地事件循环、协程 I/O 恢复入口和多线程回环 echo 集成测试已完成。

## 环境

- 平台：WSL2 Ubuntu 22.04
- 构建目录：`/tmp/coroutine-lib-build-m4`
- 分支：`feature-x`

## 验证命令

```sh
cmake -S . -B /tmp/coroutine-lib-build-m4 -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCOROUTINE_BUILD_TESTS=ON \
  -DCOROUTINE_BUILD_BENCHMARKS=OFF
cmake --build /tmp/coroutine-lib-build-m4
ctest --test-dir /tmp/coroutine-lib-build-m4 --output-on-failure
```

## 验证结果

定向 M4 测试：`scheduler_test`、`hook_test`、`poll_hook_test`、`echo_server_test`，结果 `4/4` 通过。

全量结果见本阶段验收命令输出；提交前必须确认全部测试通过。

## 覆盖内容

- 每个工作线程持有独立 `EventLoop`，空闲工作线程运行事件循环并处理定时器、epoll 就绪和唤醒 fd。
- I/O 和定时器回调通过协程提交入口恢复，不在事件循环回调中直接切换上下文。
- 协程 `accept`、`connect`、`recv`、`send` 和基础 `read`/`write` 场景可在回环连接中完成。
- 多线程本地回环 echo 客户端收到完整 `ping` 响应，服务端和客户端在停止前退出。
- 用户已有的 `fiber_lib/6hook/*` 与 `.vscode/` 修改未进入本阶段提交。

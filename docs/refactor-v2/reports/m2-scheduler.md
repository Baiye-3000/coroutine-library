# M2 无锁队列与工作窃取报告

## 状态

M2 的 T10-T13 已完成。每工作线程本地队列、独立入口队列和最小调度器已实现，功能测试和普通压力测试通过，TSAN 已完成构建但运行环境不支持，待提交推送。

## 环境

- 平台：WSL2 Ubuntu 22.04
- 内核：`6.18.33.2-microsoft-standard-WSL2`
- 编译器：GCC 11.4.0
- 构建目录：`/tmp/coroutine-lib-build-m2`
- TSAN 构建目录：`/tmp/coroutine-lib-build-m2-tsan`
- 分支：`feature-x`

## 验证命令

```sh
cmake -S . -B /tmp/coroutine-lib-build-m2 -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCOROUTINE_BUILD_TESTS=ON \
  -DCOROUTINE_BUILD_BENCHMARKS=OFF
cmake --build /tmp/coroutine-lib-build-m2
ctest --test-dir /tmp/coroutine-lib-build-m2 \
  -R "config_test|load_balancer_test|metrics_test|deque_test|scheduler_test|scheduler_stress" \
  --output-on-failure
```

## 测试结果

普通测试结果：

```text
100% tests passed, 0 tests failed out of 6
```

`scheduler_stress` 独立连续执行 3 次，均输出：

```text
PASS scheduler_stress
```

覆盖内容包括本地队列 LIFO、窃取 FIFO、多生产者提交、独立入口、任务完成计数、空闲唤醒、停止和停止后拒绝提交。

TSAN 命令：

```sh
cmake -S . -B /tmp/coroutine-lib-build-m2-tsan -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCOROUTINE_BUILD_TESTS=ON \
  -DCMAKE_CXX_FLAGS='-fsanitize=thread -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=thread'
cmake --build /tmp/coroutine-lib-build-m2-tsan
ctest --test-dir /tmp/coroutine-lib-build-m2-tsan \
  -R scheduler_stress --output-on-failure
```

TSAN 构建通过；运行阶段失败，原始错误为：

```text
FATAL: ThreadSanitizer: unexpected memory mapping
```

结论：当前 WSL2 运行环境的 TSAN 运行时不可用，不将其记为数据竞争通过，也不以推测替代结果。

## 门禁

M2 测试已完成，满足提交条件。提交时仅纳入 M2 实现、测试和报告，保留用户已有的 `fiber_lib/` 与 `.vscode/` 修改；提交号和远端分支将在推送后补录。

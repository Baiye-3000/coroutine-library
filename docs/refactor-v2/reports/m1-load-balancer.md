# M1 配置与负载均衡报告

## 状态

M1 的 T6、T7、T8 已完成。配置、负载选择和最小快照导出路径已实现，Debug 构建及 M1 单元测试通过，待创建并推送阶段提交。

## 环境

- 平台：WSL2 Ubuntu 22.04
- 编译器：GCC 11.4.0
- CMake：3.22.1
- 构建生成目录：`/tmp/coroutine-lib-build-m1`
- 分支：`feature-x`

## 验证命令

```sh
cmake -S . -B /tmp/coroutine-lib-build-m1 -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCOROUTINE_BUILD_TESTS=ON \
  -DCOROUTINE_BUILD_BENCHMARKS=OFF
cmake --build /tmp/coroutine-lib-build-m1
ctest --test-dir /tmp/coroutine-lib-build-m1 \
  -R "config_test|load_balancer_test|metrics_test" --output-on-failure
```

## 测试结果

```text
1/3 Test #1: config_test ............... Passed
2/3 Test #2: load_balancer_test ........ Passed
3/3 Test #3: metrics_test ............. Passed
100% tests passed, 0 tests failed out of 3
```

- 配置测试：验证默认 CPU 线程数、显式线程数、采样周期和权重/阈值校验。
- 负载均衡测试：验证轮询、最少负载、亲和性优先、自适应策略，以及空快照和非法亲和性边界。
- 指标测试：验证队列、CPU、完成量、等待时间、提交/窃取/上下文切换/I/O 字段和快照值拷贝。

## 变更范围

- 新增 `include/coroutine/config.h`、`load_balancer.h`、`metrics.h`。
- 新增 `src/config.cpp`、`load_balancer.cpp`、`metrics_collector.cpp`。
- 扩展根构建和 CTest 目标；未修改旧版 `fiber_lib/` 实现。

## 门禁

M1 测试已完成。提交前只暂存 M1 文件，保留用户已有的 `fiber_lib/` 和 `.vscode/` 修改；提交号和远端分支将在推送后补录。

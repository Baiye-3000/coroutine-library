# 基准测试说明

M0 只建立基线采集入口；后续里程碑会增加调度器、Hook、栈容量和负载均衡对比。所有结果必须记录主机、内核、编译器、提交号、命令、重复次数和原始输出位置。

## 指标定义

- QPS：压测工具报告的每秒完成请求数。
- P99：压测工具报告的 99 百分位请求延迟，单位为微秒或毫秒，并在报告中注明。
- 上下文切换：`perf stat -e context-switches` 在完整基准进程范围内的计数。
- Cache Miss：`perf stat -e cache-misses` 在完整基准进程范围内的计数。
- 内存：基准进程峰值 RSS，优先来自 `/usr/bin/time -v` 的 Maximum resident set size。
- CPU 使用率标准差：所有工作线程采样 CPU 比例的总体标准差；至少需要两个活跃工作线程。
- 最大/最小负载比：同一采样窗口中非零工作线程归一化负载最大值除以最小值。

## M0 命令

```sh
cmake --preset debug
cmake --build --preset debug
benchmarks/run_benchmarks.sh --mode baseline
```

脚本执行三次 `ab -n 1000 -c 32`，并在 `perf` 存在时记录上下文切换和 Cache Miss。`curl`、`ab`、`perf` 或 CMake 不可用时必须在报告中写明 `unavailable` 及原因，不能用估算值填充。

## 对比规则

M6 必须在与 M0 相同的主机、命令和重复次数下运行。吞吐量提升计算为 `(new - baseline) / baseline`；P99、上下文切换和 Cache Miss 的降低计算为 `(baseline - new) / baseline`。基线分母为零或工具缺失时，该项标记为不可判定。

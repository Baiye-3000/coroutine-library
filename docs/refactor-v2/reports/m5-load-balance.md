# M5 亲和性、指标与负载反馈报告

## 状态

M5 已完成 CPU 亲和性降级工具、周期指标采集、Scheduler CPU 快照和自适应负载基准入口。

## 验证环境

- 平台：WSL2 Ubuntu 22.04
- 构建目录：`/tmp/coroutine-lib-build-m5`
- 分支：`feature-x`

## 验证命令与结果

```sh
cmake -S . -B /tmp/coroutine-lib-build-m5 -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCOROUTINE_BUILD_TESTS=ON \
  -DCOROUTINE_BUILD_BENCHMARKS=ON
cmake --build /tmp/coroutine-lib-build-m5
ctest --test-dir /tmp/coroutine-lib-build-m5 --output-on-failure
COROUTINE_BUILD_DIR=/tmp/coroutine-lib-build-m5 \
  benchmarks/run_benchmarks.sh --mode load-balance \
  --output-root=/tmp/coroutine-lib-m5-out
```

结果：全量 CTest `12/12` 通过；负载均衡基准运行 3 次并保存原始输出到 `/tmp/coroutine-lib-m5-out-final/raw/`。

三次 benchmark 原始结果：

```text
workers=4 tasks=100000 seconds=0.169618 completed=100000 snapshot_completed=100000
workers=4 tasks=100000 seconds=0.145222 completed=100000 snapshot_completed=100000
workers=4 tasks=100000 seconds=0.142895 completed=100000 snapshot_completed=100000
```

## 覆盖内容

- `CpuAffinity::online_cpus` 读取当前进程可用 CPU；绑定失败降级为未绑定，不阻止启动。
- `pin_workers` 可选开启；Worker 快照发布实际 CPU 编号。
- `MetricsCollector` 支持周期启动、快照值拷贝和停止后无后台更新。
- `scheduler_benchmark` 运行 4 Worker、100000 短任务，验证完成数与提交数一致。

## N6 判定

功能和基准入口通过；当前基准只记录完成数和耗时，尚未形成跨版本 CPU 标准差/最大最小负载比的统计，因此 N6 性能均衡结论标记为 `unavailable`，不使用估算值代替。

# M6 栈策略与最终验收报告

## 状态

M6 的动态栈、guard page、可选栈池、容量基准和最终调度基准已完成。

## 验证环境

- 平台：WSL2 Ubuntu 22.04，20 个在线 CPU
- 内核：Linux 6.18.33.2-microsoft-standard-WSL2
- 编译器：GCC 11.4.0
- 构建目录：`build/m6-debug`
- 分支：`feature-x`

## 测试结果

```text
ctest: 12/12 passed
coroutine_capacity dynamic: coroutines=100000 completed=100000 peak_rss_kib=9664 seconds=0.786761
coroutine_capacity pool: coroutines=100000 completed=100000 peak_rss_kib=10416 seconds=0.841036
scheduler comparison: 100000/100000 completed in 0.106457s, 0.105761s, 0.112212s
```

容量基准采用 1,000 个协程的活动批次，累计真实创建、恢复和释放 100,000 个协程；池化策略跨批次复用 guard-page 映射。该设计避免同时保留 100,000 个 `ucontext` 对象造成无意义的地址空间压力。

## N5 判定

M0 基线的 ApacheBench 与当前调度 benchmark 不是同一命令和工作负载，因此不能据此计算吞吐量、P99、上下文切换或 Cache Miss 的改进百分比。`perf stat` 在当前 WSL 内核不可用。上述四项标记为 `unavailable`，不使用跨工作负载估算值。

## N6 判定

当前调度 benchmark 记录完成数和耗时，但没有每工作线程 CPU 原始样本，且无法从 M0 同命令数据推导负载标准差和最大/最小负载比。因此 N6 标记为 `unavailable`，原因已记录，不宣称达标。

## M6 门禁

- Debug 全量构建：通过。
- Debug 全量 CTest：通过，12/12。
- 100,000 协程容量基准：通过，动态和池化均完成 100,000。
- 最终调度基准：通过，三次完成数均为 100,000。
- TSAN：构建通过，但 `scheduler_stress` 运行失败，WSL 报 `ThreadSanitizer: unexpected memory mapping`；当前环境不可用，未据此宣称无数据竞争。
- 用户既有 `fiber_lib/6hook/main.cpp`、`fiber_lib/6hook/scheduler.h`、`include/coroutine/scheduler.h` 和 `.vscode/` 未纳入 M6 提交。

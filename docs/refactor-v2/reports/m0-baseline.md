# M0 基线报告

## 状态

本报告记录 M0 的构建与基线采集状态。早期 Windows 环境限制作为历史记录保留；当前已在 WSL Ubuntu 22.04 中完成构建、测试和基线采集。

## 当前环境记录

- 日期：2026-09-03
- 工作树：`feature-x`
- 当前执行环境：WSL2 Ubuntu 22.04（Windows PowerShell 仅用于启动 WSL）
- Linux 内核：`6.18.33.2-microsoft-standard-WSL2`
- Linux 用户：`baiye`
- 项目路径：`/mnt/e/协程库/coroutine-lib`
- 用户既有未提交修改：`fiber_lib/6hook/main.cpp`、`fiber_lib/6hook/scheduler.h`、`.vscode/`

## 环境准备记录

- 便携式 CMake 3.30.5：已下载至被忽略的 `tools/cmake-3.30.5-windows-x86_64/`，可运行。
- 便携式 Ninja 1.12.1：已下载至被忽略的 `tools/ninja/`，可运行。
- WSL Ubuntu-22.04：已注册并完成初始化，默认 Linux 用户为 `baiye`。
- 已安装依赖：GCC/G++ 11.4.0、CMake 3.22.1、Ninja 1.10.1、ApacheBench、`linux-tools-generic`。
- WSL 文件系统注意事项：仓库位于 `/mnt/e`，构建目录使用 `/tmp/coroutine-lib-build-debug`，避免挂载盘权限和文件系统行为影响构建。
- 历史记录：2026-09-02 Windows 会话曾因管理员权限和 WSL 初始化失败而无法验证；该限制已通过完成 WSL 初始化解决。

## Linux 验证命令

```sh
cmake --preset debug
cmake --build --preset debug
ctest --test-dir build/debug --output-on-failure
benchmarks/run_benchmarks.sh --mode baseline
```

## 结果

| 项目 | 结果 | 证据 |
|---|---|---|
| 顶层 CMake 配置 | 通过 | `cmake --preset debug`；输出 `coroutine runtime M0 skeleton configured` |
| Debug 构建 | 通过 | `cmake --build --preset debug`；4 个目标步骤完成 |
| CTest | 通过 | `1/1 Test #1: config_test ... Passed` |
| 三次基线压测 | 通过 | ApacheBench 均完成 1000 请求且失败数为 0 |
| perf 上下文切换 | 不可用 | WSL 微软内核不支持当前 `perf stat`，脚本记录 `perf: unavailable` |
| perf Cache Miss | 不可用 | WSL 微软内核不支持当前 `perf stat`，脚本记录 `perf: unavailable` |

### Linux WSL 实测基线

构建和测试使用的命令：

```sh
cmake --preset debug
cmake --build --preset debug
ctest --test-dir /tmp/coroutine-lib-build-debug --output-on-failure
benchmarks/run_benchmarks.sh --mode baseline --output-dir /tmp/coroutine-m0-bench-2
```

压测报告：`/tmp/coroutine-m0-bench-2/baseline-20260903-163435.md`；原始输出目录：`/tmp/coroutine-m0-bench-2/raw/`。

| Run | 完成请求 | 失败请求 | QPS | P99 |
|---:|---:|---:|---:|---:|
| 1 | 1000 | 0 | 29649.84 | 2 ms |
| 2 | 1000 | 0 | 37372.00 | 2 ms |
| 3 | 1000 | 0 | 34373.71 | 2 ms |

`perf stat` 在当前 WSL2 微软内核不可用，脚本已先探测真实可用性；因此不填充虚构的上下文切换和 Cache Miss 数值，但 ApacheBench 基线仍正常执行并记录。

## 当前环境可执行的补充验证

以下检查在 Windows PowerShell/Git Bash 环境中实际执行并通过：

| 检查 | 结果 | 实际输出 |
|---|---|---|
| 配置测试源文件的 MinGW 编译与运行 | 通过 | `PASS config_test skeleton` |
| 采集脚本语法检查 | 通过 | `bash -n: PASS` |
| M0 文件格式检查 | 通过 | `git diff --check (M0 paths): PASS` |
| 基线采集脚本 | 不可用 | 已生成报告；缺少 `build/debug/benchmarks/baseline_server` |

## M0 门禁状态

M0 测试已完成，满足提交和推送条件。

- M0 实现提交：`1c84909`（`M0: establish runtime build and baseline`）
- 目标远端：`https://github.com/Baiye-3000/coroutine-library.git`
- 已推送分支：`feature-x`
- 报告修正提交：待本次报告更新提交后补充

M0 在 Linux 环境完成后，必须追加三次原始输出路径、汇总统计、实际提交号和远端分支，不得删除本节环境限制记录。

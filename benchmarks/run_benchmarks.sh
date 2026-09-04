#!/usr/bin/env bash
set -u

mode=""
output_root="out/benchmarks"
build_dir="${COROUTINE_BUILD_DIR:-build/debug}"
for arg in "$@"; do
    case "$arg" in
        --mode) : ;;
        baseline|load-balance|capacity|comparison) mode="$arg" ;;
        --output-root=*) output_root="${arg#*=}" ;;
        --mode=*) mode="${arg#*=}" ;;
    esac
done

if [[ -z "$mode" ]]; then
    echo "usage: $0 --mode baseline|load-balance|capacity|comparison" >&2
    exit 2
fi

mkdir -p "$output_root/raw"
report="$output_root/${mode}-$(date +%Y%m%d-%H%M%S).md"
{
    echo "# Benchmark run"
    echo
    echo "- Mode: $mode"
    echo "- Date: $(date --iso-8601=seconds 2>/dev/null || date)"
    echo "- Git commit: $(git rev-parse HEAD 2>/dev/null || echo unavailable)"
    echo "- Host CPU: $(nproc 2>/dev/null || echo unavailable) online CPUs"
    echo "- Kernel: $(uname -sr 2>/dev/null || echo unavailable)"
    echo "- Compiler: $(c++ --version 2>/dev/null | head -n 1 || echo unavailable)"
    echo "- Server command: baseline_server --port PORT --connections 1000 --duration 12"
    echo "- Client command: ab -n 1000 -c 32 http://127.0.0.1:PORT/"
    echo "- Repetitions: 3"
    echo "- perf command: perf stat -e context-switches,cache-misses ab -n 1000 -c 32 URL (when perf is available)"
    echo
} > "$report"

if [[ "$mode" == "load-balance" ]]; then
    benchmark="$build_dir/benchmarks/scheduler_benchmark"
    if [[ ! -x "$benchmark" ]]; then
        echo "Status: unavailable (missing $benchmark; build the debug benchmark target first)." >> "$report"
        echo "$report"
        exit 0
    fi
    for run in 1 2 3; do
        raw="$output_root/raw/load-balance-$run.txt"
        "$benchmark" 4 100000 > "$raw" 2>&1 || true
        echo "- Run $run raw output: $raw" >> "$report"
    done
    echo "- Status: collected three load-balance runs" >> "$report"
    echo "$report"
    exit 0
fi

if [[ "$mode" == "capacity" ]]; then
    benchmark="$build_dir/benchmarks/coroutine_capacity"
    count=100000
    for arg in "$@"; do
        case "$arg" in --coroutines) : ;; --coroutines=*) count="${arg#*=}" ;; esac
    done
    if [[ ! -x "$benchmark" ]]; then
        echo "Status: unavailable (missing $benchmark; build the benchmark target first)." >> "$report"
        echo "$report"
        exit 0
    fi
    for strategy in dynamic pool; do
        raw="$output_root/raw/capacity-$strategy.txt"
        "$benchmark" "$count" "$strategy" > "$raw" 2>&1 || true
        echo "- $strategy raw output: $raw" >> "$report"
    done
    echo "- Status: collected capacity runs for $count coroutines" >> "$report"
    echo "$report"
    exit 0
fi

if [[ "$mode" == "comparison" ]]; then
    benchmark="$build_dir/benchmarks/scheduler_benchmark"
    if [[ -x "$benchmark" ]]; then
        for run in 1 2 3; do
            raw="$output_root/raw/comparison-$run.txt"
            "$benchmark" 4 100000 > "$raw" 2>&1 || true
            echo "- Run $run runtime raw output: $raw" >> "$report"
        done
    else
        echo "- Runtime comparison: unavailable (missing $benchmark)" >> "$report"
    fi
    if command -v perf >/dev/null 2>&1 && perf stat -o /dev/null true >/dev/null 2>&1; then
        echo "- perf: available; use baseline mode for ApacheBench counters" >> "$report"
    else
        echo "- perf: unavailable (WSL kernel does not permit perf stat)" >> "$report"
    fi
    echo "- Status: collected runtime comparison data" >> "$report"
    echo "$report"
    exit 0
fi

if [[ "$mode" != "baseline" ]]; then
    echo "Status: unavailable (mode is not implemented in this milestone)." >> "$report"
    echo "$report"
    exit 0
fi

server="$build_dir/benchmarks/baseline_server"
if [[ ! -x "$server" ]]; then
    echo "Status: unavailable (missing $server; build the debug benchmark target first)." >> "$report"
    echo "$report"
    exit 0
fi

if ! command -v curl >/dev/null 2>&1; then
    echo "Status: unavailable (curl is not installed)." >> "$report"
    echo "$report"
    exit 0
fi

if ! command -v ab >/dev/null 2>&1; then
    echo "Status: unavailable (ApacheBench 'ab' is not installed)." >> "$report"
    echo "$report"
    exit 0
fi

perf_available=0
if command -v perf >/dev/null 2>&1 && perf stat -o /dev/null true >/dev/null 2>&1; then
    perf_available=1
fi

port=$((18080 + RANDOM % 1000))
for run in 1 2 3; do
    raw="$output_root/raw/baseline-$run.txt"
    "$server" --port "$port" --connections 1000 --duration 12 > "$output_root/raw/server-$run.log" 2>&1 &
    server_pid=$!
    trap 'kill "$server_pid" 2>/dev/null || true' EXIT
    sleep 1
    if [[ "$perf_available" -eq 1 ]]; then
        perf stat -o "$output_root/raw/perf-$run.txt" -e context-switches,cache-misses \
            ab -n 1000 -c 32 "http://127.0.0.1:$port/" > "$raw" 2>&1 || true
    else
        echo "perf: unavailable (perf stat is not supported by this kernel)" > "$output_root/raw/perf-$run.txt"
        ab -n 1000 -c 32 "http://127.0.0.1:$port/" > "$raw" 2>&1 || true
    fi
    kill "$server_pid" 2>/dev/null || true
    wait "$server_pid" 2>/dev/null || true
    trap - EXIT
    port=$((port + 1))
    echo "- Run $run raw output: $raw" >> "$report"
done

echo "- Status: collected three baseline runs" >> "$report"
echo "$report"

#include "coroutine/scheduler.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

int main(int argc, char** argv) {
    const int workers = argc > 1 ? std::atoi(argv[1]) : 4;
    const int tasks = argc > 2 ? std::atoi(argv[2]) : 100000;
    coroutine::RuntimeConfig config;
    config.worker_count = workers > 0 ? static_cast<std::size_t>(workers) : 1;
    config.strategy = coroutine::SchedulingStrategy::adaptive;
    coroutine::Scheduler scheduler(config);
    scheduler.start();
    std::atomic<int> completed{0};
    const auto started = std::chrono::steady_clock::now();
    for (int index = 0; index < tasks; ++index) {
        scheduler.submit([&] { completed.fetch_add(1, std::memory_order_relaxed); });
    }
    while (completed.load() != tasks) std::this_thread::sleep_for(std::chrono::milliseconds{1});
    const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    const auto snapshot = scheduler.load_snapshot();
    std::size_t total = 0;
    for (const auto& worker : snapshot) total += worker.completed_count;
    std::printf("workers=%d tasks=%d seconds=%.6f completed=%d snapshot_completed=%zu\n",
                workers, tasks, elapsed, completed.load(), total);
    scheduler.stop();
    return completed == tasks ? 0 : 1;
}

#include "../test_framework.h"
#include "coroutine/scheduler.h"

#include <atomic>
#include <chrono>
#include <thread>

int main() {
    coroutine::RuntimeConfig config;
    config.worker_count = 2;
    coroutine::Scheduler scheduler(config);
    EXPECT_TRUE(!scheduler.is_running());
    EXPECT_TRUE(scheduler.submit([] {}) == coroutine::SubmitResult::stopped);
    scheduler.start();
    std::atomic<int> completed{0};
    for (int index = 0; index < 20; ++index) {
        EXPECT_TRUE(scheduler.submit_to(0, [&completed] { ++completed; }) ==
                    coroutine::SubmitResult::accepted);
    }
    EXPECT_TRUE(scheduler.submit_to(99, [] {}) == coroutine::SubmitResult::invalid_worker);
    for (int attempt = 0; attempt < 100 && completed.load() != 20; ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds{2});
    }
    EXPECT_TRUE(completed.load() == 20);
    EXPECT_TRUE(scheduler.load_snapshot().size() == 2);
    for (const auto& worker : scheduler.load_snapshot()) EXPECT_TRUE(worker.cpu_id >= -1);
    scheduler.stop();
    EXPECT_TRUE(!scheduler.is_running());
    EXPECT_TRUE(scheduler.submit([] {}) == coroutine::SubmitResult::stopped);
    return test::finish("scheduler_test");
}

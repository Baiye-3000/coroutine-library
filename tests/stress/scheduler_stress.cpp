#include "../test_framework.h"
#include "coroutine/scheduler.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

int main() {
    coroutine::RuntimeConfig config;
    config.worker_count = 4;
    coroutine::Scheduler scheduler(config);
    scheduler.start();
    constexpr int producers = 4;
    constexpr int per_producer = 500;
    std::atomic<int> completed{0};
    std::vector<std::thread> submitters;
    for (int producer = 0; producer < producers; ++producer) {
        submitters.emplace_back([&] {
            for (int index = 0; index < per_producer; ++index) {
                while (scheduler.submit([&completed] { ++completed; }) !=
                       coroutine::SubmitResult::accepted) {
                    std::this_thread::yield();
                }
            }
        });
    }
    for (auto& submitter : submitters) {
        submitter.join();
    }
    for (int attempt = 0; attempt < 500 && completed.load() != producers * per_producer;
         ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds{2});
    }
    EXPECT_TRUE(completed.load() == producers * per_producer);
    scheduler.stop();
    return test::finish("scheduler_stress");
}

#include "../test_framework.h"

#include "coroutine/metrics.h"

#include <chrono>
#include <vector>
#include <atomic>
#include <thread>

int main() {
    using coroutine::WorkerLoadSnapshot;
    coroutine::MetricsCollector collector;
    EXPECT_TRUE(collector.interval() == std::chrono::milliseconds{100});
    const auto result = collector.collect([] {
        return std::vector<WorkerLoadSnapshot>{
            {0, 0, 2, 0.25, 4, std::chrono::microseconds{30}, true, false, {}},
            {1, 1, 0, 0.05, 6, std::chrono::microseconds{10}, true, true, {}}
        };
    }, 10, 3, 7, 1);
    EXPECT_TRUE(result.workers.size() == 2);
    EXPECT_TRUE(result.completed_count == 10);
    EXPECT_TRUE(result.submitted_count == 10);
    EXPECT_TRUE(result.steal_count == 3);
    EXPECT_TRUE(result.context_switch_count == 7);
    EXPECT_TRUE(result.waiting_io_count == 1);
    auto copy = collector.snapshot();
    copy.workers[0].queue_depth = 99;
    EXPECT_TRUE(collector.snapshot().workers[0].queue_depth == 2);
    std::atomic<int> samples{0};
    collector.start([&] {
        ++samples;
        return std::vector<WorkerLoadSnapshot>{{0, 0, static_cast<std::size_t>(samples.load()), 0.1, 1, {}, true, false, {}}};
    });
    for (int attempt = 0; attempt < 50 && samples.load() < 2; ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds{5});
    }
    EXPECT_TRUE(samples.load() >= 2);
    collector.stop();
    const auto stopped_samples = samples.load();
    std::this_thread::sleep_for(std::chrono::milliseconds{20});
    EXPECT_TRUE(samples.load() == stopped_samples);
    return test::finish("metrics_test");
}

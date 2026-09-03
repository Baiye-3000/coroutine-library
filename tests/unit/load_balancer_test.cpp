#include "../test_framework.h"

#include "coroutine/load_balancer.h"

#include <chrono>
#include <vector>

namespace {
std::vector<coroutine::WorkerLoadSnapshot> workers() {
    return {
        {0, 0, 8, 0.8, 10, std::chrono::microseconds{800}, true, false, {}},
        {1, 1, 1, 0.1, 2, std::chrono::microseconds{100}, true, false, {}},
        {2, 2, 4, 0.4, 5, std::chrono::microseconds{400}, true, false, {}}
    };
}
}

int main() {
    const auto snapshot = workers();
    coroutine::RuntimeConfig config;
    config.strategy = coroutine::SchedulingStrategy::round_robin;
    coroutine::LoadBalancer round_robin(config);
    EXPECT_TRUE(round_robin.select(snapshot).worker == 0);
    EXPECT_TRUE(round_robin.select(snapshot).worker == 1);
    config.strategy = coroutine::SchedulingStrategy::least_load;
    coroutine::LoadBalancer least_load(config);
    EXPECT_TRUE(least_load.select(snapshot).worker == 1);
    config.strategy = coroutine::SchedulingStrategy::affinity_first;
    coroutine::LoadBalancer affinity(config);
    EXPECT_TRUE(affinity.select(snapshot, 2).worker == 2);
    EXPECT_TRUE(affinity.select(snapshot, 99).worker == 1);
    config.strategy = coroutine::SchedulingStrategy::adaptive;
    config.affinity_bonus = 0.0;
    coroutine::LoadBalancer adaptive(config);
    EXPECT_TRUE(adaptive.select(snapshot).worker == 1);
    const auto empty = adaptive.select({});
    EXPECT_TRUE(!empty);
    EXPECT_TRUE(empty.error == coroutine::SelectionError::empty_snapshot);
    return test::finish("load_balancer_test");
}

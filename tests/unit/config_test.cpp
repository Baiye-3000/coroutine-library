#include "../test_framework.h"

#include "coroutine/config.h"

#include <chrono>
#include <string>

int main() {
    coroutine::RuntimeConfig config;
    EXPECT_TRUE(coroutine::default_worker_count() >= 1);
    EXPECT_TRUE(coroutine::effective_worker_count(config) >= 1);
    config.worker_count = 3;
    EXPECT_TRUE(coroutine::effective_worker_count(config) == 3);
    std::string error;
    EXPECT_TRUE(coroutine::validate_config(config, &error));
    config.pin_workers = true;
    EXPECT_TRUE(coroutine::validate_config(config, &error));
    config.metrics_interval = std::chrono::milliseconds{0};
    EXPECT_TRUE(!coroutine::validate_config(config, &error));
    config.metrics_interval = std::chrono::milliseconds{100};
    config.queue_depth_weight = 0.5;
    EXPECT_TRUE(!coroutine::validate_config(config, &error));
    config.queue_depth_weight = 0.4;
    config.idle_threshold = 0.95;
    EXPECT_TRUE(!coroutine::validate_config(config, &error));
    return test::finish("config_test");
}

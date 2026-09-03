#pragma once

#include <chrono>
#include <cstddef>
#include <string>

namespace coroutine {

enum class SchedulingStrategy {
    round_robin,
    least_load,
    affinity_first,
    adaptive
};

struct RuntimeConfig {
    std::size_t worker_count = 0;
    SchedulingStrategy strategy = SchedulingStrategy::round_robin;
    std::chrono::milliseconds metrics_interval{100};
    double affinity_max_load = 0.7;
    double overload_threshold = 0.9;
    double idle_threshold = 0.2;
    double queue_depth_weight = 0.40;
    double cpu_usage_weight = 0.30;
    double wait_time_weight = 0.20;
    double history_weight = 0.10;
    double affinity_bonus = 0.10;
};

std::size_t default_worker_count() noexcept;
std::size_t effective_worker_count(const RuntimeConfig& config) noexcept;
bool validate_config(const RuntimeConfig& config, std::string* error = nullptr);

} // namespace coroutine

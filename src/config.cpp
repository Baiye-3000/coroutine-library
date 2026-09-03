#include "coroutine/config.h"

#include <cmath>
#include <thread>

namespace coroutine {

std::size_t default_worker_count() noexcept {
    const auto count = std::thread::hardware_concurrency();
    return count == 0 ? 1 : static_cast<std::size_t>(count);
}

std::size_t effective_worker_count(const RuntimeConfig& config) noexcept {
    return config.worker_count == 0 ? default_worker_count() : config.worker_count;
}

bool validate_config(const RuntimeConfig& config, std::string* error) {
    const auto fail = [error](const char* message) {
        if (error != nullptr) {
            *error = message;
        }
        return false;
    };
    if (config.worker_count == 0) {
        // Zero is the documented automatic CPU-count mode.
    }
    if (config.metrics_interval.count() <= 0) {
        return fail("metrics_interval must be positive");
    }
    const double values[] = {
        config.affinity_max_load, config.overload_threshold, config.idle_threshold,
        config.queue_depth_weight, config.cpu_usage_weight, config.wait_time_weight,
        config.history_weight, config.affinity_bonus
    };
    for (double value : values) {
        if (!std::isfinite(value) || value < 0.0 || value > 1.0) {
            return fail("configuration values must be finite and in [0, 1]");
        }
    }
    const double weight_sum = config.queue_depth_weight + config.cpu_usage_weight +
                              config.wait_time_weight + config.history_weight;
    if (std::abs(weight_sum - 1.0) > 1e-9) {
        return fail("adaptive weights must sum to 1");
    }
    if (config.idle_threshold > config.overload_threshold) {
        return fail("idle_threshold must not exceed overload_threshold");
    }
    return true;
}

} // namespace coroutine

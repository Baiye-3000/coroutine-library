#pragma once

#include "coroutine/config.h"

#include <atomic>
#include <cstddef>
#include <optional>
#include <vector>

namespace coroutine {

struct WorkerLoadSnapshot {
    std::size_t worker_id = 0;
    int cpu_id = -1;
    std::size_t queue_depth = 0;
    double cpu_usage = 0.0;
    std::size_t completed_count = 0;
    std::chrono::microseconds total_queue_wait{0};
    bool running = false;
    bool idle = true;
    std::chrono::steady_clock::time_point last_update{};
};

enum class SelectionError {
    empty_snapshot,
    invalid_configuration
};

struct SelectionResult {
    std::optional<std::size_t> worker;
    SelectionError error = SelectionError::empty_snapshot;

    explicit operator bool() const noexcept { return worker.has_value(); }
};

class LoadBalancer {
public:
    explicit LoadBalancer(RuntimeConfig config = {});

    SelectionResult select(const std::vector<WorkerLoadSnapshot>& snapshot,
                           std::optional<std::size_t> affinity_worker = std::nullopt) const;
    void update(RuntimeConfig config);
    RuntimeConfig config() const;

private:
    RuntimeConfig config_;
    mutable std::atomic<std::size_t> round_robin_index_{0};
};

} // namespace coroutine

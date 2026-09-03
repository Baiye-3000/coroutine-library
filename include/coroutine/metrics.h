#pragma once

#include "coroutine/load_balancer.h"

#include <chrono>
#include <functional>
#include <vector>

namespace coroutine {

struct RuntimeMetrics {
    std::vector<WorkerLoadSnapshot> workers;
    std::chrono::steady_clock::time_point sampled_at{};
    std::size_t completed_count = 0;
    std::size_t steal_count = 0;
    std::size_t submitted_count = 0;
    std::size_t context_switch_count = 0;
    std::size_t waiting_io_count = 0;
};

class MetricsCollector {
public:
    using SnapshotSource = std::function<std::vector<WorkerLoadSnapshot>()>;

    explicit MetricsCollector(std::chrono::milliseconds interval =
                                  std::chrono::milliseconds{100});

    RuntimeMetrics collect(const SnapshotSource& source,
                           std::size_t submitted_count = 0,
                           std::size_t steal_count = 0,
                           std::size_t context_switch_count = 0,
                           std::size_t waiting_io_count = 0) const;
    RuntimeMetrics snapshot() const;
    std::chrono::milliseconds interval() const noexcept;

private:
    std::chrono::milliseconds interval_;
    mutable RuntimeMetrics latest_;
};

} // namespace coroutine

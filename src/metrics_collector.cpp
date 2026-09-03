#include "coroutine/metrics.h"

#include <numeric>
#include <stdexcept>

namespace coroutine {

MetricsCollector::MetricsCollector(std::chrono::milliseconds interval)
    : interval_(interval) {
    if (interval_.count() <= 0) {
        throw std::invalid_argument("metrics interval must be positive");
    }
}

RuntimeMetrics MetricsCollector::collect(const SnapshotSource& source,
                                         std::size_t submitted_count,
                                         std::size_t steal_count,
                                         std::size_t context_switch_count,
                                         std::size_t waiting_io_count) const {
    if (!source) {
        throw std::invalid_argument("metrics snapshot source is empty");
    }
    RuntimeMetrics result;
    result.workers = source();
    result.sampled_at = std::chrono::steady_clock::now();
    result.submitted_count = submitted_count;
    result.steal_count = steal_count;
    result.context_switch_count = context_switch_count;
    result.waiting_io_count = waiting_io_count;
    result.completed_count = std::accumulate(
        result.workers.begin(), result.workers.end(), std::size_t{0},
        [](std::size_t total, const WorkerLoadSnapshot& worker) {
            return total + worker.completed_count;
        });
    latest_ = result;
    return result;
}

RuntimeMetrics MetricsCollector::snapshot() const {
    return latest_;
}

std::chrono::milliseconds MetricsCollector::interval() const noexcept {
    return interval_;
}

} // namespace coroutine

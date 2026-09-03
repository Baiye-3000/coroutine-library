#include "coroutine/load_balancer.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace coroutine {
namespace {

double bounded(double value) {
    return std::clamp(std::isfinite(value) ? value : 1.0, 0.0, 1.0);
}

double score(const WorkerLoadSnapshot& worker, const RuntimeConfig& config,
             std::size_t max_queue, std::chrono::microseconds max_wait) {
    const double queue = max_queue == 0 ? 0.0 :
        bounded(static_cast<double>(worker.queue_depth) / static_cast<double>(max_queue));
    const double wait = max_wait.count() == 0 ? 0.0 :
        bounded(static_cast<double>(worker.total_queue_wait.count()) /
                static_cast<double>(max_wait.count()));
    const double history = worker.completed_count == 0 ? 0.0 :
        bounded(1.0 / (1.0 + static_cast<double>(worker.completed_count)));
    return config.queue_depth_weight * queue +
           config.cpu_usage_weight * bounded(worker.cpu_usage) +
           config.wait_time_weight * wait +
           config.history_weight * history;
}

std::size_t least_loaded(const std::vector<WorkerLoadSnapshot>& snapshot,
                         const RuntimeConfig& config) {
    std::size_t max_queue = 0;
    std::chrono::microseconds max_wait{0};
    for (const auto& worker : snapshot) {
        max_queue = std::max(max_queue, worker.queue_depth);
        max_wait = std::max(max_wait, worker.total_queue_wait);
    }
    std::size_t selected = 0;
    double selected_score = std::numeric_limits<double>::infinity();
    for (std::size_t index = 0; index < snapshot.size(); ++index) {
        const double current = score(snapshot[index], config, max_queue, max_wait);
        if (current < selected_score) {
            selected_score = current;
            selected = index;
        }
    }
    return selected;
}

} // namespace

LoadBalancer::LoadBalancer(RuntimeConfig config) : config_(config) {}

SelectionResult LoadBalancer::select(const std::vector<WorkerLoadSnapshot>& snapshot,
                                     std::optional<std::size_t> affinity_worker) const {
    if (snapshot.empty()) {
        return {std::nullopt, SelectionError::empty_snapshot};
    }
    if (!validate_config(config_)) {
        return {std::nullopt, SelectionError::invalid_configuration};
    }
    const auto affinity_is_usable = [&] {
        return affinity_worker.has_value() &&
               *affinity_worker < snapshot.size() &&
               snapshot[*affinity_worker].cpu_usage <= config_.affinity_max_load;
    };
    switch (config_.strategy) {
    case SchedulingStrategy::round_robin:
        return {round_robin_index_.fetch_add(1) % snapshot.size(), {}};
    case SchedulingStrategy::least_load:
        return {least_loaded(snapshot, config_), {}};
    case SchedulingStrategy::affinity_first:
        if (affinity_is_usable()) {
            return {*affinity_worker, {}};
        }
        return {least_loaded(snapshot, config_), {}};
    case SchedulingStrategy::adaptive: {
        auto selected = least_loaded(snapshot, config_);
        double best = score(snapshot[selected], config_, 0, std::chrono::microseconds{0});
        for (std::size_t index = 0; index < snapshot.size(); ++index) {
            double current = score(snapshot[index], config_, 0, std::chrono::microseconds{0});
            if (affinity_worker && *affinity_worker == index &&
                snapshot[index].cpu_usage <= config_.affinity_max_load) {
                current -= config_.affinity_bonus;
            }
            if (current < best) {
                best = current;
                selected = index;
            }
        }
        return {selected, {}};
    }
    }
    return {std::nullopt, SelectionError::invalid_configuration};
}

void LoadBalancer::update(RuntimeConfig config) {
    config_ = config;
}

RuntimeConfig LoadBalancer::config() const {
    return config_;
}

} // namespace coroutine

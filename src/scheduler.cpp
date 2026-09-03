#include "coroutine/scheduler.h"

#include <chrono>
#include <stdexcept>

namespace coroutine {

Scheduler::Scheduler(RuntimeConfig config)
    : config_(config), load_balancer_(config_) {
    if (!validate_config(config_)) {
        throw std::invalid_argument("invalid runtime configuration");
    }
    const auto count = effective_worker_count(config_);
    workers_.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        auto worker = std::make_unique<Worker>(index);
        worker->id = index;
        workers_.push_back(std::move(worker));
    }
}

Scheduler::~Scheduler() { stop(); }

void Scheduler::start() {
    std::lock_guard<std::mutex> lock(lifecycle_mutex_);
    if (running_.exchange(true)) {
        return;
    }
    stopping_.store(false);
    for (auto& worker : workers_) {
        worker->thread = std::thread(&Scheduler::run_worker, this, std::ref(*worker));
    }
}

void Scheduler::stop() {
    std::lock_guard<std::mutex> lock(lifecycle_mutex_);
    if (!running_.exchange(false)) {
        return;
    }
    stopping_.store(true);
    for (auto& worker : workers_) {
        worker->ingress.close();
        worker->wake.notify_all();
    }
    for (auto& worker : workers_) {
        if (worker->thread.joinable()) {
            worker->thread.join();
        }
    }
}

bool Scheduler::is_running() const noexcept { return running_.load(); }

SubmitResult Scheduler::submit(Task task, SubmitOptions options) {
    if (!task) {
        return SubmitResult::invalid_task;
    }
    if (!running_.load() || stopping_.load()) {
        return SubmitResult::stopped;
    }
    const auto snapshot = load_snapshot();
    const auto selection = load_balancer_.select(snapshot, options.affinity_worker);
    if (!selection) {
        return SubmitResult::stopped;
    }
    return enqueue(*selection.worker, std::move(task));
}

SubmitResult Scheduler::submit_to(std::size_t worker, Task task) {
    if (!task) {
        return SubmitResult::invalid_task;
    }
    if (worker >= workers_.size()) {
        return SubmitResult::invalid_worker;
    }
    if (!running_.load() || stopping_.load()) {
        return SubmitResult::stopped;
    }
    return enqueue(worker, std::move(task));
}

SubmitResult Scheduler::enqueue(std::size_t worker, Task task) {
    auto node = std::make_unique<RunnableTask>();
    node->function = std::move(task);
    node->submitted_worker = worker;
    auto& target = *workers_[worker];
    if (stopping_.load() || !target.ingress.push(node.get())) return SubmitResult::stopped;
    node.release();
    submitted_count_.fetch_add(1);
    target.wake.notify_one();
    return SubmitResult::accepted;
}

RunnableTask* Scheduler::take_task(Worker& worker) {
    if (auto* task = worker.local.pop()) {
        return task;
    }
    for (auto* task = worker.ingress.drain(); task != nullptr;) {
            auto* next = task->next;
            task->next = nullptr;
            worker.local.push(task);
            task = next;
    }
    if (auto* task = worker.local.pop()) {
        return task;
    }
    for (auto& candidate : workers_) {
        if (candidate->id == worker.id) {
            continue;
        }
        if (auto* task = candidate->local.steal()) {
            steal_count_.fetch_add(1);
            return task;
        }
    }
    return nullptr;
}

void Scheduler::run_worker(Worker& worker) {
    {
        std::lock_guard<std::mutex> lock(worker.snapshot_mutex);
        worker.snapshot.running = true;
        worker.snapshot.idle = false;
        worker.snapshot.last_update = std::chrono::steady_clock::now();
    }
    while (running_.load() || worker.local.approximate_size() != 0) {
        auto* task = take_task(worker);
        if (task == nullptr) {
            {
                std::lock_guard<std::mutex> lock(worker.snapshot_mutex);
                worker.snapshot.idle = true;
                worker.snapshot.queue_depth = worker.local.approximate_size();
                worker.snapshot.last_update = std::chrono::steady_clock::now();
            }
            std::unique_lock<std::mutex> lock(worker.wake_mutex);
            worker.wake.wait_for(lock, std::chrono::milliseconds{10}, [this] {
                return !running_.load() || stopping_.load();
            });
            continue;
        }
        {
            std::lock_guard<std::mutex> lock(worker.snapshot_mutex);
            worker.snapshot.idle = false;
            worker.snapshot.queue_depth = worker.local.approximate_size();
        }
        try {
            task->function();
        } catch (...) {
            // A callback cannot terminate the worker thread.
        }
        delete task;
        completed_count_.fetch_add(1);
        {
            std::lock_guard<std::mutex> lock(worker.snapshot_mutex);
            ++worker.snapshot.completed_count;
            worker.snapshot.queue_depth = worker.local.approximate_size();
            worker.snapshot.last_update = std::chrono::steady_clock::now();
        }
    }
    {
        std::lock_guard<std::mutex> lock(worker.snapshot_mutex);
        worker.snapshot.running = false;
        worker.snapshot.idle = true;
    }
}

RuntimeConfig Scheduler::config() const { return config_; }

std::vector<WorkerLoadSnapshot> Scheduler::load_snapshot() const {
    std::vector<WorkerLoadSnapshot> result;
    result.reserve(workers_.size());
    for (const auto& worker : workers_) {
        std::lock_guard<std::mutex> lock(worker->snapshot_mutex);
        auto snapshot = worker->snapshot;
        snapshot.queue_depth = worker->local.approximate_size();
        result.push_back(snapshot);
    }
    return result;
}

RuntimeMetrics Scheduler::metrics_snapshot() const {
    MetricsCollector collector(config_.metrics_interval);
    return collector.collect([this] { return load_snapshot(); },
                             submitted_count_.load(), steal_count_.load());
}

} // namespace coroutine

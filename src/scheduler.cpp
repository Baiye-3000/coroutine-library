#include "coroutine/scheduler.h"

#include <chrono>
#include <stdexcept>
#include <algorithm>

namespace coroutine {

thread_local Scheduler* Scheduler::current_scheduler_ = nullptr;
thread_local std::size_t Scheduler::current_worker_id_ = static_cast<std::size_t>(-1);

Scheduler::Scheduler(RuntimeConfig config)
    : config_(config), load_balancer_(config_), metrics_collector_(config.metrics_interval) {
    if (!validate_config(config_)) {
        throw std::invalid_argument("invalid runtime configuration");
    }
    const auto count = effective_worker_count(config_);
    workers_.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        auto worker = std::make_unique<Worker>(index);
        worker->id = index;
        worker->event_loop = std::make_unique<EventLoop>(index);
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
    metrics_collector_.start([this] { return load_snapshot(); });
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
    metrics_collector_.stop();
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

SubmitResult Scheduler::submit_coroutine(std::shared_ptr<Coroutine> coroutine, SubmitOptions options) {
    if (!coroutine) return SubmitResult::invalid_task;
    return submit([this, coroutine = std::move(coroutine)] {
        Coroutine::set_current_owner(coroutine);
        if (coroutine->state() == CoroutineState::ready || coroutine->state() == CoroutineState::waiting) {
            coroutine->resume();
        }
        Coroutine::set_current_owner(nullptr);
        if (coroutine->state() == CoroutineState::ready) {
            submit_coroutine(coroutine, SubmitOptions{current_worker_id_});
        }
    }, std::move(options));
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
    current_scheduler_ = this;
    current_worker_id_ = worker.id;
    const auto cpus = CpuAffinity::online_cpus();
    if (!cpus.empty()) {
        worker.cpu_id = cpus[worker.id % cpus.size()];
        if (config_.pin_workers && !CpuAffinity::bind_current_thread(worker.cpu_id)) worker.cpu_id = -1;
    }
    {
        std::lock_guard<std::mutex> lock(worker.snapshot_mutex);
        worker.snapshot.running = true;
        worker.snapshot.cpu_id = worker.cpu_id;
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
            lock.unlock();
            worker.event_loop->run_once(std::chrono::milliseconds{10});
            continue;
        }
        {
            std::lock_guard<std::mutex> lock(worker.snapshot_mutex);
            worker.snapshot.idle = false;
            worker.snapshot.queue_depth = worker.local.approximate_size();
        }
        const auto started = std::chrono::steady_clock::now();
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
            worker.snapshot.cpu_usage = std::clamp(
                std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count() / 0.01,
                0.0, 1.0);
            worker.snapshot.queue_depth = worker.local.approximate_size();
            worker.snapshot.last_update = std::chrono::steady_clock::now();
        }
    }
    {
        std::lock_guard<std::mutex> lock(worker.snapshot_mutex);
        worker.snapshot.running = false;
        worker.snapshot.idle = true;
    }
    current_scheduler_ = nullptr;
    current_worker_id_ = static_cast<std::size_t>(-1);
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

Scheduler* Scheduler::current() noexcept { return current_scheduler_; }
std::size_t Scheduler::current_worker_id() noexcept { return current_worker_id_; }
EventLoop* Scheduler::current_event_loop() noexcept {
    if (current_scheduler_ == nullptr || current_worker_id_ >= current_scheduler_->workers_.size()) return nullptr;
    return current_scheduler_->workers_[current_worker_id_]->event_loop.get();
}

} // namespace coroutine

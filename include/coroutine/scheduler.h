#pragma once

#include "coroutine/load_balancer.h"
#include "coroutine/metrics.h"
#include "coroutine/work_stealing_deque.h"
#include "coroutine/coroutine.h"
#include "coroutine/event_loop.h"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

namespace coroutine {

enum class SubmitResult { accepted, stopped, invalid_worker, invalid_task, queue_full };
using Task = std::function<void()>;

struct SubmitOptions {
    std::optional<std::size_t> affinity_worker;
};

class Scheduler {
public:
    explicit Scheduler(RuntimeConfig config = {});
    ~Scheduler();

    void start();
    void stop();
    bool is_running() const noexcept;
    SubmitResult submit(Task task, SubmitOptions options = {});
    SubmitResult submit_to(std::size_t worker, Task task);
    SubmitResult submit_coroutine(std::shared_ptr<Coroutine> coroutine,
                                  SubmitOptions options = {});
    RuntimeConfig config() const;
    std::vector<WorkerLoadSnapshot> load_snapshot() const;
    RuntimeMetrics metrics_snapshot() const;
    static Scheduler* current() noexcept;
    static std::size_t current_worker_id() noexcept;
    static EventLoop* current_event_loop() noexcept;

private:
    struct Worker {
        explicit Worker(std::size_t worker_id) : id(worker_id) {
            snapshot.worker_id = worker_id;
        }
        std::size_t id;
        WorkStealingDeque local;
        IngressQueue ingress;
        WorkerLoadSnapshot snapshot;
        mutable std::mutex snapshot_mutex;
        std::condition_variable wake;
        std::mutex wake_mutex;
        std::thread thread;
        std::unique_ptr<EventLoop> event_loop;
    };

    void run_worker(Worker& worker);
    RunnableTask* take_task(Worker& worker);
    SubmitResult enqueue(std::size_t worker, Task task);

    static thread_local Scheduler* current_scheduler_;
    static thread_local std::size_t current_worker_id_;

    RuntimeConfig config_;
    LoadBalancer load_balancer_;
    std::vector<std::unique_ptr<Worker>> workers_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopping_{false};
    mutable std::mutex lifecycle_mutex_;
    std::atomic<std::size_t> submitted_count_{0};
    std::atomic<std::size_t> completed_count_{0};
    std::atomic<std::size_t> steal_count_{0};
};

} // namespace coroutine

#pragma once

#include "coroutine/timer_queue.h"

#include <functional>
#include <memory>

namespace coroutine {

enum class Event { read, write };
enum class EventResult { accepted, duplicate, closed, invalid_fd };
enum class CancelReason { timeout, closed, shutdown, explicit_cancel };

struct WaitRegistration {
    std::function<void()> callback;
};

class EventLoop {
public:
    explicit EventLoop(std::size_t worker_id);
    ~EventLoop();
    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    EventResult add(int fd, Event event, WaitRegistration registration);
    bool cancel(int fd, Event event, CancelReason reason);
    int run_once(std::chrono::milliseconds timeout);
    void wake();
    TimerQueue& timers() noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace coroutine

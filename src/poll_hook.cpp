#include "coroutine/hook.h"
#include "coroutine/coroutine.h"
#include "coroutine/scheduler.h"

#include <poll.h>
#include <atomic>
#include <memory>

namespace coroutine {

unsigned poll_wait(::pollfd* fds, unsigned count, int timeout_ms) {
    if (!hook_enabled() || Coroutine::current() == nullptr || Scheduler::current_event_loop() == nullptr || count == 0) {
        return static_cast<unsigned>(::poll(fds, count, timeout_ms));
    }
    const auto immediate = ::poll(fds, count, 0);
    if (immediate != 0 || timeout_ms == 0) return static_cast<unsigned>(immediate);
    auto owner = Coroutine::current_owner();
    auto* loop = Scheduler::current_event_loop();
    if (!owner || !loop) return static_cast<unsigned>(::poll(fds, count, timeout_ms));
    auto fired = std::make_shared<std::atomic<bool>>(false);
    for (unsigned index = 0; index < count; ++index) {
        if (fds[index].fd < 0) continue;
        const bool wants_read = (fds[index].events & (POLLIN | POLLPRI)) != 0;
        const bool wants_write = (fds[index].events & POLLOUT) != 0;
        const auto callback = [owner, fired] {
            if (!fired->exchange(true)) {
                if (auto* scheduler = Scheduler::current()) scheduler->submit_coroutine(owner, SubmitOptions{Scheduler::current_worker_id()});
            }
        };
        if (wants_read) loop->add(fds[index].fd, Event::read, WaitRegistration{callback});
        if (wants_write) loop->add(fds[index].fd, Event::write, WaitRegistration{callback});
    }
    TimerHandle timer = 0;
    if (timeout_ms >= 0) {
        timer = loop->timers().add(std::chrono::milliseconds{timeout_ms}, [owner, fired] {
            if (!fired->exchange(true)) {
                if (auto* scheduler = Scheduler::current()) scheduler->submit_coroutine(owner, SubmitOptions{Scheduler::current_worker_id()});
            }
        });
    }
    if (!Coroutine::wait()) return static_cast<unsigned>(-1);
    if (timer != 0) loop->timers().cancel(timer);
    for (unsigned index = 0; index < count; ++index) {
        if (fds[index].fd < 0) continue;
        if (fds[index].events & (POLLIN | POLLPRI)) loop->cancel(fds[index].fd, Event::read, CancelReason::explicit_cancel);
        if (fds[index].events & POLLOUT) loop->cancel(fds[index].fd, Event::write, CancelReason::explicit_cancel);
    }
    return static_cast<unsigned>(::poll(fds, count, 0));
}

} // namespace coroutine

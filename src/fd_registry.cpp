#include "coroutine/fd_registry.h"

namespace coroutine {

EventResult FdRegistry::register_wait(int fd, Event event) {
    if (fd < 0) return EventResult::invalid_fd;
    std::lock_guard<std::mutex> lock(mutex_);
    auto& state = states_[fd];
    if (state.closed) return EventResult::closed;
    const auto index = event == Event::read ? 0u : 1u;
    if (state.waiting[index]) return EventResult::duplicate;
    state.waiting[index] = true;
    return EventResult::accepted;
}

bool FdRegistry::complete(int fd, Event event) {
    return cancel(fd, event, CancelReason::shutdown);
}

bool FdRegistry::cancel(int fd, Event event, CancelReason) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = states_.find(fd);
    if (it == states_.end()) return false;
    auto& waiting = it->second.waiting[event == Event::read ? 0u : 1u];
    if (!waiting) return false;
    waiting = false;
    if (!it->second.waiting[0] && !it->second.waiting[1] && !it->second.closed) states_.erase(it);
    return true;
}

bool FdRegistry::close(int fd) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& state = states_[fd];
    if (state.closed) return false;
    state.closed = true;
    state.waiting = {false, false};
    return true;
}

bool FdRegistry::is_closed(int fd) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = states_.find(fd);
    return it != states_.end() && it->second.closed;
}

} // namespace coroutine

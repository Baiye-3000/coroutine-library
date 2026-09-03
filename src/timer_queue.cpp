#include "coroutine/timer_queue.h"

#include <algorithm>

namespace coroutine {

TimerHandle TimerQueue::add(Duration delay, TimerCallback callback) {
    if (!callback) return 0;
    std::lock_guard<std::mutex> lock(mutex_);
    const auto handle = next_handle_++;
    entries_.push({std::chrono::steady_clock::now() + std::max(delay, Duration{0}),
                   handle, std::move(callback)});
    active_[handle] = true;
    return handle;
}

bool TimerQueue::cancel(TimerHandle handle) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = active_.find(handle);
    if (it == active_.end() || !it->second) return false;
    it->second = false;
    return true;
}

Duration TimerQueue::next_timeout() const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto now = std::chrono::steady_clock::now();
    while (!entries_.empty()) {
        const auto& entry = entries_.top();
        const auto it = active_.find(entry.handle);
        if (it != active_.end() && it->second) {
            if (entry.deadline <= now) return Duration{0};
            return std::chrono::duration_cast<Duration>(entry.deadline - now);
        }
        active_.erase(entry.handle);
        entries_.pop();
    }
    return Duration::max();
}

std::size_t TimerQueue::expire(TimePoint now, std::vector<TimerCallback>& callbacks) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::size_t count = 0;
    while (!entries_.empty() && entries_.top().deadline <= now) {
        auto entry = entries_.top();
        entries_.pop();
        const auto it = active_.find(entry.handle);
        if (it == active_.end() || !it->second) continue;
        active_.erase(it);
        callbacks.push_back(std::move(entry.callback));
        ++count;
    }
    return count;
}

} // namespace coroutine

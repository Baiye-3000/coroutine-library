#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <vector>

namespace coroutine {

using Duration = std::chrono::milliseconds;
using TimePoint = std::chrono::steady_clock::time_point;
using TimerHandle = std::uint64_t;
using TimerCallback = std::function<void()>;

class TimerQueue {
public:
    TimerHandle add(Duration delay, TimerCallback callback);
    bool cancel(TimerHandle handle);
    Duration next_timeout() const;
    std::size_t expire(TimePoint now, std::vector<TimerCallback>& callbacks);

private:
    struct Entry {
        TimePoint deadline;
        TimerHandle handle;
        TimerCallback callback;
        bool operator>(const Entry& other) const noexcept {
            return deadline > other.deadline;
        }
    };
    mutable std::mutex mutex_;
    mutable std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> entries_;
    mutable std::unordered_map<TimerHandle, bool> active_;
    TimerHandle next_handle_ = 1;
};

} // namespace coroutine

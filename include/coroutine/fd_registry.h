#pragma once

#include "coroutine/event_loop.h"

#include <array>
#include <cstddef>
#include <mutex>
#include <unordered_map>

namespace coroutine {

class FdRegistry {
public:
    EventResult register_wait(int fd, Event event);
    bool complete(int fd, Event event);
    bool cancel(int fd, Event event, CancelReason reason);
    bool close(int fd);
    bool is_closed(int fd) const;

private:
    struct State {
        bool closed = false;
        std::array<bool, 2> waiting{false, false};
    };
    mutable std::mutex mutex_;
    std::unordered_map<int, State> states_;
};

} // namespace coroutine

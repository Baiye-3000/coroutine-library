#include "coroutine/event_loop.h"

#include <cerrno>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <array>
#include <mutex>
#include <unordered_map>

namespace coroutine {
namespace {
int mask(Event event) { return event == Event::read ? EPOLLIN : EPOLLOUT; }
}

struct EventLoop::Impl {
    explicit Impl(std::size_t id) : worker_id(id), epoll_fd(epoll_create1(EPOLL_CLOEXEC)), wake_fd(eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) {
        epoll_event event{};
        event.events = EPOLLIN;
        event.data.fd = wake_fd;
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, wake_fd, &event);
    }
    ~Impl() {
        if (epoll_fd >= 0) close(epoll_fd);
        if (wake_fd >= 0) close(wake_fd);
    }
    std::size_t worker_id;
    int epoll_fd;
    int wake_fd;
    std::mutex mutex;
    std::unordered_map<int, std::array<WaitRegistration, 2>> waits;
    TimerQueue timers;
};

EventLoop::EventLoop(std::size_t worker_id) : impl_(std::make_unique<Impl>(worker_id)) {}
EventLoop::~EventLoop() = default;

EventResult EventLoop::add(int fd, Event event, WaitRegistration registration) {
    if (fd < 0 || impl_->epoll_fd < 0) return EventResult::invalid_fd;
    if (!registration.callback) return EventResult::invalid_fd;
    const auto index = event == Event::read ? 0u : 1u;
    std::lock_guard<std::mutex> lock(impl_->mutex);
    auto& slots = impl_->waits[fd];
    if (slots[index].callback) return EventResult::duplicate;
    slots[index] = std::move(registration);
    epoll_event event_data{};
    event_data.events = EPOLLIN | EPOLLOUT | EPOLLET;
    event_data.data.fd = fd;
    if (epoll_ctl(impl_->epoll_fd, EPOLL_CTL_ADD, fd, &event_data) < 0 && errno != EEXIST) {
        slots[index] = {};
        return EventResult::invalid_fd;
    }
    if (errno == EEXIST) epoll_ctl(impl_->epoll_fd, EPOLL_CTL_MOD, fd, &event_data);
    return EventResult::accepted;
}

bool EventLoop::cancel(int fd, Event event, CancelReason) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    const auto it = impl_->waits.find(fd);
    if (it == impl_->waits.end()) return false;
    auto& slot = it->second[event == Event::read ? 0u : 1u];
    if (!slot.callback) return false;
    slot = {};
    if (!it->second[0].callback && !it->second[1].callback) {
        epoll_ctl(impl_->epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
        impl_->waits.erase(it);
    }
    return true;
}

int EventLoop::run_once(std::chrono::milliseconds timeout) {
    std::vector<TimerCallback> callbacks;
    impl_->timers.expire(std::chrono::steady_clock::now(), callbacks);
    for (auto& callback : callbacks) callback();
    const auto timer_timeout = impl_->timers.next_timeout();
    if (timer_timeout != Duration::max()) timeout = std::min(timeout, timer_timeout);
    std::array<epoll_event, 32> events{};
    const auto count = epoll_wait(impl_->epoll_fd, events.data(), static_cast<int>(events.size()),
                                  static_cast<int>(timeout.count()));
    if (count < 0 && errno == EINTR) return 0;
    for (int index = 0; index < count; ++index) {
        if (events[index].data.fd == impl_->wake_fd) {
            std::uint64_t value;
            while (read(impl_->wake_fd, &value, sizeof(value)) > 0) {}
            continue;
        }
        std::array<WaitRegistration, 2> callbacks_for_fd;
        {
            std::lock_guard<std::mutex> lock(impl_->mutex);
            const auto it = impl_->waits.find(events[index].data.fd);
            if (it == impl_->waits.end()) continue;
            callbacks_for_fd = it->second;
            const auto ready = events[index].events;
            callbacks_for_fd = {};
            if (ready & (EPOLLIN | EPOLLERR | EPOLLHUP)) callbacks_for_fd[0] = it->second[0];
            if (ready & (EPOLLOUT | EPOLLERR | EPOLLHUP)) callbacks_for_fd[1] = it->second[1];
            if (ready & (EPOLLIN | EPOLLERR | EPOLLHUP)) it->second[0] = {};
            if (ready & (EPOLLOUT | EPOLLERR | EPOLLHUP)) it->second[1] = {};
            if (!it->second[0].callback && !it->second[1].callback) {
                impl_->waits.erase(it);
                epoll_ctl(impl_->epoll_fd, EPOLL_CTL_DEL, events[index].data.fd, nullptr);
            } else {
                epoll_event updated{};
                updated.events = EPOLLIN | EPOLLOUT | EPOLLET;
                updated.data.fd = events[index].data.fd;
                epoll_ctl(impl_->epoll_fd, EPOLL_CTL_MOD, events[index].data.fd, &updated);
            }
        }
        if ((events[index].events & (EPOLLIN | EPOLLERR | EPOLLHUP)) && callbacks_for_fd[0].callback) callbacks_for_fd[0].callback();
        if ((events[index].events & (EPOLLOUT | EPOLLERR | EPOLLHUP)) && callbacks_for_fd[1].callback) callbacks_for_fd[1].callback();
    }
    callbacks.clear();
    impl_->timers.expire(std::chrono::steady_clock::now(), callbacks);
    for (auto& callback : callbacks) callback();
    return count < 0 ? -1 : count;
}

void EventLoop::wake() {
    const std::uint64_t value = 1;
    (void)write(impl_->wake_fd, &value, sizeof(value));
}

TimerQueue& EventLoop::timers() noexcept { return impl_->timers; }

} // namespace coroutine

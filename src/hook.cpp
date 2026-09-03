#include "coroutine/hook.h"
#include "coroutine/scheduler.h"

#include <atomic>
#include <cerrno>
#include <ctime>
#include <thread>
#include <sys/syscall.h>
#include <fcntl.h>
#include <unistd.h>

namespace coroutine {
namespace {
thread_local bool enabled = false;
}

void set_hook_enabled(bool value) noexcept { enabled = value; }
bool hook_enabled() noexcept { return enabled; }

int sleep_nanoseconds(long nanoseconds) {
    if (enabled && Coroutine::current() != nullptr && Scheduler::current_event_loop() != nullptr) {
        const auto owner = Coroutine::current_owner();
        if (owner) {
            const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::nanoseconds{nanoseconds} + std::chrono::milliseconds{1});
            Scheduler::current_event_loop()->timers().add(milliseconds, [owner] {
                if (auto* scheduler = Scheduler::current()) scheduler->submit_coroutine(owner, SubmitOptions{Scheduler::current_worker_id()});
            });
            if (Coroutine::wait()) return 0;
        }
    }
    timespec request{nanoseconds / 1000000000L, nanoseconds % 1000000000L};
    return ::nanosleep(&request, nullptr);
}

unsigned sleep_seconds(unsigned seconds) {
    if (seconds > 0 && enabled && Coroutine::current() != nullptr) {
        return sleep_nanoseconds(static_cast<long>(seconds) * 1000000000L) == 0 ? 0 : seconds;
    }
    return ::sleep(seconds);
}

int sleep_microseconds(unsigned useconds) {
    if (enabled && Coroutine::current() != nullptr) return sleep_nanoseconds(static_cast<long>(useconds) * 1000L);
    return ::usleep(useconds);
}

namespace {
bool user_nonblocking(int fd) {
    const auto flags = ::fcntl(fd, F_GETFL, 0);
    return flags >= 0 && (flags & O_NONBLOCK) != 0;
}

template <typename Call>
ssize_t io_wait(int fd, Event event, Call call) {
    const bool originally_nonblocking = user_nonblocking(fd);
    if (!originally_nonblocking) {
        const auto flags = ::fcntl(fd, F_GETFL, 0);
        if (flags >= 0) (void)::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
    for (;;) {
        const auto result = call();
        if (result >= 0 || errno == EINTR) {
            if (result >= 0 || errno != EINTR) return result;
            continue;
        }
        if (!(errno == EAGAIN || errno == EWOULDBLOCK) || originally_nonblocking || !enabled || Coroutine::current() == nullptr) return result;
        auto* loop = Scheduler::current_event_loop();
        auto owner = Coroutine::current_owner();
        if (loop == nullptr || !owner) return result;
        std::atomic<bool> fired{false};
        const auto registration = loop->add(fd, event, WaitRegistration{[owner, &fired] {
            if (!fired.exchange(true)) {
                if (auto* scheduler = Scheduler::current()) scheduler->submit_coroutine(owner, SubmitOptions{Scheduler::current_worker_id()});
            }
        }});
        if (registration != EventResult::accepted) return result;
        if (!Coroutine::wait()) return result;
    }
}
}

ssize_t read_wait(int fd, void* buffer, size_t count) {
    return io_wait(fd, Event::read, [&] { return ::syscall(SYS_read, fd, buffer, count); });
}

ssize_t write_wait(int fd, const void* buffer, size_t count) {
    return io_wait(fd, Event::write, [&] { return ::syscall(SYS_write, fd, buffer, count); });
}

ssize_t recv_wait(int fd, void* buffer, size_t count, int flags) {
    return io_wait(fd, Event::read, [&] { return ::syscall(SYS_recvfrom, fd, buffer, count, flags, nullptr, nullptr); });
}

ssize_t send_wait(int fd, const void* buffer, size_t count, int flags) {
    return io_wait(fd, Event::write, [&] { return ::syscall(SYS_sendto, fd, buffer, count, flags, nullptr, 0); });
}

int connect_wait(int fd, const sockaddr* address, socklen_t length) {
    const auto flags = ::fcntl(fd, F_GETFL, 0);
    const bool originally_nonblocking = flags >= 0 && (flags & O_NONBLOCK) != 0;
    if (flags >= 0 && !originally_nonblocking) (void)::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    for (;;) {
        const auto result = static_cast<int>(::syscall(SYS_connect, fd, address, length));
        if (result == 0 || (errno != EINPROGRESS && errno != EALREADY && errno != EAGAIN) || originally_nonblocking || !enabled || Coroutine::current() == nullptr) return result;
        auto* loop = Scheduler::current_event_loop();
        auto owner = Coroutine::current_owner();
        if (loop == nullptr || !owner) return result;
        if (loop->add(fd, Event::write, WaitRegistration{[owner] {
                if (auto* scheduler = Scheduler::current()) scheduler->submit_coroutine(owner, SubmitOptions{Scheduler::current_worker_id()});
            }}) != EventResult::accepted) return result;
        if (!Coroutine::wait()) return result;
        int error = 0;
        socklen_t size = sizeof(error);
        if (::getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &size) == 0 && error == 0) return 0;
        errno = error == 0 ? ECONNABORTED : error;
        if (errno != EINPROGRESS) return -1;
    }
}

int accept_wait(int fd, sockaddr* address, socklen_t* length) {
    return static_cast<int>(io_wait(fd, Event::read, [&] { return ::syscall(SYS_accept, fd, address, length); }));
}

} // namespace coroutine

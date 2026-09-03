#pragma once

#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>

namespace coroutine {

void set_hook_enabled(bool enabled) noexcept;
bool hook_enabled() noexcept;
unsigned poll_wait(::pollfd* fds, unsigned count, int timeout_ms);
unsigned sleep_seconds(unsigned seconds);
int sleep_microseconds(unsigned useconds);
int sleep_nanoseconds(long nanoseconds);
ssize_t read_wait(int fd, void* buffer, size_t count);
ssize_t write_wait(int fd, const void* buffer, size_t count);
ssize_t recv_wait(int fd, void* buffer, size_t count, int flags);
ssize_t send_wait(int fd, const void* buffer, size_t count, int flags);
int connect_wait(int fd, const sockaddr* address, socklen_t length);
int accept_wait(int fd, sockaddr* address, socklen_t* length);
int connect_wait(int fd, const sockaddr* address, socklen_t length);
int accept_wait(int fd, sockaddr* address, socklen_t* length);

} // namespace coroutine

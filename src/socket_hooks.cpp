#include <dlfcn.h>
#include <sys/syscall.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>
#include "coroutine/hook.h"
#include "coroutine/scheduler.h"

namespace {

template <typename Function>
Function resolve(const char* name) {
    static thread_local bool resolving = false;
    if (resolving) return nullptr;
    resolving = true;
    auto* symbol = dlsym(RTLD_NEXT, name);
    resolving = false;
    return reinterpret_cast<Function>(symbol);
}

}

extern "C" ssize_t read(int fd, void* buffer, size_t count) {
    using Function = ssize_t (*)(int, void*, size_t);
    const auto function = resolve<Function>("read");
    if (coroutine::hook_enabled() && coroutine::Coroutine::current() != nullptr) return coroutine::read_wait(fd, buffer, count);
    return function == nullptr ? ::syscall(SYS_read, fd, buffer, count)
                               : function(fd, buffer, count);
}

extern "C" ssize_t write(int fd, const void* buffer, size_t count) {
    using Function = ssize_t (*)(int, const void*, size_t);
    const auto function = resolve<Function>("write");
    if (coroutine::hook_enabled() && coroutine::Coroutine::current() != nullptr) return coroutine::write_wait(fd, buffer, count);
    return function == nullptr ? ::syscall(SYS_write, fd, buffer, count)
                               : function(fd, buffer, count);
}

extern "C" ssize_t readv(int fd, const iovec* vectors, int count) {
    using Function = ssize_t (*)(int, const iovec*, int);
    const auto function = resolve<Function>("readv");
    if (coroutine::hook_enabled() && coroutine::Coroutine::current() != nullptr) {
        for (;;) {
            const auto result = function == nullptr ? static_cast<ssize_t>(::syscall(SYS_readv, fd, vectors, count)) : function(fd, vectors, count);
            if (result >= 0 || errno != EAGAIN || !coroutine::Coroutine::current()) return result;
            if (!coroutine::Coroutine::wait()) return result;
        }
    }
    return function == nullptr ? -1 : function(fd, vectors, count);
}

extern "C" ssize_t writev(int fd, const iovec* vectors, int count) {
    using Function = ssize_t (*)(int, const iovec*, int);
    const auto function = resolve<Function>("writev");
    return function == nullptr ? -1 : function(fd, vectors, count);
}

extern "C" ssize_t recv(int fd, void* buffer, size_t count, int flags) {
    using Function = ssize_t (*)(int, void*, size_t, int);
    const auto function = resolve<Function>("recv");
    if (coroutine::hook_enabled() && coroutine::Coroutine::current() != nullptr) return coroutine::recv_wait(fd, buffer, count, flags);
    return function == nullptr ? -1 : function(fd, buffer, count, flags);
}

extern "C" ssize_t recvfrom(int fd, void* buffer, size_t count, int flags,
                             sockaddr* address, socklen_t* length) {
    using Function = ssize_t (*)(int, void*, size_t, int, sockaddr*, socklen_t*);
    const auto function = resolve<Function>("recvfrom");
    if (coroutine::hook_enabled() && coroutine::Coroutine::current() != nullptr) return coroutine::recv_wait(fd, buffer, count, flags);
    return function == nullptr ? -1 : function(fd, buffer, count, flags, address, length);
}

extern "C" ssize_t recvmsg(int fd, msghdr* message, int flags) {
    using Function = ssize_t (*)(int, msghdr*, int);
    const auto function = resolve<Function>("recvmsg");
    return function == nullptr ? -1 : function(fd, message, flags);
}

extern "C" ssize_t send(int fd, const void* buffer, size_t count, int flags) {
    using Function = ssize_t (*)(int, const void*, size_t, int);
    const auto function = resolve<Function>("send");
    if (coroutine::hook_enabled() && coroutine::Coroutine::current() != nullptr) return coroutine::send_wait(fd, buffer, count, flags);
    return function == nullptr ? -1 : function(fd, buffer, count, flags);
}

extern "C" ssize_t sendto(int fd, const void* buffer, size_t count, int flags,
                           const sockaddr* address, socklen_t length) {
    using Function = ssize_t (*)(int, const void*, size_t, int, const sockaddr*, socklen_t);
    const auto function = resolve<Function>("sendto");
    if (coroutine::hook_enabled() && coroutine::Coroutine::current() != nullptr) return coroutine::send_wait(fd, buffer, count, flags);
    return function == nullptr ? -1 : function(fd, buffer, count, flags, address, length);
}

extern "C" ssize_t sendmsg(int fd, const msghdr* message, int flags) {
    using Function = ssize_t (*)(int, const msghdr*, int);
    const auto function = resolve<Function>("sendmsg");
    return function == nullptr ? -1 : function(fd, message, flags);
}

extern "C" int socket(int domain, int type, int protocol) {
    using Function = int (*)(int, int, int);
    const auto function = resolve<Function>("socket");
    return function == nullptr ? -1 : function(domain, type, protocol);
}

extern "C" int accept(int fd, sockaddr* address, socklen_t* length) {
    using Function = int (*)(int, sockaddr*, socklen_t*);
    const auto function = resolve<Function>("accept");
    if (coroutine::hook_enabled() && coroutine::Coroutine::current() != nullptr) return coroutine::accept_wait(fd, address, length);
    return function == nullptr ? -1 : function(fd, address, length);
}

extern "C" int connect(int fd, const sockaddr* address, socklen_t length) {
    using Function = int (*)(int, const sockaddr*, socklen_t);
    const auto function = resolve<Function>("connect");
    if (coroutine::hook_enabled() && coroutine::Coroutine::current() != nullptr) return coroutine::connect_wait(fd, address, length);
    return function == nullptr ? -1 : function(fd, address, length);
}

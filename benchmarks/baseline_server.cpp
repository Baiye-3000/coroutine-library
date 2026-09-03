#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

constexpr const char* kResponse =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: 13\r\n"
    "Connection: close\r\n\r\n"
    "Hello, World!";

int make_nonblocking(int fd) {
    const int flags = fcntl(fd, F_GETFL, 0);
    return flags < 0 ? -1 : fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int parse_arg(int argc, char** argv, const char* name, int fallback) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::strcmp(argv[i], name) == 0) {
            return std::atoi(argv[i + 1]);
        }
    }
    return fallback;
}

} // namespace

int main(int argc, char** argv) {
    const int port = parse_arg(argc, argv, "--port", 18080);
    const int max_connections = parse_arg(argc, argv, "--connections", 0);
    const int duration_seconds = parse_arg(argc, argv, "--duration", 10);
    if (port <= 0 || port > 65535 || max_connections < 0 || duration_seconds <= 0) {
        std::fprintf(stderr,
                     "usage: %s [--port PORT] [--connections COUNT] [--duration SECONDS]\n",
                     argv[0]);
        return EXIT_FAILURE;
    }

    const int listener = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (listener < 0) {
        std::perror("socket");
        return EXIT_FAILURE;
    }

    int reuse = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(static_cast<uint16_t>(port));
    if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0 ||
        listen(listener, 1024) < 0) {
        std::perror("bind/listen");
        close(listener);
        return EXIT_FAILURE;
    }

    const int epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd < 0) {
        std::perror("epoll_create1");
        close(listener);
        return EXIT_FAILURE;
    }
    epoll_event registration{};
    registration.events = EPOLLIN;
    registration.data.fd = listener;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listener, &registration) < 0) {
        std::perror("epoll_ctl");
        close(epoll_fd);
        close(listener);
        return EXIT_FAILURE;
    }

    std::printf("baseline_server listening on 127.0.0.1:%d for %d seconds (connections: %d)\n",
                port, duration_seconds, max_connections);
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::seconds(duration_seconds);
    epoll_event events[64]{};
    char buffer[4096]{};
    int accepted_connections = 0;

    while (std::chrono::steady_clock::now() < deadline) {
        const int ready = epoll_wait(epoll_fd, events, 64, 100);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::perror("epoll_wait");
            break;
        }
        for (int i = 0; i < ready; ++i) {
            if (events[i].data.fd == listener) {
                if (max_connections > 0 && accepted_connections >= max_connections) {
                    continue;
                }
                for (;;) {
                    const int client = accept(listener, nullptr, nullptr);
                    if (client < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break;
                        }
                        if (errno != EINTR) {
                            std::perror("accept");
                        }
                        break;
                    }
                    if (make_nonblocking(client) < 0) {
                        close(client);
                        continue;
                    }
                    const int descriptor_flags = fcntl(client, F_GETFD, 0);
                    if (descriptor_flags < 0 ||
                        fcntl(client, F_SETFD, descriptor_flags | FD_CLOEXEC) < 0) {
                        close(client);
                        continue;
                    }
                    ++accepted_connections;
                    epoll_event client_event{};
                    client_event.events = EPOLLIN | EPOLLRDHUP;
                    client_event.data.fd = client;
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client, &client_event) < 0) {
                        close(client);
                    }
                }
                continue;
            }

            const int client = events[i].data.fd;
            const ssize_t received = recv(client, buffer, sizeof(buffer), 0);
            if (received > 0) {
                const char* response = kResponse;
                size_t remaining = std::strlen(kResponse);
                while (remaining > 0) {
                    const ssize_t sent = send(client, response, remaining, MSG_NOSIGNAL);
                    if (sent > 0) {
                        response += sent;
                        remaining -= static_cast<size_t>(sent);
                        continue;
                    }
                    if (sent < 0 && errno == EINTR) {
                        continue;
                    }
                    break;
                }
            }
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client, nullptr);
            close(client);
        }
    }

    close(epoll_fd);
    close(listener);
    return EXIT_SUCCESS;
}

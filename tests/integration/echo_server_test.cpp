#include "../test_framework.h"
#include "coroutine/coroutine.h"
#include "coroutine/hook.h"
#include "coroutine/scheduler.h"

#include <arpa/inet.h>
#include <atomic>
#include <cstring>
#include <memory>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

int main() {
    coroutine::RuntimeConfig config;
    config.worker_count = 2;
    coroutine::Scheduler scheduler(config);
    scheduler.start();
    coroutine::set_hook_enabled(true);

    const int listener = ::socket(AF_INET, SOCK_STREAM, 0);
    EXPECT_TRUE(listener >= 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    EXPECT_TRUE(::bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0);
    EXPECT_TRUE(::listen(listener, 8) == 0);
    socklen_t address_length = sizeof(address);
    EXPECT_TRUE(::getsockname(listener, reinterpret_cast<sockaddr*>(&address), &address_length) == 0);

    std::atomic<bool> server_done{false};
    auto server = std::make_shared<coroutine::Coroutine>([&] {
        const int client = ::accept(listener, nullptr, nullptr);
        EXPECT_TRUE(client >= 0);
        char input[16]{};
        const auto received = ::recv(client, input, sizeof(input), 0);
        EXPECT_TRUE(received == 4);
        EXPECT_TRUE(std::memcmp(input, "ping", 4) == 0);
        EXPECT_TRUE(::send(client, input, static_cast<size_t>(received), 0) == received);
        ::close(client);
        server_done.store(true);
    });
    EXPECT_TRUE(scheduler.submit_coroutine(server, coroutine::SubmitOptions{0}) == coroutine::SubmitResult::accepted);

    std::atomic<bool> client_done{false};
    std::thread client([&] {
        const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        EXPECT_TRUE(fd >= 0);
        EXPECT_TRUE(::connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0);
        EXPECT_TRUE(::send(fd, "ping", 4, 0) == 4);
        char output[16]{};
        EXPECT_TRUE(::recv(fd, output, sizeof(output), 0) == 4);
        EXPECT_TRUE(std::memcmp(output, "ping", 4) == 0);
        ::close(fd);
        client_done.store(true);
    });

    for (int attempt = 0; attempt < 500 && (!server_done.load() || !client_done.load()); ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds{2});
    }
    client.join();
    EXPECT_TRUE(server_done.load());
    EXPECT_TRUE(client_done.load());
    ::close(listener);
    scheduler.stop();
    coroutine::set_hook_enabled(false);
    return test::finish("echo_server_test");
}

#include "../test_framework.h"
#include "coroutine/hook.h"
#include "coroutine/coroutine.h"
#include "coroutine/scheduler.h"

#include <cerrno>
#include <chrono>
#include <sys/socket.h>
#include <unistd.h>
#include <memory>
#include <atomic>

int main() {
    coroutine::set_hook_enabled(false);
    EXPECT_TRUE(!coroutine::hook_enabled());
    const auto started = std::chrono::steady_clock::now();
    EXPECT_TRUE(coroutine::sleep_microseconds(1000) == 0);
    EXPECT_TRUE(std::chrono::steady_clock::now() - started >= std::chrono::microseconds{500});
    const int sockets = ::socket(AF_UNIX, SOCK_STREAM, 0);
    EXPECT_TRUE(sockets >= 0);
    if (sockets >= 0) ::close(sockets);
    coroutine::set_hook_enabled(true);
    EXPECT_TRUE(coroutine::hook_enabled());
    coroutine::RuntimeConfig config;
    config.worker_count = 1;
    coroutine::Scheduler scheduler(config);
    scheduler.start();
    int descriptors[2]{};
    EXPECT_TRUE(::pipe(descriptors) == 0);
    std::atomic<bool> reader_done{false};
    std::atomic<bool> other_ran{false};
    auto reader = std::make_shared<coroutine::Coroutine>([&] {
        char value = 0;
        EXPECT_TRUE(::read(descriptors[0], &value, 1) == 1);
        EXPECT_TRUE(value == 'z');
        reader_done.store(true);
    });
    EXPECT_TRUE(scheduler.submit_coroutine(reader) == coroutine::SubmitResult::accepted);
    EXPECT_TRUE(scheduler.submit([&] { other_ran.store(true); }) == coroutine::SubmitResult::accepted);
    for (int i = 0; i < 100 && !other_ran.load(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds{1});
    EXPECT_TRUE(other_ran.load());
    EXPECT_TRUE(!reader_done.load());
    EXPECT_TRUE(::write(descriptors[1], "z", 1) == 1);
    for (int i = 0; i < 100 && !reader_done.load(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds{1});
    EXPECT_TRUE(reader_done.load());
    std::atomic<bool> sleeper_done{false};
    const auto sleep_started = std::chrono::steady_clock::now();
    auto sleeper = std::make_shared<coroutine::Coroutine>([&] {
        EXPECT_TRUE(coroutine::sleep_microseconds(20000) == 0);
        sleeper_done.store(true);
    });
    std::atomic<bool> during_sleep{false};
    EXPECT_TRUE(scheduler.submit_coroutine(sleeper) == coroutine::SubmitResult::accepted);
    EXPECT_TRUE(scheduler.submit([&] { during_sleep.store(true); }) == coroutine::SubmitResult::accepted);
    for (int i = 0; i < 100 && !sleeper_done.load(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds{1});
    EXPECT_TRUE(during_sleep.load());
    EXPECT_TRUE(sleeper_done.load());
    EXPECT_TRUE(std::chrono::steady_clock::now() - sleep_started >= std::chrono::milliseconds{15});
    ::close(descriptors[0]);
    ::close(descriptors[1]);
    scheduler.stop();
    coroutine::set_hook_enabled(false);
    return test::finish("hook_test");
}

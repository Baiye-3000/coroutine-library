#include "../test_framework.h"
#include "coroutine/event_loop.h"

#include <chrono>
#include <atomic>
#include <unistd.h>
#include <thread>
#include <vector>

int main() {
    coroutine::TimerQueue timers;
    std::vector<int> fired;
    const auto first = timers.add(std::chrono::milliseconds{1}, [&] { fired.push_back(1); });
    const auto cancelled = timers.add(std::chrono::milliseconds{1}, [&] { fired.push_back(2); });
    EXPECT_TRUE(first != 0);
    EXPECT_TRUE(timers.cancel(cancelled));
    EXPECT_TRUE(!timers.cancel(cancelled));
    std::this_thread::sleep_for(std::chrono::milliseconds{3});
    std::vector<coroutine::TimerCallback> callbacks;
    EXPECT_TRUE(timers.expire(std::chrono::steady_clock::now(), callbacks) == 1);
    for (auto& callback : callbacks) callback();
    EXPECT_TRUE(fired.size() == 1 && fired[0] == 1);

    coroutine::EventLoop loop(0);
    EXPECT_TRUE(loop.add(-1, coroutine::Event::read, {}) == coroutine::EventResult::invalid_fd);
    loop.wake();
    EXPECT_TRUE(loop.run_once(std::chrono::milliseconds{10}) >= 0);
    int descriptors[2]{};
    EXPECT_TRUE(::pipe(descriptors) == 0);
    std::atomic<int> ready{0};
    EXPECT_TRUE(loop.add(descriptors[0], coroutine::Event::read, {[&] { ++ready; }}) ==
                coroutine::EventResult::accepted);
    EXPECT_TRUE(loop.add(descriptors[0], coroutine::Event::read, {[&] { ++ready; }}) ==
                coroutine::EventResult::duplicate);
    char value = 'x';
    EXPECT_TRUE(::write(descriptors[1], &value, 1) == 1);
    EXPECT_TRUE(loop.run_once(std::chrono::milliseconds{100}) == 1);
    EXPECT_TRUE(ready.load() == 1);
    ::close(descriptors[0]);
    ::close(descriptors[1]);
    return test::finish("timer_test");
}

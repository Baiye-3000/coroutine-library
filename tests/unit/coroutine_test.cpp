#include "../test_framework.h"
#include "coroutine/coroutine.h"
#include "coroutine/stack_allocator.h"
#include "coroutine/config.h"

#include <stdexcept>

int main() {
    coroutine::StackAllocator stack(16 * 1024);
    EXPECT_TRUE(stack.data() != nullptr);
    EXPECT_TRUE(stack.size() == 16 * 1024);
    {
        {
            coroutine::StackAllocator guarded({32 * 1024, 64 * 1024, true, true});
            EXPECT_TRUE(guarded.data() != nullptr);
            EXPECT_TRUE(guarded.size() >= 32 * 1024);
        }
        coroutine::StackAllocator reused({32 * 1024, 64 * 1024, true, true});
        EXPECT_TRUE(reused.data() != nullptr);
    }
    bool rejected = false;
    try { coroutine::StackAllocator invalid({64 * 1024, 32 * 1024, true, true}); }
    catch (const std::invalid_argument&) { rejected = true; }
    EXPECT_TRUE(rejected);
    coroutine::RuntimeConfig config;
    std::string error;
    config.stack_initial_size = 8 * 1024;
    EXPECT_TRUE(!coroutine::validate_config(config, &error));
    int calls = 0;
    coroutine::Coroutine routine([&] {
        ++calls;
        EXPECT_TRUE(coroutine::Coroutine::current() != nullptr);
        EXPECT_TRUE(coroutine::Coroutine::yield());
        ++calls;
    });
    EXPECT_TRUE(routine.state() == coroutine::CoroutineState::ready);
    EXPECT_TRUE(routine.resume());
    EXPECT_TRUE(calls == 1);
    EXPECT_TRUE(routine.state() == coroutine::CoroutineState::ready);
    EXPECT_TRUE(!routine.resume());
    EXPECT_TRUE(calls == 2);
    EXPECT_TRUE(routine.state() == coroutine::CoroutineState::finished);
    EXPECT_TRUE(!routine.resume());
    coroutine::Coroutine cancelled([] {});
    cancelled.cancel();
    EXPECT_TRUE(cancelled.state() == coroutine::CoroutineState::cancelled);
    EXPECT_TRUE(!cancelled.resume());
    return test::finish("coroutine_test");
}

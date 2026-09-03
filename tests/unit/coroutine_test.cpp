#include "../test_framework.h"
#include "coroutine/coroutine.h"
#include "coroutine/stack_allocator.h"

int main() {
    coroutine::StackAllocator stack(16 * 1024);
    EXPECT_TRUE(stack.data() != nullptr);
    EXPECT_TRUE(stack.size() == 16 * 1024);
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

#include "../test_framework.h"
#include "coroutine/fd_registry.h"

int main() {
    coroutine::FdRegistry registry;
    EXPECT_TRUE(registry.register_wait(3, coroutine::Event::read) == coroutine::EventResult::accepted);
    EXPECT_TRUE(registry.register_wait(3, coroutine::Event::write) == coroutine::EventResult::accepted);
    EXPECT_TRUE(registry.register_wait(3, coroutine::Event::read) == coroutine::EventResult::duplicate);
    EXPECT_TRUE(registry.complete(3, coroutine::Event::read));
    EXPECT_TRUE(registry.close(3));
    EXPECT_TRUE(registry.is_closed(3));
    EXPECT_TRUE(registry.register_wait(3, coroutine::Event::read) == coroutine::EventResult::closed);
    return test::finish("fd_registry_test");
}

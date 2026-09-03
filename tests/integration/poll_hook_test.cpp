#include "../test_framework.h"
#include "coroutine/hook.h"

#include <poll.h>
#include <unistd.h>

int main() {
    int descriptors[2]{};
    EXPECT_TRUE(::pipe(descriptors) == 0);
    char value = 'x';
    EXPECT_TRUE(::write(descriptors[1], &value, 1) == 1);
    pollfd item{descriptors[0], POLLIN, 0};
    EXPECT_TRUE(coroutine::poll_wait(&item, 1, 100) == 1);
    EXPECT_TRUE((item.revents & POLLIN) != 0);
    EXPECT_TRUE(coroutine::poll_wait(nullptr, 0, 0) == 0);
    ::close(descriptors[0]);
    ::close(descriptors[1]);
    return test::finish("poll_hook_test");
}

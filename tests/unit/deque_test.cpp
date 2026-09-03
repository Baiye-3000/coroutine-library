#include "../test_framework.h"
#include "coroutine/work_stealing_deque.h"

int main() {
    coroutine::WorkStealingDeque deque;
    EXPECT_TRUE(!deque.push(nullptr));
    coroutine::RunnableTask first{[] {}, 0};
    coroutine::RunnableTask second{[] {}, 0};
    EXPECT_TRUE(deque.push(&first));
    EXPECT_TRUE(deque.push(&second));
    EXPECT_TRUE(deque.approximate_size() == 2);
    EXPECT_TRUE(deque.pop() == &second);
    EXPECT_TRUE(deque.steal() == &first);
    EXPECT_TRUE(deque.pop() == nullptr);
    return test::finish("deque_test");
}

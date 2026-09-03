#pragma once

#include <cstddef>
#include <functional>
#include <atomic>
#include <memory>

namespace coroutine {

struct RunnableTask {
    std::function<void()> function;
    std::size_t submitted_worker = 0;
    RunnableTask* next = nullptr;
};

class IngressQueue {
public:
    bool push(RunnableTask* task);
    RunnableTask* drain();
    void close() noexcept;
    bool closed() const noexcept;
private:
    std::atomic<RunnableTask*> head_{nullptr};
    std::atomic<bool> closed_{false};
};

class WorkStealingDeque {
public:
    WorkStealingDeque();
    bool push(RunnableTask* task);
    RunnableTask* pop();
    RunnableTask* steal();
    std::size_t approximate_size() const noexcept;

private:
    static constexpr std::size_t capacity_ = 65536;
    std::unique_ptr<std::atomic<RunnableTask*>[]> slots_;
    std::atomic<std::size_t> top_{0};
    std::atomic<std::size_t> bottom_{0};
};

} // namespace coroutine

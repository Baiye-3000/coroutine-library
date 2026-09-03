#include "coroutine/work_stealing_deque.h"

namespace coroutine {

bool IngressQueue::push(RunnableTask* task) {
    if (task == nullptr || closed_.load(std::memory_order_acquire)) return false;
    auto* head = head_.load(std::memory_order_relaxed);
    do {
        task->next = head;
    } while (!head_.compare_exchange_weak(
        head, task, std::memory_order_release, std::memory_order_relaxed));
    if (closed_.load(std::memory_order_acquire)) return false;
    return true;
}

RunnableTask* IngressQueue::drain() {
    auto* list = head_.exchange(nullptr, std::memory_order_acq_rel);
    RunnableTask* reversed = nullptr;
    while (list != nullptr) {
        auto* next = list->next;
        list->next = reversed;
        reversed = list;
        list = next;
    }
    return reversed;
}

void IngressQueue::close() noexcept { closed_.store(true, std::memory_order_release); }

bool IngressQueue::closed() const noexcept { return closed_.load(std::memory_order_acquire); }

WorkStealingDeque::WorkStealingDeque()
    : slots_(std::make_unique<std::atomic<RunnableTask*>[]>(capacity_)) {
    for (std::size_t index = 0; index < capacity_; ++index) {
        slots_[index].store(nullptr, std::memory_order_relaxed);
    }
}

bool WorkStealingDeque::push(RunnableTask* task) {
    if (task == nullptr) return false;
    const auto bottom = bottom_.load(std::memory_order_relaxed);
    const auto top = top_.load(std::memory_order_acquire);
    if (bottom - top >= capacity_) return false;
    slots_[bottom % capacity_].store(task, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_release);
    bottom_.store(bottom + 1, std::memory_order_relaxed);
    return true;
}

RunnableTask* WorkStealingDeque::pop() {
    auto bottom = bottom_.load(std::memory_order_relaxed);
    if (bottom == 0) return nullptr;
    --bottom;
    bottom_.store(bottom, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    auto top = top_.load(std::memory_order_relaxed);
    if (top > bottom) {
        bottom_.store(top, std::memory_order_relaxed);
        return nullptr;
    }
    auto* task = slots_[bottom % capacity_].load(std::memory_order_relaxed);
    if (top == bottom) {
        if (!top_.compare_exchange_strong(
                top, top + 1, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            task = nullptr;
        }
        bottom_.store(top + 1, std::memory_order_relaxed);
    }
    return task;
}

RunnableTask* WorkStealingDeque::steal() {
    auto top = top_.load(std::memory_order_acquire);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    const auto bottom = bottom_.load(std::memory_order_acquire);
    if (top >= bottom) return nullptr;
    auto* task = slots_[top % capacity_].load(std::memory_order_relaxed);
    if (!top_.compare_exchange_strong(
            top, top + 1, std::memory_order_seq_cst, std::memory_order_relaxed)) return nullptr;
    return task;
}

std::size_t WorkStealingDeque::approximate_size() const noexcept {
    const auto bottom = bottom_.load(std::memory_order_relaxed);
    const auto top = top_.load(std::memory_order_relaxed);
    return bottom > top ? bottom - top : 0;
}

} // namespace coroutine

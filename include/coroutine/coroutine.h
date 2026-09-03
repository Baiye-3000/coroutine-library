#pragma once

#include <cstddef>
#include <functional>
#include <memory>

namespace coroutine {

enum class CoroutineState { ready, running, waiting, finished, cancelled };

class Coroutine {
public:
    using Function = std::function<void()>;

    explicit Coroutine(Function function, std::size_t stack_size = 64 * 1024);
    ~Coroutine();
    Coroutine(const Coroutine&) = delete;
    Coroutine& operator=(const Coroutine&) = delete;

    bool resume();
    static bool yield();
    static bool wait();
    static Coroutine* current() noexcept;
    static void set_current_owner(std::shared_ptr<Coroutine> owner);
    static std::shared_ptr<Coroutine> current_owner();
    void cancel() noexcept;
    bool waiting() const noexcept;
    CoroutineState state() const noexcept;
    std::size_t last_worker() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace coroutine

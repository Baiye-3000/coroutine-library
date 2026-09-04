#include "coroutine/coroutine.h"
#include "coroutine/stack_allocator.h"

#include <cstdlib>
#include <cstdint>
#include <ucontext.h>
#include <utility>

namespace coroutine {

namespace {
thread_local Coroutine* current_coroutine = nullptr;
thread_local std::shared_ptr<Coroutine> owner_coroutine;
}

struct Coroutine::Impl {
    explicit Impl(Function fn, StackAllocator::Options options) : function(std::move(fn)), stack(options) {
        getcontext(&context);
        context.uc_stack.ss_sp = stack.data();
        context.uc_stack.ss_size = stack.size();
        context.uc_link = &caller;
        makecontext(&context, reinterpret_cast<void (*)()>(&Impl::entry), 1,
                    reinterpret_cast<std::uintptr_t>(this));
    }
    static void entry(std::uintptr_t raw) {
        auto* self = reinterpret_cast<Impl*>(raw);
        current_coroutine = reinterpret_cast<Coroutine*>(self->owner);
        self->state = CoroutineState::running;
        try {
            self->function();
            self->state = CoroutineState::finished;
        } catch (...) {
            self->state = CoroutineState::finished;
        }
        current_coroutine = nullptr;
    }
    ucontext_t context{};
    ucontext_t caller{};
    Function function;
    StackAllocator stack;
    CoroutineState state = CoroutineState::ready;
    bool cancelled = false;
    void* owner = nullptr;
};

Coroutine::Coroutine(Function function, std::size_t stack_size)
    : impl_(std::make_unique<Impl>(std::move(function), StackAllocator::Options{stack_size, stack_size, false, false})) {
    impl_->owner = this;
}
Coroutine::Coroutine(Function function, StackAllocator::Options stack_options)
    : impl_(std::make_unique<Impl>(std::move(function), stack_options)) {
    impl_->owner = this;
}
Coroutine::~Coroutine() = default;

bool Coroutine::resume() {
    if (impl_->cancelled || impl_->state == CoroutineState::finished) return false;
    impl_->state = CoroutineState::running;
    current_coroutine = this;
    if (swapcontext(&impl_->caller, &impl_->context) != 0) return false;
    current_coroutine = nullptr;
    return impl_->state != CoroutineState::finished;
}

bool Coroutine::yield() {
    if (current_coroutine == nullptr) return false;
    auto* self = current_coroutine;
    self->impl_->state = CoroutineState::ready;
    if (swapcontext(&self->impl_->context, &self->impl_->caller) != 0) return false;
    if (!self->impl_->cancelled) self->impl_->state = CoroutineState::running;
    return true;
}

bool Coroutine::wait() {
    if (current_coroutine == nullptr) return false;
    current_coroutine->impl_->state = CoroutineState::waiting;
    auto* self = current_coroutine;
    const auto result = swapcontext(&self->impl_->context, &self->impl_->caller) == 0;
    current_coroutine = self;
    if (result && !self->impl_->cancelled) self->impl_->state = CoroutineState::running;
    return result;
}

Coroutine* Coroutine::current() noexcept { return current_coroutine; }
void Coroutine::set_current_owner(std::shared_ptr<Coroutine> owner) { owner_coroutine = std::move(owner); }
std::shared_ptr<Coroutine> Coroutine::current_owner() { return owner_coroutine; }

void Coroutine::cancel() noexcept {
    impl_->cancelled = true;
    if (impl_->state != CoroutineState::finished) impl_->state = CoroutineState::cancelled;
}

CoroutineState Coroutine::state() const noexcept { return impl_->state; }
bool Coroutine::waiting() const noexcept { return impl_->state == CoroutineState::waiting; }
std::size_t Coroutine::last_worker() const noexcept { return static_cast<std::size_t>(-1); }

} // namespace coroutine

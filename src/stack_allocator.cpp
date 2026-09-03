#include "coroutine/stack_allocator.h"

#include <stdexcept>

namespace coroutine {

StackAllocator::StackAllocator(std::size_t size) : storage_(nullptr), size_(size) {
    if (size_ < 16 * 1024) {
        throw std::invalid_argument("coroutine stack is too small");
    }
    storage_ = std::make_unique<std::byte[]>(size_);
}

std::byte* StackAllocator::data() noexcept { return storage_.get(); }
const std::byte* StackAllocator::data() const noexcept { return storage_.get(); }
std::size_t StackAllocator::size() const noexcept { return size_; }

} // namespace coroutine

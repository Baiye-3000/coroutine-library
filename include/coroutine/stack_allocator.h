#pragma once

#include <cstddef>
#include <memory>

namespace coroutine {

class StackAllocator {
public:
    explicit StackAllocator(std::size_t size);
    std::byte* data() noexcept;
    const std::byte* data() const noexcept;
    std::size_t size() const noexcept;

private:
    std::unique_ptr<std::byte[]> storage_;
    std::size_t size_;
};

} // namespace coroutine

#pragma once

#include <cstddef>
#include <memory>

namespace coroutine {

class StackAllocator {
public:
    struct Options {
        std::size_t size = 64 * 1024;
        std::size_t max_size = 1024 * 1024;
        bool guard_page = false;
        bool pool = false;
    };

    explicit StackAllocator(std::size_t size);
    explicit StackAllocator(Options options);
    ~StackAllocator();
    StackAllocator(const StackAllocator&) = delete;
    StackAllocator& operator=(const StackAllocator&) = delete;
    StackAllocator(StackAllocator&& other) noexcept;
    StackAllocator& operator=(StackAllocator&& other) noexcept;
    std::byte* data() noexcept;
    const std::byte* data() const noexcept;
    std::size_t size() const noexcept;

private:
    std::unique_ptr<std::byte[]> storage_;
    void* mapping_ = nullptr;
    std::size_t mapping_size_ = 0;
    std::size_t size_;
    bool pooled_ = false;
};

} // namespace coroutine

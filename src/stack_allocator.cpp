#include "coroutine/stack_allocator.h"

#include <algorithm>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <utility>
#ifdef __linux__
#include <sys/mman.h>
#include <unistd.h>
#include <vector>
#endif

namespace coroutine {
namespace {
constexpr std::size_t minimum_stack = 16 * 1024;
#ifdef __linux__
struct PooledBlock { void* mapping; std::size_t mapping_size; std::size_t usable_size; };
std::mutex pool_mutex;
std::vector<PooledBlock> pool;
std::size_t page_size() {
    const long value = sysconf(_SC_PAGESIZE);
    return value > 0 ? static_cast<std::size_t>(value) : 4096;
}
#endif
}

StackAllocator::StackAllocator(std::size_t size) : StackAllocator(Options{size, size, false, false}) {}

StackAllocator::StackAllocator(Options options) : size_(options.size) {
    if (options.size < minimum_stack) throw std::invalid_argument("coroutine stack is too small");
    if (options.max_size < options.size) throw std::invalid_argument("coroutine stack exceeds configured maximum");
#ifdef __linux__
    if (options.guard_page) {
        const auto page = page_size();
        size_ = (options.size + page - 1) / page * page;
        const auto total = size_ + page;
        if (options.pool) {
            std::lock_guard<std::mutex> lock(pool_mutex);
            auto it = std::find_if(pool.begin(), pool.end(), [this](const PooledBlock& block) {
                return block.usable_size == size_;
            });
            if (it != pool.end()) {
                mapping_ = it->mapping;
                mapping_size_ = it->mapping_size;
                pool.erase(it);
            }
        }
        if (mapping_ == nullptr) {
            mapping_ = mmap(nullptr, total, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (mapping_ == MAP_FAILED) {
                mapping_ = nullptr;
                throw std::bad_alloc();
            }
            mapping_size_ = total;
            if (mprotect(mapping_, page, PROT_NONE) != 0) {
                munmap(mapping_, total);
                mapping_ = nullptr;
                throw std::bad_alloc();
            }
        }
        pooled_ = options.pool;
        return;
    }
#else
    (void)options;
#endif
    storage_ = std::make_unique<std::byte[]>(size_);
}

StackAllocator::~StackAllocator() {
#ifdef __linux__
    if (mapping_ != nullptr) {
        if (pooled_) {
            std::lock_guard<std::mutex> lock(pool_mutex);
            if (pool.size() < 64) {
                std::memset(static_cast<std::byte*>(mapping_) + page_size(), 0, size_);
                pool.push_back({mapping_, mapping_size_, size_});
                mapping_ = nullptr;
            }
        }
        if (mapping_ != nullptr) munmap(mapping_, mapping_size_);
    }
#endif
}

StackAllocator::StackAllocator(StackAllocator&& other) noexcept
    : storage_(std::move(other.storage_)), mapping_(other.mapping_), mapping_size_(other.mapping_size_),
      size_(other.size_), pooled_(other.pooled_) {
    other.mapping_ = nullptr;
    other.mapping_size_ = 0;
    other.pooled_ = false;
}

StackAllocator& StackAllocator::operator=(StackAllocator&& other) noexcept {
    if (this != &other) {
        std::swap(storage_, other.storage_);
        std::swap(mapping_, other.mapping_);
        std::swap(mapping_size_, other.mapping_size_);
        std::swap(size_, other.size_);
        std::swap(pooled_, other.pooled_);
    }
    return *this;
}

std::byte* StackAllocator::data() noexcept {
#ifdef __linux__
    if (mapping_ != nullptr) return static_cast<std::byte*>(mapping_) + page_size();
#endif
    return storage_.get();
}
const std::byte* StackAllocator::data() const noexcept {
    return const_cast<StackAllocator*>(this)->data();
}
std::size_t StackAllocator::size() const noexcept { return size_; }
} // namespace coroutine

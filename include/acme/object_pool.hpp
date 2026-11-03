#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace acme {

// ============================================================================
// LESSON 4: Zero Heap Allocations on the Hot Path (Pre-Allocated Object Pool)
// ============================================================================
// In languages like Java/Python/Go, creating an object allocates on the heap
// and relies on a Garbage Collector. Even in standard C++, calling `new` or
// `malloc` on the trading hot path is forbidden because:
//   1. The OS allocator may acquire a global lock or trigger a page fault.
//   2. Heap-allocated nodes are scattered in RAM, causing L1/L2/L3 cache misses.
//
// Solution: Pre-allocate a contiguous arena at startup, maintain an O(1)
// free-list of indices, and use C++20 `std::construct_at` / `std::destroy_at`
// for explicit object lifetime management.
template <typename T>
class ObjectPool {
public:
    explicit ObjectPool(std::size_t capacity)
        : storage_(capacity), free_indices_(capacity) {
        // Populate the free-list stack in reverse so index 0 is popped first
        // (maximizing spatial locality in CPU cache).
        for (std::size_t i = 0; i < capacity; ++i) {
            free_indices_[i] = capacity - 1U - i;
        }
    }

    // Rule of 5: Disable copying of a large memory arena; allow moving.
    ObjectPool(const ObjectPool&)            = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;
    ObjectPool(ObjectPool&&) noexcept        = default;
    ObjectPool& operator=(ObjectPool&&) noexcept = default;
    ~ObjectPool()                            = default;

    template <typename... Args>
    [[nodiscard]] T* allocate(Args&&... args) noexcept {
        if (free_indices_.empty()) [[unlikely]] {
            return nullptr; // Pool exhausted
        }
        const std::size_t idx = free_indices_.back();
        free_indices_.pop_back();

        T* slot = &storage_[idx];
        std::construct_at(slot, std::forward<Args>(args)...);
        return slot;
    }

    void deallocate(T* ptr) noexcept {
        assert(ptr != nullptr);
        assert(ptr >= storage_.data() && ptr < storage_.data() + storage_.size());
        std::destroy_at(ptr);
        const auto idx = static_cast<std::size_t>(ptr - storage_.data());
        free_indices_.push_back(idx);
    }

    [[nodiscard]] std::size_t available() const noexcept {
        return free_indices_.size();
    }

    [[nodiscard]] std::size_t capacity() const noexcept {
        return storage_.size();
    }

private:
    std::vector<T>           storage_;
    std::vector<std::size_t> free_indices_;
};

} // namespace acme

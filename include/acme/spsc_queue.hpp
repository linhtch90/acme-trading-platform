#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <type_traits>

namespace acme {

// ============================================================================
// LESSON 5: Lock-Free SPSC Ring Buffer & False Sharing Prevention
// ============================================================================
// In a real trading system, one thread reads network packets from the NIC
// (Producer) and pushes market events to the Strategy/OrderBook thread (Consumer).
//
// Best practices applied here:
//   1. `static_assert` that `std::atomic<std::size_t>::is_always_lock_free` is true.
//   2. Power-of-2 capacity so modulo is a single-cycle bitwise AND (`& MASK`).
//   3. Separate 64-byte cache lines (`alignas(64)`) for shared atomic indices
//      and thread-local cached indices so producer writes never invalidate
//      consumer L1 cache lines (preventing False Sharing).
template <typename T, std::size_t Capacity>
class SpscQueue {
    static_assert(Capacity >= 2 && (Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a power of 2 for fast bitwise modulo");
    static_assert(std::is_trivially_copyable_v<T>,
                  "T must be trivially copyable for lock-free ring buffer safety");
    static_assert(std::atomic<std::size_t>::is_always_lock_free,
                  "std::atomic<std::size_t> must be lock-free on target hardware");

public:
    SpscQueue() = default;

    // Atomic queues cannot be copied or moved safely while in use
    SpscQueue(const SpscQueue&)            = delete;
    SpscQueue& operator=(const SpscQueue&) = delete;
    SpscQueue(SpscQueue&&)                 = delete;
    SpscQueue& operator=(SpscQueue&&)      = delete;
    ~SpscQueue()                           = default;

    // Called ONLY by the Producer thread
    [[nodiscard]] bool try_push(const T& item) noexcept {
        const std::size_t current_write = write_idx_.load(std::memory_order_relaxed);
        const std::size_t next_write    = (current_write + 1U) & MASK;

        // Check if queue is full using producer-local cached read index first
        // to avoid cross-core cache coherence traffic on every push.
        if (next_write == cached_read_idx_) {
            cached_read_idx_ = read_idx_.load(std::memory_order_acquire);
            if (next_write == cached_read_idx_) [[unlikely]] {
                return false; // Queue is full
            }
        }

        buffer_[current_write] = item;
        // Release semantics guarantee `buffer_[current_write] = item` is visible
        // in memory BEFORE `write_idx_` is observed by the consumer.
        write_idx_.store(next_write, std::memory_order_release);
        return true;
    }

    // Called ONLY by the Consumer thread
    [[nodiscard]] bool try_pop(T& out_item) noexcept {
        const std::size_t current_read = read_idx_.load(std::memory_order_relaxed);

        // Check if queue is empty using consumer-local cached write index first
        if (current_read == cached_write_idx_) {
            cached_write_idx_ = write_idx_.load(std::memory_order_acquire);
            if (current_read == cached_write_idx_) {
                return false; // Queue is empty
            }
        }

        out_item = buffer_[current_read];
        const std::size_t next_read = (current_read + 1U) & MASK;
        read_idx_.store(next_read, std::memory_order_release);
        return true;
    }

private:
    static constexpr std::size_t MASK            = Capacity - 1U;
    static constexpr std::size_t CACHE_LINE_SIZE = 64U;

    // Shared atomic written by Producer, read by Consumer (when consumer cache misses)
    alignas(CACHE_LINE_SIZE) std::atomic<std::size_t> write_idx_{0};

    // Producer-private cache of read_idx_ (kept on its own cache line so updating
    // it never invalidates the consumer's read of write_idx_)
    alignas(CACHE_LINE_SIZE) std::size_t cached_read_idx_{0};

    // Shared atomic written by Consumer, read by Producer (when producer cache misses)
    alignas(CACHE_LINE_SIZE) std::atomic<std::size_t> read_idx_{0};

    // Consumer-private cache of write_idx_
    alignas(CACHE_LINE_SIZE) std::size_t cached_write_idx_{0};

    // Contiguous ring buffer storage on its own cache line boundary
    alignas(CACHE_LINE_SIZE) std::array<T, Capacity> buffer_{};
};

} // namespace acme

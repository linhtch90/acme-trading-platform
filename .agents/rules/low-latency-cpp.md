# Low-Latency C++20 Engineering Rules

When writing or modifying C++ files in `include/` or `src/`:

1. **No Dynamic Allocation on the Hot Path:**
   - Pre-allocate all memory in constructors (`ObjectPool`, `FlatHashMap`, `std::vector::reserve`).
   - Verify that `test_zero_heap_allocations_on_hot_path` in `tests/test_order_book.cpp` passes with `0` heap allocations.
2. **Mark Hot Functions `noexcept` and `[[nodiscard]]`:**
   - All hot-path methods (`add_order`, `cancel_order`, `reduce_order`, `try_push`, `try_pop`) must be `noexcept`.
   - Use `[[unlikely]]` on error/edge-case branches so the compiler optimizes instruction layout for the common fast path.
3. **Cache-Line Awareness (`64` bytes):**
   - Separate atomic variables written by different threads with `alignas(64)` to prevent False Sharing.
   - Keep hot structs (`Trade`, `OrderNode`, `PriceLevel`) compact (`<= 64` bytes).

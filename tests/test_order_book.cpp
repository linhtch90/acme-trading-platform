#include "acme/order_book.hpp"
#include "acme/spsc_queue.hpp"
#include "acme/types.hpp"

#include <atomic>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <new>
#include <thread>
#include <vector>

using namespace acme;

// ============================================================================
// HFT TEST BEST PRACTICE: Intercept Global `operator new` to Prove Zero Allocations
// ============================================================================
namespace {
std::atomic<std::size_t> g_heap_allocations{0};
} // namespace

void* operator new(std::size_t size) {
    g_heap_allocations.fetch_add(1, std::memory_order_relaxed);
    void* ptr = std::malloc(size);
    if (ptr == nullptr) {
        throw std::bad_alloc{};
    }
    return ptr;
}

void operator delete(void* ptr) noexcept {
    std::free(ptr);
}

void operator delete(void* ptr, std::size_t) noexcept {
    std::free(ptr);
}

namespace {

void test_zero_heap_allocations_on_hot_path() {
    OrderBook book(4096, 256);

    // Reset allocation counter AFTER startup initialization
    g_heap_allocations.store(0, std::memory_order_seq_cst);

    std::size_t trade_count = 0;
    auto on_trade = [&trade_count](const Trade&) noexcept {
        ++trade_count;
    };

    // Perform adds, partial fills, full matches, reductions, and cancellations
    book.add_order(1, Side::Buy,  to_ticks(99.90), 100, OrderType::Limit, on_trade);
    book.add_order(2, Side::Buy,  to_ticks(100.00), 200, OrderType::Limit, on_trade);
    book.add_order(3, Side::Sell, to_ticks(100.10), 150, OrderType::Limit, on_trade);
    book.add_order(4, Side::Sell, to_ticks(100.05), 100, OrderType::Limit, on_trade);

    assert(book.reduce_order(2, 120) == true);
    book.add_order(5, Side::Buy,  to_ticks(100.10), 200, OrderType::Limit, on_trade);
    assert(book.cancel_order(1) == true);

    const std::size_t hot_path_allocs = g_heap_allocations.load(std::memory_order_seq_cst);
    assert(hot_path_allocs == 0 && "Hot path must perform ZERO heap allocations!");
    assert(trade_count == 2);
    std::cout << "[PASS] test_zero_heap_allocations_on_hot_path (0 bytes allocated on heap)\n";
}

void test_price_time_priority_and_price_improvement() {
    OrderBook book(1024, 128);

    // Three sellers at $100.00 (FIFO order: 1, 2, 3)
    book.add_order(1, Side::Sell, to_ticks(100.00), 50);
    book.add_order(2, Side::Sell, to_ticks(100.00), 50);
    book.add_order(3, Side::Sell, to_ticks(100.00), 50);

    // Buyer willing to pay up to $101.00 for 75 shares
    // Must execute at the resting maker price ($100.00), filling #1 (50) and part of #2 (25)
    std::vector<Trade> trades;
    Quantity remaining = book.add_order(10, Side::Buy, to_ticks(101.00), 75, OrderType::Limit, &trades);

    assert(remaining == 0);
    assert(trades.size() == 2);

    assert(trades[0].maker_order_id == 1);
    assert(trades[0].quantity == 50);
    assert(trades[0].price == to_ticks(100.00));

    assert(trades[1].maker_order_id == 2);
    assert(trades[1].quantity == 25);
    assert(trades[1].price == to_ticks(100.00));

    BookTop top = book.top_of_book();
    assert(top.best_ask.has_value() && *top.best_ask == to_ticks(100.00));
    assert(top.ask_quantity == 75); // 25 left on #2 + 50 on #3
    std::cout << "[PASS] test_price_time_priority_and_price_improvement\n";
}

void test_intrusive_list_cancel_and_validation() {
    OrderBook book(1024, 128);

    book.add_order(1, Side::Buy, to_ticks(50.00), 10);
    book.add_order(2, Side::Buy, to_ticks(50.00), 20);
    book.add_order(3, Side::Buy, to_ticks(50.00), 30);

    // Duplicate OrderId #2 must be rejected without corrupting state
    assert(book.add_order(2, Side::Buy, to_ticks(50.00), 999) == 0);
    assert(book.top_of_book().bid_quantity == 60);

    // Invalid price (<= 0) and zero quantity must be rejected
    assert(book.add_order(99, Side::Buy, 0, 10) == 0);
    assert(book.add_order(99, Side::Buy, to_ticks(50.00), 0) == 0);

    // Cancel middle node (#2)
    assert(book.cancel_order(2) == true);
    assert(book.top_of_book().bid_quantity == 40);

    // Cancel head node (#1)
    assert(book.cancel_order(1) == true);
    assert(book.top_of_book().bid_quantity == 30);

    // Cancel tail/last node (#3) -> should remove the price level completely
    assert(book.cancel_order(3) == true);
    assert(!book.top_of_book().best_bid.has_value());
    assert(book.resting_order_count() == 0);
    std::cout << "[PASS] test_intrusive_list_cancel_and_validation\n";
}

void test_ioc_and_market_orders() {
    OrderBook book(1024, 128);
    book.add_order(1, Side::Sell, to_ticks(100.00), 40);
    book.add_order(2, Side::Sell, to_ticks(100.05), 60);

    std::vector<Trade> trades;
    // IOC Buy for 100 shares @ $100.00 -> should fill 40 at $100.00 and cancel remaining 60
    Quantity resting = book.add_order(3, Side::Buy, to_ticks(100.00), 100, OrderType::IOC, &trades);

    assert(resting == 0);
    assert(trades.size() == 1);
    assert(trades[0].quantity == 40);
    assert(book.resting_order_count() == 1);

    // Market Buy for 100 shares -> fills remaining 60 @ $100.05 and never rests
    trades.clear();
    resting = book.add_order(4, Side::Buy, 0, 100, OrderType::Market, &trades);
    assert(resting == 0);
    assert(trades.size() == 1);
    assert(trades[0].quantity == 60);
    assert(book.resting_order_count() == 0);
    std::cout << "[PASS] test_ioc_and_market_orders\n";
}

void test_spsc_queue_multithreaded() {
    SpscQueue<std::uint64_t, 4096> queue;
    constexpr std::uint64_t NUM_ITEMS = 200'000;

    std::thread producer([&queue]() {
        for (std::uint64_t i = 1; i <= NUM_ITEMS; ++i) {
            while (!queue.try_push(i)) {}
        }
    });

    std::uint64_t sum = 0;
    std::thread consumer([&queue, &sum]() {
        for (std::uint64_t expected = 1; expected <= NUM_ITEMS; ++expected) {
            std::uint64_t val = 0;
            while (!queue.try_pop(val)) {}
            assert(val == expected);
            sum += val;
        }
    });

    producer.join();
    consumer.join();
    assert(sum == (NUM_ITEMS * (NUM_ITEMS + 1U)) / 2U);
    std::cout << "[PASS] test_spsc_queue_multithreaded (200,000 messages verified)\n";
}

} // namespace

int main() {
    std::cout << "Running ACME Trading Platform Unit Tests...\n";
    test_zero_heap_allocations_on_hot_path();
    test_price_time_priority_and_price_improvement();
    test_intrusive_list_cancel_and_validation();
    test_ioc_and_market_orders();
    test_spsc_queue_multithreaded();
    std::cout << "All unit tests passed!\n";
    return 0;
}

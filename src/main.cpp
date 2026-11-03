#include "acme/order_book.hpp"
#include "acme/spsc_queue.hpp"
#include "acme/types.hpp"

#include <iomanip>
#include <iostream>
#include <thread>

using namespace acme;

namespace {

enum class CommandAction : std::uint8_t {
    Add    = 0,
    Cancel = 1
};

struct OrderEvent {
    CommandAction action{CommandAction::Add};
    OrderId       id{};
    Side          side{Side::Buy};
    Price         price{};
    Quantity      quantity{};
    OrderType     type{OrderType::Limit};
};

void print_trade(const Trade& t) {
    std::cout << "  >>> [TRADE EXECUTED] "
              << "Aggressor(" << to_string(t.taker_side) << " #" << t.taker_order_id << ") "
              << "matched Resting(#" << t.maker_order_id << ") : "
              << t.quantity << " shares @ $"
              << std::fixed << std::setprecision(4) << to_human_price(t.price) << "\n";
}

} // namespace

int main() {
    std::cout << "========================================================\n";
    std::cout << "  ACME Low-Latency C++20 Matching Engine & SPSC Demo\n";
    std::cout << "========================================================\n\n";

    OrderBook book(10'000);
    SpscQueue<OrderEvent, 1024> event_queue;

    // 1. Producer thread: simulates a network gateway pushing orders into a lock-free ring buffer
    std::thread gateway_thread([&event_queue]() {
        const OrderEvent scenario[] = {
            // Build the Bid side (Buyers)
            {CommandAction::Add, 101, Side::Buy,  to_ticks(99.95), 200, OrderType::Limit},
            {CommandAction::Add, 102, Side::Buy,  to_ticks(100.00), 150, OrderType::Limit},
            {CommandAction::Add, 103, Side::Buy,  to_ticks(100.00), 300, OrderType::Limit}, // Same price as #102, behind in FIFO queue

            // Build the Ask side (Sellers)
            {CommandAction::Add, 201, Side::Sell, to_ticks(100.05), 100, OrderType::Limit},
            {CommandAction::Add, 202, Side::Sell, to_ticks(100.05), 250, OrderType::Limit}, // Behind #201 at $100.05
            {CommandAction::Add, 203, Side::Sell, to_ticks(100.10), 400, OrderType::Limit},
        };

        for (const auto& ev : scenario) {
            while (!event_queue.try_push(ev)) {
                // Spin-wait if full (in HFT, threads busy-poll rather than sleeping)
            }
        }
    });

    gateway_thread.join();

    // 2. Consumer: Matching Engine drains the lock-free SPSC queue
    std::cout << "[Step 1] Draining initial limit orders from lock-free SPSC queue...\n";
    OrderEvent ev{};
    while (event_queue.try_pop(ev)) {
        if (ev.action == CommandAction::Add) {
            book.add_order(ev.id, ev.side, ev.price, ev.quantity, ev.type, print_trade);
        }
    }

    book.print_depth();

    // 3. Demonstrate aggressive order sweeping multiple orders in FIFO Price-Time Priority
    std::cout << "[Step 2] Incoming Aggressive BUY #301 for 450 shares @ Limit $100.10\n";
    std::cout << "         Expectation:\n";
    std::cout << "           1) Fills 100 from Ask #201 @ $100.05 (Best price, oldest order)\n";
    std::cout << "           2) Fills 250 from Ask #202 @ $100.05 (Best price, second in FIFO)\n";
    std::cout << "           3) Fills 100 from Ask #203 @ $100.10 (Next price level)\n\n";

    book.add_order(301, Side::Buy, to_ticks(100.10), 450, OrderType::Limit, print_trade);
    book.print_depth();

    // 4. Demonstrate O(1) Order Cancellation via Intrusive Doubly-Linked List
    std::cout << "[Step 3] Market Maker cancels Bid #102 (150 shares @ $100.00) in O(1) time...\n";
    book.cancel_order(102);
    book.print_depth();

    return 0;
}

// 03_orderbook_v2_vector.cpp
// ============================================================
// VERSION 2 demo -- sorted std::vector<PriceLevel> per side. Correctness
// check (05-sorted-vector-implementation.md).
// ============================================================
//   g++ -std=c++20 -Wall -Wextra 03_orderbook_v2_vector.cpp -o v2 && ./v2
// ============================================================

#include "order_workload.hpp"
#include "orderbook_v2_vector.hpp"

#include <iostream>

int main() {
    constexpr std::size_t N = 20000;
    const auto ops = generate_workload(N, /*seed=*/4242);   // SAME seed as 01 -- same workload

    BookV2 book;
    std::size_t applied = 0, failed = 0;
    for (const auto& op : ops) {
        bool ok = false;
        switch (op.kind) {
            case OpKind::Add:     ok = book.add(op.order_id, op.is_buy, op.price_ticks, op.qty); break;
            case OpKind::Execute: ok = book.reduce(op.order_id, op.qty); break;
            case OpKind::Cancel:  ok = book.reduce(op.order_id, op.qty); break;
            case OpKind::Delete:  ok = book.remove(op.order_id); break;
            case OpKind::Replace: ok = book.replace(op.order_id, op.new_order_id, op.is_buy,
                                                      op.price_ticks, op.qty); break;
        }
        if (ok) ++applied; else ++failed;
    }

    std::cout << "=== BookV2 (sorted vector) -- correctness check ===\n";
    std::cout << "ops applied: " << applied << " / " << ops.size() << "  (" << failed << " failed)\n";
    std::cout << "live orders in book: " << book.order_count() << "\n";

    if (book.has_bid() && book.has_ask()) {
        std::cout << "\nbest bid = " << book.best_bid() << " x" << book.best_bid_qty() << '\n';
        std::cout << "best ask = " << book.best_ask() << " x" << book.best_ask_qty() << '\n';
        std::cout << "best_bid < best_ask? "
                  << (book.best_bid() < book.best_ask() ? "haan (sahi)" : "NAHI (BUG!)") << '\n';
    }

    std::cout << "\n(01_orderbook_v1_map.cpp SAME seed/workload use karta -- output (order_count,\n"
                 " best bid/ask) V1 se IDENTICAL hona chahiye -- 08_orderbook_tests.cpp isi ko\n"
                 " automatically verify karta.)\n";

    return failed == 0 ? 0 : 1;
}

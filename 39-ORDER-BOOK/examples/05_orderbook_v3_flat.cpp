// 05_orderbook_v3_flat.cpp
// ============================================================
// VERSION 3 demo -- flat tick-indexed array + intrusive lists + flat
// hash index. Correctness check (07/08/09/10/11/12).
// ============================================================
//   g++ -std=c++20 -Wall -Wextra 05_orderbook_v3_flat.cpp -o v3 && ./v3
// ============================================================

#include "order_workload.hpp"
#include "orderbook_v3_flat.hpp"

#include <iostream>

int main() {
    constexpr std::size_t N = 20000;
    const auto ops = generate_workload(N, /*seed=*/4242);   // SAME seed as 01/03

    BookV3 book(/*center=*/10000, /*max_orders=*/32768);
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

    std::cout << "=== BookV3 (flat array + intrusive list + flat hash) -- correctness check ===\n";
    std::cout << "ops applied: " << applied << " / " << ops.size() << "  (" << failed << " failed)\n";
    std::cout << "live orders in book: " << book.order_count() << "\n";

    if (book.has_bid() && book.has_ask()) {
        std::cout << "\nbest bid = " << book.best_bid() << " x" << book.best_bid_qty() << '\n';
        std::cout << "best ask = " << book.best_ask() << " x" << book.best_ask_qty() << '\n';
        std::cout << "best_bid < best_ask? "
                  << (book.best_bid() < book.best_ask() ? "haan (sahi)" : "NAHI (BUG!)") << '\n';
    }

    std::cout << "\n(01/03 SAME seed/workload -- output V1/V2 se IDENTICAL hona chahiye)\n";

    return failed == 0 ? 0 : 1;
}

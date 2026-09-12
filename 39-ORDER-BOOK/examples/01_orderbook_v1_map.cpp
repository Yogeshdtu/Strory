// 01_orderbook_v1_map.cpp
// ============================================================
// VERSION 1 demo -- std::map-based order book, correctness check.
// SIMPLE, CORRECT. Isse optimize karna hai (03, 05), par pehle
// baseline zaroori hai (spec: "pehle correct, phir fast").
// ============================================================
//   g++ -std=c++20 -Wall -Wextra 01_orderbook_v1_map.cpp -o v1 && ./v1
// ============================================================

#include "order_workload.hpp"
#include "orderbook_v1_map.hpp"

#include <iostream>

int main() {
    constexpr std::size_t N = 20000;
    const auto ops = generate_workload(N, /*seed=*/4242);

    BookV1 book;
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

    std::cout << "=== BookV1 (std::map) -- correctness check ===\n";
    std::cout << "ops applied: " << applied << " / " << ops.size()
              << "  (" << failed << " failed -- workload sirf valid ops banaata, yeh 0 hona chahiye)\n";
    std::cout << "live orders in book: " << book.order_count() << "\n";

    if (book.has_bid() && book.has_ask()) {
        std::cout << "\nbest bid = " << book.best_bid() << " x" << book.best_bid_qty() << '\n';
        std::cout << "best ask = " << book.best_ask() << " x" << book.best_ask_qty() << '\n';
        std::cout << "best_bid < best_ask? "
                  << (book.best_bid() < book.best_ask() ? "haan (sahi)" : "NAHI (BUG!)") << '\n';
    }

    std::cout << "\n(02_orderbook_v1_bench.cpp isi book ki LATENCY measure karta)\n";
    return failed == 0 ? 0 : 1;
}

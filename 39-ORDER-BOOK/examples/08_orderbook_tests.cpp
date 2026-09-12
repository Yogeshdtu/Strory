// 08_orderbook_tests.cpp
// ============================================================
// Unit tests + invariant checks -- 16-testing-order-book.md.
// Har scripted scenario TEENO versions (V1/V2/V3) pe chalta -- taaki
// "correct" ka matlab "V1 jaisa behavior" ho, sirf "V1 khud sahi hai"
// (jo verify nahi ho sakta bina reference ke) na ho.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra 08_orderbook_tests.cpp -o tests && ./tests
// ============================================================

#include "order_workload.hpp"
#include "orderbook_v1_map.hpp"
#include "orderbook_v2_vector.hpp"
#include "orderbook_v3_flat.hpp"

#include <cstdio>
#include <string>
#include <vector>

static int g_run = 0, g_pass = 0;

static void check(bool cond, const std::string& name) {
    ++g_run;
    if (cond) { ++g_pass; std::printf("  PASS  %s\n", name.c_str()); }
    else      { std::printf("  FAIL  %s\n", name.c_str()); }
}

// V3 ek BOUNDED price range rakhta (center ke +-NUM_LEVELS ticks) -- sab
// test prices isi liye ek common `BASE` (V3's center, 10000) ke RELATIVE
// choose kiye gaye hain, taaki V1/V2/V3 teeno SAME literal prices pe test
// ho sakein bina V3 ki range se bahar jaaye.
constexpr Price BASE = 10000;

// ============================================================
//  Scripted scenarios -- generic template, teeno versions pe chalta
// ============================================================
template <class Book>
static void test_add_and_top_of_book(Book& book, const char* tag) {
    book.add(1, true, BASE - 1, 50);    // best bid
    book.add(2, true, BASE - 2, 30);    // second bid
    book.add(3, false, BASE + 5, 20);   // ask
    check(book.best_bid() == BASE - 1, std::string(tag) + ": best_bid == BASE-1");
    check(book.best_bid_qty() == 50, std::string(tag) + ": best_bid_qty == 50");
    check(book.best_ask() == BASE + 5, std::string(tag) + ": best_ask == BASE+5");
}

template <class Book>
static void test_price_time_priority(Book& book, const char* tag) {
    const Price p = BASE - 10;
    book.add(10, true, p, 100);
    book.add(11, true, p, 50);
    book.add(12, true, p, 75);
    const auto ids = book.ids_at_price(true, p);
    check(ids.size() == 3, std::string(tag) + ": 3 orders at the level");
    check(!ids.empty() && ids[0] == 10, std::string(tag) + ": FIFO order[0] == 10 (first added)");
    check(ids.size() > 1 && ids[1] == 11, std::string(tag) + ": FIFO order[1] == 11 (second added)");
    check(ids.size() > 2 && ids[2] == 12, std::string(tag) + ": FIFO order[2] == 12 (third added)");
}

template <class Book>
static void test_partial_cancel_keeps_order(Book& book, const char* tag) {
    const Price p = BASE - 20;
    book.add(20, true, p, 100);
    const bool ok = book.reduce(20, 30);   // partial cancel
    check(ok, std::string(tag) + ": partial cancel succeeds");
    const auto ids = book.ids_at_price(true, p);
    check(ids.size() == 1 && ids[0] == 20, std::string(tag) + ": order 20 still present after partial cancel");
}

template <class Book>
static void test_full_execute_removes_order(Book& book, const char* tag) {
    const Price p = BASE - 30;
    book.add(30, true, p, 50);
    const bool ok = book.reduce(30, 50);   // full execute (fill == remaining qty)
    check(ok, std::string(tag) + ": full execute succeeds");
    const auto ids = book.ids_at_price(true, p);
    check(ids.empty(), std::string(tag) + ": level empty after full execute");
    check(!book.has_bid(), std::string(tag) + ": no bids left (book was empty except this one order)");
}

template <class Book>
static void test_replace(Book& book, const char* tag) {
    const Price old_p = BASE - 40, new_p = BASE - 45;
    book.add(40, true, old_p, 60);
    const bool ok = book.replace(40, 41, true, new_p, 70);
    check(ok, std::string(tag) + ": replace succeeds");
    check(book.ids_at_price(true, old_p).empty(), std::string(tag) + ": old price level empty after replace");
    const auto ids = book.ids_at_price(true, new_p);
    check(ids.size() == 1 && ids[0] == 41, std::string(tag) + ": new order 41 present at new price");
}

template <class Book>
static void test_operations_on_nonexistent_order(Book& book, const char* tag) {
    check(!book.reduce(9999, 10), std::string(tag) + ": reduce(nonexistent) returns false, no crash");
    check(!book.remove(9999), std::string(tag) + ": remove(nonexistent) returns false, no crash");
}

template <class Book>
static void test_duplicate_add_rejected(Book& book, const char* tag) {
    const Price p = BASE - 50;
    book.add(50, true, p, 10);
    const bool ok = book.add(50, true, p, 20);   // same id again
    check(!ok, std::string(tag) + ": duplicate add(id=50) rejected");
}

// ============================================================
//  Cross-version equivalence -- SAME op sequence, V1/V2/V3 pe, state
//  compare karo AFTER EVERY OP (sirf end mein nahi -- ek intermediate
//  divergence bhi pakadna chahiye).
// ============================================================
static bool cross_version_equivalence(std::size_t n, std::uint64_t seed) {
    const auto ops = generate_workload(n, seed);
    BookV1 v1;
    BookV2 v2;
    BookV3 v3(10000, n + 10);   // generous headroom -- workload N ops se zyada live orders kabhi nahi hote

    for (std::size_t i = 0; i < ops.size(); ++i) {
        const auto& op = ops[i];
        switch (op.kind) {
            case OpKind::Add:
                v1.add(op.order_id, op.is_buy, op.price_ticks, op.qty);
                v2.add(op.order_id, op.is_buy, op.price_ticks, op.qty);
                v3.add(op.order_id, op.is_buy, op.price_ticks, op.qty);
                break;
            case OpKind::Execute:
            case OpKind::Cancel:
                v1.reduce(op.order_id, op.qty);
                v2.reduce(op.order_id, op.qty);
                v3.reduce(op.order_id, op.qty);
                break;
            case OpKind::Delete:
                v1.remove(op.order_id);
                v2.remove(op.order_id);
                v3.remove(op.order_id);
                break;
            case OpKind::Replace:
                v1.replace(op.order_id, op.new_order_id, op.is_buy, op.price_ticks, op.qty);
                v2.replace(op.order_id, op.new_order_id, op.is_buy, op.price_ticks, op.qty);
                v3.replace(op.order_id, op.new_order_id, op.is_buy, op.price_ticks, op.qty);
                break;
        }

        if (v1.order_count() != v2.order_count() || v2.order_count() != v3.order_count()) {
            std::printf("  DIVERGENCE at op %zu: order_count V1=%zu V2=%zu V3=%zu\n",
                        i, v1.order_count(), v2.order_count(), v3.order_count());
            return false;
        }
        if (v1.has_bid() != v2.has_bid() || v2.has_bid() != v3.has_bid()) {
            std::printf("  DIVERGENCE at op %zu: has_bid mismatch\n", i);
            return false;
        }
        if (v1.has_bid() && (v1.best_bid() != v2.best_bid() || v2.best_bid() != v3.best_bid())) {
            std::printf("  DIVERGENCE at op %zu: best_bid V1=%lld V2=%lld V3=%lld\n", i,
                        static_cast<long long>(v1.best_bid()), static_cast<long long>(v2.best_bid()),
                        static_cast<long long>(v3.best_bid()));
            return false;
        }
        if (v1.has_ask() && (v1.best_ask() != v2.best_ask() || v2.best_ask() != v3.best_ask())) {
            std::printf("  DIVERGENCE at op %zu: best_ask mismatch\n", i);
            return false;
        }
    }
    return true;
}

int main() {
    std::printf("=== Scripted scenarios (V1, V2, V3) ===\n");
    {
        BookV1 b; test_add_and_top_of_book(b, "V1");
        BookV1 b2; test_price_time_priority(b2, "V1");
        BookV1 b3; test_partial_cancel_keeps_order(b3, "V1");
        BookV1 b4; test_full_execute_removes_order(b4, "V1");
        BookV1 b5; test_replace(b5, "V1");
        BookV1 b6; test_operations_on_nonexistent_order(b6, "V1");
        BookV1 b7; test_duplicate_add_rejected(b7, "V1");
    }
    {
        BookV2 b; test_add_and_top_of_book(b, "V2");
        BookV2 b2; test_price_time_priority(b2, "V2");
        BookV2 b3; test_partial_cancel_keeps_order(b3, "V2");
        BookV2 b4; test_full_execute_removes_order(b4, "V2");
        BookV2 b5; test_replace(b5, "V2");
        BookV2 b6; test_operations_on_nonexistent_order(b6, "V2");
        BookV2 b7; test_duplicate_add_rejected(b7, "V2");
    }
    {
        BookV3 b(10000, 1024); test_add_and_top_of_book(b, "V3");
        BookV3 b2(10000, 1024); test_price_time_priority(b2, "V3");
        BookV3 b3(10000, 1024); test_partial_cancel_keeps_order(b3, "V3");
        BookV3 b4(10000, 1024); test_full_execute_removes_order(b4, "V3");
        BookV3 b5(10000, 1024); test_replace(b5, "V3");
        BookV3 b6(10000, 1024); test_operations_on_nonexistent_order(b6, "V3");
        BookV3 b7(10000, 1024); test_duplicate_add_rejected(b7, "V3");
    }

    std::printf("\n=== Cross-version equivalence (V1 == V2 == V3, checked AFTER EVERY OP) ===\n");
    for (std::uint64_t seed : {1ULL, 42ULL, 777ULL, 123456ULL}) {
        const bool ok = cross_version_equivalence(5000, seed);
        check(ok, "cross-version equivalence, seed=" + std::to_string(seed) + " (5000 ops)");
    }

    std::printf("\n%d / %d tests passed\n", g_pass, g_run);
    return (g_pass == g_run) ? 0 : 1;
}

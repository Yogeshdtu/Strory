// 08_legacy_to_modern.cpp
// ============================================================
// Same program, twice: a C++98-style version and its modern
// (C++17/20) rewrite. Side by side -- what changed and why.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 08_legacy_to_modern.cpp -o lm && ./lm
// ============================================================

#include <algorithm>
#include <cstdio>
#include <map>
#include <memory>
#include <numeric>
#include <ranges>
#include <string>
#include <vector>

struct Trade { std::string sym; double px; long qty; };

// ============================================================
//  LEGACY (C++98 idioms)
// ============================================================
namespace legacy {

// raw owning pointer, manual delete, index loops, typedef, functor struct
struct SymMatches {
    std::string want;
    SymMatches(const std::string& w) : want(w) {}
    bool operator()(const Trade& t) const { return t.sym == want; }
};

double totalNotional(const std::vector<Trade>& trades, const std::string& sym) {
    double sum = 0.0;
    for (std::vector<Trade>::const_iterator it = trades.begin(); it != trades.end(); ++it) {
        if (it->sym == sym) {
            sum += it->px * it->qty;
        }
    }
    return sum;
}

Trade* biggestTrade(const std::vector<Trade>& trades) {   // returns a raw pointer (into the vector -- fragile)
    if (trades.empty()) return 0;                          // NULL, actually 0
    const Trade* best = &trades[0];
    for (std::size_t i = 1; i < trades.size(); ++i) {
        if (trades[i].qty > best->qty) best = &trades[i];
    }
    return const_cast<Trade*>(best);
}

void run() {
    std::vector<Trade> trades;
    trades.push_back(Trade());  trades.back().sym = "AAPL"; trades.back().px = 190; trades.back().qty = 100;
    trades.push_back(Trade());  trades.back().sym = "MSFT"; trades.back().px = 400; trades.back().qty = 50;
    trades.push_back(Trade());  trades.back().sym = "AAPL"; trades.back().px = 191; trades.back().qty = 300;

    std::printf("  [legacy] AAPL notional = %.2f\n", totalNotional(trades, "AAPL"));
    Trade* big = biggestTrade(trades);
    if (big) std::printf("  [legacy] biggest: %s x%ld\n", big->sym.c_str(), big->qty);

    // count per symbol -- verbose map insert
    std::map<std::string, int> counts;
    for (std::size_t i = 0; i < trades.size(); ++i) {
        std::map<std::string, int>::iterator f = counts.find(trades[i].sym);
        if (f == counts.end()) counts.insert(std::make_pair(trades[i].sym, 1));
        else ++f->second;
    }
    for (std::map<std::string, int>::const_iterator it = counts.begin(); it != counts.end(); ++it)
        std::printf("  [legacy] %s: %d\n", it->first.c_str(), it->second);
}

} // namespace legacy

// ============================================================
//  MODERN (C++17/20 idioms)
// ============================================================
namespace modern {

double totalNotional(const std::vector<Trade>& trades, std::string_view sym) {
    // ranges: filter by symbol, project to notional, sum -- no manual loop
    auto notionals = trades
                   | std::views::filter([sym](const Trade& t){ return t.sym == sym; })
                   | std::views::transform([](const Trade& t){ return t.px * static_cast<double>(t.qty); });
    return std::accumulate(notionals.begin(), notionals.end(), 0.0);
}

const Trade* biggestTrade(const std::vector<Trade>& trades) {
    auto it = std::ranges::max_element(trades, {}, &Trade::qty);   // projection: compare by .qty
    return it == trades.end() ? nullptr : &*it;
}

void run() {
    // aggregate init, emplace_back, uniform
    std::vector<Trade> trades{
        {"AAPL", 190.0, 100},
        {"MSFT", 400.0,  50},
        {"AAPL", 191.0, 300},
    };

    std::printf("  [modern] AAPL notional = %.2f\n", totalNotional(trades, "AAPL"));
    if (const Trade* big = biggestTrade(trades))
        std::printf("  [modern] biggest: %s x%ld\n", big->sym.c_str(), big->qty);

    // count per symbol -- operator[] does insert-or-find
    std::map<std::string, int> counts;
    for (const auto& t : trades) ++counts[t.sym];
    for (const auto& [sym, n] : counts)                 // structured binding
        std::printf("  [modern] %s: %d\n", sym.c_str(), n);
}

} // namespace modern

int main() {
    std::printf("=== legacy (C++98 idioms) ===\n");
    legacy::run();
    std::printf("\n=== modern (C++17/20 idioms) ===\n");
    modern::run();

    std::printf(
        "\n"
        "  Changes: iterator loops -> range-for / ranges pipelines; functor structs ->\n"
        "  lambdas; `0`/NULL -> nullptr; typedef spelled-out iterators -> auto; verbose\n"
        "  map find/insert -> operator[] / structured bindings; raw owning pointers ->\n"
        "  values / smart pointers / spans. Same behaviour, less code, fewer bug surfaces.\n");
    return 0;
}

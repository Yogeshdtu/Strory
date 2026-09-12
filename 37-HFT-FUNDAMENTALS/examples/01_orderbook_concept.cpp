// 01_orderbook_concept.cpp
// ============================================================
// Order book ka SIMPLEST conceptual model — price -> resting qty
// ============================================================
//   g++ -std=c++20 -Wall -Wextra 01_orderbook_concept.cpp -o ob && ./ob
// ============================================================
//
// Yeh sirf CONCEPT dikhane ke liye hai: order book "hai kya" — do sorted
// price-level maps (bids descending, asks ascending). Yeh production-grade
// data structure NAHI hai (std::map log-time hai, per-order cancel O(1)
// nahi). Woh version — intrusive doubly-linked lists per price level,
// flat price-indexed array, O(1) cancel — folder 39-ORDER-BOOK mein
// banega, measured aur optimized.

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <map>

// Price ko TICKS mein rakho (integer), double mein NAHI.
// Kyun: 03-VARIABLES-DATA-TYPES/06 mein dekha tha ki 0.1 + 0.2 != 0.3
// floating point mein. Ek order book mein price equality baar-baar check
// hoti (matching, level lookup) — floating point rounding se do "same"
// price alag keys ban jaate. Real exchanges bhi price ko integer ticks
// mein hi bhejte hain (tick size lesson 08 mein).
using Price = std::int64_t;  // e.g. 10050 = Rs 100.50 agar tick = paisa
using Qty   = std::int64_t;

// Sabse simple book: har price level pe sirf TOTAL resting quantity.
// (Yeh "L2" hai — level 2. Individual order tracking "L3" hai, lesson
//  02 aur 38-MARKET-DATA/02 mein L1/L2/L3 ka fark aayega.)
class SimpleBook {
public:
    // Note: bids_/asks_ do ALAG types hain (comparator alag) — isliye
    // ternary se ek reference nahi nikaal sakte (`is_bid ? bids_ : asks_`
    // compile nahi hoga, dono branch ka common type nahi milta). if/else
    // mein duplicate karna padta — chhota sa price is genericity ka.
    void add(bool is_bid, Price px, Qty qty) {
        if (is_bid) bids_[px] += qty;
        else        asks_[px] += qty;
    }

    // qty=0 ya negative ho jaaye to level hata do (sold out / fully cancelled)
    void reduce(bool is_bid, Price px, Qty qty) {
        if (is_bid) reduce_side(bids_, px, qty);
        else        reduce_side(asks_, px, qty);
    }

    bool has_bid() const { return !bids_.empty(); }
    bool has_ask() const { return !asks_.empty(); }

    // bids_ std::greater se sorted hai -> begin() = HIGHEST price = best bid.
    Price best_bid() const { return bids_.begin()->first; }
    // asks_ default ascending -> begin() = LOWEST price = best ask.
    Price best_ask() const { return asks_.begin()->first; }

    void print_depth(std::size_t levels) const {
        std::cout << std::setw(10) << "bid qty" << std::setw(9) << "px"
                  << "  |  " << std::setw(9) << "px" << std::setw(10) << "ask qty" << '\n';
        std::cout << std::string(46, '-') << '\n';
        auto bit = bids_.begin();
        auto ait = asks_.begin();
        for (std::size_t i = 0; i < levels; ++i) {
            if (bit != bids_.end()) {
                std::cout << std::setw(10) << bit->second << std::setw(9) << bit->first;
                ++bit;
            } else {
                std::cout << std::setw(10) << ' ' << std::setw(9) << ' ';
            }
            std::cout << "  |  ";
            if (ait != asks_.end()) {
                std::cout << std::setw(9) << ait->first << std::setw(10) << ait->second;
                ++ait;
            }
            std::cout << '\n';
        }
    }

private:
    template <class Side>
    static void reduce_side(Side& side, Price px, Qty qty) {
        auto it = side.find(px);
        if (it == side.end()) return;
        it->second -= qty;
        if (it->second <= 0) side.erase(it);
    }

    std::map<Price, Qty, std::greater<Price>> bids_;  // best = begin()
    std::map<Price, Qty> asks_;                        // best = begin()
};

int main() {
    SimpleBook book;

    // ============================================================
    //  1. BOOK BANAO — kai price levels dono taraf
    // ============================================================
    // Real market mein yeh "add order" messages har microsecond aate
    // (38-MARKET-DATA is incremental update). Yahan hardcoded seed data.
    book.add(true, 10048, 200);   // bid: 100.48, 200 qty
    book.add(true, 10047, 500);
    book.add(true, 10046, 800);
    book.add(true, 10045, 1200);

    book.add(false, 10050, 150);  // ask: 100.50, 150 qty
    book.add(false, 10051, 400);
    book.add(false, 10052, 900);
    book.add(false, 10053, 300);

    std::cout << "=== Initial book (top 4 levels each side) ===\n";
    book.print_depth(4);

    // ============================================================
    //  2. BEST BID/ASK, SPREAD, MID
    // ============================================================
    if (book.has_bid() && book.has_ask()) {
        const Price bb = book.best_bid();
        const Price ba = book.best_ask();
        std::cout << "\nbest bid = " << bb << "  best ask = " << ba
                  << "  spread = " << (ba - bb) << " ticks"
                  << "  mid = " << static_cast<double>(bb + ba) / 2.0 << '\n';
    }

    // ============================================================
    //  3. EK ORDER AATA HAI (add) — top-of-book badalta hai
    // ============================================================
    // Naya aggressive-looking bid best price se upar aata hai -> woh
    // naya best bid ban jaata. Yeh "top of book changed" event hai jo
    // strategy ko trigger karta (10-hft-strategies-overview).
    book.add(true, 10049, 100);
    std::cout << "\n=== Naya bid 10049 aane ke baad ===\n";
    book.print_depth(4);
    std::cout << "naya best bid = " << book.best_bid() << '\n';

    // ============================================================
    //  4. CANCEL / FILL — reduce()
    // ============================================================
    // best bid poora fill/cancel ho jaata -> price level gayab, best bid
    // agle level pe "fall back" karta. Yeh price movement ka ek source hai.
    book.reduce(true, 10049, 100);
    std::cout << "\n=== 10049 poora cancel hone ke baad ===\n";
    std::cout << "best bid wapas = " << book.best_bid() << '\n';

    return 0;
}

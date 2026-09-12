// 02_spread_calculator.cpp
// ============================================================
// Bid/ask/spread/mid/microprice/imbalance — L1 quote se nikalne wale
// numbers, jo 05-bid-ask-spread.md mein explain honge
// ============================================================
//   g++ -std=c++20 -Wall -Wextra 02_spread_calculator.cpp -o spread && ./spread
// ============================================================

#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

struct L1Quote {
    std::string symbol;
    double bid_px;
    double bid_qty;
    double ask_px;
    double ask_qty;
};

// mid = seedha beech ka price. Sabse simple, par size ko ignore karta.
double mid_price(const L1Quote& q) {
    return (q.bid_px + q.ask_px) / 2.0;
}

// spread = ask - bid, aur bps (basis points) mein — taaki alag price
// waale stocks ka spread compare ho sake (Rs 5 ka spread Rs 100 ke stock
// pe bahut hai, Rs 5000 ke stock pe kuch nahi).
double spread_abs(const L1Quote& q) { return q.ask_px - q.bid_px; }
double spread_bps(const L1Quote& q) {
    return spread_abs(q) / mid_price(q) * 10000.0;
}

// microprice: size-weighted mid. Common heuristic (exact formula venue
// aur paper ke hisaab se thoda differ karta) —
//   micro = (bid_px * ask_qty + ask_px * bid_qty) / (bid_qty + ask_qty)
// Intuition: OPPOSITE side ki qty se weight milta. Agar bid_qty bahut
// bada hai (bahut buyers) to price micro ASK ki taraf khinchta — kyunki
// itni buying pressure hai ki ask level jaldi khaaya ja sakta, price
// upar jaane ka zyada chance. Simple mid yeh signal miss kar deta.
double microprice(const L1Quote& q) {
    return (q.bid_px * q.ask_qty + q.ask_px * q.bid_qty) / (q.bid_qty + q.ask_qty);
}

// imbalance: -1 (sab ask side) se +1 (sab bid side). Order-flow signal —
// kai simple market-making/short-term strategies (10) iska use karti.
double imbalance(const L1Quote& q) {
    return (q.bid_qty - q.ask_qty) / (q.bid_qty + q.ask_qty);
}

void print_row(const L1Quote& q) {
    std::cout << std::left << std::setw(6) << q.symbol << std::right
              << std::fixed << std::setprecision(2)
              << "  bid " << std::setw(9) << q.bid_px << " x" << std::setw(6) << q.bid_qty
              << "   ask " << std::setw(9) << q.ask_px << " x" << std::setw(6) << q.ask_qty
              << "  |  mid=" << std::setw(9) << mid_price(q)
              << "  spread=" << std::setw(6) << spread_abs(q)
              << " (" << std::setw(6) << std::setprecision(1) << spread_bps(q) << " bps)"
              << "  micro=" << std::setprecision(3) << std::setw(9) << microprice(q)
              << "  imb=" << std::setw(6) << std::setprecision(2) << imbalance(q)
              << '\n';
}

int main() {
    // ============================================================
    //  Kuch alag L1 snapshots — dhyaan se dekho micro/imbalance kaise
    //  shift karte hain jab bid_qty aur ask_qty ka RATIO badalta hai,
    //  chahe bid_px/ask_px/mid bilkul same rahein.
    // ============================================================
    std::vector<L1Quote> quotes = {
        {"BAL1", 100.00, 500.0, 100.10, 500.0},   // balanced -> micro == mid
        {"BUY-P", 100.00, 5000.0, 100.10, 500.0}, // bid side heavy -> micro upar khinchta
        {"SELL-P", 100.00, 500.0, 100.10, 5000.0},// ask side heavy -> micro neeche khinchta
        {"TIGHT", 2500.00, 800.0, 2500.05, 800.0},// bade price, chhota spread -> bps chhota
        {"WIDE",  50.00, 300.0, 51.00, 300.0},    // chhote price, bada spread -> bps bada
    };

    std::cout << "=== L1 quotes: spread / microprice / imbalance ===\n\n";
    for (const auto& q : quotes) print_row(q);

    // ============================================================
    //  Nichod: BAL1 aur BUY-P/SELL-P ka bid_px/ask_px/mid IDENTICAL hai.
    //  Sirf QTY ratio badla — aur microprice + imbalance ne woh pakad
    //  liya jo mid ne miss kar diya.
    // ============================================================
    std::cout << "\nnote: BAL1 vs BUY-P vs SELL-P — bid_px/ask_px/mid sab\n"
                 "same hain; sirf qty ratio badla. mid ise miss karta,\n"
                 "microprice aur imbalance dono pakadte hain.\n";

    return 0;
}

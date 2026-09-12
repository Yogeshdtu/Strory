// 04_matching_rules.cpp
// ============================================================
// Price-time priority (FIFO) vs pro-rata matching — same book, same
// incoming order, do alag matching rules, do alag results
// ============================================================
//   g++ -std=c++20 -Wall -Wextra 04_matching_rules.cpp -o match && ./match
// ============================================================
//
// Yeh 07-price-time-priority.md ka demo hai. Concept-level — real
// matching engine (38/39/40 folders) mein yeh logic order book ke saath
// tightly integrated hota aur bahut zyada edge cases handle karta
// (self-trade prevention, min-qty, iceberg refresh, ...).

#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

struct RestingOrder {
    int id;
    long long qty;
    int arrival_seq;  // chhota = pehle aaya = time priority mein aage
};

struct Fill {
    int order_id;
    long long filled_qty;
};

// ============================================================
//  PRICE-TIME PRIORITY (FIFO) — jyaadatar exchanges (NSE, Nasdaq, ...)
//  ka default rule. Same price pe: jo PEHLE aaya, use PEHLE fill milta.
//  Poora fill milta before doosre order ko kuch bhi milta.
// ============================================================
std::vector<Fill> match_fifo(std::vector<RestingOrder> book, long long incoming_qty) {
    std::vector<Fill> fills;
    // arrival_seq ke hisaab se already sorted maan rahe (real book mein
    // yeh insertion order hi hota — koi sort nahi lagta).
    for (auto& o : book) {
        if (incoming_qty <= 0) break;
        const long long fill = std::min(o.qty, incoming_qty);
        fills.push_back({o.id, fill});
        incoming_qty -= fill;
    }
    return fills;
}

// ============================================================
//  PRO-RATA — kuch derivatives venues (e.g. kuch futures/options books)
//  is model ka use karte: fill SIZE ke proportion mein baantta hai, arrival
//  time se farq nahi padta (ya sirf tie-break ke liye padta).
//  Formula: fill_i = round(incoming * qty_i / total_resting_qty)
//  Rounding se total thoda kam/zyada ho sakta — leftover ko convention
//  se allocate karte (yahan: sabse bade order ko, ya time-priority se).
// ============================================================
std::vector<Fill> match_prorata(std::vector<RestingOrder> book, long long incoming_qty) {
    long long total = 0;
    for (const auto& o : book) total += o.qty;

    std::vector<Fill> fills;
    long long allocated = 0;
    for (const auto& o : book) {
        long long share = (incoming_qty * o.qty) / total;  // integer floor
        fills.push_back({o.id, share});
        allocated += share;
    }

    // leftover (rounding se bacha hua) — common convention: time-priority
    // se sabse pehle order ko de do (yahan book[0], jo arrival mein sabse
    // pehla hai) taaki koi qty un-allocated na reh jaaye.
    long long leftover = incoming_qty - allocated;
    if (leftover > 0 && !fills.empty()) fills.front().filled_qty += leftover;

    return fills;
}

void print_fills(const std::string& label, const std::vector<Fill>& fills, long long incoming_qty) {
    std::cout << label << " (incoming sell qty = " << incoming_qty << "):\n";
    long long total = 0;
    for (const auto& f : fills) {
        if (f.filled_qty <= 0) continue;
        std::cout << "  order " << f.order_id << "  filled " << std::setw(4) << f.filled_qty << '\n';
        total += f.filled_qty;
    }
    std::cout << "  total filled = " << total << "\n\n";
}

int main() {
    // ============================================================
    //  EK PRICE LEVEL PE 4 RESTING BID ORDERS (arrival order mein)
    // ============================================================
    // seq 1 pehle aaya (sabse zyada time-priority), seq 4 sabse baad.
    const std::vector<RestingOrder> level = {
        {101, 100, 1},
        {102,  50, 2},
        {103, 200, 3},
        {104,  75, 4},
    };
    long long total_resting = 0;
    for (const auto& o : level) total_resting += o.qty;

    std::cout << "=== Resting bids at this price (arrival order) ===\n";
    for (const auto& o : level)
        std::cout << "  order " << o.id << "  qty=" << std::setw(4) << o.qty
                  << "  arrived #" << o.arrival_seq << '\n';
    std::cout << "  total resting qty = " << total_resting << "\n\n";

    // Incoming aggressive SELL order, price-crossing, qty = 180
    // (poore level ko khatam nahi karta -> dono rules ka farq dikhta)
    const long long incoming = 180;

    // ============================================================
    //  DONO RULES SE MATCH KARO, RESULT COMPARE KARO
    // ============================================================
    print_fills("--- FIFO (price-time priority) ---", match_fifo(level, incoming), incoming);
    print_fills("--- PRO-RATA (size-proportional) ---", match_prorata(level, incoming), incoming);

    // ============================================================
    //  NICHOD
    // ============================================================
    std::cout << "FIFO: order 101 (pehla aaya) POORA fill hota (100), phir 102\n"
                 "poora (50), phir 103 sirf 30 (incoming khatam). order 104 ko\n"
                 "KUCH nahi milta — poori tarah time se piche.\n\n"
                 "PRO-RATA: sab 4 orders ko UNKI SIZE ke hisaab se hissa milta,\n"
                 "arrival time se koi farq nahi padta — order 104 (sabse baad\n"
                 "aaya) ko bhi kuch fill milta kyunki uski size thi.\n\n"
                 "Yeh isliye maayne rakhta: FIFO mein FAST hona (pehle order\n"
                 "bhejna) sabse important edge hai. Pro-rata mein SIZE bhejna\n"
                 "edge hai, speed utni nahi (isiliye kuch pro-rata books mein\n"
                 "latency-arb ka faayda FIFO books se kam hota).\n";

    return 0;
}

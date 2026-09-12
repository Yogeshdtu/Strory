// 03_latency_budget.cpp
// ============================================================
// Tick-to-trade latency budget calculator — 14-latency-budget.md ka tool
// ============================================================
//   g++ -std=c++20 -Wall -Wextra 03_latency_budget.cpp -o budget && ./budget
// ============================================================
//
// IMPORTANT: neeche ke stage names aur ns numbers ILLUSTRATIVE hain —
// order-of-magnitude sahi (nanoseconds se low-microseconds range, jo
// real co-located HFT systems mein hota hai), par kisi specific firm ya
// venue ke real production numbers NAHI hain (woh publicly verifiable
// nahi hain, aur venue/hardware/strategy ke hisaab se bahut alag hote).
// Iss tool ka maqsad NUMBER yaad karna nahi, **method** seekhna hai:
// budget banao -> measure karo -> jo stage budget se upar hai wahi
// pehle fix karo (Amdahl, 35-PROFILING/01).

#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

struct Stage {
    std::string name;
    double budget_ns;   // is stage ko kitna milna "chahiye"
    double p50_ns;       // measured typical
    double p99_9_ns;      // measured tail — HFT mein yeh number maayne rakhta (35/05)
};

int main() {
    const double total_budget_ns = 1000.0;  // wire-to-wire target (illustrative)

    std::vector<Stage> stages = {
        {"NIC RX + kernel bypass", 150, 120, 180},
        {"feed decode/parse",      150, 140, 300},
        {"book update",            200, 180, 250},
        {"strategy decision",      200, 190, 900},  // <- tail blows the budget
        {"risk check",             100,  80, 110},
        {"order encode",           100,  90, 120},
        {"NIC TX",                 150, 140, 170},
    };

    // ============================================================
    //  1. TABLE PRINT — har stage ka budget vs measured p50 vs p99.9
    // ============================================================
    std::cout << std::fixed << std::setprecision(0);
    std::cout << std::left  << std::setw(24) << "stage"
              << std::right << std::setw(10) << "budget"
              << std::setw(10) << "p50"
              << std::setw(10) << "p99.9"
              << "  flag\n";
    std::cout << std::string(60, '-') << '\n';

    double sum_budget = 0, sum_p50 = 0, sum_p99_9 = 0;
    for (const auto& s : stages) {
        const bool over_p50   = s.p50_ns   > s.budget_ns;
        const bool over_p99_9 = s.p99_9_ns > s.budget_ns;
        std::cout << std::left  << std::setw(24) << s.name
                  << std::right << std::setw(10) << s.budget_ns
                  << std::setw(10) << s.p50_ns
                  << std::setw(10) << s.p99_9_ns
                  << "  " << (over_p99_9 ? (over_p50 ? "OVER (p50 too!)" : "over at tail")
                                          : "ok");
        std::cout << '\n';
        sum_budget += s.budget_ns;
        sum_p50    += s.p50_ns;
        sum_p99_9  += s.p99_9_ns;
    }

    std::cout << std::string(60, '-') << '\n';
    std::cout << std::left  << std::setw(24) << "TOTAL"
              << std::right << std::setw(10) << sum_budget
              << std::setw(10) << sum_p50
              << std::setw(10) << sum_p99_9 << "\n\n";

    // ============================================================
    //  2. BUDGET vs GOAL
    // ============================================================
    std::cout << "target (wire-to-wire): " << total_budget_ns << " ns\n";
    std::cout << "sum of stage budgets:  " << sum_budget << " ns\n";
    std::cout << "measured p50 total:    " << sum_p50 << " ns  ("
              << (sum_p50 <= total_budget_ns ? "under target" : "OVER target") << ")\n";
    std::cout << "measured p99.9 total:  " << sum_p99_9 << " ns  ("
              << (sum_p99_9 <= total_budget_ns ? "under target" : "OVER target") << ")\n\n";

    // ============================================================
    //  3. SABSE BADA TAIL OFFENDER DHOONDO (Amdahl: yehi pehle fix karo)
    // ============================================================
    const Stage* worst = &stages.front();
    for (const auto& s : stages) {
        if ((s.p99_9_ns - s.budget_ns) > (worst->p99_9_ns - worst->budget_ns)) worst = &s;
    }
    std::cout << "sabse bada tail-budget overrun: \"" << worst->name << "\" ("
              << (worst->p99_9_ns - worst->budget_ns) << " ns over at p99.9)\n";
    std::cout << "-> agla optimization pass YAHIN lagao, doosri stages pe nahi\n"
                 "   (chhoti stage ko shave karne se total mushkil se hilega).\n";

    return 0;
}

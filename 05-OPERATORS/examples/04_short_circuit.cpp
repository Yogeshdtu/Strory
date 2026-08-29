// 04_short_circuit.cpp
// ============================================================
// Short-circuit evaluation -- safety aur performance
// ============================================================
//   g++ -std=c++20 -Wall -Wextra 04_short_circuit.cpp -o sc && ./sc
// ============================================================

#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>

// Side effect track karne ke liye
int callCount = 0;
bool sideEffect(const char* name) {
    ++callCount;
    std::cout << "[" << name << " CHALA] ";
    return true;
}

struct Node {
    int value;
};

// Ek mehnga check simulate karo
bool expensiveCheck(int x) {
    volatile int sum = 0;
    for (int i = 0; i < 200; ++i) sum += i * x;
    return sum > 0;
}

bool cheapCheck(int x) {
    return x > 0;
}

int main() {
    std::cout << std::boolalpha;

    // ============================================================
    //  1. SHORT-CIRCUIT KA BASIC DEMO
    // ============================================================
    std::cout << "===== 1. SHORT-CIRCUIT =====\n";

    std::cout << "  false && f()  -> ";
    callCount = 0;
    if (false && sideEffect("A")) {}
    std::cout << " (f() " << callCount << " baar chala)  ✅ skip hua\n";

    std::cout << "  true  && f()  -> ";
    callCount = 0;
    if (true && sideEffect("B")) {}
    std::cout << " (f() " << callCount << " baar chala)\n";

    std::cout << "  true  || f()  -> ";
    callCount = 0;
    if (true || sideEffect("C")) {}
    std::cout << " (f() " << callCount << " baar chala)  ✅ skip hua\n";

    std::cout << "  false || f()  -> ";
    callCount = 0;
    if (false || sideEffect("D")) {}
    std::cout << " (f() " << callCount << " baar chala)\n";

    // ============================================================
    //  2. ⚠️ & aur | SHORT-CIRCUIT NAHI KARTE
    // ============================================================
    std::cout << "\n===== 2. ⚠️ & vs && =====\n";

    std::cout << "  false &  f()  -> ";
    callCount = 0;
    if (false & static_cast<int>(sideEffect("E"))) {}
    std::cout << " (f() " << callCount << " baar chala)  ⚠️ CHAL GAYA!\n";

    std::cout << "  true  |  f()  -> ";
    callCount = 0;
    if (true | static_cast<int>(sideEffect("F"))) {}
    std::cout << " (f() " << callCount << " baar chala)  ⚠️ CHAL GAYA!\n";

    std::cout << "\n  RULE: booleans ke liye HAMESHA && aur || use karo.\n";

    // ============================================================
    //  3. SAFETY: NULL CHECK
    // ============================================================
    std::cout << "\n===== 3. SAFETY: NULL CHECK =====\n";
    Node* nullPtr = nullptr;

    // ✅ SAFE -- ptr null hai to ptr->value evaluate hi nahi hoga
    if (nullPtr != nullptr && nullPtr->value > 5) {
        std::cout << "  kabhi nahi chalega\n";
    } else {
        std::cout << "  ✅ (ptr != nullptr && ptr->value > 5)  -> safe, koi crash nahi\n";
    }

    // ❌ Yeh CRASH karta:
    //    if (nullPtr->value > 5 && nullPtr != nullptr) { }
    std::cout << "  ❌ (ptr->value > 5 && ptr != nullptr)  -> CRASH hota\n";
    std::cout << "  ⚠️ ORDER MATTER KARTA HAI: check pehle, use baad mein\n";

    // ============================================================
    //  4. SAFETY: ARRAY BOUNDS
    // ============================================================
    std::cout << "\n===== 4. SAFETY: ARRAY BOUNDS =====\n";
    const std::vector<int> v = {10, 20, 30};
    const std::size_t badIndex = 100;

    if (badIndex < v.size() && v[badIndex] == 10) {
        std::cout << "  found\n";
    } else {
        std::cout << "  ✅ (index < size && arr[index] == x)  -> safe\n";
    }
    std::cout << "  ❌ (arr[index] == x && index < size)  -> out of bounds read\n";

    // Empty container
    const std::vector<int> empty;
    if (!empty.empty() && empty.front() == 5) {
        std::cout << "  found\n";
    } else {
        std::cout << "  ✅ (!v.empty() && v.front() == x)     -> safe on empty\n";
    }

    // ============================================================
    //  5. PERFORMANCE: SASTA CHECK PEHLE
    // ============================================================
    std::cout << "\n===== 5. PERFORMANCE: ORDER MATTERS =====\n";
    constexpr int N = 300000;
    std::vector<int> data(1000);
    // Zyada tar values NEGATIVE -- taaki cheapCheck aksar false de
    for (std::size_t i = 0; i < data.size(); ++i) {
        data[i] = (i % 10 == 0) ? 1 : -1;
    }

    // ✅ Sasta check pehle -- 90% cases mein mehnga check chalega hi nahi
    auto t1 = std::chrono::steady_clock::now();
    int count1 = 0;
    for (int i = 0; i < N; ++i) {
        const int x = data[static_cast<std::size_t>(i) % data.size()];
        if (cheapCheck(x) && expensiveCheck(x)) ++count1;
    }
    auto t2 = std::chrono::steady_clock::now();

    // ❌ Mehnga check pehle -- HAMESHA chalega
    int count2 = 0;
    for (int i = 0; i < N; ++i) {
        const int x = data[static_cast<std::size_t>(i) % data.size()];
        if (expensiveCheck(x) && cheapCheck(x)) ++count2;
    }
    auto t3 = std::chrono::steady_clock::now();

    auto ms = [](auto a, auto b) {
        return std::chrono::duration<double, std::milli>(b - a).count();
    };

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  " << N << " iterations, 90% cheap check FALSE deta hai:\n";
    std::cout << "    cheap && expensive : " << std::setw(8) << ms(t1, t2)
              << " ms   ✅\n";
    std::cout << "    expensive && cheap : " << std::setw(8) << ms(t2, t3)
              << " ms   ⚠️ ";
    if (ms(t1, t2) > 0.0) std::cout << (ms(t2,t3) / ms(t1,t2)) << "x SLOWER";
    std::cout << "\n";
    std::cout << "  (results match? " << (count1 == count2 ? "haan ✅" : "NAHI ❌") << ")\n";

    std::cout << "\n  HFT: hot path mein sabse SELECTIVE aur SASTA check pehle rakho.\n";
    std::cout << "  Zyada tar messages jaldi reject ho jaayenge aur mehnge\n";
    std::cout << "  validation kabhi chalenge hi nahi.\n";

    // ============================================================
    //  6. DE MORGAN'S LAWS
    // ============================================================
    std::cout << "\n===== 6. DE MORGAN'S LAWS =====\n";
    std::cout << "  !(a && b)  ==  !a || !b\n";
    std::cout << "  !(a || b)  ==  !a && !b\n\n";

    for (int i = 0; i < 4; ++i) {
        const bool a = (i & 2) != 0;
        const bool b = (i & 1) != 0;
        std::cout << "    a=" << std::setw(5) << a << " b=" << std::setw(5) << b
                  << "   !(a&&b)=" << std::setw(5) << !(a && b)
                  << " (!a||!b)=" << std::setw(5) << (!a || !b)
                  << "   !(a||b)=" << std::setw(5) << !(a || b)
                  << " (!a&&!b)=" << std::setw(5) << (!a && !b) << "\n";
    }

    std::cout << "\n  Use: double negatives hatana\n";
    std::cout << "    ⚠️ if (!(!isValid || !isReady))\n";
    std::cout << "    ✅ if (isValid && isReady)\n";

    // ============================================================
    //  7. BRANCHLESS COUNTING
    // ============================================================
    std::cout << "\n===== 7. BRANCHLESS COUNTING =====\n";
    const int arr[] = {50, 150, 200, 80, 300, 20, 120};

    int countBranch = 0;
    for (const int x : arr) { if (x > 100) ++countBranch; }

    int countBranchless = 0;
    for (const int x : arr) { countBranchless += (x > 100); }   // true=1, false=0

    std::cout << "  Array: {50, 150, 200, 80, 300, 20, 120}\n";
    std::cout << "  100 se bade (if ke saath):  " << countBranch << "\n";
    std::cout << "  100 se bade (branchless):   " << countBranchless << "\n";
    std::cout << "\n  ⚠️ Branchless HAMESHA tez nahi hota!\n";
    std::cout << "  Predictable branches CPU sahi guess kar leti hai (~0 cost).\n";
    std::cout << "  Unpredictable (50/50) branches pe misprediction ~15-20 cycles.\n";
    std::cout << "  Isliye: MEASURE karo, assume mat karo. (Folder 31/36)\n";

    return 0;
}

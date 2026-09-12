// 02_range_based.cpp
// ============================================================
// range-based for (C++11) -- aur COPY ka trap
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 02_range_based.cpp -o rf && ./rf
// ============================================================
// Yeh dikhata hai:
//   1. for (auto x : c)        -> har element ki COPY
//   2. for (auto& x : c)       -> reference -- modify kar sakte ho
//   3. for (const auto& x : c) -> read-only, no copy  (DEFAULT choice)
//   4. copy ki asli cost (bade elements pe) -- measured
//   5. structured bindings: for (auto& [k, v] : map)
//   6. C-array bhi chalega; par pointer decay ke baad NAHI
// ============================================================

#include <chrono>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <vector>

int main() {
    // ============================================================
    //  1. auto x -> COPY. Original ko change nahi karta.
    // ============================================================
    std::cout << "===== 1. for (auto x : v)  -- COPY =====\n";
    std::vector<int> v = {1, 2, 3, 4, 5};

    for (auto x : v) {      // har iteration mein x = v[i] ki ek copy
        x *= 10;            // sirf copy badli
    }
    std::cout << "  v ke baad: ";
    for (auto x : v) std::cout << x << " ";
    std::cout << "  <- unchanged (copy modify hui thi)\n";

    // ============================================================
    //  2. auto& x -> REFERENCE. Original change hota hai.
    // ============================================================
    std::cout << "\n===== 2. for (auto& x : v)  -- REFERENCE =====\n";
    for (auto& x : v) {     // x seedha v[i] hai
        x *= 10;
    }
    std::cout << "  v ke baad: ";
    for (const auto& x : v) std::cout << x << " ";
    std::cout << "  <- badal gaya\n";

    // ============================================================
    //  3. const auto& x -> read-only, NO copy. Yeh default choice hai.
    // ============================================================
    std::cout << "\n===== 3. for (const auto& x : v)  -- READ, no copy =====\n";
    long long sum = 0;
    for (const auto& x : v) {
        sum += x;          // x badal nahi sakte (const) -- compile error agar koshish
    }
    std::cout << "  sum = " << sum << "\n";
    std::cout << "  RULE: sirf padhna hai -> const auto&\n"
                 "        modify karna hai -> auto&\n"
                 "        chhoti trivial value + copy chahiye -> auto\n";

    // ============================================================
    //  4. COPY ki asli COST -- bade elements pe
    // ============================================================
    std::cout << "\n===== 4. COPY vs REFERENCE -- cost (std::string elements) =====\n";
    std::vector<std::string> names(200000, std::string(64, 'x'));   // 200k * 64-byte strings

    auto t0 = std::chrono::steady_clock::now();
    std::size_t lenByCopy = 0;
    for (auto s : names) {              // ⚠️ har iteration ek 64-byte string COPY (heap alloc!)
        lenByCopy += s.size();
    }
    auto t1 = std::chrono::steady_clock::now();
    std::size_t lenByRef = 0;
    for (const auto& s : names) {       // ✅ koi copy nahi
        lenByRef += s.size();
    }
    auto t2 = std::chrono::steady_clock::now();

    auto ms = [](auto a, auto b) {
        return std::chrono::duration<double, std::milli>(b - a).count();
    };
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  for (auto s : names)        : " << std::setw(7) << ms(t0, t1) << " ms\n";
    std::cout << "  for (const auto& s : names) : " << std::setw(7) << ms(t1, t2) << " ms\n";
    if (ms(t1, t2) > 0.0)
        std::cout << "  copy version ~" << (ms(t0, t1) / ms(t1, t2)) << "x slower "
                  << "(200k heap allocations + frees)\n";
    std::cout << "  (lengths match? " << (lenByCopy == lenByRef ? "haan" : "NAHI") << ")\n";

    // ============================================================
    //  5. STRUCTURED BINDINGS -- map pe
    // ============================================================
    std::cout << "\n===== 5. for (auto& [k, val] : map) =====\n";
    std::map<std::string, int> stock = {{"AAPL", 100}, {"MSFT", 50}, {"NVDA", 30}};

    for (const auto& [sym, qty] : stock) {     // pair ke members ko naam mil gaye
        std::cout << "  " << sym << " -> " << qty << "\n";
    }
    // modify karna ho to auto& [k, val], phir val badlo (k const rehta hai map mein)
    for (auto& [sym, qty] : stock) {
        qty += 1;
    }
    std::cout << "  har qty +1 ke baad NVDA = " << stock["NVDA"] << "\n";

    // ============================================================
    //  6. C-array bhi chalta hai -- par sirf jab tak array hai
    // ============================================================
    std::cout << "\n===== 6. C-array + range-for =====\n";
    int arr[] = {10, 20, 30};
    int total = 0;
    for (int x : arr) total += x;         // ✅ size compiler ko pata hai
    std::cout << "  arr ka sum = " << total << "\n";
    std::cout << "  ⚠️ agar array pointer mein decay ho gaya (function param), to\n"
                 "     range-for nahi chalega -- size info kho jaati hai (folder 09)\n";

    return 0;
}

// 04_const_methods.cpp
// ============================================================
// const member functions -- const-correctness, `mutable`, const/non-const overload
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 04_const_methods.cpp -o cm && ./cm
// ============================================================
//   `void f() const` -> "yeh method object ko MODIFY nahi karega".
//   - const method ke andar `this` ek `const T*` ki tarah behave karta hai.
//   - const object pe sirf const methods call ho sakte hain.
//   - `mutable` member const method ke andar bhi badla ja sakta (cache/counter).
// ============================================================

#include <iostream>
#include <string>
#include <vector>

class Report {
    std::vector<int>       data_;
    mutable std::size_t    accessCount_ = 0;    // mutable -- "logical const" cache/stat
    mutable long long      cachedSum_   = 0;
    mutable bool           sumValid_    = false;

public:
    explicit Report(std::vector<int> d) : data_(std::move(d)) {}

    // ---- const method: sirf padhta hai ----
    std::size_t size() const { return data_.size(); }

    // ---- mutable ko const method mein badalna -- allowed ----
    long long sum() const {
        ++accessCount_;                          // mutable -> const method mein bhi OK
        if (!sumValid_) {
            cachedSum_ = 0;
            for (int x : data_) cachedSum_ += x;
            sumValid_ = true;                     // lazy cache
        }
        return cachedSum_;
    }

    std::size_t accessCount() const { return accessCount_; }

    // ---- const / non-const OVERLOAD -- element access ----
    //   non-const object -> modifiable reference milta hai
    //   const object     -> read-only reference
    int&       at(std::size_t i)       { sumValid_ = false; return data_.at(i); }
    const int& at(std::size_t i) const { return data_.at(i); }

    // ---- non-const method: modify karta hai -> `const` NAHI ----
    void push(int v) { data_.push_back(v); sumValid_ = false; }
};

void printSummary(const Report& r) {             // const& -> sirf const methods
    std::cout << "  size=" << r.size() << "  sum=" << r.sum()
              << "  accesses=" << r.accessCount() << "\n";
    // r.push(9);          // ❌ compile ERROR -- push() non-const, r is const&
    // r.at(0) = 100;      // ❌ compile ERROR -- const overload returns const int&
}

int main() {
    Report r{{10, 20, 30}};

    std::cout << "=== const method + mutable cache ===\n";
    printSummary(r);                              // sum computes + caches
    printSummary(r);                              // sum from cache; accessCount badhta gaya

    std::cout << "\n=== non-const object: modify via at() ===\n";
    r.at(1) = 200;                                // non-const overload -> writable, cache invalidate
    r.push(40);
    printSummary(r);                              // recompute: 10+200+30+40 = 280

    std::cout << "\n=== const object ===\n";
    const Report cr{{1, 2, 3}};
    std::cout << "  cr.sum() = " << cr.sum() << "\n";
    // cr.push(4);         // ❌ non-const method on const object
    // cr.at(0) = 9;       // ❌ const overload -> const int&

    std::cout <<
        "\n"
        "  `... const`  -> method object ko modify nahi karega (this = const T*)\n"
        "  `mutable`     -> woh member const method mein bhi modifiable (cache/stats)\n"
        "  const + non-const overload -> const object read-only, non-const writable\n"
        "  const& parameter -> callee sirf const methods use kar sakta\n";
    return 0;
}

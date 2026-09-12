// 02_rule_of_three.cpp
// ============================================================
// Rule of Three -- destructor + copy ctor + copy assign, teenon saath
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 02_rule_of_three.cpp -o r3 && ./r3
// ============================================================
//   Agar class ek raw resource own karti hai aur aap DESTRUCTOR likhte hain:
//   -> copy ctor aur copy assign bhi likhne padenge (deep copy),
//      warna default SHALLOW copy -> 2 objects same pointer own -> double-free.
//
//   Yahan CORRECT version hai. #if 0 wala block "sirf destructor" (broken) dikhata hai --
//   uska comment mein explanation.
// ============================================================

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

namespace { long g_new = 0, g_del = 0; }
void* operator new[](std::size_t n)     { ++g_new; void* p = std::malloc(n ? n : 1); if (!p) throw std::bad_alloc{}; return p; }
void  operator delete[](void* p) noexcept { if (p) { ++g_del; std::free(p); } }
void  operator delete[](void* p, std::size_t) noexcept { if (p) { ++g_del; std::free(p); } }

// ---- CORRECT: Rule of Three ----
class IntArray {
    std::size_t n_    = 0;
    int*        data_ = nullptr;

public:
    IntArray() = default;

    explicit IntArray(std::size_t n) : n_(n), data_(new int[n]) {
        for (std::size_t i = 0; i < n_; ++i) data_[i] = static_cast<int>(i);
    }

    // 1. DESTRUCTOR
    ~IntArray() { delete[] data_; }

    // 2. COPY CONSTRUCTOR -- deep
    IntArray(const IntArray& o) : n_(o.n_), data_(new int[o.n_]) {
        std::memcpy(data_, o.data_, n_ * sizeof(int));
    }

    // 3. COPY ASSIGNMENT -- deep, self-safe, exception-safe-ish (alloc before free)
    IntArray& operator=(const IntArray& o) {
        if (this != &o) {
            int* fresh = new int[o.n_];
            std::memcpy(fresh, o.data_, o.n_ * sizeof(int));
            delete[] data_;
            data_ = fresh;
            n_    = o.n_;
        }
        return *this;
    }

    std::size_t size() const { return n_; }
    int&       operator[](std::size_t i)       { return data_[i]; }
    const int& operator[](std::size_t i) const { return data_[i]; }
};

int main() {
    std::printf("=== correct Rule of Three ===\n");
    {
        IntArray a{5};
        a[2] = 999;

        IntArray b = a;              // copy ctor -> b has its OWN buffer
        b[2] = -1;                   // modify b
        std::printf("  a[2]=%d  b[2]=%d   (independent -- deep copy)\n", a[2], b[2]);

        IntArray c{3};
        c = a;                       // copy assign -> c gets its own copy of a's data
        std::printf("  c.size()=%zu  c[2]=%d\n", c.size(), c[2]);
    }   // ~c, ~b, ~a  -- har ek apna buffer free karta, no double-free

    std::printf("\n[alloc] new[]=%ld delete[]=%ld  outstanding=%ld  %s\n",
                g_new, g_del, g_new - g_del, (g_new == g_del) ? "(clean)" : "(LEAK/BUG)");

    std::printf(
        "\n"
        "  Rule of Three: destructor likha -> copy ctor + copy assign bhi likho.\n"
        "  #if 0 block (neeche) mein 'sirf destructor' version -- default shallow copy\n"
        "  se do objects same pointer own karte -> double-free (crash / heap corruption).\n"
        "  Aur bhi behtar: raw pointer ki jagah std::vector<int> -> Rule of ZERO (folder 17).\n");
    return 0;
}

#if 0
// ---- BROKEN: only a destructor (Rule of Three violated) ----
class BadArray {
    std::size_t n_;
    int*        data_;
public:
    explicit BadArray(std::size_t n) : n_(n), data_(new int[n]) {}
    ~BadArray() { delete[] data_; }
    // NO copy ctor, NO copy assign -> compiler generates SHALLOW ones:
    //   BadArray b = a;   -> b.data_ = a.data_   (same pointer!)
    //   scope end -> ~b deletes data_, ~a deletes data_ AGAIN -> double-free -> UB
};
// int main() { BadArray a{5}; BadArray b = a; }   // crash / "free(): double free detected"
#endif

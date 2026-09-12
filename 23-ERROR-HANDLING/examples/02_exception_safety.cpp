// 02_exception_safety.cpp
// ============================================================
// Exception safety ke 4 levels: no-throw, strong, basic, no-guarantee.
// Ek buggy grow() (leak on throw) → RAII fix (strong guarantee, rollback).
// Plus copy-and-swap assignment jo strong guarantee "muft" deta hai.
//
// Leak ko live-object counter (Flaky::live) se DIKHAYA gaya hai — dawa
// nahi, measurement.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow 02_exception_safety.cpp -o es && ./es
// ============================================================

#include <cstdio>
#include <cstring>
#include <utility>     // std::move, std::swap, std::exchange
#include <stdexcept>
#include <new>         // ::operator new / delete

// Ek type jo "N-vi copy pe" throw karta hai — safety demo ke liye.
// (value pe throw karte to initial fill bhi phat jaati; hume throw
//  SIRF grow ki copy-loop ke beech chahiye.)
struct Flaky {
    int v = 0;
    static inline int live = 0;          // kitne Flaky abhi zinda hain (ctor +1, dtor -1)
    static inline int copies = 0;        // ab tak kitni copy-ctor calls
    static inline int throw_at_copy = -1;// is number wali copy-ctor throw karegi

    Flaky() { ++live; }
    explicit Flaky(int x) : v(x) { ++live; }
    Flaky(const Flaky& o) : v(o.v) {
        if (++copies == throw_at_copy) throw std::runtime_error("Flaky copy blew up");
        ++live;
    }
    Flaky& operator=(const Flaky&) = default;
    ~Flaky() { --live; }
};

// ============================================================
//  1. BASIC GUARANTEE + LEAK — galat tarika
// ============================================================
// grow ke waqt naya buffer + purane elements copy. Agar copy loop beech
// mein throw kare: naya buffer LEAK, aur usme jo elements ban chuke the
// unke dtors bhi nahi chale. Object khud valid rehta hai (basic), par
// resource gaya.
struct BadVec {
    Flaky* data_ = nullptr;
    std::size_t size_ = 0, cap_ = 0;

    ~BadVec() {
        for (std::size_t i = 0; i < size_; ++i) data_[i].~Flaky();
        ::operator delete(data_);
    }

    void push_back_leaky(const Flaky& x) {
        if (size_ == cap_) {
            std::size_t ncap = cap_ ? cap_ * 2 : 4;
            Flaky* nd = static_cast<Flaky*>(::operator new(ncap * sizeof(Flaky)));
            // ⚠️ throw yahan => `nd` leak, aur nd[0..i) ke dtors bhi nahi chale.
            for (std::size_t i = 0; i < size_; ++i) new (nd + i) Flaky(data_[i]);
            for (std::size_t i = 0; i < size_; ++i) data_[i].~Flaky();
            ::operator delete(data_);
            data_ = nd; cap_ = ncap;
        }
        new (data_ + size_) Flaky(x);
        ++size_;
    }
};

// ============================================================
//  2. STRONG GUARANTEE — "commit or rollback"
// ============================================================
// Naya buffer ek RAII holder (RawBuf) mein. Copy loop throw kare to
// RawBuf ka dtor naya buffer + uske banaye elements saaf kar deta hai,
// aur PURANA object bilkul untouched (strong guarantee).
struct GoodVec {
    Flaky* data_ = nullptr;
    std::size_t size_ = 0, cap_ = 0;

    ~GoodVec() { destroy_(data_, size_); }

    static void destroy_(Flaky* d, std::size_t n) {
        for (std::size_t i = 0; i < n; ++i) d[i].~Flaky();
        ::operator delete(d);
    }

    struct RawBuf {                       // RAII: raw storage + kitne construct hue
        Flaky* p = nullptr;
        std::size_t built = 0;
        explicit RawBuf(std::size_t c)
            : p(static_cast<Flaky*>(::operator new(c * sizeof(Flaky)))) {}
        ~RawBuf() {                       // release() nahi hua => sab wapas (rollback)
            if (!p) return;              // <-- committed; kuch nahi karna
            for (std::size_t i = 0; i < built; ++i) p[i].~Flaky();
            ::operator delete(p);
        }
        Flaky* release() { built = 0; return std::exchange(p, nullptr); }
        RawBuf(const RawBuf&) = delete;
        RawBuf& operator=(const RawBuf&) = delete;
    };

    void push_back_strong(const Flaky& x) {
        if (size_ < cap_) {
            new (data_ + size_) Flaky(x);
            ++size_;
            return;
        }
        std::size_t ncap = cap_ ? cap_ * 2 : 4;
        RawBuf nb(ncap);
        for (std::size_t i = 0; i < size_; ++i) {      // <-- throw yahan?
            new (nb.p + i) Flaky(data_[i]);
            ++nb.built;                                 // -> nb ka dtor rollback karega
        }
        new (nb.p + size_) Flaky(x);
        ++nb.built;
        // Yahan pahunch gaye = kuch throw nahi hua. Ab COMMIT (sab noexcept):
        destroy_(data_, size_);
        data_ = nb.release();
        cap_  = ncap;
        ++size_;
    }
};

// ============================================================
//  3. NO-THROW (nothrow guarantee) — swap: sirf pointers/ints exchange
// ============================================================
struct Buffer {
    char* p = nullptr;
    std::size_t n = 0;

    Buffer() = default;
    explicit Buffer(const char* s) : n(std::strlen(s)) {
        p = static_cast<char*>(::operator new(n + 1));
        std::memcpy(p, s, n + 1);
    }
    ~Buffer() { ::operator delete(p); }

    Buffer(const Buffer& o) : n(o.n) {              // alloc => throw kar SAKTA hai
        p = static_cast<char*>(::operator new(n + 1));
        std::memcpy(p, o.p ? o.p : "", n + 1);
    }

    friend void swap(Buffer& a, Buffer& b) noexcept {   // <-- noexcept: nothrow guarantee
        std::swap(a.p, b.p);
        std::swap(a.n, b.n);
    }

    // ---- copy-and-swap assignment: STRONG guarantee, muft ----
    // Parameter BY VALUE => copy pehle (throw yahan hua to *this untouched).
    // Copy ban gayi => noexcept swap => purana data temp ke saath destruct.
    Buffer& operator=(Buffer rhs) noexcept {
        swap(*this, rhs);
        return *this;
    }
};

int main() {
    // --------------------------------------------------------
    //  DEMO 1 — basic guarantee + LEAK (galat)
    //  4 push se cap (4) bhar do; 5th push grow karega; grow ki copy
    //  loop mein v==30 wali copy throw karegi.
    // --------------------------------------------------------
    std::puts("DEMO 1: BadVec — grow ki copy-loop ke beech throw => buffer leak");
    Flaky::live = 0; Flaky::copies = 0;
    Flaky::throw_at_copy = 7;               // 4 initial + grow ki 3rd copy
    {
        BadVec bv;
        bv.push_back_leaky(Flaky{10});
        bv.push_back_leaky(Flaky{20});
        bv.push_back_leaky(Flaky{30});
        bv.push_back_leaky(Flaky{40});     // cap ab 4, bhara hua (copies=4)
        std::printf("   4 push ke baad: size=%zu  Flaky::live=%d\n", bv.size_, Flaky::live);
        try {
            bv.push_back_leaky(Flaky{50}); // grow: copy#5,#6, phir #7 -> throw
        } catch (const std::exception& e) {
            std::printf("   caught: %s\n", e.what());
        }
        std::printf("   fail ke baad: size=%zu  Flaky::live=%d  (4 hona chahiye tha)\n",
                    bv.size_, Flaky::live);
    }
    std::printf("   scope ke baad Flaky::live=%d   <-- 0 nahi! (%d Flaky LEAK — dtor kabhi nahi chala)\n\n",
                Flaky::live, Flaky::live);

    // --------------------------------------------------------
    //  DEMO 2 — strong guarantee (sahi) — wahi scenario, par rollback
    // --------------------------------------------------------
    std::puts("DEMO 2: GoodVec — wahi throw, par RawBuf rollback karta hai");
    Flaky::live = 0; Flaky::copies = 0;
    Flaky::throw_at_copy = 7;
    {
        GoodVec gv;
        gv.push_back_strong(Flaky{10});
        gv.push_back_strong(Flaky{20});
        gv.push_back_strong(Flaky{30});
        gv.push_back_strong(Flaky{40});
        std::printf("   4 push ke baad: size=%zu  Flaky::live=%d\n", gv.size_, Flaky::live);
        try {
            gv.push_back_strong(Flaky{50});
        } catch (const std::exception& e) {
            std::printf("   caught: %s\n", e.what());
        }
        std::printf("   fail ke baad: size=%zu  Flaky::live=%d  (purana state INTACT)\n",
                    gv.size_, Flaky::live);
    }
    std::printf("   scope ke baad Flaky::live=%d   <-- 0 (koi leak nahi)\n\n", Flaky::live);

    // --------------------------------------------------------
    //  DEMO 3 — copy-and-swap assignment (strong, noexcept swap)
    // --------------------------------------------------------
    Flaky::throw_at_copy = -1;
    std::puts("DEMO 3: copy-and-swap assignment");
    Buffer x{"hello"};
    Buffer y{"world-longer-string"};
    x = y;                              // copy y -> swap -> purana "hello" temp ke saath gaya
    std::printf("   x = \"%s\"  (assign ke baad)\n", x.p);
    x = x;                              // self-assign bhi safe (copy pehle banti hai)
    std::printf("   x = \"%s\"  (self-assign ke baad, still fine)\n", x.p);

    std::puts("\nSaar: strong guarantee = 'kaam poora ho ya kuch na badle'.");
    std::puts("Rasta: naya kaam side buffer mein karo, phir NOEXCEPT swap se commit.");
    return 0;
}

// 01_vector.cpp  --  49-PROJECTS advanced P1
// ============================================================
// A growable array with the std::vector core: raw storage + placement new
// (NOT new T[n]), geometric growth, Rule of 5 with copy-and-swap, reserve
// that moves-if-noexcept, contiguous iterators, at() that throws, and a
// strong-guarantee relocate (a throwing element leaves the vector intact).
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -Wconversion -g -O0 01_vector.cpp -o t && ./t
// ============================================================

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <utility>

template <class T>
class Vector {
public:
    Vector() = default;
    explicit Vector(std::size_t n) { reserve(n); for (std::size_t i = 0; i < n; ++i) emplace_back(); }

    Vector(const Vector& o) {
        reserve(o.size_);
        for (std::size_t i = 0; i < o.size_; ++i) ::new (data_ + i) T(o.data_[i]);
        size_ = o.size_;
    }
    Vector(Vector&& o) noexcept
        : data_(std::exchange(o.data_, nullptr)),
          size_(std::exchange(o.size_, 0)),
          cap_(std::exchange(o.cap_, 0)) {}

    Vector& operator=(Vector o) {                    // by value: copy-and-swap covers copy AND move
        swap(o);                                     // (the argument's copy/move may throw at the call site)
        return *this;
    }
    ~Vector() { clear(); std::free(data_); }

    void swap(Vector& o) noexcept {
        std::swap(data_, o.data_);
        std::swap(size_, o.size_);
        std::swap(cap_,  o.cap_);
    }

    // --- capacity ---
    std::size_t size()     const { return size_; }
    std::size_t capacity() const { return cap_; }
    bool        empty()    const { return size_ == 0; }

    void reserve(std::size_t n) {
        if (n <= cap_) return;
        T* nd = static_cast<T*>(std::malloc(n * sizeof(T)));
        if (!nd) throw std::bad_alloc{};
        // move if the element promises not to throw, else copy (strong guarantee)
        std::size_t i = 0;
        try {
            for (; i < size_; ++i) ::new (nd + i) T(std::move_if_noexcept(data_[i]));
        } catch (...) {
            for (std::size_t j = 0; j < i; ++j) nd[j].~T();
            std::free(nd);
            throw;                                  // *this untouched
        }
        for (std::size_t j = 0; j < size_; ++j) data_[j].~T();
        std::free(data_);
        data_ = nd;
        cap_  = n;
    }

    // --- modifiers ---
    template <class... Args>
    T& emplace_back(Args&&... args) {
        if (size_ == cap_) reserve(cap_ ? cap_ * 2 : 4);
        T* p = ::new (data_ + size_) T(std::forward<Args>(args)...);
        ++size_;
        return *p;
    }
    void push_back(const T& v) { emplace_back(v); }
    void push_back(T&& v)      { emplace_back(std::move(v)); }
    void pop_back()            { assert(size_ > 0); data_[--size_].~T(); }
    void clear() { for (std::size_t i = 0; i < size_; ++i) data_[i].~T(); size_ = 0; }

    // --- access ---
    T&       operator[](std::size_t i)       { return data_[i]; }
    const T& operator[](std::size_t i) const { return data_[i]; }
    T&       at(std::size_t i)       { if (i >= size_) throw std::out_of_range("Vector::at"); return data_[i]; }
    const T& at(std::size_t i) const { if (i >= size_) throw std::out_of_range("Vector::at"); return data_[i]; }
    T&       front() { return data_[0]; }
    T&       back()  { return data_[size_ - 1]; }

    // --- contiguous iterators (random-access -> std::sort works) ---
    T*       begin()       { return data_; }
    T*       end()         { return data_ + size_; }
    const T* begin() const { return data_; }
    const T* end()   const { return data_ + size_; }

private:
    T*          data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t cap_  = 0;
};

// A type whose copy ctor throws on the Nth copy -- for the strong-guarantee test.
struct Throwy {
    int v;
    static inline int copies = 0;
    static inline int throw_at = -1;
    Throwy(int x = 0) : v(x) {}
    Throwy(const Throwy& o) : v(o.v) { if (++copies == throw_at) throw std::runtime_error("boom"); }
    Throwy(Throwy&& o) noexcept(false) : v(o.v) {}   // deliberately CAN throw -> reserve() COPIES
    Throwy& operator=(const Throwy&) = default;
    Throwy& operator=(Throwy&&) = default;
};

int main() {
    // basic push / index / grow
    {
        Vector<int> v;
        for (int i = 0; i < 1000; ++i) v.push_back(i);
        assert(v.size() == 1000);
        assert(v.capacity() >= 1000);
        assert(v[0] == 0 && v[999] == 999 && v.back() == 999);
        v.pop_back();
        assert(v.size() == 999 && v.back() == 998);
    }

    // reserve doesn't change size; emplace_back forwards
    {
        Vector<std::pair<int, int>> v;
        v.reserve(64);
        assert(v.capacity() >= 64 && v.size() == 0);
        v.emplace_back(1, 2);
        assert(v.size() == 1 && v[0].first == 1 && v[0].second == 2);
    }

    // copy is independent; move empties the source
    {
        Vector<int> a;
        for (int i = 0; i < 10; ++i) a.push_back(i * i);
        Vector<int> b = a;                           // copy
        b[0] = 999;
        assert(a[0] == 0 && b[0] == 999 && a.size() == b.size());

        Vector<int> c = std::move(a);                // move
        assert(c.size() == 10 && a.size() == 0 && a.capacity() == 0);

        a = c;                                       // copy-assign into a moved-from vector
        assert(a.size() == 10 && a[3] == 9);
    }

    // std::sort works on the iterators (random-access)
    {
        Vector<int> v;
        for (int x : {5, 2, 8, 1, 9, 3, 7}) v.push_back(x);
        std::sort(v.begin(), v.end());
        assert(std::is_sorted(v.begin(), v.end()) && v.front() == 1 && v.back() == 9);
    }

    // at() throws out_of_range
    {
        Vector<int> v; v.push_back(1);
        bool threw = false;
        try { (void)v.at(5); } catch (const std::out_of_range&) { threw = true; }
        assert(threw);
    }

    // strong guarantee: a throwing copy during relocate leaves the vector intact
    {
        Vector<Throwy> v;
        v.reserve(4);
        for (int i = 0; i < 4; ++i) v.emplace_back(i);   // size 4, cap 4
        Throwy::copies = 0;
        Throwy::throw_at = 3;                            // 3rd copy during the next relocate throws
        bool threw = false;
        try { v.reserve(64); } catch (const std::runtime_error&) { threw = true; }
        assert(threw);
        assert(v.size() == 4 && v.capacity() == 4);      // unchanged
        assert(v[0].v == 0 && v[1].v == 1 && v[2].v == 2 && v[3].v == 3);
        Throwy::throw_at = -1;
    }

    std::puts("01_vector: ALL PASS");
    return 0;
}

// ============================================================
// TALKING POINTS
//   - Raw storage (malloc) + placement new: `new T[n]` would default-construct
//     every slot; a vector must only construct what you push (14).
//   - Rule of 5 via copy-and-swap: operator=(Vector o) takes by value, so the
//     compiler picks copy or move for the argument, then we just swap. Self-
//     assignment is automatically safe.
//   - reserve() uses std::move_if_noexcept: if T's move can throw, it COPIES
//     during relocate so a mid-relocate throw leaves the original intact
//     (strong exception guarantee). Throwy's move is deliberately non-noexcept
//     to exercise that path.
//   - Contiguous T* iterators are random-access -> std::sort/std::lower_bound
//     just work. (19, 21)
// ============================================================

# 04 — OOP & design: worked solutions

Poore code un designs ke liye jinme special-member ya ownership subtlety hai.
Baaki `04-oop-design-problems.md` ke `<details>` blocks mein.

---

## A2 — Rule of 3, copy-and-swap

```cpp
#include <cstring>
#include <utility>

class Str {
    char*       p_ = nullptr;
    std::size_t n_ = 0;
public:
    Str() = default;
    Str(const char* s) : n_(std::strlen(s)) {
        p_ = new char[n_ + 1];
        std::memcpy(p_, s, n_ + 1);
    }
    Str(const Str& o) : n_(o.n_) {
        p_ = new char[n_ + 1];
        std::memcpy(p_, o.p_ ? o.p_ : "", n_ + 1);
    }
    friend void swap(Str& a, Str& b) noexcept {
        std::swap(a.p_, b.p_);
        std::swap(a.n_, b.n_);
    }
    Str& operator=(Str o) {           // by value: handles copy AND move; self-assign safe
        swap(*this, o);
        return *this;
    }
    Str(Str&& o) noexcept { swap(*this, o); }   // Rule of 5 freebie
    ~Str() { delete[] p_; }

    const char* c_str() const { return p_ ? p_ : ""; }
    std::size_t size()  const { return n_; }
};
```

`operator=` **by value** → caller ki copy/move elision use hoti; body sirf
`swap`. Self-assign (`s = s`) → argument ek alag copy, swap harmless. Alloc throw
kare → `*this` untouched (strong exception safety). Ek special member likha to
paanch ke baare mein socho.

---

## B1 — LRU cache (`std::list` + `unordered_map`)

```cpp
#include <list>
#include <optional>
#include <unordered_map>

template <class K, class V>
class LruCache {
    using Item = std::pair<K, V>;
    std::list<Item>                                         order_;   // front = MRU
    std::unordered_map<K, typename std::list<Item>::iterator> index_;
    std::size_t cap_;
public:
    explicit LruCache(std::size_t cap) : cap_(cap ? cap : 1) {}

    std::optional<V> get(const K& k) {
        auto it = index_.find(k);
        if (it == index_.end()) return std::nullopt;
        order_.splice(order_.begin(), order_, it->second);   // move node to front, O(1)
        return it->second->second;
    }
    void put(const K& k, V v) {
        if (auto it = index_.find(k); it != index_.end()) {
            it->second->second = std::move(v);
            order_.splice(order_.begin(), order_, it->second);
            return;
        }
        if (order_.size() == cap_) {                          // evict LRU
            index_.erase(order_.back().first);
            order_.pop_back();
        }
        order_.emplace_front(k, std::move(v));
        index_[k] = order_.begin();
    }
};
```

`std::list::splice` node ko move karta **bina iterator invalidate kiye** — yehi
reason `list` chuna, `vector` nahi. Har op `O(1)` (hash + splice). Zero-alloc
variant (`05-stl-solutions.md` C2): nodes ek array + intrusive indices.

---

## B8 — `ScopeGuard`

```cpp
#include <utility>

template <class F>
class ScopeGuard {
    F    f_;
    bool active_ = true;
public:
    explicit ScopeGuard(F f) : f_(std::move(f)) {}
    ScopeGuard(ScopeGuard&& o) noexcept : f_(std::move(o.f_)), active_(o.active_) { o.active_ = false; }
    ScopeGuard(const ScopeGuard&)            = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;
    ~ScopeGuard() { if (active_) f_(); }          // f_ must be noexcept-ish
    void dismiss() noexcept { active_ = false; }
};
template <class F> ScopeGuard(F) -> ScopeGuard<F>;   // CTAD

// use:
//   FILE* fp = std::fopen("x", "r");
//   ScopeGuard close{[&]{ if (fp) std::fclose(fp); }};
//   ... ; guard runs fclose on every exit path
```

Arbitrary cleanup ko RAII banata. `dismiss()` — commit ho gaya, cleanup mat
karo. Move transfers responsibility. `f_()` se throw = `std::terminate` (dtor),
so cleanup lambdas ko non-throwing rakho.

---

## C5 — `Result<T, E>` (expected-lite)

```cpp
#include <utility>
#include <new>

template <class T, class E>
class Result {
    union { T ok_; E err_; };
    bool has_;
public:
    Result(T v)  : ok_(std::move(v)),  has_(true)  {}
    Result(E e, int /*tag*/) : err_(std::move(e)), has_(false) {}
    Result(const Result& o) : has_(o.has_) {
        if (has_) ::new (&ok_) T(o.ok_); else ::new (&err_) E(o.err_);
    }
    ~Result() { if (has_) ok_.~T(); else err_.~E(); }

    bool has_value() const { return has_; }
    const T& value() const { return ok_; }
    const E& error() const { return err_; }
    T value_or(T alt) const { return has_ ? ok_ : alt; }

    template <class F>                              // map: T -> U, error passes through
    auto map(F f) const -> Result<decltype(f(ok_)), E> {
        if (has_) return Result<decltype(f(ok_)), E>(f(ok_));
        return Result<decltype(f(ok_)), E>(err_, 0);
    }
};
```

Tagged union, no exceptions, no heap. Hot-path error handling bina stack-unwind
ke. `E` chhota rakho (enum / error code). C++23 mein `std::expected<T, E>` yehi,
`.and_then` / `.transform` ke saath.

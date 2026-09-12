# 03 — Pointers & memory: worked solutions

Poore code — allocators, smart pointers, linked structures. Baaki
`03-pointers-memory-problems.md` ke `<details>` blocks mein.

---

## B1 — Bump / arena allocator

```cpp
#include <cstddef>
#include <cstdint>
#include <new>

class Arena {
public:
    Arena(void* buffer, std::size_t size)
        : base_(static_cast<std::byte*>(buffer)), cur_(base_), end_(base_ + size) {}

    void* allocate(std::size_t n, std::size_t align = alignof(std::max_align_t)) {
        std::uintptr_t p = reinterpret_cast<std::uintptr_t>(cur_);
        p = (p + align - 1) & ~(align - 1);              // align up (align = pow2)
        std::byte* aligned = reinterpret_cast<std::byte*>(p);
        if (aligned + n > end_) return nullptr;          // out of room
        cur_ = aligned + n;
        return aligned;
    }
    void reset() { cur_ = base_; }                       // free EVERYTHING, O(1)
    std::size_t used() const { return static_cast<std::size_t>(cur_ - base_); }

private:
    std::byte* base_;
    std::byte* cur_;
    std::byte* end_;
};
```

Per-object bookkeeping zero. Individual free nahi — poora `reset()`. HFT:
per-message scratch, per-request arena. Objects trivially-destructible rakho (ya
destructors ki list maintain karo) taaki `reset()` == "sab gaya".

---

## B2 — Fixed-size free-list pool

```cpp
#include <array>
#include <cstdint>
#include <new>
#include <utility>

template <class T, std::size_t N>
class Pool {
    union Slot {
        Slot* next;
        alignas(T) std::byte storage[sizeof(T)];
        Slot() : next(nullptr) {}
        ~Slot() {}
    };
    std::array<Slot, N> slots_{};
    Slot* free_ = nullptr;

public:
    Pool() { for (std::size_t i = 0; i < N; ++i) { slots_[i].next = free_; free_ = &slots_[i]; } }

    template <class... Args>
    T* acquire(Args&&... args) {
        if (!free_) return nullptr;                      // exhausted
        Slot* s = free_;
        free_ = free_->next;
        return ::new (s->storage) T(std::forward<Args>(args)...);   // placement new
    }
    void release(T* p) {
        if (!p) return;
        p->~T();                                        // explicit dtor
        Slot* s = reinterpret_cast<Slot*>(p);
        s->next = free_;
        free_ = s;
    }
};
```

`acquire`/`release` = head pop/push + one construct/destroy. Zero `malloc` after
the pool exists. Exhaustion → `nullptr` (caller decides — never silently
`malloc`). LIFO reuse = the just-freed slot is hottest in cache.

---

## B6 — `unique_ptr` from scratch

```cpp
#include <utility>

template <class T>
class UniquePtr {
    T* p_ = nullptr;
public:
    UniquePtr() noexcept = default;
    explicit UniquePtr(T* p) noexcept : p_(p) {}

    UniquePtr(const UniquePtr&)            = delete;     // move-only
    UniquePtr& operator=(const UniquePtr&) = delete;

    UniquePtr(UniquePtr&& o) noexcept : p_(std::exchange(o.p_, nullptr)) {}
    UniquePtr& operator=(UniquePtr&& o) noexcept {
        if (this != &o) { delete p_; p_ = std::exchange(o.p_, nullptr); }
        return *this;
    }
    ~UniquePtr() { delete p_; }

    T&   operator*()  const { return *p_; }
    T*   operator->() const noexcept { return p_; }
    T*   get()        const noexcept { return p_; }
    explicit operator bool() const noexcept { return p_ != nullptr; }

    T*   release() noexcept { return std::exchange(p_, nullptr); }
    void reset(T* p = nullptr) noexcept { T* old = std::exchange(p_, p); delete old; }
};
```

`std::exchange(o.p_, nullptr)` — steal + null the source in one expression.
Move-assign: self-check, delete old, steal. `reset`: delete **old** after
swapping in the new (`reset(get())` safe). Real `std::unique_ptr` also carries a
deleter with EBO.

---

## C1 — Implement `memmove`

```cpp
#include <cstddef>
void* my_memmove(void* dst, const void* src, std::size_t n) {
    auto* d = static_cast<unsigned char*>(dst);
    auto* s = static_cast<const unsigned char*>(src);
    if (d == s || n == 0) return dst;
    if (d < s) {                                   // no overlap forward, ya dst peeche
        for (std::size_t i = 0; i < n; ++i) d[i] = s[i];
    } else {                                       // dst src ke aage -> copy backward
        for (std::size_t i = n; i-- > 0; ) d[i] = s[i];
    }
    return dst;
}
```

`memcpy` overlapping regions pe UB. `memmove` direction choose karta: agar `dst >
src` aur overlap hai, forward copy `src` ke un bytes ko overwrite kar deti jo
abhi padhne baaki the → backward copy. Real `memmove` word-at-a-time + alignment
handling; yeh byte version correct hai, bas slow.

---

## B10 — Detect cycle + find start (Floyd)

```cpp
struct Node { int val; Node* next; };

Node* cycle_start(Node* head) {
    Node* slow = head;
    Node* fast = head;
    while (fast && fast->next) {
        slow = slow->next;
        fast = fast->next->next;
        if (slow == fast) {                 // meeting point
            Node* a = head;
            while (a != slow) { a = a->next; slow = slow->next; }
            return a;                        // cycle start
        }
    }
    return nullptr;                          // no cycle
}
```

Meeting ke baad: head se `a`, meeting-point se `slow`, dono `+1` — jahan milte
hain wahi cycle ka start. `O(n)` time, `O(1)` space. Proof: meeting pe fast ne
slow se ek extra full loop cover kiya → distances ka modular equation.

# 08 — Lock-free: worked solutions

Poore code — spinlock, Treiber stack, SPSC ring, seqlock. Har atomic op ke saath
memory-order justification. `-O2 -pthread`; TSan (Linux/WSL) ke neeche verify.

---

## 1 — `atomic_flag` TTAS spinlock

```cpp
#include <atomic>
#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
static inline void cpu_pause() { _mm_pause(); }
#else
static inline void cpu_pause() {}
#endif

class SpinLock {
    std::atomic_flag f_ = ATOMIC_FLAG_INIT;
public:
    void lock() noexcept {
        while (f_.test_and_set(std::memory_order_acquire)) {
            while (f_.test(std::memory_order_relaxed))     // C++20: spin on a plain load
                cpu_pause();                               // don't hammer the bus with RMW
        }
    }
    bool try_lock() noexcept { return !f_.test_and_set(std::memory_order_acquire); }
    void unlock() noexcept { f_.clear(std::memory_order_release); }
};
```

**acquire** on `lock` / **release** on `unlock` = critical-section reads/writes
can't leak out either end (happens-before). Inner `test` loop (TTAS) avoids the
expensive `LOCK`-prefixed RMW while the lock is held by someone else. No
fairness — a thread can starve. HFT: often a pinned-core spinlock is exactly
what you want (no syscall).

---

## 3 + 4 — Treiber stack + ABA fix (tagged head)

```cpp
#include <atomic>
#include <cstdint>

template <class T>
class TreiberStack {
    struct Node { T value; Node* next; };
    struct Tagged { Node* ptr; std::uintptr_t tag; };
    std::atomic<Tagged> head_{Tagged{nullptr, 0}};      // needs 16-byte CAS (cmpxchg16b)
public:
    static_assert(std::atomic<Tagged>::is_always_lock_free
                  || true, "may fall back to a lock on some targets");

    void push(T v) {
        Node* n = new Node{std::move(v), nullptr};
        Tagged old = head_.load(std::memory_order_relaxed);
        do {
            n->next = old.ptr;
        } while (!head_.compare_exchange_weak(
                     old, Tagged{n, old.tag + 1},
                     std::memory_order_release, std::memory_order_relaxed));
    }
    bool pop(T& out) {
        Tagged old = head_.load(std::memory_order_acquire);
        while (old.ptr) {
            Tagged next{old.ptr->next, old.tag + 1};
            if (head_.compare_exchange_weak(old, next,
                    std::memory_order_acquire, std::memory_order_relaxed)) {
                out = std::move(old.ptr->value);
                delete old.ptr;                          // safe ONLY if no ABA + no other reader
                return true;
            }
        }
        return false;
    }
};
```

**ABA:** without the `tag`, a thread that read `head == A`, stalled, then woke to
find `head == A` again (after A was popped, freed, and a new node reused A's
address) would `CAS` successfully against a stale `next`. The `tag++` on every
successful CAS makes the `Tagged` value differ → stale CAS fails. `compare_
exchange_weak` in a loop (spurious failure is fine, we retry). Real reclamation
still needs hazard pointers / epochs (`delete` above is only safe under extra
assumptions).

---

## 5 — SPSC ring buffer

```cpp
#include <atomic>
#include <cstddef>
#include <new>

template <class T, std::size_t Capacity>
class SpscRing {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");
    static constexpr std::size_t kMask = Capacity - 1;

    alignas(64) std::atomic<std::size_t> head_{0};   // consumer writes
    std::size_t                          head_cache_{0};
    alignas(64) std::atomic<std::size_t> tail_{0};   // producer writes
    std::size_t                          tail_cache_{0};
    alignas(64) T buf_[Capacity]{};
public:
    bool try_push(const T& v) {
        const std::size_t t = tail_.load(std::memory_order_relaxed);
        if (t - head_cache_ == Capacity) {                       // maybe full — refresh
            head_cache_ = head_.load(std::memory_order_acquire);
            if (t - head_cache_ == Capacity) return false;       // really full
        }
        buf_[t & kMask] = v;
        tail_.store(t + 1, std::memory_order_release);           // publish the write
        return true;
    }
    bool try_pop(T& out) {
        const std::size_t h = head_.load(std::memory_order_relaxed);
        if (h == tail_cache_) {                                  // maybe empty — refresh
            tail_cache_ = tail_.load(std::memory_order_acquire);
            if (h == tail_cache_) return false;                  // really empty
        }
        out = buf_[h & kMask];
        head_.store(h + 1, std::memory_order_release);
        return true;
    }
};
```

**No CAS** — each index has exactly one writer, so a plain `load`/`store` with
`acquire`/`release` is enough. `release` store on `tail_` pairs with `acquire`
load on the consumer side → the `buf_[...] = v` write happens-before the read.
`alignas(64)` on `head_`/`tail_` + caching the opposite index means the common
path touches only lines the current thread owns (no coherence traffic). Full
tested version: `46-INTERVIEW-PREP/examples/03_spsc_ring_buffer.cpp`.

---

## 7 — Seqlock (1 writer, N readers)

```cpp
#include <atomic>
#include <cstdint>

template <class T>   // T: small, trivially copyable
class SeqLock {
    std::atomic<std::uint64_t> seq_{0};
    T                          data_{};
public:
    void store(const T& v) {                       // single writer only
        const std::uint64_t s = seq_.load(std::memory_order_relaxed);
        seq_.store(s + 1, std::memory_order_relaxed);          // odd -> write in progress
        std::atomic_thread_fence(std::memory_order_release);
        data_ = v;
        std::atomic_thread_fence(std::memory_order_release);
        seq_.store(s + 2, std::memory_order_release);          // even -> done
    }
    T load() const {
        T out;
        std::uint64_t s1, s2;
        do {
            s1 = seq_.load(std::memory_order_acquire);
            if (s1 & 1) continue;                              // writer active, retry
            out = data_;
            std::atomic_thread_fence(std::memory_order_acquire);
            s2 = seq_.load(std::memory_order_relaxed);
        } while (s1 != s2);                                    // changed under us -> retry
        return out;
    }
};
```

Readers **never block the writer** and never take a lock — they just retry on the
rare collision. Ideal for a hot market-data snapshot (`{price, qty}`) read by
many strategy threads. Strictly there's a data race on `data_` under the C++
memory model; production code uses per-field atomics or careful `memcpy` +
fences (folder 28). x86: the fences are nearly free.

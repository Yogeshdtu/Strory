// 02_cas_loop.cpp
// ============================================================
// compare_exchange (CAS) — lock-free ka core primitive. CAS-loop
// pattern: read -> compute -> try-swap -> retry-if-changed. Iske se
// aap koi bhi read-modify-write atomically kar sakte ho, bhale
// hardware me uska direct instruction na ho (e.g. atomic max, atomic
// multiply, atomic "update a struct field").
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 02_cas_loop.cpp -o cas -pthread && ./cas
// ============================================================

#include <cstdio>
#include <atomic>
#include <thread>
#include <vector>
#include <cstdint>

// ============================================================
//  Pattern: atomic "apply an arbitrary function" via a CAS loop
// ============================================================
template <class T, class Fn>
static T atomic_update(std::atomic<T>& a, Fn fn) {
    T old = a.load(std::memory_order_relaxed);
    T next;
    do {
        next = fn(old);
        // compare_exchange_weak: agar a == old, to a = next (return true).
        // agar a != old (kisi aur ne badla), old ko current value se
        // update karke return false -> loop retry.
    } while (!a.compare_exchange_weak(old, next,
                                     std::memory_order_release,   // success
                                     std::memory_order_relaxed)); // failure
    return next;
}

int main() {
    constexpr int  kThreads = 8;
    constexpr long kIters   = 500'000;

    // --------------------------------------------------------
    //  1. Atomic increment via CAS loop (fetch_add ki jagah — demo)
    // --------------------------------------------------------
    {
        std::atomic<long> counter{0};
        std::vector<std::thread> ts;
        for (int i = 0; i < kThreads; ++i)
            ts.emplace_back([&] {
                for (long k = 0; k < kIters; ++k)
                    atomic_update(counter, [](long v) { return v + 1; });
            });
        for (auto& t : ts) t.join();
        long expected = static_cast<long>(kThreads) * kIters;
        std::printf("1. CAS-loop increment : %ld  %s\n",
                    counter.load(), counter.load() == expected ? "OK" : "WRONG");
    }

    // --------------------------------------------------------
    //  2. Atomic MAX (hardware me nahi hai -> CAS loop se)
    // --------------------------------------------------------
    {
        std::atomic<long> hi{0};
        std::vector<std::thread> ts;
        for (int t = 0; t < kThreads; ++t)
            ts.emplace_back([&, t] {
                for (long k = 0; k < kIters; ++k) {
                    long candidate = (t * 7919L + k * 31L) % 100000;
                    // atomic max: sirf tab store karo jab candidate bada ho
                    long cur = hi.load(std::memory_order_relaxed);
                    while (candidate > cur &&
                           !hi.compare_exchange_weak(cur, candidate,
                                                     std::memory_order_relaxed)) {
                        // cur ab current value se refresh ho gaya; loop re-check
                    }
                }
            });
        for (auto& t : ts) t.join();
        std::printf("2. CAS-loop atomic max : %ld  (should be 99999)\n", hi.load());
    }

    // --------------------------------------------------------
    //  3. weak vs strong — spurious failure
    // --------------------------------------------------------
    // compare_exchange_WEAK: LL/SC style archs (ARM) pe "spuriously" fail
    //   kar sakta hai bhale a == expected ho. Isliye HAMESHA loop mein.
    //   x86 pe spurious failure nahi hota, par portable code loop maanta.
    // compare_exchange_STRONG: kabhi spurious fail nahi. Loop ke BAHAR
    //   single try ke liye use karo (e.g. "ek baar try, warna give up").
    {
        std::atomic<int> v{10};
        int expected = 10;
        bool ok = v.compare_exchange_strong(expected, 20);   // single try, no loop
        std::printf("3. strong single-try  : ok=%d  v=%d  (expected after: 20)\n", ok, v.load());

        expected = 999;                                       // wrong expected
        ok = v.compare_exchange_strong(expected, 30);
        std::printf("   wrong expected      : ok=%d  expected updated to current = %d\n",
                    ok, expected);
    }

    // --------------------------------------------------------
    //  4. Lock via CAS (a spinlock) — dikhane ke liye ki mutex bhi
    //     internally aisa kuch karta hai (fast path)
    // --------------------------------------------------------
    {
        std::atomic<bool> locked{false};
        auto lock = [&] {
            bool expected = false;
            while (!locked.compare_exchange_weak(expected, true,
                                                 std::memory_order_acquire,
                                                 std::memory_order_relaxed)) {
                expected = false;                 // CAS ne expected ko `true` set kiya, reset
                // (real spinlock: yahan _mm_pause() / cpu_relax)
            }
        };
        auto unlock = [&] { locked.store(false, std::memory_order_release); };

        long shared = 0;
        std::vector<std::thread> ts;
        for (int i = 0; i < 4; ++i)
            ts.emplace_back([&] {
                for (long k = 0; k < 100000; ++k) { lock(); ++shared; unlock(); }
            });
        for (auto& t : ts) t.join();
        std::printf("4. CAS spinlock        : shared=%ld  (should be 400000)\n", shared);
    }

    std::puts("\nSaar:");
    std::puts(" - CAS = compare_exchange: 'agar value abhi bhi X hai to Y kar do, atomically'.");
    std::puts(" - CAS-loop: read -> compute -> try-CAS -> agar fail (koi aur ne badla) retry.");
    std::puts("   Isse koi bhi RMW atomically ho jaata (max, multiply, struct-field update).");
    std::puts(" - WEAK: spurious fail ho sakta (ARM LL/SC) -> hamesha loop mein.");
    std::puts("   STRONG: no spurious fail -> loop ke bahar single-try ke liye.");
    std::puts(" - Fail hone pe `expected` current value se UPDATE ho jaata (free re-read).");
    std::puts(" - High contention pe CAS-loop 'livelock-ish' — bahut retries. fetch_add");
    std::puts("   (jab available ho) CAS-loop se behtar (ek hi hardware op).");
    return 0;
}

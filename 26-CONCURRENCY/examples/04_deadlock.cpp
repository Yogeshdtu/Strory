// 04_deadlock.cpp
// ============================================================
// ⚠️ DEADLOCK: do threads, do mutex, ULTE order mein lock.
// Thread 1: lock A -> lock B.   Thread 2: lock B -> lock A.
// Dono ek-ek lock le lete, phir doosre ka wait karte -> hamesha ke liye.
//
// Yahan hum std::timed_mutex + try_lock_for use karte hain taaki program
// HANG na kare — deadlock DETECT karke back off karte, phir 2 FIXES dikhate.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -pthread 04_deadlock.cpp -o dl && ./dl
// ============================================================

#include <cstdio>
#include <thread>
#include <mutex>
#include <chrono>
#include <atomic>

using namespace std::chrono_literals;

// ============================================================
//  1. THE DEADLOCK-PRONE PATTERN (safely demonstrated with timeouts)
// ============================================================
static std::timed_mutex mA, mB;
static std::atomic<int> g_deadlock_hits{0};

static void worker_AB() {                       // lock A then B
    for (int i = 0; i < 5; ++i) {
        std::unique_lock<std::timed_mutex> la(mA);
        std::this_thread::sleep_for(1ms);       // window for the other thread to grab mB
        if (!mB.try_lock_for(50ms)) {           // ⚠️ would block forever without timeout
            ++g_deadlock_hits;
            la.unlock();                        // back off, retry
            std::this_thread::sleep_for(1ms);
            continue;
        }
        std::unique_lock<std::timed_mutex> lb(mB, std::adopt_lock);
        // ... critical section touching A and B ...
    }
}
static void worker_BA() {                       // lock B then A  (opposite order!)
    for (int i = 0; i < 5; ++i) {
        std::unique_lock<std::timed_mutex> lb(mB);
        std::this_thread::sleep_for(1ms);
        if (!mA.try_lock_for(50ms)) {
            ++g_deadlock_hits;
            lb.unlock();
            std::this_thread::sleep_for(1ms);
            continue;
        }
        std::unique_lock<std::timed_mutex> la(mA, std::adopt_lock);
    }
}

// ============================================================
//  2. FIX A — consistent lock ORDER (always A before B)
// ============================================================
static std::mutex fx1, fx2;
static long g_shared1 = 0, g_shared2 = 0;

static void ordered_worker() {
    for (int i = 0; i < 100000; ++i) {
        std::lock_guard<std::mutex> l1(fx1);    // EVERYONE locks fx1 first...
        std::lock_guard<std::mutex> l2(fx2);    // ...then fx2. No cycle possible.
        ++g_shared1; ++g_shared2;
    }
}

// ============================================================
//  3. FIX B — std::scoped_lock (locks all at once, deadlock-free algo)
// ============================================================
static void scoped_worker() {
    for (int i = 0; i < 100000; ++i) {
        std::scoped_lock lk(fx1, fx2);          // order doesn't matter — uses a
        --g_shared1; --g_shared2;               // deadlock-avoidance locking algorithm
    }
}

int main() {
    // --------------------------------------------------------
    //  1. deadlock-prone pattern (detected via timeouts)
    // --------------------------------------------------------
    std::puts("1. opposite lock order (would deadlock — timeouts save us):");
    {
        std::thread t1(worker_AB), t2(worker_BA);
        t1.join(); t2.join();
        std::printf("   near-deadlocks detected & backed off: %d times\n",
                    g_deadlock_hits.load());
        std::puts("   (bina timeout ke: dono threads hamesha ke liye block)");
    }

    // --------------------------------------------------------
    //  2. FIX A — consistent order
    // --------------------------------------------------------
    std::puts("\n2. FIX A — consistent lock order (fx1 always before fx2):");
    {
        std::thread a(ordered_worker), b(ordered_worker);
        a.join(); b.join();
        std::printf("   g_shared1=%ld g_shared2=%ld  (no deadlock, correct)\n",
                    g_shared1, g_shared2);
    }

    // --------------------------------------------------------
    //  3. FIX B — scoped_lock
    // --------------------------------------------------------
    std::puts("\n3. FIX B — std::scoped_lock(fx1, fx2) (locks both atomically):");
    {
        std::thread a(scoped_worker), b(scoped_worker);
        a.join(); b.join();
        std::printf("   g_shared1=%ld g_shared2=%ld  (back to 0, correct)\n",
                    g_shared1, g_shared2);
    }

    std::puts("\nDeadlock ke 4 conditions (Coffman) — sab chahiye:");
    std::puts("  1. mutual exclusion   2. hold-and-wait");
    std::puts("  3. no preemption      4. circular wait");
    std::puts("Todo koi ek:");
    std::puts("  - circular wait -> CONSISTENT LOCK ORDER (fix A) — sabse common");
    std::puts("  - hold-and-wait -> std::scoped_lock / std::lock (fix B) — sab ek saath lo");
    std::puts("  - lock hierarchy, try_lock + backoff, ya lock-free (folder 28)");
    return 0;
}

// 05_deadlock_debug.cpp
// ============================================================
// ⚠️  DEADLOCK demo. Do threads, do mutexes, ULTA lock order.
//     DEFAULT run SAFE hai (ordered locking -> complete ho jaata).
//     `--deadlock` do to woh classic AB/BA deadlock trigger karega
//     aur program HANG karega -- tab dusre terminal se gdb attach karo.
//     Compile CLEAN. Yeh lesson 08 (multithreaded gdb) ka lab hai.
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -g -O0 -pthread 05_deadlock_debug.cpp -o dl
//   ./dl                    # "done: a=... b=..."   (safe, ordered)
//   ./dl --deadlock &       # HANGS
//   gdb -p $!               # attach
//     (gdb) thread apply all bt
//     (gdb) info threads
//   # ya Linux pe:  cat /proc/$!/task/*/stack   (kernel side)
// ============================================================
// Deadlock ko gdb se inspect karna: dono threads `__lll_lock_wait` /
// `pthread_mutex_lock` mein atke dikhenge, `bt` se pata chalega
// kaun kaunsa mutex hold kiye hue kispe wait kar raha -> lock-order
// cycle. Expected gdb output + FIX file ke neeche.
// ============================================================

#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>

static std::mutex mtx_a;
static std::mutex mtx_b;
static long shared_a = 0;
static long shared_b = 0;

// SAFE: dono threads mutexes ko HAMESHA usi order mein lete (a phir b).
// Isliye koi cycle nahi ban sakta.
static void safe_t1() {
    for (int i = 0; i < 100000; ++i) {
        std::scoped_lock lk(mtx_a, mtx_b);   // deadlock-free: ek saath dono, ordered
        ++shared_a; ++shared_b;
    }
}
static void safe_t2() {
    for (int i = 0; i < 100000; ++i) {
        std::scoped_lock lk(mtx_a, mtx_b);
        --shared_a; --shared_b;
    }
}

// BUG PATH: t1 leta a->b, t2 leta b->a. Beech mein ek chhota sleep
// taaki interleaving reliably deadlock kare.
static void bad_t1() {
    std::lock_guard<std::mutex> la(mtx_a);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::lock_guard<std::mutex> lb(mtx_b);     // t2 ne b hold kiya hai -> wait forever
    ++shared_a; ++shared_b;
}
static void bad_t2() {
    std::lock_guard<std::mutex> lb(mtx_b);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::lock_guard<std::mutex> la(mtx_a);     // t1 ne a hold kiya hai -> wait forever
    ++shared_a; ++shared_b;
}

int main(int argc, char** argv) {
    const bool deadlock = (argc > 1 && std::strcmp(argv[1], "--deadlock") == 0);

    if (deadlock) {
        std::printf("starting DEADLOCK path -- program will hang.\n");
        std::printf("attach: gdb -p %ld   then: thread apply all bt\n",
                    static_cast<long>(
#if defined(_WIN32)
                        0
#else
                        getpid()
#endif
                        ));
        std::fflush(stdout);
        std::thread t1(bad_t1);
        std::thread t2(bad_t2);
        t1.join();      // kabhi return nahi hoga
        t2.join();
    } else {
        std::thread t1(safe_t1);
        std::thread t2(safe_t2);
        t1.join();
        t2.join();
        std::printf("done (safe, ordered locking): a=%ld b=%ld\n", shared_a, shared_b);
    }
    return 0;
}

// ============================================================
//                 G D B   D E A D L O C K   I N S P E C T I O N
// ============================================================
//
// $ ./dl --deadlock &
// $ gdb -p <pid>
// (gdb) info threads
//   Id   Target Id          Frame
// * 1    Thread ... (LWP..)  0x... in __futex_abstimed_wait ...
//   2    Thread ... (LWP..)  0x... in ___lll_lock_wait () ...
//   3    Thread ... (LWP..)  0x... in ___lll_lock_wait () ...
//
// (gdb) thread apply all bt
//
// Thread 3 (bad_t2):
//   #0  ___lll_lock_wait ()
//   #1  pthread_mutex_lock ()
//   #2  std::mutex::lock ()
//   #3  bad_t2 ()            05_deadlock_debug.cpp:60   <-- lock mtx_a ke liye wait
//   ...
//   (locals/source se: mtx_b ALREADY held by this thread, line 58)
//
// Thread 2 (bad_t1):
//   #0  ___lll_lock_wait ()
//   #3  bad_t1 ()            05_deadlock_debug.cpp:54   <-- lock mtx_b ke liye wait
//   (mtx_a ALREADY held by this thread, line 52)
//
// DIAGNOSIS -- lock-order cycle:
//   Thread 2 holds A, wants B.
//   Thread 3 holds B, wants A.
//   Neither can proceed -> deadlock (26-CONCURRENCY/09).
//   gdb mein pehchano: >=2 threads `__lll_lock_wait`/`pthread_mutex_lock`
//   mein, aur unke bt ek-doosre ke held mutexes pe wait karte.
//
// FIX (koi ek):
//   1. GLOBAL LOCK ORDER -- har jagah mutexes ko usi order mein lo
//      (address order, ya named order a-before-b). `bad_t2` bhi a->b le.
//   2. std::scoped_lock(mtx_a, mtx_b) -- ek call mein dono, yeh
//      deadlock-avoidance algorithm use karta (try+back-off). `safe_*`
//      yehi karta.
//   3. std::lock(mtx_a, mtx_b) phir dono ko adopt karo.
//   4. Lock hi mat share karo -- ek coarse mutex, ya lock-free (28).
//
// DETECTION TOOLS:
//   - valgrind --tool=helgrind ./dl --deadlock   -> "lock order violated"
//     (07-valgrind) -- yeh potential deadlock ko bina hang hue pakadta.
//   - TSan -fsanitize=thread bhi lock-order-inversion warn karta.
//   - -D_GLIBCXX_DEBUG + std::scoped_lock hamesha use karo.
// ============================================================

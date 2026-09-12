// 05_reordering_demo.cpp
// ============================================================
// STORE BUFFERING — reordering ko apni aankhon se dekho.
//
//   Thread 1:  x = 1;  r1 = y;      Thread 2:  y = 1;  r2 = x;
//
// "Intuitively" r1==0 && r2==0 NAMUMKIN lagta (dono stores pehle
// honi chahiye). PAR x86 pe har CPU ka ek STORE BUFFER hota hai:
// `x = 1` buffer mein jaata, `r1 = y` seedha memory se padhta —
// isliye T1 ko `x=1` ka effect nahi dikhta jab tak flush na ho.
// Nateeja: KABHI-KABHI r1==0 && r2==0 (dono stores buffer mein reh
// gayi jab loads hui).
//
//   relaxed / release-acquire  -> yeh reordering ALLOWED -> dikhega
//   seq_cst (ya ek fence)      -> disallowed -> KABHI nahi dikhega
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 05_reordering_demo.cpp -o ro -pthread && ./ro
//   (-O2 pe reordering zyada visible; -O0 pe bhi kabhi-kabhi)
// ============================================================

#include <cstdio>
#include <atomic>
#include <thread>

template <std::memory_order Store, std::memory_order Load>
static long run(const char* label, long rounds) {
    std::atomic<int> x{0}, y{0};
    std::atomic<int> go{0}, done{0};
    int r1 = 0, r2 = 0;
    long both_zero = 0;

    std::thread t1([&] {
        for (long i = 0; i < rounds; ++i) {
            while (go.load(std::memory_order_acquire) != i + 1) { }
            x.store(1, Store);
            r1 = y.load(Load);
            done.fetch_add(1, std::memory_order_release);
        }
    });
    std::thread t2([&] {
        for (long i = 0; i < rounds; ++i) {
            while (go.load(std::memory_order_acquire) != i + 1) { }
            y.store(1, Store);
            r2 = x.load(Load);
            done.fetch_add(1, std::memory_order_release);
        }
    });

    for (long i = 0; i < rounds; ++i) {
        x.store(0, std::memory_order_relaxed);
        y.store(0, std::memory_order_relaxed);
        done.store(0, std::memory_order_relaxed);
        go.store(static_cast<int>(i + 1), std::memory_order_release);   // release both threads
        while (done.load(std::memory_order_acquire) != 2) { }
        if (r1 == 0 && r2 == 0) ++both_zero;
    }
    t1.join(); t2.join();

    std::printf("  %-28s r1==0 && r2==0 : %6ld / %ld rounds\n", label, both_zero, rounds);
    return both_zero;
}

int main() {
    constexpr long kRounds = 200000;

    std::puts("Store buffering litmus test (x86):\n");

    long relaxed = run<std::memory_order_relaxed, std::memory_order_relaxed>(
        "relaxed store + relaxed load", kRounds);

    long relacq = run<std::memory_order_release, std::memory_order_acquire>(
        "release store + acquire load", kRounds);

    long seqcst = run<std::memory_order_seq_cst, std::memory_order_seq_cst>(
        "seq_cst store + seq_cst load", kRounds);

    std::puts("");
    std::printf("relaxed      : %s\n", relaxed ? "REORDERING OBSERVED (r1==r2==0 happened)"
                                               : "not seen this run (try more rounds / rebuild)");
    std::printf("release/acq  : %s\n", relacq  ? "REORDERING OBSERVED (rel/acq does NOT stop SB!)"
                                               : "not seen this run");
    std::printf("seq_cst      : %s\n", seqcst  ? "!! unexpected — should be 0"
                                               : "0 — as guaranteed (total order forbids r1==r2==0)");

    std::puts("\nSaar:");
    std::puts(" - x86 'strong' hai par TSO: STORE -> LOAD reorder allowed (store buffer).");
    std::puts("   Yeh classic 'store buffering' hai — dono threads apni store ko");
    std::puts("   buffer mein chhod ke doosre ki (purani) value padh lete.");
    std::puts(" - release/acquire is reordering ko NAHI rokta (woh sirf ek-taraf ka");
    std::puts("   ordering deta: release ke pehle ka kaam, acquire ke baad visible).");
    std::puts(" - seq_cst (ya `atomic_thread_fence(seq_cst)` dono stores/loads ke beech)");
    std::puts("   ek TOTAL ORDER enforce karta -> r1==r2==0 namumkin. Isliye Dekker/");
    std::puts("   Peterson jaise algorithms ko seq_cst (ya fences) chahiye.");
    std::puts(" - Lesson: 'x86 strong hai' != 'sab kuch ordered hai'. SB gap real hai.");
    return 0;
}

// 08_litmus_tests.cpp
// ============================================================
// Classic memory-model litmus tests, run as programs (persistent
// worker threads + a per-round handshake, so we can do many rounds).
//   MP  (message passing)   : data + flag. rel/acq is enough.
//   SB  (store buffering)    : see 05_reordering_demo.cpp (needs seq_cst).
//   IRIW (independent reads  : two readers can disagree on the order of
//         of independent       two independent writes -> needs seq_cst.
//         writes)
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 08_litmus_tests.cpp -o lit -pthread && ./lit
// ============================================================

#include <cstdio>
#include <atomic>
#include <thread>

// ============================================================
//  MESSAGE PASSING (MP)
//    T1:  data = 42 ;  flag.store(1, REL)
//    T2:  while(!flag.load(ACQ)) ;  r = data
//  Correct: r == 42 always (rel/acq synchronizes-with).
//  With RELAXED flag: r could be 0 (stale data) on weak archs.
// ============================================================
template <std::memory_order StoreMO, std::memory_order LoadMO>
static long mp_test(long rounds) {
    std::atomic<int> data{0};      // atomic so the plain read in the checker isn't itself a race
    std::atomic<int> flag{0};
    std::atomic<long> gen{0};      // round handshake
    std::atomic<long> ackP{0}, ackC{0};
    std::atomic<long> bad{0};
    std::atomic<bool> stop{false};

    std::thread producer([&] {
        for (long r = 1; ; ++r) {
            while (gen.load(std::memory_order_acquire) != r) { if (stop.load()) return; }
            data.store(42, std::memory_order_relaxed);
            flag.store(1, StoreMO);
            ackP.store(r, std::memory_order_release);
        }
    });
    std::thread consumer([&] {
        for (long r = 1; ; ++r) {
            while (gen.load(std::memory_order_acquire) != r) { if (stop.load()) return; }
            while (flag.load(LoadMO) == 0) { }
            if (data.load(std::memory_order_relaxed) != 42) bad.fetch_add(1, std::memory_order_relaxed);
            ackC.store(r, std::memory_order_release);
        }
    });

    for (long r = 1; r <= rounds; ++r) {
        data.store(0, std::memory_order_relaxed);
        flag.store(0, std::memory_order_relaxed);
        gen.store(r, std::memory_order_release);
        while (ackP.load(std::memory_order_acquire) != r ||
               ackC.load(std::memory_order_acquire) != r) { }
    }
    stop.store(true);
    gen.fetch_add(1, std::memory_order_release);   // unblock any waiting worker
    producer.join(); consumer.join();
    return bad.load();
}

// ============================================================
//  IRIW — independent reads of independent writes
//    W1: x = 1                 RA: r1 = x ; r2 = y
//    W2: y = 1                 RB: r3 = y ; r4 = x
//  Non-seq_cst: RA can see (x=1,y=0) while RB sees (y=1,x=0)
//  -> the two writes appear in OPPOSITE orders. seq_cst forbids it.
// ============================================================
template <std::memory_order StoreMO, std::memory_order LoadMO>
static long iriw_test(long rounds) {
    std::atomic<int> x{0}, y{0};
    std::atomic<int> r1{0}, r2{0}, r3{0}, r4{0};
    std::atomic<long> gen{0};
    std::atomic<long> ack{0};                 // 4 workers each bump; round done at 4*r
    std::atomic<long> disagreements{0};
    std::atomic<bool> stop{false};

    auto worker = [&](int which) {
        for (long r = 1; ; ++r) {
            while (gen.load(std::memory_order_acquire) != r) { if (stop.load()) return; }
            switch (which) {
                case 0: x.store(1, StoreMO); break;
                case 1: y.store(1, StoreMO); break;
                case 2: r1.store(x.load(LoadMO), std::memory_order_relaxed);
                        r2.store(y.load(LoadMO), std::memory_order_relaxed); break;
                case 3: r3.store(y.load(LoadMO), std::memory_order_relaxed);
                        r4.store(x.load(LoadMO), std::memory_order_relaxed); break;
            }
            ack.fetch_add(1, std::memory_order_release);
        }
    };
    std::thread w0(worker, 0), w1(worker, 1), w2(worker, 2), w3(worker, 3);

    for (long r = 1; r <= rounds; ++r) {
        x.store(0, std::memory_order_relaxed); y.store(0, std::memory_order_relaxed);
        r1.store(0, std::memory_order_relaxed); r2.store(0, std::memory_order_relaxed);
        r3.store(0, std::memory_order_relaxed); r4.store(0, std::memory_order_relaxed);
        gen.store(r, std::memory_order_release);
        while (ack.load(std::memory_order_acquire) != 4 * r) { }
        if (r1.load() == 1 && r2.load() == 0 && r3.load() == 1 && r4.load() == 0)
            disagreements.fetch_add(1, std::memory_order_relaxed);
    }
    stop.store(true);
    gen.fetch_add(1, std::memory_order_release);
    w0.join(); w1.join(); w2.join(); w3.join();
    return disagreements.load();
}

int main() {
    constexpr long kRounds = 1'000'000;

    std::puts("=== Message Passing (MP) ===");
    std::printf("  relaxed flag : %ld bad reads / %ld  (x86: usually 0; weak arch: >0 => broken)\n",
                mp_test<std::memory_order_relaxed, std::memory_order_relaxed>(kRounds), kRounds);
    std::printf("  rel / acq    : %ld bad reads / %ld  (0 => GUARANTEED everywhere)\n",
                mp_test<std::memory_order_release, std::memory_order_acquire>(kRounds), kRounds);
    std::printf("  seq_cst      : %ld bad reads / %ld  (0 => also fine, but pricier)\n",
                mp_test<std::memory_order_seq_cst, std::memory_order_seq_cst>(kRounds), kRounds);

    std::puts("\n=== Store Buffering (SB) ===");
    std::puts("  see 05_reordering_demo.cpp: relaxed & rel/acq allow r1==r2==0 on x86;");
    std::puts("  only seq_cst (or a seq_cst fence between the store and the load) forbids it.");

    std::puts("\n=== IRIW (independent reads of independent writes) ===");
    std::printf("  rel / acq : %ld disagreements / %ld  (allowed by the model; ~never on x86 hw)\n",
                iriw_test<std::memory_order_release, std::memory_order_acquire>(kRounds), kRounds);
    std::printf("  seq_cst   : %ld disagreements / %ld  (0 => single total order forbids it)\n",
                iriw_test<std::memory_order_seq_cst, std::memory_order_seq_cst>(kRounds), kRounds);
    std::puts("  (x86 hardware is strong enough that IRIW disagreement isn't observed even");
    std::puts("   with acq/rel — but the C++ MODEL allows it, so portable code that needs");
    std::puts("   ALL threads to agree on one global order must use seq_cst.)");

    std::puts("\nSaar:");
    std::puts(" - MP (one producer publishes to one consumer): release/acquire is the");
    std::puts("   right minimal tool. seq_cst works but costs more.");
    std::puts(" - SB / Dekker / Peterson (mutual exclusion via flags): need seq_cst or fences.");
    std::puts(" - IRIW (everyone must agree on the order of independent events): seq_cst.");
    std::puts(" - 'It didn't fail on my x86' is not a proof — reason from the model, test");
    std::puts("   on ARM, or use TSan / a model checker (CDSChecker, herd7).");
    return 0;
}

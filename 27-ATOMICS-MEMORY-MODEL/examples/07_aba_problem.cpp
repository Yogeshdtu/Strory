// 07_aba_problem.cpp
// ============================================================
// THE ABA PROBLEM, deterministically reproduced.
//
// A CAS(head, A, B) succeeds because head "is A" — but between the
// read and the CAS, another thread popped A, popped B, and pushed A
// BACK. head is A again (the "A" in ABA), so our stale CAS succeeds
// and installs B — which is already gone. Stack corrupted.
//
// FIX shown: a TAGGED head — pack {node index, version tag} into one
// uint64. Every push bumps the tag, so the stale {A, tag=k} no longer
// equals {A, tag=k+2} -> CAS fails -> retry -> correct.
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 07_aba_problem.cpp -o aba -pthread && ./aba
// ============================================================

#include <cstdio>
#include <atomic>
#include <thread>
#include <cstdint>

// A tiny fixed pool of nodes, referenced by INDEX (not raw pointer).
struct Node { int value; std::uint32_t next; };   // next = index, 0xFFFFFFFF = null
static constexpr std::uint32_t NIL = 0xFFFFFFFFu;
static Node g_pool[8];

// BROKEN: plain index head, no version tag -> ABA
static std::atomic<std::uint32_t> g_head_broken{NIL};

int main() {
    // pool: node i has value 10*i
    for (int i = 0; i < 8; ++i) g_pool[i] = Node{10 * i, NIL};

    // ========================================================
    //  DEMO 1 — force the ABA interleaving
    // ========================================================
    // stack: 0 -> 1 -> 2   (head = 0)
    g_pool[0].next = 1; g_pool[1].next = 2; g_pool[2].next = NIL;
    g_head_broken.store(0, std::memory_order_relaxed);

    std::atomic<int> phase{0};

    // Thread A: begin a pop of node 0. Reads head=0, reads next=1.
    // Then WAITS (phase handshake) while thread B does A-B-A. Then
    // completes its CAS(head, 0, 1) — which WRONGLY succeeds.
    std::thread ta([&] {
        std::uint32_t h    = g_head_broken.load(std::memory_order_acquire);  // h = 0
        std::uint32_t next = g_pool[h].next;                                  // next = 1

        phase.store(1, std::memory_order_release);                 // "B, go do A-B-A"
        while (phase.load(std::memory_order_acquire) != 2) { }     // wait for B

        // Stale CAS: head "is still 0" (B pushed 0 back), so this succeeds
        // and installs next=1 as the head. But node 1 was already popped!
        bool ok = g_head_broken.compare_exchange_strong(h, next,
                                                        std::memory_order_release,
                                                        std::memory_order_relaxed);
        std::printf("  [A] stale CAS(head, 0, 1) succeeded = %d  -> head now = %u\n",
                    ok, g_head_broken.load());
    });

    // Thread B: pop 0, pop 1, push 0 back. Now head = 0 again, but 0->next = 2.
    std::thread tb([&] {
        while (phase.load(std::memory_order_acquire) != 1) { }

        std::uint32_t h0 = g_head_broken.load(std::memory_order_acquire);     // 0
        g_head_broken.store(g_pool[h0].next, std::memory_order_release);      // head = 1  (pop 0)
        std::uint32_t h1 = g_head_broken.load(std::memory_order_acquire);     // 1
        g_head_broken.store(g_pool[h1].next, std::memory_order_release);      // head = 2  (pop 1)
        // "reuse" node 0 and push it back
        g_pool[0].next = g_head_broken.load(std::memory_order_acquire);       // 0->next = 2
        g_head_broken.store(0, std::memory_order_release);                    // head = 0  (push 0)
        std::printf("  [B] did pop(0) pop(1) push(0). head = 0 again, but 0->next = %u\n",
                    g_pool[0].next);

        phase.store(2, std::memory_order_release);                 // "A, finish your CAS"
    });

    ta.join(); tb.join();

    std::uint32_t final_head = g_head_broken.load();
    std::printf("\n  RESULT: head = %u  (value would be %d)\n",
                final_head, final_head == NIL ? -1 : g_pool[final_head].value);
    std::puts("  ^ head is now node 1 — a node that was ALREADY POPPED. The stack is");
    std::puts("    corrupted: node 1 is 'live' again, and its ->next (=2) leaks node 2");
    std::puts("    depending on timing. Classic ABA.\n");

    // ========================================================
    //  DEMO 2 — TAGGED head: {index:32, tag:32} in one uint64
    // ========================================================
    // pack/unpack helpers
    auto pack   = [](std::uint32_t idx, std::uint32_t tag) -> std::uint64_t {
        return (static_cast<std::uint64_t>(tag) << 32) | idx;
    };
    auto idx_of = [](std::uint64_t v) -> std::uint32_t { return static_cast<std::uint32_t>(v); };
    auto tag_of = [](std::uint64_t v) -> std::uint32_t { return static_cast<std::uint32_t>(v >> 32); };

    // rebuild stack 0 -> 1 -> 2, tagged
    g_pool[0].next = 1; g_pool[1].next = 2; g_pool[2].next = NIL;
    std::atomic<std::uint64_t> head_tagged{ pack(0, 0) };

    auto push_tagged = [&](std::uint32_t idx) {
        std::uint64_t cur = head_tagged.load(std::memory_order_relaxed);
        std::uint64_t nxt;
        do {
            g_pool[idx].next = idx_of(cur);
            nxt = pack(idx, tag_of(cur) + 1);        // <-- tag bumps on every push
        } while (!head_tagged.compare_exchange_weak(cur, nxt, std::memory_order_release,
                                                    std::memory_order_relaxed));
    };

    // Thread A reads {idx=0, tag=0}, computes it would CAS to {idx=1, tag=?}.
    std::uint64_t a_saw = head_tagged.load(std::memory_order_acquire);       // {0, 0}
    std::uint32_t a_next = g_pool[idx_of(a_saw)].next;                        // 1

    // Thread B does pop(0) pop(1) push(0): each pop/push changes the tag.
    // (do it inline for determinism)
    { // pop 0
        std::uint64_t c = head_tagged.load(std::memory_order_acquire);
        head_tagged.store(pack(g_pool[idx_of(c)].next, tag_of(c) + 1), std::memory_order_release);
    }
    { // pop 1
        std::uint64_t c = head_tagged.load(std::memory_order_acquire);
        head_tagged.store(pack(g_pool[idx_of(c)].next, tag_of(c) + 1), std::memory_order_release);
    }
    push_tagged(0);   // push 0 back (tag bumps again)

    // Now Thread A tries its stale CAS with the value it saw earlier: {0, 0}
    std::uint64_t expected = a_saw;                                          // {0, 0}
    bool ok = head_tagged.compare_exchange_strong(
        expected, pack(a_next, tag_of(a_saw) + 1),
        std::memory_order_release, std::memory_order_relaxed);

    std::printf("  tagged: A's stale CAS (expected {idx=%u,tag=%u}) succeeded = %d\n",
                idx_of(a_saw), tag_of(a_saw), ok);
    std::printf("          current head = {idx=%u, tag=%u}\n",
                idx_of(head_tagged.load()), tag_of(head_tagged.load()));
    std::puts("  ^ FAILS — the tag moved (0 -> 3). A retries with the fresh head. No ABA.");

    std::puts("\nSaar:");
    std::puts(" - ABA: CAS checks VALUE equality, not 'nothing happened'. A->B->A between");
    std::puts("   read and CAS -> stale CAS succeeds -> lock-free structure corrupts.");
    std::puts(" - Fixes: (a) tagged/versioned pointer (idx+tag in one word, tag bumps");
    std::puts("   every op) — needs a wide-enough CAS (here 64-bit; raw pointers need");
    std::puts("   128-bit DWCAS / `cmpxchg16b`).  (b) hazard pointers / epochs / RCU —");
    std::puts("   don't reuse/free a node while any thread might still reference it");
    std::puts("   (folder 28 files 10-11).  (c) don't reuse addresses (index pools help).");
    return 0;
}

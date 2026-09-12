// 01_market_data_sim.cpp
// ============================================================
// PROJECT 1 -- Market Data Simulator. Yeh driver:
//   1. ek run ke stats (add/cancel counts, seq monotonic, ts monotonic)
//   2. book NON-CROSSING invariant (bids hamesha asks se neeche) -- L2Book
//      se verify
//   3. DETERMINISM: do independent sims same seed pe -> byte-identical stream
// ============================================================
//   g++ -std=c++20 -O2 -Wall -Wextra 01_market_data_sim.cpp -o md && ./md
// ============================================================

#include "mh_engine_common.hpp"

#include <cstdio>

using namespace mhft;

int main() {
    constexpr std::uint64_t N = 100000;

    MarketDataSimulator sim(44);
    sim.set_limit(N);
    L2Book book(3 * N + 16);

    std::uint64_t adds = 0, cancels = 0;
    Seq  last_seq = 0;
    Ts   last_ts  = 0;
    bool seq_ok = true, ts_ok = true, noncross_ok = true;
    MdMessage m;
    while (sim.next(m)) {
        if (m.type == MdType::Add)    ++adds;
        if (m.type == MdType::Cancel) ++cancels;
        if (m.seq != last_seq + 1) seq_ok = false;
        if (m.ts  <  last_ts)      ts_ok  = false;
        last_seq = m.seq; last_ts = m.ts;

        book.apply(m);
        if (book.has_bid() && book.has_ask() && book.best_bid() >= book.best_ask())
            noncross_ok = false;
    }

    std::printf("messages produced : %llu\n", (unsigned long long)sim.produced());
    std::printf("  adds=%llu  cancels=%llu  (add:cancel = %.2f)\n",
                (unsigned long long)adds, (unsigned long long)cancels,
                cancels ? static_cast<double>(adds) / static_cast<double>(cancels) : 0.0);
    std::printf("  sequence strictly +1 : %s\n", seq_ok ? "yes" : "NO");
    std::printf("  timestamps monotonic : %s\n", ts_ok  ? "yes" : "NO");
    std::printf("  book never crossed   : %s  (bids always < asks)\n", noncross_ok ? "yes" : "NO");
    std::printf("  final BBO            : %lld / %lld  (spread %lld ticks)\n",
                (long long)book.best_bid(), (long long)book.best_ask(),
                (long long)(book.best_ask() - book.best_bid()));

    // ---- determinism: two fresh sims, same seed, compare every field ----
    MarketDataSimulator a(44), b(44);
    a.set_limit(N); b.set_limit(N);
    MdMessage ma, mb;
    std::uint64_t compared = 0, mism = 0;
    while (a.next(ma)) {
        if (!b.next(mb)) { mism = 1; break; }
        if (ma.seq != mb.seq || ma.ts != mb.ts || ma.type != mb.type ||
            ma.side != mb.side || ma.order_id != mb.order_id ||
            ma.px != mb.px || ma.qty != mb.qty) ++mism;
        ++compared;
    }
    std::printf("\ndeterminism : %llu messages compared, %llu mismatches -> %s\n",
                (unsigned long long)compared, (unsigned long long)mism,
                mism == 0 ? "IDENTICAL" : "*** NON-DETERMINISTIC ***");

    const bool ok = seq_ok && ts_ok && noncross_ok && mism == 0;
    std::printf("\n%s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}

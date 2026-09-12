# 15 — Extensions: where to go from here

## Prerequisites
- Poora folder 44 (aur us tak pahunchne ke liye 01–43)

## Yeh topic abhi kyun

Mini engine complete hai. Yeh lesson ideas deta — kya add karo, kya
seekho, kaise aage badho. Har extension ke saath: kaunsa folder/concept
foundation hai.

## Make it faster (the 43 loop, again)

| Idea | Foundation | Expected |
|---|---|---|
| Thread it: one pinned thread per stage + SPSC queues | 41, project 7 | throughput ↑; latency only if pinned + isolated (41/13) |
| SIMD parse: parse N frames at once (AVX2) | 31/SIMD, 34/asm | maybe 1.2–2× on parse — but parse isn't the bottleneck here (measure first, 43/03) |
| Replace `std::vector<Fill>` return with a caller-provided span | 36/04, 43/15 | kills the last per-decision allocation |
| PGO + BOLT on the whole binary | 43/06 | 5–20% if `perf` says frontend-bound |
| Batch market-data apply (process 16 messages, then check strategy once) | 36/16 batching | fewer strategy calls; changes semantics — decide if OK |
| Cache-line-align the hot state; split hot/cold fields | 43/07, 43/08 | measure — this pipeline may already be small enough |
| **Bitmap summary index** over the flat book, so `rewalk_bb/ba` skips empty levels 64-at-a-time (`countl_zero`/`countr_zero`) instead of scanning 2048 | 39/11, 05/05 bit-tricks | **measured: p99.9 ~1256 → ~888 ns**, p50 unchanged — yeh 14 ka tail-regression hi theek karta hai. Har `bid_`/`ask_` write ke saath bit sync karna padega (6 jagah); gate pehle chalao |

**Rule (43/16): profile first, one change at a time, correctness gate,
re-measure, explain. Stop when the number is below the noise floor or the
next 5% costs a rewrite.**

## Make it more realistic

| Idea | Foundation |
|---|---|
| Replay from a recorded PCAP / market-data capture instead of the sim | 38/16, 13 |
| Real order gateway: TCP to a mock exchange (`TCP_NODELAY`, `writev`, keepalive) | 42/11 |
| Async fills: fills come back on a separate path, out of order — exercise the OMS + generation handles | 42, project 6, 10 |
| Gap detection + recovery: inject sequence gaps, request a snapshot, re-sync the book | 38/04, 38/13 |
| A/B feed arbitration: two feeds, take the first copy of each message | 38/12 |
| Conflation: under load, drop stale market-data updates instead of blocking | 38/15, project 7 trap 3 |
| Multiple symbols: shard the book/strategy by symbol, watch for false sharing | 43/08 |
| Hardware timestamps at ingress/egress for a true wire-to-wire measurement | 42/08, 42/14 |

## Make it more correct / robust

| Idea | Foundation |
|---|---|
| Property-based fuzzing of the whole pipeline against a reference engine | 40/16 |
| Invariant checks as real `if`-guards (not `assert`) that log-cold or kill | 13 |
| Deterministic replay in CI on every commit, output diffed against a golden run | 13, 35 |
| Shadow mode: run the new engine next to the old on live data, don't send orders, diff decisions | 13 |
| Cancel/replace (amend) support in the OMS | project 10 |
| Per-symbol + gross/net risk limits, short-sale locate, restricted lists | project 9, 37/13 |
| A UB/sanitizer pass: ASan + UBSan + TSan (on Linux) over the test suite | 23, 27 |

## Learn deeper (the specialised tracks, explicitly out of this course's scope)

37's SPECIALIZED / DOMAIN-SPECIFIC list — jinhe yeh course jaan-boojh kar
nahi karta:

| Topic | Kahan seekho |
|---|---|
| **Alpha / signal research** | quant research literature; this is the proprietary part |
| **FPGA / HLS for tick-to-trade** | 42/13 was overview-only; a hardware-design career |
| **Full DPDK / ef_vi application development** | 42/03–05 concepts; vendor docs + real NICs |
| **Exchange protocol specs** (CME MDP 3.0, Nasdaq ITCH exact, NSE) | licensed venue documentation |
| **Quant finance math** (stochastic calc, options pricing) | a different degree |
| **Regulatory detail** (Reg NMS, MiFID II, SEBI) | legal/compliance domain |
| **Colocation / cross-connect / microwave** | 42/12 intro; exchange + telco operations |

## A suggested path after this course

1. **Rebuild one component from scratch** without looking — the order book
   is the classic. Measure it. (39)
2. **Take a recorded market-data file** (many are public: LOBSTER, Nasdaq
   ITCH samples) and feed it through your engine. Diff two runs.
3. **Do the 43 loop** on your engine end to end — baseline, profile with
   `perf`, find the real bottleneck, fix one thing, re-measure, write the
   explanation.
4. **Read real code**: the LMAX Disruptor (Java, but the design is the
   point), Seastar, `moodycamel::ConcurrentQueue`, an open-source matching
   engine.
5. **Interview prep** (folder 46): the order book, `std::map` vs flat
   array, cache misses, memory ordering, "why no locks on the hot path",
   virtual dispatch cost, false sharing.

## The one thing to take away

> **Measure. Change one thing. Prove it's still correct. Measure again.
> Explain what changed. Stop when it's fast enough.**

Har folder — 35 se 44 tak — isi ek loop ke around hai. Woh loop hi HFT
performance engineering hai.

## Folder 44 complete — aur HFT build track complete

```
36 low-latency C++  -> 37 fundamentals -> 38 market data -> 39 order book
-> 40 matching engine -> 41 concurrency -> 42 networking -> 43 optimization
-> 44 CAPSTONE  ✅
```

## Next
→ [`../45-DEBUGGING/00-README.md`](../45-DEBUGGING/00-README.md)

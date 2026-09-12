# Examples — Folder 38 (market data)

> Portable `.cpp` — sab MinGW/Windows x86-64 pe chalte, koi external
> dependency nahi. **`-O2` mandatory** benchmark examples (04, 05, 06, 10)
> ke liye. `./build.ps1 folder 38-MARKET-DATA` → **10/10 OK** under strict
> flags. `wire_protocol.hpp` sab 10 examples ka SHARED protocol header hai
> (glob `*.cpp` isse alag se compile nahi karta — sirf `#include` hoti hai).

## The box these were measured on

**AMD Ryzen 7 4700U** (Zen 2), ~2.0 GHz effective, **Windows x64 + MinGW-w64
GCC 15.1.0**. TSC ~2.0 GHz (`ticks_per_ns ≈ 1.996`). **Quote the RATIOS /
shapes**, not the tail absolutes.

**⚠️ `max` on the per-op-timed loops (04, 05, 06, 10) is OS-interrupt
noise** — unpinned Windows desktop, so a scheduler tick lands on the
per-op `rdtsc()` window somewhere in a 200k-sample run. **p50 / p99 /
p99.9 are the real signal** (folder 35/06). This contrast itself is a
lesson carried forward from folders 35-37.

## Compile / run

```bash
./build.ps1 fast 38-MARKET-DATA/examples/04_parser_benchmark.cpp
./build.ps1 fast 38-MARKET-DATA/examples/05_zero_copy_parser.cpp
./build.ps1 fast 38-MARKET-DATA/examples/06_parser_comparison.cpp
./build.ps1 fast 38-MARKET-DATA/examples/10_feed_handler.cpp
# ... 01/02/03/07/08/09 -O0 se bhi theek hain (no timing)
```

## The protocol

`wire_protocol.hpp` ek teaching-purpose, ITCH-style binary protocol
define karta — 16-byte header (`length`, `msg_type`, `seq_num`,
`exch_ts_ns`, sab big-endian) + 5 message types (`AddOrder` 41B,
`Execute` 28B, `Cancel` 28B, `Delete` 24B, `Replace` 44B). Plus
`generate_feed()` — a deterministic (seeded) synthetic feed generator with
realistic lifecycle (Add → Execute/Cancel/Delete/Replace against live
orders).

## Examples

| File | Lesson(s) | Kya dikhata / measured (this box) |
|---|---|---|
| `wire_protocol.hpp` | 06 | Shared protocol: structs, byteswap helpers, `peek_header`, feed generator. Not compiled standalone. |
| `01_message_structs.cpp` | 06 | Wire struct sizes (`static_assert`-verified), `alignof==1` (pack(1) → safe for `-Wcast-align`), a raw hex dump, the "unswapped seq_num" garbled demo. |
| `02_endian_handling.cpp` | 10 | Garbled-without-swap proof (`0x12345678`→`0x78563412`), misaligned `memcpy` read, and **measured `bswap64()` cost: 0.753 ns/swap** (-O2). |
| `03_simple_parser.cpp` | 16 (step 1) | Correct, naive parser — owned `ParsedEvent` per message, pushed into an un-reserved `std::vector`. Correctness-verified (10000/10000 parsed, type mix matches generator, seq strictly monotonic). |
| `04_parser_benchmark.cpp` | 16 (step 2) | Measures 03's parser: **p50 30.1 / p99 50.1 / p99.9 771.5 / max 1728075.1 ns** — the tail is the allocation signature (36/01's pattern, here). |
| `05_zero_copy_parser.cpp` | 09 (step 3) | Overlay-cast reads, zero allocation, fixed `std::array<SymbolState,32>` sink. **p50 30.1 / p99 30.1 / p99.9 40.1 / max 6121.6 ns.** |
| `06_parser_comparison.cpp` | 09, 16 (step 4) | Both parsers, same feed, same run. **p99.9 ratio (naive/zero-copy) = 22.2×**, p50 identical (30.1 vs 30.1) — the tail, not the typical case, is what changed. |
| `07_market_data_simulator.cpp` | 01, 14 | Generates 50k messages, reports mix (Add 55.2% / Execute 17.9% / Cancel 11.3% / Delete 9.0% / Replace 6.6%), inter-message `exch_ts_ns` gaps (median 2740 ns → ~364k msgs/sec synthetic throughput), and proves the generator is deterministic (byte-identical re-run). |
| `08_gap_detection.cpp` | 04 | Simulates ~0.5% loss, detects gaps via `expected_seq` tracking: **96 gaps / 20000 sent, sanity-verified exact match** against actual drop count. |
| `09_ab_arbitration.cpp` | 12 | Two independently-lossy (~1% each) copies of one feed, arbitrated. **Single-feed loss ~0.99% → arbitrated loss ~0.02% — ~49.5× better, zero round-trips.** |
| `10_feed_handler.cpp` | 16 (step 5) | The capstone: framing + gap-detection + zero-copy dispatch + fixed book state, as one `FeedHandler` class. End-to-end **p50 30.1 / p99 30.1 / p99.9 40.1 / max 3596.8 ns** — matches the standalone zero-copy parser (05); framing+gap-detection added no extra tail. |

## Key numbers (this run)

```
02: bswap64() = 0.753 ns/swap                          (-O2)
04: naive parser      p50 30.1  p99.9  771.5 ns
05: zero-copy parser  p50 30.1  p99.9   40.1 ns
06: p99.9 ratio (naive/zero-copy) = 22.2x, p50 unchanged
07: message mix Add 55.2% / Exec 17.9% / Cancel 11.3% / Delete 9.0% / Replace 6.6%
08: 96/20000 gaps, sanity-verified
09: single-feed loss 0.99% -> arbitrated loss 0.02% (49.5x, zero round-trips)
10: end-to-end p50 30.1  p99.9  40.1 ns  (== parser-alone numbers)
```

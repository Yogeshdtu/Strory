# Examples — Folder 39 (order book)

> Portable `.cpp` — sab MinGW/Windows x86-64 pe chalte. **`-O2` mandatory**
> benchmark files (02, 04, 06, 07) ke liye. `./build.ps1 folder
> 39-ORDER-BOOK` → **9/9 OK** under strict flags. 4 shared `.hpp` headers
> (`order_workload.hpp`, `orderbook_types.hpp`, `orderbook_v1_map.hpp`,
> `orderbook_v2_vector.hpp`, `orderbook_v3_flat.hpp`) — glob `*.cpp` inhe
> alag se compile nahi karta.

## The box these were measured on

**AMD Ryzen 7 4700U** (Zen 2), ~2.0 GHz, **Windows x64 + MinGW-w64 GCC
15.1.0**. TSC ~2.0 GHz. **Quote the ratios/shapes** — tail absolutes and
`max` vary run-to-run on this unpinned box (folder 35/06's lesson,
carried forward).

## Scope

Yeh order book **market-data ko CONSUME karta** (38-MARKET-DATA jaisi
Add/Execute/Cancel/Delete/Replace events apply karke apni state maintain
karta) — yeh khud MATCHING/crossing NAHI karta (koi price-cross check).
Woh 40-MATCHING-ENGINE ka kaam hai. Detail: `01-order-book-requirements.md`.

## Compile / run

```bash
./build.ps1 fast 39-ORDER-BOOK/examples/02_orderbook_v1_bench.cpp
./build.ps1 fast 39-ORDER-BOOK/examples/04_orderbook_v2_bench.cpp
./build.ps1 fast 39-ORDER-BOOK/examples/06_orderbook_v3_bench.cpp
./build.ps1 fast 39-ORDER-BOOK/examples/07_comparison_suite.cpp
```

## The three versions

| Version | Bids/asks storage | Per-level FIFO | Order-id lookup |
|---|---|---|---|
| **V1** (`orderbook_v1_map.hpp`) | `std::map<Price, Level, greater<>>` / `std::map<Price, Level>` | `std::list<Order>` | `std::unordered_map` (stores a `list::iterator` — O(1) direct access) |
| **V2** (`orderbook_v2_vector.hpp`) | sorted `std::vector<Level>` | `std::deque<Order>` | `std::unordered_map` (stores only `{side, price}` — **O(level size) linear scan** to find the order within the level) |
| **V3** (`orderbook_v3_flat.hpp`) | tick-indexed `std::array<Level, 256>` per side | intrusive doubly-linked list in a pre-allocated arena (`uint32_t` index links) | custom tombstone-based open-addressed flat hash (`FlatIdIndex`) → arena slot, **O(1) direct access** |

## Examples

| File | Lesson(s) | Kya |
|---|---|---|
| `order_workload.hpp` | — | Shared deterministic op generator (Add/Execute/Cancel/Delete/Replace, realistic lifecycle) |
| `orderbook_types.hpp` | — | `Price`/`Qty`/`OrderId` shared aliases |
| `01_orderbook_v1_map.cpp` | 03 | V1 correctness demo |
| `02_orderbook_v1_bench.cpp` | 04 | V1 latency distribution |
| `03_orderbook_v2_vector.cpp` | 05 | V2 correctness demo — output **identical** to V1 |
| `04_orderbook_v2_bench.cpp` | 06 | V2 latency distribution — **overall worse than V1**, measured, explained |
| `05_orderbook_v3_flat.cpp` | 07-12 | V3 correctness demo — output identical to V1/V2 |
| `06_orderbook_v3_bench.cpp` | 14 | V3 latency distribution — clear winner |
| `07_comparison_suite.cpp` | 14, 15 | All three, same run, same workload + cross-version state equivalence check |
| `08_orderbook_tests.cpp` | 16 | 58 unit tests (scripted scenarios × 3 versions + cross-version equivalence after every op, 4 seeds) |
| `09_orderbook_fuzz.cpp` | 16 | 30000-op fuzz run, ~10% deliberately-invalid ops injected, checked against an independent O(n) reference model |

## Key numbers (this run, N=200000 ops, `-O2`)

```
                    p50      p99      p99.9     (ns)
V1 (map)            140.3    831.6    1502.9
V2 (sorted vector)  150.3   1312.5    2745.2    <- WORSE than V1 overall!
V3 (flat+intrusive) 120.2    480.9     661.3    <- clear winner

Add only p99.9:      V1 10700.3   V2 2755.2   V3 531.0
Exec/Cancel p50:     V1  200.4    V2  360.7   V3 130.2   <- V2 worse than V1 here
```

**V2 improves Add (cache-friendly contiguous array beats tree navigation,
p99.9 ~3.9× better than V1) but REGRESSES Execute/Cancel (~1.8× worse p50
than V1)** — because its order-id index stores only `{side, price}`, not
a direct handle, forcing an `O(level size)` linear scan to find the order
inside the level's `deque` on every reduce/remove. V3 fixes this with an
arena+intrusive-list (`O(1)` direct slot access) and wins on both. Full
story: `06-measuring-v2.md`, `15-what-changed-and-why.md`.

**Correctness**: all three versions agree exactly on `order_count`,
`best_bid`, `best_ask`, `best_bid_qty` (verified in `07`), on every op in
4 seeded 5000-op runs (verified in `08`), and against an independent
reference model across 30000 ops with ~3000 injected edge cases —
duplicate adds, ops on nonexistent ids, over-execute clamping — with
**zero disagreements** (verified in `09`).

# Examples — Folder 46 (Interview prep)

Three kinds of material:

1. **`*.cpp`** — 8 classic interview **coding problems**, each with a
   clean solution, an assertion-based `main()` that runs, complexity
   notes, and a "what the interviewer is really testing" comment.
   Portable; `./build.ps1 folder 46-INTERVIEW-PREP` → **8/8 OK** under
   strict warnings.
2. **`design/*.md`** — 4 worked **system-design** answers following the
   `15-system-design-rounds.md` arc.
3. **`mocks/*.md`** — 4 full **mock-interview transcripts** with rubrics
   (read the interviewer lines aloud, answer from memory, then compare).

---

## Coding problems (`*.cpp`)

| File | Problem | What it tests |
|---|---|---|
| `01_reverse_linked_list.cpp` | reverse a singly linked list (iterative + recursive) | pointer dance, O(1) vs O(n) space, edge cases, "when NOT a linked list" |
| `02_lru_cache.cpp` | O(1) get/put LRU | map → list-node choice, `std::list::splice`, iterator stability, eviction order |
| `03_spsc_ring_buffer.cpp` | **the HFT favourite** — lock-free SPSC ring | why no CAS, acquire/release + happens-before, power-of-two mask, `alignas(64)`, cached opposite index; 2-thread blast asserts in-order delivery |
| `04_parse_fixed_point_price.cpp` | ASCII price → int64 ticks, no float | why fixed-point (exact compares), single-pass `const char*` walk, edge cases, overflow guard, round-trip |
| `05_object_pool.cpp` | fixed-capacity pool, intrusive free list | no hot-path `new`, placement new + explicit dtor, exhaustion → nullptr, LIFO reuse, `owns()` |
| `06_top_of_book.cpp` | tiny L2 book: add/cancel/trade + O(1) BBO | flat array by tick, cached BBO + bounded re-walk, dense id index, integer prices, non-crossing invariant |
| `07_atoi_edge_cases.cpp` | string → int, every edge case | whitespace/sign/digits/stop, **overflow clamp without signed-overflow UB** (check before the arithmetic) |
| `08_moving_average.cpp` | O(1) fixed-window SMA + division-free threshold | running sum (no re-scan), ring + mask, integer exactness, `mid > sma*(1+thr)` → integer cross-multiply (no `idiv`) |

```bash
./build.ps1 folder 46-INTERVIEW-PREP        # compile all 8 (strict)
# each binary runs assertions and prints "... ALL PASS"
g++ -std=c++20 -O2 46-INTERVIEW-PREP/examples/03_spsc_ring_buffer.cpp -o t && ./t
```

Each `.cpp` ends with **TALKING POINTS** — the follow-ups an interviewer
asks and the HFT angle (why the naive data structure is wrong on the hot
path). Several map directly onto course components: `03`→ folders 28/41,
`05`→ 36, `06`→ 39/44, `04`/`08`→ 43/44.

## Design problems (`design/*.md`)

| File | Prompt |
|---|---|
| `01_feed_handler.md` | UDP-multicast market-data feed handler (A/B arbitration, gap recovery, alloc-free parse) |
| `02_matching_engine.md` | limit order book + matching engine (price-time priority, IOC/FOK, STP, the FOK-precheck bug) |
| `03_risk_gateway.md` | pre-trade risk checks on the hot path (5 O(1) checks, rate-limit ≠ kill switch) |
| `04_tick_to_trade_pipeline.md` | the full pipeline — one message end to end, every cost named; `template<class Venue>` naive-vs-optimized with a correctness gate |

## Mock transcripts (`mocks/*.md`)

| File | Round |
|---|---|
| `01_cpp_deep_dive.md` | C++ deep-dive (pointers/refs, Rule of 3/5, `noexcept` moves, `std::move`, virtual dtor) |
| `02_latency_systems.md` | latency numbers, pointer-chasing, an off-CPU tail spike, a data race |
| `03_system_design.md` | design an order book (clarify → flat array → defend → crossing → scale → verify) |
| `04_market_making_game.md` | make-a-market game + probability (two heads in a row, St. Petersburg, Bayesian shrink, mental math) |

Each transcript has a **rubric** and a "bar for a strong hire" line. Use
them with a partner, on a timer.

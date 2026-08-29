# 38 — MARKET DATA (PHASE 26)

## Prerequisites
`37-HFT-FUNDAMENTALS`, `30-NETWORKING`, `11-STRUCTS`

## Yeh folder kyun
Market data hi HFT ka **input** hai. Agar aap ise tez aur sahi parse nahi kar sakte,
baaki sab bekaar hai.

## Is folder ki files

| # | File | Kya seekhoge |
|---|------|--------------|
| 01 | `01-what-is-market-data.md` | Feeds, ticks, updates, latency ka source |
| 02 | `02-l1-l2-l3-data.md` | **L1 (top of book), L2 (price levels), L3 (individual orders)** |
| 03 | `03-snapshots-vs-incremental.md` | Snapshot, incremental updates, recovery |
| 04 | `04-sequence-numbers.md` | **Gap detection**, out-of-order, duplicates |
| 05 | `05-binary-protocols.md` | Binary vs text, why binary wins |
| 06 | `06-itch-protocol.md` | **ITCH-style protocol** — message types, layout, parsing |
| 07 | `07-fix-and-fast.md` | FIX (text), FAST (compressed) — kab dikhte hain |
| 08 | `08-sbe.md` | Simple Binary Encoding — schema, codegen |
| 09 | `09-zero-copy-parsing.md` | **In-place parsing** — no allocation, no copy, spans |
| 10 | `10-endianness-handling.md` | Network byte order, `byteswap`, safe reads |
| 11 | `11-message-framing.md` | Length prefixes, delimiters, partial reads |
| 12 | `12-ab-feed-arbitration.md` | **A/B feeds** — dual feeds, first-wins, gap fill |
| 13 | `13-recovery-and-retransmission.md` | Gap recovery, snapshot channels, replay |
| 14 | `14-timestamping-and-clocks.md` | Exchange timestamps, receive timestamps, PTP, clock skew |
| 15 | `15-conflation.md` | Conflation strategies, when to drop updates |
| 16 | `16-building-feed-handler.md` | **BUILD: poora feed handler** — simple → measured → optimized |
| 17 | `17-exercises.md` | Practice + parsing challenges |

## Examples

| File | Kya |
|---|---|
| `examples/01_message_structs.cpp` | Wire structs + static_asserts |
| `examples/02_endian_handling.cpp` | Safe byte order conversion |
| `examples/03_simple_parser.cpp` | **Step 1: correct, simple parser** |
| `examples/04_parser_benchmark.cpp` | **Step 2: measure it** |
| `examples/05_zero_copy_parser.cpp` | **Step 3: optimized, no-copy** |
| `examples/06_parser_comparison.cpp` | **Step 4: before/after numbers** |
| `examples/07_market_data_simulator.cpp` | **Full simulator** — messages, timestamps, seq numbers |
| `examples/08_gap_detection.cpp` | Sequence gap handling |
| `examples/09_ab_arbitration.cpp` | Dual feed arbitration |
| `examples/10_feed_handler.cpp` | **Complete feed handler** |

## Time
3–4 hafte

## Status
⏳ **Yeh folder abhi syllabus stage pe hai.** Upar ki file list poori plan hai —
content agle batch mein aayega.

## Next
→ [`../39-ORDER-BOOK/00-README.md`](../39-ORDER-BOOK/00-README.md)

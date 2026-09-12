# 20 — Resume & projects for HFT

## Prerequisites
Your project work (folders 39–44 count as substantial), lesson `18`.

## What an HFT recruiter/engineer screens for (in ~20 seconds)

- **C++ depth signals** — "modern C++", specific libraries, "lock-free",
  "SIMD", "template metaprogramming", a real systems project.
- **Latency/systems signals** — "kernel bypass", "cache", "profiling",
  "`perf`", "nanosecond", numbers with units.
- **Measured results** — "reduced p99 latency from 4 µs to 900 ns", not
  "improved performance".
- **Relevant domain** — order book, matching engine, market data, feed
  handler, low-latency messaging.
- **Rigor** — testing, benchmarking, "correctness gate", determinism.

Red flags: only web/CRUD experience with no systems depth; buzzwords with
no substance; no numbers anywhere; typos in a field that prizes
precision.

---

## Resume structure (1 page, ordered by relevance)

```
NAME | email | GitHub | LinkedIn | (location, work authorization if relevant)

SUMMARY (optional, 2 lines)
  "Low-latency C++ engineer. Built [order book / matching engine /
   lock-free pipeline]. Focus: cache-aware data structures, profiling,
   deterministic systems."

SKILLS (tight, honest — you WILL be grilled on these)
  Languages:  C++17/20 (primary), Python, a bit of x86-64 asm
  Systems:    Linux, perf, gdb, ASan/UBSan/TSan, valgrind, sanitizers
  Concepts:   lock-free SPSC/MPMC, memory model, cache/branch, SIMD, RAII
  Tools:      CMake, git, Google Benchmark, HdrHistogram

EXPERIENCE  (each bullet: what you built + how + MEASURED result)

PROJECTS  (the HFT-relevant ones, with numbers and a repo link)

EDUCATION  (degree, relevant coursework: OS, architecture, networks)
```

Put **projects above experience** if your jobs aren't HFT-relevant and
the projects are.

---

## Writing bullets that land

**Formula:** *[Built/Optimized] [what] using [how], achieving [measured
result] [vs baseline].*

| Weak | Strong |
|---|---|
| "Worked on a trading system" | "Built a single-threaded tick-to-trade pipeline (feed decode → L2 book → strategy → risk → OMS); deterministic replay, 12 integration configs" |
| "Made the code faster" | "Cut order-book update from ~170 ns to ~67 ns/msg by replacing `std::map` with a flat array indexed by price tick + cached BBO; verified BBO byte-identical to a `std::map` reference over a 118k-msg session" |
| "Used lock-free programming" | "Implemented a wait-free SPSC ring (acquire/release on two `alignas(64)` indices, cached opposite index); ~6 M msg/s hand-off, consumer saw every message in order" |
| "Optimized a parser" | "Hand-rolled a bounds-checked binary feed parser (memcpy + bswap, frame-validated once); ~1.8 ns/frame at `-O2`, 200k-frame agreement test vs a portable reference" |
| "Reduced latency" | "Removed hot-path allocation with a fixed-size object pool + generation-checked handles; alloc+free p99.9 ~30 ns vs ~180 ns for `new`/`delete`" |

Always: **units**, a **baseline**, and how you **verified correctness**.

---

## Projects that matter for HFT (you have these)

| Project | The one-line pitch | Where in this course |
|---|---|---|
| **Limit order book** | flat-array-by-tick, cached BBO, dense id index, O(1) add/cancel; BBO verified vs a `std::map` reference | 39, 44 |
| **Matching engine** | price-time priority, Limit/Market/IOC/FOK, self-trade prevention; fuzz-verified vs an independent reference (0 disagreements / 30k commands) | 40 |
| **Lock-free SPSC queue** | wait-free, acquire/release, false-sharing-safe, cached opposite index | 28, 41 |
| **Market data feed handler** | UDP-style framing, sequence gaps, non-crossing book by construction, alloc-free parse | 38, 42, 44 |
| **Low-latency pipeline optimization** | v0→v3 tick-to-order: `std::map`+`stod` → flat array + integer parse + fixed-point + division-free SMA; ~60–80×, order stream byte-identical | 43 |
| **Mini HFT engine (capstone)** | all of the above connected, deterministic, `template<class Venue>` naive-vs-optimized with a correctness gate before any speedup claim | 44 |
| **Async lock-free logger** | hot path enqueues a POD (~20–40 ns) to an SPSC ring; background thread formats/writes; drop + count on full | 41, 45 |

For the resume, pick **2–3**, give each a bullet with numbers + a repo
link. Be ready to whiteboard any of them (`18`).

---

## GitHub / code sample hygiene

- A clean README: what it is, how to build (`cmake` / one command), how
  to run the tests/benchmarks, and the **measured numbers with the
  machine described**.
- Tests that run (`ctest` / a script). A benchmark that prints a
  histogram or ratios.
- Small, readable commits. No committed build artifacts. A `.gitignore`.
- Honest caveats ("benchmarked on an unpinned laptop; ratios not
  absolutes") — this reads as *rigor*, not weakness, to an HFT reviewer.
- One flagship repo done well beats five half-finished ones.

---

## Common resume mistakes

1. **No numbers.** Every performance/scale claim needs a figure + unit +
   baseline.
2. **Skills you can't defend.** If "SFINAE" or "TCP internals" is on
   there, expect a deep follow-up. Cut what you can't back up.
3. **Two pages / dense walls.** One page, scannable, most-relevant first.
4. **Passive "was responsible for".** Active verbs, "I built / measured /
   reduced".
5. **Typos / inconsistent formatting.** In a precision field this is
   disqualifying for some reviewers.
6. **Listing the language version wrong** ("C++11" while using concepts).
7. **No repo link** for a project you describe in detail.

---

## Cover letter / outreach (when asked for one)

3 short paragraphs: (1) the specific role + one genuine reason you want
*this* firm (their tech/market/scale — something real). (2) your single
most relevant project, one sentence, with the measured result. (3) a
concrete thing you'd want to work on there + a line inviting the
conversation. No fluff, no "I am passionate about excellence."

---

## Final week checklist

- [ ] Resume: 1 page, projects with numbers + repo links, skills you can
      all defend
- [ ] Flagship repo: builds in one command, tests run, README has the
      measured numbers + machine
- [ ] 2–3 project bullets you can whiteboard cold (`18`)
- [ ] LinkedIn/GitHub consistent with the resume
- [ ] A specific, honest "why this firm" for each application

---

**Folder 46 complete.**

## Next
→ [`../47-CODING-PROBLEMS/00-README.md`](../47-CODING-PROBLEMS/00-README.md)

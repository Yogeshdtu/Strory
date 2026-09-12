# 05 — HFT projects

The HFT project track is **folder [`44-HFT-PROJECTS`](../44-HFT-PROJECTS/00-README.md)** —
the course capstone. It is not repeated here.

---

## What's there

Folder 44 assembles folders `01`–`43` into one connected, deterministic
**`MiniHftEngine`**:

```
MarketDataSimulator → FeedParser → L2Book → Strategy → RiskEngine → OrderManager → Venue → fills → OMS + PnL
```

15 lessons + 12 example drivers + 11 shared `mh_*.hpp` headers,
`./build.ps1 folder 44-HFT-PROJECTS` → **12/12 OK**. Every timestamp comes from
the event stream (no wall clock) → replay is byte-identical.

| Folder-44 project | Reuses / builds on |
|---|---|
| Market-data simulator | 38 |
| Feed parser (alloc-free, fixed-point price) | 38, 43 |
| L2 order book (flat array + cached BBO) | 39, `47/10` |
| Matching engine (the venue) | 40 |
| Memory pool / object pool | 14, 36, `47/03`, `49 advanced P2` |
| SPSC queue (wire → engine hand-off) | 41, `47/08`, `49 advanced P4` |
| Strategy simulator | 43 |
| Risk engine (5 O(1) checks) | 37/13, `47/10 #8` |
| Order manager (state machine + gen-checked handles) | 37 |
| `template<class Venue>` naive vs optimized + correctness gate | 43 |

The capstone optimization: `NaiveEngine` (`std::map` matching-engine venue) vs
`OptimizedEngine` (`FastVenue` flat array + per-level FIFO) — proven
**byte-identical across 5 configs before** the ~1.5–1.6× speedup is claimed
(book stage ~150 → ~67 ns/msg).

---

## How folder 49's projects feed folder 44

The general projects here are the **prerequisites in disguise**:

- **Advanced P1 (custom `Vector`)** → the flat-array book and pools in 44 are the
  same "raw storage + placement `new` + no per-element default-construct" idea.
- **Advanced P2 (allocator)** → 44's `FixedPool` / `ObjectPool` are the
  fixed-size-pool and segregated-free-list from P2, specialized for `Order`.
- **Advanced P4 (concurrent queue)** → 44's wire→engine hand-off *is* the SPSC
  ring from P4.
- **Advanced P5 (epoll server)** → the OS event loop a real feed handler / order
  gateway sits on (folder 42).
- **Advanced P6 (JSON parser)** → the recursive-descent + precise-error +
  depth-limit discipline is exactly what a robust wire-protocol parser needs.
- **Intermediate P5 (KV store)** → write-ahead log + replay = the determinism /
  crash-recovery model 44 uses for the engine.

Do folder 49's advanced projects, then folder 44 is "the same techniques, wired
into a trading system".

---

## For the resume

Folder `46/20` (resume & projects) lists the 7 that matter and the bullet
formula. In short: **order book, matching engine, SPSC queue, feed handler,
pipeline optimization, mini HFT engine, async logger** — all in folder 44, each
with a *measured* result. A general portfolio project from here (custom vector,
allocator, thread pool, JSON parser) is a good second-tier bullet to show
breadth.

## Next
→ [`../44-HFT-PROJECTS/00-README.md`](../44-HFT-PROJECTS/00-README.md)

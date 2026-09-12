# Design problem — Full tick-to-trade pipeline

> This is the integration of designs 01–03 + strategy + order gateway +
> OMS + logging. The concrete reference is course folder 44's
> `MiniHftEngine`.

---

## Prompt

"Design the whole thing: market data comes in, orders go out. Walk me
through one market message end to end and name every cost."

---

## 1. Clarify

- **Instruments:** how many, and do we shard across cores? (yes — one
  single-threaded pipeline per instrument group)
- **Strategy:** given (a mechanical rule for the exercise — *not* alpha).
- **Latency target:** wire-in → wire-out, p99.9 (say < 5 µs kernel-bypass,
  or a specific budget).
- **Determinism:** byte-identical replay required (yes).
- **Threading:** single-threaded per instrument, or shared-nothing stages
  on separate cores connected by SPSC queues?

## 2. The pipeline

```
NIC ─► Feed decoder ─► L2/L3 book ─► Strategy ─► Risk ─► Order gateway ─► NIC
        (design 01)     (design 02)              (design 03)   + OMS
                                    ▲                           │
                                    └──────── fills ────────────┘
```

- **Single-threaded per instrument** — no locks, deterministic ordering,
  one instrument's state (book + strategy + risk + OMS) fits in L1/L2.
- **Scale by sharding instruments across cores**, not by threading one
  instrument.
- Any thread boundary (e.g. NIC-poll thread → engine thread) is an
  **SPSC ring** (`03_spsc_ring_buffer.cpp`).
- **Async logger** off the hot path: enqueue a POD (~20–40 ns) to an
  SPSC ring; a background thread (housekeeping core) formats + writes
  binary.

## 3. One market message, end to end (name every cost)

| Step | What | ~Cost |
|---|---|---|
| NIC → poll | busy-poll sees the descriptor | ns |
| Frame split + A/B + gap check | pointer math, seq compare | ~few ns |
| Decode | bounds-check once, fixed-offset fields, `bswap` | ~1–2 ns/field |
| Book apply | `arr[idx] += qty` + maybe a 1–2-tick BBO re-walk | ~15–70 ns |
| Strategy `on_book` | ring-buffer SMA running sum + book imbalance + a **division-free** cross-multiply threshold; cooldown check | ~tens of ns |
| (if it acts) Risk `check` | 5 integer comparisons, rolling-window rate | ~few ns |
| (if OK) OMS submit + encode | pool-allocated order record, gen-checked handle, encode to the wire format | ~tens of ns |
| Send | `write` on a warm TCP session / bypass TX | ns–µs |
| Fill comes back | OMS state transition, position + P&L update | ~tens of ns |

**No** allocation, syscalls (except the final send), or locks on this
path. Time comes from `rdtsc` / the event stream, never the wall clock.

## 4. Determinism — how, and why it matters

- No wall-clock reads in logic — every timestamp from the event stream.
- Fixed RNG seeds; no unordered-map iteration affecting output; no data
  races.
- ⇒ a recorded input session **replays byte-identical**.
- Enables: reproducing a production incident offline, regression testing
  ("did this change alter any fill?"), and `rr`-style time-travel
  debugging (`45/09`).

## 5. The optimization loop, applied end to end

`MiniHftEngine` is `template<class Venue>`:
- `NaiveEngine` — `std::map` `MatchingEngine` as the venue (one tree
  insert + one `std::list`-node `malloc` per market message).
- `OptimizedEngine` — `FastVenue` (flat-array aggregate book + per-level
  FIFO + IOC sweep).

Profiling found the venue mirror was the `book` stage's bulk (~150
ns/msg); `FastVenue` halves it (~67 ns/msg) → **~1.5–1.6× end-to-end**.
**Correctness gate first:** an integration test proves `NaiveEngine ==
OptimizedEngine` byte-for-byte (fills, qty, P&L, position) across 5
seed/config combos, both deterministic, invariants held — **before** the
speedup is reported.

## 6. Failure modes (cross-cutting)

| Failure | Handling |
|---|---|
| Feed gap | A/B arbitration; snapshot resync; strategy suspends on stale book |
| Slow consumer anywhere | SPSC ring fills → drop + count + alert; never block upstream |
| Risk breach | reject; kill switch after K real breaches (rate-limit excluded) |
| Late exchange ack for a recycled order slot | **generation-checked handle** → `get()` returns null → ack dropped safely |
| Crash | core dump + the recorded input session → replay offline under `rr` |
| Logger backpressure | drop + count (the logger must never stall the hot path) |

## 7. Measure

`rdtsc` checkpoints per stage → an always-on HdrHistogram keyed to a git
commit + NIC hardware timestamps for the true wire-to-wire number. CI
replays a recorded session and fails on p50/p99/p99.9 regression. Every
optimization PR carries a before/after histogram and passes the
naive-vs-optimized correctness gate.

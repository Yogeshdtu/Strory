# 16 — Production mein latency measure karna

## Prerequisites
- `03-rdtsc-timing.md`, `05-percentiles.md`, `06-jitter-and-tail-latency.md`,
  `07-histograms.md`
- `28-LOCK-FREE` (SPSC/MPSC rings), `29-LINUX-SYSTEMS` (tracing)
- `examples/08_latency_recorder.cpp`

## Yeh topic abhi kyun
Lab benchmark (lesson 08–09) ideal conditions mein ek number deta. Par
**production** mein real inputs, real load, real noisy neighbours hote —
aur wahi jagah hai jahan p99.9 actually maayne rakhta. Challenge: measure
karo **bina system ko dheema kiye** (measurement khud latency add na kare),
continuously, aur us data ko aggregate karo. Yeh HFT ka roz ka kaam hai.

---

## Requirements — production measurement se

1. **Low overhead** — hot path pe ~few ns per measurement, no alloc, no
   lock, no syscall. Warna tum jo naap rahe ho use hi badal doge.
2. **Always on** — sirf "jab problem ho tab" nahi; regression ko turant
   catch karne ke liye 24/7.
3. **Full distribution** — p50/p99/p99.9/p99.99/max, mean nahi (lesson 04–05).
4. **Per-stage** — end-to-end nahi, har stage (packet-in → parse → decide →
   order-out) alag, taaki regression localize ho.
5. **Aggregatable** — multi-thread / multi-host → merge → global percentiles
   (lesson 05, 07).
6. **Queryable / dashboarded** — time series, alerting.

---

## Technique 1: inline `rdtsc` + fixed histogram

`08_latency_recorder.cpp` ka pattern — production ka bread and butter:

```cpp
LatencyHistogram stage_parse;                 // 30 KB fixed, per-thread

// hot path:
uint64_t t0 = rdtsc_fenced();                 // ~9–18 ns (lesson 03)
parse(msg);
uint64_t t1 = rdtsc_fenced();
stage_parse.record(t1 - t0);                  // ~2 ns (lesson 07)
```

- **Cost:** ~2 ns record + timer. Acceptable jab stage khud ~µs hai; agar
  stage ~50 ns hai to plain `__rdtsc()` (no fence) + accept the OoO slop,
  ya sample (technique 3).
- **Calibrate once** at startup (`ticks_per_ns`), store, convert to ns at
  query time (off hot path).
- **Per-thread histogram** — no sharing, no lock. A housekeeping thread
  periodically snapshots + merges + publishes (technique 4).
- **`rdtscp` + `aux` check** if threads can migrate — discard cross-core
  samples (lesson 03).

**Measured (`08`, is box):** `record()` ~2 ns/event, histogram percentiles
within ~1.3% of exact, 29.5 KB fixed.

---

## Technique 2: SPSC ring → aggregator thread

Agar hot path pe **kuch bhi** extra (even the ~2 ns record) na chahiye, ya
tumhe raw per-event traces chahiye:

```cpp
// hot thread: just push a timestamp tuple, lock-free, no math
struct Sample { uint32_t stage; uint64_t t0, t1; };
spsc_ring<Sample, 1<<16> ring;                 // folder 28

// hot path:
ring.try_push({STAGE_PARSE, t0, t1});          // ~5–10 ns, no contention

// aggregator thread (pinned to a non-trading core):
Sample s;
while (ring.try_pop(s)) {
    hist[s.stage].record(s.t1 - s.t0);          // all the math here, off hot path
}
```

- Hot path: one lock-free push, no division, no histogram walk.
- Aggregator does calibration conversion, histogram updates, publishing,
  even writing raw samples to disk for post-mortem.
- **Ring full → drop** (count the drops). Under a latency spike the hot
  thread must not block on a full ring — dropping samples is correct, and
  the drop count itself is a signal.
- This is the shape most HFT instrumentation actually takes.

---

## Technique 3: sampling (when even a ring push is too much)

- **1-in-N** — `if ((++n & 1023) == 0) record(...)` — one cheap counter +
  branch. Tail estimate weaker (rare events under-sampled) but overhead
  ~zero.
- **Time-gated** — record at most once per e.g. 10 µs (`if (now - last >
  10us)`).
- **Conditional / exceedance** — only record when `t1 - t0 > threshold`
  (e.g. > p99 baseline). You lose the distribution shape but capture every
  tail event cheaply, with a full context dump (which message, which
  branch). Great for "catch and diagnose the spikes".

---

## Technique 4: publishing the snapshot

Aggregator/housekeeping thread, every 1 s:
- **Recorder pattern** (HdrHistogram) — double-buffered: writers keep
  recording into buffer A while the reader takes a consistent copy of B,
  then swap. No writer stall.
- Convert to ns, compute p50/p90/p99/p99.9/p99.99/max.
- Push to: a metrics system (Prometheus histogram / StatsD),
  a shared-memory region a dashboard reads, or a log line
  (`stage=parse p50=210 p99=480 p99_9=1350 max=41000 drops=0`).
- **Merge across threads** (per-thread histograms add) → per-process; merge
  across hosts (histograms ship as `.hgrm` / protobuf) → fleet-wide
  (lesson 05 — never average the p99s).

---

## Technique 5: OS / kernel-level tracing (no code changes)

| Tool | Kya | Overhead |
|---|---|---|
| **USDT probes** (`SDT.h`) | static tracepoints you compile in (`DTRACE_PROBE`), zero cost when not attached, `bpftrace`/`perf` attach at will | ~0 idle |
| **uprobes** (`perf probe -x ./app 'func'`) | attach to any function at runtime, no recompile; ~µs per hit | attach-time only |
| **eBPF** (`bpftrace`, BCC) | in-kernel programs: histogram a function's latency, argument, off-CPU time, syscall latency — live, aggregated in-kernel | low, tunable |
| **`perf sched` / `perf trace`** | scheduling latency, syscall latency, per-thread timeline | moderate |
| **LTTng** | high-throughput userspace+kernel tracing, ring-buffered to disk | low, designed for always-on |
| **ftrace / function_graph** | kernel function timing | moderate |

Example — histogram a function's latency live, no code change:
```bash
bpftrace -e 'uprobe:/path/app:parse_message { @t[tid] = nsecs; }
             uretprobe:/path/app:parse_message /@t[tid]/ {
                 @ns = hist(nsecs - @t[tid]); delete(@t[tid]); }'
```
Prints a power-of-2 histogram of `parse_message` latency, aggregated in the
kernel, while the app runs. Perfect for ad-hoc production diagnosis.

For the hottest HFT paths, kernel-transition cost (uprobe ~µs) is too much →
use compiled-in USDT (near-zero when detached) or the inline histogram
(technique 1).

---

## Coordinated omission — in production too

Lesson 05 ka concept load-testing ke alaawa production mein bhi:
- Agar tum **only measure requests you actually processed**, aur ek stall ke
  dauraan upstream ne backpressure lagaake requests **rok diye**, to woh "jo
  requests aati" invisible hain → tumhari measured p99 achhi dikhti jabki
  users ka experience bura.
- **Fix:** measure at the **entry** (when the request *arrived* / *should
  have been served*), not when you got around to it. Queue wait time counts.
  Instrument `t0 = arrival_time` (from the packet / a load balancer
  timestamp), not `t0 = when_my_thread_picked_it_up`.

---

## Black box vs white box

- **White box** — the app instruments itself (techniques 1–4). Precise,
  per-stage, but needs code + knows only what it measures.
- **Black box** — measure from outside: a tap on the wire (hardware
  timestamping on the NIC / a switch), a synthetic prober sending known
  requests and timing replies, RUM (real user monitoring). Catches what the
  app can't see (NIC queue, kernel, network) and can't be fooled by the
  app's own blind spots.
- HFT: **both** — NIC hardware timestamps (PTP-synced) at wire in/out give
  the true end-to-end, and inline `rdtsc` per stage gives the breakdown.
  Reconcile the two.

---

## > **HFT relevance**

> - **Per-stage `rdtsc` + per-thread HdrHistogram**, snapshot every 1 s by a
>   housekeeping thread on a non-trading core, published to a dashboard.
>   p50/p99/p99.9/p99.99/max per stage, always on.
> - **NIC hardware timestamps** (Solarflare/Exablaze/PTP) for true
>   wire-to-wire; software stages fill in the breakdown.
> - **Exceedance capture** — when tick-to-trade > threshold, dump the full
>   context (which symbol, book state, code path) to a ring for offline
>   analysis. The spikes are where the money leaks.
> - **Drop counters** on every ring — a rising drop count means the hot path
>   is spending time it didn't before.
> - **Regression alerting** — p99.9 per stage vs a rolling baseline; a
>   kernel upgrade / BIOS change / noisy deploy shows up in minutes.
> - Measurement overhead itself is budgeted and measured — the instrument
>   must cost < ~1% of the stage it measures.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — heavy instrumentation on the hot path
`std::chrono` (~38 ns) + a `mutex` + a `map` insert per event → you're
measuring your instrument. Inline `rdtsc` + fixed per-thread histogram, or a
ring.

### Trap 2 — measuring from "thread picked it up", not arrival
Hides queue wait = coordinated omission in production. Timestamp at entry.

### Trap 3 — global shared histogram with a lock
Contention + false sharing (folder 27). Per-thread, merge off-path.

### Trap 4 — averaging percentiles across threads/hosts
`(p99_a + p99_b)/2` is wrong (lesson 05). Merge histograms, then percentile.

### Trap 5 — only measuring when investigating
The regression that shipped last Tuesday is invisible if you started
measuring on Thursday. Always on.

### Trap 6 — uprobe on the hottest path
~µs per hit (kernel transition) wrecks a ~100 ns stage. Compiled-in USDT
(near-zero detached) or inline histogram for the hot path; uprobes for
colder code / ad-hoc.

### Trap 7 — ring backpressure blocking the hot thread
Full ring → hot thread must **drop + count**, never block. A blocked
producer under load is the opposite of what you want.

---

## Hands-on

```bash
./build.ps1 fast 35-PROFILING-BENCHMARKING/examples/08_latency_recorder.cpp
# record() ~2 ns/event ; histogram p50..p99.99 within ~1.3% of exact ;
# 29.5 KB fixed ; bimodal distribution visible in the log-scaled dump

# Linux, live histogram of any function without recompiling:
bpftrace -e 'uprobe:./app:hot_fn { @s[tid]=nsecs }
             uretprobe:./app:hot_fn /@s[tid]/ { @=hist(nsecs-@s[tid]); delete(@s[tid]) }'
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "prod = same as lab benchmark" | real inputs/load/neighbours; tail lives here |
| "`std::chrono` per event is fine" | ~38 ns; use rdtsc + fixed histogram / ring |
| "one global histogram + mutex" | per-thread, lock-free, merge off-path |
| "measure when my thread starts work" | measure at arrival (queue wait counts) |
| "turn on measurement to investigate" | always on — catch regressions live |
| "uprobe everything" | ~µs/hit; USDT / inline for hot paths |

---

## Exercises

1. Your tick-to-trade p99 dashboard has read "480 ns" steadily for months.
   Traders report worse fills lately. You add a synthetic prober (black box)
   that times replies from outside; it reports p99 = 2.1 µs. Both are
   "correct". What's the gap?

   <details><summary>Answer</summary>

   The white-box instrument measures **only the stages it wraps** — probably
   `parse → strategy → risk → encode`, from "thread dequeues the message" to
   "thread hands the order to the NIC path". It does **not** see: (a) **NIC
   RX queue / kernel / driver time** before your thread got the message, (b)
   **TX queue / NIC serialization** after you handed off, (c) **queue wait**
   if messages arrive faster than you process them (coordinated omission —
   you time from dequeue, not arrival), (d) any **batching / interrupt
   coalescing** on the NIC. The black-box prober sees the *whole* path
   wire-to-wire, so 2.1 µs − 480 ns ≈ 1.6 µs is happening in the parts your
   instrument skips. Fixes: add **NIC hardware timestamps** (RX and TX) to
   get true wire-to-wire, timestamp at **arrival** not dequeue (exposes
   queue wait), and instrument the RX/TX handoff stages. The recent
   regression is likely in one of those blind spots (a driver/NIC config
   change, rising load causing queue buildup).
   </details>

2. You add per-stage `std::chrono::steady_clock::now()` timestamps (6 stages
   = 7 calls) to a hot path whose total budget is 300 ns. After deploying,
   the p50 rises to ~560 ns. What happened, and how should it have been done?

   <details><summary>Answer</summary>

   7 × `steady_clock::now()` at ~38 ns each ≈ **266 ns of pure instrument
   overhead** added to a 300 ns path — you nearly doubled it, and you're now
   measuring "the path + 7 clock reads", not the path. Each stage's reported
   time is also inflated by ~38 ns. Correct approach: (a) use **`rdtsc`**
   (~1 tick raw, ~9–18 ns fenced) instead of `chrono` — 7 reads ≈ 60–120 ns,
   still significant on a 300 ns budget, so (b) consider **plain `__rdtsc()`
   without fences** (~1 tick, accept OoO slop of a few ns — negligible vs the
   stages) for the intermediate boundaries, fencing only the outer t0/t1, or
   (c) **sample** — only timestamp 1-in-1000 requests fully, so steady-state
   overhead ≈ 0, or (d) push the 7 raw TSC values into an **SPSC ring** and
   do all subtraction/conversion/histogram in the aggregator thread — hot
   path cost is 7 counter reads + one ring push. Subtract the measured
   instrument self-cost from each stage. Budget the instrument at <1% of the
   stage.
   </details>

3. Fleet of 20 hosts, each publishing per-minute latency stats. The
   monitoring system stores `p99` per host per minute and shows
   `avg(p99)` across the fleet as the headline number. Why is this
   misleading, and what should it show instead?

   <details><summary>Answer</summary>

   `avg(p99)` across hosts is **not the fleet p99** — percentiles don't
   average (lesson 05). If 19 hosts have p99 = 500 µs and 1 host is on fire
   with p99 = 50 ms, `avg(p99) ≈ 2.98 ms` — a number no host actually has,
   and it *hides* the one broken host (looks like "everything mildly slow"
   rather than "19 fine, 1 dead"). It also can't answer "what latency does
   the worst 1% of *all* requests see". Fix: hosts publish **mergeable
   histograms** (HdrHistogram / DDSketch), the monitoring system **merges**
   them (bucket counts add) and computes the **true fleet-wide p99/p99.9**
   from the merged histogram. Additionally show **per-host p99 as a
   heatmap / max-across-hosts** so an outlier host is obvious, and
   **request-weighted** merge (a host serving 10× traffic contributes 10×).
   `avg(p99)` should not exist on the dashboard.
   </details>

---

## Interview questions

1. Production latency instrument ke 6 requirements (overhead, always-on,
   distribution, per-stage, aggregatable, queryable).
2. Inline `rdtsc` + fixed histogram vs SPSC-ring-to-aggregator — trade-offs.
3. Sampling strategies (1-in-N, time-gated, exceedance) — kab kaunsa.
4. Coordinated omission production mein — arrival-time vs dequeue-time.
5. Per-thread histograms + merge — kyun shared+lock nahi.
6. White box vs black box measurement — har ek kya catch karta.
7. eBPF/uprobe vs USDT vs inline — hot path pe kaunsa aur kyun.

---

## Next
→ [`17-exercises.md`](17-exercises.md)

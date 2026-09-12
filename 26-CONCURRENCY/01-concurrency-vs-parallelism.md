# 01 — Concurrency vs parallelism

## Prerequisites
- `08-FUNCTIONS` (call stack), `25-OBJECT-MODEL` file 03 (thread storage)
- Bas — yeh folder ka entry point hai

## Yeh topic abhi kyun
Ab tak ek thread tha — instructions ek line mein, ek ke baad ek. Ab **kai** honge.
Sabse pehle do words clear karo jo log interchangeably (galat) use karte hain:
**concurrency** aur **parallelism**. Yeh design decisions drive karta hai — kya
aapko latency chahiye, throughput chahiye, ya bas responsiveness.

---

## Definitions

| | **Concurrency** | **Parallelism** |
|---|---|---|
| Kya | Kai tasks ko **manage** karna jo overlap kar sakte hain (structure) | Kai tasks ko **ek saath execute** karna (hardware) |
| Zaroorat | 1 core kaafi (time-slicing) | 2+ cores |
| Analogy | Ek chef 3 dishes ke beech switch karta | 3 chefs, 3 dishes, ek saath |
| Goal | Responsiveness, latency-hiding, structure | Throughput (more work/second) |
| Example | Ek web server 1000 connections handle karta (mostly waiting on I/O) | Ek matrix multiply 8 cores pe |

> **Concurrency is about dealing with lots of things at once. Parallelism is about
> doing lots of things at once.** — Rob Pike

Concurrency ek **program-structure** property hai. Parallelism ek **runtime**
property hai. Ek concurrent program parallel chal *sakta* hai (agar cores hon), ya
nahi (1 core pe interleaved).

---

## Teen alag reasons to use threads

### 1. Parallelism — CPU-bound work faster

```cpp
// 8 cores, ek bada array process karna
std::for_each(std::execution::par, v.begin(), v.end(), heavy_transform);
```
Kaam ko N pieces mein baato, N cores pe chalao → ~N× faster (Amdahl's law tak).
Yeh `examples/07_thread_pool` (fib × 64 tasks → ~5.7× on 8 cores).

### 2. Latency hiding — I/O-bound work overlap

```cpp
auto a = std::async(fetch, url_a);    // network request chal raha
auto b = std::async(fetch, url_b);    // parallel
process(a.get(), b.get());            // dono ek saath wait
```
Threads mostly **wait** karte hain (network, disk). CPU idle rehta — usse doosre
kaam pe lagao. Yahan "parallelism" nahi (CPU kaam nahi kar raha), sirf overlap.

### 3. Responsiveness / structure — background work

Ek GUI ka main thread responsive rahe jab ek slow computation background thread pe
chale. Ya ek trading engine ka event loop tick process kare jab ek logging thread
disk pe likhe. **Concurrency for separation of concerns.**

---

## Interleaving — ek core pe bhi "concurrent"

```
1 core, 2 threads (T1, T2):

time ──►
T1: ▓▓▓░░░░▓▓▓░░░▓▓▓        (▓ = running, ░ = waiting for CPU)
T2: ░░░▓▓▓░░░▓▓▓░░░▓▓▓

OS scheduler time-slices: har ~few ms ek context switch.
```

Yeh **concurrent** hai (dono progress kar rahe, overlapping) par **parallel nahi**
(ek waqt mein ek hi actually running). 2 cores pe:

```
2 cores:
core 0, T1: ▓▓▓▓▓▓▓▓▓▓▓▓
core 1, T2: ▓▓▓▓▓▓▓▓▓▓▓▓     <- ab parallel bhi
```

---

## Amdahl's law — parallelism ki limit

Agar aapke program ka **fraction `p`** parallelize ho sakta (baaki `1-p` serial
hai), to N cores pe max speedup:

```
speedup(N) = 1 / ( (1 - p) + p/N )
```

- `p = 0.95`, `N = 8` → speedup ≈ 5.9× (not 8).
- `p = 0.95`, `N = ∞` → speedup → **20×** (the serial 5% caps you).
- `p = 0.5` → even infinite cores → max **2×**.

**Serial bottleneck (lock contention, a shared counter, a single queue) is what
kills scaling** — `examples/03` shows mutex vs atomic vs "no sharing" (1248 / 430
/ 1.9 ms).

---

## > **HFT relevance**
> HFT concurrency is mostly **not about parallelism for throughput** — it's about:
> - **Pinning the hot path to one core** (isolated, no scheduler, no
>   hyperthread sibling) and keeping it a **single thread** doing one thing per
>   tick — no locks, no context switches, no jitter.
> - **Offloading everything non-critical** (logging, telemetry, risk snapshots,
>   admin) to other cores via **lock-free SPSC queues** (folder 28) so the hot
>   thread never blocks.
> - **Parallelism where it's safe and cold** — backtesting, analytics, order-book
>   snapshots for a UI.
>
> The mental model: the tick path is a **concurrent** system (hot thread + helper
> threads communicating), but the hot thread itself is deliberately **serial** and
> **uninterrupted**. Contention, false sharing, and syscalls (which a lock can
> cause) are the enemies (files 07, 16; folder 28, 41).

---

## Hands-on

```bash
./build.ps1 fast 26-CONCURRENCY/examples/07_thread_pool.cpp   # parallelism: ~5.7x on 8 cores
./build.ps1 fast 26-CONCURRENCY/examples/03_mutex_fix.cpp     # serial bottleneck: 1248 vs 1.9 ms
```

Compute your machine's Amdahl speedup for `p = 0.9`, `p = 0.99` at `N = 4, 8, 16`.
Then look at `examples/03` — which version has `p ≈ 1` (no serial part)?

---

## ⚠️ Traps

### Trap 1 — "concurrent" == "parallel"
Concurrency = structure (can overlap). Parallelism = simultaneous execution
(needs cores). A concurrent program on 1 core is not parallel.

### Trap 2 — expecting N× speedup from N threads
Amdahl: the serial fraction caps you. A shared lock / counter / queue *is* serial.

### Trap 3 — threading I/O-bound work for "parallelism"
The CPU isn't the bottleneck — you're hiding latency, not doing more compute.
More threads than the I/O concurrency limit just add overhead.

### Trap 4 — more threads than cores for CPU-bound work
Oversubscription → context-switch thrash, cache pollution. Match `hardware_concurrency`.

### Trap 5 — assuming threads make code faster
Threads add coordination cost (locks, atomics, cache coherence). If the work is
small or serial, threads make it **slower** (`examples/03`: mutex 1248 ms vs
serial-ish 1.9 ms).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "concurrency and parallelism are the same" | Concurrency = structure; parallelism = simultaneous execution |
| "N threads → N× faster" | Amdahl: serial fraction caps it; contention adds serial |
| "threading always helps" | Coordination cost can exceed the benefit for small/serial work |
| "1 core can't run concurrent code" | It can — time-sliced interleaving; just not in parallel |
| "I/O-bound work needs many cores" | It needs overlap (async), not compute parallelism |
| "the hot path should be multi-threaded" | HFT: keep it single-threaded + uninterrupted; offload the rest |

---

## Exercises

1. **Classify:** (a) rendering video frames on a GPU, (b) a chat server handling
   10k idle connections, (c) `std::sort` of 100M ints with `par`, (d) a UI thread
   + a background autosave.

   <details><summary>Answer</summary>

   (a) parallelism (throughput). (b) concurrency (mostly waiting; structure).
   (c) parallelism. (d) concurrency (responsiveness / separation).
   </details>

2. **Amdahl:** `p = 0.8`. Speedup at `N = 4`? At `N = ∞`?

   <details><summary>Answer</summary>

   `N=4`: `1 / (0.2 + 0.8/4) = 1 / 0.4 = 2.5×`. `N=∞`: `1 / 0.2 = 5×` (the 20%
   serial part caps it).
   </details>

3. **Bottleneck:** in `examples/03`, the mutex version does 16M locked
   increments across 8 threads and takes ~1248 ms; the "local + combine" version
   takes ~1.9 ms. What's `p` (parallel fraction) for each, roughly?

   <details><summary>Answer</summary>

   Mutex: `p ≈ 0` for the increment work — it's fully serialized by the single
   lock (plus contention overhead), so 8 threads are *slower* than 1. Local+combine:
   `p ≈ 1` — each thread's loop is independent, only the final combine is serial
   (negligible), so it runs at ~single-thread speed but 8-way.
   </details>

4. **Design:** a market-data handler must decode 2M messages/sec and also write a
   full audit log. How do you split the work?

   <details><summary>Answer</summary>

   Hot thread: decode + process, pinned to an isolated core, single-threaded, no
   locks. It pushes a compact record onto a **lock-free SPSC queue** per message.
   A separate logger thread on another core drains the queue and writes to disk.
   The hot thread never blocks on I/O or a lock.
   </details>

5. **Oversubscription:** you run 64 CPU-bound threads on an 8-core machine. What
   happens vs 8 threads?

   <details><summary>Answer</summary>

   ~Same total throughput at best, usually worse: 64 threads time-slice on 8
   cores → frequent context switches (each ~µs + cache/TLB pollution), more
   scheduler overhead, worse cache locality. Match the thread count to the core
   count for CPU-bound work (a pool — `examples/07`).
   </details>

---

## Interview questions

1. Concurrency vs parallelism — definition, ek example each.
2. Threads use karne ke 3 alag reasons (parallelism / latency-hiding / structure).
3. Amdahl's law — formula, aur "serial 5% caps you at 20×" ka matlab.
4. 1 core pe "concurrent" code kaise chalta (interleaving)?
5. N threads ≠ N× speedup — kyun (serial fraction, contention)?
6. I/O-bound vs CPU-bound work — threading strategy alag kyun?
7. HFT hot path — single-threaded kyun, baaki kaam kahan?

---

## Next
→ [`02-process-vs-thread.md`](02-process-vs-thread.md)

# 10 — Logging strategy (aur HFT async logging)

## Prerequisites
- `04-INPUT-OUTPUT/03-buffering-and-flushing.md` (stdout buffering, `std::endl` cost)
- `26-CONCURRENCY` (logging thread), `41-HFT-CONCURRENCY` (SPSC queue)
- `43-HFT-OPTIMIZATION/04-hot-cold-path-separation.md`

## Yeh topic abhi kyun

gdb ke saath tum ek waqt pe **ek** run inspect karte ho, apni machine pe.
Logs alag hain: woh **production** mein, **hazaar runs** baad, **jab tum
so rahe the** — batate hain kya hua. 3am ka page aata hai to log hi
tumhara crime scene hai. Achhe logs = fast root cause; bure logs =
"reproduce karne ki koshish karta hoon" (aksar impossible).

Aur HFT mein ek twist: **hot path mein normal logging = latency spike**.
Ek `printf`/`std::cout` critical section mein = mutex + format + syscall =
microseconds. Solution: async logging. Yeh lesson dono cover karta.

---

## Log levels — aur kab kaunsa

| Level | Kab | Production mein on? |
|---|---|---|
| **TRACE** | har function entry/exit, har loop iteration | ❌ (dev / targeted only) |
| **DEBUG** | intermediate values, decisions, "kyun yeh branch" | ❌ usually (ya sampled) |
| **INFO** | significant events: startup, config loaded, connection, order sent | ✅ |
| **WARN** | kuch galat par recoverable: retry, fallback, degraded mode | ✅ |
| **ERROR** | operation fail hui, needs attention: exception, rejected order, data gap | ✅ (alert) |
| **FATAL** | process mar raha hai | ✅ (page immediately) |

Rule: **level ek runtime knob ho** (config / env / signal). Prod normally
`INFO`; ek incident ke waqt `DEBUG` on kar do bina redeploy (`SIGHUP`
handler, `kill -USR1`, admin endpoint).

---

## Kya log karo — aur kya nahi

### ✅ Log karo

- **Decisions + unke inputs:** "rejected order 4471: price 100.25 outside
  collar [99.9, 100.1], mid=100.0" — na sirf "rejected".
- **State transitions:** "session AUTH → READY", "book crossed, resolving".
- **Boundary events:** external message in/out (ID + key fields), config
  change, resource limit hit.
- **Correlation IDs:** har request/order ke saath ek unique id jo poore
  system ke logs mein carry ho — ek trade ko end-to-end trace kar sako.
- **Quantities jo debugging mein chahiye:** counts, sizes, seq numbers,
  timestamps (nanosecond, monotonic source — `29-LINUX-SYSTEMS/16`).

### ❌ Mat log karo

- `log("here")`, `log("entering function")` bina context — noise, aur
  hypothesis ke bina print jaisa hi bekaar (`01` trap 2).
- **Hot loop ke andar** per-iteration logs (prod mein) — GBs/min, disk
  bandwidth khatam, aur real events dab jaate.
- **Secrets:** passwords, API keys, full card numbers, PII (compliance +
  security).
- **Aur phir usi cheez ko throw bhi karna** (`log-and-throw` antipattern,
  neeche).

---

## Structured logging

Free text (`"user alice did 3 trades in 5s"`) grep-friendly hai par
machine-parse nahi. Structured = key=value ya JSON:

```
2026-09-05T09:31:00.123456789Z lvl=WARN comp=risk evt=order_reject
  order_id=4471 reason=price_collar px=100.25 mid=100.00 band_bps=10
  trace_id=a1b2c3
```
ya
```json
{"ts":"...","lvl":"WARN","comp":"risk","evt":"order_reject","order_id":4471,
 "reason":"price_collar","px":100.25,"mid":100.00,"trace_id":"a1b2c3"}
```

Faayda: `evt=order_reject reason=price_collar` pe filter, `px` pe
aggregate, dashboards, alerting rules — sab automated. Incident mein
"pichhle 5 min mein kitne collar rejects, kis symbol pe" ek query.

Libraries: **spdlog** (fast, header-mostly, sync+async), **quill**
(low-latency, async-first, HFT-oriented), Boost.Log, glog. Rolling your
own bhi common HFT mein (neeche).

---

## Antipatterns

### log-and-throw
```cpp
if (bad) {
    LOG_ERROR("bad input: {}", x);
    throw std::runtime_error("bad input");   // ❌ ab caller bhi log karega
}
```
Ya to **log karo aur handle** karo, ya **throw karo** aur caller ko decide
karne do. Dono karne se har error do-teen baar logs mein, alag stack
depths pe — noise, aur "kitne asli errors" count galat.

### Logging as control flow
Log line ko parse karke behaviour badalna — logs ka format ek din badlega
aur sab toot jaayega. Logs **output** hain, API nahi.

### `std::endl` per line
`std::endl` = `'\n'` **+ flush**. Har log line pe flush = per-line syscall
(`04-IO/03`). `'\n'` use karo, flush ko explicit/periodic/at-exit rakho
(crash pe last buffer chala jaaye iska trade-off — critical errors ke liye
flush, INFO ke liye nahi).

### Time from `localtime` in the hot path
`localtime`/`strftime` locale locks leta, slow. Timestamp raw
(`clock_gettime(CLOCK_REALTIME)` ya TSC) capture karo, human format
**offline / background thread** mein.

---

## HFT: async logging

**Problem:** critical path (feed → book → strategy → order) mein ek log
line = format (µs) + mutex (contention) + `write()` (syscall, µs, page
faults). p99 latency spike. Aur ek slow disk log ke peeche pura pipeline
ruk sakta.

**Solution:** hot thread sirf **raw data** ek lock-free queue mein daale;
ek **background thread** format + write kare.

```
   [hot thread]                         [logger thread]
   log(evt, order_id, px, mid) ──push──► SPSC ring ──pop──► format ──► write()
        ~20 ns (POD copy)                (41-HFT-CONCURRENCY)   (disk / mmap file)
```

### Sketch

```cpp
// Hot path: sirf POD, fixed size, no allocation, no format.
struct LogRec {
    std::uint64_t tsc;          // rdtsc -- calibrate offline
    std::uint16_t evt;          // enum -> string offline
    std::uint16_t comp;
    std::int64_t  a, b, c;      // generic payload slots
};

inline void log_fast(std::uint16_t evt, std::int64_t a,
                     std::int64_t b = 0, std::int64_t c = 0) noexcept {
    LogRec r{ rdtsc(), evt, kCompStrat, a, b, c };
    if (!g_log_queue.try_push(r)) [[unlikely]] {
        g_log_dropped.fetch_add(1, std::memory_order_relaxed);  // never block the hot path
    }
}

// Background thread:
void logger_loop() {
    LogRec r;
    while (g_running.load(std::memory_order_relaxed)) {
        while (g_log_queue.try_pop(r)) {
            // tsc -> wall time (offset calibrated at startup),
            // evt/comp -> strings, payload -> named fields,
            // append to an mmap'd file or a buffered stream.
            format_and_append(r);
        }
        std::this_thread::yield();     // ya a short nanosleep
    }
}
```

Key decisions:

- **Hot side does the minimum:** ek `rdtsc` + ek fixed-size struct copy +
  ek `try_push`. ~20–40 ns. Koi `std::string`, koi `snprintf`, koi lock.
- **Queue full → drop + count**, never block. Ek dropped-count metric
  batata logging backpressure ho rahi hai (tune queue size / logger).
- **`[[unlikely]]`** on the full branch (`43/04-05`).
- **Binary log on disk**, decode offline — even format+write ko background
  se aage `hktcat`-style tool pe daal sakte. Ya text but pre-sized buffer
  + periodic `writev`.
- **`mmap`'d ring file** (`29-LINUX-SYSTEMS/07`) — logger thread bas memcpy
  kare, OS flush kare. Crash pe bhi last records disk pe.
- **Logger thread ko pin** karo ek non-critical core pe (`29/11`), hot
  threads se door; uski cache pollution isolate.
- **Nanosecond timestamps from one monotonic source** taaki events order
  kar sako across threads (`27` happens-before ko logs mein reconstruct).

### Trade-off

| | Sync log | Async (queue + bg thread) |
|---|---|---|
| Hot-path cost | µs (format + lock + syscall) | ~20–40 ns (POD push) |
| Crash pe last lines | flush kiya to safe | queue mein jo tha woh lost (mmap ring se mitigate) |
| Ordering across threads | natural (per-line lock) | tsc timestamp se reconstruct karna padta |
| Complexity | low | SPSC/MPSC queue, bg thread, offline decoder |
| CPU | hot thread pe | ek dedicated core |

Non-HFT service: **spdlog async mode** kaafi hai (ek background thread +
a bounded queue, similar idea, ready-made). Hand-rolled sirf jab har ns
matter karta ho.

> **HFT relevance:** trading engines ka logging **hamesha** async +
> binary hota. Ek "why did we send that order" post-mortem 10 microseconds
> ki granularity pe events chahiye — par woh events capture karne ki cost
> 10 ns honi chahiye, 10 µs nahi. Yeh folder 43 ka hot/cold separation
> logging pe applied hai: hot = enqueue POD; cold = format, stringify,
> write, rotate.

---

## Logs + the other tools

- Log ne bataya **kaunsa input** crash se pehle aaya → us input pe gdb /
  replay harness.
- Log ne bataya **kaunsi assertion / invariant** pehle toota → wahan se
  bisect (`01`).
- Correlation id ne ek bad trade isolate kiya → us id ke saare log lines
  ek jagah → timeline.
- Metrics (dropped count, queue depth, error rate) ne **kab** bataya →
  time window narrow.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — Hot path mein `LOG_DEBUG(...)` "bas abhi ke liye"
`LOG_DEBUG` disabled bhi ho, argument evaluation ho sakta (`expensive()`
call), aur macro na ho to branch bhi. HFT hot loop mein koi log statement
nahi — enqueue-POD ya kuch nahi.

### Trap 2 — `std::endl` everywhere
Per-line flush = per-line syscall. `'\n'`, periodic flush.

### Trap 3 — Unstructured "grep karke nikaal lenge"
Ek incident mein 40 GB logs. Free-text grep slow + aggregate impossible.
Structured (key=value/JSON) se query + dashboard.

### Trap 4 — log-and-throw
Ek error, teen log lines, teen stack depths. Log **ya** throw, dono nahi.

### Trap 5 — Async logger silently drops, koi metric nahi
Queue full → drop. Agar dropped-count kahin nahi dikhta → tum ko lagega
logs complete hain, actually 30% missing. **Dropped counter** expose karo.

### Trap 6 — Secrets / PII logs mein
Compliance violation + agar logs leak → breach. Redact at the source.

### Trap 7 — Logger thread ko critical core pe pin karna
Woh hot threads se cache/HT resources cheenta. Alag core, alag NUMA node
if possible (`29/14`).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Zyada log = behtar debugging" | Signal-to-noise. Decisions + inputs, not "here". |
| "`LOG_DEBUG` prod mein cost-free hai" | Arg eval + branch + (if on) format+IO. Hot path: zero. |
| "Async logging = spdlog laga do" | Non-HFT haan. HFT: POD enqueue ~20ns, bg format, binary, mmap, drop-count. |
| "Crash pe logs to milenge hi" | Buffered/async → last lines lost. Critical: flush; async: mmap ring. |
| "Free text theek hai" | Structured se filter/aggregate/alert. Incident mein query chahiye. |

---

## Hands-on

```bash
# std::endl vs '\n' cost -- 04-IO/03 ka benchmark dobara
# (per-line flush ka syscall overhead dekho: strace -c ./prog)
strace -f -e trace=write -c ./your_prog 2>&1 | tail

# folder 41 ka SPSC queue + ek bg consumer = mini async logger
#   41-HFT-CONCURRENCY/examples/spsc_queue.hpp
# folder 44 ka engine per-stage timing = "structured event" ka basic shape
```

Ek chhota async logger likho: `LogRec` POD, `SpscQueue<LogRec, 4096>`,
producer thread `try_push` (full → `dropped++`), consumer thread `try_pop`
→ `fprintf` to a file. Producer ka enqueue cost `rdtsc` se measure karo —
target < 50 ns.

---

## Exercises

1. Ek service p99 latency 2 µs se 40 µs spike karti har ~100ms. Logs
   dekho: har spike ke saath ek `LOG_INFO` line "processed batch". Kya ho
   raha, fix?
   <details><summary>Answer</summary>
   `LOG_INFO` synchronous hai — format + lock + `write()` (aur shayad
   `std::endl` flush → syscall). Har 100ms pe batch boundary pe woh line
   critical path mein 38 µs kha rahi (disk hiccup / lock contention).
   Fix: us log ko async karo (POD enqueue), ya batch-boundary pe hi kyun,
   ya level ghata ke DEBUG (prod off). Verify: spike gaya, enqueue cost
   ~ns.
   </details>

2. Async logger: hot-path enqueue ~25 ns, par ek incident ke baad pata
   chala logs mein ~15% events missing hain (gaps in seq). Kya hua, aur
   kaunsa ek number add karna chahiye tha?
   <details><summary>Answer</summary>
   Queue **full ho rahi thi** — logger thread (format + write) producer
   se dheema, ring bhar gaya, `try_push` fail → events silently dropped.
   Chahiye tha: **`dropped` counter** (aur ideally queue-depth high-water
   mark) as a metric. Fixes: bada ring, tez logger (binary not text,
   `writev`, mmap), logger thread ko better core, ya sampling under load.
   </details>

3. "log-and-throw" ke exact do nuksaan batao.
   <details><summary>Answer</summary>
   (1) **Duplicate logs** — throw site logs, phir har `catch` layer bhi
   log kar sakta → ek error 3-4 baar, alag stack depths pe, "error rate"
   metric inflated. (2) **Responsibility confusion** — jo function throw
   karta usne decide kar liya "yeh loggable error hai", par caller ke
   paas context ho sakta ki yeh expected/recoverable hai (retry) — ab
   noise. Rule: log **ya** throw; jo layer decide kar sakti hai severity,
   wahi log kare.
   </details>

4. Nanosecond timestamps do threads ke logs mein — ek thread `t=1000`,
   doosra `t=999` par tumhe pata hai pehla event pehle hua. Kaise
   possible, kya fix?
   <details><summary>Answer</summary>
   Do threads ne alag time source use kiya (per-core TSC without
   invariant/sync, ya ek ne `CLOCK_REALTIME` jo NTP se peeche jump kiya).
   Fix: **ek** monotonic source (`CLOCK_MONOTONIC_RAW`, ya ek calibrated
   invariant TSC read via the same helper), aur logs order karne ke liye
   raw ticks store karo, wall-clock offline. HFT boxes TSC ko invariant +
   synced rakhte iss wajah se (`29/16`, `35/06`).
   </details>

---

## Interview questions

1. Log levels — prod mein kaunse on, aur level ko runtime pe kaise
   badloge bina redeploy?
2. Structured vs free-text logging — incident response mein fark?
3. log-and-throw kyun antipattern? Alternative?
4. HFT async logger design karo: hot-path kya kare, bg thread kya, queue
   full pe kya?
5. `std::endl` ka hidden cost? Async logger mein crash pe last lines kaise
   bachao?
6. Correlation id kya, aur ek distributed trade flow debug karne mein
   kaise help karta?

---

## Next
→ [`11-perf-for-debugging.md`](11-perf-for-debugging.md)

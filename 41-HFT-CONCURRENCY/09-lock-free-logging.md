# 09 — Async logging: hot path sirf enqueue karta hai

## Prerequisites
- `08-core-pinning-strategy.md`
- `36-LOW-LATENCY-CPP` (allocation avoidance, syscall avoidance)

## Yeh topic abhi kyun

Logging ek "boring infrastructure" cheez lagti hai, PAR har hot-path
function ISSE touch karta (`log("order accepted")`, `log("risk check
passed")`, etc.) — agar HAR call synchronous format+write karti hai,
yeh cost EVERYWHERE multiply hoti. Yeh 36's allocation/syscall-avoidance
principle ka EK specific, high-impact application hai.

---

## Naive vs async — architecture

```
NAIVE:
  hot-path-function() { ...; log("event happened"); ... }
                              |
                        format string (sprintf-jaisa)
                        + write() syscall (console/file)
                        SEEDHA yahin, INLINE, HOT PATH ke andar

ASYNC (04's SpscQueue use karke):
  hot-path-function() { ...; logger.log(code, tag); ... }
                              |
                        SIRF ek fixed POD struct ENQUEUE
                        (no format, no syscall, no allocation)
                                    |
                                    v
                        [background thread]
                        DEQUEUE -> format -> write (jitna time le, hot
                        path ko PATA hi nahi)
```

---

## `07_async_logger.cpp` — measured

```cpp
struct LogRecord {
    std::uint64_t t_tsc;
    std::uint32_t code;      // enum/event-id -- STRING FORMATTING NAHI hot path pe
    char          tag[48];   // fixed buffer, koi heap allocation nahi
};

bool log(std::uint32_t code, const char* tag) {   // HOT PATH
    LogRecord r{...};
    return q_.try_push(r);    // BAS itna hi
}
```

**Measured (N=500K, dono SAME "discard" sink pe likhte — fair
comparison, sirf sync-vs-async variable isolated):**

```
naive log() (format+write ON hot path)   : p50 1082.1 ns
async log() (sirf enqueue)               : p50   70.1 ns
mean: naive 1194.7 ns vs async 86.0 ns  (13.9x)
```

**~15x median improvement, ~14x mean.** Naive path ki cost `fprintf` ke
andar hoti (format string parsing + underlying `write`/`fwrite`
syscall path) — async path ki cost bas ek atomic-load + struct-copy +
atomic-store (`try_push`'s andar).

---

## Design decisions worth naming explicitly

1. **Fixed-size record, no allocation.** `tag[48]` -- agar longer string
   ho, TRUNCATE hoti (`strncpy`), kabhi heap allocate nahi karti. Yeh
   36's "allocation-free hot path" ka direct application.
2. **No string formatting on hot path.** `code` ek integer/enum hai, na
   ki pehle-se-formatted string — actual human-readable text
   BACKGROUND thread banata (jab CPU-cost matter nahi karta).
3. **Drop-on-full policy, explicit.** `try_push()` false de sakta agar
   queue full ho (background thread bahut peeche reh gaya) — `log()`
   isse IGNORE karta (return value discard). Yeh EXPLICIT design choice
   hai: **logger KABHI hot path ko slow/block NAHI kar sakta**, chahe
   iska matlab kuch log lines KHO jaayein. (37/13's risk-systems
   philosophy se related: observability infrastructure khud NEVER
   bottleneck nahi ban sakti.)

---

## ⚠️ Traps / Common mistakes

### Trap 1 — background thread ko BLOCKING banana (agar full ho to push block kare)
Agar `log()` full hone pe BLOCK kare (jab tak jagah na ho), logger ab
HOT PATH ko slow kar sakta jab consumer peeche reh jaaye — bilkul
opposite of iska poora purpose. Drop-on-full (ya overwrite-oldest, ek
alternative policy) zaroori hai.

### Trap 2 — LogRecord mein `std::string` rakhna
`std::string` heap-allocate kar sakti (SSO se bahar ki strings ke
liye) — hot path pe koi bhi allocation is design ke poore point ko
khatam kar deti. Fixed `char[N]` buffer hi sahi hai.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Logging "itni chhoti cheez hai, matter nahi karti" | Har hot-path call ISSE touch karti -- 15x cost multiply everywhere |
| Async logger ko "guaranteed delivery" chahiye | Drop-on-full EXPLICIT, correct choice hai (hot path ko kabhi block na karo) |
| `std::string`/`std::format` theek hai log records ke liye | Heap allocation risk -- fixed-size POD chahiye |

---

## Hands-on

```bash
./build.ps1 fast 41-HFT-CONCURRENCY/examples/07_async_logger.cpp
```

---

## Exercises

1. Async logger ka background thread bahut peeche reh gaya (queue full
   rehti). Kya HOT PATH latency par isse asar padega?
   <details><summary>Answer</summary>
   Nahi -- `try_push()` sirf `false` return karega (drop), koi wait/block
   nahi. Hot path ki cost HAMESHA constant rehti (ek try_push() call ki
   cost), consumer ki speed se INDEPENDENT. Yeh exactly is design ka
   poora point hai.
   </details>

---

## Interview questions

1. Naive aur async logging ka architecture-fark batao.
2. `LogRecord` fixed-size POD kyun hai, `std::string` kyun nahi?
3. Drop-on-full policy kyun (aur kis principle se related hai, 37/13 se)?

---

## Next
→ [`10-wait-free-reads.md`](10-wait-free-reads.md)

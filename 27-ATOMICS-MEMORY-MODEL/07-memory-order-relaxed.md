# 07 — `memory_order_relaxed`

## Prerequisites
- `06-why-memory-ordering.md`
- [`examples/03_relaxed_ordering.cpp`](examples/03_relaxed_ordering.cpp)

## Yeh topic abhi kyun
`relaxed` sabse weak order hai — **atomicity milta hai, ordering bilkul nahi**.
Iske exact guarantees (aur non-guarantees) samajhna zaroori: kahaan yeh perfectly
sahi hai (counters), aur kahaan silently toot jaata hai (data publish karna).

---

## Kya `relaxed` guarantee karta hai

`a.fetch_add(1, std::memory_order_relaxed)` / `a.load(relaxed)` / `a.store(v,
relaxed)`:

1. **Atomicity** — operation indivisible. No lost updates, no torn read/write.
2. **Modification-order consistency** — har *single* atomic variable ke liye ek
   total order of writes hota hai jispe sab threads agree karte hain. Ek thread us
   variable ki values ko us order ke *reverse* mein nahi dekhega (no going
   backwards on one variable — "coherence").
3. **Nothing else.** Iss operation aur *kisi bhi doosri* memory operation
   (atomic ya non-atomic, same ya different variable) ke beech **koi
   happens-before / ordering nahi**.

```cpp
std::atomic<int> x{0}, y{0};
// Thread 1                       // Thread 2
x.store(1, relaxed);             r1 = y.load(relaxed);   // can see 0
y.store(1, relaxed);             r2 = x.load(relaxed);   // can see 0
// r1 == 1 && r2 == 0  is allowed (Thread 2 saw y's write but not x's)
```

Alag variables ke relaxed operations ek doosre ke around freely reorder ho sakte
hain — compiler aur CPU dono.

---

## Kahan `relaxed` perfectly sahi hai

### 1. Standalone counters (value matters, timing/ordering nahi)
```cpp
std::atomic<uint64_t> requests_served{0};
// many threads:
requests_served.fetch_add(1, std::memory_order_relaxed);
// one reader, later:
std::cout << requests_served.load(std::memory_order_relaxed);
```
Aap ko sirf **sahi total** chahiye. Kaunsi increment kab dikhi — matter nahi
karta. `relaxed` = fastest, exact count (`examples/03` demo 1: exact har run).

Metrics, stats counters, event counts, dropped-packet counts — sab `relaxed`.

### 2. A flag whose ordering is provided elsewhere
```cpp
done.store(true, std::memory_order_relaxed);   // OK *if* the data this gates
                                               // is synchronized by another edge
```
Rare aur fragile — usually you just want `release`/`acquire`.

### 3. `shared_ptr` reference count increment
libstdc++ `shared_ptr` ka refcount **increment** `relaxed` karta hai (aap ke paas
already ek valid pointer hai, count badhana ordering nahi mangta). **Decrement**
`acq_rel` hota hai (last decrement se pehle ki saari uses ko dtor se pehle order
karna hai). Yeh ek classic real-world `relaxed`+`acq_rel` pairing hai.

### 4. Idempotent "seen it" / lazy-init sentinels
"Kisi ne yeh kaam kar diya" type flags jahan double-work harmless hai.

---

## Kahan `relaxed` toot jaata hai

### Data publication — the #1 mistake
```cpp
Config* cfg = nullptr;                 // plain
std::atomic<bool> ready{false};

// Producer
cfg = new Config(...);                 // (A)
ready.store(true, std::memory_order_relaxed);   // ❌

// Consumer
while (!ready.load(std::memory_order_relaxed)) {}   // ❌
cfg->use();                            // (B) — may see cfg == nullptr / garbage
```
No synchronizes-with edge → (A) does **not** happen-before (B). The consumer can
see `ready == true` with `cfg` still null or half-constructed. Also `cfg` itself
is a plain pointer read racing a write → **data race → UB**. Fix: `release` /
`acquire` (file 08).

### Two-variable invariants / Dekker-style flags
`relaxed` "I set my flag, did you set yours?" mutual-exclusion — broken (see the
store-buffering demo; even `release/acquire` isn't enough — you need `seq_cst`).

### "It works on x86"
x86 TSO makes many relaxed programs *appear* ordered. The same code on ARM
reorders freely. `relaxed` code that relies on incidental x86 ordering is a latent
bug.

---

## `examples/03` — what it shows

- **Demo 1 (counters):** 8 threads × N `fetch_add(1, relaxed)` → **exact** total,
  every run. Relaxed is all you need for a count.
- **Demo 2 (publish/subscribe with relaxed):** producer writes a payload then a
  `relaxed` flag; consumer spins on the flag then checks the payload.
  On this x86 box: **0 mismatches** observed (TSO hides it) — but the code is
  formally UB, and the lesson text says so. Run under TSan / on ARM → mismatches.
  **Measurement contradicting expectation → teach it, don't hide it (CLAUDE.md
  Rule 2): "0 mismatches here does NOT mean correct — it means x86 is forgiving."**

---

## Mental model

> `relaxed` = "this one variable's accesses are atomic and can't be seen out of
> their own write order. Everything else is fair game to reorder."

Use it when the **only** thing you care about is the atomic value itself, in
isolation.

---

## > **HFT relevance**
> - **Stats/metrics counters on the hot path** — `fetch_add(1, relaxed)`. Cheapest
>   correct increment; you read the aggregate off the hot path.
> - **Ring-buffer index bump in SPSC** is *not* pure relaxed — the index
>   `release`s the slot data to the consumer (folder 28). Relaxed there would let
>   the consumer read a slot before the producer's write lands.
> - **Never publish a pointer/struct/sequence with a `relaxed` store** — that's
>   the bug that survives every x86 test and blows up on the ARM box or under a
>   compiler upgrade.
> - **`relaxed` load to *peek*** (e.g. "is the queue roughly non-empty?") before
>   doing the real `acquire` load is a valid fast-path trick — the peek carries no
>   guarantee, the real load does.

---

## Hands-on

```bash
./build.ps1 fast 27-ATOMICS-MEMORY-MODEL/examples/03_relaxed_ordering.cpp
```

Then:
- Demo 1: change `relaxed` → `seq_cst`. Same answer, slightly slower — proves
  relaxed is sufficient for a counter.
- Demo 2: run under `-fsanitize=thread` (Linux) — TSan flags the payload
  read/write as a race even though the output "looks right".
- `g++ -O2 -S`: `fetch_add(1, relaxed)` → `lock xadd` (same as seq_cst on x86 for
  an RMW — the RMW is already a full barrier); `store(v, relaxed)` → plain `mov`
  (vs `xchg`/`mfence` for seq_cst).

---

## ⚠️ Traps

### Trap 1 — `relaxed` to publish data
No happens-before edge. Reader sees the flag but not the guarded data. Use
`release`/`acquire`.

### Trap 2 — "relaxed counter can lose counts"
No — it's a full atomic RMW. The count is **exact**. Only *ordering* is relaxed.

### Trap 3 — testing relaxed correctness on x86 only
x86 TSO hides most relaxed reordering. Test on ARM / under a model checker / TSan.

### Trap 4 — relaxed for a spin flag that gates work
`while (!go.load(relaxed))` then touching data `go` was meant to protect → race.
`acquire`.

### Trap 5 — assuming relaxed ops to *different* atomics keep program order
They don't — they reorder freely w.r.t. each other and w.r.t. non-atomics.

### Trap 6 — relaxed for `shared_ptr`-style refcount **decrement**
Increment `relaxed` is fine; the final **decrement** must be `acq_rel` (order all
prior uses before the destructor).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "relaxed can lose updates" | Fully atomic RMW — exact count; only ordering is dropped |
| "relaxed = volatile" | `volatile` isn't atomic at all; `relaxed` is atomic + no ordering |
| "relaxed flag safely publishes data" | No edge — use `release`/`acquire` |
| "relaxed ops keep source order" | Only per-single-variable (coherence); across variables, no |
| "if it's right on x86 it's right" | x86 TSO hides relaxed reordering; ARM won't |
| "seq_cst counter is wrong to use" | It's just slightly slower — relaxed is the *optimization*, both are correct for a count |

---

## Exercises

1. **relaxed enough?** (a) a hit counter read once at shutdown; (b) `stop` flag
   that a worker checks then flushes a buffer; (c) publishing a freshly-built
   lookup table pointer; (d) `shared_ptr` refcount ++.

   <details><summary>Answer</summary>

   (a) yes — count only. (b) no — `stop` gates the flush of data the setter wrote;
   needs `release`/`acquire`. (c) no — publication; `release`/`acquire`.
   (d) yes — increment is `relaxed` (decrement is `acq_rel`).
   </details>

2. **Outcome legal?** `x,y` atomic, all `relaxed`. T1: `x.store(1); y.store(1);`
   T2: `a=y.load(); b=x.load();` — is `a==1, b==0` allowed?

   <details><summary>Answer</summary>

   Yes. `relaxed` gives no ordering between the two stores or the two loads, so T2
   can see `y`'s new value and `x`'s old one.
   </details>

3. **Why exact but unordered:** explain how a `relaxed` counter is simultaneously
   "always the right total" and "no ordering".

   <details><summary>Answer</summary>

   Each `fetch_add` is an indivisible RMW on the modification order of that one
   variable — no add is lost, and all threads agree on the sequence of values it
   took. "No ordering" means those adds aren't ordered relative to *other* memory
   ops, which is irrelevant when you only read the final count.
   </details>

4. **x86 vs ARM:** `examples/03` demo 2 shows 0 payload mismatches here. Does that
   make the code correct? What would you expect on ARM?

   <details><summary>Answer</summary>

   No — it's still a data race / UB. x86 TSO happens to preserve store-store order
   so the payload write is visible before the flag. ARM's weak model reorders them
   → the consumer sees the flag set with a stale/garbage payload → mismatches.
   </details>

5. **Peek pattern:** is `if (count.load(relaxed) > 0) { v = count.load(acquire);
   ... }` a valid optimization?

   <details><summary>Answer</summary>

   Yes as a *fast-path filter* — the relaxed peek carries no guarantee, but it's
   cheap and you re-load with `acquire` before acting on anything the count gates.
   Just don't skip the acquire load.
   </details>

---

## Interview questions

1. `relaxed` exactly kya guarantee karta (teen cheezein) aur kya nahi?
2. Modification-order / coherence — ek single atomic variable ke liye kya milta?
3. `relaxed` counter exact hota hai par unordered — dono kaise sach?
4. `relaxed` se data publish kyun galat — kaunsa edge missing?
5. `shared_ptr` refcount: increment `relaxed`, decrement `acq_rel` — kyun alag?
6. x86 pe relaxed program "sahi chalta hai" — ARM pe kyun nahi?
7. relaxed peek + acquire load — valid pattern kyun?

---

## Next
→ [`08-acquire-release.md`](08-acquire-release.md)

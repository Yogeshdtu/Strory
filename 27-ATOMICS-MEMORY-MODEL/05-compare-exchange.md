# 05 — Compare-and-swap (CAS)

## Prerequisites
- `04-atomic-operations.md`
- [`examples/02_cas_loop.cpp`](examples/02_cas_loop.cpp)

## Yeh topic abhi kyun
CAS (`compare_exchange`) lock-free programming ka **universal primitive** hai —
iske se aap koi bhi read-modify-write atomically kar sakte ho, bhale hardware me
uska direct instruction na ho. Har lock-free stack/queue/list ke andar ek CAS loop
hai. Iske do variants (`weak`/`strong`), spurious failure, aur "`expected` gets
overwritten" — yeh precise details matter karte hain.

---

## What CAS does

```cpp
bool compare_exchange_strong(T& expected, T desired, memory_order mo);
```

Atomically:
```
if (*this == expected) { *this = desired; return true; }
else                   { expected = *this; return false; }   // <-- expected is OVERWRITTEN
```

- **Success** (`*this == expected`): store `desired`, return `true`.
- **Failure** (`*this != expected`): **write the current value into `expected`**
  (so you can retry with a fresh read for free), return `false`.

x86: `lock cmpxchg`. `expected` in `rax`; on failure `rax` gets the current value.

---

## The CAS loop pattern

"Do an arbitrary RMW atomically":

```cpp
T old = a.load(std::memory_order_relaxed);
T next;
do {
    next = compute(old);                      // whatever transform you want
} while (!a.compare_exchange_weak(old, next,
                                 std::memory_order_release,   // on success
                                 std::memory_order_relaxed)); // on failure
// on exit: a was == old and is now == next, atomically
```

- Read → compute → try to swap.
- If another thread changed `a` in between, the CAS fails, **`old` is refreshed to
  the current value**, and you loop — recompute `next` from the new `old`.
- This gives you `fetch_max`, `fetch_mul`, "update field 3 of a small struct",
  pushing onto a lock-free stack — anything (`examples/02`).

---

## `weak` vs `strong`

| | `compare_exchange_weak` | `compare_exchange_strong` |
|---|---|---|
| Spurious failure? | **Yes** — may return `false` even when `*this == expected` | **No** |
| Why | LL/SC architectures (ARM, POWER, RISC-V): the "store-conditional" can fail due to an interrupt / cache event, not a real value change | strong retries internally to hide it |
| Use | **inside a loop** (you're going to retry anyway — weak is cheaper, no hidden retry) | **single try, no loop** ("try once, else give up"); or a loop where retrying is very expensive |
| x86 | no spurious failure — `weak` and `strong` compile the same | same |

```cpp
// LOOP -> weak
while (!a.compare_exchange_weak(exp, des, mo)) { des = f(exp); }

// SINGLE TRY -> strong
T exp = expectedValue;
if (a.compare_exchange_strong(exp, des)) { /* got it */ } else { /* someone else has it */ }
```

**Rule: in a loop, use `weak`. Outside a loop, use `strong`.**

---

## Success vs failure memory order

```cpp
a.compare_exchange_weak(expected, desired,
                        std::memory_order_release,   // applied IF the CAS succeeds
                        std::memory_order_relaxed);  // applied IF the CAS fails (a load)
```

- **Success order** — the order for the RMW when it goes through (e.g. `release`
  to publish, `acq_rel` if you also read shared data guarded by it).
- **Failure order** — the order for the *load* that reads the current value. Must
  be **no stronger** than success, and can't be `release`/`acq_rel` (a failed CAS
  is just a load).
- Common: `(release, relaxed)` for publish; `(acq_rel, acquire)` for
  "read-and-maybe-update shared state"; `(relaxed, relaxed)` for a lock-free
  counter/stack where ordering is handled elsewhere.
- Single-arg form `compare_exchange_weak(exp, des, mo)` uses `mo` for success and
  a derived (weaker) order for failure.

---

## The `expected` gotcha

```cpp
int expected = 10;
bool ok = a.compare_exchange_strong(expected, 20);
// if a was 10:  ok == true,  a == 20,  expected still 10
// if a was 15:  ok == false, a unchanged, expected == 15  (!)
```

After a failed CAS, **`expected` holds the current value** — great for a loop
(free re-read), but a trap if you reuse `expected` afterward without resetting:

```cpp
int expected = false_flag_value;
while (!flag.compare_exchange_weak(expected, true)) {
    expected = false_flag_value;   // ⚠️ MUST reset — CAS set it to `true` on failure
}
```

`examples/02` spinlock: after a failed CAS, `expected` becomes `true`, so you
reset it to `false` before retrying.

---

## CAS-loop pitfalls

### Livelock under high contention
N threads all CAS-looping on the same word → each other's success makes the others
fail → lots of retries, little progress. It's still *lock-free* (some thread always
progresses) but throughput craters. Mitigations: back off (exponential + jitter),
combine (batch updates), or use `fetch_add` (one hardware op, no retry) when the
operation allows it.

### ABA (file 15)
CAS checks **value equality**, not "nothing changed". If the value went `A → B →
A` between your read and your CAS, the CAS succeeds on a stale premise. Fix: tagged
pointers (a version counter in the word — `examples/07`), or hazard pointers /
epochs (folder 28).

### `fetch_add` beats a CAS loop when it fits
`counter.fetch_add(1)` is one `lock xadd` — no retry, no livelock. Only CAS-loop
when there's no direct `fetch_*` (max, mul, struct update, conditional store).

---

## > **HFT relevance**
> - **CAS loop is the core of every hand-rolled lock-free structure** — SPSC/MPSC
>   queue enqueue, Treiber stack push/pop, a lock-free free-list (folder 28). Get
>   `weak`-in-a-loop and the memory orders right.
> - **Prefer `fetch_add` over a CAS loop** wherever the operation is "add N" —
>   ring-buffer index bump, slot allocation, sequence numbers. One instruction, no
>   contention-driven retry storm.
> - **Bounded CAS retries + backoff** on the rare compound update (e.g. an MPMC
>   producer claiming a slot in a Vyukov queue) — measure the retry rate under
>   load; if it's high, the design is contended and needs sharding.
> - **`(release, relaxed)`** for publish-via-CAS; **`(acq_rel, acquire)`** when the
>   CAS both reads guarded data and publishes.
> - **Always ABA-proof** a CAS on a reused pointer/index (tagged word) — a
>   lock-free structure that "works in testing" and corrupts under load is the
>   classic CAS-loop bug (`examples/07`, file 15).

---

## Hands-on

```bash
./build.ps1 27-ATOMICS-MEMORY-MODEL/examples/02_cas_loop.cpp
```

CAS-loop increment, CAS-loop atomic max, `weak` vs `strong` single-try, a CAS
spinlock. Then:
- `g++ -O2 -S` a `compare_exchange_weak` → `lock cmpxchg`, and note there's no
  retry loop for it on x86 (weak == strong).
- 8 threads CAS-looping `+1` vs 8 threads `fetch_add(1)` — time both; the CAS-loop
  is slower (retries under contention).
- Add a print of the retry count in the CAS-loop increment under 8 threads.

---

## ⚠️ Traps

### Trap 1 — `weak` outside a loop
A spurious failure (ARM) makes your single-try "fail" for no reason. Use `strong`
for a single try.

### Trap 2 — not resetting `expected` after a failed CAS
CAS wrote the current value into `expected`. In a spinlock loop you must reset it
to the "unlocked" value before retrying.

### Trap 3 — failure order stronger than success order
Illegal. Failure is a load — `relaxed`/`acquire`/`seq_cst` only, and ≤ success.

### Trap 4 — CAS loop where `fetch_add` would do
`while (!a.compare_exchange_weak(cur, cur + 1)) {}` — just `a.fetch_add(1)`. One
op, no retries.

### Trap 5 — ignoring ABA on a reused pointer/index
`compare_exchange_strong(head, head->next)` on a Treiber stack without a version
tag → ABA corruption (`examples/07`, file 15).

### Trap 6 — high-contention CAS loop assumed "fine because lock-free"
Lock-free ≠ fast. A hot CAS loop can spend most cycles retrying. Measure; `fetch_*`
or shard.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`compare_exchange` doesn't touch `expected` on failure" | It **overwrites** `expected` with the current value (free re-read) |
| "`weak` and `strong` are interchangeable" | `weak` can fail spuriously (ARM); loop → weak, single try → strong |
| "one memory order for CAS" | Two — success and failure (failure is a load, ≤ success, no release) |
| "CAS is atomic so ABA can't happen" | CAS checks value equality; A→B→A defeats it (file 15) |
| "CAS loop = lock-free = fast" | Lock-free; under contention it can be a retry storm — measure |
| "always CAS-loop RMWs" | Use `fetch_add`/`fetch_or` when they fit — no retries |

---

## Exercises

1. **weak or strong:** (a) inside a push loop on a lock-free stack; (b) "try to
   claim this slot once, else move on"; (c) a loop where recomputing `next` costs
   a syscall.

   <details><summary>Answer</summary>

   (a) `weak` — you retry anyway, weak is cheaper. (b) `strong` — single try, no
   spurious failure. (c) `strong` inside the loop — a spurious failure would waste
   the expensive recompute, so pay for strong's internal retry instead.
   </details>

2. **Trace `expected`:** `std::atomic<int> a{7}; int e = 5; bool ok =
   a.compare_exchange_strong(e, 9);` — `ok`, `a`, `e` after?

   <details><summary>Answer</summary>

   `a` was 7 ≠ 5 → `ok == false`, `a` unchanged (7), `e` overwritten to 7. A
   second `compare_exchange_strong(e /*==7*/, 9)` would now succeed.
   </details>

3. **Atomic clamp-add:** add `n` to `std::atomic<int> a` but never let it exceed
   `LIMIT`. CAS loop.

   <details><summary>Answer</summary>

   ```cpp
   int cur = a.load(std::memory_order_relaxed), next;
   do {
       next = cur + n;
       if (next > LIMIT) next = LIMIT;
   } while (!a.compare_exchange_weak(cur, next, std::memory_order_relaxed));
   ```
   </details>

4. **Retry storm:** 16 threads run `while (!a.compare_exchange_weak(cur, cur + 1))
   { }` on one atomic. What happens vs `a.fetch_add(1)`?

   <details><summary>Answer</summary>

   The CAS loop: each successful increment invalidates every other thread's `cur`,
   so ~15 of 16 threads fail and retry each round — huge retry rate, throughput
   collapses (though it's still lock-free — someone always wins). `fetch_add(1)`
   is one `lock xadd` per thread — no retry, just the cache-line contention
   (~tens of ns), several × faster.
   </details>

5. **Memory orders:** you CAS a lock-free queue's tail pointer to publish a new
   node whose fields you just wrote. Which success/failure orders?

   <details><summary>Answer</summary>

   Success: `std::memory_order_release` (so a consumer that acquires the tail sees
   the node's fields). Failure: `std::memory_order_relaxed` (just a re-read; you'll
   retry). If the enqueue also needs to *read* other shared state guarded by the
   tail, use `acq_rel` / `acquire`.
   </details>

---

## Interview questions

1. `compare_exchange` — success aur failure pe exactly kya hota (`expected` ka kya)?
2. `weak` vs `strong` — spurious failure, kab kaunsa?
3. CAS-loop pattern — read/compute/try/retry — kis ke liye (arbitrary RMW).
4. CAS ke do memory orders — failure order pe kya restriction?
5. `expected` reset karna kab zaroori (spinlock loop)?
6. CAS loop vs `fetch_add` — kab kaunsa, contention pe kya?
7. CAS aur ABA — kyun CAS ABA se nahi bachata?

---

## Next
→ [`06-why-memory-ordering.md`](06-why-memory-ordering.md)

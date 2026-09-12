# 13 — `std::atomic_ref` (C++20)

## Prerequisites
- `03-std-atomic.md`, `08-acquire-release.md`

## Yeh topic abhi kyun
`std::atomic<T>` **owns** apna storage. Par kabhi-kabhi aap ke paas ek plain
object hota hai (ek `int` in an array, a struct field, a buffer element) jise
aap **kuch der ke liye** atomically access karna chahte ho — bina uska type
badle. `std::atomic_ref<T>` yahi karta hai: ek existing non-atomic object pe
ek atomic "view".

---

## The problem it solves

```cpp
// You have a big array of plain ints, filled single-threaded.
std::vector<int> hist(1024, 0);

// Now a phase where many threads bump buckets concurrently.
// Option A: make it vector<atomic<int>> — but then the single-threaded fill,
//           copies, serialization, and APIs all have to deal with atomic<int>.
// Option B: atomic_ref — treat individual elements as atomic *only during* the
//           concurrent phase.
std::for_each(std::execution::par, work.begin(), work.end(), [&](auto& w) {
    std::atomic_ref<int> bucket{hist[w.index]};
    bucket.fetch_add(1, std::memory_order_relaxed);
});
// After: hist is a plain vector<int> again. Exact counts.
```

`std::atomic<T>` changes the **type** of the object forever. `std::atomic_ref<T>`
is a **temporary atomic view** over an object that stays type `T`.

---

## Rules

```cpp
#include <atomic>

int x = 0;
{
    std::atomic_ref<int> r{x};        // r references x
    r.store(5, std::memory_order_release);
    int v = r.load(std::memory_order_acquire);
    r.fetch_add(1, std::memory_order_relaxed);
}   // r gone; x is a plain int again, value 6
```

- **Same API as `std::atomic<T>`** — `load`, `store`, `exchange`,
  `compare_exchange_*`, `fetch_*`, `wait`/`notify`, `is_lock_free`,
  `is_always_lock_free`.
- **`T` must be trivially copyable.**
- **Lifetime:** the referenced object must outlive **every** `atomic_ref` to it.
- **Alignment:** the object must be suitably aligned —
  `std::atomic_ref<T>::required_alignment` (may be **stricter** than `alignof(T)`;
  e.g. an `atomic_ref<int64_t>` may need 8-byte alignment even if the platform
  `alignof(long long)` in a packed struct is less). Misaligned → UB.
- **All access must go through `atomic_ref` while any `atomic_ref` to it exists.**
  Mixing a plain `x = 7;` write with a concurrent `atomic_ref` access to the same
  `x` is a **data race** — the "atomic view" only counts if *everyone* uses it.
- `atomic_ref` is **copyable** (all copies reference the same object); it does not
  own anything.
- Const-ness: `std::atomic_ref<const T>` gives you atomic loads only.

---

## `atomic_ref` vs `atomic` vs `volatile`

| | `std::atomic<T>` | `std::atomic_ref<T>` | `volatile T` |
|---|---|---|---|
| Atomicity | yes | yes (while ref alive, if all access via ref) | **no** |
| Memory ordering control | yes | yes | no |
| Changes the object's type | yes (permanent) | no (temporary view) | yes (qualifier) |
| Works on an existing plain object | no (must construct as atomic) | **yes** | n/a |
| Use for cross-thread sync | ✅ | ✅ | ❌ never |

`volatile` is for memory-mapped I/O / `setjmp` / signal `sig_atomic_t` — **not**
threading. `atomic_ref` is the tool when you need atomicity on an object you
can't or don't want to declare `atomic`.

---

## When to use it

- **A field in a struct that's mostly used single-threaded** but has one
  concurrent-update phase (a refcount you don't want to templatize the struct
  over, a "dirty" flag).
- **Elements of a large plain array/buffer** updated concurrently (histogram,
  bitmap, per-slot sequence numbers) — keep the container `T`, view elements
  atomically.
- **Interop** — an API hands you `int*` / `T&` and you need one atomic operation
  on it without owning it.
- **Parallel algorithms** (`std::execution::par`) that need an atomic accumulator
  over caller-owned storage.

Don't use it as a general replacement for `std::atomic` when you *do* own the
object and it's *always* shared — just declare `std::atomic<T>`.

---

## Gotchas

### 1. `required_alignment` > `alignof(T)`
```cpp
struct Packed { char c; int64_t v; } __attribute__((packed));
Packed p;
std::atomic_ref<int64_t> r{p.v};   // ⚠️ p.v likely mis-aligned → UB
static_assert(alignof(Packed) >= std::atomic_ref<int64_t>::required_alignment); // fails
```
Check `required_alignment`; `alignas` the storage if needed.

### 2. Escaping the object's lifetime
```cpp
std::atomic_ref<int> make() { int local = 0; return std::atomic_ref<int>{local}; } // ⚠️ dangling
```

### 3. Mixed access
While an `atomic_ref` to `x` is live and another thread uses it, **this** thread
must not touch `x` non-atomically. All-or-nothing.

### 4. Not lock-free for big `T`
Same as `std::atomic` — `atomic_ref<Big>` may use a lock table. Check
`is_always_lock_free`.

---

## > **HFT relevance**
> - **Per-slot sequence numbers in a ring buffer** stored inline in a plain
>   `Slot[]` — `std::atomic_ref<uint32_t>{slot.seq}` for the publish/acquire,
>   while the payload stays plain POD you `memcpy`. No need to wrap the whole
>   `Slot` in `atomic`.
> - **A hot struct with one atomic field** — e.g. an `Order` that's built and
>   consumed single-threaded but whose `state` is flipped by a cancel thread:
>   `atomic_ref<uint8_t>{order.state}` at the two concurrent sites, plain
>   elsewhere. Keeps `Order` trivially copyable and cheap.
> - **Parallel warm-up / analytics** over caller-owned market-data arrays —
>   `atomic_ref` accumulators without re-typing the arrays the hot path reads
>   plainly.
> - **Watch `required_alignment`** in packed wire structs — an `atomic_ref` over a
>   misaligned field is UB and on x86 may also straddle a cache line (tears +
>   `lock` split-lock penalty).

---

## Hands-on

```bash
# no dedicated example file; quick check:
cat > /tmp/aref.cpp <<'EOF'
#include <atomic>
#include <thread>
#include <vector>
#include <iostream>
int main() {
    std::vector<int> hist(8, 0);
    auto work = [&](int b){ for (int i = 0; i < 100000; ++i)
        std::atomic_ref<int>{hist[b]}.fetch_add(1, std::memory_order_relaxed); };
    std::vector<std::thread> ts;
    for (int t = 0; t < 8; ++t) ts.emplace_back(work, t % 8);
    for (auto& x : ts) x.join();
    for (int v : hist) std::cout << v << ' ';           // each 100000
    std::cout << "\naligned? " << std::atomic_ref<int>::required_alignment << '\n';
}
EOF
g++ -std=c++20 -O2 -Wall -Wextra /tmp/aref.cpp -o /tmp/aref && /tmp/aref
```
Then: try `std::atomic_ref<long long>` over a field of a `#pragma pack(1)` struct
and watch the `static_assert(alignof(...) >= required_alignment)` fail.

---

## ⚠️ Traps

### Trap 1 — mixed atomic/non-atomic access to the same object
All access must be via `atomic_ref` while any `atomic_ref` to it is alive.
Otherwise it's a data race.

### Trap 2 — `required_alignment` ignored
May exceed `alignof(T)` (packed structs, `char` buffers). Misaligned → UB.

### Trap 3 — referenced object outlived by the ref
`atomic_ref` doesn't own or extend anything. Dangling ref = UB.

### Trap 4 — using it where `std::atomic` fits
If you own the object and it's always shared, just declare `std::atomic<T>`.
`atomic_ref` is for objects you can't re-type.

### Trap 5 — assuming lock-free for large `T`
`atomic_ref<BigStruct>` may take a lock. Check `is_always_lock_free`.

### Trap 6 — `volatile` instead
`volatile` gives neither atomicity nor ordering. Never for threads.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`atomic_ref` changes the object to atomic type" | No — temporary atomic *view*; object stays type `T` |
| "some threads can use `atomic_ref`, others plain access" | All access via `atomic_ref` while any ref is live, or it's a race |
| "alignment is just `alignof(T)`" | Check `required_alignment` — can be stricter |
| "`atomic_ref` extends the object's lifetime" | It owns nothing; object must outlive all refs |
| "`atomic_ref` is always lock-free" | Same size limits as `std::atomic` |
| "`volatile int&` is the same idea" | `volatile` ≠ atomic, ≠ ordering — unrelated |

---

## Exercises

1. **`atomic` or `atomic_ref`:** (a) a shared stop flag you declare yourself;
   (b) one element of a caller-provided `int[]` bumped by many threads in one
   phase; (c) a `state` byte inside an otherwise plain trivially-copyable `Order`.

   <details><summary>Answer</summary>

   (a) `std::atomic<bool>` — you own it, always shared. (b) `std::atomic_ref<int>`
   over the element — keep the array plain. (c) `std::atomic_ref<uint8_t>` at the
   concurrent sites — keeps `Order` trivially copyable / memcpy-able.
   </details>

2. **Race?** thread A: `std::atomic_ref<int>{x}.store(1, release);` thread B:
   `int v = x;` (plain read), no `atomic_ref`.

   <details><summary>Answer</summary>

   Data race → UB. B's plain read of `x` conflicts with A's atomic store, and B
   isn't going through an `atomic_ref`, so there's no atomic-vs-atomic exemption.
   B must also use `std::atomic_ref<int>{x}.load(acquire)`.
   </details>

3. **Alignment:** `struct __attribute__((packed)) W { uint8_t tag; uint64_t seq; };`
   — is `std::atomic_ref<uint64_t>{w.seq}` OK?

   <details><summary>Answer</summary>

   Almost certainly not — `w.seq` is at offset 1, so misaligned;
   `atomic_ref<uint64_t>::required_alignment` is 8. UB (and on x86 a split-lock
   penalty even if it "works"). Fix: don't pack, or add explicit padding /
   `alignas`.
   </details>

4. **Lifetime:** why is returning `std::atomic_ref<int>` to a local `int` broken?

   <details><summary>Answer</summary>

   `atomic_ref` stores a pointer to the object but doesn't own or extend it. The
   local `int` dies at function return; the returned ref dangles → UB on any use.
   </details>

5. **Why not just `vector<atomic<int>>`:** one advantage of `atomic_ref` over
   making the whole container atomic.

   <details><summary>Answer</summary>

   The container stays `vector<int>` — trivially copyable, serializable,
   movable, usable with plain-`int` APIs, and the single-threaded fill/read paths
   pay nothing. Atomicity is scoped to the one concurrent phase.
   </details>

---

## Interview questions

1. `std::atomic_ref<T>` `std::atomic<T>` se kaise alag — ownership, type?
2. "All access via the ref" rule — kyun, todne pe kya?
3. `required_alignment` — `alignof(T)` se strict kab, kahan bite karta (packed)?
4. `atomic_ref` lifetime constraint — object kab tak zinda rehna chahiye?
5. Kaunse real cases mein `atomic_ref` right choice hai (array element, struct field, interop)?
6. `atomic_ref` vs `volatile` — kaunsa threading ke liye, kyun?
7. Bade `T` pe `atomic_ref` lock-free hai? Kaise check karein?

---

## Next
→ [`14-lock-free-definitions.md`](14-lock-free-definitions.md)

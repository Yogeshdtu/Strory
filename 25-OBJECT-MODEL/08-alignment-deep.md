# 08 — Alignment deep: `alignas`, over-aligned types, `std::align`

## Prerequisites
- `07-object-representation.md`, `14-MEMORY`, `24-COMPILATION-LINKING` file 06
- `11-STRUCTS` (padding)

## Yeh topic abhi kyun
Alignment = "ek object ka address kis number ka multiple hona chahiye." Padding
(file 07) isi se aata hai. Aur HFT mein: **cache-line alignment** (false sharing,
folder 26/32), **SIMD alignment** (`__m256` needs 32), **DMA / hugepage
alignment**, aur `alignas` + placement new + aligned allocation — sab yahin se.

---

## Basics

```cpp
alignof(char)    // 1
alignof(int)     // 4  (usually)
alignof(double)  // 8  (usually)
alignof(void*)   // 8  (x86-64)
alignof(std::max_align_t)   // 16 (usually) — "biggest alignment any scalar needs"
```

- Har complete type ka ek **alignment requirement** — `alignof(T)`, hamesha a
  power of 2.
- Ek `T` object ka address `alignof(T)` ka multiple hona **chahiye** — warna
  **misaligned access = UB** (x86 pe aksar "kaam karta" but slow / SIMD pe crashes;
  ARM pe hard fault).
- `alignof(T)` divides `sizeof(T)` (tail padding — file 07).
- Struct ka alignment = uske members ke max alignment.

```cpp
struct S { char c; double d; };   // alignof(S) == 8 (from double), sizeof == 16
```

---

## `alignas` — increase alignment

```cpp
alignas(16) float vec4[4];              // 16-byte aligned (SIMD)
alignas(64) std::atomic<long> counter;  // own cache line (false-sharing fix)

struct alignas(64) CacheLinePadded {
    std::atomic<std::uint64_t> value;   // rest of the 64 bytes is padding
};

alignas(4096) unsigned char page_buf[4096];   // page-aligned
```

- `alignas(N)` — N a power of 2, `>=` the type's natural alignment (you can't
  *reduce* alignment with `alignas`).
- `alignas(T)` — same as `alignas(alignof(T))`.
- On a member → the member (and often the whole struct) gets that alignment.
- On a `struct` definition → every object of that type is over-aligned, and
  `sizeof` grows to a multiple of `alignas` (so arrays stay aligned).

```cpp
struct alignas(64) X { int a; };
sizeof(X);    // 64 (not 4) — tail padding to keep X[] elements 64-aligned
```

---

## Over-aligned types & allocation

An **over-aligned type** = `alignof(T) > alignof(std::max_align_t)` (usually
`> 16`). E.g. `alignas(64)` structs, `__m256` (32), page-aligned buffers.

| Allocation | Handles over-alignment? |
|---|---|
| `new T` / `new T[n]` | **Yes** (C++17) — calls the aligned `operator new(size, align_val_t)` |
| `std::malloc` / `std::calloc` | **No** — only `max_align_t`. `alignas(64)` object in `malloc`'d memory = UB |
| `std::aligned_alloc(align, size)` | Yes (C11/C++17) — `size` must be a multiple of `align` |
| `posix_memalign` / `_aligned_malloc` (Win) | Yes (platform) |
| `std::allocator<T>` | Yes (respects `alignof(T)`) |
| `operator new(size, std::align_val_t{64})` | Yes — explicit |
| Automatic (stack) `alignas(64) T x;` | Yes — compiler aligns the frame slot |

```cpp
// C++17: this just works for alignas(64) T
T* p = new T;
delete p;

// malloc does NOT:
T* bad = static_cast<T*>(std::malloc(sizeof(T)));   // ⚠️ not 64-aligned -> UB on use
```

`std::pmr` resources and custom arenas must honor the requested alignment —
`std::pmr::monotonic_buffer_resource` does; a naive bump allocator must round the
cursor up.

---

## `std::align` — carve an aligned block out of a buffer

```cpp
#include <memory>

std::byte storage[1024];
void*        ptr  = storage;
std::size_t  space = sizeof storage;

// want a 64-aligned region of 200 bytes inside `storage`
void* aligned = std::align(/*alignment=*/64, /*size=*/200, /*ptr, space=*/ptr, space);
if (aligned) {
    // `aligned` is 64-aligned, `space` bytes remain after it
    // ptr advanced, space reduced
}
```

`std::align` bumps `ptr` up to the next `alignment` boundary if there's room,
updates `ptr`/`space`, returns the aligned pointer (or `nullptr` if it doesn't
fit). This is the primitive inside arena allocators.

Manual version:
```cpp
std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(p);
std::uintptr_t aligned = (addr + (A - 1)) & ~(std::uintptr_t{A} - 1);   // round up to A (power of 2)
```

---

## `std::hardware_destructive_interference_size` (C++17)

The "no false sharing if you're this far apart" size — usually 64, but can differ
(some CPUs prefetch pairs of lines → 128). `std::hardware_constructive_interference_size`
= "pack within this to share a line."

⚠️ GCC's `-Winterference-size`: using these across a **ABI boundary** (as a struct
member alignment in a header shared between TUs built with different `-mtune`) can
mismatch. For a single binary it's fine; many codebases just use a `constexpr
std::size_t kCacheLine = 64;` and `static_assert` it against
`hardware_destructive_interference_size`.

---

## Andar kya hota hai

- **Aligned load/store**: on x86-64, most instructions tolerate misalignment with
  a penalty (a split load crossing a cache line ≈ 2x). SIMD `movaps`/`vmovaps`
  **fault** on misalignment; `movups` don't (but were historically slower —
  now ≈ equal when actually aligned).
- **`alignas` on a local** → compiler over-aligns the stack frame (may add a
  `and rsp, -64` + a saved base pointer).
- **`alignas` on a global** → the linker places it in a suitably-aligned section
  slot.
- **C++17 aligned new** → `__cxa` calls `operator new(std::size_t, std::align_val_t)`;
  the default impl forwards to `aligned_alloc` / `posix_memalign`.
- ASan/valgrind can catch some misaligned accesses; UBSan `-fsanitize=alignment`
  catches misaligned pointer use directly.

---

## > **HFT relevance**
> - **Cache-line alignment to kill false sharing** — `alignas(64)` on per-thread
>   counters / queue head & tail / hot mutable shared state (folder 26 file 16,
>   `examples/08_false_sharing.cpp` there — measured speedup). Pad structs so two
>   hot fields written by different threads land on different lines.
> - **SIMD** — `alignas(32)` / `alignas(64)` on arrays you'll process with
>   AVX2/AVX-512; use aligned loads. (Or let the compiler auto-vectorize with
>   `movups` and eat the ~0 cost when actually aligned.)
> - **Arena allocators must align** — round the bump cursor up to `alignof(T)`
>   (or 64 for cache-line-hot objects). `std::align` is the primitive.
> - **`malloc` for an over-aligned type is UB** — use `new` (C++17), aligned
>   allocators, or `std::aligned_alloc`.
> - **Hugepages / DMA buffers** — `alignas(2*1024*1024)` or `mmap` + `MAP_HUGETLB`;
>   the object model just needs the storage to satisfy `alignof(T)`.

---

## Hands-on

```bash
./build.ps1 26-CONCURRENCY/examples/08_false_sharing.cpp   # (folder 26) alignas(64) measured
```

- `alignof` / `sizeof` for `int`, `double`, `void*`, `std::max_align_t`,
  `struct{char;double;}`, `struct alignas(64){int;}`.
- `std::align` a 64-aligned 128-byte region out of a `std::byte[512]`; print the
  original vs aligned address and the remaining `space`.
- Manual round-up: `(addr + 63) & ~std::uintptr_t{63}` — verify it matches
  `std::align`.

---

## ⚠️ Traps

### Trap 1 — `malloc` for an `alignas(64)` type
```cpp
alignas(64) struct Slot { std::atomic<long> v; };
auto* s = static_cast<Slot*>(std::malloc(sizeof(Slot)));   // ⚠️ not 64-aligned -> UB
auto* s2 = new Slot;                                        // ✅ C++17 aligned new
```

### Trap 2 — `alignas` smaller than natural alignment
```cpp
alignas(1) int x;   // ⚠️ error / ignored — can't weaken alignment
```

### Trap 3 — `alignas(64)` struct, then `sizeof` surprise
`sizeof` becomes a multiple of 64 → a `Slot[1000]` is 64 KB, not 4 KB. Intended
for cache-line isolation, but budget the memory.

### Trap 4 — forgetting `size` must be a multiple of `alignment` for `aligned_alloc`
`std::aligned_alloc(64, 100)` — UB (100 not a multiple of 64). Round `size` up.

### Trap 5 — misaligned pointer from pointer arithmetic on `char*`
```cpp
int* ip = reinterpret_cast<int*>(char_buf + 1);   // ⚠️ misaligned -> UB on deref
```
Use `std::align` / round up, or `memcpy` in/out.

### Trap 6 — `hardware_destructive_interference_size` in a shared header
GCC `-Winterference-size` — its value can vary with `-mtune`. Use a fixed
`constexpr` 64 for struct layout; `static_assert` against the std value.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "misaligned access just works on x86" | Often, with a penalty; SIMD faults; ARM faults; it's UB regardless |
| "`malloc` returns memory aligned for anything" | Only up to `max_align_t` (16); over-aligned types → UB |
| "`alignas` can shrink alignment" | No — only increase (≥ natural) |
| "`alignas(64)` struct is still `sizeof` == fields" | `sizeof` rounds up to a multiple of 64 |
| "`new T` doesn't handle over-alignment" | C++17: it does (aligned `operator new`) |
| "cache line is always 64" | Usually; `hardware_destructive_interference_size` can be 128 |

---

## Exercises

1. **Predict:** `struct S { char c; alignas(16) int arr[4]; };` — `alignof(S)`,
   `sizeof(S)`, `offsetof(S, arr)`?

   <details><summary>Answer</summary>

   `alignof(S) == 16` (from the `alignas(16)` member). `arr` at offset 16 (15
   bytes padding after `c`). `sizeof(S) == 32` (16 for `c`+pad, 16 for `arr`;
   already a multiple of 16). 
   </details>

2. **Allocation choice:** you need 1000 `alignas(64) struct Counter { std::atomic<long>
   v; };`. Which allocators are valid?

   <details><summary>Answer</summary>

   `new Counter[1000]` (C++17 aligned new), `std::vector<Counter>` (its allocator
   respects `alignof`), `std::aligned_alloc(64, 64*1000)`, `operator new(64*1000,
   std::align_val_t{64})`. **Not** `std::malloc(64*1000)` (only 16-aligned).
   </details>

3. **`std::align`:** `std::byte buf[100]; void* p = buf; size_t space = 100;` — after
   `std::align(32, 40, p, space)`, if `buf` started at an address `≡ 8 (mod 32)`,
   what are `p` and `space` roughly?

   <details><summary>Answer</summary>

   `p` is bumped forward by 24 bytes to the next 32-boundary; `space` becomes `100
   - 24 = 76` (≥ 40, so it fits) and the function returns the aligned `p`. If it
   hadn't fit, it returns `nullptr` and leaves `p`/`space` unchanged.
   </details>

4. **Round-up formula:** align `p` (a `std::uintptr_t`) up to 4096. Expression?

   <details><summary>Answer</summary>

   `(p + 4095) & ~std::uintptr_t{4095}` — add `A-1`, then mask off the low bits.
   Works because 4096 is a power of two.
   </details>

5. **False sharing fix:** two threads each increment their own `long` in `long
   counters[2];`. Why slow, and the one-line fix?

   <details><summary>Answer</summary>

   `counters[0]` and `counters[1]` share a cache line → every increment on one
   invalidates the other core's copy (ping-pong). Fix: `struct alignas(64)
   Counter { long v; }; Counter counters[2];` — each on its own line. (folder 26
   `08_false_sharing.cpp` measures the speedup.)
   </details>

---

## Interview questions

1. Alignment kya hai, misaligned access ka kya (UB, penalty, SIMD fault)?
2. `alignof(std::max_align_t)` — kya matlab, `malloc` se kya rishta?
3. Over-aligned type — definition, kaunse allocators handle karte?
4. `alignas` — kya kar sakta, kya nahi (shrink)?
5. `alignas(64)` struct ka `sizeof` pe kya asar?
6. `std::align` — kya karta, arena allocator mein role?
7. `hardware_destructive_interference_size` — kya, `-Winterference-size` ka issue?

---

## Next
→ [`09-placement-new.md`](09-placement-new.md)

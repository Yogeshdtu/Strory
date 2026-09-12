# 05 — Padding aur alignment — deep dive

## Prerequisites
- [`01-what-is-a-struct.md`](01-what-is-a-struct.md), [`03-nested-structs.md`](03-nested-structs.md)
- `03-VARIABLES-DATA-TYPES/05-int-deep-dive.md` (bytes), `09-ARRAYS/10-array-performance.md` (cache lines)

## Yeh topic abhi kyun
**Yeh folder ka sabse important lesson hai.** Compiler struct members ke beech
**gaps (padding)** daalta hai taaki har member "aligned" ho. Result: `sizeof`
members ke jod se bada hota hai — aur **member order badalne se struct ka size
aadha** ho sakta hai. HFT mein yeh cache efficiency ka sabse bada single lever
hai.

---

## Alignment — har type ko ek "natural" boundary chahiye

```cpp
alignof(char)        // 1  -- kisi bhi address pe
alignof(std::int16_t)// 2  -- even address
alignof(std::int32_t)// 4  -- multiple of 4
alignof(std::int64_t)// 8
alignof(double)      // 8
alignof(void*)       // 8
```

**Rule: a `T` object must live at an address that's a multiple of `alignof(T)`.**
Misaligned access on x86 is slow (or a fault on ARM / for SIMD). The compiler
guarantees alignment by inserting padding.

---

## Padding — the compiler fills gaps

```cpp
struct Bad {
    char         a;    // offset 0        (1 byte)
    // >>> 7 bytes PADDING  (so `b` starts at a multiple of 8)
    double       b;    // offset 8        (8 bytes)
    char         c;    // offset 16       (1 byte)
    // >>> 3 bytes PADDING  (so `d` starts at a multiple of 4)
    std::int32_t d;    // offset 20       (4 bytes)
    // total 24 bytes  (16 useful, 8 padding)
};
```

Two rules:
1. **Each member** starts at an offset that's a multiple of its `alignof`.
   → padding *before* an under-aligned member.
2. **The struct's size** is a multiple of the struct's `alignof` (= max member
   alignment). → *tail* padding.

`sizeof(Bad)` = **24**, not 13. `alignof(Bad)` = 8 (its biggest member).

`examples/02_padding_demo.cpp` prints these offsets.

---

## Member reordering — same data, smaller struct

```cpp
struct Good {               // Bad ke SAME members, DESCENDING alignment order
    double       b;    // offset 0
    std::int32_t d;    // offset 8
    char         a;    // offset 12
    char         c;    // offset 13
    // >>> 2 bytes tail padding (size -> multiple of 8)
    // total 16 bytes
};
```

`sizeof(Good)` = **16** vs `sizeof(Bad)` = 24 — **8 bytes (33%) saved**, zero
behaviour change.

### Rule of thumb: **members in descending alignment order**

```
pointers / double / int64_t   (align 8)
   ↓
int32_t / float               (align 4)
   ↓
int16_t / short               (align 2)
   ↓
char / bool / int8_t          (align 1)
```

`examples/03_struct_optimization.cpp` — a realistic `Level` struct: **bad order
40 bytes (45% padding), good order 24 bytes (8%)**. That's 1.67x denser → 1.67x
fewer cache lines to scan a book.

---

## `alignof` / `alignas`

```cpp
alignof(T)                          // query: alignment requirement of T

struct alignas(64) CacheLineAligned {   // force 64-byte alignment (a whole cache line)
    std::atomic<long> counter;
    char pad[64 - sizeof(std::atomic<long>)];   // (or let the compiler tail-pad)
};

alignas(32) float simdRow[8];       // 32-byte aligned for AVX loads
```

**Over-align** (`alignas(64)`) to:
- Put a hot struct on its own cache line → avoid **false sharing** (two threads
  writing different fields in the same line → cache-line ping-pong, folder 28).
- Enable aligned SIMD loads.

**Under-align** — you can't below the natural requirement (that's what `#pragma
pack` does, file 06, with trade-offs).

---

## Finding padding

```cpp
// 1. static_assert -- lock the size
static_assert(sizeof(Level) == 24, "Level layout changed!");

// 2. -Wpadded -- compiler tells you where it padded
//    g++ -Wpadded file.cpp   ->  "warning: padding struct 'Level' with 4 bytes to align 'qty'"

// 3. offsetof -- inspect each member's byte offset
std::cout << offsetof(Level, qty);

// 4. `pahole` (Linux, from dwarves) -- prints the full layout with holes
//    pahole -C Level ./binary
```

⚠️ `-Wpadded` is noisy (warns on *every* struct with any padding) — use it as a
one-off audit, not a permanent flag.

---

## When you CAN'T reorder

Wire formats / ABI / API structs have a **fixed** member order (the protocol
dictates it). Then:
- **Explicit padding fields**: `char _reserved[3];` — document the gap.
- `#pragma pack` to remove padding entirely (file 06) — with the unaligned-access
  trade-off.
- A separate "layout struct" for the wire + a nicely-ordered struct for internal
  use, converting between them.

---

## Andar kya hota hai

- The compiler computes each member's offset: `offset = round_up(previous_end,
  alignof(member))`. Any gap is padding (bytes are indeterminate — often 0, don't
  rely on it).
- Struct `sizeof` = `round_up(last_member_end, alignof(struct))`.
- Nested struct → contributes its `sizeof` and its `alignof`.
- Array of struct → `stride = sizeof(struct)` (which already includes tail
  padding, so `arr[i]` stays aligned).
- Reading a padded struct's raw bytes shows the gaps; `memcmp` of two "equal"
  structs can differ in the padding → **don't `memcmp` structs for equality**.

> **HFT relevance:** Struct size directly controls how many records fit in a
> 64-byte cache line and in L1/L2. Reordering `Level` from 40 → 24 bytes means a
> 32-level book side goes from 1280 → 768 bytes — the difference between spilling
> L1 and not. HFT code reviews check struct layout; `static_assert(sizeof(...))`
> guards it in CI; hot shared structs are `alignas(64)` to kill false sharing.
> `-Wpadded` / `pahole` are standard audit tools. Folders 28, 32, 39.

---

## Hands-on

```bash
./build.ps1 11-STRUCTS/examples/02_padding_demo.cpp
./build.ps1 11-STRUCTS/examples/03_struct_optimization.cpp
g++ -std=c++20 -Wpadded 11-STRUCTS/examples/02_padding_demo.cpp -o /dev/null   # see the warnings
```

---

## ⚠️ Traps

### Trap 1 — `sizeof(struct)` == sum of members
```cpp
struct M { char c; int i; };  // sizeof 8, not 5
```

### Trap 2 — `memcmp` for struct equality
```cpp
if (std::memcmp(&a, &b, sizeof(a)) == 0) { }   // ⚠️ padding bytes may differ. Member-wise ==
```

### Trap 3 — mixed-alignment member order
```cpp
struct S { char a; double b; char c; int d; };   // ⚠️ 24 bytes. Reorder -> 16
```

### Trap 4 — `alignas` smaller than natural
```cpp
struct alignas(1) S { double d; };   // ❌ error -- can't under-align (use #pragma pack)
```

### Trap 5 — relying on padding byte values
```cpp
struct S { char c; int i; };  S s{};  // padding after `c` is indeterminate, not guaranteed 0
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`sizeof` = sum of member sizes" | + padding (alignment) |
| "Member order doesn't affect size" | Descending order minimizes padding — often 30–45% smaller |
| "`memcmp` compares structs correctly" | Padding bytes differ — member-wise `==` |
| "Padding bytes are zero" | Indeterminate |
| "`alignas` can shrink alignment" | Only increase — `#pragma pack` to remove padding |

---

## Exercises

1. **Compute by hand:** `struct S { char a; int b; char c; double d; short e; };`
   — write each offset, the padding, and `sizeof`. Verify with `offsetof` +
   `sizeof`.

2. **Reorder:** minimize `sizeof(S)` from exercise 1 by reordering. How many bytes
   saved?

3. **`-Wpadded`:** compile `02_padding_demo.cpp` with `-Wpadded`. Match each
   warning to an offset in the output.

4. **`static_assert` guard:** `struct Level { std::int64_t px, qty; std::int32_t
   n; char side; };` — `static_assert(sizeof(Level) == 24)`. Now add a `bool`
   after `side` — still 24? Add a `double` — assert fires?

5. **`alignas` / false sharing:** two `std::atomic<long>` counters in a struct vs
   each in its own `alignas(64)` struct. Two threads increment one each, 10M
   times. `-O2`, time. (Folder 28 preview.)

6. **Nested alignment:** `struct Inner { double d; };  struct Outer { char c;
   Inner in; char c2; };` — `sizeof(Outer)`? Reorder to minimize.

---

## Interview questions

1. Alignment kya hai? `alignof(double)` aur kyun?
2. Padding kyun aur kahan add hoti hai (2 rules)?
3. Member reordering se `sizeof` kaise ghatta hai — rule?
4. `alignas(64)` struct pe — 2 reasons (false sharing, SIMD)?
5. Struct equality ke liye `memcmp` kyun galat?
6. Wire-format struct jismein order fix ho — padding kaise handle karo?
7. `-Wpadded` / `pahole` — kya batate hain?

---

## Next
→ [`06-packed-structs.md`](06-packed-structs.md)

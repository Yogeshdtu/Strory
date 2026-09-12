# 04 — `std::string` internals: SSO

## Prerequisites
- [`02-std-string-basics.md`](02-std-string-basics.md)
- `08-FUNCTIONS/05-the-call-stack.md` (stack vs heap), `09-ARRAYS/10-array-performance.md`

## Yeh topic abhi kyun
`std::string` heap use karta hai — allocation, jo slow hai. **SSO (Small String
Optimization)** iska hal hai: chhoti strings `std::string` object ke **andar hi**
(inline) rehti hain — **koi allocation nahi**. HFT mein yeh ek core allocation-
avoidance mechanism hai, aur "`std::string` copy free hai kya?" ka jawab ismein hai.

---

## Layout (libstdc++, 64-bit)

```
sizeof(std::string) == 32 bytes:

  ┌──────────────────┬──────────────────┬────────────────────────────┐
  │ char*  _M_p      │ size_t _M_len    │ union { char buf[16];      │
  │  (8)             │  (8)             │         size_t capacity; } │
  │                 │                  │  (16)                      │
  └──────────────────┴──────────────────┴────────────────────────────┘
```

- **`_M_p`** — pointer to the char data
- **`_M_len`** — length (`size()` is just this — O(1))
- **union**: either a **16-byte inline buffer** (`buf[16]` = 15 chars + `'\0'`),
  OR (for long strings) the heap **capacity**

### Short string (≤ 15 chars)
```
  _M_p ─────► _M_local_buf   (points INSIDE itself)
  _M_len = 5
  _M_local_buf = "hello\0..."
```
**Data is inline. Zero heap allocation.** Construction, copy = just bytes in the
32-byte object.

### Long string (≥ 16 chars)
```
  _M_p ─────► [heap block]  "this is a longer string\0"
  _M_len = 23
  _M_allocated_capacity = 30
```
`_M_p` points to a heap allocation. Construction/copy → `new` + `memcpy`.

---

## Measure the threshold — `examples/03_sso_demo.cpp`

Global `operator new` override counts allocations while building a string of each
length. **Run at `-O0`** (at `-O2` GCC elides the allocation — see below).

```
len | heap allocations
----+-----------------
 15 | 0 alloc      <- inline (SSO)
 16 | 1 alloc  (17 bytes)   <- HEAP
 ...
```

**libstdc++: threshold = 15 chars.** (libc++: 22. MSVC: 15. It's
implementation-specific — never rely on the exact number.)

### `-O2` elides it entirely
At `-O2`, a short-lived local `std::string s(len, 'a')` whose only use is
`s[0]` → GCC omits the `operator new`/`operator delete` calls (C++14's explicit
**new-expression elision** allowance — the same rule that lets `make_shared`
fuse allocations). The demo then shows "0 allocations" for all lengths. That's a
real optimization worth knowing (folder 33) — but for *seeing* the SSO boundary
you need `-O0`.

---

## Capacity growth — geometric (~2x)

`examples/03_sso_demo.cpp` append loop (libstdc++):

```
  cap 15 -> 30 -> 60 -> 120 -> 240 -> 480 -> ...   (x2.00 each time)
```

Each growth: `new` bigger block + `memcpy` all + `delete` old. `n` appends
without `reserve` → ~`log₂(n)` reallocations, total copy work O(n) amortized.
**`reserve(finalSize)` → exactly one allocation** (file 07).

---

## Consequences

### Copy cost depends on length
```cpp
std::string a = "short";              // SSO
std::string b = a;                    // copy 32 bytes -- NO allocation

std::string c = "this is a long string here";   // heap
std::string d = c;                    // new + memcpy -- allocation
```

### Move is cheap either way
```cpp
std::string e = std::move(c);         // long: steal pointer, c -> empty
std::string f = std::move(a);         // short: copy 32 bytes, a -> "" (SSO can't "steal")
```

### `std::string` in containers
`std::vector<std::string>` of short strings → the char data is inline in each
`std::string` element, which is inline in the vector's buffer → surprisingly
cache-friendly. Long strings → each element's data is a separate heap block
(pointer chase).

---

## SSO vs `std::string_view`

| | SSO short `std::string` | `std::string_view` |
|---|---|---|
| Owns data | ✅ (inline) | ❌ (borrows) |
| Allocation | none (if ≤ threshold) | never |
| Can outlive source | ✅ (it IS the source) | ❌ (dangling risk) |
| Size | 32 bytes | 16 bytes |
| Mutable | ✅ | ❌ |

For a **short owned key** → SSO `std::string` (safe, no alloc). For a **read-only
view into existing data** → `std::string_view` (file 05).

---

## Andar kya hota hai

- `std::string s = "abc"` (short): the 3 chars + `'\0'` are `memcpy`'d into
  `_M_local_buf`, `_M_p` set to `&_M_local_buf`, `_M_len = 3`. No `operator new`.
- `s += "x"` while short and fits: write into `_M_local_buf`, bump `_M_len`.
- Crossing the threshold: `operator new`, `memcpy` inline → heap, switch `_M_p`.
- `-O2` may keep small strings entirely in registers / elide the object.

> **HFT relevance:** Short identifiers — instrument symbols ("AAPL", "ES"),
> venue codes, small keys — fit in SSO → passed around, stored in maps, copied,
> all **allocation-free**. This is why `std::string` (not `char*`) is acceptable
> for small keys even in latency-sensitive code. Anything that might exceed the
> threshold (log lines, JSON, order messages) is handled via `reserve`+reuse or
> `std::string_view`. Know your implementation's threshold for your key sizes.

---

## Hands-on

```bash
g++ -std=c++20 -O0 10-STRINGS/examples/03_sso_demo.cpp -o sso && ./sso
# then -O2 -- notice the allocations vanish (elision)
g++ -std=c++20 -O2 10-STRINGS/examples/03_sso_demo.cpp -o sso2 && ./sso2
```

---

## ⚠️ Traps

### Trap 1 — relying on the exact threshold
```cpp
static_assert(sizeof(std::string) == 32);   // ⚠️ implementation-specific
```

### Trap 2 — "`std::string` copy is always expensive"
Short strings copy 32 bytes, no allocation. It's length-dependent.

### Trap 3 — benchmarking SSO at `-O2`
Allocation elided → misleading. `-O0`/`-O1` to observe.

### Trap 4 — assuming `std::move` steals for short strings
SSO strings: move = copy 32 bytes + clear source. No pointer to steal.

### Trap 5 — appending in a loop without `reserve`
```cpp
for (int i = 0; i < 1e6; ++i) s += 'x';   // ⚠️ ~20 reallocations. s.reserve(1e6)
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Every `std::string` allocates" | Short strings (≤ ~15) use inline SSO |
| "SSO threshold is 15 everywhere" | libc++: 22, MSVC: 15 — implementation-specific |
| "`std::string` copy cost is fixed" | Depends on length (SSO vs heap) |
| "`std::move` is always O(1)" | For SSO strings it copies 32 bytes |
| "SSO demo works at any `-O`" | `-O2` elides — use `-O0`/`-O1` |

---

## Exercises

1. **Find your threshold:** run `03_sso_demo.cpp` at `-O0`. First length with a
   heap allocation? `sizeof(std::string)` on your machine?

2. **Elision:** run it at `-O2`. What changes? Why (C++14 allowance)?

3. **Copy cost:** benchmark copying a `"short"` string vs a 40-char string, 10M
   times, `-O0` (so allocations aren't elided). Ratio?

4. **Capacity growth:** log `capacity()` after each `push_back` up to 1000 chars.
   Growth factor? Number of reallocations?

5. **`reserve`:** same append loop with `s.reserve(1000)` first — how many
   reallocations now (instrument via `operator new`)?

6. **Container locality:** `std::vector<std::string>` of 100000 short vs long
   strings — sum all `.size()`. `-O2`, time. Explain the difference.

---

## Interview questions

1. SSO kya hai? `std::string` ke andar layout?
2. libstdc++ ka SSO threshold? Yeh portable hai?
3. Short vs long `std::string` — copy aur move ki cost?
4. Capacity growth pattern? `reserve` kyun?
5. SSO string ka `std::move` — kya hota hai?
6. `-O2` pe SSO benchmark misleading kyun ho sakta hai (elision)?

---

## Next
→ [`05-string-view.md`](05-string-view.md)

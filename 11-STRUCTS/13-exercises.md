# 13 — Folder 11 Revision + Exercises

## Prerequisites
Lessons 01–12 aur saare 8 examples chalaye hue (`04_aos_vs_soa` `-O2` pe).

---

## PART A — Concept check

1. Struct kya hai? `sizeof(struct)` kaise decide hota hai?
2. Struct copy / assignment — kya karta hai?
3. Struct pass by value vs `const&` — kab kaunsa? Small-struct exception?
4. Aggregate init — order, partial-init behaviour?
5. `S s;` vs `S s{};` for a struct with `int` members?
6. Default member initializer — storage add karta hai?
7. Designated initializers (C++20) — rules?
8. Nested struct member — pointer ya inline? Access cost?
9. Alignment kya hai? Padding kyun aur kahan?
10. Member reordering se `sizeof` kaise ghatta — rule?
11. `alignas(64)` — 2 reasons?
12. Struct equality ke liye `memcmp` kyun galat?
13. `#pragma pack(1)` — `sizeof`/`alignof` pe asar, trade-off?
14. Packed struct member ka `&` — problem?
15. `reinterpret_cast<Msg*>(buf)` vs `memcpy` — kyun `memcpy`?
16. AoS vs SoA — kab kaunsa? Cache-line math?
17. Union — members kaise? Active member rule?
18. Safe type punning — union nahi to kya?
19. `std::variant` vs union — 3 differences?
20. `std::visit` exhaustiveness — `switch` se fark?
21. `std::variant` vs `virtual` — closed vs open set?
22. `enum` vs `enum class` — 3 differences?
23. `enum class` underlying type — default, explicit kab?
24. Bitfields wire formats ke liye kyun unsafe?
25. `struct` vs `class` — technical fark (sirf ek)?

---

## PART B — Output prediction

### B1
```cpp
struct S { char a; int b; };
std::cout << sizeof(S) << " " << offsetof(S, b);
```
<details><summary>Answer</summary>`8 4` — 3 bytes padding after `a` so `b` is 4-aligned.</details>

### B2
```cpp
struct P { int x = 1, y = 2; };
P a; P b{10};
std::cout << a.x << a.y << " " << b.x << b.y;
```
<details><summary>Answer</summary>`12 102` — DMI defaults; `b` overrides `x` only.</details>

### B3
```cpp
struct Good { double d; int i; char c; };
struct Bad  { char c; double d; int i; };
std::cout << sizeof(Good) << " " << sizeof(Bad);
```
<details><summary>Answer</summary>`16 24` — Good: 8+4+1+3pad. Bad: 1+7pad+8+4+4pad.</details>

### B4
```cpp
union U { std::int32_t i; float f; };
U u; u.i = 0x3f800000;
std::cout << u.f;   // (reading inactive member -- UB, but on GCC/Clang:)
```
<details><summary>Answer</summary>`1` — `0x3f800000` is the IEEE-754 bit pattern for `1.0f`. (Technically UB; `std::bit_cast` is the correct way.)</details>

### B5
```cpp
enum class Side : std::uint8_t { Buy = 1, Sell = 2 };
std::cout << sizeof(Side) << " " << static_cast<int>(Side::Sell);
```
<details><summary>Answer</summary>`1 2`</details>

### B6
```cpp
std::variant<int, std::string> v = 42;
std::cout << v.index() << " ";
v = "hi";
std::cout << v.index() << " " << std::holds_alternative<std::string>(v);
```
<details><summary>Answer</summary>`0 1 1`</details>

### B7
```cpp
#pragma pack(push, 1)
struct M { std::uint8_t t; std::uint32_t v; };
#pragma pack(pop)
std::cout << sizeof(M) << " " << offsetof(M, v);
```
<details><summary>Answer</summary>`5 1` — packed: no padding, `v` at offset 1.</details>

### B8
```cpp
struct A { int x; };
class  B { public: int x; };
std::cout << std::boolalpha << (sizeof(A) == sizeof(B));
```
<details><summary>Answer</summary>`true` — identical layout; only default access differs.</details>

---

## PART C — Find the bug

### C1
```cpp
struct Trade { int id; double price; };
Trade t{192.5, 1001};
```
<details><summary>Answer</summary>Positional init in wrong order: `id = 192` (narrowing from 192.5!), `price = 1001`. Use `Trade{ .id = 1001, .price = 192.5 }`.</details>

### C2
```cpp
struct Level { char side; std::int64_t price; std::int32_t count; std::int64_t qty; };
static_assert(sizeof(Level) == 22);
```
<details><summary>Answer</summary>Not 22 — padding makes it ~32/40. Also bad member order. Reorder descending (int64s first) and fix the assert to the real size.</details>

### C3
```cpp
struct Wire { unsigned version : 4; unsigned type : 4; };
Wire w; std::memcpy(&w, networkBytes, 1);
```
<details><summary>Answer</summary>Bitfield bit-order is implementation-defined → `version`/`type` may be swapped vs the sender. Use explicit masks: `version = b >> 4; type = b & 0x0F;` (per the protocol spec).</details>

### C4
```cpp
union V { int i; std::string s; };
V v; v.s = "hello";
```
<details><summary>Answer</summary>`s` was never constructed (union member with a non-trivial ctor). `placement new (&v.s) std::string("hello");` and later `v.s.~basic_string();`. Or just use `std::variant<int, std::string>`.</details>

### C5
```cpp
auto* m = reinterpret_cast<const QuoteMsg*>(recvBuffer);
process(m->price);
```
<details><summary>Answer</summary>Strict-aliasing UB + (if `QuoteMsg` is packed) unaligned pointer deref. `QuoteMsg m; std::memcpy(&m, recvBuffer, sizeof(m)); process(m.price);`</details>

### C6
```cpp
enum Priority { Low, Medium, High };
void setLevel(int);
setLevel(High);
```
<details><summary>Answer</summary>Unscoped enum implicitly converts to `int` (2) — probably not what `setLevel` expects. `enum class Priority` + `static_cast<int>` where genuinely needed.</details>

### C7
```cpp
std::variant<Quote, Trade> ev = Trade{...};
Quote q = std::get<Quote>(ev);
```
<details><summary>Answer</summary>`ev` holds `Trade` → `std::get<Quote>` throws `std::bad_variant_access`. `if (auto* q = std::get_if<Quote>(&ev))` or `std::visit`.</details>

### C8
```cpp
if (std::memcmp(&order1, &order2, sizeof(Order)) == 0) { /* equal */ }
```
<details><summary>Answer</summary>Padding bytes are indeterminate — two "equal" orders can have different padding → `memcmp` says "not equal". Member-wise `operator==` (or `= default` if it's an aggregate of comparable members).</details>

---

## PART D — Practical tasks

### D1. Layout optimizer
Given 5 structs (mixed member orders), for each: print `sizeof`, `alignof`, every
`offsetof`, the padding, and a reordered version with minimum size. Verify with
`static_assert`. Use `-Wpadded` to cross-check.

### D2. Order book side (zero heap)
```cpp
struct Level { std::int64_t price, qty; std::int32_t orderCount; };
template <std::size_t Depth>
class BookSide {
    std::array<Level, Depth> levels_{};
    std::size_t count_ = 0;
public:
    void insert(std::int64_t px, std::int64_t qty);   // keep sorted, best at [0]
    void erase(std::int64_t px);
    Level best() const;                                // O(1)
    std::span<const Level> view() const;
};
```
`static_assert(sizeof(Level) == 24)`. Reorder `Level` to be minimal. Bench 1M ops.

### D3. Wire protocol codec
A packed 24-byte `OrderMsg` (type, side, symbol[8], price int32, qty int32, seq
int32). `static_assert` `sizeof`/`alignof`/`offsetof` for every field.
`encode(const Order&) -> std::array<std::byte, 24>` and
`decode(std::span<const std::byte, 24>) -> std::optional<Order>` — via `memcpy`,
with endianness handling. Fuzz `decode` with garbage.

### D4. AoS vs SoA vs AoSoA
`struct Particle { float x,y,z,vx,vy,vz; }` — 1M particles, 3 layouts (AoS, SoA,
AoSoA-8). Two kernels: "sum of x" (1 field) and "advance: x+=vx,y+=vy,z+=vz" (6
fields). Table: ns/particle for each layout × kernel × `{-O2, -O3 -march=native}`.
Explain every number.

### D5. Event system — variant
```cpp
struct Quote { double bid, ask; };
struct Trade { double px; long qty; };
struct Cancel { long orderId; };
using Event = std::variant<Quote, Trade, Cancel>;
```
A `Processor` that `std::visit`s a stream, maintaining running stats (last mid,
total traded qty, cancel count). Then: add a `Reject` event — how many places
does the compiler force you to update?

### D6. Enum toolkit
`enum class OrderType : std::uint8_t { Market, Limit, Stop, StopLimit, Iceberg };`
— `toString` (switch, no default → `-Wswitch` clean), `fromString`, `isValid(int)`.
A `Flags` enum with `operator|`/`&`/`~` and `has()`. Put `OrderType` in a packed
struct — 1 byte?

### D7. `struct` vs `class` refactor
Take a data-heavy module. For each type, apply the invariant test. Convert
invariant-bearing `struct`s to `class`es with a validating interface; leave
transparent data as `struct`. Note what bugs the encapsulation would have
prevented.

### D8. `pahole`-style layout printer
Write a program that, for a given struct, prints its layout ASCII-art:
```
struct Level {           /* size: 24, align: 8 */
    int64_t price;        /*   0   8 */
    int64_t qty;          /*   8   8 */
    int32_t orderCount;   /*  16   4 */
    /* padding */         /*  20   4 */
};
```
Using `sizeof`, `alignof`, `offsetof`. (On Linux, compare with real `pahole`.)

---

## PART E — Self-assessment

```
[ ] struct = related data, value semantics, const& params -- clear
[ ] Aggregate init, DMI, designated init (C++20) -- kab kaunsa
[ ] Nested structs inline hote hain, chained . access free
[ ] Alignment + padding -- rules, member reorder se size kam (measured)
[ ] alignof / alignas, -Wpadded, static_assert(sizeof)
[ ] #pragma pack + memcpy decode + endianness (packed wire structs)
[ ] Packed member ka & = UB; reinterpret_cast avoid
[ ] AoS vs SoA -- kab kaunsa, cache-line math, ~1.5x-4x measured
[ ] union -- shared memory, active member, bit_cast for punning
[ ] std::variant -- type-safe, visit exhaustive, closed set
[ ] enum class hamesha -- scoped, no implicit int, underlying type
[ ] enum class + switch no-default -> -Wswitch safety
[ ] Bitfields impl-defined -> masks for portability
[ ] struct vs class -- sirf default access, invariant test
[ ] Saare 8 examples chalaye
```

**Scoring:**
- **12–15** → PHASE 4 done! Folder 12 (Pointers) — "yahan se asli C++". 🎯
- **8–11** → Files 05, 06, 07, 09 dobara.
- **< 8** → Poora folder, `02`/`03` padding examples, `08` wire struct.

---

## PART F — Challenge

### Challenge 1: "Full order book"

```cpp
struct Level  { std::int64_t price, qty; std::int32_t orderCount; };   // reorder to 24 B
struct Order  { std::uint64_t id; std::int64_t price, qty; Side side; };

template <std::size_t Depth>
class OrderBook {
    std::array<Level, Depth> bids_{}, asks_{};   // sorted; bids best = highest, asks best = lowest
    std::size_t bidCount_ = 0, askCount_ = 0;
public:
    void onAdd(const Order&);
    void onCancel(std::uint64_t id, std::int64_t price, Side);
    void onModify(std::uint64_t id, std::int64_t newQty, Side);
    Level bestBid() const;   Level bestAsk() const;   // O(1)
    std::int64_t spread() const;
    std::span<const Level> bids() const;   std::span<const Level> asks() const;
};
```

Requirements: **zero heap** (fixed `Depth`), `static_assert` every struct size,
best bid/ask O(1), sorted inserts (measure array-shift vs a sorted vector), a
packed `WireEvent` codec feeding `onAdd/onCancel/onModify` via `memcpy`.
`-Wall -Wextra -Wshadow -Wpadded` audit, `-Werror` for the first three. Bench a
1M-event replay. This is the seed for folder 39.

### Challenge 2: "Layout museum"

Every layout concept as a live, measured demo: padding (before/after reorder,
with sizes), `alignas(64)` false-sharing (2-thread benchmark), `#pragma pack` +
unaligned-access timing, AoS/SoA/AoSoA (3 kernels), union bit_cast, variant vs
virtual dispatch (`-S` comparison). One program, a results table, a written
explanation of each number.

### Challenge 3: "Zero-copy binary protocol"

Design a compact binary message format (header + typed payload via anonymous
union or `variant`-of-PODs). Packed structs, `static_assert`ed layout,
endianness, a `memcpy`-based codec, and a decoder that takes `std::span<const
std::byte>` and never OOBs / never allocates. Fuzz with truncated/garbage input.
Then benchmark decode throughput (messages/sec) at `-O2` and `-O3 -march=native`.

---

## 🎉 Folder 11 complete — PHASE 4 done

Structs: bundling data, aggregate/DMI/designated init, nested layout, pass/return
semantics, **padding & alignment** (member reorder → 30–45% smaller, measured),
packed wire structs + `memcpy` decode, **AoS vs SoA** (~1.5x–4x measured), unions
& `std::bit_cast`, `std::variant` + `visit`, `enum class`, bitfields (and why
masks are better), `struct` vs `class`.

**PHASE 4 (Arrays, Strings, Structs) complete.** Aapke paas ab data structures
hain — contiguous, sized, bundled, layout-controlled.

Agla (PHASE 5): **pointers** — "yahan se asli C++ shuru hoti hai." Har
prerequisite (variables → addresses → memory → arrays → structs) ho chuka hai.

---

## Next
→ [`../12-POINTERS/00-README.md`](../12-POINTERS/00-README.md)

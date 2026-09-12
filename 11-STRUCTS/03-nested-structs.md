# 03 — Nested structs, arrays of structs

## Prerequisites
- [`02-struct-initialization.md`](02-struct-initialization.md)
- `09-ARRAYS/06-multidimensional-arrays.md` (contiguous layout)

## Yeh topic abhi kyun
Real data structures nest hoti hain — ek `Order` ke andar `Price` aur `Timestamp`
structs, ek `OrderBook` ke andar `Level`s ka array. Yeh lesson: nesting ka
memory layout aur access.

---

## Struct inside struct

```cpp
struct Timestamp { std::int32_t sec; std::int32_t nsec; };
struct Price     { std::int64_t raw; };   // fixed-point

struct Order {
    std::uint64_t id;
    Price         price;         // <- a struct member
    Timestamp     created;       // <- another struct member
    std::int64_t  qty;
};
```

### Access — chained dot

```cpp
Order o{};
o.price.raw = 1923400;
o.created.sec = 1700000000;
std::int64_t p = o.price.raw;
```

### Init — nested braces

```cpp
Order o{ 1001, {1923400}, {1700000000, 500}, 100 };
Order p{ .id = 1001, .price = {1923400}, .created = {1700000000, 500}, .qty = 100 };
```

---

## Memory layout — flattened, contiguous

Nested structs are **not** pointers — the sub-struct's bytes are laid out
**inline**, right there in the parent:

```
   Order (contiguous):
   ┌──────────┬─────────────┬──────────────────┬──────────┐
   │ id (8)   │ price.raw(8)│ created.sec(4)   │ qty (8)  │
   │          │             │ created.nsec(4)  │          │
   └──────────┴─────────────┴──────────────────┴──────────┘
   offset 0    8             16                 24
```

`sizeof(Order)` = sum of member sizes + padding, where a nested struct
contributes its own `sizeof` (with its own internal padding) and its own
`alignof`. No indirection — `o.price.raw` is `load [&o + 8]`.

⚠️ A nested struct's `alignof` propagates: if `Timestamp` has `alignof 4`, it
must start on a 4-byte boundary inside `Order` → possible padding before it (file 05).

---

## Arrays of structs (AoS)

```cpp
struct Level { std::int64_t price; std::int64_t qty; };   // 16 bytes

Level book[10];                          // 160 contiguous bytes
std::array<Level, 10> book2{};
std::vector<Level> book3(10);

book[3].price = 100;
for (const Level& l : book2) total += l.qty;
```

`book[i]` → `&book[0] + i * sizeof(Level)` — one scaled load to the struct, then
member offset. Contiguous → cache-friendly for **whole-struct** access. For
**one-field** scans over many structs → SoA often wins (file 07).

### Struct containing an array

```cpp
struct OrderBook {
    std::array<Level, 32> bids;
    std::array<Level, 32> asks;
    std::uint32_t bidCount, askCount;
};   // one big contiguous block -- no heap, cache-resident
```

---

## Nested + designated init (C++20)

```cpp
OrderBook ob{
    .bids = {{ {100, 500}, {99, 300} }},   // array-of-struct init needs the extra braces
    .bidCount = 2,
};
```

(The `{{ ... }}` — outer for `std::array`, inner for the element list.)

---

## Deep nesting — keep it shallow

```cpp
config.network.tcp.socket.options.keepalive.interval   // ⚠️ hard to read/pass
```

2–3 levels is fine. Deeper → flatten, or pass sub-structs by reference:

```cpp
void tune(SocketOptions& opts);
tune(config.network.tcp.socket.options);
```

---

## Andar kya hota hai

- Nested struct members are laid out inline in declaration order, each aligned to
  its own `alignof`, with padding as needed. The parent's `alignof` is the max of
  all members' (including nested structs').
- `o.a.b.c` → a single `load` at a compile-time constant offset (`offset_of_a +
  offset_of_b + offset_of_c`). No pointer chasing.
- An array-of-structs is one contiguous block; `arr[i].field` is `base + i*stride
  + field_offset`.
- No vtable / no header for plain nested structs — layout is exactly the members.

> **HFT relevance:** Order books, market-data snapshots, and risk aggregates are
> nested plain structs / arrays-of-structs in one contiguous, heap-free block —
> `std::array<Level, Depth>` inside a `Book` struct, the whole thing cache-
> resident and `memcpy`-able. Chained `.` access is free (constant offsets). The
> one decision is AoS vs SoA for the hot scan (file 07). Deep config trees live
> in cold startup code where readability wins.

---

## Hands-on

`examples/01_struct_basics.cpp` (`vector<Point>`), and build a small nested
`OrderBook` with `std::array<Level, N>` members; print `sizeof` and offsets.

```bash
./build.ps1 11-STRUCTS/examples/01_struct_basics.cpp
```

---

## ⚠️ Traps

### Trap 1 — thinking a nested struct is a pointer
```cpp
o.price.raw = 5;    // direct -- no allocation, no indirection. (It's inline.)
```

### Trap 2 — nested struct alignment causing surprise padding
```cpp
struct Inner { double d; };            // align 8
struct Outer { char c; Inner in; };    // 7 bytes padding before `in` -> sizeof 16
```

### Trap 3 — array-of-struct init needs nested braces
```cpp
std::array<Level, 2> a{ {100,500}, {99,300} };     // ⚠️ may need { { {100,500}, {99,300} } }
```

### Trap 4 — `vector<vector<Level>>` for a 2D book
```cpp
std::vector<std::vector<Level>> book;   // ⚠️ scattered. std::array<Level,N> or flat vector
```

### Trap 5 — deep chains passed around
```cpp
process(cfg.a.b.c.d.e);   // ⚠️ fragile. Pass cfg.a.b.c.d.e's type by ref, or flatten
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Nested struct member is a pointer" | Inline bytes — no indirection |
| "`o.a.b.c` chases pointers" | Single load at a constant offset |
| "`sizeof(Outer)` = `sizeof` members" | + padding from nested `alignof` too |
| "Array-of-structs is like `T**`" | One contiguous block |
| "Deep nesting is free" | Free for the CPU; costly for readers |

---

## Exercises

1. **Nested layout:** `struct A { int x; };  struct B { A a; double d; };` —
   `sizeof(B)`? `offsetof(B, d)`? Why?

2. **Chained access:** build `struct Engine { int hp; };  struct Car { Engine e;
   std::string model; };` — set and read `car.e.hp`.

3. **AoS:** `std::array<Level, 5>` order book side; fill; find best (max price)
   bid; total qty.

4. **Struct with array member:** `struct Histogram { std::array<int, 256>
   buckets; long total; };` — `add(Histogram&, int byte)`, `mode(const
   Histogram&)`.

5. **Alignment padding:** `struct P1 { char c; struct { double d; } inner; };` —
   `sizeof`? Move `c` after `inner` — `sizeof` now?

6. **Flatten:** a 4-deep config chain — rewrite so hot code takes the leaf
   sub-struct by reference.

---

## Interview questions

1. Nested struct member — pointer ya inline? Access ki cost?
2. `sizeof(Outer)` with a nested struct — kaise compute hota hai?
3. Array-of-structs ka memory layout? `arr[i].field` ka address?
4. Nested struct ki alignment parent pe kaise asar karti hai?
5. `std::array<Level, N>` as a struct member vs `std::vector<Level>` — trade-offs?
6. Deep nesting kab problem, kaise handle karo?

---

## Next
→ [`04-structs-and-functions.md`](04-structs-and-functions.md)

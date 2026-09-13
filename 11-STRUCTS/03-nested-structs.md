# 03 — Nested structs, arrays of structs

## Prerequisites
- [`02-struct-initialization.md`](02-struct-initialization.md)
- `09-ARRAYS/06-multidimensional-arrays.md` (contiguous layout)

## Yeh topic abhi kyun
Real data structures nest hoti hain — ek `Order` ke andar `Price` aur `Timestamp`
structs, ek `OrderBook` ke andar `Level`s ka array. Yeh lesson: nesting ka
memory layout aur access.

---

## Struct ke andar struct

```cpp
struct Timestamp { std::int32_t sec; std::int32_t nsec; };
struct Price     { std::int64_t raw; };   // fixed-point

struct Order {
    std::uint64_t id;
    Price         price;         // <- ek struct member
    Timestamp     created;       // <- ek aur struct member
    std::int64_t  qty;
};
```

### Access — dot ki chain

```cpp
Order o{};
o.price.raw = 1923400;
o.created.sec = 1700000000;
std::int64_t p = o.price.raw;
```

### Init — braces ke andar braces

```cpp
Order o{ 1001, {1923400}, {1700000000, 500}, 100 };
Order p{ .id = 1001, .price = {1923400}, .created = {1700000000, 500}, .qty = 100 };
```

---

## Memory layout — sab ek line mein, contiguous

Nested structs **pointer nahi** hote — andar wale struct ke bytes parent ke **andar hi** rakhe jaate hain:

```
   Order (contiguous):
   ┌──────────┬─────────────┬──────────────────┬──────────┐
   │ id (8)   │ price.raw(8)│ created.sec(4)   │ qty (8)  │
   │          │             │ created.nsec(4)  │          │
   └──────────┴─────────────┴──────────────────┴──────────┘
   offset 0    8             16                 24
```

(GCC 16.2 pe check kiya: `sizeof(Order)` = 32, `offsetof(Order, created)` = 16.)

Analogy: nested struct ek dabbe ke andar rakha chhota dabba hai — alag almari mein rakhi cheez ki
parchi (pointer) nahi. Bada dabba uthao, andar wala saath aata hai.

`sizeof(Order)` = members ke size ka jod + padding, jahan nested struct apna `sizeof` (apni andar ki
padding samet) aur apna `alignof` laata hai. Koi indirection nahi — `o.price.raw` matlab `load [&o + 8]`.

⚠️ Nested struct ka `alignof` upar tak jaata hai: agar `Timestamp` ka `alignof` 4 hai, to use `Order`
ke andar 4-byte boundary pe hi shuru hona padega → uske pehle padding aa sakti hai (file 05).

---

## Arrays of structs (AoS)

```cpp
struct Level { std::int64_t price; std::int64_t qty; };   // 16 bytes

Level book[10];                          // 160 bytes ek saath
std::array<Level, 10> book2{};
std::vector<Level> book3(10);

book[3].price = 100;
for (const Level& l : book2) total += l.qty;
```

`book[i]` → `&book[0] + i * sizeof(Level)` — struct tak ek scaled load, phir member ka offset.
Contiguous hai → **poore struct** ke access ke liye cache-friendly. Bahut saare structs ke **ek hi field**
ke scan ke liye → aksar SoA jeetta hai (file 07).

### Struct jiske andar array ho

```cpp
struct OrderBook {
    std::array<Level, 32> bids;
    std::array<Level, 32> asks;
    std::uint32_t bidCount, askCount;
};   // ek bada contiguous block -- heap nahi, cache mein rehta hai (sizeof 1032, GCC 16.2)
```

---

## Nested + designated init (C++20)

```cpp
OrderBook ob{
    .bids = {{ {100, 500}, {99, 300} }},   // array-of-struct init ko extra braces chahiye
    .bidCount = 2,
};
```

(`{{ ... }}` — bahar wala `std::array` ke liye, andar wala element list ke liye.)

⚠️ GCC 16.2 pe yeh chalta hai, par `-Wextra` chhode gaye members (`asks`, `askCount`) ke liye
`-Wmissing-field-initializers` warning deta hai. Woh members value-init hi hote hain (zero) — warning bas
yaad dilati hai. Chup karana ho to unhe bhi likh do (`.asks = {}`).

---

## Gehri nesting — uthli rakho

```cpp
config.network.tcp.socket.options.keepalive.interval   // ⚠️ padhna/pass karna mushkil
```

2–3 level theek hain. Usse gehra → flatten karo, ya andar ke struct ko reference se pass karo:

```cpp
void tune(SocketOptions& opts);
tune(config.network.tcp.socket.options);
```

---

## Andar kya hota hai

- Nested struct ke members declaration order mein inline rakhe jaate hain, har ek apne `alignof` pe,
  zaroorat ho to padding ke saath. Parent ka `alignof` = saare members (nested structs samet) mein
  sabse bada.
- `o.a.b.c` → ek hi `load`, compile-time constant offset pe (`offset_of_a + offset_of_b + offset_of_c`).
  Koi pointer chasing nahi.
- Array-of-structs ek hi contiguous block hai; `arr[i].field` = `base + i*stride + field_offset`.
- Plain nested structs mein na vtable, na header — layout bilkul members jitna.

> **HFT relevance:** Order books, market-data snapshots, aur risk aggregates nested plain structs /
> arrays-of-structs hote hain, ek contiguous, heap-free block mein — `Book` struct ke andar
> `std::array<Level, Depth>`, poori cheez cache mein aur `memcpy` ho sakne layak. Dot ki chain free hai
> (constant offsets). Asli faisla bas ek hai: hot scan ke liye AoS vs SoA (file 07). Gehre config trees
> thande startup code mein rehte hain, jahan padhne mein aasaani jeetti hai.

---

## Hands-on

`examples/01_struct_basics.cpp` (`vector<Point>`), aur ek chhota nested `OrderBook` banao jisme
`std::array<Level, N>` members hon; `sizeof` aur offsets print karo.

```bash
./build.ps1 11-STRUCTS/examples/01_struct_basics.cpp
```

---

## ⚠️ Traps

### Trap 1 — nested struct ko pointer samajhna
```cpp
o.price.raw = 5;    // seedha -- na allocation, na indirection. (Inline hai.)
```

### Trap 2 — nested struct ki alignment se achanak padding
```cpp
struct Inner { double d; };            // align 8
struct Outer { char c; Inner in; };    // `in` se pehle 7 bytes padding -> sizeof 16
```

### Trap 3 — array-of-struct init ko extra braces chahiye
```cpp
std::array<Level, 2> a{ {100,500}, {99,300} };       // ❌ GCC 16.2: too many initializers for 'std::array<Level, 2>'
std::array<Level, 2> a{ { {100,500}, {99,300} } };   // ✅
```
Kyun: `std::array` andar ek C array wala struct hai. Pehla `{100,500}` poore andar ke array ko bhar
deta hai, aur doosre `{99,300}` ke liye koi member bachta hi nahi.

### Trap 4 — 2D book ke liye `vector<vector<Level>>`
```cpp
std::vector<std::vector<Level>> book;   // ⚠️ bikhra hua. std::array<Level,N> ya flat vector lo
```

### Trap 5 — gehri chains idhar-udhar pass karna
```cpp
process(cfg.a.b.c.d.e);   // ⚠️ nazuk. cfg.a.b.c.d.e ke type ko ref se lo, ya flatten karo
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Nested struct member ek pointer hai" | Inline bytes — koi indirection nahi |
| "`o.a.b.c` pointers chase karta hai" | Constant offset pe ek load |
| "`sizeof(Outer)` = members ke `sizeof` ka jod" | + nested `alignof` ki padding bhi |
| "Array-of-structs `T**` jaisa hai" | Ek contiguous block |
| "Gehri nesting free hai" | CPU ke liye free; padhne wale ke liye mehngi |
| "`std::array<Level,2>{ {..}, {..} }` chal jaayega" | Error — teen level braces chahiye |

---

## Exercises

1. **Nested layout:** `struct A { int x; };  struct B { A a; double d; };` — `sizeof(B)`?
   `offsetof(B, d)`? Kyun?
   <details><summary>Answer</summary>

   `sizeof(B)` = 16, `offsetof(B, d)` = 8 (GCC 16.2 pe chala ke). `A` 4 bytes ka hai; `double` ko
   8-byte boundary chahiye, isliye `a` ke baad 4 bytes padding, phir `d`.
   </details>

2. **Chained access:** `struct Engine { int hp; };  struct Car { Engine e; std::string model; };` banao —
   `car.e.hp` set karo aur padho.

3. **AoS:** `std::array<Level, 5>` order book ki ek side; bharo; best (max price) bid dhoondho; kul qty.

4. **Array member wala struct:** `struct Histogram { std::array<int, 256> buckets; long total; };` —
   `add(Histogram&, int byte)`, `mode(const Histogram&)`.

5. **Alignment padding:** `struct P1 { char c; struct { double d; } inner; };` — `sizeof`? `c` ko `inner`
   ke baad le jao — ab `sizeof`?

6. **Flatten:** 4 level gehri config chain — aisa rewrite karo ki hot code sirf aakhri sub-struct ko
   reference se le.

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

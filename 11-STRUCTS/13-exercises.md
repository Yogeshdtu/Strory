# 13 — Folder 11 Revision + Exercises

## Prerequisites
Lessons 01–12 aur saare 8 examples chalaye hue (`04_aos_vs_soa` `-O2` pe).

---

## PART A — Concept check

1. Struct kya hai? `sizeof(struct)` kaise tay hota hai?
2. Struct copy / assignment — kya karta hai?
3. Struct pass by value vs `const&` — kab kaunsa? Chhote struct ka exception (aur ABI ka fark)?
4. Aggregate init — order, partial-init behaviour?
5. `int` members wale struct ke liye `S s;` vs `S s{};`?
6. Default member initializer — storage badhata hai?
7. Designated initializers (C++20) — niyam?
8. Nested struct member — pointer ya inline? Access cost?
9. Alignment kya hai? Padding kyun aur kahan?
10. Member reordering se `sizeof` kaise ghatta — niyam?
11. `alignas(64)` — 2 wajah?
12. Struct equality ke liye `memcmp` kyun galat?
13. `#pragma pack(1)` — `sizeof`/`alignof` pe asar, trade-off?
14. Packed struct member ka `&` — problem? GCC kab warn karta hai, kab nahi?
15. `reinterpret_cast<Msg*>(buf)` vs `memcpy` — kyun `memcpy`?
16. AoS vs SoA — kab kaunsa? Cache-line ka hisaab?
17. Union — members kaise? Active member ka niyam?
18. Safe type punning — union nahi to kya?
19. `std::variant` vs union — 3 farq?
20. `std::visit` exhaustiveness — `switch` se fark?
21. `std::variant` vs `virtual` — band vs khula set?
22. `enum` vs `enum class` — 3 farq?
23. `enum class` underlying type — default, khud kab dena?
24. Bitfields wire formats ke liye kyun unsafe?
25. `struct` vs `class` — technical fark (sirf ek)?

---

## PART B — Output prediction

### B1
```cpp
struct S { char a; int b; };
std::cout << sizeof(S) << " " << offsetof(S, b);
```
<details><summary>Answer</summary>`8 4` — `a` ke baad 3 bytes padding taaki `b` 4-aligned ho.</details>

### B2
```cpp
struct P { int x = 1, y = 2; };
P a; P b{10};
std::cout << a.x << a.y << " " << b.x << b.y;
```
<details><summary>Answer</summary>`12 102` — DMI defaults; `b` sirf `x` badalta hai. (Chala ke.)</details>

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
std::cout << u.f;   // (inactive member padhna -- UB, par GCC pe:)
```
<details><summary>Answer</summary>`1` — `0x3f800000` `1.0f` ka IEEE-754 bit pattern hai. (Technically UB; sahi tareeqa `std::bit_cast`.)</details>

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
<details><summary>Answer</summary>`0 1 1` (chala ke; `bool` print hota hai `1`).</details>

### B7
```cpp
#pragma pack(push, 1)
struct M { std::uint8_t t; std::uint32_t v; };
#pragma pack(pop)
std::cout << sizeof(M) << " " << offsetof(M, v);
```
<details><summary>Answer</summary>`5 1` — packed: padding nahi, `v` offset 1 pe.</details>

### B8
```cpp
struct A { int x; };
class  B { public: int x; };
std::cout << std::boolalpha << (sizeof(A) == sizeof(B));
```
<details><summary>Answer</summary>`true` — layout same; sirf default access alag. (Types phir bhi alag — `std::is_same_v<A, B>` `false`.)</details>

### B9
```cpp
std::variant<int, double, std::string> v = std::string("x");
v = {};
std::cout << v.index();
```
<details><summary>Answer</summary>`0` — `v = {}` pehle alternative (`int`, value 0) pe reset karta hai; string destroy. (GCC 16.2 pe chala ke — lesson 09.)</details>

---

## PART C — Find the bug

### C1
```cpp
struct Trade { int id; double price; };
Trade t{192.5, 1001};
```
<details><summary>Answer</summary>

Positional init galat order mein. Brace init narrowing rokta hai, isliye yeh **compile hi nahi hota**:
`error: narrowing conversion of '1.925e+2' from 'double' to 'int'` (GCC 16.2). Designated init lo:
`Trade{ .id = 1001, .price = 192.5 }`. ⚠️ Khatarnaak version woh hai jahan dono field same type ke hon —
`Quote{101.5, 101.0}` (bid/ask ulta) chupchaap compile hota hai (lesson 02).
</details>

### C2
```cpp
struct Level { char side; std::int64_t price; std::int32_t count; std::int64_t qty; };
static_assert(sizeof(Level) == 22);
```
<details><summary>Answer</summary>

22 nahi — padding se **32** (1+7pad+8+4+4pad+8). Aur member order bhi bura. Descending reorder karo
(`price, qty, count, side`) → **24**, aur assert asli size pe set karo. (Dono GCC 16.2 pe naape.)
</details>

### C3
```cpp
struct Wire { unsigned version : 4; unsigned type : 4; };
Wire w; std::memcpy(&w, networkBytes, 1);
```
<details><summary>Answer</summary>

Bitfield ka bit-order implementation-defined → `version`/`type` sender ke hisaab se ulte ho sakte hain. Upar se
`sizeof(Wire)` 4 hai (unit `unsigned`), 1 nahi — 1 byte copy karke baaki 3 bytes garbage. Explicit masks lo:
`version = b >> 4; type = b & 0x0F;` (protocol spec ke hisaab se).
</details>

### C4
```cpp
union V { int i; std::string s; };
V v; v.s = "hello";
```
<details><summary>Answer</summary>

Compile hi nahi hoga: `error: use of deleted function 'V::V()'` — non-trivial member wale union ka default
ctor/dtor deleted. Ctor/dtor likh bhi do to `s` kabhi construct nahi hua → `v.s = ...` UB. Sahi:
`new (&v.s) std::string("hello");` aur baad mein `v.s.~basic_string();`. Ya seedha `std::variant<int, std::string>`.
</details>

### C5
```cpp
auto* m = reinterpret_cast<const QuoteMsg*>(recvBuffer);
process(m->price);
```
<details><summary>Answer</summary>Strict-aliasing UB + (agar `QuoteMsg` packed hai) unaligned pointer deref. `QuoteMsg m; std::memcpy(&m, recvBuffer, sizeof(m)); process(m.price);`</details>

### C6
```cpp
enum Priority { Low, Medium, High };
void setLevel(int);
setLevel(High);
```
<details><summary>Answer</summary>Unscoped enum chupchaap `int` (2) ban jaata hai — shayad `setLevel` yeh nahi chahta. `enum class Priority` + jahan sach mein chahiye wahan `static_cast<int>`.</details>

### C7
```cpp
std::variant<Quote, Trade> ev = Trade{...};
Quote q = std::get<Quote>(ev);
```
<details><summary>Answer</summary>`ev` mein `Trade` hai → `std::get<Quote>` `std::bad_variant_access` throw karta hai. `if (auto* q = std::get_if<Quote>(&ev))` ya `std::visit`.</details>

### C8
```cpp
if (std::memcmp(&order1, &order2, sizeof(Order)) == 0) { /* equal */ }
```
<details><summary>Answer</summary>Padding bytes indeterminate hain — do "barabar" orders ki padding alag ho sakti hai → `memcmp` "barabar nahi" kahega. Member-wise `operator==` (ya comparable members wala aggregate ho to `= default`).</details>

### C9
```cpp
#pragma pack(push, 1)
struct Hdr { std::uint8_t type; std::uint32_t len; };
#pragma pack(pop)
Hdr h{};
std::uint32_t* p = &h.len;   // "compiler ne warning nahi di, to theek hai"
```
<details><summary>Answer</summary>

`p` misaligned pointer hai (offset 1) → dereference UB. GCC 16.2 `-Waddress-of-packed-member` **sirf `[[gnu::packed]]`
structs pe** deta hai, `#pragma pack` pe chup (lesson 06). Warning na aana safety ka saboot nahi. `memcpy` se value
nikaalo.
</details>

---

## PART D — Practical tasks

### D1. Layout optimizer
5 structs (mile-jule member order) — har ek ke liye: `sizeof`, `alignof`, har `offsetof`, padding, aur minimum size
wala reordered version print karo. `static_assert` se verify. Cross-check ke liye `-Wpadded` — MinGW pe
`-mno-ms-bitfields` ke saath, warna beech ki padding nahi dikhegi (lesson 05).

### D2. Order book side (zero heap)
```cpp
struct Level { std::int64_t price, qty; std::int32_t orderCount; };
template <std::size_t Depth>
class BookSide {
    std::array<Level, Depth> levels_{};
    std::size_t count_ = 0;
public:
    void insert(std::int64_t px, std::int64_t qty);   // sorted rakho, best [0] pe
    void erase(std::int64_t px);
    Level best() const;                                // O(1)
    std::span<const Level> view() const;
};
```
`static_assert(sizeof(Level) == 24)`. `Level` ko minimal reorder karo. 1M ops bench karo (kernel `[[gnu::noipa]]` mein).

### D3. Wire protocol codec
Packed 24-byte `OrderMsg` (type, side, symbol[8], price int32, qty int32, seq int32). Har field ke liye `sizeof`/
`alignof`/`offsetof` `static_assert`. `encode(const Order&) -> std::array<std::byte, 24>` aur
`decode(std::span<const std::byte, 24>) -> std::optional<Order>` — `memcpy` se, endianness handling ke saath.
`decode` ko garbage se fuzz karo.

### D4. AoS vs SoA vs AoSoA
`struct Particle { float x,y,z,vx,vy,vz; }` — 1M particles, 3 layouts (AoS, SoA, AoSoA-8). Do kernels: "sum of x"
(1 field) aur "advance: x+=vx,y+=vy,z+=vz" (6 fields). Table: har layout × kernel × `{-O2, -O3 -march=native}` ke liye
ns/particle. Har number samjhao — kaunsa loop vectorize hua (`-fopt-info-vec-optimized`), aur har number physically
possible hai ya nahi (lesson 07 ka dead-rep trap).

### D5. Event system — variant
```cpp
struct Quote { double bid, ask; };
struct Trade { double px; long qty; };
struct Cancel { long orderId; };
using Event = std::variant<Quote, Trade, Cancel>;
```
Ek `Processor` jo stream pe `std::visit` kare aur running stats rakhe (last mid, total traded qty, cancel count). Phir:
ek `Reject` event jodo — compiler kitni jagah update karne pe majboor karta hai?

### D6. Enum toolkit
`enum class OrderType : std::uint8_t { Market, Limit, Stop, StopLimit, Iceberg };` — `toString` (switch, default nahi →
`-Wswitch` clean), `fromString`, `isValid(int)`. `operator|`/`&`/`~` aur `has()` wala `Flags` enum. `OrderType` ko ek
packed struct mein daalo — 1 byte? (Haan — `sizeof(OrderType)` 1, packed `{OrderType; uint32_t}` = 5.)

### D7. `struct` vs `class` refactor
Ek data-heavy module lo. Har type pe invariant test lagao. Invariant wale `struct`s ko validating interface wali
`class` banao; transparent data `struct` hi rehne do. Likho encapsulation kaunse bugs rok leta.

### D8. `pahole`-style layout printer
Aisa program likho jo diye gaye struct ka layout ASCII-art mein print kare:
```
struct Level {           /* size: 24, align: 8 */
    int64_t price;        /*   0   8 */
    int64_t qty;          /*   8   8 */
    int32_t orderCount;   /*  16   4 */
    /* padding */         /*  20   4 */
};
```
`sizeof`, `alignof`, `offsetof` se. (Linux pe asli `pahole` se milao.)

---

## PART E — Self-assessment

```
[ ] struct = related data, value semantics, const& params -- saaf
[ ] Aggregate init, DMI, designated init (C++20) -- kab kaunsa
[ ] Nested structs inline hote hain, chained . access free
[ ] Alignment + padding -- niyam, member reorder se size kam (naapa)
[ ] alignof / alignas, -Wpadded (MinGW ki seema), static_assert(sizeof)
[ ] #pragma pack + memcpy decode + endianness (packed wire structs)
[ ] Packed member ka & = UB; #pragma pack pe GCC chup; reinterpret_cast se bacho
[ ] AoS vs SoA -- kab kaunsa, cache-line hisaab, flags ke saath 2.3x-10x naapa
[ ] union -- shared memory, active member, punning ke liye bit_cast
[ ] std::variant -- type-safe, visit exhaustive, band set
[ ] enum class hamesha -- scoped, chupchaap int nahi, underlying type
[ ] enum class + bina-default switch -> -Wswitch safety
[ ] Bitfields impl-defined (ek flag se 8 vs 4 bytes) -> portability ke liye masks
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
struct Level  { std::int64_t price, qty; std::int32_t orderCount; };   // 24 B tak reorder
struct Order  { std::uint64_t id; std::int64_t price, qty; Side side; };

template <std::size_t Depth>
class OrderBook {
    std::array<Level, Depth> bids_{}, asks_{};   // sorted; bids best = sabse unchi, asks best = sabse neechi
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

Requirements: **zero heap** (fixed `Depth`), har struct size `static_assert`, best bid/ask O(1), sorted inserts
(array-shift vs sorted vector naapo), `memcpy` se `onAdd/onCancel/onModify` ko feed karta packed `WireEvent` codec.
`-Wall -Wextra -Wshadow -Wpadded` audit (MinGW pe `-mno-ms-bitfields` ke saath), pehle teeno ke liye `-Werror`.
1M-event replay bench karo. Yeh folder 39 ka beej hai.

### Challenge 2: "Layout museum"

Har layout concept ek live, naapa hua demo: padding (reorder se pehle/baad, sizes ke saath), `alignas(64)` false
sharing (2-thread benchmark), `#pragma pack` + unaligned-access timing (lesson 06 mein x86 pe packed tez nikla — apni
machine pe?), AoS/SoA/AoSoA (3 kernels), union bit_cast, variant vs virtual dispatch (`-S` comparison), bitfield
layout `-mms-bitfields` vs `-mno-ms-bitfields`. Ek program, ek results table, har number ki likhit explanation.

### Challenge 3: "Zero-copy binary protocol"

Ek compact binary message format design karo (header + anonymous union ya PODs ke `variant` se typed payload). Packed
structs, `static_assert` kiya layout, endianness, `memcpy`-based codec, aur ek decoder jo `std::span<const std::byte>`
leta hai aur kabhi OOB / allocate nahi karta. Truncated/garbage input se fuzz. Phir `-O2` aur `-O3 -march=native` pe
decode throughput (messages/sec) bench karo.

---

## 🎉 Folder 11 complete — PHASE 4 done

Structs: data bundle karna, aggregate/DMI/designated init, nested layout, pass/return semantics (ABI ke saath),
**padding & alignment** (member reorder → 30–45% chhota, naapa), packed wire structs + `memcpy` decode, **AoS vs SoA**
(flags ke hisaab se 2.3×–10× naapa), unions & `std::bit_cast`, `std::variant` + `visit`, `enum class`, bitfields (aur
masks kyun behtar), `struct` vs `class`.

**PHASE 4 (Arrays, Strings, Structs) complete.** Aapke paas ab data structures hain — contiguous, sized, bundled,
layout-controlled.

Agla (PHASE 5): **pointers** — "yahan se asli C++ shuru hoti hai." Har prerequisite (variables → addresses → memory →
arrays → structs) ho chuka hai.

---

## Next
→ [`../12-POINTERS/00-README.md`](../12-POINTERS/00-README.md)

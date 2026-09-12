# 04 — Structs aur functions

## Prerequisites
- [`03-nested-structs.md`](03-nested-structs.md)
- `08-FUNCTIONS/03-parameters-and-arguments.md`, `04-return-values.md` (RVO)
- `09-ARRAYS/10-array-performance.md`

## Yeh topic abhi kyun
Struct ko function ko dena / return karna — pass by value vs `const&` vs `&`, aur
copies ki cost. Yeh folder 08 ke parameter lesson ka struct-specific application
hai, plus RVO aur move.

---

## Pass by value — full copy

```cpp
struct Big { double data[64]; };            // 512 bytes

void process(Big b);                        // ⚠️ 512-byte memcpy per call
void process(const Big& b);                 // ✅ 8-byte pointer per call
```

- Trivial struct → `memcpy` of `sizeof`.
- Struct with `std::string` / `std::vector` members → member-wise copy (each
  non-trivial member copied — heap allocation for long strings).

**Rule: pointer-size (8 bytes) se bada → pass by `const&`.** Small trivial
structs (`Point` = 16 bytes, `Timestamp` = 8) — by value is fine (fits in a
register or two, sometimes faster than the indirection).

---

## Pass by `const&` — read, no copy

```cpp
double notional(const Order& o) {           // no copy, o can't be modified
    return o.price * static_cast<double>(o.qty);
}
```

## Pass by `&` — modify caller's struct

```cpp
void applyFill(Order& o, std::int64_t fillQty) {
    o.qty -= fillQty;
    o.status = (o.qty == 0) ? Status::Filled : Status::PartiallyFilled;
}
```

## Pass by value + `std::move` — when the function needs its own copy

```cpp
struct Record { std::string key; std::vector<int> data; };

void store(Record r) {                      // by value...
    cache.emplace(std::move(r.key), std::move(r.data));   // ...then steal
}
store(std::move(myRecord));                 // caller moves in -> zero copies
```

(Folder 18 mein move deep.)

---

## Return by value — RVO makes it free

```cpp
Order buildOrder(std::string_view sym, double px, std::int64_t qty) {
    Order o;
    o.symbol = sym;
    o.price = px;
    o.qty = qty;
    return o;                               // NRVO -- constructed directly in caller's slot
}
Order o = buildOrder("AAPL", 192.34, 100);  // no copy, no move
```

C++17 guarantees copy elision for prvalues; NRVO (named local return) is
near-universal in practice. **Return big structs by value — don't do output
parameters just to "avoid the copy".** And **don't `return std::move(o)`** (breaks
NRVO — folder 08 lesson 04, folder 18).

---

## Multiple return values — a struct

```cpp
struct BestQuote { std::int64_t bid, ask; std::int32_t bidSz, askSz; };

BestQuote topOfBook(const OrderBook& b);

auto [bid, ask, bidSz, askSz] = topOfBook(book);   // structured binding
```

Better than `std::pair`/`std::tuple` (named fields) or out-params (folder 08
lesson 04).

---

## Comparison — `operator==` / `<=>`

Structs are **not** comparable by default:

```cpp
struct P { int x, y; };
P a, b;
if (a == b) { }                            // ❌ ERROR

struct P {
    int x, y;
    bool operator==(const P&) const = default;   // C++20 -- member-wise ==
    auto operator<=>(const P&) const = default;  // C++20 -- member-wise ordering (all 6 ops)
};
```

`= default` → the compiler generates member-wise comparison. Custom logic → write
the body (folder 15, 18).

---

## Andar kya hota hai

- **By value, small trivial struct** (≤ 16 bytes): passed in 1–2 registers
  (`rax`, `rdx`) — often *faster* than `const&` (no memory round-trip).
- **By value, bigger**: caller copies to a temp / the stack, passes a hidden
  pointer to it (like the return-value ABI). Real copy cost.
- **By `const&` / `&`**: just the address (8 bytes, a register). Function derefs.
- **Return by value**: caller passes a hidden pointer to where the result goes;
  the function constructs it there (RVO). Zero copies.
- `-O2` inlines small functions → the whole parameter/return dance disappears,
  the compiler works on the caller's struct directly.

> **HFT relevance:** Hot-path functions take order/message structs by `const&`
> (or `std::span<const Msg>` for batches) — an accidental by-value `Order` or
> `MarketDataMsg` copy is a `memcpy` (+ allocator hit if it has a `std::string`).
> Small POD structs (`Price`, `Timestamp`, a 4-field `BestQuote`) by value is
> idiomatic and register-efficient. Results returned by value (RVO). `= default`
> comparisons for POD keys. Folders 18, 36.

---

## Hands-on

`examples/01_struct_basics.cpp` (`const&` params, return by value), and:

```cpp
struct Big { double d[100]; };
// benchmark: void f(Big) vs void f(const Big&), 1M calls, -O2
```

```bash
./build.ps1 11-STRUCTS/examples/01_struct_basics.cpp
```

---

## ⚠️ Traps

### Trap 1 — big struct by value
```cpp
void render(Scene s);   // ⚠️ copies the whole scene. const Scene&
```

### Trap 2 — `return std::move(local)`
```cpp
Order f() { Order o; ...; return std::move(o); }   // ⚠️ breaks NRVO. return o;
```

### Trap 3 — output parameter instead of return
```cpp
void topOfBook(const Book& b, BestQuote& out);   // ⚠️ awkward. return BestQuote (RVO)
```

### Trap 4 — `==` on a struct without defining it
```cpp
if (order1 == order2) { }   // ❌ ERROR. = default (C++20) or write operator==
```

### Trap 5 — small struct by `const&` in a hot leaf
```cpp
int f(const Point& p);   // for a 16-byte Point, by value may be faster (register). Measure
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Always pass structs by `const&`" | Small trivial structs by value can be faster |
| "Big struct return needs an out-param" | Return by value — RVO is free |
| "`return std::move(x)` is faster" | Breaks NRVO — plain `return x;` |
| "Structs compare with `==` by default" | No — `= default` (C++20) or write it |
| "By-value copy of a struct is a `memcpy`" | Only for trivial members; `std::string` member → alloc |

---

## Exercises

1. **Param cost:** `struct Buf { char b[1024]; };` — `void f(Buf)` vs `void
   f(const Buf&)`, 1M calls, `-O2`, time. Now `struct Small { int a, b; };` —
   same test. Which flips?

2. **RVO:** `Order makeOrder(...)` returning a local. `-O2 -S -masm=intel` —
   any copy/move constructor `call`?

3. **`return std::move`:** add `return std::move(o);` — assembly change? (May
   introduce a move.)

4. **Multiple return:** `struct MinMax { int lo, hi; };  MinMax range(std::span<const
   int>)`. Structured binding at the call site.

5. **`= default` compare:** `struct Key { int a; std::string b; };` — add
   `operator==` and `operator<=>` `= default`. Use `Key` in a `std::set` and a
   `std::unordered_map` (the latter needs a hash — discuss).

6. **Move-in:** `struct Job { std::string name; std::vector<int> input; };  void
   submit(Job);` — call with `std::move`. Instrument copies vs moves.

---

## Interview questions

1. Struct pass by value vs `const&` — kab kaunsa? Small struct exception?
2. Struct return by value — RVO, cost?
3. `return std::move(localStruct)` kyun galat?
4. Function se 2+ related values return — struct kyun better than `pair`/out-param?
5. Struct `==` / `<=>` — `= default` (C++20) kya karta hai?
6. Struct-by-value ki cost trivial vs non-trivial members?

---

## Next
→ [`05-padding-and-alignment.md`](05-padding-and-alignment.md)

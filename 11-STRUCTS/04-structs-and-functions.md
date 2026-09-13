# 04 — Structs aur functions

## Prerequisites
- [`03-nested-structs.md`](03-nested-structs.md)
- `08-FUNCTIONS/03-parameters-and-arguments.md`, `04-return-values.md` (RVO)
- `09-ARRAYS/10-array-performance.md`, `09-ARRAYS/07-arrays-as-parameters.md` (ABI ki pehli jhalak)

## Yeh topic abhi kyun
Struct ko function ko dena / return karna — pass by value vs `const&` vs `&`, aur
copies ki cost. Yeh folder 08 ke parameter lesson ka struct-specific application
hai, plus RVO aur move.

---

## Pass by value — poori copy

```cpp
struct Big { double data[64]; };            // 512 bytes

void process(Big b);                        // ⚠️ har call pe 512-byte memcpy
void process(const Big& b);                 // ✅ har call pe 8-byte pointer
```

- Trivial struct → `sizeof` bytes ka `memcpy`.
- `std::string` / `std::vector` members wala struct → member-by-member copy (har non-trivial member copy
  hota hai — lambi strings ke liye heap allocation).

**Rule: bada struct → `const&`.** Chhote trivial structs (jaise 8-byte `Timestamp`) by value theek hain —
kitne bytes tak "chhota" maana jaaye, woh **platform ke ABI pe depend** karta hai (neeche "Andar kya
hota hai" mein naapa hua farq).

---

## Pass by `const&` — padho, copy nahi

```cpp
double notional(const Order& o) {           // copy nahi, o badal nahi sakta
    return o.price * static_cast<double>(o.qty);
}
```

## Pass by `&` — caller ka struct badlo

```cpp
void applyFill(Order& o, std::int64_t fillQty) {
    o.qty -= fillQty;
    o.status = (o.qty == 0) ? Status::Filled : Status::PartiallyFilled;
}
```

## Pass by value + `std::move` — jab function ko apni copy chahiye

```cpp
struct Record { std::string key; std::vector<int> data; };

void store(Record r) {                      // by value...
    cache.emplace(std::move(r.key), std::move(r.data));   // ...phir andar ka maal le lo
}
store(std::move(myRecord));                 // caller move karke deta hai -> zero copies
```

(Folder 18 mein move gehraai se.)

---

## Return by value — RVO se free

```cpp
Order buildOrder(std::string_view sym, double px, std::int64_t qty) {
    Order o;
    o.symbol = sym;
    o.price = px;
    o.qty = qty;
    return o;                               // NRVO -- seedha caller ki jagah pe banta hai
}
Order o = buildOrder("AAPL", 192.34, 100);  // na copy, na move
```

C++17 prvalues ke liye copy elision guarantee karta hai; NRVO (naam wala local return) practically har
jagah hota hai. **Bade structs value se return karo — "copy bachane" ke liye output parameters mat
banao.** Aur **`return std::move(o)` mat likho** (NRVO tod deta hai — folder 08 lesson 04, folder 18).

---

## Kai values return — ek struct

```cpp
struct BestQuote { std::int64_t bid, ask; std::int32_t bidSz, askSz; };

BestQuote topOfBook(const OrderBook& b);

auto [bid, ask, bidSz, askSz] = topOfBook(book);   // structured binding
```

`std::pair`/`std::tuple` (fields ke naam nahi) ya out-params (folder 08 lesson 04) se behtar.

---

## Comparison — `operator==` / `<=>`

Structs default mein compare **nahi** hote:

```cpp
struct P { int x, y; };
P a, b;
if (a == b) { }                            // ❌ ERROR

struct P {
    int x, y;
    bool operator==(const P&) const = default;   // C++20 -- member-wise ==
    auto operator<=>(const P&) const = default;  // C++20 -- member-wise ordering (saare 6 operators)
};
```

`= default` → compiler member-by-member comparison bana deta hai. Apna logic chahiye → body khud likho
(folder 15, 18). (`= default` member ya friend hi ho sakta hai — file 01, exercise 5.)

---

## Andar kya hota hai — naapa hua, platform ke hisaab se

GCC 16.2, Windows x64, `-O2`, `[[gnu::noinline]]` functions ka assembly dekha:

```
ts_val(Timestamp):          // Timestamp = 8 bytes, BY VALUE
    mov  rax, rcx           //   poora struct rcx REGISTER mein aaya (sec neeche, nsec upar)
    shr  rax, 32
    add  eax, ecx

pt_val(Point):              // Point = 16 bytes, BY VALUE
    movsd xmm0, [rcx]       //   rcx mein POINTER aaya -- memory se load
    addsd xmm0, [rcx+8]

pt_ref(Point const&):       // wahi Point, const& se
    movsd xmm0, [rcx]       //   BILKUL same code
    addsd xmm0, [rcx+8]
```

| Situation | Windows x64 (is box pe dekha) | Linux / SysV ABI (ABI ke niyam, is box pe chalaya nahi) |
|---|---|---|
| 8-byte trivial struct by value | ek register (`rcx`) | ek register (`rdi`) |
| 16-byte struct (`Point{double,double}`) by value | **pointer** (caller copy banata hai, callee `[rcx]` se padhta hai) | do registers (`xmm0`, `xmm1`) |
| 16-byte struct `const&` | pointer | pointer |
| 8-byte return | `rax` | `rax` |
| 16-byte `Point` return | hidden pointer mein likha (`[rcx]`) | `xmm0`, `xmm1` mein |
| Bada struct return | hidden pointer (RVO: seedha caller ki jagah) | hidden pointer |

Iska matlab:
- **Windows x64 pe** sirf 1, 2, 4, 8 byte ke structs register mein jaate hain. 16-byte `Point` by value
  lene se kuch nahi milta — callee ka code `const&` jaisa hi hai, upar se caller ko ek copy banani padti hai.
- **Linux pe** wahi `Point` by value registers mein jaata hai aur memory round-trip bachta hai — isliye
  wahan "chhota struct by value" kabhi `const&` se tez hota hai.
- **`-O2` chhote functions inline kar deta hai** → tab yeh saara parameter/return ka khel gayab, compiler
  seedha caller ke struct pe kaam karta hai. ABI ka fark sirf non-inlined, garam functions pe dikhta hai.

> **HFT relevance:** Hot-path functions order/message structs `const&` se lete hain (batches ke liye
> `std::span<const Msg>`) — galti se by-value `Order` ya `MarketDataMsg` copy = `memcpy` (+ `std::string`
> ho to allocator). Chhote POD structs (`Price`, `Timestamp`) by value idiomatic hain. Par "16-byte struct
> by value register mein jaata hai" **platform pe depend** karta hai — HFT production aam taur pe Linux
> hai (wahan sach), par Windows pe wahi code pointer banata hai. Results value se return (RVO). POD keys ke
> liye `= default` comparisons. Folders 18, 34 (calling conventions), 36.

---

## Hands-on

`examples/01_struct_basics.cpp` (`const&` params, return by value), aur:

```cpp
struct Big { double d[100]; };
// benchmark: void f(Big) vs void f(const Big&), 1M calls, -O2
```

```bash
./build.ps1 11-STRUCTS/examples/01_struct_basics.cpp
# khud assembly dekho:
g++ -std=c++20 -O2 -S -masm=intel file.cpp -o file.s    # [[gnu::noinline]] lagao warna inline ho jaayega
```

---

## ⚠️ Traps

### Trap 1 — bada struct by value
```cpp
void render(Scene s);   // ⚠️ poora scene copy. const Scene& lo
```

### Trap 2 — `return std::move(local)`
```cpp
Order f() { Order o; ...; return std::move(o); }   // ⚠️ NRVO tootta hai. return o;
```

### Trap 3 — return ki jagah output parameter
```cpp
void topOfBook(const Book& b, BestQuote& out);   // ⚠️ bhaari. BestQuote return karo (RVO)
```

### Trap 4 — bina define kiye struct pe `==`
```cpp
if (order1 == order2) { }   // ❌ ERROR. = default (C++20) ya operator== likho
```

### Trap 5 — "chhota struct by value hamesha tez" maan lena
```cpp
int f(Point p);          // 16-byte Point: Linux pe registers; Windows x64 pe pointer -- const& jaisa hi
```
Garam, non-inlined leaf function hai to apne target platform pe assembly dekho aur naapo.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Structs hamesha `const&` se pass karo" | Chhote trivial structs by value theek; tez hain ya nahi — ABI pe depend |
| "16-byte struct by value hamesha registers mein" | Linux (SysV) pe haan; Windows x64 pe pointer (assembly se dekha) |
| "Bada struct return karne ke liye out-param chahiye" | Value se return — RVO free hai |
| "`return std::move(x)` tez hai" | NRVO tootta hai — seedha `return x;` |
| "Structs default mein `==` se compare hote hain" | Nahi — `= default` (C++20) ya khud likho |
| "Struct ki by-value copy bas `memcpy` hai" | Sirf trivial members pe; `std::string` member → allocation |

---

## Exercises

1. **Param cost:** `struct Buf { char b[1024]; };` — `void f(Buf)` vs `void f(const Buf&)`, 1M calls,
   `-O2`, time lo. Ab `struct Small { int a, b; };` — wahi test. Kaunsa ulta pada?

2. **RVO:** local return karne wala `Order makeOrder(...)`. `-O2 -S -masm=intel` — koi copy/move constructor
   `call` dikha?

3. **`return std::move`:** `return std::move(o);` jodo — assembly badla? (Move aa sakta hai.)

4. **Multiple return:** `struct MinMax { int lo, hi; };  MinMax range(std::span<const int>)`. Call site pe
   structured binding.

5. **`= default` compare:** `struct Key { int a; std::string b; };` — `operator==` aur `operator<=>`
   `= default` jodo. `Key` ko `std::set` aur `std::unordered_map` mein use karo (doosre ko hash chahiye —
   charcha karo).

6. **Move-in:** `struct Job { std::string name; std::vector<int> input; };  void submit(Job);` —
   `std::move` ke saath call karo. Copies vs moves gino.

7. **ABI khud dekho:** `struct T8 { int a, b; };` aur `struct P16 { double x, y; };` — dono ke liye
   `[[gnu::noinline]]` by-value aur `const&` functions likho, `-O2 -S` se assembly dekho. Kaunsa register mein
   aaya, kaunsa pointer se?
   <details><summary>Answer (Windows x64, GCC 16.2)</summary>

   `T8` by value: poora struct `rcx` register mein. `P16` by value: `rcx` mein pointer, code `const&`
   version jaisa hi (`movsd xmm0, [rcx]`). Linux pe `P16` by value `xmm0`/`xmm1` mein aata (SysV ABI).
   </details>

---

## Interview questions

1. Struct pass by value vs `const&` — kab kaunsa? Small struct exception?
2. Struct return by value — RVO, cost?
3. `return std::move(localStruct)` kyun galat?
4. Function se 2+ related values return — struct kyun better than `pair`/out-param?
5. Struct `==` / `<=>` — `= default` (C++20) kya karta hai?
6. Struct-by-value ki cost trivial vs non-trivial members?
7. 16-byte struct by value — Linux aur Windows x64 pe kaise pass hota hai?

---

## Next
→ [`05-padding-and-alignment.md`](05-padding-and-alignment.md)

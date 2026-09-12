# 06 — References in loops (`for (auto& x : v)`)

## Prerequisites
- [`03-references-as-parameters.md`](03-references-as-parameters.md)
- Folder 07 file 02 (range-based `for`, copy vs const-ref — measured)

## Yeh topic abhi kyun
Range-based `for` mein loop variable ka type — `auto`, `auto&`, `const auto&` —
har iteration pe ya to ek **copy** banata hai ya ek **alias**. Bade elements pe
yeh galti har iteration mein ek allocation/copy add kar deti hai. Folder 07 mein
yeh ~50x measure hua tha; yahan reference angle se poora karte hain.

---

## Teen forms

```cpp
std::vector<std::string> names = {...};

for (auto x : names)        { /* x har baar ek COPY */ }
for (auto& x : names)       { /* x har element ka ALIAS -- modify kar sakte ho */ }
for (const auto& x : names) { /* x har element ka read-only alias -- no copy, no modify */ }
```

Desugared (kya compiler banata hai):

```cpp
{
    auto&& __range = names;
    auto __b = std::begin(__range);
    auto __e = std::end(__range);
    for (; __b != __e; ++__b) {
        auto x = *__b;          // <-- yahan aapka form: 'auto' -> copy, 'auto&' -> bind
        // body
    }
}
```

`*__b` element ka reference deta hai. `auto x` usse **copy** karta hai. `auto& x`
usse **bind** karta hai (zero copy).

---

## Decision rule

| Chahiye | Form |
|---|---|
| Elements ko **modify** karna | `for (auto& x : v)` |
| Sirf **padhna**, element bada (`string`, `vector`, `struct`) | `for (const auto& x : v)` |
| Sirf padhna, element **chhota POD** (`int`, `double`, pointer) | `for (auto x : v)` (ya `const auto&` — dono theek) |
| Jaan-boojh kar har element ki **apni copy** chahiye (modify without touching original) | `for (auto x : v)` |

Default sochne ka tareeka: **`const auto&` likho jab tak modify na karna ho ya
element sach mein POD na ho.**

---

## Modify karna — `auto&` zaroori

```cpp
std::vector<int> v = {1, 2, 3};

for (auto x : v)  x *= 10;      // ⚠️ kuch nahi hua -- x ek copy thi
for (auto& x : v) x *= 10;      // ✅ v ab {10, 20, 30}
```

`const auto&` bhi modify nahi karne dega:

```cpp
for (const auto& x : v) x *= 10;   // ❌ compile ERROR -- x const hai
```

---

## Chhupa hua copy — type mismatch

```cpp
std::map<std::string, int> m = {...};

for (const auto& [k, v] : m)          { }   // ✅ element type: std::pair<const std::string, int>
for (const std::pair<std::string, int>& p : m) { }   // ⚠️ COPY har iteration!
```

Doosri line galat hai: map ka element `std::pair<const std::string, int>` hai
(key `const`). `std::pair<std::string, int>` alag type hai → har iteration ek
**temporary** banti hai, aur `const&` usse bind hokar copy ko hi zinda rakhta
hai. `-Wrange-loop-construct` (GCC/Clang) yeh warn karta hai. Fix: `auto`
istemaal karo — `for (const auto& [k, v] : m)`.

---

## ⚠️ Range expression khud ek temporary — lifetime

```cpp
for (auto& x : makeVector()) { ... }   // ✅ (C++11+) -- makeVector() ka temp
                                       //    poore loop tak zinda (range-for extend karta hai)

for (auto& x : getObj().itemsRef()) { ... }   // ⚠️ getObj() ka temp is line ke baad marta;
                                              //    itemsRef() ek ref deta hai us mare hue
                                              //    object ke andar -> dangling loop! (C++23 se fix)
```

Pehli form safe hai — pura temporary range-for ke andar lifetime-extend hota hai.
Doosri form **classic bug**: `getObj()` ka temporary `.itemsRef()` ke baad marta
hai, aur loop ek dead container par chalta hai. (C++23 ne range-for temporary
lifetime rules extend karke isse theek kiya, par `-std=c++20` pe yeh UB hai.)

Safe: `auto obj = getObj(); for (auto& x : obj.itemsRef()) { ... }`.

---

## Andar kya hota hai

- **`const auto& x`** → loop body mein `x` = `*__b` (ek pointer deref). Zero
  per-iteration allocation. Compiler aksar isse aur simplify karke seedha index
  pe pahunch jaata hai.
- **`auto x` on `std::string`** → har iteration: copy ctor → agar string > SSO
  (libstdc++ 15 chars) to `operator new` + `memcpy`, phir iteration ke end pe
  `operator delete`. 1e6 elements = 2e6 heap ops jo `const auto&` mein zero the.
  Folder 07 file 02 mein 200-char strings pe yeh **~50x** slowdown tha.
- **`auto x` on `int`** → ek register move. `const auto& x` on `int` → ek
  address + ek load. Practically dono `-O2` pe barabar; POD ke liye `auto x`
  thoda zyada direct.
- **`auto&` vs iterator invalidation:** loop ke andar `v.push_back(...)` mat
  karo — element references (aur `auto& x`) realloc pe dangle (file 09,
  `04_dangling_reference.cpp` BUG 2).

> **HFT relevance:** market-data replay / book rebuild loops millions of
> elements par chalte hain. `for (const auto& evt : events)` vs
> `for (auto evt : events)` — agar `evt` 64+ bytes ka struct hai, doosri form
> har iteration ek copy + extra cache traffic deti hai. Aur agar element ke
> andar koi heap-owning member (`std::string`, `std::vector`) ho to har
> iteration alloc/free. `const auto&` default hai; `auto&` jab in-place update
> (e.g. book level ki qty adjust) karna ho.

---

## Hands-on

```bash
./build.ps1 fast 07-LOOPS/examples/02_range_based.cpp
```

(Folder 07 ka example — copy vs `const&` range-for ~50x measured.) Aur khud
likho: `std::vector<std::array<int,16>>` pe `auto` vs `const auto&` loop, `-O2`
pe time.

---

## ⚠️ Traps

### Trap 1 — modify karne ke liye `auto` (bina `&`)
```cpp
for (auto x : v) x = clean(x);   // ⚠️ v unchanged -- x copy thi
```

### Trap 2 — `const&` ke saath galat element type
```cpp
for (const std::pair<std::string,int>& p : myMap)   // ⚠️ har iter copy (key const hai map mein)
for (const auto& p : myMap)                          // ✅
```

### Trap 3 — `getTemp().refMember()` par loop (C++20)
```cpp
for (auto& x : buildConfig().sections()) { }   // ⚠️ buildConfig() temp gaya -> dangling loop
```

### Trap 4 — loop ke andar container resize
```cpp
for (auto& x : v) if (cond(x)) v.push_back(...);   // ⚠️ realloc -> x aur iterators dangling
```

### Trap 5 — `auto&&` ka galat matlab
```cpp
for (auto&& x : v) { }   // yeh "forwarding reference" hai -- lvalue range pe `auto&` jaisa.
                         // Bura nahi, par jaanbujh ke `const auto&` / `auto&` likho -- intent saaf
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`for (auto x : v)` reference hai" | Copy. Alias ke liye `auto&` / `const auto&` |
| "`const auto&` sirf style hai" | Bade elements pe har iteration ka copy/alloc bachata |
| "map pe `const pair<string,int>&` theek hai" | Key `const` hai → copy. `const auto&` likho |
| "`for (auto& x : makeVec())` dangling hai" | Safe — range temp poore loop tak extend |
| "`for (auto& x : getObj().ref())` bhi safe" | C++20 mein dangling — pehle `auto o = getObj();` |

---

## Exercises

1. **Predict:** `std::vector<int> v{1,2,3};`
   ```cpp
   for (auto x : v)  x += 100;
   for (auto& x : v) x += 100;
   ```
   Har loop ke baad `v`?

   <details><summary>Answer</summary>

   Pehle ke baad `v` = `{1,2,3}` (copies modify hui). Doosre ke baad `{101,102,103}`.
   </details>

2. **Copy count:** `std::vector<std::string> v` mein 1000 strings, har 100 chars
   (SSO se bada). `for (auto s : v) len += s.size();` vs `for (const auto& s :
   v) ...`. Har version mein kitni heap allocations (approx)?

   <details><summary>Answer</summary>

   `auto s` → ~1000 allocations (har iteration ek string copy, 100 > 15 SSO
   limit → heap) + ~1000 frees. `const auto&` → **0** (sirf alias). Folder 07
   file 02 mein 200-char pe ~50x time farq.
   </details>

3. **Map trap:** `std::map<std::string,int> m` — `for (const std::pair<std::string,
   int>& kv : m)` warning kaunsi, kyun copy banti hai, fix?

   <details><summary>Answer</summary>

   `-Wrange-loop-construct`. Map ka value_type `std::pair<const std::string,
   int>` hai; `pair<std::string,int>` alag type → implicit conversion → per-iter
   temporary. Fix: `for (const auto& kv : m)`.
   </details>

4. **Dangling loop:** `struct Cfg { std::vector<int> nums; std::vector<int>&
   ref() { return nums; } }; Cfg makeCfg();` — `for (auto& n : makeCfg().ref())`
   kyun UB (`-std=c++20`)? Fix.

   <details><summary>Answer</summary>

   `makeCfg()` temporary `.ref()` call ke turant baad (full-expression end)
   destroy hota hai; loop us dead `Cfg` ke `nums` par chalta hai. Fix: `auto cfg
   = makeCfg(); for (auto& n : cfg.ref()) { ... }`.
   </details>

5. **POD case:** `std::vector<double> v` (1e7). `for (auto x : v) s += x;` vs
   `for (const auto& x : v) s += x;` — `-O2` pe time. Farq meaningful?

   <details><summary>Answer</summary>

   `double` register mein aata hai; dono forms `-O2` pe lagbhag identical
   (compiler `const auto&` ko bhi seedha load bana deta). POD ke liye `auto x`
   bilkul theek — copy-cost yahan zero-ish.
   </details>

---

## Interview questions

1. `for (auto x : v)` vs `for (auto& x : v)` vs `for (const auto& x : v)` — kab kaunsa?
2. Range-based `for` desugar karke dikhao — copy kahan hota hai?
3. `for (const pair<string,int>& p : map)` slow kyun? warning kaunsi?
4. `for (auto& x : getObj().ref())` — C++20 mein kya problem, kaise fix?
5. Loop body mein `v.push_back()` — `auto& x` ke saath kya hota hai?
6. POD elements ke liye `const auto&` vs `auto` — koi practical farq?

---

## Next
→ [`07-reference-members.md`](07-reference-members.md)

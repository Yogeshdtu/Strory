# 04 — Range-based `for` (C++11)

## Prerequisites
- [`03-for-loop.md`](03-for-loop.md)
- `03-VARIABLES-DATA-TYPES/12-auto-and-type-deduction.md` (`auto`)
- `03-VARIABLES-DATA-TYPES/13-type-conversions.md` (copies)
- `05-OPERATORS/08-ternary-operator.md` mein references chhue the; poora folder 13

## Yeh topic abhi kyun
Zyada tar loops "har element pe kuch karo" hote hain. Classic `for` mein iske liye
index, size, aur `[]` — teen jagah galti ho sakti hai (off-by-one, wrong size,
signed/unsigned). Range-based `for` (C++11) yeh sab hata deta hai:

```cpp
for (const auto& x : container) { use(x); }
```

Par ismein ek chhupa hua trap hai: **`auto` bina `&` ke har element ki copy
banata hai.** Bade elements pe yeh bahut slow ho sakta hai — hum measure karenge.

---

## Syntax

```cpp
for (declaration : range) {
    // body -- har element ke liye ek baar
}
```

```cpp
std::vector<int> v = {10, 20, 30};
for (int x : v) {
    std::cout << x << " ";        // 10 20 30
}
```

Kya kya chal sakta hai `range` ki jagah:
- STL containers: `vector`, `array`, `map`, `set`, `string`, …
- C-style arrays (jab tak size compiler ko pata ho — pointer decay ke baad **nahi**)
- `std::initializer_list`: `for (int x : {1, 2, 3})`
- Koi bhi type jisme `begin()` / `end()` ho

### Andar se yeh classic `for` hai

```cpp
for (auto x : v) { body; }
```

roughly expand hota hai:

```cpp
{
    auto&& __range = v;
    auto __begin = std::begin(__range);
    auto __end   = std::end(__range);
    for (; __begin != __end; ++__begin) {
        auto x = *__begin;        // <-- yahan copy ya reference decide hota hai
        body;
    }
}
```

Woh `auto x = *__begin;` line hi sab kuch hai.

---

## 🔑 `auto` vs `auto&` vs `const auto&`

| Form | `x` kya hai | Modify? | Copy? | Kab |
|---|---|---|---|---|
| `for (auto x : c)` | har element ki **copy** | copy badalta hai, original nahi | **haan** | chhoti trivial values (`int`, `char`, pointer) jab copy chahiye |
| `for (auto& x : c)` | element ka **reference** | **haan, original badalta hai** | nahi | in-place modify karna ho |
| `for (const auto& x : c)` | element ka **read-only reference** | nahi | nahi | **sirf padhna** — yeh DEFAULT choice |

```cpp
std::vector<int> v = {1, 2, 3};

for (auto x : v)        x *= 10;      // v unchanged (copy modify hui)
for (auto& x : v)       x *= 10;      // v ab {10, 20, 30}
for (const auto& x : v) sum += x;     // read only; x *= 2 -> compile error
```

### Rule of thumb

```
Sirf padhna hai?           -> const auto&
Modify karna hai?          -> auto&
Chhoti value + copy chahiye -> auto
```

Jab confuse ho — `const auto&` default rakho. Chhoti types (`int`) pe `const int&`
aur `int` ka performance farq zero hai; bade types pe `const auto&` bahut bacha
leta hai.

---

## COPY ki asli cost — measured

`examples/02_range_based.cpp` — 200,000 strings, har ek 64 chars:

```cpp
std::vector<std::string> names(200000, std::string(64, 'x'));

for (auto s : names)        lenByCopy += s.size();   // har iteration: string COPY (heap alloc + memcpy + free)
for (const auto& s : names) lenByRef  += s.size();   // koi copy nahi
```

Measured (GCC 15.1, `-O2`, x86-64):

```
  for (auto s : names)        :  ~31 ms
  for (const auto& s : names) :  ~0.6 ms
                                 --------
                                 ~50x slower
```

Copy version har iteration mein ek 64-byte `std::string` banata hai — **heap
allocation + memcpy + free**. 200k iterations = 200k malloc/free pairs, kuch bhi
"kaam" kiye bina.

> **HFT relevance:** `for (auto order : orderBook)` jaisa code ek accidental
> `Order` copy per iteration bana deta hai — hot path mein yeh microseconds jod
> deta hai aur allocator lock/heap ko chhoo sakta hai. HFT code review mein
> `for (auto x :` bina `&` ke ek red flag hai. Hamesha `const auto&` / `auto&`.
> `-Wrange-loop-construct` (Clang) is copy pe warn karta hai.

---

## Structured bindings — `map` / `pair` / `tuple` / structs

```cpp
std::map<std::string, int> stock = {{"AAPL", 100}, {"MSFT", 50}};

for (const auto& [symbol, qty] : stock) {     // pair ke .first/.second ko naam mile
    std::cout << symbol << " -> " << qty << "\n";
}

for (auto& [symbol, qty] : stock) {
    qty += 1;                                  // value modify (key const rehti hai)
}
```

`auto& [k, v]` vs `const auto& [k, v]` vs `auto [k, v]` — wahi rules jaise upar.

---

## ⚠️ Traps

### Trap 1 — bina `&` ke bade elements copy
```cpp
for (auto row : matrix) { }        // ⚠️ har row (poora vector!) copy
for (const auto& row : matrix) { } // ✅
```

### Trap 2 — iterate karte waqt container ka size badalna
```cpp
for (const auto& x : v) {
    if (cond(x)) v.push_back(...);  // 💥 reallocation -> x dangling -> UB
}
```
Range-`for` cache karta hai `begin`/`end` shuru mein. Container modify karna ho to
classic index loop ya `erase`-`remove` idiom (folder 19).

### Trap 3 — `auto&` se `std::vector<bool>`
```cpp
std::vector<bool> flags(10);
for (auto& b : flags) b = true;    // ⚠️ vector<bool> proxy deta hai -- `auto&` yahan alag behave karta
```
`vector<bool>` special hai (bit-packed). `for (auto b : flags)` ya
`flags[i] = true` use karo. (Folder 19.)

### Trap 4 — temporary range ka dangling (pre-C++23)
```cpp
for (auto x : getConfig().items()) { }   // ⚠️ C++20: getConfig() temporary destroy, items() dangling
```
C++23 ne yeh fix kar diya, par pre-C++23 compilers pe: range ko pehle ek variable
mein le lo. `for (auto&& r = getConfig(); const auto& x : r.items())` (C++20
init-form) bhi option hai.

### Trap 5 — index chahiye tha
```cpp
for (const auto& x : v) { }        // index nahi milta
for (std::size_t i = 0; i < v.size(); ++i) { }   // index chahiye to classic
// ya C++20:
for (std::size_t i = 0; const auto& x : v) { use(i, x); ++i; }   // init-statement
// ya std::views::enumerate (C++23 / ranges-v3)
```

---

## Andar kya hota hai / performance

- Range-`for` ka codegen classic index-`for` **jaisa hi** hota hai `-O2` pe
  contiguous containers (`vector`, `array`) ke liye — same vectorization, same
  cache behaviour (file 09).
- `const auto&` par element pe koi copy nahi → sirf pointer increment + deref.
- `std::map` / `std::set` pe iteration node-by-node hota hai (tree walk) — har
  step ek possible cache miss. Contiguous nahi. (Folder 19, 32.)
- `std::string` pe range-`for` byte-by-byte hai (careful: UTF-8 mein ek "character"
  multi-byte ho sakta hai — folder 10).

---

## Hands-on

`examples/02_range_based.cpp` — copy vs reference, modify, structured bindings,
C-array, aur **measured ~50x copy cost**:

```bash
./build.ps1 fast 07-LOOPS/examples/02_range_based.cpp    # -O2 for the benchmark
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`for (auto x : v)` reference hota hai" | **Copy**. Reference chahiye to `auto&` / `const auto&` |
| "Chhoti types pe `const auto&` slow" | Farq zero; safe default |
| "Range-`for` classic `for` se slow" | Contiguous containers pe same codegen |
| "Iterate karte waqt `push_back` safe hai" | Reallocation → dangling → UB |
| "Index milta hai range-`for` mein" | Nahi — classic `for` ya C++20 init-statement / enumerate |
| "`for (auto& b : vector<bool>)` normal hai" | `vector<bool>` proxy — special-case |

---

## Exercises

1. **Copy vs ref:**
   ```cpp
   std::vector<int> v = {1, 2, 3};
   for (auto x : v) x += 100;
   for (int x : v) std::cout << x << " ";
   ```
   Output? Ab `auto x` ko `auto& x` karo — output?
   <details><summary>Answer</summary>Pehle: `1 2 3` (copy). `auto&` ke saath: `101 102 103`.</details>

2. **Measure:** `examples/02_range_based.cpp` chalao. `auto s` vs `const auto& s`
   ka ratio apni machine pe likho.

3. **Structured bindings:** ek `std::map<std::string, double>` prices banao, sabko
   `const auto& [sym, px]` se print karo. Phir sabko 1% badhao (`auto& [sym, px]`).

4. **Modify in place:** ek `std::vector<int>` ke har element ko uske square se
   replace karo, range-`for` se.

5. **Dangling trap:** yeh kyun UB hai, aur do fix —
   ```cpp
   for (char c : std::string("hello") + "!") std::cout << c;
   ```
   (Hint: C++20 pe. Actually yeh specific case theek hai kyunki full-expression
   lifetime extend hoti hai — par `getStr().substr(0,3)` pe nahi. Test dono.)

6. **Index chahiye:** C++20 init-statement se ek `vector<std::string>` ko
   `"[0] apple"`, `"[1] banana"` style mein print karo.

7. **`vector<bool>` gotcha:** `std::vector<bool> b(5)` — `for (auto& x : b) x = true;`
   compile hua? `for (auto x : b)` se sab print karo.

---

## Interview questions

1. `for (auto x : v)` mein `x` copy hai ya reference? Kaise pata?
2. `auto` / `auto&` / `const auto&` — kab kaunsa?
3. Range-`for` andar se kis code mein expand hota hai?
4. Range-`for` chalte waqt container mein `push_back` — kya hota hai?
5. Range-`for` classic index `for` se performance mein alag? (Contiguous vs node-based)
6. `std::map` pe structured binding — `[k, v]` mein `k` modify kar sakte ho?
7. Range-`for` mein index kaise laayein (2 tareeke)?

---

## Next
→ [`05-nested-loops.md`](05-nested-loops.md)

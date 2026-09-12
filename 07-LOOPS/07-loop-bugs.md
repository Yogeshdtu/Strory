# 07 — Loop bugs — catalogue

## Prerequisites
- [`01-while-loop.md`](01-while-loop.md) … [`06-break-continue.md`](06-break-continue.md)
- `06-CONDITIONS/07-conditional-bugs.md` (`=` vs `==`, unsigned, stray `;`)
- `05-OPERATORS/03-comparison-operators.md` (signed/unsigned, float, NaN)

## Yeh topic abhi kyun
Loops se aane wale bugs 3 flavours mein aate hain: **off-by-one** (ek zyada / ek
kam iteration), **infinite** (condition kabhi false nahi hoti), aur **out-of-bounds
/ invalid access** (index range se bahar). Zyada tar compile ho jaate hain aur
chal jaate hain — bas galat.

Yeh lesson unka catalogue hai + `examples/04_loop_bugs.cpp` jisme saat live demo
hain (har ek safety-capped, program hang nahi hoga).

---

## Bug 1 — Off-by-one (`<=` vs `<`)

```cpp
int a[5];                       // valid index: 0, 1, 2, 3, 4
for (int i = 0; i <= 5; ++i)    // ⚠️ i == 5 pe a[5] -> OUT OF BOUNDS (UB)
    a[i] = 0;
```

**Half-open range `[0, n)` convention:** `n` elements ke liye `i` `0` se `n-1`
tak. Condition `i < n`. `<=` matlab ek extra iteration + `a[n]` access.

### Fixes
```cpp
for (int i = 0; i < 5; ++i) a[i] = 0;          // ✅
for (int& x : a) x = 0;                          // ✅ range-for -- index hi nahi
for (std::size_t i = 0; i < std::size(a); ++i)  // ✅ size khud se
```

### Doosra rukh — ek KAM
```cpp
for (int i = 1; i < n; ++i)     // ⚠️ index 0 miss (shuru 1 se)
for (int i = 0; i < n - 1; ++i) // ⚠️ aakhri element miss (jaan-boojh kar ho to theek)
```

Off-by-one boundaries pe hota hai: `0` vs `1` start, `< n` vs `<= n`, `n-1` vs `n`.
Har naya loop likhte waqt: "kitni iterations? pehla index? aakhri index?"

---

## Bug 2 — Infinite: update missing / progress nahi

```cpp
int i = 0;
while (i < 5) {
    std::cout << i;
    // ⚠️ ++i bhool gaye
}
```

`for` mein iteration-expression alag hai isliye kam hota hai — par `for` mein bhi:

```cpp
for (int i = 0; i < n; )        // ⚠️ update khaali, aur body mein bhi nahi
    process(i);
```

**Rule:** har loop ke liye poocho — "kaunsa variable / state har iteration mein
condition ko false ke kareeb le jaata hai?" Agar jawab nahi hai → infinite.

---

## Bug 3 — Infinite: unsigned reverse loop

```cpp
std::vector<int> v = {1, 2, 3};
for (std::size_t i = v.size() - 1; i >= 0; --i)    // ⚠️
    process(v[i]);
```

Do problems:
1. `i` ka type `std::size_t` (**unsigned**) — `i >= 0` **hamesha true**
2. `i == 0` pe `--i` → wrap → `18446744073709551615` → `v[huge]` OOB

`-Wall -Wextra` → `-Wtype-limits`: *"comparison of unsigned expression `>= 0` is
always true"*.

### Fixes
```cpp
for (std::size_t i = v.size(); i-- > 0; )   process(v[i]);   // ✅ "i-- > 0" idiom
for (auto i = std::ssize(v) - 1; i >= 0; --i) process(v[i]); // ✅ signed size (C++20)
for (auto it = v.rbegin(); it != v.rend(); ++it) process(*it); // ✅ reverse iterator
for (const auto& x : v | std::views::reverse) process(x);      // ✅ C++20 ranges
```

The `i-- > 0` idiom: condition me `i` ka purana value use hota hai (`i > 0`?),
phir `i` decrement. Jab `i == 0`: `0 > 0` false → loop khatam, `i` `SIZE_MAX` ban
gaya par use nahi hua.

---

## Bug 4 — Infinite: float loop counter

```cpp
for (double x = 0.0; x != 1.0; x += 0.1)     // ⚠️ x kabhi thik 1.0 nahi
    use(x);
```

`0.1` binary floating-point mein exact nahi (folder 03 file 06, folder 05 file 03).
10 baar jodne pe `x` ≈ `0.9999999999999999` — `!= 1.0` still true → next `1.0999…`
→ overshoot → (best case) bahut der baad rukta, (worst case) `<` na hone pe never.

`examples/04_loop_bugs.cpp` BUG 4 output (17-digit precision):
```
0  0.10000000000000001  0.20000000000000001  ...  0.99999999999999989  1.0999999999999999
```

### Fix — integer counter, phir scale
```cpp
for (int k = 0; k < 10; ++k) {
    double x = k * 0.1;      // 0.0, 0.1, ..., 0.9  (har baar fresh, error jama nahi hota)
    use(x);
}
```

Agar float condition zaroori ho: `x < 1.0` (na ki `!=`), aur epsilon ka dhyaan.

---

## Bug 5 — Counter body mein modify

```cpp
for (std::size_t i = 0; i < v.size(); ++i) {
    process(v[i]);
    ++i;                 // ⚠️ ab step 2 -> 0, 2, 4, ... -> aadhe elements skip
}
```

Loop header pehle se `++i` karta hai; body ka extra `++i` = total step 2.

### Fix — step loop header mein
```cpp
for (std::size_t i = 0; i < v.size(); i += 2) process(v[i]);
```

Ya agar conditionally skip karna ho — `continue` (file 06), counter mat chhedo.

---

## Bug 6 — Galat direction

```cpp
for (int c = 5; c > 0; ++c)     // ⚠️ c badh raha hai -> c > 0 kabhi false nahi
    countdown(c);                //    (int overflow tak, jo UB)
```

`--` chahiye tha. Ya condition `c < 5` (agar upar jaana hai).

---

## Bug 7 — `while` + `continue` → update skip

```cpp
int i = 0;
while (i < n) {
    if (shouldSkip(i)) continue;   // ⚠️ ++i skip -> i stuck -> forever
    process(i);
    ++i;
}
```

`while`/`do-while` mein `continue` **seedha condition** pe jaata hai — koi
iteration-expression nahi. `for` mein yeh bug nahi hota.

### Fixes
```cpp
for (int i = 0; i < n; ++i) {           // ✅ for: continue ke baad bhi ++i chalta hai
    if (shouldSkip(i)) continue;
    process(i);
}

int i = 0;                              // ✅ ya while mein ++i pehle
while (i < n) {
    int cur = i++;
    if (shouldSkip(cur)) continue;
    process(cur);
}
```

---

## Bonus — Iterator invalidation (preview, folder 19)

```cpp
std::vector<int> v = {1, 2, 3, 4, 5};
for (auto it = v.begin(); it != v.end(); ++it) {
    if (*it % 2 == 0) v.erase(it);      // ⚠️ erase() `it` ko invalidate karta hai -> UB
}
```

Container ko iterate karte waqt modify karna (`erase`, `push_back` jo reallocate
kare, `insert`) → iterators / references / range-`for` ka cached `end` sab
invalid ho sakte hain.

### Fixes
```cpp
for (auto it = v.begin(); it != v.end(); ) {
    if (*it % 2 == 0) it = v.erase(it);   // erase lautta hai next valid iterator
    else ++it;
}

std::erase_if(v, [](int x) { return x % 2 == 0; });   // ✅ C++20 -- best
```

Poora folder 19 mein — abhi bas yaad rakho: **iterate karte waqt container ka
structure mat badlo.**

---

## Kaunsa warning kya pakadta hai

| Bug | Warning | `-Wall -Wextra`? |
|---|---|---|
| off-by-one (const-size array) | `-Warray-bounds` (kabhi), ASan runtime | ⚠️ kabhi |
| unsigned reverse (`i >= 0`) | `-Wtype-limits` | ✅ |
| signed/unsigned compare | `-Wsign-compare` | ✅ |
| stray `;` after loop | `-Wempty-body`, `-Wmisleading-indentation` | ✅ |
| float `!=` counter | `-Wfloat-equal` (opt-in) | ❌ default |
| infinite (no update) | — | ❌ (logic bug) |
| iterator invalidation | ASan / `_GLIBCXX_DEBUG` runtime | ❌ compile-time |

Jo compiler nahi pakadta — **AddressSanitizer** (`-fsanitize=address`) aur
**`-D_GLIBCXX_DEBUG`** (libstdc++ debug mode) runtime pe bahut kuch pakad lete
hain. `make san FILE=...` / `./build.ps1 san ...` use karo.

> **HFT relevance:** Off-by-one in a market-data ring buffer index = corrupt book
> ya crash. Unsigned underflow in a "bytes remaining" counter = 18-quintillion-byte
> read. Iterator invalidation in an order map during a matching sweep = UB in the
> hottest code. HFT teams: `-Wall -Wextra -Werror`, ASan/UBSan in CI, fuzzing on
> parsers, aur bounds-checked indices in debug builds (`assert(i < n)`), release
> mein `[[assume]]` / raw. Folders 35, 45.

---

## Hands-on

```bash
./build.ps1 07-LOOPS/examples/04_loop_bugs.cpp
```

Compile ke waqt `-Wtype-limits` warning padho (BUG 3). Har bug `[buggy]` vs
`[fixed]` output deta hai; notes file ke neeche.

Phir ASan ke saath:
```bash
./build.ps1 san 07-LOOPS/examples/04_loop_bugs.cpp
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`i <= n` se poora array" | `n+1` iterations, `a[n]` OOB. `i < n` |
| "`for (size_t i = n-1; i >= 0; --i)` reverse ke liye theek" | `i >= 0` hamesha true; `--i` wrap. `i-- > 0` idiom |
| "float counter fine agar `!=` ki jagah kuch aur" | Float counter hi avoid karo — integer + scale |
| "loop ke andar `erase` normal hai" | Iterator invalidation → UB; `erase_if` / erase-return |
| "compiler saare loop bugs pakad lega" | Logic bugs nahi — ASan/UBSan/tests chahiye |

---

## Exercises

1. **Har bug spot karo:**
   ```cpp
   int s = 0;
   for (int i = 1; i <= v.size(); ++i) s += v[i];
   for (size_t i = v.size() - 1; i >= 0; --i) print(v[i]);
   for (double t = 0; t != 2.0; t += 0.2) tick(t);
   for (int k = 10; k > 0; ++k) work(k);
   ```
   <details><summary>Answer</summary>
   L2: start 1 (miss v[0]), `<=` (v[size] OOB), signed/unsigned compare. L3:
   unsigned `>= 0` + wrap. L4: float `!=` counter. L5: `++k` galat direction.
   </details>

2. **Fix `04_loop_bugs.cpp` mentally:** har `[buggy]` loop ko sahi likho.

3. **Reverse 4 tareeke:** ek `vector<int>` ko ulta print karo — `i-- > 0` idiom,
   `std::ssize`, `rbegin/rend`, `views::reverse`. Sab same output?

4. **Iterator invalidation:** yeh code chalao `-fsanitize=address` ke saath —
   ```cpp
   std::vector<int> v = {1,2,3,4,5,6};
   for (auto it = v.begin(); it != v.end(); ++it)
       if (*it % 2 == 0) v.erase(it);
   ```
   ASan ne kya bola? Ab `std::erase_if` se fix karo.

5. **Off-by-one lab:** ek function `int sumRange(const int* a, int lo, int hi)` —
   `a[lo]` se `a[hi]` tak (inclusive) sum. Boundaries theek karo. Test: `lo == hi`,
   `lo > hi`, poora array.

6. **ASan catch:** `int a[10]; for (int i = 0; i <= 10; ++i) a[i] = i;` — `make san`
   / `./build.ps1 san` se chalao. Error message likho.

7. **Infinite hunt:** 3 alag infinite loops likho (missing update, wrong direction,
   `while`+`continue`), har ek ko fix karo.

---

## Interview questions

1. Off-by-one bug kaise hota hai? Half-open range convention se kaise bachte hain?
2. `for (size_t i = n-1; i >= 0; --i)` mein 2 bugs? 3 correct reverse patterns?
3. Float ko loop counter kyun nahi banate? Sahi tareeka?
4. `while` + `continue` se infinite loop — mechanism aur fix?
5. Iterator invalidation kya hai? Loop ke andar `erase` kaise karte hain?
6. Kaunse loop bugs `-Wall -Wextra` pakadta hai, kaunse ke liye ASan/tests chahiye?

---

## Next
→ [`08-loop-patterns.md`](08-loop-patterns.md)

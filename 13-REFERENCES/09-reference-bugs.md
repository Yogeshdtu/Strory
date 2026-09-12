# 09 — Reference bugs — dangling, lifetime, hidden copies

## Prerequisites
- [`04-const-references.md`](04-const-references.md), [`05-returning-references.md`](05-returning-references.md), [`06-references-in-loops.md`](06-references-in-loops.md), [`07-reference-members.md`](07-reference-members.md)
- Folder 12 file 13 (pointer bug catalog — bahut overlap)

## Yeh topic abhi kyun
Reference "safe pointer" hai — par sirf **use-syntax** safe hai. Lifetime aap ke
haath mein hai, aur galti hone pe symptom pointer bug jaisa hi: dangling,
use-after-free, silent corruption, ya "test mein chalta, prod mein nahi". Ek
catalog + har ek ka detection.

---

## Bug 1 — reference to returned local

```cpp
int& make() { int x = 42; return x; }        // ⚠️ -Wreturn-local-addr
int& r = make();
use(r);                                       // UB -- x return pe mar gaya
```

**Detect:** `-Wreturn-local-addr` (GCC/Clang, on by `-Wall`). ASan:
`stack-use-after-return`.
**Fix:** value return karo (`int make()`), ya referent ko caller/`static`/member
mein zinda rakho.

---

## Bug 2 — reference to a temporary (extension nahi hui)

```cpp
const int& r = std::min(3, 7);               // ⚠️ 3,7 temporaries; min ek ka ref lautata; dono line ke baad gaye
const std::string& s = obj.name();           // agar name() by value lautata -> s dangling
const int& t = identity(42);                 // extension call ke through nahi jaati
```

**Detect:** `-Wdangling-reference` (GCC 13+), kuch cases `-Wdangling` / clang
`-Wdangling-gsl`. ASan: `stack-use-after-scope` / heap-use-after-free.
**Fix:** `auto` se value pakdo (`auto r = std::min(3,7);`), ya direct bind
(`const int& r = 42;`).

---

## Bug 3 — container reallocation reference ko invalidate karti hai

```cpp
std::vector<int> v{1, 2, 3};
int& first = v[0];
v.push_back(4);              // capacity full -> naya buffer -> first ab freed memory
first = 99;                  // ⚠️ heap-use-after-free
```

Yeh sirf `push_back` nahi — `insert`, `resize`, `reserve` (bada), `emplace_back`,
sab jab realloc ho. `std::string` ka bhi same. `std::deque`/`std::list`/`std::map`
ke apne rules (mostly stable, par `deque` insert middle nahi).

**Detect:** `04_dangling_reference.cpp` BUG 2 style — `&first` vs `&v[0]` compare.
ASan: heap-use-after-free. `_GLIBCXX_ASSERTIONS` yahan help nahi karta (index
valid hai, memory freed hai).
**Fix:** reference ko store mat karo across mutation; ya index rakho (`std::size_t
i = 0;` phir `v[i]`); ya pehle `v.reserve(final_size)`.

---

## Bug 4 — range-`for` over a temporary's reference-returning method

```cpp
for (auto& x : getConfig().items())          // ⚠️ getConfig() temp .items() ke baad marta;
    process(x);                              //    loop dead object par (C++20)
```

**Detect:** GCC `-Wdangling-reference` kabhi kabhi; warna sirf review / ASan.
**Fix:** `auto cfg = getConfig(); for (auto& x : cfg.items()) ...`.

---

## Bug 5 — reference member bound to a temporary / short-lived object

```cpp
struct View { const std::string& s; View(const std::string& x): s(x) {} };
View v{ std::string("hi") };                  // ⚠️ temp gaya -> v.s dangling
// ya:
View makeView() { std::string local = "hi"; return View{local}; }   // ⚠️ local gaya
```

**Detect:** `-Wdangling-reference`, `-Winit-list-lifetime` (kuch cases).
**Fix:** member ko pointer banao (nullable, explicit), ya value member (own the
data), ya guarantee karo bound object longer-lived hai.

---

## Bug 6 — hidden copy via reference bind (type mismatch)

```cpp
long counter = 0;
const int& r = counter;        // ⚠️ 'counter' long hai -> ek int TEMPORARY bani, r usse bind
counter = 5;
std::cout << r;                // 0 -- r us temp int ka alias hai, counter ka nahi
```

Ya loop mein (file 06):

```cpp
for (const std::pair<std::string,int>& p : myMap)   // ⚠️ per-iteration copy (map key const hai)
```

**Detect:** `-Wrange-loop-construct` (loop case). Scalar case aksar silent —
review: "reference ka type binding expression se exact match karta hai?"
**Fix:** types match karo (`const long& r = counter;`), ya `auto` use karo.

---

## Bug 7 — dangling `string_view` / `span` (reference jaisa hi)

```cpp
std::string_view sv = std::string("temp");    // ⚠️ temp gaya -> sv points to freed data (folder 10)
std::span<int>  sp  = makeVector();            // ⚠️ same
```

`string_view` / `span` "non-owning references" hain — reference lifetime rules
un par bhi lagte hain, par **koi lifetime extension nahi** (yeh sirf `T&` ko
milti hai, aur woh bhi limited).

**Detect:** `-Wdangling-gsl` (clang), GCC kuch cases. ASan.
**Fix:** owner ko named variable mein rakho jab tak view zinda hai.

---

## Bug 8 — self-reference se aliasing bug

```cpp
void append(std::vector<int>& dst, const std::vector<int>& src) {
    for (int x : src) dst.push_back(x);
}
append(v, v);      // ⚠️ dst aur src same! push_back realloc -> src (== dst) ke iterators dangling mid-loop
```

**Detect:** review; ASan agar realloc hit hua. Testing with `append(v, v)`.
**Fix:** `if (&dst == &src) return handleSelf();`, ya `src` ki copy le lo, ya
index-based loop with cached `size()`.

---

## Detection cheat-sheet

| Tool | Kya pakadta |
|---|---|
| `-Wall -Wextra` | `-Wreturn-local-addr`, `-Wdangling-reference` (GCC13+), `-Wrange-loop-construct` |
| `-Wdangling` / `-Wdangling-gsl` (clang) | view/temporary bindings |
| ASan (`-fsanitize=address`) | stack-use-after-return/-scope, heap-use-after-free — **exact line** |
| UBSan | kuch reference misuse |
| `_GLIBCXX_ASSERTIONS` | container **index** OOB — dangling nahi |
| Review question | "Referent is reference se zyada der zinda rahega?" |

⚠️ **MinGW-w64 pe ASan/UBSan nahi** (folder 09/12 note) — real diagnosis
Linux/Clang/WSL pe. `./build.ps1 san` `_GLIBCXX_ASSERTIONS + stack-protector` pe
girta hai.

> **HFT relevance:** dangling reference bugs production mein sabse mehnge —
> silent memory corruption, ghante debugging, aur aksar sirf load ke under
> reproduce (jab allocator freed block reuse karta hai). Isliye: (1) accessors
> ki storage lifetime code-review hoti hai, (2) hot structures pre-sized taaki
> realloc-invalidation ka sawaal na ho, (3) CI mein ek ASan build chalta hai
> (Linux), (4) `string_view`/`span` params ke saath owner-lifetime comment.

---

## Hands-on

```bash
./build.ps1 13-REFERENCES/examples/04_dangling_reference.cpp
```

BUG 1 (return local — compile warning), BUG 2 (realloc — `&first` vs `&v[0]`
deterministic proof), BUG 3 (extension call ke through nahi). Linux pe ASan
version README mein.

---

## ⚠️ Traps (quick-fire)

### Trap 1
```cpp
auto& x = getVec().front();   // ⚠️ temp vec gaya
```

### Trap 2
```cpp
const std::string& s = boolFlag ? a : std::string("b");   // ⚠️ ek branch temporary -> conditional dangling
```

### Trap 3
```cpp
std::vector<int>& r = *new std::vector<int>();   // ⚠️ reference se "own" karne ki koshish -> leak (koi delete nahi)
```

### Trap 4
```cpp
int& r = arr[i];  arr = std::move(other);  use(r);   // ⚠️ arr ka buffer badla -> r stale
```

### Trap 5
```cpp
const auto& v = obj.getValue();   // getValue() returns by value -> v ek temp ka alias.
                                  // OK (extended) -- par agar getValue() returns `const T&`
                                  // to `obj` ki lifetime pe depend
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Reference safe hai, dangling sirf pointers mein" | Same lifetime bugs — bas syntax safe |
| "Compile warning na aaya to reference safe hai" | Compiler transitive/aliased cases miss karta |
| "`const T&` bind hamesha lifetime extend karta" | Sirf local var + direct temp; member/return/call = nahi |
| "`v[i]` ka reference stable hai" | Realloc (`push_back`/`resize`/`reserve`) pe dangle |
| "`string_view` param lena copy-free win, bas" | Owner ki lifetime document/ensure karo — dangling risk |

---

## Exercises

1. **Catalog match:** har snippet ko bug number do —
   ```cpp
   (a) int& f(){ int x=0; return x; }
   (b) int& r = v[0]; v.push_back(9);
   (c) const int& r = std::max(a+1, b+1);
   (d) for (auto& e : makeMap().entries()) {}
   (e) struct H { std::string& s; }; H h{ std::string("x") };
   ```

   <details><summary>Answer</summary>

   (a) Bug 1. (b) Bug 3. (c) Bug 2. (d) Bug 4. (e) Bug 5.
   </details>

2. **Fix each:** (a)–(e) upar ke — ek-ek line ka fix.

   <details><summary>Answer</summary>

   (a) `int f()` value return. (b) index rakho (`size_t i=0; ... v[i]`) ya
   `v.reserve` pehle. (c) `auto r = std::max(a+1, b+1);`. (d) `auto m =
   makeMap(); for (auto& e : m.entries())`. (e) `H` mein `std::string s;` (value)
   ya `const std::string* s;`.
   </details>

3. **Aliasing:** `void addTo(std::vector<int>& d, const std::vector<int>& s)`
   jo `s` ke elements `d` mein append kare. `addTo(v, v)` kyun toot sakta,
   deterministically kaise reproduce (capacity choti rakh ke).

   <details><summary>Answer</summary>

   `d` aur `s` same object. `d.push_back` capacity exceed → realloc → `s` (==
   `d`) ka purana buffer freed, par loop us buffer se padh raha → UB.
   Reproduce: `v` ko `reserve` mat karo, kai elements rakho. Fix: `const auto n
   = s.size(); for (size_t i=0;i<n;++i) d.push_back(s[i]);` after `d.reserve(d.size()+n)`,
   ya `&d==&s` check.
   </details>

4. **Conditional dangling:** `const std::string& pick(bool b, const std::string&
   x) { return b ? x : std::string("default"); }` — dono branches ka lifetime
   analyze karo. Safe banao.

   <details><summary>Answer</summary>

   `b == true` → `x` (caller ka, thoda safe — caller lifetime pe depend). `b ==
   false` → local temporary `std::string("default")` → return pe marta →
   dangling. Fix: `std::string pick(...)` return by value.
   </details>

5. **Detection:** BUG 3 (realloc) ka ASan output (Linux) kya kehta — kaunsa
   error class, kaunse do stack traces? (Concept batao.)

   <details><summary>Answer</summary>

   `heap-use-after-free` — ek trace "WRITE/READ here" (`first = 99` line), doosra
   "freed by" (`push_back` → `__gnu_cxx::new_allocator::deallocate` realloc ke
   andar), teesra "previously allocated by" (original vector buffer). Exact
   lines ke saath.
   </details>

---

## Interview questions

1. Reference "safe" kis maayne mein hai, kis maayne mein nahi?
2. Dangling reference ke 5 alag sources bata.
3. Container reallocation aur references — kaunse ops invalidate karte hain?
4. `const T&` lifetime extension **kab nahi** hoti? (member, return, call)
5. `append(v, v)` self-aliasing bug — kaise pakdo aur fix karo?
6. Reference bugs debug karne ke tools — MinGW pe kya missing, alternative kya?

---

## Next
→ [`10-exercises.md`](10-exercises.md)

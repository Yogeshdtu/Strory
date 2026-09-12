# 10 — Folder 13 Revision + Exercises

## Prerequisites
Lessons 01–09 aur saare 5 examples chalaye hue.

---

## PART A — Concept check

1. Reference kya hai (ek line)? Pointer se 3 conceptual farq?
2. Reference ke 3 rules — init, rebind, null?
3. `int& r = x; r = y;` — `x`, `r`, `&r` ka kya hua? "Rebind" kyun nahi?
4. `&` ke 3 meanings (context ke saath)?
5. `auto x = ref;` vs `auto& x = ref;` — farq?
6. Reference ko storage milta hai? Kis case mein haan, kis mein nahi?
7. Pointer kab chahiye jahan reference kaam nahi karti — 4 concrete cases?
8. `int*&` vs `int&*` — kaunsa valid, kaunsa nahi, kyun?
9. `T` / `const T&` / `T&` / `T*` param — kis situation mein kaunsa?
10. Chhote type (`int`) ke liye `const int&` param kyun bura?
11. `const T&` temporary ko bind karta hai — lifetime extension ke exact rules?
12. Extension **kab nahi** hoti — 3 cases?
13. Reference return kab valid — 4 legit patterns?
14. `return local;` (reference) pe machine level pe kya hota?
15. `for (auto x : v)` vs `for (auto& x : v)` vs `for (const auto& x : v)` — kab kaunsa?
16. `for (const pair<string,int>& p : map)` slow kyun? Warning kaunsi?
17. Reference member wali class ke kaunse special members chale jaate hain?
18. `std::reference_wrapper<T>` reference member se kaise better?
19. lvalue vs rvalue — 3-3 examples? `std::move` actually kya karta?
20. Dangling reference ke 5 alag sources? MinGW pe detection ka kya missing?

---

## PART B — Output prediction

### B1
```cpp
int x = 10;
int& r = x;
r = 20;
x = 30;
std::cout << r << " " << (&x == &r);
```
<details><summary>Answer</summary>`30 1` — `r` aur `x` ek hi object; `&x == &r`.</details>

### B2
```cpp
int a = 1, b = 2;
int& r = a;
r = b;
b = 50;
std::cout << a << " " << b << " " << r;
```
<details><summary>Answer</summary>`2 50 2` — `r = b` ne `a = 2` kiya; `b = 50` sirf `b`; `r` abhi bhi `a` ka alias.</details>

### B3
```cpp
int v = 9;
int& ref = v;
auto  a = ref;  a = 100;
auto& c = ref;  c = 200;
std::cout << v << " " << a;
```
<details><summary>Answer</summary>`200 100` — `a` copy (`v` untouched by `a=100`), `c` alias (`c=200` → `v=200`).</details>

### B4
```cpp
const int& r = 42;
const int& s = std::min(2, 9);
std::cout << r << " " << s;
```
<details><summary>Answer</summary>`42` phir **UB** — `r` direct-bind (extended, safe); `s` dangling (`2`,`9` temps line ke baad gaye, `min` un mein se ek ka ref lautaya). Aksar `2` print hota par bharosa nahi.</details>

### B5
```cpp
std::vector<int> v{10, 20, 30};
int& first = v[0];
int* addr1 = &first;
for (int i = 0; i < 500; ++i) v.push_back(i);
std::cout << (addr1 == &v[0]);
```
<details><summary>Answer</summary>`0` — `push_back` ne realloc kiya, `v[0]` ab naye buffer pe; `first`/`addr1` purane (freed) buffer ko point karte hain.</details>

### B6
```cpp
long counter = 0;
const int& r = counter;
counter = 5;
std::cout << r;
```
<details><summary>Answer</summary>`0` — types alag (`long` vs `int`), ek chhupi `int` temporary bani, `r` usse bind. `counter` badalne se `r` nahi badla.</details>

### B7
```cpp
std::string a(100, 'x');
std::string b = std::move(a);
std::cout << b.size() << " " << a.size();
```
<details><summary>Answer</summary>`100 0` (typical) — move ke baad `a` "valid but unspecified", libstdc++ mein usually empty. `b` ne buffer le liya.</details>

### B8
```cpp
struct S { int& r; S(int& x) : r(x) {} };
int a = 1, b = 2;
S s1{a}, s2{b};
s1.r = 9;
std::cout << a << " " << b << " " << sizeof(S);
// s1 = s2;   // compile hoga?
```
<details><summary>Answer</summary>`9 2 8` — `s1.r` `a` ka alias. `sizeof(S) == 8` (hidden pointer). `s1 = s2;` **compile ERROR** — copy-assign deleted (reference member).</details>

---

## PART C — Find the bug

### C1
```cpp
int& biggest(int a, int b) {
    return (a > b) ? a : b;
}
```
<details><summary>Answer</summary>`a`, `b` by-value params (locals) hain — unka reference return dangling (`-Wreturn-local-addr`). Fix: `int biggest(int a, int b)` (value return).</details>

### C2
```cpp
void scale(std::vector<double> v, double k) {
    for (double& x : v) x *= k;
}
// caller: scale(data, 2.0);  data unchanged -- kyun?
```
<details><summary>Answer</summary>`v` **by value** — function ki copy scale hui, caller ka `data` nahi. Fix: `void scale(std::vector<double>& v, double k)`.</details>

### C3
```cpp
const std::string& name() {
    std::string s = fetchName();
    return s;
}
```
<details><summary>Answer</summary>`s` local — returned ref dangling. Fix: `std::string name()` return by value (RVO), ya kisi long-lived member/`static` ka ref.</details>

### C4
```cpp
for (const std::pair<std::string, int>& kv : histogram) {   // histogram: std::map<std::string,int>
    total += kv.second;
}
```
<details><summary>Answer</summary>Map ka element `pair<const std::string, int>` hai → `pair<std::string,int>` alag type → har iteration temporary copy (`-Wrange-loop-construct`). Fix: `for (const auto& kv : histogram)`.</details>

### C5
```cpp
struct Cache {
    const Config& cfg;
    Cache() : cfg(Config{}) {}   // default config
};
```
<details><summary>Answer</summary>`Config{}` temporary — member bind lifetime extend **nahi** karta → `cfg` construct ke baad dangling. Fix: value member `Config cfg;`, ya pointer + explicitly-owned Config, ya ctor mein caller se `const Config&` lo jo longer-lived ho.</details>

### C6
```cpp
std::string_view trimmedFirstWord(const std::string& line) {
    std::string first = line.substr(0, line.find(' '));
    return first;      // string_view banke
}
```
<details><summary>Answer</summary>`first` local `std::string` — return pe marta; returned `string_view` freed data ko point karta. Fix: `std::string` return karo, ya `string_view` ko seedha `line` ke andar compute karo (`line.substr` view-style: `std::string_view(line).substr(0, ...)`).</details>

### C7
```cpp
void append(std::vector<int>& dst, const std::vector<int>& src) {
    for (std::size_t i = 0; i < src.size(); ++i)
        dst.push_back(src[i]);
}
// call: append(v, v);
```
<details><summary>Answer</summary>Self-aliasing — `dst` aur `src` same. `push_back` realloc → `src` (== `dst`) ka purana buffer freed, loop us se padh raha → UB. Fix: `if (&dst == &src)` special-case, ya `dst.reserve(dst.size()+src.size())` pehle + cached size (reserve realloc-invalidation hata deta hai, phir bhi self-append ke liye cached `n` chahiye).</details>

### C8
```cpp
int& getSlot(std::vector<int>& v, std::size_t i) { return v[i]; }

int& a = getSlot(v, 0);
int& b = getSlot(v, 1);
v.reserve(v.capacity() * 2);
a = 10; b = 20;
```
<details><summary>Answer</summary>`getSlot` khud theek hai (caller ka `v`). Par `a`, `b` lene ke baad `v.reserve` ne realloc kiya → `a`, `b` dangling → `a=10; b=20;` heap-use-after-free. Fix: references ko mutation ke aar-paar mat pakdo; indices use karo.</details>

---

## PART D — Write it

### D1 — swap teen tareeke
`swapByValue` (kaam nahi karega), `swapByPtr`, `swapByRef` — teenon likho,
`main` mein dikhao kaunsa caller ke vars swap karta hai.

<details><summary>Hint</summary>
`swapByRef(int& a, int& b) { int t = a; a = b; b = t; }`. By-value version apni
copies swap karti hai. Dekho `05_ref_vs_ptr.cpp` / `02_pass_by_reference.cpp`.
</details>

### D2 — `minmax` output params
`void minmax(const std::vector<int>& v, int& lo, int& hi)` — ek pass mein dono.
`const&` input, `int&` outputs — dono choices justify karo (comment mein).

### D3 — assignable "reference" member
`struct Watcher { std::reference_wrapper<int> target; ... };` — `target` ko
rebind karne wali `void watch(int& x)` method, aur `a = b;` jo compile ho.
Dikhao `sizeof(Watcher)` ek pointer jitna hai.

### D4 — chaining builder
`class Query { std::string sql_; public: Query& select(...); Query& where(...);
Query& orderBy(...); const std::string& str() const; };` — har setter `*this` ka
`Query&` return kare. `q.select("*").where("x>1").orderBy("x")` chale.

### D5 — dangling detector (address compare)
Ek function jo demonstrate kare ki `std::vector` realloc ke baad purani element
reference stale hai — bina UB ke: pehle `&v[0]` save karo, `push_back` loop,
phir naya `&v[0]` compare karo aur report karo (`04_dangling_reference.cpp`
BUG 2 pattern).

### D6 — copy vs move mini-benchmark
`std::string` (200 chars) ka `std::vector` banao (10000 elements). Ek loop jo
har element ko dusre vector mein `push_back(s)` (copy) kare, dusra jo
`push_back(std::move(s))` kare. `-O2` pe time. Ratio? (Folder 18 ka preview —
real numbers khud nikalo.)

<details><summary>Hint</summary>
Move version O(1) per element (pointer steal), copy O(n) (alloc + memcpy). Custom
`operator new` counter (folder 10 `03_sso_demo.cpp` style) allocations bhi dikha
sakta hai. `-O0` pe mat measure.
</details>

---

## PART E — HFT angle

1. **Handler signature:** ek `onQuote` callback — `Quote` struct 48 bytes ka.
   By value vs `const Quote&` — 10M events pe kitne bytes copy bachega? Cache
   angle?

2. **Book accessor:** `Book& book(SymbolId)` internal `std::array<Book, N>` ka
   ref deta hai. Yeh kab safe, kab nahi? Agar `std::vector<Book>` hota aur naye
   symbols add hote to?

3. **Pre-sizing:** hot path mein element references ko store karne se pehle
   container ko `reserve(maxSize)` kyun — kaunsa bug class poori tarah gayab ho
   jaata hai?

4. **`noexcept` move:** `std::vector<Order>` grow karte waqt `Order` ka move
   ctor `noexcept` na ho to kya hota? Latency pe asar?

5. **`string_view` payload:** ek parser jo incoming buffer ke andar
   `std::string_view` fields deta hai — buffer ki lifetime kya honi chahiye?
   Ek line ka comment jo har aisa API document kare.

---

## PART F — Challenge

**"Zero-copy pipeline stage"** — ek chhota event-processing chain banao:

```
RawFeed (owns std::vector<std::byte>)
   -> parse()  -> Message (string_view / span into RawFeed, no copy)
   -> enrich() -> takes const Message&, fills an Enriched (values, not views)
   -> route()  -> takes const Enriched&, calls a handler
```

Requirements:
- `parse()` koi allocation na kare — `Message` ke fields `RawFeed` ke buffer mein
  point karein (`string_view`, `span<const std::byte>`).
- `enrich()` aur `route()` sab `const T&` params lein.
- Ek `static_assert` jo `Message` ko trivially-copyable rakhe (sirf views + PODs).
- Ek deliberate-bug variant (comment mein): `RawFeed` ko `parse()` ke baad
  destroy/reassign karke dikhao `Message` ke views dangle ho gaye — aur uska fix
  (RawFeed ko stage se zyada zinda rakho).
- `-O2` pe chalao; ek chhota benchmark: 1M messages, per-message ns.

Yeh folder 10 (string_view, csv_parser), folder 11 (trivially-copyable structs),
aur is folder (const&, dangling) ko jodta hai — aur folder 36 (low-latency
pipeline) ka seed hai.

---

## Next
→ [`../14-MEMORY/00-README.md`](../14-MEMORY/00-README.md)

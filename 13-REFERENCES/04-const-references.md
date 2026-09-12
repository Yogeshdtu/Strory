# 04 — `const T&` — the standard read-only parameter

## Prerequisites
- [`03-references-as-parameters.md`](03-references-as-parameters.md)
- Folder 12 file 07 (`const` + pointers)

## Yeh topic abhi kyun
`const T&` C++ ka **sabse common parameter type** hai bade objects ke liye. Ismein
do cheezein ek saath hain: **`&`** (no copy) + **`const`** (read-only guarantee).
Aur ek chhota jaadu — yeh **temporaries ko bhi bind** kar leta hai, unki life bhi
badha deta hai. Yeh samajhna zaroori hai (aur iske edge cases bhi).

---

## `const T&` = "dekhoge, chhuoge nahi"

```cpp
long long total(const std::vector<int>& v) {
    long long s = 0;
    for (int e : v) s += e;      // padh sakte ho
    // v.push_back(1);           // ❌ ERROR -- const, modify nahi
    return s;
}
```

- **`&`** → `v` copy nahi hui, sirf ek address (8 bytes) aaya.
- **`const`** → compiler rok dega agar function galti se `v` modify kare. Caller
  ko yeh **contract** milta hai: "tera object safe hai."

Yehi wajah hai ki iterators, algorithms, aur har well-designed API bade
read-only inputs ke liye `const T&` leti hai.

---

## Jaadu: `const T&` temporaries ko bind karta hai (+ lifetime extend)

Non-const `T&` sirf ek maujooda modifiable lvalue se bind hoti hai. `const T&`
**temporary (rvalue) se bhi** bind ho jaati hai — aur us temporary ki zindagi
reference ke scope tak **badha di jaati hai**:

```cpp
const std::string& r = std::string("hello") + "!";
// "hello!" temporary normally is line ke end pe marta.
// const ref se bind hone se woh 'r' ke scope tak zinda rehta.
std::cout << r;      // ✅ safe -- "hello!"
```

```cpp
void print(const std::string& s);
print("literal");             // ✅ "literal" -> temporary std::string -> const& bind. Kaam khatam, temp mar gaya
print(a + b);                 // ✅ temporary result const& se bind
```

Isi wajah se `void f(const std::string&)` ko aap literal, expression, ya named
variable — kuch bhi de sakte ho. `void f(std::string&)` sirf named `std::string`
lega.

### ⚠️ Lifetime extension ke rules (yaad rakho)

1. **Sirf direct binding** extend karta hai:
   ```cpp
   const int& ok = 42;              // ✅ temp 42 -> ok ke scope tak
   const int& bad = identity(42);   // ❌ function ke through -> extension NAHI. bad dangles
   ```
   (`04_dangling_reference.cpp` BUG 3 yahi hai.)

2. **Return karne se** extension nahi jaata:
   ```cpp
   const std::string& oops() {
       return std::string("temp");  // ❌ -Wreturn-local-addr -- temp function ke saath marta
   }
   ```

3. **Member/struct mein bind** — extension nahi (file 07):
   ```cpp
   struct Holder { const std::string& s; };
   Holder h{ std::string("x") };   // ❌ h.s dangles -- temp is statement ke baad gaya
   ```

Rule: **extension sirf tab jab ek local `const T&` seedha ek temporary se bind ho.**

---

## `const T&` vs by value — kab kya

```cpp
void a(int x);                    // ✅ chhota -> by value (register)
void b(const std::string& s);     // ✅ bada, read -> const&
void c(std::string s);            // ⚠️ tabhi jab function ko apni copy chahiye HI (store karega)
void d(std::string_view s);       // ✅ sirf "dekhna hai, string ho ya literal" -> string_view (folder 10)
```

Modern nuance:
- Agar function input ko **store** karega (member mein rakhega), to `std::string s`
  by value + `std::move` aksar `const std::string&` se behtar (ek copy bachta hai
  jab caller rvalue de). Detail folder 18.
- Agar function ko sirf **padhna** hai aur type `std::string`/`const char*`
  dono aa sakte hain → `std::string_view` (koi alloc nahi, folder 10 file 04).
- Warna `const T&` — safe default.

---

## `const` correctness ripple

`const T&` lene ka matlab: us `T` ke sirf `const` member functions call kar
paoge:

```cpp
struct Book {
    int bestBid() const;        // const -> const& se call ho sakta
    void applyTrade(Trade t);   // non-const -> const& se NAHI
};

void report(const Book& b) {
    b.bestBid();                // ✅
    // b.applyTrade({});        // ❌ ERROR
}
```

Isliye classes mein "sirf padhne wale" methods ko `const` mark karna zaroori hai
(folder 15) — warna woh `const T&` params se unusable ho jaate hain.

---

## Andar kya hota hai

- `const T&` param = **ek pointer** ABI level pe. `total(vec)` → `mov rdi, &vec; call`.
- **Lifetime extension** compiler ke book-keeping se hota hai: temporary ko caller
  ke stack pe ek slot milta hai, uska destructor scope ke end pe schedule hota hai
  (normal full-expression ke end pe nahi). Zero runtime cost — bas destructor ka
  timing shift.
- **`const` optimization ko seedha tez nahi karta** yahan — `const T&` ka `const`
  bas compile-time check hai; woh alias analysis ko utna nahi kholta jitna log
  sochte hain (koi aur non-const path us object ko badal sakta hai). Asli
  no-alias promise `__restrict` deta hai.
- **Chhote type pe `const int&`**: ek pointer pass + callee mein ek extra load —
  jabki `int` seedha register mein aa jaata. Isliye POD by value.

> **HFT relevance:** har hot-path function jo ek book / order / message ko sirf
> padhta hai — `const T&`. Copy zero, aur `const` se aap (aur compiler) ko pata
> rehta hai yeh path object ko nahi chhoo raha. `std::string_view` /
> `std::span<const T>` isi idea ka aur bhi patla version hai jab aap sirf ek
> window dekhna chahte ho. POD price/qty by value — indirection se sasta.

---

## Hands-on

```bash
./build.ps1 fast 13-REFERENCES/examples/03_const_ref_performance.cpp
```

`const&` vs by-value `std::vector` — ~190x measured. Aur
[`examples/04_dangling_reference.cpp`](examples/04_dangling_reference.cpp) BUG 3 —
lifetime extension ka function-call wala loophole.

---

## ⚠️ Traps

### Trap 1 — extension function ke through nahi jaati
```cpp
const int& r = std::min(3, 7);   // ⚠️ std::min const int& return karta hai;
                                 //    args (3,7) temporaries -> is line ke baad mar gaye -> r dangles
```

### Trap 2 — `const T&` member (file 07)
```cpp
struct S { const std::string& name; };
S s{ std::string("temp") };   // ⚠️ s.name dangling -- member bind extension nahi karta
```

### Trap 3 — type mismatch → chhupi temporary
```cpp
void f(const long& x);
int i = 5;
f(i);   // ⚠️ i -> long temporary bani, us temp ka const& gaya. i aur x alag objects
```

### Trap 4 — `const&` return karke chain karna
```cpp
const std::string& first(const std::vector<std::string>& v) { return v.front(); }
const std::string& x = first(makeVec());   // ⚠️ makeVec() temp is statement ke baad gaya -> x dangling
```

### Trap 5 — sochna `const` ne performance badha di
`const T&` ka fayda **no copy** hai. `const` khud yahan optimization hook nahi —
woh correctness ke liye hai.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`const T&` temporary ko permanently zinda rakhta hai" | Sirf direct local binding, sirf us scope tak |
| "`const int& x` `int x` se tez" | Ulta — `int` register mein; `const int&` ek indirection |
| "`const` alias analysis khol deta hai" | Nahi zyada — `__restrict` chahiye no-alias ke liye |
| "`f(a + b)` ko `f(std::string&)` lega" | Nahi — sirf `const std::string&` (ya value/`&&`) |
| "Return `const T&` hamesha safe" | Sirf tab jab referent caller ke paas zinda ho (file 05) |

---

## Exercises

1. **Bind kya hoga:** har line — compile? dangling?
   ```cpp
   const int& a = 5;
   const int& b = std::min(2, 9);
   int x = 1; const int& c = x;
   const std::string& d = "hi";
   const std::string& e = d + "!";
   ```

   <details><summary>Answer</summary>

   `a` ✅ — direct prvalue bind, temp `e`'s scope tak extended.
   `b` ⚠️ compiles par **dangles** — `2` aur `9` temporaries hain, `std::min`
   unme se ek ka `const int&` return karta hai, dono is line ke baad mar jaate.
   `c` ✅ — named lvalue, koi temp nahi.
   `d` ✅ — literal se bana `std::string` temporary, `d` ke scope tak extended.
   `e` ✅ — `d + "!"` ek naya `std::string` prvalue; seedha `e` se bind → extended.
   </details>

2. **`const` ripple:** ek `struct Timer { long long ns() const; void reset();
   };` — `void log(const Timer& t)` ke andar `t.ns()` chalega? `t.reset()`?
   `reset` ko usable banane ke liye kya badloge (signature)?

   <details><summary>Answer</summary>

   `t.ns()` ✅ (const method). `t.reset()` ❌ (non-const method on const ref).
   `reset` chahiye to `void log(Timer& t)` (non-const ref) lo — par tab yeh
   "read-only log" nahi raha.
   </details>

3. **string_view swap:** `void greet(const std::string& s)` ko
   `std::string_view` version mein badlo. `greet("hi")` aur
   `greet(std::string("hi"))` — dono mein pehle kitni allocations, ab kitni?

   <details><summary>Answer</summary>

   `const std::string&` + `greet("hi")` → ek temporary `std::string` (short →
   SSO, no heap; long → 1 alloc). `std::string_view` version → **zero** — view
   bas literal ke pointer+length ko wrap karta hai. Named `std::string` arg dono
   mein zero.
   </details>

4. **Measure:** [`examples/03_const_ref_performance.cpp`](examples/03_const_ref_performance.cpp)
   chalao (`-O2`). `by value` ns/call aur `by const&` ns/call note karo. Ratio?
   `N` (vector size) double karo — value ka time kaise badla, ref ka?

   <details><summary>Answer</summary>

   ~524 ns/call vs ~2.7 ns/call → ~190x (machine-dependent). `N` double → by-value
   time roughly double (bada memcpy + bada alloc), by-const& time ~same (~2-3 ns,
   size se independent — sirf front/back padhta hai).
   </details>

5. **Fix the dangler:** `const std::string& name() { std::string s = load();
   return s; }` — kya galat, warning kaunsi, do fixes.

   <details><summary>Answer</summary>

   `s` local hai, function ke saath marta → returned ref dangles;
   `-Wreturn-local-addr`. Fix A: `std::string name()` (return by value, RVO).
   Fix B: agar `s` kisi long-lived cache/member mein rehta hai to us member ka
   `const std::string&` return karo.
   </details>

---

## Interview questions

1. `const T&` param ke do fayde (alag-alag) kya?
2. `const T&` temporary ko bind kar leta hai — lifetime extension ke exact rules?
3. Extension kab **nahi** hoti? (kam se kam 3 cases)
4. Chhote type ke liye `const T&` kyun bura idea?
5. `const` reference alias analysis / optimization ko kitna help karta hai — sach kya hai?
6. `const std::string&` vs `std::string_view` vs `std::string` (by value + move) — kab kaunsa?

---

## Next
→ [`05-returning-references.md`](05-returning-references.md)

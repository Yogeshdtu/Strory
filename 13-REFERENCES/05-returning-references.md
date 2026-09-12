# 05 — Returning references (aur dangling ka khatra)

## Prerequisites
- [`04-const-references.md`](04-const-references.md)
- Folder 08 file 05 (the call stack — stack frame return pe dhah jaata hai)
- Folder 12 file 12 (dangling pointers)

## Yeh topic abhi kyun
Function **reference return** kar sakta hai — `T&` ya `const T&`. Isse chaining
(`cout << a << b`), `operator[]` ko assignable banana (`v[i] = 5`), aur bade
objects ko bina copy ke bahar dena possible hota hai. Par galat referent return
karo to **dangling reference** — folder 12 ke dangling pointer ka jhurwa, aur
utna hi khatarnaak.

---

## Kab reference return karna THEEK hai

Sirf tab jab referent function ke return ke baad bhi **zinda** ho:

### 1. Caller ne jo object diya, usi ka element / part

```cpp
int& at(std::vector<int>& v, std::size_t i) {
    return v[i];                 // v caller ke paas zinda -> v[i] bhi
}
at(vec, 3) = 99;                 // vec[3] = 99   (assignable -- yeh operator[] ki trick hai)
```

### 2. Object ka apna member (`*this` ke through)

```cpp
class Builder {
    std::string s_;
public:
    Builder& add(const std::string& p) { s_ += p; return *this; }   // *this zinda hai
};
b.add("a").add("b").add("c");    // chaining -- har add wahi object lauta raha
```

### 3. `static` / global / long-lived storage

```cpp
const std::string& appName() {
    static const std::string name = "engine";   // program ke end tak zinda
    return name;
}
```

### 4. `operator<<` / stream chaining

```cpp
std::ostream& operator<<(std::ostream& os, const Price& p) {
    os << p.ticks;
    return os;                   // wahi stream object -- caller ka
}
std::cout << a << b << c;        // har << ne stream ka ref return kiya
```

---

## Kab DANGLING hoti hai (kabhi mat karna)

### ❌ Local variable ka reference

```cpp
int& bad() {
    int local = 42;
    return local;               // ⚠️ -Wreturn-local-addr -- local return pe destroy
}
int& r = bad();                 // r ek dead object ka alias
std::cout << r;                 // UB -- garbage ya crash
```

Stack frame return hote hi invalid ho jaata hai (folder 08). `local` ki jagah
agla function call reuse kar lega.

### ❌ Function ke andar bana temporary

```cpp
const std::string& bad2() {
    return std::string("temp"); // ⚠️ temporary function ke saath marta
}
```

### ❌ By-value parameter ka reference

```cpp
const std::string& bad3(std::string s) {   // s ek COPY hai, local jaisa
    return s;                   // ⚠️ s function ke end pe destroy
}
```

### ❌ Reference return ko by-value bhej ke chain karna

```cpp
const std::string& first(const std::vector<std::string>& v) { return v.front(); }

const std::string& x = first(makeVector());   // ⚠️ makeVector() ka temp is statement ke
                                              //    baad marta -> x dangling
// Sahi: auto vec = makeVector(); const std::string& x = first(vec);
```

---

## `-Wreturn-local-addr` — compiler kuch pakad leta hai

GCC/Clang seedhe `return local;` / `return &local;` / `return std::string("x");`
ko warn karte hain (references ke liye bhi). Par **transitively** nahi pakadte:

```cpp
int& sneaky() {
    int local = 42;
    int& r = local;
    return r;                   // GCC 15 yeh bhi pakad leta hai (-Wreturn-local-addr)
}
int& sneakier(int* p) {
    int local = 42;
    p = &local;
    return *p;                  // ⚠️ warning nahi -- p ke through chhup gaya. UB phir bhi
}
```

To warning ki tarah **compiler par bharosa mat rakho**. Rule yaad rakho:
**"referent function ke bahar zinda rahega?"** — nahi to reference return mat karo.

---

## `T&` vs `const T&` return

```cpp
      int& at(std::size_t i);         // v[i] = 5   allowed  (mutable slot)
const int& at(std::size_t i) const;   // sirf padhna
```

Containers dono overload dete hain (const object pe const version chalti hai).
Agar caller ko modify nahi karne dena → `const T&` return karo.

---

## Andar kya hota hai

- **Reference return = ek address return.** `int& at(...)` → function `rax` mein
  `v.data() + i*4` daal ke return karta hai; caller usse `[rax]` ki tarah use
  karta hai. `int at(...)` (by value) → `rax` mein **value** hoti.
- **Dangling case:** `return local;` → function `rax` mein us stack slot ka
  address daalta hai jo return ke turant baad **kisi aur ka** ho jaata hai. Agla
  `call` / local usko overwrite kar dega → `r` garbage padhega.
- **`static` local:** ek fixed `.data`/`.bss` address — hamesha valid, to safe.
  (Thread-safety: `static` local init C++11 se thread-safe, par baad ke reads
  pe koi lock nahi.)
- **RVO se koi takraav nahi:** RVO **value** returns pe hai. Reference return
  pe RVO ka sawaal hi nahi — waha to sirf address ja raha hai.

> **HFT relevance:** `Book& book(SymbolId s)` jaisa accessor jo internal storage
> ka reference deta hai — zero copy, caller seedha update karta hai. Bilkul
> valid, jab tak woh storage (arena / `std::array` / member) engine ke lifetime
> tak zinda hai. Khatra tab jab koi galti se ek local `std::vector` ya function
> temporary ka reference bahar de de — woh ek stale pointer hai jo aksar
> "chalta hai" test mein aur production mein random corrupt karta hai. Isliye
> accessors ki storage lifetime review hoti hai.

---

## Hands-on

```bash
./build.ps1 13-REFERENCES/examples/04_dangling_reference.cpp
```

BUG 1 (return local — compile warning + runtime crash, isliye call nahi karte),
BUG 3 (reference-return ko call ke through chain → dangling). FIX section mein
`refIntoCaller` (caller ka element — safe) aur `returnByValue`.

---

## ⚠️ Traps

### Trap 1 — local / temporary ka reference return
```cpp
int& f() { int x = 1; return x; }   // ⚠️ -Wreturn-local-addr. Value return karo
```

### Trap 2 — by-value param ka reference return
```cpp
const std::string& f(std::string s) { return s; }   // ⚠️ s bhi local hai
```

### Trap 3 — reference return + argument temporary
```cpp
const T& firstOf(const std::vector<T>& v) { return v[0]; }
const T& r = firstOf({a, b, c});   // ⚠️ initializer-list temp is line ke baad gaya
```

### Trap 4 — chain karke store
```cpp
auto& x = getVec().front();   // ⚠️ getVec() temp gaya -> x dangling. 'auto v = getVec();' pehle
```

### Trap 5 — `const T&` return karke usse `std::string` member `.c_str()`
```cpp
const char* p = obj.name().c_str();   // agar name() ek temp return kare -> p dangling (folder 10)
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Reference return hamesha copy bachata hai, to accha" | Sirf jab referent bahar zinda ho — warna dangling |
| "`-Wreturn-local-addr` na aaye to safe hai" | Compiler transitive cases miss karta — khud check karo |
| "`static` local ka ref return risky hai" | Safe (lifetime = program). Thread-safety alag sawaal |
| "RVO reference return ko optimize karta" | RVO value returns pe hai; reference return waise hi ek address |
| "`return *this;` kabhi galat nahi" | Sahi — `*this` caller ka object hai, zinda |

---

## Exercises

1. **Safe ya dangling:** har function —
   ```cpp
   int& a(std::vector<int>& v) { return v.back(); }
   int& b()                    { int x = 0; return x; }
   const std::string& c()      { static std::string s = "x"; return s; }
   int& d(int n)               { return n; }
   Widget& e(Widget& w)        { w.tick(); return w; }
   ```

   <details><summary>Answer</summary>

   `a` safe (v caller ka). `b` dangling (local). `c` safe (static). `d` dangling
   (`n` by-value param = local). `e` safe (caller ka `w`, aur chaining ke liye
   accha).
   </details>

2. **operator[]:** ek chhoti `class IntBox { int data_[4]; }` — `int&
   operator[](std::size_t i)` aur `const int& operator[](std::size_t i) const`
   dono likho. `box[2] = 9;` kaise kaam karta hai? const box pe `box[2] = 9`?

   <details><summary>Answer</summary>

   `int& operator[]` `data_[i]` ka modifiable alias deta hai → `box[2] = 9`
   `data_[2]` ko likhta hai. `const` box pe const overload chalti hai → `const
   int&` → `box[2] = 9` compile ERROR (read-only).
   </details>

3. **Chaining:** `Builder& add(...)` return `*this`. Agar galti se `Builder
   add(...)` (by value) likha to `b.add("a").add("b")` mein kya hota (kitne
   objects, kya original `b` badla)?

   <details><summary>Answer</summary>

   By value → har `add` ek naya temporary `Builder` lautaata; second `add`
   temporary pe chalti; original `b` mein sirf pehla `add` ka asar. Copies bhi
   banti. `Builder&` return → sab ek hi `b` pe.
   </details>

4. **Fix chain:** `const std::string& longest(const std::vector<std::string>&
   v);` diya. `const std::string& x = longest(readLines());` — kya bug, 2-line
   fix.

   <details><summary>Answer</summary>

   `readLines()` temp is statement ke baad marta → `x` us temp ke andar ke
   element ko alias karta → dangling. Fix: `auto lines = readLines(); const
   std::string& x = longest(lines);`.
   </details>

5. **Transitive miss:** ek function likho jo local ka reference **through a
   pointer** return kare taaki `-Wreturn-local-addr` na aaye. Phir `-O2` pe
   chalao — output? (UB, batao kya dikha.)

   <details><summary>Answer</summary>

   `int& via(){ int x=7; int* p=&x; return *p; }` — GCC aksar warn nahi karta.
   `-O2` pe caller mein garbage / stale value, ya optimizer ke assumptions se
   ajeeb output. UB — number run-to-run badal sakta hai.
   </details>

---

## Interview questions

1. Reference return kab valid? 4 legit patterns bata.
2. `return local;` (reference) pe kya hota hai, machine level pe?
3. `-Wreturn-local-addr` kis case mein miss ho jaata hai?
4. `T&` vs `const T&` return — container `operator[]` kaise use karta hai?
5. `static` local ka reference return — safe? kya caveat?
6. "Reference return ne copy bachaya" — yeh kab galat optimization-soch hai?

---

## Next
→ [`06-references-in-loops.md`](06-references-in-loops.md)

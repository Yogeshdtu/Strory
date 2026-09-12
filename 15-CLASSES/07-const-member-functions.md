# 07 — `const` member functions & const-correctness

## Prerequisites
- [`02-members-and-methods.md`](02-members-and-methods.md) (`this`)
- Folder 12 file 07 (`const` + pointers), folder 13 file 04 (`const T&`)

## Yeh topic abhi kyun
`const` sirf variables ke liye nahi — **methods** bhi `const` ho sakte hain,
matlab "yeh method object ko modify nahi karega". `const`-correctness ki poori
chain isi par depends karti hai: `const T&` parameters (folder 13), `const`
objects, immutable APIs. Aur `mutable` ka ek legit exception.

---

## `const` method = "object read-only for this call"

```cpp
class Rectangle {
    double w_, h_;
public:
    Rectangle(double w, double h) : w_(w), h_(h) {}

    double area() const { return w_ * h_; }       // const -- sirf padhta hai
    double perimeter() const { return 2 * (w_ + h_); }

    void scale(double k) { w_ *= k; h_ *= k; }     // NON-const -- modify karta hai
};
```

- `const` **method signature ka hissa** hai (`)` ke baad, `{` se pehle).
- `const` method ke andar: `this` ka type `const Rectangle*` ban jaata hai → koi
  member modify karne ki koshish = **compile error**.
- Non-`const` method → `this` normal `Rectangle*`.

```cpp
double area() const {
    // w_ = 10;              // ❌ ERROR -- const method mein member modify nahi
    return w_ * h_;          // ✅ read OK
}
```

---

## `const` objects sirf `const` methods call kar sakte hain

```cpp
Rectangle r{3, 4};
const Rectangle cr{5, 6};

r.area();        // ✅ non-const object, const method
r.scale(2);      // ✅ non-const object, non-const method

cr.area();       // ✅ const object, const method
cr.scale(2);     // ❌ ERROR -- const object pe non-const method nahi
```

Isiliye har method jo genuinely object ko modify nahi karta, usse **`const`
mark karo** — warna woh `const` objects aur `const T&` parameters se unusable
ho jaata hai (folder 13 file 04 mein yeh dekha tha).

```cpp
void logArea(const Rectangle& rect) {   // const& -> sirf const methods
    std::cout << rect.area();            // ✅ area() const hai
    // rect.scale(2);                    // ❌ scale() non-const, rect const&
}
```

**Const-correctness ki chain:** `const` param → `const` method call → us method
ne `const` return → ... har link `const` hona chahiye, ek bhi non-`const` link
poori chain tod deta.

---

## `const` overloading — do versions

Ek method ka `const` aur non-`const` dono version ho sakta hai — object ke
const-ness ke hisaab se resolve hota hai:

```cpp
class Buffer {
    std::vector<int> data_;
public:
    int&       at(std::size_t i)       { return data_.at(i); }   // non-const object
    const int& at(std::size_t i) const { return data_.at(i); }   // const object
};

Buffer b;
const Buffer cb = b;

b.at(0) = 99;         // non-const at() -> int& -> writable
int x = cb.at(0);     // const at() -> const int& -> read-only
// cb.at(0) = 99;     // ❌ const at() returns const int&
```

STL containers exactly yeh karte hain (`operator[]`, `begin()`/`end()`,
`front()`, `data()`).

---

## `mutable` — "logical const" ka exception

Kabhi ek member conceptually part of the "state" nahi hota — ek cache, ek
counter, ek mutex — aur usse `const` method ke andar bhi modify karna sahi hai.
`mutable` yehi karta hai:

```cpp
class DataSet {
    std::vector<double>       values_;
    mutable double            cachedMean_ = 0;    // cache -- logical state nahi
    mutable bool              meanValid_  = false;
    mutable std::size_t       queryCount_ = 0;    // stat

public:
    double mean() const {                          // const -- caller ke liye "read"
        ++queryCount_;                              // mutable -> allowed in const method
        if (!meanValid_) {
            double s = 0;
            for (double v : values_) s += v;
            cachedMean_ = s / static_cast<double>(values_.size());
            meanValid_ = true;                      // mutable
        }
        return cachedMean_;
    }
};
```

`mean()` ko `const` rakha kyunki **caller ke perspective se** yeh object ko nahi
badalta (bas ek value compute/return karta). Andar cache update hota hai, par
woh observable state nahi. Yeh valid use hai `mutable` ka.

⚠️ **`mutable` ka misuse:** actual state ko `mutable` bana ke `const` method se
badalna — yeh `const` ka jhoot hai, aur multi-threading mein data race (kai
threads ek `const` object ko "read-only" maan ke share karenge).

---

## `const` aur `this` — exact

```cpp
class C {
    int n_;
public:
    void f()       { /* this: C*        */ }
    void g() const { /* this: const C*  */ }

    C*       self()       { return this; }   // C*
    const C* self() const { return this; }   // const C*
};
```

`const` method ke andar `this` `const C*` hai → aap `this` se koi non-`const`
method bhi call nahi kar sakte (woh `this` ko `C*` maangega).

---

## Andar kya hota hai

- `const` method aur non-`const` method — **generated code lagbhag identical**
  agar dono sirf padhte hain. `const` ek **compile-time contract** hai; woh
  optimizer ko utna nahi kholta jitna log sochte (kyunki koi aur `const_cast` /
  alias us object ko badal sakta hai).
- **`const` object jo genuinely `const` hai** (`const int x = 5;` at namespace
  scope, ya ek `const` local jiska address kabhi non-const nahi banta) — compiler
  uski value constant-fold kar sakta, `.rodata` mein rakh sakta. Par `const T&`
  parameter aisa nahi (woh ek non-const object ko refer kar sakta).
- **`mutable` member** — object `.rodata` mein nahi ja sakta (kuch to change
  hota hai), aur `const` method us member ke liye actual store emit karta.
- **`const` overload resolution** — pure compile-time; runtime pe koi branch
  nahi.

> **HFT relevance:** const-correctness discipline se hot-path read APIs
> (`book.bestBid() const`, `session.isActive() const`) `const Book&` /
> `const Session&` params se call ho sakti hain — zero copy, aur code review mein
> "yeh path state nahi chhoo raha" clear. `mutable` ka legit use: ek lazily
> computed / cached derived value (e.g. top-of-book snapshot) jise `const`
> accessor expose karta — par multi-thread context mein `mutable` + `const`
> shared object = careful (data race); us case mein `mutable std::atomic<...>`
> ya explicit synchronization. Actual mutable state ko `mutable` se chhupana
> avoid.

---

## Hands-on

```bash
./build.ps1 15-CLASSES/examples/04_const_methods.cpp
```

`Report` — `mutable` cache + access counter, `const`/non-`const` `at()` overload,
`const Report&` parameter jo sirf `const` methods use kar sakta. Uncomment karke
dekho: `const` object pe `push()`, `const at()` ke result ko assign — errors.

---

## ⚠️ Traps

### Trap 1 — read-only method ko `const` mark na karna
```cpp
double area() { return w_ * h_; }   // ⚠️ const nahi -> const Rect& se call nahi hoga
```

### Trap 2 — `const` method se non-`const` method call
```cpp
int total() const { normalize(); return sum_; }   // ⚠️ normalize() non-const -> error
```

### Trap 3 — `mutable` se actual state chhupana
```cpp
class Account { mutable long long balance_; public: void spend(long long c) const { balance_ -= c; } };
// ⚠️ spend() "const" jhoot bol raha; const& se galti se modify; thread-unsafe
```

### Trap 4 — pointer member: `const` method usse kya rok sakta
```cpp
class C { int* p_; public: void f() const { *p_ = 5; } };   // ✅ compiles! `const` p_ ko const banata (int* const), *p_ ko nahi
```

### Trap 5 — `const_cast` se const hatana
```cpp
void g(const C& c) { const_cast<C&>(c).scale(2); }   // ⚠️ agar asli object const tha -> UB
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`const` method slow/fast" | Same code as read-only non-const; compile-time contract |
| "`const` method kuch bhi modify nahi kar sakta" | `mutable` members, aur `int* p_` ka `*p_` (pointee) modify ho sakta |
| "`const` object `.rodata` mein jaata" | Sirf genuinely-const objects; `const T&` param nahi |
| "`mutable` = 'ignore const'" | Sirf non-observable state (cache/stat/mutex) ke liye |
| "`const` optimizer ko bahut madad karta" | Thoda; `const T&` alias-analysis nahi kholti — `__restrict` chahiye |

---

## Exercises

1. **Mark const:** `class Vec3 { double x_,y_,z_; public: double length(); void
   normalize(); double dot(const Vec3& o); Vec3 cross(const Vec3& o); };` —
   kaunse methods `const` hone chahiye?

   <details><summary>Answer</summary>

   `length() const`, `dot(const Vec3&) const`, `cross(const Vec3&) const` (naya
   Vec3 return, `*this` unchanged). `normalize()` non-const (modifies `*this`).
   </details>

2. **const object:** `const Vec3 v{1,2,3};` — upar wale (marked) methods mein se
   kaunse `v` pe call ho sakenge? `v.normalize()`?

   <details><summary>Answer</summary>

   `v.length()`, `v.dot(...)`, `v.cross(...)` — OK (const methods). `v.normalize()`
   → ERROR (non-const method on const object).
   </details>

3. **Overload:** `class Grid { std::vector<int> cells_; public: ??? at(int r,
   int c) ???; };` — `const` aur non-`const` overloads likho taaki `g.at(1,2) =
   9;` aur `const Grid cg; int x = cg.at(1,2);` dono kaam karein.

   <details><summary>Answer</summary>

   `int& at(int r, int c) { return cells_.at(...); }` aur `const int& at(int r,
   int c) const { return cells_.at(...); }`. Non-const object → writable ref;
   const object → const ref.
   </details>

4. **mutable legit?:** in mein se `mutable` kis member pe justified?
   ```cpp
   class Sensor {
       double lastReading_;      // (a)
       mutable double smoothed_; // (b) -- lazily computed from history
       mutable std::size_t reads_; // (c) -- diagnostics counter
       bool calibrated_;         // (d)
   };
   ```

   <details><summary>Answer</summary>

   (b) and (c) — derived/cache and a diagnostic stat, not observable state.
   `lastReading_` and `calibrated_` are real state — `mutable` there would be a
   `const` lie.
   </details>

5. **Broken chain:** `void report(const Report& r) { r.recompute(); std::cout <<
   r.value(); }` where `recompute()` is non-`const`. Fix without removing
   `const` from the parameter.

   <details><summary>Answer</summary>

   Make `value()` itself do the lazy recompute internally using `mutable` cache
   fields (so it can stay `const`), and drop the external `recompute()` call.
   Or have the caller pass a non-const `Report&` if it genuinely mutates — but
   then it's not a "report" (read) operation.
   </details>

---

## Interview questions

1. `const` method kya guarantee karta? `this` ka type kya banta?
2. `const` object pe non-`const` method kyun call nahi hota?
3. `const` / non-`const` overload — resolution kaise, STL kahan use karta?
4. `mutable` — legit use case, aur misuse?
5. `class C { int* p_; void f() const { *p_ = 1; } };` compile hota hai? Kyun?
6. `const` optimizer ko kitni madad karta (sach)?

---

## Next
→ [`08-static-members.md`](08-static-members.md)

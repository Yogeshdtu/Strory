# 06 — Destructors

## Prerequisites
- [`04-constructors.md`](04-constructors.md)
- Folder 14 (memory — leaks, cleanup), folder 08 file 05 (scope end)

## Yeh topic abhi kyun
Constructor object banate waqt chalta hai; **destructor** object ke **khatam
hone** pe. Uska kaam: jo resources ctor / lifetime ne acquire kiye (memory,
file, lock, socket) unhe **release** karna. Yeh RAII (folder 17) ka doosra
aadha hissa hai — deterministic cleanup, garbage collector ke bina.

---

## Syntax aur basic

```cpp
class FileWriter {
    std::FILE* f_;
public:
    explicit FileWriter(const char* path) : f_(std::fopen(path, "w")) {}

    ~FileWriter() {                     // destructor: ~ClassName, no params, no return
        if (f_) std::fclose(f_);        // cleanup -- jo ctor ne khola, band karo
    }
};
```

- Naam `~ClassName`, **koi parameter nahi**, **koi return nahi**, **overload
  nahi** ho sakta (ek hi destructor per class).
- Automatic: aap explicitly call nahi karte (placement new ke case ko chhod ke —
  folder 14 file 10).
- Agar aap na likho, compiler ek generate karta hai jo har member ka destructor
  (declaration order ke **ulte** order mein) chalata hai.

---

## Destructor kab chalta hai

| Object kaisa | Destructor kab |
|---|---|
| Local (automatic) | scope `}` pe — declaration order ke **reverse** mein |
| `static` / global | program exit pe (construction ke reverse) — folder 14 file 07 |
| Member of another object | enclosing object ke destructor mein (reverse decl order) |
| Heap (`new`) | jab aap `delete` karo (bhoolo → leak — folder 14 file 05) |
| `std::vector<T>` element | vector destroy / erase / clear pe |
| Temporary | full-expression ke end pe (`;`) — ya lifetime-extended (folder 13 file 04) |
| Exception thrown | stack unwinding — har fully-constructed local ka dtor |

```cpp
{
    Widget a;                          // ctor a
    Widget b;                          // ctor b
    Widget c;                          // ctor c
}                                       // dtor c, dtor b, dtor a   (reverse!)
```

Reverse order isliye ki baad wale objects pehle wale par depend kar sakte hain
(e.g. `c` `a` ka reference rakhta ho) — to `c` pehle jaana chahiye.

---

## Kya destructor karta hai (RAII preview)

```cpp
class ScopedLock {
    std::mutex& m_;
public:
    explicit ScopedLock(std::mutex& m) : m_(m) { m_.lock(); }    // acquire
    ~ScopedLock() { m_.unlock(); }                                // release -- GUARANTEED
};

void f() {
    ScopedLock lk{someMutex};          // lock
    doWork();
    if (early) return;                 // dtor chalta -> unlock. leak nahi
    doMore();
}                                       // dtor chalta -> unlock
```

Har exit path se (normal return, early return, **exception**) destructor chalta
hai → resource kabhi leak nahi hota. Yeh C++ ka signature pattern hai. Folder 17
poora ispe.

---

## Compiler-generated destructor

```cpp
class Point { double x_, y_; };         // no user destructor -> compiler ~Point() = trivial
                                        // (x_, y_ POD -> kuch nahi karta)

class Record {
    std::string   name_;                // has a destructor
    std::vector<int> data_;             // has a destructor
};                                       // compiler ~Record() -> ~data_(), ~name_()  (reverse)
```

- **Trivial destructor** — agar sab members trivially destructible aur no user
  destructor. Zero code. `is_trivially_destructible_v<T>` true.
- **Non-trivial** — members ke destructors chain hote hain.
- Aap likhte tabhi ho jab class **owns a raw resource** (raw `new`, `FILE*`,
  fd, handle). Agar sab members RAII types (`std::string`, `std::vector`,
  `std::unique_ptr`) hain → **destructor mat likho** (Rule of Zero — folder 18).

---

## Rule of Three/Five preview

Agar aap **destructor** likhte ho (matlab class raw resource own karti hai), to
tumhe **copy constructor** aur **copy assignment** bhi handle karne padenge —
warna default shallow-copy do objects ko same resource ka owner bana deta →
**double-free** (folder 14 file 06).

```cpp
class Buffer {
    int* data_;
    std::size_t n_;
public:
    explicit Buffer(std::size_t n) : data_(new int[n]), n_(n) {}
    ~Buffer() { delete[] data_; }
    // ⚠️ copy ctor / copy assign define nahi kiye -> default shallow copy ->
    //    Buffer b = a;  -> b.data_ == a.data_ -> dono destructors delete[] karenge -> DOUBLE FREE
};
```

Poora "Rule of 0/3/5" + move semantics folder 18. Abhi yaad rakho: **destructor
likha = copy/move bhi socho** (ya `= delete`, ya use RAII members aur destructor
hi mat likho).

---

## `virtual` destructor (folder 16 preview)

Agar class **polymorphic base** hai (koi `virtual` method hai) aur aap uske
objects `Base*` ke through `delete` karoge — destructor **`virtual`** hona
chahiye:

```cpp
struct Base { virtual ~Base() = default; };        // ✅ virtual dtor
struct Derived : Base { std::vector<int> big_; };

Base* p = new Derived;
delete p;                                            // virtual dtor -> ~Derived() chalta -> big_ freed
                                                    // non-virtual hota -> sirf ~Base() -> big_ LEAK (UB)
```

Detail + measured leak demo folder 16 file 07. Abhi: **base class ka destructor
`virtual` (ya `protected` non-virtual)**.

---

## Andar kya hota hai

- `Widget a;` scope end → compiler `a.~Widget()` ka call emit karta us `}` se
  pehle, aur multiple locals ke liye **reverse construction order**.
- **Trivial dtor** → koi instruction nahi. `std::vector<Point>` clear → agar
  `Point` trivially destructible, loop hi nahi (bas `free`).
- **Non-trivial** → dtor body + har member ka dtor call (reverse decl order),
  phir base ka dtor (folder 16).
- **Exception unwinding** → runtime har stack frame ke fully-constructed objects
  ke dtors dhoondh ke chalata (`.eh_frame` tables). Isiliye dtors ko `noexcept`
  (implicitly hain) rakhna zaroori — dtor se throw karna during unwinding =
  `std::terminate`.
- `delete p` → `p->~T()` phir `operator delete(p)` (folder 14 file 04).

> **HFT relevance:** deterministic destruction hi wajah hai C++ ki HFT
> dominance — koi GC pause nahi, cleanup exactly wahan jahan scope khatam.
> RAII se locks, buffers, pool handles hot path pe bhi leak-free. Par hot path pe
> **non-trivial destructors ki cost** dhyaan mein: ek `std::vector<T>` member ka
> dtor = loop over elements. Isliye hot structs ko **trivially destructible**
> rakha jaata (POD members, no owning members) → `is_trivially_destructible`
> static_assert. Pool se object "return" karna = trivial dtor (ya explicit
> reset), `delete` nahi.

---

## Hands-on

```bash
./build.ps1 15-CLASSES/examples/05_static_members.cpp
```

`Connection` ka destructor `s_liveCount` ghatata hai — block scope se nikalte hi
`close #3` print hota hai (baaki `main` end pe reverse order). Aur khud: 3
`ScopedLock`-jaise trace objects banao, reverse-order destruction dekho.

---

## ⚠️ Traps

### Trap 1 — raw resource own kiya, destructor nahi likha
```cpp
class C { int* p_; public: C() : p_(new int[100]) {} };   // ⚠️ ~C() nahi -> leak har baar
```

### Trap 2 — destructor likha, copy/move nahi (Rule of 3/5)
```cpp
class C { int* p_; public: ~C() { delete[] p_; } };   // ⚠️ C b = a; -> double free
```

### Trap 3 — destructor se throw
```cpp
~C() { if (bad) throw std::runtime_error("x"); }   // ⚠️ unwinding ke dauraan -> std::terminate
```

### Trap 4 — base class non-virtual destructor
```cpp
struct B { ~B(); };  struct D : B { std::vector<int> v; };
B* p = new D;  delete p;   // ⚠️ sirf ~B() -> D::v leak, UB (folder 16 file 07)
```

### Trap 5 — destructor ko manually call karna (non-placement)
```cpp
Widget w;  w.~Widget();   // ⚠️ phir scope end pe DOBARA dtor -> double destruction UB.
                          //    Manual dtor sirf placement new ke saath (folder 14 file 10)
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Destructor ko explicitly call karna padta" | Automatic — scope/delete/exception pe |
| "Objects construction order mein destroy hote" | **Reverse** construction order |
| "Har class ko destructor chahiye" | Sirf jab raw resource own ho; RAII members → Rule of Zero |
| "Destructor likha to bas cleanup, aur kuch nahi" | Rule of 3/5 — copy/move bhi handle karo |
| "Destructor se exception fine hai" | Unwinding ke dauraan → `std::terminate`. Dtors `noexcept` |

---

## Exercises

1. **Order:** `struct T { T(int i):id(i){std::cout<<"c"<<id;} ~T(){std::cout<<"d"<<id;}
   int id; };` — `{ T a{1}; T b{2}; { T c{3}; } T d{4}; }` — poora output?

   <details><summary>Answer</summary>

   `c1 c2 c3 d3 c4 d4 d2 d1` — `c` inner block ke `}` pe (`d3`), phir `d`, phir
   block end pe `b`, `a` reverse.
   </details>

2. **Missing dtor leak:** `class Arr { int* p_; std::size_t n_; public:
   Arr(std::size_t n) : p_(new int[n]), n_(n) {} };` — leak kahan? Fix (2
   tareeke: dtor, ya member type badlo).

   <details><summary>Answer</summary>

   `p_` array kabhi `delete[]` nahi hota → leak har `Arr` pe. Fix A: `~Arr() {
   delete[] p_; }` (+ Rule of 3/5). Fix B: `std::vector<int> p_;` (ya
   `std::unique_ptr<int[]>`) → destructor apne aap, Rule of Zero.
   </details>

3. **Double free:** `class B { char* d_; public: B(std::size_t n):d_(new
   char[n]){} ~B(){ delete[] d_; } };  B x{10}; B y = x;` — kya hota program
   exit pe? Kyun?

   <details><summary>Answer</summary>

   Default copy ctor → `y.d_ == x.d_` (shallow). Dono destructors `delete[]`
   same pointer → double-free → crash / heap corruption. Fix: deep copy ctor +
   copy assign, ya `= delete` copy, ya `std::vector`/`std::string` member.
   </details>

4. **Trivial check:** in classes ka `std::is_trivially_destructible_v`? `struct
   A { int x; };`, `struct B { std::string s; };`, `struct C { int a; double b;
   };`, `struct D { ~D(){} };`.

   <details><summary>Answer</summary>

   `A` true, `B` false (`std::string` has dtor), `C` true, `D` false
   (user-declared dtor, even if empty).
   </details>

5. **RAII lock:** `class Guard { std::mutex& m_; public: Guard(std::mutex& m) :
   m_(m) { m_.lock(); } ~Guard() { m_.unlock(); } };` — ek function likho jisme
   `Guard` ke baad `throw` ho. Unlock hota hai? Kyun?

   <details><summary>Answer</summary>

   Haan — `throw` stack unwinding trigger karta, jo `Guard` (fully constructed
   local) ka destructor chalata → `m_.unlock()`. Isiliye RAII exception-safe hai.
   </details>

---

## Interview questions

1. Destructor kab chalta hai (kam se kam 4 situations)?
2. Multiple locals — destruction order? Kyun reverse?
3. Compiler-generated destructor kya karta? Trivial kab?
4. Rule of Three/Five — destructor likha to aur kya socho?
5. Destructor se throw karna — kya hota, kyun mana?
6. Base class destructor `virtual` kab zaroori?

---

## Next
→ [`07-const-member-functions.md`](07-const-member-functions.md)

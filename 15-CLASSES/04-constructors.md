# 04 — Constructors

## Prerequisites
- [`03-access-specifiers.md`](03-access-specifiers.md)
- Folder 11 file 02 (aggregate init), folder 14 (object lifetime)

## Yeh topic abhi kyun
Constructor woh function hai jo object ke **banne ke waqt** chalti hai — uska
kaam: object ko ek **valid initial state** mein laana (invariant establish
karna). Types: default, parameterized, delegating, copy/move (folder 18), plus
`= default` / `= delete`. Yeh galat hone pe object hi galat banta hai.

---

## Basic constructor

```cpp
class Rational {
    long num_;
    long den_;
public:
    Rational(long num, long den)                    // parameterized constructor
        : num_(num), den_(den == 0 ? 1 : den) {     // member init list (file 05)
    }                                                // body -- extra setup / checks
};

Rational half{1, 2};        // constructor call -- half.num_ = 1, half.den_ = 2
```

- Naam **class jaisa**, **koi return type nahi** (void bhi nahi).
- Overload ho sakta hai (alag parameters).
- Body se pehle **member initializer list** chalti hai — asli init wahan (file 05).

---

## Default constructor

**Koi argument nahi lene wala** constructor. `T obj;` iske bina compile nahi
hota.

```cpp
class Timer {
    long long startNs_;
public:
    Timer() : startNs_(0) {}      // user-defined default ctor
};

Timer t;                          // ✅ default ctor chala
```

### Compiler kab khud deta hai

Agar aap **koi constructor declare nahi karte**, compiler ek `public` default
ctor generate karta hai (jo members ko default-init karta — POD ke liye garbage,
class members ke liye unke default ctors).

```cpp
class A { int x; };               // compiler default ctor -> x uninitialized
class B { int x = 0; };           // compiler default ctor -> x = 0 (DMI)
class C { C(int); };              // ⚠️ ek ctor declare kiya -> compiler default ctor NAHI deta
                                  //    `C c;` ab compile ERROR

C c;                              // ❌ no default ctor
```

Chahiye to `= default` se wapas maango:
```cpp
class C {
public:
    C() = default;                // "compiler wala default ctor de do"
    C(int);
};
```

---

## `= default` aur `= delete`

```cpp
class Widget {
public:
    Widget() = default;                       // compiler-generated default ctor
    Widget(const Widget&) = default;          // compiler-generated copy ctor
    Widget& operator=(const Widget&) = delete;// copy assignment BANNED
};
```

- **`= default`** — "compiler, tu banaa de" (aur woh trivial/optimal hota hai).
  Explicit hone se intent clear + kabhi-kabhi zaroori (jaise ek aur ctor declare
  karne ke baad default ctor wapas laana).
- **`= delete`** — "yeh operation allowed hi nahi". Use karne ki koshish =
  compile error. Non-copyable types, non-movable types, unwanted implicit
  conversions rokne ke liye.

```cpp
class FileHandle {
public:
    FileHandle(const char* path);
    FileHandle(const FileHandle&)            = delete;   // do owners na ban sakein
    FileHandle& operator=(const FileHandle&) = delete;
    // (move ops folder 18)
};
```

---

## Delegating constructors

Ek constructor doosre ko **call** kar sakta hai (code duplication hatane ke
liye):

```cpp
class Connection {
    std::string host_;
    int         port_;
    int         timeoutMs_;
public:
    Connection(std::string host, int port, int timeoutMs)
        : host_(std::move(host)), port_(port), timeoutMs_(timeoutMs) {}

    Connection(std::string host, int port)
        : Connection(std::move(host), port, 5000) {}      // delegate -- default timeout

    Connection()
        : Connection("localhost", 8080) {}               // delegate -- all defaults
};
```

- Delegate **member init list mein** hi hota hai, aur woh **akela** entry ho
  (`: Connection(...), x_(1)` ❌).
- Target ctor pehle poora chalta hai (object fully constructed), phir delegating
  ctor ka body.
- ⚠️ Cycle mat banao (`A() : A(0) {}` aur `A(int) : A() {}`) — UB.

---

## Constructor kya kar sakta

```cpp
class Buffer {
    std::size_t size_;
    int*        data_;
public:
    explicit Buffer(std::size_t n)
        : size_(n), data_(new int[n]) {          // resource acquire (RAII -- folder 17)
        std::fill(data_, data_ + n, 0);          // body: extra init
        if (n > 1'000'000) throw std::length_error("too big");   // ctor throw kar sakta
    }
    // (destructor + copy/move -- folder 17/18)
};
```

- Members init karna (primary job).
- Resources acquire karna (memory, file, lock — RAII).
- Invariant check karna; violate ho to **`throw`** (object banega hi nahi — jo
  members ban chuke unke destructors chalenge).
- ⚠️ Ctor ke andar `virtual` call → derived override **nahi** chalta (folder 16).

---

## Initialization syntax — kaunsa ctor chalta

```cpp
struct P {
    P();                 // #1 default
    P(int);              // #2
    P(int, int);         // #3
    P(std::initializer_list<int>);   // #4 -- braces isse prefer karte hain!
};

P a;            // #1
P b(5);         // #2
P c{5};         // #4 if it exists, else #2   ⚠️
P d(1, 2);      // #3
P e{1, 2};      // #4 if it exists, else #3
P f{};          // #1 (default) -- initializer_list wala NAHI (empty braces special)
P g = 5;        // #2 (copy-init) -- #2 explicit hota to ERROR (file 11)
```

**Gotcha:** agar `std::initializer_list` ctor hai, `{}` uske paas jaata hai
(`std::vector<int> v{3}` → ek element `3`, teen nahi — `v(3)` teen zeros).
Folder 19 mein detail.

---

## Andar kya hota hai

- **`T obj{args}`** → memory allocate (stack: `sub rsp`; heap: `operator new`),
  phir constructor run: member init list order mein members construct, phir body.
- **Trivial default ctor** (`= default`, sab members trivial, no bases) → koi
  code nahi, sirf uninitialized memory (POD jaisa). `-O2` pe zero cost.
- **Non-trivial ctor** → actual instructions: har member ka init, body.
- **Delegating** → target ctor ka full call (aksar inline), phir delegating body.
- **Throw in ctor** → jo members/sub-objects construct ho chuke, unke destructors
  reverse order mein chalte; memory release; exception propagate. Object "kabhi
  existed nahi".
- Ek user-declared ctor → **aggregate-ness** khatam (folder 11 file 02): `T{a, b}`
  ab aggregate init nahi, constructor call.

> **HFT relevance:** hot-path objects ke ctors ko **trivial ya near-trivial**
> rakha jaata — pool se object nikaalna = placement new + ek chhota ctor (ya
> `= default` + explicit reset). Bhaari ctors (allocation, syscalls, validation
> loops) startup / cold path pe. `explicit` single-arg ctors (file 11) taaki
> hot code mein accidental conversions (aur unke chhupe ctor calls) na hon.
> `= delete` copy on big owning types taaki galti se deep copy na ho jaaye.
> Aggregate `struct`s (no ctor) wire messages ke liye — `{}` init + `memcpy`.

---

## Hands-on

```bash
./build.ps1 15-CLASSES/examples/02_constructors.cpp
```

Default → delegating → param → copy ctors ka call order (prints se). `= delete`
wali `Unique` ko copy karne ki koshish (uncomment) → compile error.

---

## ⚠️ Traps

### Trap 1 — ek ctor declare kiya, default ctor gaya
```cpp
class C { public: C(int); };  C c;   // ❌ no default ctor. C() = default; add karo
```

### Trap 2 — delegating ctor ke saath aur member init
```cpp
C(int x) : C(), x_(x) {}   // ❌ delegate akela hona chahiye
```

### Trap 3 — `{}` aur `initializer_list` ctor
```cpp
std::vector<int> v{5};      // ⚠️ ek element (5), NOT size 5. v(5) for size
```

### Trap 4 — ctor mein `virtual` call
```cpp
Base() { init(); }   // ⚠️ agar init() virtual -> Base ka version chalta, Derived ka nahi (folder 16)
```

### Trap 5 — member init list order galat
```cpp
C(int x) : b_(x), a_(b_) {}   // ⚠️ a_ pehle declare -> a_ garbage (file 05)
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Compiler hamesha default ctor deta" | Sirf jab aap koi ctor declare na karo |
| "`= default` aur user-defined `{}` same" | `= default` trivial/optimal; `{}` user-provided (aggregate-ness, triviality par asar) |
| "`= delete` sirf destructor ke liye" | Kisi bhi special member / overload pe — unwanted ops ban |
| "`P c{5}` hamesha `P(int)` call karta" | `initializer_list` ctor ho to woh jeeta |
| "Ctor throw kare to memory leak" | Constructed sub-objects ke destructors chalte; memory freed |

---

## Exercises

1. **Default ctor gayab:** `class Vec { double x_, y_; public: Vec(double x,
   double y):x_(x),y_(y){} };  Vec v;` — error kyun? 2 tareeke fix.

   <details><summary>Answer</summary>

   `Vec(double,double)` declare karne se compiler default ctor nahi deta. Fix:
   `Vec() = default;` (x_/y_ garbage) ya `Vec() : x_(0), y_(0) {}`, ya DMI
   `double x_ = 0, y_ = 0;` + `Vec() = default;`.
   </details>

2. **Delegation:** `class Rect { int w_, h_; public: ... };` — 3 ctors:
   `Rect(int w, int h)`, `Rect(int side)` (square, delegates), `Rect()` (1x1,
   delegates). Likho.

   <details><summary>Answer</summary>

   `Rect(int w, int h) : w_(w), h_(h) {}` · `Rect(int side) : Rect(side, side)
   {}` · `Rect() : Rect(1) {}` (ya `: Rect(1,1)`).
   </details>

3. **`= delete` use:** ek `class Logger` jo copy nahi hona chahiye (ek hi
   instance ka concept). Copy ctor + copy assignment delete karo. `Logger b =
   a;` par error?

   <details><summary>Answer</summary>

   `Logger(const Logger&) = delete; Logger& operator=(const Logger&) = delete;`
   → `Logger b = a;` → "use of deleted function 'Logger(const Logger&)'".
   </details>

4. **initializer_list trap:** `std::vector<int> a(3, 7);` vs `std::vector<int>
   b{3, 7};` — dono ka content? Kyun alag?

   <details><summary>Answer</summary>

   `a` = `{7, 7, 7}` (3 copies of 7 — `(count, value)` ctor). `b` = `{3, 7}` (2
   elements — `initializer_list` ctor, jo `{}` ke saath prefer hota).
   </details>

5. **Throw in ctor:** `class Res { int* p_; public: Res(std::size_t n) : p_(new
   int[n]) { if (n == 0) throw std::invalid_argument("n"); } };` — `Res r{0};` pe
   `p_` leak hota hai? (Trick.)

   <details><summary>Answer</summary>

   `p_(new int[0])` succeed hota (0-size array valid), phir body throw karta →
   `Res` object banega nahi, par `p_` ka array **leak** — kyunki destructor
   nahi chala (object incomplete). Fix: validate **pehle** (init list se pehle
   possible nahi → body ke shuru mein, allocation se pehle; ya RAII member
   jaise `std::vector<int>`).
   </details>

---

## Interview questions

1. Constructor ke rules (naam, return, overload)?
2. Compiler default ctor kab deta, kab nahi?
3. `= default` vs `= delete` — kab kaunsa?
4. Delegating constructor — syntax rules, execution order?
5. Ctor throw kare to allocated sub-objects ka kya?
6. `{}` init aur `std::initializer_list` ctor ka gotcha?

---

## Next
→ [`05-member-initializer-lists.md`](05-member-initializer-lists.md)

# 04 — `new` and `delete`

## Prerequisites
- [`03-heap-deep-dive.md`](03-heap-deep-dive.md)
- Folder 11 (structs) / early class idea — constructor/destructor
- Folder 12 file 12 (dangling), file 13 (bug catalog)

## Yeh topic abhi kyun
`new` / `delete` C++ ka **manual heap** interface hai. Sahi use karna — har
`new` ka theek ek `delete`, sahi form (`[]` vs nahi), sahi order — RAII (folder
17) samajhne se pehle zaroori. Aur galtiyan (leak, double-free, mismatch) sab
yahin se aati hain.

---

## `new` = 2 steps, `delete` = 2 steps

```cpp
Widget* w = new Widget(1, 2);
//           └─ 1. operator new(sizeof(Widget))  -> raw memory (≈ malloc)
//              2. Widget(1, 2) constructor       -> us memory pe object banao
//           w ab ek constructed Widget ko point karta hai

delete w;
//     1. w->~Widget()          -> destructor (resources release)
//     2. operator delete(w)    -> memory free (≈ free)
```

`malloc` sirf step 1 karta hai (raw bytes, koi constructor nahi). Isiliye
`malloc`'d memory pe non-trivial object "use" karna UB hai jab tak aap placement
new se construct na karo (file 10).

---

## Forms

```cpp
int*    a = new int;              // uninitialized (garbage)
int*    b = new int();            // value-initialized -> 0
int*    c = new int(42);          // 42
int*    d = new int{42};          // 42 (brace)
auto*   e = new int[5];           // 5 uninitialized ints
auto*   f = new int[5]();         // 5 zeros
auto*   g = new int[5]{1, 2, 3};  // 1, 2, 3, 0, 0

delete a;  delete b;  delete c;  delete d;
delete[] e;  delete[] f;  delete[] g;      // NOTE: [] -- array form
```

**Rule:** `new` ↔ `delete`, `new[]` ↔ `delete[]`. Cross karna **UB** (aksar
crash ya corruption — array form allocation ke aage element count store karta
hai; `delete` bina `[]` usse miss karta).

---

## `nothrow` — exception ki jagah `nullptr`

```cpp
#include <new>
int* p = new (std::nothrow) int[1'000'000'000];   // fail -> nullptr (bad_alloc throw nahi)
if (!p) { /* handle */ }
```

Default `new` fail hone pe `std::bad_alloc` throw karta (RAII cleanup chal
jaata). `nothrow` version un contexts ke liye jahan exceptions off/unwanted hain
(kuch embedded / HFT builds). **Phir aapko `nullptr` check karna hi padega.**

---

## Constructor/destructor sach mein chalte hain

```cpp
struct Conn {
    Conn()  { std::puts("open socket"); }
    ~Conn() { std::puts("close socket"); }
};

Conn* c = new Conn;   // "open socket"
delete c;             // "close socket"

Conn* arr = new Conn[3];   // "open socket" x3
delete[] arr;              // "close socket" x3  (reverse order)
delete arr;                // ⚠️ UB -- sirf ek dtor + wrong free. HAMESHA delete[]
```

[`examples/03_new_delete.cpp`](examples/03_new_delete.cpp) yeh live dikhata hai
(ctor/dtor prints ke saath).

---

## `new` mostly mat likho — RAII wrappers use karo

Raw `new`/`delete` correct likhna mushkil hai (har exit path pe `delete`,
exceptions, ownership). Modern C++:

| Chahiye | Use |
|---|---|
| Dynamic array | `std::vector<T>` |
| Single owned object | `std::unique_ptr<T>` (`std::make_unique<T>(...)`) |
| Shared ownership | `std::shared_ptr<T>` (`std::make_shared`) |
| Dynamic string | `std::string` |
| Fixed-size, on stack | `std::array<T, N>` (heap hi nahi) |

Yeh sab andar `new`/`delete` karte hain, par **destructor mein `delete` guarantee**
karte hain (RAII — folder 17). `new` seedha likhna aaj "code smell" hai — sivaay
library / allocator code ke.

```cpp
// ❌ purana
Widget* w = new Widget();
// ... har return / throw pe delete w; yaad rakho ...
delete w;

// ✅ aaj
auto w = std::make_unique<Widget>();
// scope end pe automatic delete -- har path pe, exception pe bhi
```

---

## Andar kya hota hai

- **`new T`** → `call operator new` (≈ `malloc(sizeof(T))`), phir constructor
  inline/`call`. `-O2` pe chhote constructors inline.
- **`new T[n]`** → `operator new[](n * sizeof(T) + cookie)`. "Cookie" (usually 8
  bytes, allocation ke shuru mein) element count store karta hai — `delete[]` ko
  pata chale kitne destructors chalane hain. Isiliye `new T[]` ka pointer aur
  actual allocation start alag ho sakte (POD ke liye cookie skip ho sakta).
- **`delete p`** → `p->~T()` (agar non-trivial), phir `operator delete(p)`.
  C++14+ **sized delete**: `operator delete(void*, size_t)` — allocator ko size
  bhi milta (tez free).
- **Exception safety:** `new T(...)` mein agar constructor throw kare → `operator
  delete` automatically call hota (memory leak nahi hota), par aapke paas pointer
  nahi aata. Array mein: jitne construct hue, unke destructors + free.
- **Replaceable:** linker aapki global `operator new`/`delete` ko library wali
  ke upar prefer karta. C++14+ compiler in calls ko elide bhi kar sakta hai
  (folder 10 mein dekha — short-lived `std::string`).

> **HFT relevance:** hot path pe `new`/`delete` = allocator ka non-deterministic
> path (file 03, 08). Isliye: (1) objects pre-allocate (pools, `std::vector`
> reserve), (2) jahan dynamic zaroori ho — custom `operator new` per-type pool
> pe route, (3) `-fno-exceptions` builds mein `new(nothrow)` + explicit checks.
> `std::make_unique` non-hot paths (setup, config) mein default; hot path pe
> object lifetime pre-arranged hoti hai.

---

## Hands-on

```bash
./build.ps1 14-MEMORY/examples/03_new_delete.cpp
```

Dekho: ctor/dtor kab chalte, `new[]`/`delete[]` reverse-order dtors, `nothrow`.
Phir file ke comment-out UB lines ko ek-ek karke enable karke (`san` build ya
Linux ASan) dekho kya diagnose hota.

---

## ⚠️ Traps

### Trap 1 — `new[]` ko `delete` (bina `[]`)
```cpp
int* p = new int[10];  delete p;    // ⚠️ UB. delete[] p;
```

### Trap 2 — `new` ko `delete[]`
```cpp
int* p = new int;  delete[] p;      // ⚠️ UB (ulta)
```

### Trap 3 — `new` + `free` / `malloc` + `delete`
```cpp
free(new int);              // ⚠️ UB
delete (int*)malloc(4);     // ⚠️ UB
```

### Trap 4 — `new int` uninitialized padhna
```cpp
int* p = new int;  std::cout << *p;   // ⚠️ garbage. new int() / new int{} for 0
```

### Trap 5 — leak on early return
```cpp
Widget* w = new Widget();
if (bad) return;            // ⚠️ leak -- delete w miss. unique_ptr use karo
delete w;
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`new` bas memory deta hai" | Memory **+ constructor**. `delete` = destructor + free |
| "`delete` vs `delete[]` ek jaisa" | Alag — array form n destructors chalata; mismatch UB |
| "`new int` initialized (0) hota" | Nahi — garbage. `new int()` / `new int{}` → 0 |
| "`new` fail hone pe `nullptr`" | `std::bad_alloc` throw. `nullptr` sirf `new(nothrow)` se |
| "Raw `new`/`delete` normal modern C++ hai" | `vector` / `unique_ptr` / `make_unique` — raw new = smell |

---

## Exercises

1. **Form quiz:** har ek ke baad value kya, aur sahi delete kya?
   `new int`, `new int()`, `new int(9)`, `new int[3]`, `new int[3]()`,
   `new int[3]{7}`.

   <details><summary>Answer</summary>

   `new int` → garbage, `delete`. `new int()` → 0, `delete`. `new int(9)` → 9,
   `delete`. `new int[3]` → 3 garbage, `delete[]`. `new int[3]()` → `{0,0,0}`,
   `delete[]`. `new int[3]{7}` → `{7,0,0}`, `delete[]`.
   </details>

2. **ctor/dtor count:** `struct S { S(){puts("c");} ~S(){puts("d");} };` —
   `S* a = new S[3]; delete[] a;` ka output? `delete a;` (galat) kya karta?

   <details><summary>Answer</summary>

   `new S[3]` → `c c c`. `delete[] a` → `d d d` (reverse). `delete a` (bina
   `[]`) → UB: aksar sirf ek `d` + wrong free pointer (cookie offset) → crash /
   heap corruption.
   </details>

3. **Leak fix:** ek function jisme `new` ke baad ek `if (err) return;` hai.
   `unique_ptr` se rewrite karo. Ab early return pe kya hota?

   <details><summary>Answer</summary>

   `auto p = std::make_unique<T>();` — `if (err) return;` pe `p` ka destructor
   automatically `delete` karta. Koi leak nahi, koi manual cleanup nahi.
   </details>

4. **nothrow:** `new int[1'000'000'000'000]` (default) vs `new (std::nothrow)
   int[...]` — dono ka behaviour? Kaunsa `try/catch` maangta, kaunsa `if`?

   <details><summary>Answer</summary>

   Default → `std::bad_alloc` throw → `try/catch` (ya program terminate).
   `nothrow` → `nullptr` → `if (!p)` check. Dono handle na karo to crash.
   </details>

5. **Elision:** `for (int i=0;i<1e7;++i){ int* p = new int(i); sum += *p; delete
   p; }` — `-O2` pe: allocations sach mein hoti hain? `operator new` override
   counter se check. Barrier (`asm volatile`) add karo — ab?

   <details><summary>Answer</summary>

   `-O2` pe compiler poora dekh ke `new`/`delete` **elide** kar sakta hai (C++14
   allowance) → counter 0. `asm volatile("" ::: "memory")` ya pointer ko escape
   karwane se elision rukta → counter 1e7.
   </details>

---

## Interview questions

1. `new T` ke 2 steps? `delete p` ke 2 steps? `malloc` in mein se kya karta?
2. `delete` vs `delete[]` — internally farq, mismatch pe kya?
3. `new` failure pe kya? `nothrow` ka use case?
4. `new int` vs `new int()` — initialization?
5. Raw `new`/`delete` ki jagah kya, kyun (RAII)?
6. Constructor `new T(...)` mein throw kare to memory leak hota? Kyun nahi?

---

## Next
→ [`05-memory-leaks.md`](05-memory-leaks.md)

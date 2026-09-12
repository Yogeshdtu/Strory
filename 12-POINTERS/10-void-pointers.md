# 10 — `void*` — type-erased pointers

## Prerequisites
- [`03-dereferencing.md`](03-dereferencing.md), [`07-const-and-pointers.md`](07-const-and-pointers.md)

## Yeh topic abhi kyun
`void*` = "pointer to *something*, type unknown." C ka type-erasure mechanism —
`malloc`, `memcpy`, `qsort` sab `void*` use karte hain. Modern C++ mein kam
(templates + `std::any` + `std::variant` isse replace karte hain), par C interop
aur low-level code mein zaroori.

---

## `void*` — ek address jiska element type hi nahi

```cpp
int    x = 5;
void*  v = &x;         // koi bhi T* -> void* (implicit, cast ki zaroorat nahi)

// *v;                 // ❌ ERROR -- type hi nahi, deref kaise karein
// v + 1;              // ❌ ERROR -- element size nahi, to arithmetic nahi

int* p = static_cast<int*>(v);   // wapas asli type mein cast karo
*p;                              // 5  -- ab chal gaya
```

`void*` kisi bhi object ka address rakh sakta hai. Par usme **type kho jaata
hai** — use karne se pehle aapko khud `static_cast` karke sahi type wapas lana
padta hai.

---

## Conversions

```cpp
int* p = ...;
void* v = p;                        // ✅ implicit  T* -> void*
int* q = static_cast<int*>(v);      // ✅ explicit  void* -> T*  (aap type ki guarantee de rahe ho)
int* r = v;                         // ❌ C++ mein ERROR (C mein chalta hai) -- cast chahiye
const int* cp = ...;
const void* cv = cp;                // ✅ const bacha hua hai
void* bad = cv;                     // ❌ const gira nahi sakte
```

⚠️ `static_cast<WrongType*>(v)` compile to ho jaata hai, par woh ek **jhoot** hai
— usse deref karna UB hai. `void*` aapko koi safety nahi deta; aap **vaada** kar
rahe ho ki aapko asli type pata hai.

---

## `void*` kahan-kahan dikhta hai

```cpp
// C ke memory functions
void* p = std::malloc(100);
std::memcpy(dst, src, n);           // void* dst, const void* src
std::memset(buf, 0, n);

// C-style callbacks jo "user data" lete hain
void register_handler(void (*cb)(void* userData), void* userData);

// qsort
void qsort(void* base, size_t n, size_t size,
           int (*cmp)(const void*, const void*));

// C++: operator new void* return karta hai
void* operator new(std::size_t);

// pthreads
pthread_create(&t, nullptr, threadFn, arg);   // void* (*)(void*), void* arg
```

---

## `void*` vs modern C++

| Zaroorat | C: `void*` | Modern C++ |
|---|---|---|
| "koi bhi type" container | `void*` + manual casts | templates, `std::any`, `std::variant` |
| State wala callback | `void*` userData | capture wala lambda, `std::function` |
| Generic algorithm | `qsort(void*, ...)` | `std::sort` (templated, type-safe, tez) |
| Raw byte buffer | `void*` / `char*` | `std::byte*`, `std::span<std::byte>` |
| Opaque handle (PImpl) | `void*` | `std::unique_ptr<Impl>` |

`std::sort` `qsort` se isiliye jeetta hai kyunki woh type-safe hai (koi `void*`
cast nahi) aur uska comparator **inline** ho sakta hai (`void*` ke through
function-pointer call nahi karni padti).

---

## `void*` aur alignment / lifetime

```cpp
void* raw = std::malloc(sizeof(Widget));
Widget* w = static_cast<Widget*>(raw);     // ⚠️ malloc aligned memory to deta hai,
                                           //    par koi Widget CONSTRUCT nahi hua!
new (raw) Widget{...};                     // placement new -- wahin construct karo (folder 14)
```

Yeh bahut zaroori baat hai: `malloc`/`operator new` sirf raw, **uninitialized**
memory dete hain. `void*` ko `T*` mein cast karne se `T` **ban nahi jaata** —
uske liye construction chahiye (`new`, placement new). Aur memory `T` ke liye
aligned bhi honi chahiye (`malloc` max alignment guarantee karta hai; `char[]`
buffer ke liye `alignas` lagana padta hai).

---

## Andar kya hota hai

- `void*` 8 bytes ka hai (ek address), baaki pointers jaisa hi.
- `T* -> void* -> T*` ka round-trip ek no-op hai (bits wahi rehte hain);
  `void* -> WrongType*` bhi *bit level pe* no-op hai — par matlab ke hisaab se
  jhoot hai.
- `void*` wale generics (`qsort`, `void*` callbacks) ek **indirect call** force
  karte hain (function pointer ke through) aur comparator/handler ko **inline
  nahi hone dete** → templated version se slow, kyunki wahan compiler ko concrete
  type dikhta hai.
- Arithmetic nahi hai → compiler `v + n` reject karta hai (`lea` ko scale chahiye
  hota, jo yahan hai hi nahi).

> **HFT relevance:** Hot HFT code mein `void*` avoid kiya jaata hai kyunki woh
> inlining aur type safety dono maar deta hai — templates / `if constexpr` /
> concrete overloads wahi flexibility zero cost pe aur poori optimization ke
> saath dete hain. `void*` sirf C-API aur OS boundaries pe bachta hai (epoll
> `data.ptr`, thread args, `mmap` ka return, DPDK/kernel-bypass APIs) aur wahan
> bhi turant typed pointer mein cast kar liya jaata hai. Raw byte buffers ke liye
> `void*` ki jagah `std::byte*` / `std::span<std::byte>` use hota hai (arithmetic
> chalti hai, aur intent saaf dikhta hai).

---

## Hands-on

```cpp
#include <cstdlib>
#include <cstring>
#include <iostream>
int main() {
    int x = 42;
    void* v = &x;
    std::cout << *static_cast<int*>(v) << "\n";     // 42

    void* buf = std::malloc(sizeof(double));
    double* d = static_cast<double*>(buf);
    *d = 3.14;                                       // theek hai: double trivial hai, malloc aligned hai
    std::cout << *d << "\n";
    std::free(buf);
}
```

```bash
g++ -std=c++20 -Wall -Wextra vp.cpp -o vp && ./vp
```

---

## ⚠️ Traps

### Trap 1 — `void*` ko deref karna
```cpp
void* v = &x;  *v;   // ❌ ERROR -- pehle cast karo
```

### Trap 2 — `void*` pe arithmetic
```cpp
void* v = buf;  v + 4;   // ❌ ERROR (GCC extension ise +4 bytes maanta hai -- portable nahi)
```

### Trap 3 — galat type mein cast
```cpp
double d;  void* v = &d;  int* p = static_cast<int*>(v);  *p;   // ⚠️ UB -- yeh int hai hi nahi
```

### Trap 4 — `void* -> T*` bina construction ke
```cpp
void* raw = malloc(sizeof(std::string));
std::string* s = static_cast<std::string*>(raw);
s->size();   // ⚠️ koi string construct hi nahi hui -- UB. Pehle placement new
```

### Trap 5 — C++ mein `void*` jahan template kaafi tha
```cpp
void process(void* data, TypeTag tag);   // ⚠️ galti hone ka pura mauka. Overloads / template / variant lo
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`void*` deref ho sakta hai" | Pehle concrete `T*` mein cast karo |
| "`void*` pe `+` chalta hai" | Element size nahi — arithmetic nahi |
| "`static_cast<T*>(voidp)` safe bana deta hai" | Woh sirf ek vaada hai — galat type = UB |
| "`void* -> T*` ek `T` bana deta hai" | Sirf bits ko dobara padhta hai — construction alag cheez hai |
| "Generic C++ ke liye `void*` theek hai" | Templates: type-safe + inline-able + tez |

---

## Exercises

1. **Round-trip:** `long n = 99; void* v = &n;` — `static_cast` se wapas nikaal ke
   print karo. Ab `int*` mein cast karke print karo — sahi aaya? Kyun nahi?

2. **`qsort`:** ek `int[]` ko C ke `qsort` se sort karo (`int(*)(const void*,
   const void*)` comparator likho). Phir `std::sort` se. Kisme cast lagana pada?

3. **Callback + userData:** `void forEach(int* a, size_t n, void (*cb)(int,
   void*), void* ud);` implement karo; `ud` ke zariye running-sum accumulator
   pass karo. Phir wahi capturing lambda + `std::function` se dobara karo.

4. **Alignment:** `void* p = malloc(1);` — `reinterpret_cast<uintptr_t>(p) %
   alignof(std::max_align_t)` print karo. Hamesha 0 aata hai?

5. **Placement new:** `void* raw = ::operator new(sizeof(std::string)); auto* s =
   new (raw) std::string("hi"); ... s->~basic_string(); ::operator delete(raw);`
   — chala ke dekho.

6. **PImpl sketch:** ek class banao jisme `void* impl_` (opaque) ho. Phir wahi
   modern `std::unique_ptr<Impl>` wala version. Kisme manual `delete` karna pada?

---

## Interview questions

1. `void*` kya hai? Deref / arithmetic kyun nahi?
2. `T* <-> void*` conversions — kaunsa implicit, kaunsa explicit?
3. `void*` ko galat type mein cast — kya hota hai?
4. `qsort` vs `std::sort` — `void*` ka performance impact?
5. `void*` se `T*` cast karne ke baad `T` construct hua? Nahi to kaise?
6. Modern C++ mein `void*` kahan bacha hai, kahan replace hua?

---

## Next
→ [`11-function-pointers.md`](11-function-pointers.md)

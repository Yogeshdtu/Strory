# 02 — Object lifetime

## Prerequisites
- `01-what-is-an-object.md`, `17-RAII` (dtors, unwinding)
- [`examples/01_lifetime_demo.cpp`](examples/01_lifetime_demo.cpp)

## Yeh topic abhi kyun
"Lifetime" woh window hai jismein ek object ko use karna **legal** hai. Us window
ke bahar — pehle ya baad — access karna **UB** hai. RAII, dangling pointers,
placement new, storage reuse — sab is ek concept pe khade hain. Aur `sizeof(T)`
storage hai; lifetime uss storage mein object ki "existence" hai — **do alag
cheezein**.

---

## Lifetime kab shuru, kab khatam

### Non-trivial type (constructor / destructor hai)

```
storage allocate  ─────►  CONSTRUCTOR runs  ─────►  [ LIFETIME ]  ─────►  DESTRUCTOR starts  ─────►  storage freed
                          (jab yeh COMPLETE                                (jab yeh START
                           hota — lifetime START)                          hota — lifetime END)
```

- **Start:** jab initialization (constructor) **complete** ho jaaye. Constructor
  ke *andar* `this` ko poora object samajh ke use karna — risky (members abhi ban
  rahe).
- **End:** jab destructor **call hona shuru** ho. Destructor ke andar members abhi
  zinda hain (reverse order mein destruct honge).

### Trivial type (`int`, `double`, POD struct)

- **Start:** jab storage acquire ho jaaye aur woh object ke type/alignment ke liye
  suitable ho. Koi constructor nahi chalta.
- **End:** jab storage release ho, ya usi jagah naya object aa jaaye (reuse).
- Value **indeterminate** rehti jab tak likha na jaaye (`int x;` ka `x` — garbage;
  padho to UB most types ke liye).

**Implicit-lifetime types** (C++20): scalar types, aggregate types with only
implicit-lifetime members, aur kuch class types. Inke objects `std::malloc` /
`std::memcpy` / `std::bit_cast` **implicitly create** kar sakte hain — bina
placement new ke. Isliye `T* p = (T*)std::malloc(sizeof(T)); p->field = ...;` C++20
mein OK hai agar `T` implicit-lifetime hai.

---

## Storage duration vs lifetime

**Alag cheezein:**

```cpp
alignas(Widget) unsigned char buf[sizeof(Widget)];   // STORAGE: poore scope ka

// buf ka storage yahan allocated hai, par usme koi Widget OBJECT nahi.

Widget* w = ::new (buf) Widget(...);   // Widget ki LIFETIME yahan shuru
w->use();
w->~Widget();                           // Widget ki LIFETIME yahan khatam

// buf ka storage abhi bhi allocated. Koi Widget object nahi.

Widget* w2 = ::new (buf) Widget(...);  // usi storage mein NAYA Widget — nayi lifetime
w2->~Widget();
```

| | Storage duration | Lifetime |
|---|---|---|
| Kya | memory kitni der reserved | object us memory mein kitni der "hai" |
| Automatic var | scope | (usually) same as storage |
| `char buf[N]` | scope | jitni der aap placement-new/dtor se manage karo |
| `new Widget` | `delete` tak | ctor → dtor (aap `delete` se dono trigger karte) |

`examples/01` section 3: ek `buf`, teen alag Widgets ek ke baad ek — storage ek,
lifetimes teen.

---

## Us window ke bahar access = UB

```cpp
// BEFORE lifetime:
Widget w;
// ... (w ka ctor abhi chal raha, ek member se doosre ko access — careful)

// AFTER lifetime:
Widget* p = new Widget;
delete p;
p->use();               // ⚠️ use-after-free — UB
Widget& r = *p;
int n = r.field;         // ⚠️ UB

// Storage reuse ke baad, PURANE pointer se:
int x = 5;
new (&x) float(1.5f);   // x ki lifetime khatam, float ki shuru
int y = x;               // ⚠️ UB — `x` object ab exist nahi karta
```

**"Kaam kar gaya" ≠ legal.** Freed memory abhi tak overwrite nahi hui to `p->use()`
"chal jaata" hai — par compiler `-O2` pe assume karta hai woh access valid tha,
aur us assumption pe optimize karta (file 16).

---

## Pointer / reference lifetime rules ke saath

Storage reuse ka ek sharp corner:

```cpp
struct Thing { const int id; };      // const member!
Thing a{1};
Thing* p = &a;
new (&a) Thing{2};                    // a ki jagah naya Thing
// p abhi bhi &a ko point karta — par kya p->id ab 2 dega?
int wrong = p->id;                    // ⚠️ UB — p purane object ko refer karta
int right = std::launder(p)->id;      // ✅ std::launder — "is address pe jo AB hai"
Thing* p2 = new (&a) Thing{3};        // ✅ ya naya pointer use karo
```

`std::launder` (file 09) compiler ko batata: "is pointer ke through jo *ab* us
address pe hai usse access karo, jo *pehle* tha usse nahi." Zaroori jab type mein
`const` / reference members hon, ya vtable (compiler purani value/vptr cache kar
sakta).

---

## Constructor / destructor ke beech throw (folder 23 recap)

- **Ctor throw** → jitne members/bases **ban chuke** unke dtors chalte, object
  khud ki lifetime **shuru hi nahi hui** → `~T()` nahi chalta.
- **Dtor throw during unwinding** → `std::terminate`.

Yeh lifetime rules ka hi consequence hai: "lifetime tabhi shuru jab ctor complete."

---

## Andar kya hota hai

- Trivial object: koi runtime marker nahi — lifetime compiler ke model mein exist
  karti hai (optimization rules ke liye). Bytes bas storage mein hain.
- Non-trivial: ctor = ek function call jo members initialize karta + vptr set karta
  (polymorphic ho to). Dtor = reverse. `new`/`delete` = `operator new` +
  ctor / dtor + `operator delete`.
- **Object lifetime = aliasing/optimization ka foundation.** "Is object ki value
  is point pe kya hai" compiler is model se derive karta. UB (out-of-lifetime
  access) us model ko todta → arbitrary codegen.
- `std::launder` → `__builtin_launder` → optimization barrier: "is pointer ki
  provenance ke baare mein kuch mat maano."

---

## > **HFT relevance**
> - **Arena / pool / slab allocators** — pre-allocate ek bada `std::byte[]`, phir
>   objects ke lifetimes ko placement-new + explicit-dtor se manage karo (file 09).
>   Hot path pe zero `malloc`. Poora order-book / message-pool design isi pe.
> - **Ring buffers of trivially-copyable messages** — slot ka storage fixed,
>   objects `memcpy` se in-place aate-jaate (trivial → lifetime storage ke saath).
> - **`std::launder` after reuse** — jab ek slab slot ko ek naye object ke liye
>   reuse karo aur us type mein `const`/reference/vtable ho.
> - **Dangling = the classic "works in dev, spikes in prod" bug** — ek reference
>   jo ek destroyed temporary ya popped queue-entry ko point karta (file 05).
>   Object-lifetime rules exactly batate hain kahan yeh ho sakta.

---

## Hands-on

```bash
./build.ps1 25-OBJECT-MODEL/examples/01_lifetime_demo.cpp
```

Sections: automatic (scope), manual in `buf` (placement new + explicit dtor),
storage reuse (3 objects), alag type in same slot. Phir:
- Ek `int x = 5;` par `new (&x) double(2.5);` karo, phir `x` read karo (`-O0` pe
  "kaam karega" — par UB). `std::launder`-jaisa correct access socho.
- `Tracer` object ko `delete` karne ke baad `->hello()` call karo — `-O0` vs
  `-fsanitize=address` (Linux) pe farq.

---

## ⚠️ Traps

### Trap 1 — storage zinda = object zinda maanna
`char buf[N]` scope-long hai par usme koi object nahi jab tak aap na banao.
`sizeof` != lifetime.

### Trap 2 — placement-new ke baad purane pointer se access (const/ref/vtable members)
```cpp
new (&obj) T{...};
oldPtr->member;          // ⚠️ UB if T has const/ref members or a vtable
std::launder(oldPtr)->member;   // ✅
```

### Trap 3 — use-after-free "kaam kar gaya"
`delete p; p->f();` — memory abhi overwrite nahi hui → "chala". `-O2` pe /
production mein → corruption.

### Trap 4 — explicit dtor call bhoolna (placement new ke saath)
`::new (buf) std::string(...)` ke baad `->~basic_string()` na karo → leak (string
ka heap buffer).

### Trap 5 — dtor call + phir automatic dtor bhi (double destruction)
```cpp
{
  std::string s = "x";
  s.~basic_string();     // ⚠️ manual dtor
}                         // scope-end pe s ka dtor DOBARA — double free
```
Placement new + manual dtor sirf un objects pe jinka automatic dtor nahi chalega.

### Trap 6 — implicit-lifetime assume karna non-trivial type ke liye
`T* p = (T*)malloc(sizeof(T)); p->method();` — sirf agar `T` **implicit-lifetime**
hai (C++20). `std::string`, `std::vector` — nahi. Unke liye placement new.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "storage allocated = object exists" | Non-trivial type: object exists only ctor→dtor |
| "lifetime = scope, hamesha" | Automatic vars: usually. `char buf[]` + placement new: you decide |
| "use-after-free crashes reliably" | Often "works" until overwritten; UB regardless, `-O2` bites |
| "placement new ke baad koi cleanup nahi" | Explicit `p->~T()` — no automatic dtor for that object |
| "purana pointer reuse ke baad theek hai" | UB for const/ref/vtable members — `std::launder` or a fresh pointer |
| "`malloc` + cast + use = fine" | Only for implicit-lifetime types (C++20); else placement new |

---

## Exercises

1. **Lifetime span:** for `Widget w;` in a function, when does `w`'s lifetime start
   and end?

   <details><summary>Answer</summary>

   Start: when `Widget`'s constructor completes (at the declaration). End: when its
   destructor **begins** (at the end of the enclosing scope, or earlier via
   unwinding). The storage is the stack slot; the lifetime is ctor→dtor.
   </details>

2. **Reuse:** `int x = 1; new (&x) float(2.0f); float f = x_as_float;` — how do you
   correctly read the float that now lives at `&x`?

   <details><summary>Answer</summary>

   Use the pointer returned by placement new: `float* fp = new (&x) float(2.0f);
   float f = *fp;` — or `float f = *std::launder(reinterpret_cast<float*>(&x));`.
   Reading `x` (the `int` name) is UB — that object's lifetime ended when the
   `float` was placed.
   </details>

3. **Double destruction:** what's wrong?
   ```cpp
   alignas(std::string) unsigned char b[sizeof(std::string)];
   auto* s = new (b) std::string("hi");
   // ... use s ...
   ```

   <details><summary>Answer</summary>

   Nothing yet, but you must call `s->~basic_string()` (via an alias) before `b`
   goes out of scope — otherwise the string's heap buffer leaks. And you must
   **not** also have an automatic `std::string` there; `b` is raw bytes with no
   automatic dtor, so exactly one explicit dtor call is correct.
   </details>

4. **Trivial vs not:** `struct A { int x, y; };` and `struct B { std::string s; };`
   — for which is `T* p = (T*)malloc(sizeof(T)); p->/*member*/ = ...;` OK in C++20?

   <details><summary>Answer</summary>

   `A` — OK (implicit-lifetime aggregate; `malloc` + write implicitly creates the
   object in C++20). `B` — **not** OK; `std::string` is not implicit-lifetime, its
   constructor must run — use placement new (`new (p) B{...}`).
   </details>

5. **`std::launder` need:** `struct T { const int id; };` in a slab. After
   `new (slot) T{2}` over an old `T{1}`, why is `oldPtr->id` UB, and what fixes it?

   <details><summary>Answer</summary>

   The compiler is allowed to assume `oldPtr->id` still refers to the *old*
   object's `const int` (whose value it may have cached / propagated as `1`).
   Accessing the new object through the stale pointer is UB. Fix: use the pointer
   returned by placement new, or `std::launder(oldPtr)->id`.
   </details>

---

## Interview questions

1. Object lifetime kab shuru, kab khatam — trivial vs non-trivial type.
2. Storage duration vs lifetime — ek example jahan dono alag hain.
3. Out-of-lifetime access ka kya (UB) — "kaam kar gaya" kyun proof nahi?
4. `std::launder` kis liye — kab zaroori (const/ref/vtable members + reuse)?
5. Implicit-lifetime types (C++20) — `malloc` + use kab legal?
6. Placement new ke baad cleanup ki zimmedari kiski?
7. Ctor ke beech throw — object ki lifetime shuru hui? `~T()` chalega?

---

## Next
→ [`03-storage-duration.md`](03-storage-duration.md)

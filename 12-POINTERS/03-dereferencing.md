# 03 — `*` — dereferencing

## Prerequisites
- [`01-what-is-a-pointer.md`](01-what-is-a-pointer.md), [`02-address-of-operator.md`](02-address-of-operator.md)

## Yeh topic abhi kyun
`*p` = "jis address pe `p` point karta hai, wahan ki value." Yeh pointer ka
poora point hai — data tak indirect pahunch. Aur `*` ke **do alag uses**
(declaration mein type banane ke liye, expression mein deref) confusing hain.

---

## `*p` — "us address pe rakhi value"

```cpp
int  x = 10;
int* p = &x;

int y = *p;        // y = 10  -- p jahan point karta hai, wahan ki value padho
*p = 20;           // us jagah pe 20 likho -> ab x = 20
*p += 5;           // padho-badlo-likho -> ab x = 25
(*p)++;            // x -> 26   (⚠️ parens zaroori: *p++ alag cheez hai, file 05)
```

`*p` ek **lvalue** hai — matlab usse sirf padh hi nahi sakte, usme **likh** bhi
sakte ho. Jab tak `p` `x` pe point kar raha hai, `*p` `x` ka doosra naam hai.

---

## 🔑 Declaration ka `*` vs expression ka `*`

Yeh woh jagah hai jahan zyadatar log phaste hain. Ek hi symbol, do bilkul alag kaam:

```cpp
int* p = &x;       // DECLARATION: `*` type ka hissa hai -- "pointer to int"
int  y = *p;       // EXPRESSION:  `*` dereference operator hai
```

- **Declaration mein** (`int* p`, `double* q`) → `*` sirf **type banata** hai.
  Chalte waqt yeh kuch "karta" nahi.
- **Expression mein** (`*p`, `*(p + 1)`) → `*` asli **memory access** karta hai.

Aur isi wajah se yeh trap hai:

```cpp
int* p, q;         // ⚠️ p pointer hai, par q sirf int hai! `*` naam se chipakta hai, type se nahi
int *p, *q;        // dono int* -- isi tarah likho, ya har ek alag line pe
```

---

## Chained dereference

```cpp
int   x   = 7;
int*  px  = &x;
int** ppx = &px;   // pointer ka pointer (file 09)

*ppx        // == px   (ek int*)
**ppx       // == x    (7)
**ppx = 42; // x -> 42
```

Har `*` **ek arrow follow karta hai**. Do arrow paar karne hain to do `*` lagao.

---

## `->` — struct/class members ke liye (file 08)

```cpp
struct P { int x, y; };
P  pt{1, 2};
P* p = &pt;

(*p).x        // 1  -- pehle deref, phir member
p->x          // 1  -- wahi cheez, chhota aur saaf tareeka
```

`p->x` **hai hi** `(*p).x`. Hamesha `->` use karo — parens bhoolna ek common bug hai.

---

## ⚠️ Invalid pointer ko deref karna = UB

```cpp
int* p = nullptr;
*p;                // 💥 null deref -> segfault (aksar)

int* q;            // initialize hi nahi kiya
*q;                // 💥 kisi random address pe padhna/likhna -> crash ya chupchaap corruption

int* r;
{ int t = 1; r = &t; }
*r;                // 💥 dangling -> UB (file 12)
```

`*p` likhne se pehle khud se poocho: **`p` null to nahi, aur jis object pe point
kar raha hai woh abhi zinda hai?** Agar pakka nahi keh sakte, to check lagao
(`if (p)`), ya reference / `std::optional` / koi container use karo.

---

## Const aur `*` (file 07)

```cpp
const int* p = &x;    // *p read-only hai:  *p = 5;  ❌
int* const p = &x;    // p dobara point nahi kar sakta, par *p = 5;  ✅
```

Mota-moti rule: `const` agar `*` se **pehle** hai → jispe point kar rahe ho woh
const hai (pointee). `*` ke **baad** hai → pointer khud const hai.

---

## Andar kya hota hai

- `*p` (read) → `mov reg, [p]` — `p` mein rakhe address se `sizeof(*p)` bytes ka
  ek load.
- `*p = v` (write) → `mov [p], v` — ek store.
- CPU **koi check nahi** karti. Agar `[p]` unmapped hai → MMU fault uthata hai →
  OS SIGSEGV bhejta hai. Aur agar `[p]` mapped to hai par galat hai (dangling
  pointer jo reuse ho chuki memory mein ghus gaya) → tum chupchaap kisi aur ka
  data padh/likh rahe ho. **Yeh doosra case zyada khatarnaak hai** — crash nahi
  hota, bas answer galat aata hai.
- `-O2` pe compiler `*p` ko register mein rakh sakta hai (baar-baar load na kare),
  par sirf tab jab woh **prove** kar sake ki beech mein koi doosra write us
  memory ko touch nahi karta (`__restrict` isme madad karta hai). Warna har baar
  dobara load karega.

> **HFT relevance:** Wire buffer se padha gaya har field, intrusive list ka har
> hop, arena ka har slot access — sab ek `*` (ya `->`) hai. Cost ke hisaab se
> yeh ek single load/store hai — practically free. Khatra poora ka poora
> **correctness** ka hai: hot path mein ek stale ya null pointer deref matlab
> crash (achha case) ya order book ki corruption (bura case). Isiliye modern
> discipline yeh hai — non-owning access `reference` / `span` / `string_view` se
> (null ho hi nahi sakte, lifetime saaf dikhti hai), ownership `unique_ptr` /
> containers se, aur raw `*`/`->` sirf wahan jahan lifetime ache se soch li ho.

---

## Hands-on

`examples/01_basic_pointers.cpp`, `examples/07_pointer_diagrams.cpp` (`**ppx`
ka demo):

```bash
./build.ps1 12-POINTERS/examples/01_basic_pointers.cpp
```

---

## ⚠️ Traps

### Trap 1 — `int* p, q;`
```cpp
int* p, q;      // ⚠️ q int hai, int* nahi. `int *p, *q;` likho ya alag-alag lines
```

### Trap 2 — `*p++` vs `(*p)++`
```cpp
int a[] = {1, 2}; int* p = a;
int x = *p++;   // x = 1, phir p aage badhta hai (postfix ++ ki binding * se tight hai)
int y = (*p)++; // y = 2, phir a[1] 3 ban jaata hai
```

### Trap 3 — null / uninitialized / dangling ko deref karna
```cpp
*nullptr;  *uninitPtr;  *danglingPtr;   // teeno 💥 UB
```

### Trap 4 — `*p` jahan `p` hil chuka ho (vector realloc)
```cpp
int& r = v[0]; v.push_back(x); r = 5;   // ⚠️ r (ek deref alias) dangle kar sakta hai
```

### Trap 5 — pointer pe `->` ki jagah `.` likh dena
```cpp
P* p = &pt;  p.x = 1;   // ❌ ERROR -- p->x  (ya (*p).x) likho
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`int* p` wala `*` deref karta hai" | Woh type banata hai; deref sirf expression mein hota hai |
| "`int* p, q;` do pointer banata hai" | `q` ek plain `int` hai |
| "`*p` sirf padhne ke liye hai" | Woh lvalue hai — padh **aur** likh dono sakte ho |
| "`p->x` aur `(*p).x` alag hain" | Bilkul same — `->` sirf chhota roop hai |
| "Deref runtime pe check hota hai" | Koi check nahi — galat `p` matlab UB |

---

## Exercises

1. **Read/write:** `int x = 5; int* p = &x;` — `*p = 20; *p += 3; (*p)++;` karo —
   har step ke baad `x` print karo.

2. **Declaration trap:** `int* a, b, *c;` — inme se pointer kaun-kaun hai? Saaf
   karke dobara likho.

3. **`*p++` vs `(*p)++`:** `int arr[3] = {10, 20, 30}; int* p = arr;` — `int x =
   *p++;` phir `int y = (*p)++;` trace karo, phir `arr` aur `x`, `y` print karo.

4. **Chained:** `int v = 1; int* p = &v; int** pp = &p;` — sirf `pp` use karke
   `v` ko `99` banao.

5. **Null-deref (dhyaan se):** `int* p = nullptr; std::cout << *p;` — chalao. Kya
   hota hai? (Segfault — yahi expected hai.)

6. **`.` vs `->`:** `struct S { int n; }; S s{5}; S* p = &s;` — `p.n = 10;` ko
   theek karo.

---

## Interview questions

1. `*p` kya karta hai? Lvalue hai — matlab?
2. Declaration mein `*` aur expression mein `*` — fark?
3. `int* p, q;` — `q` kya hai?
4. `*p++` aur `(*p)++` mein fark?
5. `p->x` == `?`
6. Invalid pointer deref — read vs write, kya hota hai?

---

## Next
→ [`04-nullptr.md`](04-nullptr.md)

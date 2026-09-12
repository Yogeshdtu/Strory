# 04 — `nullptr`

## Prerequisites
- [`03-dereferencing.md`](03-dereferencing.md)
- `08-FUNCTIONS/08-function-overloading.md` (`0` vs `nullptr` overload)

## Yeh topic abhi kyun
Ek pointer ko "kuch nahi point kar raha" batane ke liye `nullptr` (C++11).
Purane `NULL` / `0` ke saath type-safety aur overload bugs hain. Aur null
dereference C++ ka sabse common crash hai.

---

## `nullptr` — "kisi cheez pe point nahi kar raha"

```cpp
int* p = nullptr;      // p valid hai, par kisi object ko point NAHI karta

if (p == nullptr) { }  // check
if (p) { }             // wahi baat -- non-null -> true, null -> false
if (!p) { }            // "p null hai"
```

`nullptr` ka type `std::nullptr_t` hai — yeh kisi bhi pointer type mein
apne-aap convert ho jaata hai, par `int` mein **nahi**. Yahi uski asli taakat hai.

---

## `NULL` / `0` kyun nahi

```cpp
// Purana C style:
#define NULL 0         // ya ((void*)0) -- platform pe depend karta hai

int* p = NULL;         // "chal jaata hai" -- par NULL asal mein 0 hi hai
```

### Problem 1 — overload resolution (folder 08 file 08)
```cpp
void f(int);
void f(char*);

f(0);          // -> f(int)   (0 ek int literal hai)
f(NULL);       // ⚠️ zyadatar platforms pe -> f(int)! (kyunki NULL = 0) -- f(char*) nahi
f(nullptr);    // -> f(char*)  ✅ (nullptr_t pointer mein convert hota hai, int mein nahi)
```

### Problem 2 — pointer ki jagah `0` ek magic number lagta hai
```cpp
int x = 0;     // integer ko zero karo
int* p = 0;    // ⚠️ "null pointer" -- par padhne mein lagta hai "address 0 pe point karo"
int* p = nullptr;   // ✅ niyat saaf dikh rahi hai
```

### Problem 3 — templates / `auto`
```cpp
auto x = NULL;       // ⚠️ x int (ya long) hai -- pointer nahi!
auto y = nullptr;    // y std::nullptr_t hai -- sahi
```

**Rule: hamesha `nullptr`. Pointer ke liye `NULL` ya `0` kabhi nahi.**

---

## 🔑 Null dereference = crash

```cpp
int* p = nullptr;
int x = *p;            // 💥 SIGSEGV -- address 0 padhne ki koshish (unmapped hai)
p->field;             // 💥 wahi baat
p[5];                 // 💥 *(p + 5) -> address 20 (unmapped)
```

Address 0 (aur uske aas-paas ka pehla page) OS **jaan-boojh kar unmapped**
chhodta hai — taaki null deref turant fault kare, data corrupt karne ke bajaye.
Yeh bug nahi, **feature** hai: saaf crash chupchaap corruption se behtar hai.

---

## Check karna — guard clauses

```cpp
void process(Order* o) {
    if (o == nullptr) return;        // guard (folder 06 file 03)
    o->qty -= 1;                     // ab safe hai
}

// Chained -- short-circuit (folder 05 file 04)
if (node != nullptr && node->next != nullptr) { use(node->next->value); }
```

### Jab aapko *pata* hai ki null nahi ho sakta
- Pointer ki jagah **reference** parameter lo (folder 13) — woh null ho hi nahi sakti.
- `gsl::not_null<T*>` (Guidelines Support Library) — intent document bhi karta hai, check bhi.
- `[[assume(p != nullptr)]]` (C++23) — optimizer ko batata hai (galat hua to UB).

---

## `nullptr` comparisons aur containers mein

```cpp
std::vector<Node*> nodes;
nodes.push_back(nullptr);            // theek hai -- ek null element

std::find(nodes.begin(), nodes.end(), nullptr);   // chalta hai

Node* a = nullptr, *b = nullptr;
a == b;                              // true -- saare null aapas mein barabar hote hain
```

---

## Andar kya hota hai

- `nullptr` ek compile-time constant hai; pointer mein assign karne pe value `0`
  store hoti hai (all-zeros address). Standard yeh *zaroori* nahi karta ki woh
  literally 0 ho, par har asli platform 0 hi use karta hai.
- `if (p)` → `test rax, rax; jz ...` — bas ek instruction.
- `*nullptr` → `mov reg, [0]` → page 0 ki koi mapping nahi hai → page fault →
  OS SIGSEGV bhejta hai (Linux pe exit code 139) / Windows pe `0xC0000005`.
- `-O2` pe ek mazedaar cheez: agar compiler **prove** kar le ki pointer deref ho
  chuka hai, to woh **maan leta hai** ki pointer non-null hai, aur uske baad ka
  `if (p)` check **dead code samajh ke uda deta hai**. "Mera null check gayab ho
  gaya" wale surprises ki yahi wajah hai (folder 33).

> **HFT relevance:** Null deref crash ki sabse upar wali wajahon mein hai. Bachne
> ka tareeka: jahan value **hamesha maujood** rehni hai wahan **reference** lo
> (`const Order&` null ho hi nahi sakti — na check chahiye, na branch); pointer
> sirf wahan jahan "absent" ek asli state ho, aur phir har use pe guard clause.
> Sabse hot paths mein non-null ki guarantee construction ke waqt hi enforce
> ki jaati hai (ya `[[assume]]` se batayi jaati hai), taaki hot loop mein null
> branch rahe hi na. Aur `NULL`/`0` kabhi nahi — sirf `nullptr`.

---

## Hands-on

`examples/04_pointers_structs.cpp` (`applyFill`/`printOrder` mein nullptr guard),
`examples/06_dangling_pointer.cpp`:

```bash
./build.ps1 12-POINTERS/examples/04_pointers_structs.cpp
```

Ek scratch file mein `int* p = nullptr; std::cout << *p;` likh ke chalao — crash
khud dekho.

---

## ⚠️ Traps

### Trap 1 — overloaded call mein `NULL`
```cpp
handler(NULL);   // ⚠️ int wala overload chun sakta hai. handler(nullptr) likho
```

### Trap 2 — `auto x = NULL;`
```cpp
auto x = NULL;   // ⚠️ x int/long hai, pointer nahi. auto x = nullptr; likho
```

### Trap 3 — check se pehle deref
```cpp
if (p->ready && p != nullptr) { }   // ⚠️ p->ready pehle chalega -> null hua to crash
if (p != nullptr && p->ready) { }   // ✅ short-circuit bachata hai
```

### Trap 4 — maan lena ki failed lookup bhi deref ho sakta hai
```cpp
Node* n = find(key);   // miss pe nullptr deta hai
n->value;              // ⚠️ miss pe crash. Pehle check karo
```

### Trap 5 — gayab hota null check (`-O2`)
```cpp
void f(int* p) { int x = *p; if (p) g(); }   // ⚠️ compiler `if (p)` hata sakta hai (deref ho chuka hai)
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`NULL` aur `nullptr` ek hi cheez hain" | `NULL` `0` hai (ek int); `nullptr` `nullptr_t` hai |
| "`int* p = 0;` saaf likha hai" | `nullptr` niyat batata hai; `0` "address 0" jaisa padhta hai |
| "`auto x = NULL;` pointer deta hai" | Woh integer deta hai |
| "Null deref kabhi-kabhi harmless hai" | Woh UB hai — design se hi reliably segfault |
| "Use ke baad check kar lo, compiler sambhaal lega" | Deref-phir-check ko optimizer uda sakta hai |

---

## Exercises

1. **Overload:** `void log(int); void log(const char*);` — `log(0)`, `log(NULL)`,
   `log(nullptr)` — har ek kaunsa call karega? Apne platform pe test karo.

2. **`auto`:** `auto a = NULL; auto b = nullptr;` — `sizeof(a)`, `sizeof(b)`, aur
   kya dono ko `int*` mein assign kar sakte ho?

3. **Crash karao:** `Node* n = nullptr; std::cout << n->value;` — chalao. Exit code?

4. **Guard order:** `if (p->x > 0 && p) {}` vs `if (p && p->x > 0) {}` — `p ==
   nullptr` pe kaunsa safe hai? Kyun?

5. **Reference vs pointer:** `void f(Order* o)` (null guard ke saath) ko
   `void f(Order& o)` mein badlo. Reference wala version kya guarantee deta hai?

6. **Gayab check:** `void g(int* p) { *p = 1; if (!p) return; *p = 2; }` —
   `-O2 -S` se compile karo. `if (!p)` abhi bhi hai kya?

---

## Interview questions

1. `nullptr` vs `NULL` vs `0` — `NULL`/`0` ke 3 concrete problems?
2. `nullptr` ka type? Kis mein convert hota hai, kis mein nahi?
3. Null dereference — kya hota hai, aur kyun address 0 unmapped hota hai?
4. Null check ka sahi order (short-circuit)?
5. "Always present" ke liye pointer ya reference? Kyun?
6. `-O2` null check ko kyun/kab hata sakta hai?

---

## Next
→ [`05-pointer-arithmetic.md`](05-pointer-arithmetic.md)

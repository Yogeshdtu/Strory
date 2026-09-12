# 01 — Reference kya hai (alias)

## Prerequisites
- Folder 12 poora (`12-POINTERS/` — `&`, `*`, addresses, `const` + pointers)
- `03-VARIABLES-DATA-TYPES/` — variables, memory, copy

## Yeh topic abhi kyun
Pointer aap seekh chuke ho — powerful, par verbose (`*`, `&`, null checks, dangling).
Bahut saare kaam ke liye C++ ek **halka, safer** tool deta hai: **reference**.
Reference = kisi maujooda object ka **doosra naam (alias)**. Yahi cheez aage
`const T&` parameters, range-`for`, operator overloading, aur move semantics
(folder 18) ka base hai.

---

## Reference = ek object ke do naam

```cpp
int  x = 10;
int& r = x;        // r "x ka reference hai" -- x ka doosra naam
```

`r` koi **naya object nahi** hai. `r` aur `x` — dono **ek hi memory location**
ko dekhte hain. Jo `r` ko karoge, wahi `x` ko hoga, aur ulta.

```cpp
r = 99;            // x ab 99
x = 7;             // r ab 7
std::cout << (&x == &r);   // 1  -- bilkul same address
```

```
   memory:  [ 10 ]  <- ek hi int
              ^  ^
              |  |
              x  r        (do naam, ek dabba)
```

Pointer ke ulaat, yahan **koi alag "pointer variable" nahi banta** jo address
rakhe. `r` compile ke baad seedha `x` ban jaata hai (zyada tar — "Andar kya hota
hai" dekho).

---

## Syntax — `&` type ke saath, expression mein nahi

```cpp
int& r = x;        // declaration:  "int&" ek type hai -- "reference to int"
```

Yahan `&` **address-of operator nahi hai**. Yeh type ka hissa hai. Folder 12
file 02 mein `&` ke 3 meanings the — yeh teesra tha:

| Context | `&` ka matlab |
|---|---|
| `int* p = &x;` | address-of operator (expression) |
| `int& r = x;` | **reference declaration** (type) |
| `a & b` | bitwise AND (folder 05) |

Padhne ka tareeka: **type se shuru karo, naam pe ruko** — "`int&` … `r`" →
"`r` is a reference to `int`".

---

## Teen non-negotiable rules

### Rule 1 — init ZAROORI hai (declaration ke waqt hi)

```cpp
int& r;            // ❌ compile ERROR -- "declared as reference but not initialized"
int& r = x;        // ✅
```

Pointer `int* p;` (uninitialized) ban jaata hai — khatarnaak, par legal.
Reference banana hi tab hota hai jab use kisi object se **bind** karo.

### Rule 2 — REBIND nahi hoti (kabhi)

Ek baar `r` ko `x` se bind kar diya, `r` **hamesha** `x` ka alias rahega.

```cpp
int x = 1, y = 2;
int& r = x;
r = y;             // ⚠️ yeh "r ko y se rebind" NAHI hai
                   //    yeh "x = y" hai  -- y ki value x mein copy ho gayi
std::cout << &r;   // abhi bhi &x
```

Reference pe koi bhi assignment **referent ko likhta hai**, reference ko move
nahi karta. Isiliye reference ko "sealed pointer" bhi kehte hain.

### Rule 3 — koi "null reference" nahi

```cpp
int& r = *(int*)nullptr;   // ⚠️ UB -- yeh "null reference" banane ki koshish, allowed nahi
```

Reference hamesha kisi **valid object** se bandhi maani jaati hai. `nullptr`
jaisa koi "kuch nahi" state nahi. (Practically galti se dangling ho sakti hai —
file 09 — par woh bug hai, feature nahi.)

---

## Kya reference bind ho sakti hai

```cpp
int x = 5;
int& a = x;              // ✅ lvalue (named object)
int& b = arr[2];         // ✅ array element
int& c = *p;             // ✅ dereferenced pointer (agar p valid)

int& d = 42;             // ❌ ERROR -- 42 ek temporary (prvalue), non-const ref bind nahi
const int& e = 42;       // ✅ const ref temporary ko bind kar sakti (lifetime extend -- file 04)

double y = 1.0;
int& f = y;              // ❌ ERROR -- type match hona chahiye (int& <- int)
```

Rule: **non-const `T&`** sirf ek maujooda, modifiable `T` lvalue se bind hoti hai.

---

## Reference vs copy vs pointer — ek nazar mein

```cpp
int x = 10;

int  c = x;     // COPY    -- alag memory, alag zindagi. c badlo -> x safe
int& r = x;     // ALIAS   -- same memory. r badlo -> x badla
int* p = &x;    // POINTER -- alag memory (address rakhta hai). rebindable, nullable
```

| | copy `int c` | reference `int& r` | pointer `int* p` |
|---|---|---|---|
| Naya storage? | haan (ek int) | nahi (alias) | haan (8 bytes) |
| Original se link | toota | juda hua | address ke through |
| Rebind | — | ❌ | ✅ `p = &y` |
| Null | — | ❌ | ✅ |
| Use syntax | `c` | `r` | `*p`, `p->m` |

---

## Andar kya hota hai

Standard reference ko "alias" kehta hai aur **storage mandate nahi karta**.
Practically:

- **Local reference to a local**, jab compiler dono ko dekh raha hai, aksar
  **zero storage** leta hai — `r` har jagah seedha `x` ke register/stack-slot se
  replace ho jaata hai. `sizeof` par yeh nahi dikhta (`sizeof(r)` = `sizeof(int)`),
  par generated assembly mein `r` ka koi alag address nahi hota.
- **Reference parameter** (`void f(int& n)`) aksar ek **pointer ki tarah pass**
  hota hai — caller `&arg` bhejta hai, callee andar automatically deref karta hai.
  ABI level pe `f(int&)` aur `f(int*)` ka code lagbhag same hota hai.
- **Reference member** (file 07) struct mein **ek pointer jitni jagah** (8 bytes)
  leta hai — kyunki object ko us alias ko kahin store karna padta hai.

To: reference ek **compile-time alias** hai jise, jarurat padne par, compiler ek
pointer se implement karta hai — bina aapko `*`/`&` likhwaye.

> **HFT relevance:** references hot code mein default "yeh object mujhe do bina
> copy ke" tool hain — `const T&` (read) aur `T&` (mutate). Cost pointer jaisi
> (aksar zero, kabhi ek address), par null-check aur deref-syntax ka bojh nahi,
> aur compiler ko "yeh hamesha valid hai" ka bharosa milta hai → behtar
> optimization (no null branch, better alias analysis with `T&` vs raw `T*`).
> Jahan "optional" ya "rebindable" chahiye wahan pointer (ya `std::optional`,
> `std::reference_wrapper`), warna reference.

---

## Hands-on

```bash
./build.ps1 13-REFERENCES/examples/01_references_basics.cpp
```

Dekho: `&x == &r`, `r` badalne pe `x` badalta hai, `r = y` rebind nahi karti,
aur reference ko `*`/`&` nahi chahiye use karte waqt.

---

## ⚠️ Traps

### Trap 1 — `r = y` ko "rebind" samajhna
```cpp
int& r = x;
r = y;             // x = y  (copy). r abhi bhi x
```

### Trap 2 — uninitialized reference
```cpp
int& r;            // ❌ ERROR. Reference ko banate waqt hi bind karna padta hai
```

### Trap 3 — non-const ref ko temporary/literal se bind
```cpp
int& r = 42;              // ❌
int& r = a + b;           // ❌ (a+b ek prvalue)
const int& r = a + b;     // ✅ (const ref temporary ko bind + lifetime extend)
```

### Trap 4 — type mismatch pe chhupa hua conversion
```cpp
double d = 3.0;
const int& r = d;   // ⚠️ compile ho jaata hai! r ek CHHUPI temporary int ko bind hota hai
                    //    (d se bani copy). d badlo -> r nahi badlta. Aksar bug.
```

### Trap 5 — `auto` reference-ness gira deta hai
```cpp
int& getRef();
auto  a = getRef();   // a ek int hai (COPY) -- '&' auto se nahi aata
auto& b = getRef();   // b ek int& hai (alias)  -- '&' khud likhna padta hai
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Reference ek naya variable banati hai" | Alias — same object, zero naya storage (usually) |
| "`r = y` reference ko y pe le jaata hai" | `referent = y` — copy. Reference kabhi rebind nahi |
| "Reference null ho sakti hai" | Nahi. Hamesha kisi object se bandhi maani jaati |
| "`&` reference decl mein address-of hai" | Nahi — type ka hissa (`int&` = "reference to int") |
| "`auto x = ref;` alias deta hai" | Copy. Alias ke liye `auto&` |

---

## Exercises

1. **Alias proof:** `int x = 5; int& r = x;` — `&x`, `&r` print karo. Same?
   `r = 100;` ke baad `x`? `x = 7;` ke baad `r`?

   <details><summary>Answer</summary>

   `&x == &r` (bilkul same address). `r = 100` → `x` ab 100. `x = 7` → `r` ab 7.
   Ek hi memory, do naam.
   </details>

2. **Rebind test:** `int a = 1, b = 2; int& r = a; r = b; b = 50;` — ab `a`, `b`,
   `r` kya? `&r` kiske barabar?

   <details><summary>Answer</summary>

   `r = b` ne `a = 2` kiya. Phir `b = 50` sirf `b` badalta hai (`r` `a` ka alias
   hai, `b` ka nahi). To `a == 2`, `b == 50`, `r == 2`, `&r == &a`.
   </details>

3. **Compile karega?** har line alag:
   ```cpp
   int& a;
   int& b = 10;
   const int& c = 10;
   int x = 1; int& d = x;
   double y = 2.0; int& e = y;
   double z = 2.0; const int& f = z;
   ```

   <details><summary>Answer</summary>

   `a` ❌ (uninit). `b` ❌ (non-const ref ← literal). `c` ✅ (const ref ← literal,
   lifetime extend). `d` ✅. `e` ❌ (non-const ref ← different type). `f` ✅ par
   trap — `f` ek chhupi temporary `int` (value 2) ko bind karta hai; `z` badlo to
   `f` nahi badlega.
   </details>

4. **`auto` ka `&`:** `int v = 9; int& ref = v;` — `auto a = ref; a = 100;` ke
   baad `v`? Ab `auto& b = ref; b = 100;` ke baad `v`?

   <details><summary>Answer</summary>

   `auto a = ref;` → `a` ek `int` copy hai; `a = 100` `v` ko nahi chhoota (`v == 9`).
   `auto& b = ref;` → `b` ek `int&` alias hai; `b = 100` → `v == 100`.
   </details>

5. **Swap bina reference:** sirf copies use karke do `int` swap karne wali
   function likho — call ke baad caller ke variables swap **nahi** honge. Kyun?
   Phir `int&` params se sahi karo.

   <details><summary>Answer</summary>

   By-value function apni copies swap karti hai; caller ke originals untouched.
   `void swap(int& a, int& b){ int t=a; a=b; b=t; }` — ab `a`,`b` caller ke
   objects ke alias hain, to real swap hota hai.
   </details>

---

## Interview questions

1. Reference kya hai? Pointer se conceptually kaise alag?
2. Reference ke 3 rules (init, rebind, null) bata.
3. `int& r = x; r = y;` — `x`, `r`, `&r` ka kya hua? "Rebind" kyun nahi?
4. Reference ko storage milta hai? "Depends" — kis case mein haan, kis mein nahi?
5. `auto` reference-ness kyun gira deta hai? `auto&` ka kaam?
6. `const int& r = someDouble;` compile kyun ho jaata hai, aur yeh trap kyun hai?

---

## Next
→ [`02-reference-vs-pointer.md`](02-reference-vs-pointer.md)

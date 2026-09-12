# 03 — Access specifiers — `public` / `private` / `protected`

## Prerequisites
- [`02-members-and-methods.md`](02-members-and-methods.md)
- Folder 11 file 12 (`struct` vs `class` default access)

## Yeh topic abhi kyun
Encapsulation ka mechanism yahi hai: **kaun kya access kar sakta hai**. Teen
levels — `public`, `private`, `protected`. Sahi choice se class ka interface
saaf rehta hai aur implementation aazad. Galat choice se ya to koi encapsulation
nahi, ya class use karna namumkin.

---

## Teen levels

```cpp
class Widget {
public:
    // Kahin se bhi accessible -- yeh class ka INTERFACE hai
    void doTask();
    int  result() const;

protected:
    // Sirf is class ke andar + DERIVED classes ke andar (folder 16)
    int state_;
    void helper();

private:
    // Sirf is class ke apne methods (aur friends -- file 10) ke andar
    int  cache_;
    void recompute();
};
```

| Specifier | Kaun access kar sakta |
|---|---|
| `public` | koi bhi — class ke bahar, derived classes, world |
| `protected` | class khud + derived classes (folder 16); bahar se nahi |
| `private` | sirf class khud + `friend`s (file 10); derived se bhi nahi |

**Default:** `class` → `private`, `struct` → `public` (folder 11 file 12). Bas
yehi ek fark hai dono keywords mein.

---

## Access blocks — order aur repeat

```cpp
class C {
    int a_;             // private (class default)

public:
    void f();
    int  g() const;

private:
    int b_;
    void helper();

public:                 // ✅ dobara public -- allowed, kitni baar bhi
    void h();
};
```

- Ek specifier agle tak sab members pe lagta hai.
- Repeat kar sakte ho. Common style: `public` pehle (interface reader ke liye
  upar), phir `private` (implementation neeche). Ya reverse — team convention.
- **Access order layout ko affect nahi karta** aksar — par standard-layout ke
  liye saare non-static data members ka same access hona chahiye (file 13).

---

## Kya `private` banao, kya `public`

**`public` (interface):**
- Woh methods jinse class ka kaam hota hai (`deposit`, `withdraw`, `bestBid`).
- Woh cheezein jo caller ko genuinely chahiye.
- Types / constants jo API ka hissa hain.

**`private` (implementation):**
- **Saare data members** (default) — taaki invariant control mein rahe.
- Helper methods jo sirf internal.
- Cache / bookkeeping.

**`protected`:**
- Sirf tab jab aap ek base class design kar rahe ho aur derived classes ko
  controlled access chahiye (folder 16). Warna avoid — `protected` data
  encapsulation ko lagbhag `public` jitna hi kamzor karta (koi bhi derive karke
  pahunch sakta).

```cpp
// ✅ achha
class Account {
    long long balance_;              // private -- invariant
public:
    bool withdraw(long long c);      // public -- controlled
    long long balance() const;
};

// ❌ kamzor
class Account {
public:
    long long balance;              // public data -- struct jaisa, invariant nahi
};
```

---

## Access ek COMPILE-TIME check hai (per-class, not per-object)

```cpp
class Secret {
    int value_ = 42;
public:
    // ek Secret doosre Secret ka private access kar sakta -- access class-level hai
    bool sameAs(const Secret& other) const {
        return value_ == other.value_;      // ✅ other.value_ -- private, par same class
    }
};
```

Access **type ke level pe** hai, object ke nahi. Ek class apni hi kism ke
doosre objects ke private members dekh sakti hai (copy ctors, `operator==` isi
par depend karte hain).

Aur yeh **compile-time only** hai:
```cpp
Secret s;
int* p = reinterpret_cast<int*>(&s);
*p = 999;                                    // ⚠️ "works" -- access control runtime pe enforce nahi hota
```
`private` security nahi hai — woh accidental misuse rokta hai, malicious nahi.

---

## `#define private public` — mat karna, par jaan lo

Kuch log tests mein `#define private public` likhte hain private ko poke karne
ke liye. Yeh **UB** hai (ODR violation — class ka layout/mangling badal sakta),
aur brittle. Better: `friend` test class (file 10), ya testable design.

---

## Andar kya hota hai

- Access specifiers **zero runtime cost** — compiler har member access pe ek
  check karta hai ("kya yeh context is member ko dekh sakta?"), aur pass hone pe
  bilkul wahi code emit karta jo `public` hone par hota.
- Object ka **layout** access se independent (mostly): `class { private: int a;
  int b; }` aur `struct { int a, b; }` ka same layout. **Par:** agar members ke
  access alag-alag hain (`int a;` private, `int b;` public), to woh
  **standard-layout nahi** raha (file 13) — `offsetof` / `memcpy`-based tricks
  pe asar, aur members ka relative order compiler decide kar sakta.
- Name mangling access se affect nahi hota — `Widget::helper()` ka symbol same
  chahe `private` ho ya `public`.

> **HFT relevance:** encapsulation zero-cost hai, to invariant-bearing hot types
> (`OrderBook`, `Session`, `RiskLimits`) confidently `private` data + tight
> `public` interface se banaye jaate hain. **Standard-layout** banaye rakhne ke
> liye (wire structs, `memcpy`, `static_assert(offsetof...)` — folder 11), un
> types ke saare data members **same access** mein rakho (aksar sab `public` in
> a `struct`). To: rules-heavy internal type → `class` with all-private data;
> transparent wire/pod type → `struct` with all-public data. Mixed access sirf
> tab jab standard-layout ki zaroorat na ho.

---

## Hands-on

```bash
./build.ps1 15-CLASSES/examples/01_first_class.cpp
```

`BankAccount` ke private members ko bahar se access karne ki koshish karo
(uncomment) — compile error. Phir `sameAs`-style method add karke dekho ki ek
object doosre ka private dekh leta hai.

---

## ⚠️ Traps

### Trap 1 — sab `public` (encapsulation skip)
```cpp
class Order { public: int qty; double price; bool active; };   // ⚠️ struct hi bana lo, ya invariant daalo
```

### Trap 2 — `protected` data
```cpp
class Base { protected: int state_; };   // ⚠️ koi bhi derive karke state_ tod sakta. private + protected accessor
```

### Trap 3 — `private` ko security samajhna
```cpp
// reinterpret_cast / offset hacks se private "read" ho sakta -- accidental misuse rokta, malicious nahi
```

### Trap 4 — mixed access → standard-layout toota
```cpp
class Msg { public: int a; private: int b; };   // ⚠️ not standard-layout -> memcpy/offsetof assumptions fail
```

### Trap 5 — `#define private public` in tests
UB, ODR violation. `friend` ya testable design use karo.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`private` runtime pe enforce hota" | Compile-time check only; casts se bypass ho sakta |
| "Ek object doosre same-class object ka private nahi dekh sakta" | Dekh sakta — access class-level hai |
| "`protected` `private` jaisa safe" | Kamzor — koi bhi derive karke pahunch jaata |
| "Access specifier order layout badalta" | Zyada tar nahi; par mixed access = not standard-layout |
| "`public` data thoda tez" | Zero difference — access check compile-time |

---

## Exercises

1. **Access quiz:** `class C { int a_; protected: int b_; public: int c_; };` —
   `main` mein `C x;` — `x.a_`, `x.b_`, `x.c_` mein kaunsa compile hoga?

   <details><summary>Answer</summary>

   Sirf `x.c_` (public). `a_` private, `b_` protected — dono `main` se
   inaccessible.
   </details>

2. **Same-class access:** `class Box { int v_; public: Box(int v):v_(v){} bool
   biggerThan(const Box& o) const { return v_ > o.v_; } };` — `o.v_` private
   hai, phir bhi compile hota hai. Kyun?

   <details><summary>Answer</summary>

   Access type-level hai. `biggerThan` `Box` ka member hai → kisi bhi `Box`
   (including `o`) ke private members dekh sakta.
   </details>

3. **Standard-layout break:** `struct M1 { int a, b, c; };` vs `struct M2 {
   public: int a; private: int b; public: int c; };` — kaunsa standard-layout?
   `std::is_standard_layout_v` se check. `offsetof` dono pe safe?

   <details><summary>Answer</summary>

   `M1` standard-layout. `M2` **nahi** (non-static data members ka access same
   nahi). `offsetof` on `M2` → `-Winvalid-offsetof`, aur order compiler decide
   kar sakta.
   </details>

4. **Refactor to encapsulate:** `struct Clock { long long ns; };` jahan code
   `clk.ns += delta;` (delta negative bhi ho sakta → time peeche!) karta hai.
   `class` bana ke sirf `advance(long long positiveDelta)` allow karo.

   <details><summary>Answer</summary>

   `class Clock { long long ns_ = 0; public: void advance(long long d) { if (d >
   0) ns_ += d; } long long now() const { return ns_; } };` — time monotonic
   guaranteed.
   </details>

5. **Interface vs impl:** ek `class RingBuffer` design karo (sirf declarations) —
   kaunse members `public` (interface), kaunse `private` (buffer, head, tail,
   capacity)?

   <details><summary>Answer</summary>

   `public`: `push(x)`, `bool pop(x&)`, `size()`, `empty()`, `full()`, ctor with
   capacity. `private`: `std::vector<T> buf_` (ya raw array), `size_t head_,
   tail_, count_, cap_`. Caller ko sirf push/pop chahiye, indices nahi.
   </details>

---

## Interview questions

1. `public` / `private` / `protected` — kaun kya access karta?
2. `class` vs `struct` default access?
3. Access control runtime pe enforce hota hai? Bypass ho sakta?
4. Ek object doosre same-class object ka private access kar sakta — kyun?
5. `protected` data kyun aksar bura idea?
6. Mixed access aur standard-layout ka relation?

---

## Next
→ [`04-constructors.md`](04-constructors.md)

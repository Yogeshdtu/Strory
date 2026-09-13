# 08 — Unions

## Prerequisites
- [`05-padding-and-alignment.md`](05-padding-and-alignment.md), [`06-packed-structs.md`](06-packed-structs.md)
- `05-OPERATORS/05-bitwise-operators.md` (bit views), `10-STRINGS/01-c-strings.md`

## Yeh topic abhi kyun
`union` ke saare members **ek hi memory** pe rehte hain. Ek waqt mein ek hi
member valid. Yeh memory-tight variants, wire-protocol alternatives, aur low-level
bit views ke liye hai — par C++ mein iske sakht rules hain, aur galat member
padhna UB. Modern code: `std::variant` (file 09).

---

## Basic

```cpp
union Value {
    std::int32_t i;
    float        f;
    char         bytes[4];
};

sizeof(Value)      // 4  -- SABSE BADA member (sab ek doosre ke upar)
alignof(Value)     // 4  -- sabse badi member alignment
```

```
   Value v:
   ┌────┬────┬────┬────┐
   │           4 bytes  │   -- i, f, bytes[] SAB yahi 4 bytes hain
   └────┴────┴────┴────┘
```

```cpp
Value v;
v.i = 0x41424344;      // ab `i` active hai
v.bytes[0];            // 0x44 (little-endian) -- WAHI memory, char ki nazar se
v.f = 1.0f;            // ab `f` active -- purani `i` value gayi
```

Analogy: struct = teen alag daraaz (har member ki apni jagah). Union = ek hi daraaz jis pe teen label chipke
hain — jo aakhri baar rakha, wahi andar hai.

---

## 🔑 Active member ka niyam

C++ mein: **sirf "aakhri baar likha gaya" member padho.** Koi aur member padhna = technically **UB**
(C mein yeh allowed hai — "type punning").

```cpp
Value v;
v.f = 1.0f;
std::int32_t bits = v.i;    // ⚠️ C++ mein UB (`f` likha, `i` padh rahe ho)
```

GCC practically isse allow karta hai (union ke through type punning ko documented extension ki tarah
treat karta hai), par portable / standard code isse bachta hai.

### Safe type punning: `memcpy` ya `std::bit_cast`

```cpp
float src = 1.0f;
std::uint32_t bits;
std::memcpy(&bits, &src, sizeof(bits));           // ✅ hamesha well-defined

std::uint32_t bits2 = std::bit_cast<std::uint32_t>(src);   // ✅ C++20 -- wahi kaam, constexpr-friendly
```

**Bits ko dobara interpret karne ke liye `memcpy` / `bit_cast` use karo, union nahi.** Speed ki chinta mat
karo: GCC 16.2 `-O2` pe `std::bit_cast<std::uint32_t>(float)` ek instruction ban gaya — `movd eax, xmm0`.

---

## Tagged union — active member ka hisaab rakho

Raw union ko pata nahi kaunsa member active hai. Aap ek `enum` tag se track karte ho (yahi kaam `std::variant`
aapke liye karta hai):

```cpp
struct TaggedValue {
    enum class Tag { Int, Float, Text } tag;
    union {
        std::int64_t i;
        double       d;
        char         text[8];
    };
};                          // sizeof 16: tag (4) + padding (4) + union (8)

void print(const TaggedValue& t) {
    switch (t.tag) {
        case TaggedValue::Tag::Int:   /* t.i padho */   break;
        case TaggedValue::Tag::Float: /* t.d padho */   break;
        case TaggedValue::Tag::Text:  /* t.text padho */break;
    }
}
```

`examples/05_unions.cpp` — shared memory, type punning, tagged union.

---

## Non-trivial members — lifetime aap sambhalo

```cpp
union U {
    int i;
    std::string s;        // ⚠️ non-trivial -- iska constructor/destructor hai
    U() : i(0) {}         // ab union ko khud ka ctor chahiye
    ~U() {}               // ...aur dtor -- par destroy kisko karein? AAP tay karo
};
```

Jab union mein non-trivial ctor/dtor wala member ho (`std::string`, `std::vector`), union ka apna default
ctor/dtor **deleted** ho jaata hai jab tak aap khud na likho. Bina likhe `U u;` compile hi nahi hota
(`error: use of deleted function 'UStr::UStr()'` — GCC 16.2). Likh diya to active member ko **haath se**
`placement new` karna padta hai aur uska destructor explicitly bulana padta hai. Galti karna bahut aasaan.

**→ `std::variant` use karo.** Woh yeh sab sahi karta hai.

---

## Anonymous unions

```cpp
struct Packet {
    std::uint8_t type;
    union {                        // naam nahi -- members seedhe Packet pe milte hain
        std::uint32_t asInt;
        float         asFloat;
        std::uint8_t  asBytes[4];
    };
};

Packet p;
p.asInt = 42;                     // p.asInt, p.someUnionName.asInt nahi
```

Wire structs mein aam hai jahan field ka matlab `type` pe depend karta hai.

---

## Raw union kab use karo

**Haan:**
- Fixed **wire/file formats** jahan ek field kai matlabon mein se ek hai, type byte se chuna jaata hai (packed
  struct mein anonymous union).
- **Memory-critical** embedded code.
- `memcpy`/`bit_cast` semantics document karke (waise `bit_cast` zyada saaf hai).

**Nahi (`std::variant` use karo):**
- Application code mein general "inmein se ek type" value.
- Non-trivial members wala kuch bhi.
- Jahan bhi aap haath se tagged union banane wale ho.

---

## Andar kya hota hai

- Union ka `sizeof` = sabse bada member (alignment tak round up), `alignof` = sabse badi member alignment, saare
  members offset 0 pe.
- Ek member likho, doosra padho → compiler wahi bytes padhta hai; standard ise UB kehta hai (optimizer maan
  *sakta* hai ki aisa nahi hota), par GCC union ke through punning ko allow karta hai.
- `std::bit_cast<To>(from)` → compiler `memcpy` banata hai, jo `-O2` pe register move ban jaata hai (`movd`) —
  asli copy nahi.
- `std::string` wala union → string ka pointer/length/buffer baaki members ke upar overlap karta hai; galat member
  construct/destruct kiya to heap corrupt.

> **HFT relevance:** Packed wire structs mein anonymous unions dikhte hain — jaise ek price field jo flag ke
> hisaab se `int64` mantissa ya IEEE double hai. Decode `memcpy` se (file 06). Internal "event ya to Quote hai
> ya Trade ya Reject" types ke liye HFT code `std::variant` (file 09) ya plain-old-data payload wala haath se bana
> tagged union (andar `std::string` nahi) use karta hai — discriminant + `switch` ek sasta branch ban jaata hai,
> virtual dispatch se sasta. Bit-level reinterpretation (fast math ke liye float↔bits) `std::bit_cast` se —
> ek `movd`, zero overhead.

---

## Hands-on

`examples/05_unions.cpp` — shared memory, type punning (+ safe `memcpy` tareeqa), tagged union:

```bash
./build.ps1 11-STRUCTS/examples/05_unions.cpp
```

---

## ⚠️ Traps

### Trap 1 — inactive member padhna
```cpp
u.f = 1.0f;  int b = u.i;   // ⚠️ UB (standard). memcpy / bit_cast
```

### Trap 2 — `std::string` wala union, lifetime haath se nahi sambhali
```cpp
union U { int i; std::string s; U() : i(0) {} ~U() {} };
U u; u.s = "x";   // ⚠️ compile ho jaata hai (koi warning nahi), par s kabhi construct hi nahi hua -> UB
```
(Ctor/dtor na likhte to `U u;` hi compile error — deleted function.)

### Trap 3 — tag bhoolna
```cpp
TaggedValue t;  t.d = 1.0;  /* t.tag = Float bhool gaye */  print(t);   // galat member padha
```

### Trap 4 — `sizeof(union)` = members ka jod
```cpp
union U { char a[10]; int b; };  sizeof(U) == 12   // max(10, 4), align 4 tak round up -- 14 NAHI
```

### Trap 5 — jahan `std::variant` fit ho wahan union
Application-level "inmein se ek" types → `std::variant`. Raw union → sirf wire/embedded.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Koi bhi union member padhna theek hai (type punning)" | C++ mein UB — `memcpy` / `bit_cast` |
| "`sizeof(union)` = members ka jod" | Sabse bada member (aligned) |
| "Union yaad rakhta hai kaunsa member active hai" | Nahi — aap track karo (tag), ya `std::variant` |
| "`std::string` wala union bas chal jaata hai" | Ctor/dtor khud likho + haath se construct/destruct |
| "Variant types ka tareeqa union hai" | App code ke liye `std::variant`; union wire/embedded ke liye |
| "`memcpy`/`bit_cast` union se slow" | `-O2` pe ek `movd` — koi fark nahi |

---

## Exercises

1. **Bit view:** `union { float f; std::uint32_t bits; };` — `f = 3.14f` set karo, `bits` hex mein print karo.
   Phir `std::bit_cast` se wahi karo. Same result?
   <details><summary>Answer</summary>

   Dono `0x4048f5c3` (GCC 16.2 pe chala ke). Union wala GCC pe chalta hai par standard UB hai; `bit_cast` hamesha sahi.
   </details>

2. **Sizeof:** `union U { double d; char c[10]; std::int16_t s; };` — `sizeof(U)`, `alignof(U)`. Samjhao.
   <details><summary>Answer</summary>

   `sizeof` **16**, `alignof` **8**. Sabse bada member `c[10]` (10 bytes), par sabse badi alignment `double` ki (8)
   → 10 ko 8 ke agle multiple tak round up = 16.
   </details>

3. **Tagged union:** `TaggedNumber` jo tag ke saath `int` / `double` / `bool` rakhe. `add(TaggedNumber,
   TaggedNumber)` jo samajhdari se promote kare.

4. **Packed struct mein anonymous union:** `struct Field { uint8_t kind; union { int32_t i; float f; char s[4]; }; };`
   (packed) — 3 sample byte buffers decode karo.

5. **`std::string` union ka khatra:** `union U { int i; std::string s; };` ko manual `placement new` / explicit
   `s.~basic_string()` ke saath likho. Sahi karke dikhao. Phir dekho `std::variant` yeh muft mein kaise karta hai.

6. **Punning UB:** `float f = 1.0f; int i = *(int*)&f;` — `-O0 -Wall` aur `-O2 -Wall` dono se build karo. Kya
   hua? `memcpy` se fix karo.
   <details><summary>Answer (GCC 16.2, MinGW)</summary>

   `-O0`: koi warning nahi. `-O2 -Wall`: `warning: dereferencing type-punned pointer will break strict-aliasing
   rules [-Wstrict-aliasing]` (strict aliasing `-O2` pe hi on hota hai). Is chhote case mein value phir bhi sahi
   aayi — UB ka matlab "kabhi bhi kuch bhi", "abhi crash" nahi. Note: `-fsanitize=undefined` MinGW pe link hi nahi
   hota (`cannot find -lubsan`), aur UBSan strict-aliasing check karta bhi nahi — warning hi aapka signal hai.
   </details>

---

## Interview questions

1. Union ke members memory mein kaise? `sizeof` / `alignof`?
2. "Active member" niyam — inactive member padhna kya hai (C vs C++)?
3. Safe type punning kaise (union nahi to kya)?
4. Non-trivial member (`std::string`) wale union mein kya problem?
5. Tagged union kya hai? `std::variant` se relation?
6. Raw union kab use karo, kab `std::variant`?

---

## Next
→ [`09-std-variant.md`](09-std-variant.md)

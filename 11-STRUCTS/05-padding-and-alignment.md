# 05 — Padding aur alignment — deep dive

## Prerequisites
- [`01-what-is-a-struct.md`](01-what-is-a-struct.md), [`03-nested-structs.md`](03-nested-structs.md)
- `03-VARIABLES-DATA-TYPES/05-int-deep-dive.md` (bytes), `09-ARRAYS/10-array-performance.md` (cache lines)

## Yeh topic abhi kyun
**Yeh folder ka sabse important lesson hai.** Compiler struct members ke beech
**gaps (padding)** daalta hai taaki har member "aligned" ho. Result: `sizeof`
members ke jod se bada hota hai — aur **member order badalne se struct ka size
aadha** ho sakta hai. HFT mein yeh cache efficiency ka sabse bada single lever
hai.

---

## Alignment — har type ko ek "natural" boundary chahiye

```cpp
alignof(char)        // 1  -- kisi bhi address pe
alignof(std::int16_t)// 2  -- even address
alignof(std::int32_t)// 4  -- 4 ka multiple
alignof(std::int64_t)// 8
alignof(double)      // 8
alignof(void*)       // 8
```

**Rule: `T` ka object aise address pe hona chahiye jo `alignof(T)` ka multiple ho.**

Analogy: parking lot mein car sirf un khaano mein khadi ho sakti hai jo har 8 metre pe bane hain.
Scooter (char) kahin bhi khada ho sakta hai. Agar scooter ke baad car khadi karni hai, to beech ki
khaali jagah chhodni padti hai — wahi padding hai.

Galat alignment pe kya hota hai: C++ ke hisaab se misaligned object access **UB** hai. Practically, x86
pe scalar `int`/`double` ka misaligned load aam taur pe chal jaata hai; par **aligned SIMD instructions**
(`movaps` jaise) crash karti hain, kuch purane ARM cores fault dete hain, aur compiler "alignment sahi
hai" maan ke optimize karta hai. Isliye compiler padding daal ke alignment ki guarantee deta hai.

---

## Padding — compiler khaali jagah bharta hai

```cpp
struct Bad {
    char         a;    // offset 0        (1 byte)
    // >>> 7 bytes PADDING  (taaki `b` 8 ke multiple pe shuru ho)
    double       b;    // offset 8        (8 bytes)
    char         c;    // offset 16       (1 byte)
    // >>> 3 bytes PADDING  (taaki `d` 4 ke multiple pe shuru ho)
    std::int32_t d;    // offset 20       (4 bytes)
    // kul 24 bytes  (14 kaam ke, 10 padding)
};
```

Do niyam:
1. **Har member** aise offset pe shuru hota hai jo uske `alignof` ka multiple ho.
   → kam-aligned member ke *pehle* padding.
2. **Struct ka size** struct ke `alignof` (= sabse bade member ki alignment) ka multiple hota hai.
   → *peeche* (tail) padding.

`sizeof(Bad)` = **24**, 13 nahi. `alignof(Bad)` = 8 (sabse bada member).

`examples/02_padding_demo.cpp` yeh offsets print karta hai.

---

## Member reordering — wahi data, chhota struct

```cpp
struct Good {               // Bad ke WAHI members, alignment ke UTARTE order mein
    double       b;    // offset 0
    std::int32_t d;    // offset 8
    char         a;    // offset 12
    char         c;    // offset 13
    // >>> 2 bytes tail padding (size 8 ka multiple ho jaaye)
    // kul 16 bytes
};
```

`sizeof(Good)` = **16** vs `sizeof(Bad)` = 24 — **8 bytes (33%) bache**, behaviour mein zero farq.

### Aasaan rule: **members alignment ke utarte (descending) order mein**

```
pointers / double / int64_t   (align 8)
   ↓
int32_t / float               (align 4)
   ↓
int16_t / short               (align 2)
   ↓
char / bool / int8_t          (align 1)
```

`examples/03_struct_optimization.cpp` — ek asli jaisa `Level` struct: **bura order 40 bytes (45%
padding), achha order 24 bytes (8%)** (GCC 16.2 pe dobara chalaya — wahi numbers). Yaani 1.67x ghana → book
scan karne ke liye 1.67x kam cache lines.

---

## `alignof` / `alignas`

```cpp
alignof(T)                          // pucho: T ki alignment requirement

struct alignas(64) CacheLineAligned {   // 64-byte alignment zabardasti (poori cache line)
    std::atomic<long> counter;
    char pad[64 - sizeof(std::atomic<long>)];   // (ya compiler ko tail-pad karne do)
};

alignas(32) float simdRow[8];       // AVX loads ke liye 32-byte aligned
```

**Zyada align** (`alignas(64)`) karo taaki:
- Garam struct apni alag cache line pe rahe → **false sharing** se bachav (do threads ek hi line ke alag
  fields likhein → cache line idhar-udhar uchhalti hai, folder 28).
- Aligned SIMD loads use ho sakein.

**Kam align** — natural requirement se neeche nahi ja sakte (woh kaam `#pragma pack` karta hai, file 06,
trade-offs ke saath). ⚠️ Aur dhyaan: `struct alignas(1) S { double d; };` standard ke hisaab se
ill-formed hai, par **GCC 16.2 koi error ya warning nahi deta — chupchaap ignore karta hai**
(`alignof(S)` 8 hi raha, chala ke dekha). Compiler ki khamoshi ko "chal gaya" mat samjho.

---

## Padding dhoondhna

```cpp
// 1. static_assert -- size lock karo
static_assert(sizeof(Level) == 24, "Level layout changed!");

// 2. offsetof -- har member ka byte offset dekho
std::cout << offsetof(Level, qty);

// 3. -Wpadded -- compiler padding ki warning deta hai (neeche MinGW wala dhyaan!)
//    g++ -Wpadded file.cpp

// 4. `pahole` (Linux, dwarves package) -- poora layout holes ke saath print karta hai
//    pahole -C Level ./binary
```

### ⚠️ Is toolchain pe `-Wpadded` beech ki padding NAHI dikhata — chala ke pakda
Upar wala `Bad` struct, GCC 16.2 (MinGW), `-Wpadded`:

| Flags | Kya warning aayi |
|---|---|
| `-Wpadded` (default) | sirf tail padding: `padding struct size to alignment boundary with N bytes` — `a` ke baad ke 7 bytes aur `c` ke baad ke 3 bytes pe **kuch nahi** |
| `-Wpadded -mno-ms-bitfields` | `padding struct to align 'Bad::b'`, `padding struct to align 'Bad::d'` — beech ki padding bhi |

Wajah: MinGW GCC Microsoft-compatible struct layout ke liye default mein `-mms-bitfields` on rakhta hai,
aur us mode mein `-Wpadded` beech ki padding report nahi karta. Linux GCC pe yeh flag default nahi hai.

Practical rule: **is machine pe layout audit ke liye `offsetof` + `static_assert` pe bharosa karo**; ek
baar ki `-Wpadded` jaanch karni ho to `-mno-ms-bitfields` ke saath sirf audit ke liye chalao — us flag se
production build mat banao (woh bitfield layout / ABI badal deta hai).

⚠️ `-Wpadded` shor bhi bahut karta hai (thodi si padding wale har struct pe warn) — permanent flag
nahi, ek baar ki audit ke liye.

---

## Jab order badal hi NAHI sakte

Wire formats / ABI / API structs ka member order **fixed** hota hai (protocol tay karta hai). Tab:
- **Explicit padding fields**: `char _reserved[3];` — khaali jagah ko likh ke batao.
- `#pragma pack` se padding poori hatao (file 06) — unaligned access ke trade-off ke saath.
- Wire ke liye ek alag "layout struct" + andar use ke liye achhe order wala struct, aur dono ke beech convert.

---

## Andar kya hota hai

- Compiler har member ka offset nikaalta hai: `offset = round_up(pichhle_member_ka_end, alignof(member))`.
  Beech ka gap padding hai (uske bytes indeterminate — aksar 0, bharosa mat karo).
- Struct ka `sizeof` = `round_up(aakhri_member_ka_end, alignof(struct))`.
- Nested struct → apna `sizeof` aur apna `alignof` laata hai.
- Struct ka array → `stride = sizeof(struct)` (jisme tail padding pehle se hai, isliye `arr[i]` aligned
  rehta hai).
- Padded struct ke raw bytes padho to gaps dikhte hain; do "barabar" structs ka `memcmp` padding mein alag
  aa sakta hai → **struct equality ke liye `memcmp` mat karo**.

> **HFT relevance:** Struct ka size seedha tay karta hai ki 64-byte cache line aur L1/L2 mein kitne records
> aayenge. `Level` ko 40 → 24 bytes karne se 32-level book side 1280 → 768 bytes ki ho jaati hai — L1 se bahar
> bikharna ya na bikharna. HFT code review struct layout check karta hai; CI mein
> `static_assert(sizeof(...))` use bachata hai; garam shared structs `alignas(64)` hote hain false sharing
> maarne ke liye. Aur jis toolchain pe audit kar rahe ho, uske tools ki seema jaano — MinGW pe `-Wpadded`
> beech ki padding miss karta hai. Folders 28, 32, 39.

---

## Hands-on

```bash
./build.ps1 11-STRUCTS/examples/02_padding_demo.cpp
./build.ps1 11-STRUCTS/examples/03_struct_optimization.cpp
# -Wpadded audit -- MinGW pe beech ki padding dekhne ke liye -mno-ms-bitfields bhi (sirf audit):
g++ -std=c++20 -Wpadded -mno-ms-bitfields 11-STRUCTS/examples/02_padding_demo.cpp -o /dev/null
```

---

## ⚠️ Traps

### Trap 1 — `sizeof(struct)` == members ka jod
```cpp
struct M { char c; int i; };  // sizeof 8, 5 nahi
```

### Trap 2 — struct equality ke liye `memcmp`
```cpp
if (std::memcmp(&a, &b, sizeof(a)) == 0) { }   // ⚠️ padding bytes alag ho sakte hain. Member-wise ==
```

### Trap 3 — mile-jule alignment wala member order
```cpp
struct S { char a; double b; char c; int d; };   // ⚠️ 24 bytes. Reorder -> 16
```

### Trap 4 — natural se chhota `alignas`
```cpp
struct alignas(1) S { double d; };   // ⚠️ standard: ill-formed. GCC 16.2: chupchaap ignore, alignof 8 hi
```
Padding hatani hai to `#pragma pack` (file 06) — `alignas` kabhi alignment ghata nahi sakta.

### Trap 5 — padding bytes ki value pe bharosa
```cpp
struct S { char c; int i; };  S s{};  // `c` ke baad ki padding indeterminate, 0 ki guarantee nahi
```

### Trap 6 — MinGW pe `-Wpadded` ko poori audit samajhna
```bash
g++ -Wpadded file.cpp                     # ⚠️ sirf tail padding
g++ -Wpadded -mno-ms-bitfields file.cpp   # beech ki padding bhi (sirf audit ke liye)
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`sizeof` = members ke size ka jod" | + padding (alignment) |
| "Member order se size pe fark nahi" | Descending order padding kam karta hai — aksar 30–45% chhota |
| "`memcmp` structs ko sahi compare karta hai" | Padding bytes alag — member-wise `==` |
| "Padding bytes zero hote hain" | Indeterminate |
| "`alignas` alignment ghata sakta hai" | Sirf badha sakta hai; GCC chhote `alignas` ko chupchaap ignore karta hai |
| "`-Wpadded` har padding dikhata hai" | MinGW (`-mms-bitfields` default) pe sirf tail padding |
| "x86 pe misaligned access bas thoda slow hai" | C++ mein UB; aligned SIMD crash; scalar aksar chal jaata hai — bharosa mat karo |

---

## Exercises

1. **Haath se nikaalo:** `struct S { char a; int b; char c; double d; short e; };` — har offset, padding,
   aur `sizeof` likho. `offsetof` + `sizeof` se verify karo.
   <details><summary>Answer</summary>

   `a`=0, (3 padding), `b`=4, `c`=8, (7 padding), `d`=16, `e`=24, (6 tail padding) → `sizeof` = **32**
   (GCC 16.2 pe chala ke: offsets 0, 4, 8, 16, 24).
   </details>

2. **Reorder:** exercise 1 ke `S` ka `sizeof` reorder karke kam se kam karo. Kitne bytes bache?
   <details><summary>Answer</summary>

   `double d; int b; short e; char a; char c;` → 8 + 4 + 2 + 1 + 1 = 16, koi padding nahi → **16 bytes**,
   yaani 32 se 16 bytes bache.
   </details>

3. **`-Wpadded`:** `02_padding_demo.cpp` ko pehle `-Wpadded`, phir `-Wpadded -mno-ms-bitfields` se compile
   karo. Har warning ko output ke ek offset se milao. Pehle wale mein kya chhoot gaya?
   <details><summary>Answer (GCC 16.2, MinGW)</summary>

   Sirf `-Wpadded`: **1** warning — `padding struct size to alignment boundary with 2 bytes` (`Good` ki tail).
   `-mno-ms-bitfields` ke saath: **3** — `padding struct to align 'Bad::b'` (offset 1–7), `'Bad::d'`
   (offset 17–19), aur wahi tail wala. Pehle run mein `Bad` ki dono beech wali gaps chhoot gayi thi.
   </details>

4. **`static_assert` guard:** `struct Level { std::int64_t px, qty; std::int32_t n; char side; };` —
   `static_assert(sizeof(Level) == 24)`. Ab `side` ke baad ek `bool` jodo — ab bhi 24? Ek `double` jodo —
   assert fire hua?
   <details><summary>Answer</summary>

   Base 24; `bool` jodne pe bhi **24** (tail padding mein aa gaya); `double` jodne pe **32** → assert fire.
   (GCC 16.2 pe chala ke.)
   </details>

5. **`alignas` / false sharing:** ek struct mein do `std::atomic<long>` counters vs har ek apne
   `alignas(64)` struct mein. Do threads ek-ek counter 10M baar badhayein. `-O2`, time lo. (Folder 28 ki jhalak.)

6. **Nested alignment:** `struct Inner { double d; };  struct Outer { char c; Inner in; char c2; };` —
   `sizeof(Outer)`? Minimize karne ke liye reorder karo.

---

## Interview questions

1. Alignment kya hai? `alignof(double)` aur kyun?
2. Padding kyun aur kahan add hoti hai (2 rules)?
3. Member reordering se `sizeof` kaise ghatta hai — rule?
4. `alignas(64)` struct pe — 2 reasons (false sharing, SIMD)?
5. Struct equality ke liye `memcmp` kyun galat?
6. Wire-format struct jismein order fix ho — padding kaise handle karo?
7. `-Wpadded` / `pahole` — kya batate hain? Kis toolchain pe kya miss hota hai?

---

## Next
→ [`06-packed-structs.md`](06-packed-structs.md)

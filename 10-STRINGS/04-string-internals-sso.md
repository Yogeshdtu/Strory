# 04 — `std::string` internals: SSO

## Prerequisites
- [`02-std-string-basics.md`](02-std-string-basics.md)
- `08-FUNCTIONS/05-the-call-stack.md` (stack vs heap), `09-ARRAYS/10-array-performance.md`

## Yeh topic abhi kyun
`std::string` heap use karta hai — allocation, jo slow hai. **SSO (Small String
Optimization)** iska hal hai: chhoti strings `std::string` object ke **andar hi**
(inline) rehti hain — **koi allocation nahi**. HFT mein yeh ek core allocation-
avoidance mechanism hai, aur "`std::string` copy free hai kya?" ka jawab ismein hai.

Analogy: `std::string` ek batua (wallet) hai. Thode se note (chhoti string) batue ke andar hi aa
jaate hain. Bahut saare note (lambi string) ho to bank locker (heap) chahiye, aur batue mein sirf
locker ki chaabi (pointer) rehti hai.

---

## Layout (libstdc++, 64-bit)

```
sizeof(std::string) == 32 bytes:

  ┌──────────────────┬──────────────────┬────────────────────────────┐
  │ char*  _M_p      │ size_t _M_len    │ union { char buf[16];      │
  │  (8)             │  (8)             │         size_t capacity; } │
  │                 │                  │  (16)                      │
  └──────────────────┴──────────────────┴────────────────────────────┘
```

- **`_M_p`** — char data ka pointer
- **`_M_len`** — length (`size()` bas yahi lautata hai — O(1))
- **union**: ya to ek **16-byte inline buffer** (`buf[16]` = 15 chars + `'\0'`), YA (lambi strings
  ke liye) heap ki **capacity**

### Chhoti string (≤ 15 chars)
```
  _M_p ─────► _M_local_buf   (apne hi andar point karta hai)
  _M_len = 5
  _M_local_buf = "hello\0..."
```
**Data inline hai. Heap allocation zero.** Banana aur copy karna = bas 32-byte object ke andar bytes.

### Lambi string (≥ 16 chars)
```
  _M_p ─────► [heap block]  "this is a longer string\0"
  _M_len = 23
  _M_allocated_capacity = 30
```
`_M_p` ek heap allocation pe point karta hai. Banana/copy karna → `new` + `memcpy`.

---

## Threshold naapo — `examples/03_sso_demo.cpp`

Global `operator new` ko override karke ginte hain ki har length ki string banane mein kitne
allocations hue. **`-O0` pe chalao** (`-O2` pe GCC allocation hi hata deta hai — neeche dekho).

GCC 16.2, `-O0` pe asli output:
```
len | heap allocations while building a string of that length
----+-------------------------------------------------------
 14 | 0 alloc               <- inline (SSO)
 15 | 0 alloc               <- inline (SSO)
 16 | 1 alloc  (17 bytes)   <- HEAP
 17 | 1 alloc  (18 bytes)   <- HEAP
```

**libstdc++: threshold = 15 chars** (GCC 15.1 aur 16.2 dono pe wahi). (libc++: 22. MSVC: 15. Yeh
implementation-specific hai — exact number pe kabhi bharosa mat karo.)

### `-O2` pe allocation gayab
`-O2` pe ek chhoti zindagi wala local `std::string s(len, 'a')`, jiska sirf `s[0]` use hota hai →
GCC `operator new`/`operator delete` calls hata deta hai (C++14 ki **new-expression elision**
ki ijazat — wahi rule jisse `make_shared` allocations jod paata hai). Tab demo har length pe
"0 alloc" dikhata hai (16.2 pe bhi dekha: len 16, 17 pe `0 alloc`). Yeh ek asli optimization hai
jo jaanna chahiye (folder 33) — par SSO ki boundary **dekhne** ke liye `-O0` chahiye.

---

## Capacity growth — geometric (~2x)

`examples/03_sso_demo.cpp` ka append loop (libstdc++, GCC 16.2):

```
  cap 15 -> 30 -> 60 -> 120 -> 240 -> 480 -> ...   (har baar x2.00)
```

Har growth: bada naya block `new` + saara `memcpy` + purana `delete`. Bina `reserve` ke `n`
appends → ~`log₂(n)` reallocations, aur kul copy ka kaam amortized O(n). Naapa: 1,000,000 appends
pe **17 capacity badlaav** (final capacity 1,966,080). **`reserve(finalSize)` → theek ek
allocation** (file 07).

---

## Iska asar kya hai

### Copy ki cost length pe depend karti hai
```cpp
std::string a = "short";              // SSO
std::string b = a;                    // inline chars copy -- allocation NAHI

std::string c = "this is a long string here";   // heap
std::string d = c;                    // new + memcpy -- allocation
```

### Move dono case mein sasta hai
```cpp
std::string e = std::move(c);         // lambi: pointer churaao, c -> khaali
std::string f = std::move(a);         // chhoti: inline chars copy, a -> "" (SSO mein churaane ko pointer nahi)
```
(Is box pe moved-from short string ka `size()` = 0 aaya; standard sirf "valid but unspecified"
kehta hai.)

### Containers mein `std::string`
Chhoti strings ka `std::vector<std::string>` → har element ka char data us element ke andar hai,
aur element vector ke buffer ke andar → hairaan karne jitna cache-friendly. Lambi strings → har
element ka data alag heap block mein (pointer chase).

---

## SSO vs `std::string_view`

| | SSO wali chhoti `std::string` | `std::string_view` |
|---|---|---|
| Data ka maalik | ✅ (inline) | ❌ (udhaar) |
| Allocation | koi nahi (threshold ke andar) | kabhi nahi |
| Source se zyada jee sakta hai | ✅ (woh khud source hai) | ❌ (dangling ka khatra) |
| Size | 32 bytes | 16 bytes |
| Badal sakte ho | ✅ | ❌ |

**Chhoti owned key** chahiye → SSO `std::string` (safe, allocation nahi). Pehle se maujood data pe
**read-only view** chahiye → `std::string_view` (file 05).

---

## Andar kya hota hai

- `std::string s = "abc"` (chhoti): 3 chars + `'\0'` `_M_local_buf` mein `memcpy`, `_M_p =
  &_M_local_buf`, `_M_len = 3`. Koi `operator new` nahi.
- Chhoti string pe `s += "x"` jab tak fit ho: `_M_local_buf` mein likho, `_M_len` badhao.
- Threshold paar karna: `operator new`, inline se heap mein `memcpy`, `_M_p` badlo.
- `-O2` chhoti strings ko poori tarah registers mein rakh sakta hai / object hi hata sakta hai.

> **HFT relevance:** Chhote identifiers — instrument symbols ("AAPL", "ES"), venue codes, chhoti
> keys — SSO mein aa jaate hain → idhar-udhar pass karna, maps mein rakhna, copy karna, sab
> **allocation-free**. Isiliye chhoti keys ke liye latency-sensitive code mein bhi `std::string`
> (`char*` ki jagah) chal jaata hai. Jo bhi threshold paar kar sakta hai (log lines, JSON, order
> messages) woh `reserve`+reuse ya `std::string_view` se sambhala jaata hai. Apni keys ke size ke
> liye apni implementation ka threshold jaano.

---

## Hands-on

```bash
g++ -std=c++20 -O0 10-STRINGS/examples/03_sso_demo.cpp -o sso && ./sso
# phir -O2 -- dekho allocations gayab ho jaati hain (elision)
g++ -std=c++20 -O2 10-STRINGS/examples/03_sso_demo.cpp -o sso2 && ./sso2
```

---

## ⚠️ Traps

### Trap 1 — exact threshold pe bharosa
```cpp
static_assert(sizeof(std::string) == 32);   // ⚠️ implementation-specific
```

### Trap 2 — "`std::string` copy hamesha mehnga hai"
Chhoti strings ki copy mein allocation nahi hota. Cost length pe depend karti hai.

### Trap 3 — `-O2` pe SSO benchmark karna
Allocation elide ho jaata hai → dhokha. Dekhne ke liye `-O0`/`-O1`.

### Trap 4 — maan lena ki chhoti string ka `std::move` pointer churaata hai
SSO string: move = inline chars copy + source khaali. Churaane ko pointer hi nahi.

### Trap 5 — bina `reserve` ke loop mein append
```cpp
for (int i = 0; i < 1e6; ++i) s += 'x';   // ⚠️ 17 reallocations (GCC 16.2 pe gine). s.reserve(1e6)
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Har `std::string` allocate karta hai" | Chhoti strings (≤ ~15) inline SSO use karti hain |
| "SSO threshold har jagah 15 hai" | libc++: 22, MSVC: 15 — implementation-specific |
| "`std::string` copy ki cost fixed hai" | Length pe depend (SSO vs heap) |
| "`std::move` hamesha pointer churaana hai" | SSO strings ke chars copy hote hain |
| "SSO demo kisi bhi `-O` pe chalega" | `-O2` elide kar deta hai — `-O0`/`-O1` lo |

---

## Exercises

1. **Apna threshold dhoondho:** `03_sso_demo.cpp` ko `-O0` pe chalao. Kis length pe pehli heap
   allocation hui? Aapki machine pe `sizeof(std::string)`?

2. **Elision:** `-O2` pe chalao. Kya badla? Kyun (C++14 ki ijazat)?

3. **Copy cost:** `"short"` string vs 40-char string ki copy, 10M baar, `-O0` pe (taaki allocations
   elide na hon). Ratio?

4. **Capacity growth:** 1000 chars tak har `push_back` ke baad `capacity()` log karo. Growth factor?
   Kitni reallocations?

5. **`reserve`:** wahi append loop pehle `s.reserve(1000)` karke — ab kitni reallocations
   (`operator new` se gino)?

6. **Container locality:** 100000 chhoti vs lambi strings ka `std::vector<std::string>` — saare
   `.size()` jodo. `-O2`, time lo. Farq samjhao.

---

## Interview questions

1. SSO kya hai? `std::string` ke andar layout?
2. libstdc++ ka SSO threshold? Yeh portable hai?
3. Short vs long `std::string` — copy aur move ki cost?
4. Capacity growth pattern? `reserve` kyun?
5. SSO string ka `std::move` — kya hota hai?
6. `-O2` pe SSO benchmark misleading kyun ho sakta hai (elision)?

---

## Next
→ [`05-string-view.md`](05-string-view.md)

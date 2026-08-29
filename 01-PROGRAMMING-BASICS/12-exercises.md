# 12 — Phase 0 Revision + Exercises

## Prerequisites
Is folder ke saare lessons (01–11)

## Yeh file kyun
Aage badhne se pehle check karo ki foundation pakki hai. Agar yahan atke, wapas
jaake padho — sharam ki koi baat nahi. Yeh nuksaan nahi, investment hai.

---

## PART A — Concept check (bina dekhe jawab do)

Har sawal ka jawab **apne shabdon mein, likhkar** do. Sirf sochkar mat chhodo.

### Computer basics
1. Computer ke 4 main hisse kaunse hain?
2. RAM aur SSD mein 3 fark batao.
3. CPU ek second mein kitni cycles chalati hai (roughly)?
4. Operating System ka kaam kya hai? 3 points.
5. Syscall kya hai? Woh mehnga kyun hai?

### Programming
6. Algorithm aur program mein kya fark hai?
7. Programming ke 5 basic building blocks kaunse hain?
8. Compile-time error aur runtime error mein fark batao, ek-ek example do.

### Languages
9. Machine code, assembly, aur high-level language — teenon ka fark.
10. "Abstraction" ka matlab kya hai?
11. C++ ki 3 khaas baatein batao.
12. HFT mein Python kyun use nahi hoti hot path mein? 3 wajah.

### Compilation
13. Preprocessor ke 3 kaam batao.
14. Compiler ke andar ke stages (lexer se code-gen tak) batao.
15. Linker ka kaam kya hai?
16. `undefined reference to 'foo()'` kis step ka error hai?
17. `expected ';' before` kis step ka error hai?
18. Kya `main()` program ka pehla function hai? Explain.

### Program/Process
19. Program, executable, aur process mein fark.
20. Ek executable se kitne processes ban sakte hain?
21. Process aur thread mein 3 fark.

### Binary
22. `1101` (binary) decimal mein kya hai?
23. `27` (decimal) binary mein kya hai?
24. `0xFF` decimal mein kya hai?
25. 8-bit signed integer ki range kya hai?
26. Two's complement mein `-1` (8-bit) kaisa dikhega?
27. Little endian aur big endian mein fark? Network byte order kaunsa?

### Memory
28. Memory hierarchy ke levels batao, fastest se slowest.
29. Cache line kya hai? Size?
30. Stack aur heap mein 4 fark.
31. Virtual memory kya hai?
32. Page fault kya hai aur HFT mein woh problem kyun hai?

---

## PART B — Practical exercises

### B1. Terminal drill (bina mouse ke)
```bash
# 1. Home pe jao
# 2. 'phase0' naam ka folder banao
# 3. Uske andar 'day1', 'day2', 'day3' banao
# 4. day1 mein 'test.cpp' banao
# 5. Uska content 'ls -la' se dekho
# 6. day1 ko day1-backup mein copy karo
# 7. Sab kuch delete kar do
```
<details><summary>Solution</summary>

```bash
cd ~
mkdir phase0
cd phase0
mkdir day1 day2 day3
touch day1/test.cpp
ls -la day1
cp -r day1 day1-backup
cd ~
rm -rf phase0
```
</details>

### B2. Pipeline explorer
Ek program likho jo "Namaste Duniya" print kare. Phir:
1. `-E` se preprocess karke line count dekho
2. `-S` se assembly dekho
3. `-c` se object file banao aur `nm -C` se symbols dekho
4. Link karke chalao
5. `ldd` se dependencies dekho

Har step ka output apne notes mein likho.

### B3. Binary practice (kagaz pe, phir verify)
Convert karo:

| Binary → Decimal | Decimal → Binary | Hex → Decimal |
|---|---|---|
| `10110` | `45` | `0x2A` |
| `11111111` | `128` | `0xFF` |
| `10000001` | `200` | `0x100` |

Verify karne ke liye:
```bash
echo "ibase=2; 10110" | bc          # binary -> decimal
echo "obase=2; 45" | bc             # decimal -> binary
echo "ibase=16; 2A" | bc            # hex -> decimal
```

### B4. Error hunting
In 5 programs mein se har ek mein ek galti hai. Batao: **kaunsi galti aur kis stage
pe pakdi jayegi?**

```cpp
// Program 1
#include <iostrem>
int main() { return 0; }
```

```cpp
// Program 2
#include <iostream>
int main() {
    std::cout << "Hi"
    return 0;
}
```

```cpp
// Program 3
#include <iostream>
int main() {
    cout << "Hi";
    return 0;
}
```

```cpp
// Program 4
#include <iostream>
int helper();
int main() {
    std::cout << helper();
    return 0;
}
```

```cpp
// Program 5
#include <iostream>
int main() {
    int* p = nullptr;
    std::cout << *p;
    return 0;
}
```

<details><summary>Answers</summary>

1. **Preprocessor (Step 2):** `iostrem` galat spelling — `fatal error: iostrem: No such file`
2. **Parser (Step 3b):** semicolon missing — `expected ';' before 'return'`
3. **Semantic (Step 3c):** `std::` missing — `'cout' was not declared in this scope`
4. **Linker (Step 5):** `helper` declare kiya, define nahi — `undefined reference`
5. **Runtime (Step 8):** null pointer dereference — `Segmentation fault`, exit code 139

Yeh exercise poore course ka sabse useful hai. Errors ko **classify** karna seekh lo,
debugging 10x tez ho jayegi.
</details>

### B5. Cache experiment (dobara, ache se)
`cache_demo.cpp` (lesson 11 se) chalao alag `N` values ke saath: 512, 1024, 2048, 4096, 8192.

Ek table banao:

| N | Row-wise (ms) | Column-wise (ms) | Slowdown |
|---|---|---|---|
| 512 | | | |
| 1024 | | | |
| 2048 | | | |
| 4096 | | | |
| 8192 | | | |

Kya pattern dikha? Kyun?

### B6. Memory layout
`memory.cpp` (lesson 11 se) chalao. Addresses ko chhote se bade order mein sort karke
likho. Kya woh memory layout diagram se match karta hai?

### B7. Latency reasoning
Aapka HFT system ka budget hai: **market data aane se order jaane tak 5 microseconds.**

Yeh table dekho:
| Operation | Time |
|---|---|
| L1 cache hit | 1 ns |
| L3 cache hit | 15 ns |
| RAM access | 100 ns |
| Syscall | 500 ns |
| Page fault | 5000 ns |
| Heap allocation (worst case) | 10000 ns |

Sawal:
1. Aap 5 µs mein kitne RAM accesses afford kar sakte ho?
2. Ek page fault aapke budget ka kitna % kha jayega?
3. Agar aapko 3 syscalls karne pade, kitna budget bachega?
4. Kya aap hot path mein `new` call kar sakte ho? Kyun/kyun nahi?

<details><summary>Answers</summary>

1. 5000 ns / 100 ns = **50 RAM accesses**. Bahut kam! Isliye cache locality critical hai.
2. 5000 ns / 5000 ns = **100%** — ek page fault poora budget kha jaata hai. 😱
3. 3 × 500 = 1500 ns. Bacha: 3500 ns. Isliye syscalls minimize karte hain.
4. **Nahi.** Worst case 10 µs — budget se dugna. Isliye pre-allocation aur memory pools.
</details>

---

## PART C — Writing exercise

Ek page mein (Hinglish mein) likho:

> **"Jab main `g++ hello.cpp -o hello && ./hello` chalata hoon, to shuru se aakhir tak
> kya hota hai?"**

Cover karo:
- Har compilation step
- OS ka role
- Memory kaise setup hoti hai
- CPU kya karti hai
- Output screen pe kaise pahunchta hai

Yeh exercise sabse important hai. Agar aap yeh clearly likh sakte ho, aapka
Phase 0 solid hai.

---

## PART D — Self-assessment

Imaandari se tick karo:

```
[ ] Mujhe computer ke 4 hisse pata hain aur unka kaam samajh aata hai
[ ] Main algorithm aur program mein fark bata sakta hoon
[ ] Mujhe compile vs interpret ka fark clear hai
[ ] Main compilation ke 5 steps naam se bata sakta hoon
[ ] Main error dekh kar bata sakta hoon ki woh kis step ka hai
[ ] Mujhe program/executable/process ka fark pata hai
[ ] Main binary <-> decimal <-> hex convert kar sakta hoon
[ ] Mujhe two's complement samajh aata hai
[ ] Mujhe endianness ka concept clear hai
[ ] Main memory hierarchy bana sakta hoon
[ ] Mujhe stack vs heap ka fark pata hai
[ ] Mujhe pata hai cache line kya hai aur kyun matter karti hai
[ ] Main terminal se comfortably navigate kar sakta hoon
[ ] Maine compiler install kar liya hai aur test kar liya hai
[ ] Maine sabhi hands-on examples chalaye hain
```

**Scoring:**
- **13-15 ticks** → Bahut badhiya! Folder 02 pe jao. 🎉
- **10-12 ticks** → Achha. Jo miss hua woh dobara padho, phir aage.
- **7-9 ticks** → Folder dobara skim karo, hands-on zaroor karo.
- **< 7 ticks** → Poora folder dobara. Jaldi mat karo. Foundation sab kuch hai.

---

## PART E — Challenge (optional, but karo)

**Challenge: "Latency Detective"**

Ek program likho (abhi C++ nahi aata to bas plan likho) jo:
1. 1 million integers ka array banaye
2. Unhe sequentially sum kare, time measure kare
3. Unhe randomly (shuffled order mein) sum kare, time measure kare
4. Dono ka fark print kare

Phir explain karo ki fark kyun aaya, **cache line ke terms mein**.

Agar aap yeh abhi nahi likh sakte — koi baat nahi. Folder 09 (arrays) ke baad wapas
aana. Yeh challenge yahan intentionally rakha hai taaki aapko pata rahe ki aage kya
banane wale ho.

---

## Aapne Phase 0 complete kar liya! 🎉

Aapko ab pata hai:
- Computer kaise kaam karta hai
- Programming kya hai
- C++ kya hai aur kyun
- Code kaise executable banta hai
- Memory kaise organized hoti hai
- Binary system

**Ab tak aapne ek line C++ nahi likhi.** Aur yahi plan tha.

Ab likhenge. Aur ab jab likhenge, to aapko pata hoga ki **andar kya ho raha hai** —
jo 90% beginners ko nahi pata hota.

---

## Next
→ [`../02-CPP-FIRST-STEPS/00-README.md`](../02-CPP-FIRST-STEPS/00-README.md)

**Ab asli C++ shuru hoti hai.** 🚀

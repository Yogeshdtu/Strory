# 11 — Memory basics

## Prerequisites
`01-what-is-a-computer.md`, `10-bits-bytes-binary.md`

## Yeh topic abhi kyun
Agle folder mein hum variables banayenge. Variable **memory mein rehta hai**. Isliye
pehle memory ka mental model banate hain. Yeh model aage pointers (12), memory (14),
aur cache (32) mein baar-baar kaam aayega.

---

## Memory ka mental model: ek badi society

Memory ko ek **bahut badi building** samjho jisme karodon flats hain.

```
   Address     Content
   --------    -------
   1000        [ 01001101 ]     <- 1 byte
   1001        [ 11000010 ]     <- 1 byte
   1002        [ 00000000 ]
   1003        [ 01110001 ]
   1004        [ 00101010 ]
   ...         ...
```

**Do rules:**

1. **Har byte ka apna address hota hai** — flat number ki tarah, unique
2. **Har address mein exactly 1 byte (8 bits) hota hai** — flat ka size fixed hai

Address ek number hota hai. 64-bit system pe address 64 bits ka hota hai —
matlab theoretically 2^64 = 18 quintillion addresses. (Practically OS itna use nahi karta.)

Addresses hamesha **hex** mein likhe jaate hain: `0x7ffd4a2b3c40`

---

## Multi-byte values

Ek `int` 4 bytes ka hota hai. To woh **4 consecutive addresses** leta hai:

```
   Address     Content
   --------    -------
   1000        [ 00000000 ]  \
   1001        [ 00000000 ]   |  ek int (4 bytes)
   1002        [ 00100111 ]   |  value: 10000
   1003        [ 00010000 ]  /

   Hum kehte hain: "int ka address 1000 hai"
   (matlab: pehle byte ka address)
```

Yeh important hai: jab hum kehte hain "variable ka address", hamesha **pehle byte
ka address** matlab hota hai.

---

## Memory hierarchy — speed ka pyramid

Sab memory ek jaisi nahi hoti. Ek **hierarchy** hoti hai:

```
                    /\
                   /  \        REGISTERS
                  /____\       ~1 KB, 0 cycles, CPU ke andar
                 /      \
                /   L1   \     L1 CACHE
               /__________\    ~32-64 KB, ~4 cycles (~1 ns)
              /            \
             /      L2      \  L2 CACHE
            /________________\ ~256 KB-1 MB, ~12 cycles (~4 ns)
           /                  \
          /        L3          \ L3 CACHE
         /______________________\ ~8-32 MB, ~40 cycles (~15 ns)
        /                        \
       /          RAM             \ MAIN MEMORY
      /____________________________\ 8-64 GB, ~200 cycles (~100 ns)
     /                              \
    /        SSD / DISK              \ STORAGE
   /__________________________________\ 256 GB-4 TB, ~100 microseconds

   UPAR:  chhota, mehnga, BAHUT TEZ
   NEECHE: bada, sasta, SLOW
```

### Yeh hierarchy exist kyun karti hai?

Kyunki fast memory **mehngi** hai. Agar poori RAM L1 cache jitni tez hoti, aapka
laptop crore rupaye ka hota.

To design yeh hai: **chhoti fast memory mein woh data rakho jo abhi chahiye,
baaki slow memory mein.**

Aur yeh kaam karta hai kyunki programs mein **locality** hoti hai:
- **Temporal locality:** abhi jo data use kiya, woh dobara jaldi use hoga
- **Spatial locality:** jo data use kiya, uske paas wala data bhi jaldi use hoga

---

## Cache line — 64 bytes ka rule 🔑

CPU RAM se **kabhi ek byte nahi laata**. Woh hamesha ek poora **cache line** laata hai —
usually **64 bytes**.

```
   Aapne maanga:   address 1000 pe ek byte
   CPU laaya:      addresses 960-1023 (poora 64-byte block)
```

**Iska matlab kya hai?**

```cpp
// Case A: consecutive access — FAST
int arr[1000];
for (int i = 0; i < 1000; i++)
    sum += arr[i];        // 64 bytes mein 16 ints aate hain
                          // 1 cache miss ke baad 15 hits!

// Case B: scattered access — SLOW
for (int i = 0; i < 1000; i += 16)
    sum += arr[i];        // har baar naya cache line
                          // har access ek miss!
```

Case A **10-50x tez** ho sakta hai, chahe dono O(n) hain.

> **YEH HFT KA CENTRAL IDEA HAI.**
> Algorithm ki Big-O complexity aksar matter nahi karti jitni **memory access pattern**
> karti hai. Ek O(n) linear scan aksar O(log n) tree search se tez hota hai chhote
> data pe — kyunki array cache-friendly hai aur tree pointer-chasing karta hai.
> Folder 32 poora isi pe hai.

---

## RAM vs Storage — dobara, clear

| | RAM | Storage (SSD) |
|---|---|---|
| Purpose | abhi chal rahe programs | permanent files |
| Speed | ~100 ns | ~100 µs (1000x slow) |
| Volatile? | **haan** — bijli gayi, sab gaya | nahi |
| CPU seedha access kar sakti? | **haan** | nahi (pehle RAM mein aana padega) |
| Size | 8-64 GB | 256 GB - 4 TB |

**Aapka program hamesha RAM mein chalta hai.** File se data padhne ka matlab hai:
storage → RAM → CPU.

---

## Aapka program memory mein kaise dikhta hai

Yeh diagram folder 08 mein dekha tha. Dobara, thoda detail se:

```
   HIGH ADDRESS (e.g. 0x7fff...)
   +-----------------------------------+
   |  COMMAND LINE ARGS / ENV          |
   +-----------------------------------+
   |                                   |
   |  STACK                            |  <- local variables, function calls
   |     |                             |     AUTOMATIC (compiler manage karta hai)
   |     v  (neeche badhta hai)        |     FAST
   |                                   |     SIZE LIMITED (~8 MB default)
   +-----------------------------------+
   |                                   |
   |         (khali jagah)             |
   |                                   |
   +-----------------------------------+
   |     ^  (upar badhta hai)          |
   |     |                             |
   |  HEAP                             |  <- new/delete se milne wali memory
   |                                   |     MANUAL (aap manage karte ho)
   +-----------------------------------+     SLOW(er)
   |  BSS                              |  <- uninitialized globals (auto zero)
   +-----------------------------------+
   |  DATA                             |  <- initialized globals
   +-----------------------------------+
   |  TEXT / CODE                      |  <- aapke instructions (read-only)
   +-----------------------------------+
   LOW ADDRESS (e.g. 0x400000)
```

### Stack vs Heap — quick comparison

| | STACK | HEAP |
|---|---|---|
| Manage kaun karta hai | compiler (automatic) | aap (`new`/`delete`) |
| Speed | **bahut tez** (bas pointer move) | slow (allocator ko search karna padta hai) |
| Size | limited (~1-8 MB) | badi (RAM jitni) |
| Lifetime | scope khatam = memory free | jab tak `delete` na karo |
| Fragmentation | nahi | **haan** |
| Cache-friendly | **haan** (contiguous, hot) | kam (scattered) |
| Kab use | zyadatar variables | badi ya variable-size data |

> **HFT relevance:** HFT ke hot path mein hum **heap allocation bilkul avoid karte hain**.
> Kyun? Kyunki `new` ek unpredictable operation hai — kabhi 20 ns, kabhi 10,000 ns
> (agar OS se nayi memory maangni pade). Yeh **jitter** create karta hai.
> Solution: sab kuch pehle se allocate karo (memory pools), phir reuse karo.
> Folder 36 mein poora seekhoge.

---

## Virtual memory — ek important jhoot

Jab aapka program address `0x1000` dekhta hai, woh **RAM ka asli address nahi hai**.

Yeh **virtual address** hai. OS aur CPU (MMU - Memory Management Unit) milkar isse
asli physical address mein translate karte hain.

```
   AAPKA PROGRAM              OS + MMU              PHYSICAL RAM
   virtual address  ------>  translation  ------>  physical address
      0x1000                  (page table)            0x8A34F000
```

**Fayde:**
- Har process ko lagta hai uske paas poori memory hai
- Processes ek doosre ki memory nahi dekh sakte (**security + isolation**)
- RAM se zyada memory use kar sakte ho (swap)

**Cost:** Har memory access mein translation hoti hai. Isko tez karne ke liye ek cache
hota hai — **TLB (Translation Lookaside Buffer)**.

> **HFT relevance:** TLB miss mehnga hota hai (~100+ cycles). Isliye HFT systems
> **huge pages** use karte hain (4 KB ki jagah 2 MB pages) — kam pages = kam TLB entries
> = kam misses. Folder 29 aur 32 mein.

---

## Page fault — ek chhupa hua latency killer

Memory **pages** mein baanti jaati hai (usually 4 KB).

Jab aap aisi memory access karte ho jo abhi physically map nahi hui, to **page fault**
hota hai:

```
   Program:  "address 0x5000 chahiye"
      |
      v
   MMU: "yeh page mapped nahi hai!" -> PAGE FAULT
      |
      v
   OS: kernel mein jao, page allocate karo, map karo, wapas aao
      |
      v
   Program: continue (par ~1000-10000 ns waste ho gaye)
```

> **HFT relevance:** Ek page fault aapki 5 µs ki latency ko 50 µs bana sakta hai.
> Solution: `mlockall()` se saari memory pehle hi lock karo, aur startup pe poori
> memory **touch** karo (har page mein kuch likho) taaki page faults startup pe hi ho
> jaayein, trading ke waqt nahi. Isko "memory pre-faulting" ya "warming" kehte hain.

---

## Hands-on: memory dekhte hain

```bash
cd ~/cpp-practice
cat > memory.cpp << 'END'
#include <iostream>

int globalInit = 42;        // DATA section
int globalUninit;           // BSS section

int main() {
    int localVar = 10;              // STACK
    int* heapVar = new int(20);     // HEAP
    static int staticVar = 30;      // DATA

    // Addresses print karo (hex mein)
    std::cout << std::hex;
    std::cout << "Code (main):     " << (void*)&main       << "\n";
    std::cout << "Global (init):   " << (void*)&globalInit << "\n";
    std::cout << "Global (uninit): " << (void*)&globalUninit << "\n";
    std::cout << "Static:          " << (void*)&staticVar  << "\n";
    std::cout << "Heap:            " << (void*)heapVar     << "\n";
    std::cout << "Stack (local):   " << (void*)&localVar   << "\n";

    delete heapVar;
    return 0;
}
END
g++ -std=c++20 -Wall memory.cpp -o memory && ./memory
```

**Expected pattern** (numbers alag honge, order same hoga):
```
Code (main):     0x55d3f8a01189    <- sabse neeche
Global (init):   0x55d3f8a04010
Global (uninit): 0x55d3f8a04018
Static:          0x55d3f8a04014
Heap:            0x55d3f9a2b2b0    <- beech mein
Stack (local):   0x7ffd4a2b3c44    <- sabse upar (bada address)
```

Dekha? Stack ka address **bahut bada** hai, code ka **bahut chhota**. Yeh exactly
wahi layout hai jo upar diagram mein tha. 🎉

---

## Cache locality ka experiment (yeh zaroor chalao)

```bash
cat > cache_demo.cpp << 'END'
#include <iostream>
#include <chrono>
#include <vector>

int main() {
    const int N = 4096;
    std::vector<std::vector<int>> matrix(N, std::vector<int>(N, 1));
    
    // ROW-WISE: memory mein consecutive - CACHE FRIENDLY
    auto t1 = std::chrono::high_resolution_clock::now();
    long long sum1 = 0;
    for (int i = 0; i < N; ++i)
        for (int j = 0; j < N; ++j)
            sum1 += matrix[i][j];
    auto t2 = std::chrono::high_resolution_clock::now();

    // COLUMN-WISE: memory mein bikhra hua - CACHE UNFRIENDLY
    long long sum2 = 0;
    for (int j = 0; j < N; ++j)
        for (int i = 0; i < N; ++i)
            sum2 += matrix[i][j];
    auto t3 = std::chrono::high_resolution_clock::now();

    auto rowTime = std::chrono::duration_cast<std::chrono::milliseconds>(t2-t1).count();
    auto colTime = std::chrono::duration_cast<std::chrono::milliseconds>(t3-t2).count();

    std::cout << "Row-wise (cache friendly):    " << rowTime << " ms\n";
    std::cout << "Column-wise (cache unfriendly): " << colTime << " ms\n";
    std::cout << "Slowdown: " << (double)colTime / rowTime << "x\n";
    std::cout << "(sums equal: " << (sum1 == sum2) << ")\n";
    return 0;
}
END
g++ -std=c++20 -O2 cache_demo.cpp -o cache_demo && ./cache_demo
```

**Yeh chalao.** Aapko 3-10x ka fark dikhega — **same algorithm, same complexity,
sirf memory access pattern alag.**

Yeh aapka pehla performance engineering lesson hai. Ise mat bhoolna.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Memory ek flat, uniform cheez hai" | Hierarchy hai — registers/L1/L2/L3/RAM, 100x speed difference |
| "Program address = RAM address" | Nahi, virtual address hai. OS translate karta hai |
| "Ek byte access = ek byte load" | Nahi! Poora 64-byte cache line aata hai |
| "Heap aur stack alag memory hardware hain" | Nahi, dono RAM mein hain. Bas alag tarah manage hote hain |
| "Big-O hi performance batata hai" | Cache behaviour aksar zyada matter karti hai |

---

## Exercises

1. `memory.cpp` chalao. Addresses ka order note karo. Kya woh diagram se match karta hai?

2. `cache_demo.cpp` chalao. Kitna slowdown mila? `N` ko 1024 aur 8192 karke dekho —
   fark badhta hai ya ghatta hai? Kyun?
   <details><summary>Answer</summary>
   Chhote N pe (1024) fark kam hoga kyunki poora data L2/L3 cache mein fit ho jaata hai.
   Bade N pe fark badhta hai kyunki cache mein nahi samata.
   </details>

3. Ek 64-byte cache line mein kitne `int` (4 bytes) aayenge? Kitne `double` (8 bytes)?
   <details><summary>Answer</summary>16 ints, 8 doubles</details>

4. Apne CPU ka cache size dekho:
   ```bash
   lscpu | grep -i cache          # Linux
   getconf -a | grep CACHE        # Linux
   sysctl hw.l1dcachesize hw.l2cachesize    # macOS
   ```

5. Sochkar batao: heap allocation stack allocation se slow kyun hai?
   <details><summary>Answer</summary>
   Stack: bas stack pointer ko move karna hai — 1 instruction.
   Heap: allocator ko free block dhoondhna hai, metadata update karni hai, shayad
   OS se nayi memory maangni pade (`mmap`/`brk` syscall), thread-safety ke liye lock
   lena pad sakta hai. 10-1000x zyada kaam.
   </details>

6. Agar ek page fault ~5 µs leta hai aur aapka trading loop 1 µs ka budget rakhta hai,
   to ek page fault kitne loops ki latency kha jayega?
   <details><summary>Answer</summary>
   5 loops. Aur woh 5 loops mein market move ho chuka hoga. Isliye pre-faulting zaroori hai.
   </details>

---

## Interview questions

1. Stack aur heap mein kya fark hai?
2. Cache line kya hai? Uska size kitna hota hai?
3. Virtual memory kya hai? Kyun exist karti hai?
4. TLB kya hai?
5. Cache locality kya hai? Spatial aur temporal mein fark?
6. Page fault kya hai?

---

## Next
→ [`12-exercises.md`](12-exercises.md) — is folder ka final revision

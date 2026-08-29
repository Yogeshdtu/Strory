# What I Still Need To Learn — Gap Tracker

Yeh file honest hai. Iska kaam yeh **nahi** hai ki achha dikhe. Iska kaam yeh hai ki
aapko exactly bataye ki **abhi kya baaki hai**.

Jab yeh file har section mein "NONE" bolegi, tab course complete hoga.

Last updated: **Batch 2 (Phase 0/1/2 complete)**

---

## Abhi ki status (Phase 0, 1, 2 complete)

### MAJOR C++ LANGUAGE GAPS

**BAAKI HAI:**

- Control flow: `if`, `switch`, loops → folders 06, 07
- Functions, overloading, call stack, recursion → folder 08
- Arrays, array decay, `std::array`, `std::span` → folder 09
- Strings, `std::string`, `string_view`, SSO → folder 10
- Structs, unions, enums, bitfields, padding/alignment → folder 11
- Pointers (poora) → folder 12
- References aur value categories → folders 13, 18
- Memory: stack/heap/`new`/`delete`/leaks → folder 14
- Classes, constructors, destructors, operator overloading → folder 15
- Inheritance, virtual functions, vtables, polymorphism → folder 16
- RAII aur smart pointers → folder 17
- Copy/move semantics, Rule of 0/3/5, perfect forwarding → folder 18
- Templates, SFINAE, concepts, metaprogramming → folder 21
- Modern C++ (lambdas, `constexpr`, ranges, coroutines, modules) → folder 22
- Exceptions aur error handling → folder 23
- Namespaces, linkage, ODR → folder 24
- Object model, lifetime, UB, casts, RTTI → folder 25

### MAJOR STANDARD LIBRARY GAPS

**BAAKI HAI:** Poora STL (folder 19), strings (10),
smart pointers (17), type traits/concepts (21), chrono (19/35),
threading library (26), atomics (27), ranges (19/22), coroutines (22),
filesystem/random/regex/functional/bit (19), allocators aur PMR (19/36).

### MAJOR MODERN C++ GAPS

**BAAKI HAI:** Sab kuch. C++11 se C++23 tak — folder 22 mein aayega,
plus har folder mein us topic ka modern version.

### MAJOR SYSTEMS C++ GAPS

**BAAKI HAI:** Linux internals (29), networking (30), CPU architecture (31),
cache/memory performance (32), compiler optimization (33), assembly (34),
profiling/benchmarking (35).

### MAJOR HFT-RELEVANT C++ GAPS

**BAAKI HAI:** Sab kuch — low-latency C++ (36) se lekar HFT projects (44) tak.
Detail ke liye `HFT-COMPLETENESS-AUDIT.md` dekho.

---

## Kya ABHI TAK cover ho chuka hai ✅

**PHASE 0, 1, 2 — poore ho chuke hain.**


- Programming aur computer ke fundamentals
- C++ ka pehla program — har token ka meaning
- Compilation pipeline: source → preprocessor → compiler → assembler → linker →
  executable → OS loader → CPU
- Comments, statements, semicolons, blocks
- Variables, data, memory ka basic model
- Fundamental types: `int`, `char`, `bool`, `float`/`double`
- Signed/unsigned, sizes, ranges, overflow
- Floating-point precision traps
- Fixed-width types
- Initialization forms aur narrowing
- `auto`, `const`, `constexpr` (intro level)
- Type conversions aur integer promotion
- `sizeof`, `<limits>`
- Compiler flags, warnings, error reading
- Development environment setup
- **Poora I/O**: `cout`/`cin` deep, buffering aur flushing, `cerr`/`clog`,
  `getline`, input validation aur stream states, manipulators, `std::format` (C++20),
  file streams (text + binary), string streams, `printf` family aur uske dangers,
  I/O performance aur HFT logging rules
- **Poore operators**: arithmetic aur uske 4 traps, `++`/`--`, comparisons
  (signed/unsigned, float, NaN), logical + short-circuit, **saare bitwise operators**,
  bit manipulation toolkit, C++20 `<bit>`, compound assignment, ternary,
  precedence aur associativity, evaluation order aur sequencing

---

## SPECIALIZED / DOMAIN-SPECIFIC

Yeh topics **jaan-boojh kar** is course ke scope se bahar rakhe gaye hain — ya sirf
introduction level pe cover honge. Inhe silently chhodna galat hota, isliye yahan
explicitly list kar raha hoon:

| Topic | Kyun bahar | Kahan tak cover hoga |
|---|---|---|
| **FPGA / Verilog / HLS** | Hardware design ek alag career hai | Folder 42 mein sirf "yeh kya hai aur kab use hota hai" |
| **Full DPDK application development** | DPDK apne aap mein ek badi library hai | Folder 30/42 mein concepts + minimal example |
| **RDMA / InfiniBand programming** | Specialized, mostly HPC | Folder 30 mein concept level |
| **Exchange-specific protocol specs** (NSE NEAT, CME MDP 3.0, Nasdaq TotalView-ITCH exact spec) | Proprietary/licensed documents | Folder 38 mein ITCH-style generic protocol + parser |
| **Actual trading strategies (alpha)** | Yeh IP hai, koi public nahi karta | Folder 37/44 mein strategy *framework*, alpha nahi |
| **Quant finance math** (stochastic calculus, options pricing) | Alag domain — quant researcher ka kaam, C++ engineer ka nahi | Folder 37 mein basic microstructure only |
| **Regulatory/compliance detail** (SEBI, MiFID II, Reg NMS) | Legal domain | Folder 37 mein overview |
| **GPU / CUDA programming** | HFT mein aksar irrelevant (latency-wise) | Mention only |
| **Windows systems programming** | HFT Linux pe chalta hai | Setup guide tak |
| **Embedded C++ / MISRA** | Alag domain | Nahi |
| **Boost library deep dive** | Standard library pehle | Selected parts jahan relevant ho (folder 19) |
| **Qt / GUI programming** | HFT ke liye irrelevant | Nahi |
| **C++ standard committee process / wording** | Language lawyering | Nahi (par UB aur ODR jaise rules cover honge) |

Agar aapko in mein se koi topic chahiye, alag se bolna — main uske liye supplementary
material bana dunga.

---

## Kaise use karein yeh file

Har batch ke baad yeh file update hoti hai. Aapka goal:

```
Batch 1  ->  har section mein "BAAKI HAI" ki lambi list
Batch 5  ->  list chhoti ho jaati hai
Batch 9  ->  har section: NONE
```

Jab yeh dikhe:

```
### MAJOR C++ LANGUAGE GAPS
NONE

### MAJOR STANDARD LIBRARY GAPS
NONE

### MAJOR MODERN C++ GAPS
NONE

### MAJOR SYSTEMS C++ GAPS
NONE

### MAJOR HFT-RELEVANT C++ GAPS
NONE
```

...tab course structurally complete hai. Lekin yaad rakhna: **file complete hona aur
aapka seekhna complete hona — alag cheezein hain.** Content likha hona kaafi nahi,
aapko woh code likhna, chalana, todna aur samajhna padega.

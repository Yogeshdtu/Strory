# CLAUDE.md — Project brief for Claude Code

Yeh file Claude Code automatically padhta hai. Ismein poora context hai: project kya hai,
kya ban chuka hai, kya banana baaki hai, aur **kis quality bar pe** banana hai.

---

## 1. Project kya hai

**CPP-MASTERY** — ek complete C++ learning repository jo absolute zero se shuru hokar
HFT (High-Frequency Trading) engineering tak le jaati hai.

**Target learner:** koi bhi jisne kabhi programming nahi ki. Aur end goal: woh
low-latency/HFT-grade C++ systems samajh sake, likh sake, debug kar sake, profile
kar sake aur optimize kar sake.

**Language:** Sab kuch **Hinglish** mein — technical terms English mein (industry
standard hain), explanation simple Hindi mein. Code comments bhi Hinglish mein.

---

## 2. Non-negotiable requirements (original spec se)

Yeh 30 requirements user ne di thi. Inhe follow karna **mandatory** hai:

1. **Absolute beginner first** — assume karo ki learner ne kabhi program nahi likha
2. **Strict progression** — koi advanced concept uske prerequisites se pehle nahi
3. **Beginner curriculum bahut slow** — `int age = 20;` bhi tod ke samjhao
4. **Hinglish** — explanation aur code comments dono
5. **Har file mein:** `## Prerequisites`, `## Yeh topic abhi kyun`, `## Next`
6. **Theory (.md) + Code (.cpp)** har major topic ke liye
7. **Topic depth** — har major topic mein: What / Why / Intuition / Syntax /
   Simple examples / Multiple examples / Internal working / Memory model /
   Common mistakes / Edge cases / Performance / Real-world use / **HFT relevance** /
   Exercises / Interview questions / Challenge
8. **Chhoti files sirf checklist tick karne ke liye mat banao** — depth chahiye
9. **Interactive learning** — output prediction, "find the bug", memory diagrams,
   execution traces, benchmark experiments, "optimize this code"
10. **Projects** — disconnected examples nahi, connected projects
11. **HFT track mein real code** — order book, matching engine, market data simulator,
    lock-free structures, memory pool, low-latency event pipeline
12. **HFT performance engineering process** — har project ke liye:
    build simple → measure → profile → find bottleneck → optimize → re-benchmark →
    **explain kya badla**. Kabhi seedha complex optimization pe mat jao.
13. **No micro-optimization tricks without context** — har technique ke saath trade-off
14. **Completeness audits** maintain karo (neeche section 7)

---

## 3. Kya ban chuka hai (STATUS)

### ✅ COMPLETE — folders `00` se `05` tak

| Folder | Lessons | Examples | Phase |
|---|---|---|---|
| `00-START-HERE/` | 9 | — | roadmap, setup, audits, glossary |
| `01-PROGRAMMING-BASICS/` | 13 | 5 | PHASE 0 |
| `02-CPP-FIRST-STEPS/` | 12 | 7 | PHASE 1 |
| `03-VARIABLES-DATA-TYPES/` | 17 | 10 | PHASE 2 |
| `04-INPUT-OUTPUT/` | 13 | 8 | PHASE 2 |
| `05-OPERATORS/` | 12 | 6 | PHASE 2 |

**Total: ~136,000 words, 36 example files (sab compile-verified).**

### ⏳ PENDING — folders `06` se `49` tak

In sab mein **`00-README.md` already mojood hai** jisme poora syllabus likha hai —
file list, har file ka topic, examples ki list, prerequisites, aur time estimate.

**Naya folder banate waqt sabse pehle uska `00-README.md` padho** — woh aapka spec hai.
Usme jo file names aur topics likhe hain, unhi ko banao.

---

## 4. Style conventions (inhe exactly follow karo)

### Lesson file ka structure

```markdown
# NN — Topic ka naam

## Prerequisites
Kya pehle se aana chahiye (specific file names ke saath)

## Yeh topic abhi kyun
Iss point pe yeh kyun padha rahe hain — pichle topic se connection

## [Main content sections]
Simple explanation → intuition/analogy → syntax → examples →
andar kya hota hai → memory model

## ⚠️ Traps / Common mistakes
Numbered traps ke saath, har ek ka demo

## > **HFT relevance:**
Callout blocks jahan relevant ho — beginner lessons mein bhi

## Hands-on
Chalane wala code with exact compile command

## Common galat samajh
| ❌ Galat | ✅ Sahi |

## Exercises
Numbered, `<details><summary>Answer</summary>` collapsibles ke saath

## Interview questions
Numbered list

## Next
→ [`NN-agli-file.md`](NN-agli-file.md)
```

### Code example ka style

```cpp
// NN_example_name.cpp
// ============================================================
// Ek line mein kya dikhata hai
// ============================================================
//   g++ -std=c++20 -Wall -Wextra NN_example.cpp -o name && ./name
// ============================================================

#include <iostream>

int main() {
    // ============================================================
    //  1. SECTION NAAM
    // ============================================================
    // Comment mein batao: kya ho raha hai, kyun, memory mein kya,
    // performance impact, aur HFT relevance (jahan applicable ho)
    ...
}
```

### Formatting rules

- Misconception tables: `| ❌ Galat | ✅ Sahi |`
- ASCII diagrams memory layouts, pipelines, hierarchies ke liye
- Compile commands hamesha: `g++ -std=c++20 -Wall -Wextra -g file.cpp -o file`
- Benchmarks ke liye hamesha `-O2` (aur bataao ki `-O0` pe benchmark bekaar hai)
- `⚠️` traps ke liye, `✅` sahi tareeke ke liye, `❌` galat ke liye
- Har lesson ke end mein `## Next` link — chain kabhi mat todo

---

## 5. QUALITY BAR — yeh sabse important hai

### Rule 1: Har `.cpp` file compile-verified honi chahiye

```bash
g++ -std=c++20 -Wall -Wextra -Wshadow file.cpp -o /tmp/test && /tmp/test
```

Ya repo ke Makefile se:
```bash
make folder DIR=06-CONDITIONS      # poore folder ke examples check karo
make checkall                       # poore repo ke
```

**Ek bhi example bina chalaye commit mat karo.**

Exception: `*_broken_on_purpose.cpp` files — woh jaan-boojh kar fail karti hain
(learner ko fix karna hai). Unke answers file ke neeche comment mein hote hain.

### Rule 2: Benchmark ke numbers REAL hone chahiye

Kabhi "yeh 10x tez hai" mat likho bina chalaye. **Program chalao, actual number lo,
aur wahi likho.**

Aur benchmark likhte waqt dhyaan rakho ki **sirf wahi cheez measure ho jo claim kar
rahe ho**. (Is repo mein pehle ek division benchmark galat tha — loop ke andar `%`
division ki cost chhupa raha tha. Aur bit-flags memory comparison zero saving dikha
raha tha kyunki padding ne fark kha liya tha. Dono fix kiye gaye.)

Agar measurement expectation se ulta aaye — **usse chhupao mat, wahi teach karo.**
Woh aksar behtar lesson hota hai.

### Rule 3: Prerequisites chain kabhi mat todo

Pointer se pehle: variables → addresses → memory.
Mutex se pehle: functions → threads → race conditions → shared data.
Atomics memory ordering se pehle: concurrency → data races → atomics → happens-before.

### Rule 4: Har claim verifiable ho

"`int` 4 bytes ka hota hai" ❌ → "practically 4, par standard guarantee nahi karta,
`int32_t` use karo jab pakka chahiye" ✅

---

## 6. Build aur verify workflow

Repo ke root mein `Makefile` hai:

```bash
make FILE=path/to/file.cpp        # compile + run (debug flags)
make fast FILE=path/to/file.cpp   # -O2 (benchmarks ke liye ZAROORI)
make san FILE=path/to/file.cpp    # AddressSanitizer + UBSan
make asm FILE=path/to/file.cpp    # demangled assembly
make pp FILE=path/to/file.cpp     # preprocessor output
make folder DIR=06-CONDITIONS     # ek folder ke saare examples
make checkall                     # poora repo
make clean
```

**Naya folder khatam karne ke baad hamesha `make folder DIR=...` chalao.**

---

## 7. Audit files maintain karo

Teen files hain jo har batch ke baad **update honi chahiye**:

| File | Kya track karta hai |
|---|---|
| `00-START-HERE/CPP-COMPLETENESS-AUDIT.md` | C++ language + STL coverage, `[✓]/[~]/[ ]` ke saath |
| `00-START-HERE/HFT-COMPLETENESS-AUDIT.md` | HFT topics coverage |
| `00-START-HERE/WHAT-I-STILL-NEED-TO-LEARN.md` | Gap tracker — kya baaki hai |
| `00-START-HERE/BUILD-STATUS.md` | Batch status + learner ka progress checklist |

**Spec ka rule:** "Folder ka exist karna ≠ topic ka covered hona." Ek topic tabhi
`[✓]` hai jab uske paas explanation + 2+ examples + common mistakes + exercises +
interview questions ho.

`WHAT-I-STILL-NEED-TO-LEARN.md` ka final goal: har section mein "NONE" likha ho,
aur genuinely specialist topics **explicitly** "SPECIALIZED / DOMAIN-SPECIFIC"
list mein hon (silently chhode nahi).

---

## 8. Agla kaam (batch plan)

| Batch | Folders | Phase |
|---|---|---|
| **3 (AGLA)** | `06-CONDITIONS`, `07-LOOPS`, `08-FUNCTIONS` | PHASE 3 |
| 4 | `09-ARRAYS`, `10-STRINGS`, `11-STRUCTS` | PHASE 4 |
| 5 | `12-POINTERS`, `13-REFERENCES`, `14-MEMORY` | PHASE 5 |
| 6 | `15-CLASSES`, `16-OOP`, `17-RAII`, `18-COPY-MOVE` | PHASE 6–8 |
| 7 | `19-STL`, `20-DSA`, `21-TEMPLATES`, `22-MODERN-CPP`, `23-ERRORS` | PHASE 9–13 |
| 8 | `24`–`28` (build, object model, concurrency, atomics, lock-free) | PHASE 14–17 |
| 9 | `29`–`35` (Linux, networking, CPU, cache, compiler, asm, profiling) | PHASE 18–23 |
| 10 | `36-LOW-LATENCY` + HFT track `37`–`44` | PHASE 24–32 |
| 11 | `45`–`49` + final gap audit | PHASE 33–34 |

### Batch 3 mein khaas dhyaan

- `08-FUNCTIONS` mein **call stack deep dive** — stack frames, prologue/epilogue,
  return address. Yeh pointers (12) aur recursion ka foundation hai.
- `07-LOOPS` mein **cache locality** ka pehla proper introduction — row-major vs
  column-major, measured benchmark ke saath.
- `06-CONDITIONS` mein **branch prediction ka pehla parichay** — sorted vs unsorted
  array benchmark (classic demo).

---

## 9. Working style — Claude Code ke liye

### Ek baar mein ek folder

Poore batch ko ek saath mat likho. **Ek folder complete karo → verify karo →
commit karo → agla folder.** Context window manage rehta hai aur galtiyan jaldi
pakdi jaati hain.

### Har folder ka process

1. Us folder ka `00-README.md` padho — woh spec hai
2. Pichle folder ke 1-2 lessons padho — style match karne ke liye
3. Lessons likho (file `01` se shuru, numbered order mein)
4. Examples likho
5. **`make folder DIR=...` se verify karo**
6. Benchmarks chalao, **real numbers** lesson mein daalo
7. `NN-exercises.md` likho (folder ka last file)
8. `examples/README.md` likho
9. Audit files update karo
10. Git commit

### Git

Har folder ke baad commit karo:
```bash
git add . && git commit -m "Folder 06: conditions — 9 lessons + 4 examples, verified"
```

Agar repo mein git nahi hai:
```bash
git init && git add . && git commit -m "Initial: folders 00-05 complete"
```

`.gitignore` already mojood hai.

---

## 10. Reference: existing files jo style dikhate hain

Naya content likhne se pehle inhe padho:

| Kis cheez ke liye | File |
|---|---|
| Deep token-by-token explanation | `02-CPP-FIRST-STEPS/02-anatomy-line-by-line.md` |
| Traps + measured benchmarks | `03-VARIABLES-DATA-TYPES/05-int-deep-dive.md` |
| HFT relevance weaving | `04-INPUT-OUTPUT/03-buffering-and-flushing.md` |
| Bitwise/systems depth | `05-OPERATORS/05-bitwise-operators.md` |
| Exercises file format | `03-VARIABLES-DATA-TYPES/16-exercises.md` |
| Example code style | `05-OPERATORS/03_bit_manipulation.cpp` (examples/ mein) |
| Deliberately broken file | `02-CPP-FIRST-STEPS/examples/06_broken_on_purpose.cpp` |
| Curriculum master plan | `00-START-HERE/02-full-curriculum-map.md` |

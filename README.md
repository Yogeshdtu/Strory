# CPP-MASTERY

**Zero se HFT tak — ek continuous C++ journey.**

Yeh repository ek complete learning course hai. Iska starting point yeh assume karta hai ki
aapne **kabhi programming nahi ki** — na C++, na Python, kuch bhi nahi. Aur iska ending point
yeh hai ki aap **low-latency / HFT-grade C++ systems** samajh sakein, likh sakein, debug kar
sakein, profile kar sakein aur optimize kar sakein.

Sab kuch **Hinglish** mein samjhaya gaya hai — technical terms English mein (kyunki industry
mein wahi use hote hain), explanation simple Hindi mein.

---

## Sabse pehle yeh padho

**Agar aap SEEKHNE aaye ho:**
→ **[`00-START-HERE/README.md`](00-START-HERE/README.md)**
Wahan se poora roadmap, setup guide aur "kis order mein padhna hai" — sab mil jayega.

**Agar aap Claude Code se yeh repo AAGE BANANA chahte ho:**
→ **[`START-WITH-CLAUDE-CODE.md`](START-WITH-CLAUDE-CODE.md)**
Setup, pehla prompt, aur working process.
(`CLAUDE.md` Claude Code khud padh leta hai — usme poora project brief hai.)

---

## Repository ka layout

```
CPP-MASTERY/
├── 00-START-HERE/            <- roadmap, setup, audits, glossary
├── 01-PROGRAMMING-BASICS/    <- PHASE 0: computer + programming basics
├── 02-CPP-FIRST-STEPS/       <- PHASE 1: pehla program, compilation pipeline
├── 03-VARIABLES-DATA-TYPES/  <- PHASE 2: core fundamentals
├── 04-INPUT-OUTPUT/
├── 05-OPERATORS/
├── 06-CONDITIONS/            <- PHASE 3: control flow
├── 07-LOOPS/
├── 08-FUNCTIONS/
├── 09-ARRAYS/                <- PHASE 4: arrays, strings, structs
├── 10-STRINGS/
├── 11-STRUCTS/
├── 12-POINTERS/              <- PHASE 5: pointers, references, memory
├── 13-REFERENCES/
├── 14-MEMORY/
├── 15-CLASSES/               <- PHASE 6: classes + OOP
├── 16-OOP/
├── 17-RAII/                  <- PHASE 7: resource management
├── 18-COPY-MOVE/             <- PHASE 8: copy/move semantics
├── 19-STL/                   <- PHASE 9: standard library
├── 20-ALGORITHMS-DSA/        <- PHASE 10
├── 21-TEMPLATES/             <- PHASE 11
├── 22-MODERN-CPP/            <- PHASE 12: C++11..C++23
├── 23-ERROR-HANDLING/        <- PHASE 13
├── 24-COMPILATION-LINKING/   <- PHASE 14: build systems
├── 25-OBJECT-MODEL/          <- PHASE 15: lifetime, UB, ABI
├── 26-CONCURRENCY/           <- PHASE 16
├── 27-ATOMICS-MEMORY-MODEL/  <- PHASE 17
├── 28-LOCK-FREE/
├── 29-LINUX-SYSTEMS/         <- PHASE 18
├── 30-NETWORKING/            <- PHASE 19
├── 31-CPU-ARCHITECTURE/      <- PHASE 20
├── 32-CACHE-MEMORY-PERFORMANCE/ <- PHASE 21
├── 33-COMPILER-OPTIMIZATION/ <- PHASE 22
├── 34-ASSEMBLY/
├── 35-PROFILING-BENCHMARKING/<- PHASE 23
├── 36-LOW-LATENCY-CPP/       <- PHASE 24
├── 37-HFT-FUNDAMENTALS/      <- PHASE 25
├── 38-MARKET-DATA/           <- PHASE 26
├── 39-ORDER-BOOK/            <- PHASE 27
├── 40-MATCHING-ENGINE/       <- PHASE 28
├── 41-HFT-CONCURRENCY/       <- PHASE 29
├── 42-HFT-NETWORKING/        <- PHASE 30
├── 43-HFT-OPTIMIZATION/      <- PHASE 31
├── 44-HFT-PROJECTS/          <- PHASE 32
├── 45-DEBUGGING/
├── 46-INTERVIEW-PREP/        <- PHASE 33
├── 47-CODING-PROBLEMS/
├── 48-CHEATSHEETS/
└── 49-PROJECTS/              <- end-to-end projects (beginner -> HFT)
```

Har folder mein:

- `00-README.md` — us folder ka syllabus + prerequisites + kya seekhoge
- `NN-topic.md` — theory files, numbered order mein
- `examples/` — chalne wale `.cpp` files, Hinglish comments ke saath

---

## Build karne ka tarika

```bash
# ek single file compile karo
g++ -std=c++20 -Wall -Wextra -g file.cpp -o file
./file

# ya Makefile use karo (root mein hai)
make FILE=01-PROGRAMMING-BASICS/examples/hello.cpp
```

Detail ke liye: [`00-START-HERE/03-setup-your-machine.md`](00-START-HERE/03-setup-your-machine.md)

---

## Current build status

Yeh repo **batches** mein banaya ja raha hai, kyunki poora content ek baar mein likhna
practically possible nahi hai (yeh ek 2000+ page ki book ke barabar hai).

| Batch | Folders | Status |
|-------|---------|--------|
| 1 | `00`, `01`, `02`, `03` — full lessons | ✅ **DONE** |
| 1 | Baaki sabhi folders — detailed syllabus README | ✅ **DONE** |
| 2 | `04`–`08` (I/O, operators, conditions, loops, functions) | ⏳ pending |
| 3 | `09`–`14` (arrays, strings, structs, pointers, refs, memory) | ⏳ pending |
| 4 | `15`–`18` (classes, OOP, RAII, copy/move) | ⏳ pending |
| 5 | `19`–`23` (STL, DSA, templates, modern C++, errors) | ⏳ pending |
| 6 | `24`–`28` (build, object model, concurrency, atomics, lock-free) | ⏳ pending |
| 7 | `29`–`35` (Linux, networking, CPU, cache, compiler, asm, profiling) | ⏳ pending |
| 8 | `36`–`44` (low-latency + full HFT track with real code) | ⏳ pending |
| 9 | `45`–`49` + final gap audit | ⏳ pending |

Live status yahan track hota hai: [`00-START-HERE/BUILD-STATUS.md`](00-START-HERE/BUILD-STATUS.md)

---

## License / usage

Personal learning ke liye banaya gaya hai. Jitna chaaho use karo, modify karo, notes add karo.

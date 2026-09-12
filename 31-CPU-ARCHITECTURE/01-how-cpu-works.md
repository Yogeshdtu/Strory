# 01 — CPU kaise kaam karti hai: fetch-decode-execute

## Prerequisites
- `29-LINUX-SYSTEMS` (kernel/userspace, ek program kaise chalta)
- `05-OPERATORS` (bits, binary)
- `24-COMPILATION-LINKING` (source → machine code)

## Yeh folder kyun
Ab tak CPU ek black box thi — tumne C++ likha, `machine code` bana, "CPU ne chala
diya". Ab uske **andar** dekhenge: pipeline, out-of-order execution, branch
prediction, SIMD, cache — kyunki HFT mein latency yahin ban ya bigad-ti hai. Har
cheez pehle **simple**, phir gehrai mein (spec ka rule), aur jahan possible ho
**measured** (examples).

## Yeh topic abhi kyun
Baaki har lesson isi loop ka koi optimization hai. Pehle base model: CPU
instructions ko **fetch → decode → execute → retire** karti hai, ek clock ke
taal pe.

---

## Transistor se instruction tak (bahut chhota version)

- **Transistor** = ek switch (on/off). Billions ek chip pe.
- Kuch transistors mila ke **logic gates** (AND/OR/NOT), gates mila ke **adder**,
  **multiplexer**, **register** (bits store), **ALU** (arithmetic-logic unit).
- Yeh sab mila ke ek **core**: instructions padhne, samajhne, aur chalane wali
  machinery.
- Ek **clock** signal (e.g. 3 GHz = 3 billion ticks/second) sab kuch synchronize
  karta — har tick pe kaam ek step aage badhta.

Tumhe transistor-level design nahi aana — par yeh samajhna ki **"ek instruction"
CPU ke andar kai chhote steps** hai, aur woh steps **overlap** ho sakte hain
(pipeline, file `04`), yeh poore folder ki neev hai.

---

## Von Neumann model: memory mein code + data

```
      +-----------+        address / data bus         +--------------+
      |   CPU     | <-------------------------------> |    MEMORY    |
      |  (core)   |                                    | code + data  |
      +-----------+                                    +--------------+
           |
      registers (chhoti, super-fast on-chip storage)
      cache (L1/L2/L3 — memory ki copy, paas)   <- folder 32
```

- **Code aur data ek hi memory mein** (von Neumann). CPU ko address se code
  fetch karna, aur alag address se data load/store karna.
- **Registers** (file `02`): ~16-32 general-purpose, har ek 64-bit, on-chip,
  0-cycle access. Yahin actual computation hota.
- **Cache** (folder 32): DRAM ~60-100 ns door hai; cache us data ki copy CPU ke
  paas rakhta (~1-40 cycles). "Memory wall" ka ilaaj.

---

## Fetch → Decode → Execute → Retire

Ek instruction ki zindagi (classic 5-stage view, real CPUs mein 15-20+ stages):

| Stage | Kya hota |
|---|---|
| **Fetch (IF)** | `RIP` (instruction pointer) jis address pe hai, wahan se instruction bytes memory/I-cache se le aao |
| **Decode (ID)** | bytes → CPU ka internal form (**µops**, file `03`); kaunse register, kaunsa operation |
| **Execute (EX)** | ALU/multiplier/load-store unit pe kaam karo (`add`, `imul`, `load`, `cmp`...) |
| **Memory (MEM)** | agar load/store hai to cache/memory access |
| **Writeback / Retire (WB)** | result register mein likho; instruction ko "done" mark karo (in program order) |

**Naive CPU:** ek instruction poore 5 steps kare, phir agli shuru — 5 cycles per
instruction. **Modern CPU:** pipeline (file `04`) se har cycle ek instruction
retire, aur superscalar (file `05`) se **kai per cycle**.

---

## Clock, frequency, aur "cycle" ka matlab

- **1 cycle** = ek clock tick. 3 GHz → cycle = 0.333 ns. 2 GHz → 0.5 ns.
- Instruction "cost" ko hum **cycles** mein sochte hain (frequency-independent),
  aur **ns** mein tab jab absolute latency chahiye. Convert: `ns = cycles /
  GHz`.
- **Example `08`** is box pe measure karta: ~2.0 GHz effective (yeh laptop
  power-save mein hai; ek HFT box locked ~3-5 GHz pe chalta — file `13`).
- **Frequency scaling** (turbo, throttling — file `13`) matlab "cycles" stable
  hain par "ns per cycle" badalta — isi liye HFT box frequency ko **lock** karta.

> **HFT relevance:** har hot-path function ka ek **cycle budget** hota. Ek
> strategy jo tick-to-trade ~300 ns target karti, 4 GHz pe = **~1200 cycles**.
> Us budget mein market data decode + book update + signal + order encode. Har
> cache miss ~200-400 cycles kha jaata (folder 32), har branch mispredict ~15-20
> (file `07`), ek `div` ~20-40 (file `09`). Isi liye yeh folder.

---

## x86-64 — hamara target ISA

- **x86-64** (aka AMD64, Intel 64): 64-bit extension of x86. Har trading desktop/
  server yahi (kuch ARM64 Graviton pe experiment karte). Iska instruction set
  file `03`.
- **CISC front-end, RISC-ish back-end:** x86 instructions variable-length aur
  complex, par CPU unhe andar chhoti fixed **µops** mein tod ke RISC-jaisa OoO
  core chalata (file `03`, `06`).
- **Registers** (file `02`): `rax..r15` (16 GP), `rip`, `rflags`, `xmm0..15` /
  `ymm` / `zmm` (SIMD, file `10`).

Assembly padhna folder `34` mein, par is folder ke examples mein hum `./build.ps1
asm` se snippets dekhenge (branch → `cmov`, vectorized loop → `paddd`, etc.).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — "ek instruction = ek cycle"
Kuch (`add`, `mov`) ~1 cycle **throughput**, par `imul` ~3, `div` ~20-40, ek
cache-missing `load` ~200-400. Aur "cycle" latency vs throughput alag (file `09`).

### Trap 2 — "GHz zyada = program tez" (linearly)
Sirf tab jab compute-bound. Memory-bound code (folder 32) frequency se almost
independent — woh DRAM latency pe wait kar raha, jo ns mein fixed hai.

### Trap 3 — "CPU instructions program order mein chalati"
Retire program order mein hota, par **execute out-of-order** (file `06`).
Debugging/reasoning ke liye yeh important.

### Trap 4 — code aur data ko alag "cheez" samajhna
Dono memory mein bytes. `.text` (code) I-cache mein, data D-cache mein, par ek
hi address space. JIT / self-modifying code isi wajah se possible (aur khatarnak).

### Trap 5 — laptop pe benchmark karke "yeh number production mein bhi milega"
Laptop frequency scale karti (2 GHz idle, 4 GHz turbo, thermal throttle). HFT box
frequency locked, C-states off (file `13`). Ratios port karo, absolute ns nahi.

---

## Hands-on

```bash
# example 07: yeh CPU kaun hai, kya kar sakta
./build.ps1 fast 31-CPU-ARCHITECTURE/examples/07_cpu_info.cpp

# example 08: is box ki effective frequency + cycle-accurate timing
./build.ps1 fast 31-CPU-ARCHITECTURE/examples/08_rdtsc_timing.cpp

# ek chhota loop ka machine code
printf 'int f(int a,int b){return a*b+1;}\n' > /tmp/t.cpp
g++ -std=c++20 -O2 -S -masm=intel /tmp/t.cpp -o - | grep -A3 'f(int'
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "ek instruction = ek cycle" | `add` ~1, `imul` ~3, `div` ~20-40, missing load ~200-400 |
| "CPU program order mein execute karti" | retire in-order, **execute OoO** |
| "GHz 2x = 2x tez" | sirf compute-bound; memory-bound ~frequency-independent |
| "code aur data alag storage" | ek address space; I-cache vs D-cache split |
| "laptop benchmark = production number" | frequency scaling; ratios only |

---

## Exercises

1. Ek CPU 3.2 GHz pe chal rahi. Ek `div` instruction ~26 cycles leti. Kitne
   nanoseconds? Aur agar frequency 4.5 GHz ho?

   <details><summary>Answer</summary>

   3.2 GHz: cycle = 1/3.2 ns = 0.3125 ns. 26 × 0.3125 = **8.125 ns**.
   4.5 GHz: cycle = 0.222 ns. 26 × 0.222 = **5.78 ns**. Cycles fixed rehte
   (instruction ki property), ns frequency ke saath ghat-ta. Isi liye HFT
   box frequency lock karta — warna latency non-deterministic.
   </details>

2. Tumhare tick-to-trade budget ~250 ns hai, box 4 GHz pe locked. Kitne cycles?
   Ismein ek L3 miss (~250 cycles) fit hoga?

   <details><summary>Answer</summary>

   250 ns × 4 = **1000 cycles**. Ek L3 miss (main-memory access, ~200-350
   cycles) us budget ka **20-35%** kha jaata — ek single miss aur tumhara
   budget tight ho gaya. Isi liye hot path ka working set L1/L2 mein rakhna
   (folder 32), aur data structures cache-friendly (folder 20/32). Missing
   loads hi HFT ka #1 latency source hain.
   </details>

3. `RIP` (instruction pointer) kya hold karta, aur ek branch (`jmp`/`je`) usko
   kaise affect karta?

   <details><summary>Answer</summary>

   `RIP` = agli fetch karne wali instruction ka address. Normal execution mein
   har instruction ke baad RIP us instruction ki length se aage badhta
   (sequential). Ek taken branch RIP ko target address pe set kar deta — aur
   yehi problem hai: CPU ko fetch ke waqt nahi pata branch taken hoga ya nahi,
   isi liye woh **predict** karta (file `07`). Galat predict → jo instructions
   speculatively fetch/execute hui woh discard, RIP sahi target pe, pipeline
   refill (~15-20 cycles).
   </details>

4. "CISC front-end, RISC back-end" — x86-64 ke context mein iska matlab?

   <details><summary>Answer</summary>

   x86-64 instructions variable-length (1-15 bytes) aur complex (`add [rax+rcx*4+8],
   rbx` ek instruction mein memory-load + add + store). CPU ka **decoder** har
   aisi instruction ko ek ya kai chhoti, fixed-form **µops** (micro-operations)
   mein todta — e.g. woh `add [mem], rbx` → load-µop, add-µop, store-µop. Andar
   ka **out-of-order core** in µops pe kaam karta jaise ek RISC machine
   (uniform, simple ops, register-register). Faayda: x86 backward compatibility
   + code density, par modern OoO execution ka benefit.
   </details>

5. Von Neumann architecture mein "code aur data ek memory" ka ek security
   implication batao (folder 29 se connect karo).

   <details><summary>Answer</summary>

   Agar attacker data (e.g. ek input buffer) mein bytes likhwa de aur phir CPU
   ko us data ko **code** ki tarah execute karwa de (buffer overflow → return
   address overwrite → jump into the buffer), to arbitrary code execution.
   Defenses: **W^X / NX bit** (page ya to writable ya executable, dono nahi —
   `mprotect`, folder 29), ASLR, stack canaries (folder 23/29). Isi liye
   JIT compilers ko carefully `mprotect(PROT_EXEC)` karna padta aur woh ek
   attack surface hain.
   </details>

---

## Interview questions

1. Fetch-decode-execute-retire — har stage kya karta?
2. "Cycle" kya hai, cycles vs nanoseconds — convert kaise, kaunsa
   frequency-independent?
3. Von Neumann model — code aur data ek memory, iska ek faayda + ek khatra.
4. x86-64 "CISC front-end, RISC back-end" — kya matlab?
5. Instructions execute out-of-order hoti hain par retire in-order — kyun dono?
6. GHz double karne se program hamesha 2x tez kyun nahi hota?
7. HFT tick-to-trade budget ko cycles mein sochna — ek L3 miss ka asar.

---

## Next
→ [`02-registers.md`](02-registers.md)

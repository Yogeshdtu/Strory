# 12 — Disassembly tools: `objdump -d`, `perf annotate`, `gdb disassemble`

## Prerequisites
- `02`–`11` of this folder
- `35-PROFILING-BENCHMARKING` (aage — `perf` ka full treatment)

## Yeh topic abhi kyun
Ab tak zyadatar `g++ -S` / `./build.ps1 asm` / godbolt (compiler ka
**output**). Par aap **compiled binary** ki asm bhi dekh sakte ho, source ke
saath interleaved, aur — sabse important — `perf annotate` se dekh sakte ho
**kaunsi instruction pe kitne CPU samples** the. Yeh lesson tools ka
reference hai.

---

## `g++ -S` — source → asm (no binary)

```bash
g++ -std=c++20 -O2 -S -masm=intel file.cpp -o -          # Intel, to stdout
g++ -std=c++20 -O2 -S            file.cpp -o file.s      # AT&T, to a file
g++ -std=c++20 -O2 -S -fverbose-asm file.cpp -o -        # + source-var comments
g++ ... -S ... | c++filt                                  # demangle names
./build.ps1 asm 34-ASSEMBLY/examples/<file>.cpp           # this repo: Intel, demangled, first 80 lines
```
Cleanest for "what does *this source* compile to". No linking, no other TUs.
For a real "what shipped" view (with inlining across TUs, LTO, the actual
linked addresses) → `objdump`.

---

## `objdump` — disassemble an object / binary

```bash
objdump -d --demangle a.out                        # all code, AT&T
objdump -d -M intel --demangle a.out               # Intel
objdump -dS --demangle -M intel prog               # <-- SOURCE INTERLEAVED (needs -g)
objdump -d --demangle prog | awk '/<_Z.*myfunc.*>:/,/^$/'   # one function
objdump -d -j .text.hot prog                       # only the hot section (PGO/-freorder)
objdump -drwC prog | less                          # -r relocs, -w wide, -C demangle
objdump -M intel -d --start-address=0x401a00 --stop-address=0x401b00 prog
```

- **`-dS`** is the killer feature: it prints each C++ source line followed by
  the machine instructions it became (requires `-g`, ideally
  `-g -fno-omit-frame-pointer` for cleaner mapping). This is how you answer
  "which line is that `idiv`".
- Works on `.o`, executables, `.so`/`.dll`, `.a` members.
- Shows **real** addresses and inlined code (unlike `-S`).

```bash
g++ -std=c++20 -O2 -g -c 34-ASSEMBLY/examples/02_reading_loops.cpp -o /tmp/x.o
objdump -dS -M intel --demangle /tmp/x.o | less
```

---

## `perf annotate` — asm with sample percentages (Linux)

```bash
perf record -g ./prog < workload            # sample the run
perf report                                 # top functions by CPU %
perf annotate                               # interactive: pick a function
perf annotate -s myfunc --stdio             # non-interactive, one function
perf annotate --stdio -l                    # + source lines
```

Output: each instruction with the **percentage of samples** that landed on
it. The instruction eating 40% of a hot function's time is right there —
usually an `idiv`, a missing load (`mov reg, [mem]` with a high %), a
mispredicted `jne` (samples on the instruction *after* the branch), or a
spill reload. This is the #1 production-triage tool (folder 35).

Caveats: samples attribute to the instruction *after* the slow one sometimes
(skid); with `-e cycles:pp` (precise) it's better. Needs symbols (`-g` or a
separate `.debug`) and, for stacks, frame pointers or `--call-graph
dwarf`/`lbr`.

---

## `gdb` — disassemble live / at a crash

```gdb
(gdb) set disassembly-flavor intel
(gdb) disassemble myfunc
(gdb) disassemble /s myfunc          # /s = interleave source (like objdump -dS)
(gdb) disassemble /r myfunc          # /r = also show raw bytes
(gdb) x/20i $pc                       # 20 instructions from the current PC
(gdb) info registers                  # register contents (at a breakpoint / crash)
(gdb) info frame                      # frame info
(gdb) stepi / nexti                   # step one instruction
```

At a crash: `disassemble` around `$pc`, `info registers` to see what was in
each register, map the faulting instruction back to a source line (`/s`),
reconstruct the bad value (folder 34 lesson 01, 08).

---

## `addr2line` — offset → source line

```bash
addr2line -e ./prog -f -C -i 0x401a2c
#   -f function name  -C demangle  -i show inlined frames
```
A backtrace / core dump gives `prog+0x1a2c`; `addr2line` (or `eu-addr2line`,
or `llvm-symbolizer`) turns it into `myfunc at file.cpp:123` (and the chain
of inlined frames). Needs `-g` (or debug info in a separate file).

---

## Others

| Tool | Use |
|---|---|
| **`llvm-mca`** | static pipeline / throughput / port-pressure estimate for an asm snippet (`g++ -S ... \| llvm-mca -mcpu=native`) — folder 31 lesson 09 |
| **`llvm-objdump`** | objdump-alike, sometimes nicer output, `--x86-asm-syntax=intel` |
| **`nm` / `readelf` / `size`** | symbols, sections, ELF headers, `.text` size |
| **`bloaty`** | what's taking space in the binary (by section/symbol/TU) |
| **`Compiler Explorer` (godbolt)** | interactive `-S` + diff + `llvm-mca` + opt-info; best for sharing |
| **`cutter` / `Ghidra` / `IDA`** | full reverse-engineering GUIs — overkill for "did my loop vectorize" |
| **Windows**: `dumpbin /disasm`, `WinDbg` `u`, `x64dbg` | MSVC/Windows equivalents |

---

## Which tool for which question

| Question | Tool |
|---|---|
| "what does this source compile to?" | `g++ -S -masm=intel` / `./build.ps1 asm` / godbolt |
| "what's in the shipped binary (post-inline/LTO)?" | `objdump -dS -M intel` |
| "which instruction is eating the time?" | `perf annotate` (`-e cycles:pp`) |
| "which source line is this instruction?" | `objdump -dS`, `gdb disassemble /s`, `addr2line` |
| "what was in the registers at the crash?" | `gdb` (`disassemble`, `info registers`) |
| "how many cycles / which port is the bottleneck?" | `llvm-mca` |
| "why is the binary so big?" | `size`, `bloaty` |
| "compare two compilers/versions" | godbolt diff view |

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `objdump -dS` with no `-g`
No source interleaving, no line info. Build the TU with `-g` (release too —
folder 33 lesson 01).

### Trap 2 — reading `-S` and thinking it's what shipped
`-S` is per-TU, pre-link. Inlining across TUs, LTO, and the linker's layout
change things. Use `objdump` on the final binary for ground truth.

### Trap 3 — `perf annotate` samples on the wrong instruction (skid)
Non-precise events attribute to an instruction a few past the real culprit.
Use `-e cycles:pp` / `:ppp` (precise) and look a couple of instructions back.

### Trap 4 — `perf` with no symbols / no stacks
`perf` shows hex addresses → build with `-g` (or ship a `.debug`), and for
call graphs use frame pointers or `--call-graph dwarf`/`lbr`.

### Trap 5 — godbolt's binary ≠ your binary
Different compiler build, different libc, `-march=native` = godbolt's CPU.
Great for source→asm shape; not for exact bytes.

### Trap 6 — disassembling optimized code and blaming the tool
`-O2` reorders, inlines, deletes. If a function "isn't there", it was
inlined into its caller. Look at the caller, or `[[gnu::noinline]]` it for
inspection.

---

## > **HFT relevance**

> - **`perf annotate` is the p99-triage tool** — a latency spike points at a
>   function; `annotate` points at the instruction; you read it (this folder)
>   to know if it's a `div`, a cache-missing load, a mispredicted branch, or
>   a spill.
> - **`objdump -dS` on the release binary** — verify the shipped code (with
>   real inlining/LTO) matches your intent, not just the per-TU `-S`.
> - **`addr2line` on every crash / core dump** — `prog+0x...` → the exact
>   line + inlined frames. Ship `-g` (stripped to a separate `.debug`).
> - **`llvm-mca` for critical-path budgeting** of a hot kernel (folder 31/09).
> - **Snapshot a godbolt link in the PR** for the hot function's asm — a
>   reviewer sees "still a 12-instruction vectorized loop" at a glance.

---

## Hands-on

```bash
# source-interleaved disasm of a folder example:
g++ -std=c++20 -O2 -g -c 34-ASSEMBLY/examples/02_reading_loops.cpp -o /tmp/x.o
objdump -dS -M intel --demangle /tmp/x.o | sed -n '/sum_matrix/,/ret/p'

# one function's raw asm from an executable:
g++ -std=c++20 -O2 -g 34-ASSEMBLY/examples/04_stack_frame.cpp -o /tmp/sf
objdump -d -M intel --demangle /tmp/sf | awk '/<factorial.*>:/,/^$/'

# addr2line round-trip:
nm /tmp/sf | grep factorial               # get an address
addr2line -e /tmp/sf -f -C <that address>

# llvm-mca (if installed):
g++ -std=c++20 -O2 -S 33-COMPILER-OPTIMIZATION/examples/03_vectorization.cpp -o - | llvm-mca -mcpu=native | head -40

# Linux perf (on a Linux box / WSL):
perf record -g -e cycles:pp ./prog ; perf annotate --stdio -s hotfunc
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`-S` shows what shipped" | per-TU, pre-link; use `objdump` on the binary |
| "`objdump -dS` works without `-g`" | needs debug info for source interleaving |
| "`perf annotate` % is exact per instruction" | skid — use `:pp`, look a couple back |
| "no symbols → perf is useless" | build `-g` / ship a `.debug`; `--call-graph` for stacks |
| "godbolt = my binary" | source→asm shape yes; exact bytes no |
| "function missing from disasm = tool broke" | it was inlined; look at the caller |

---

## Exercises

1. Ek crash ka backtrace hai: `myprog(+0x3f2a)` aur kuch nahi (no symbols).
   Tumhare paas `myprog` + `myprog.debug` hai. Source line kaise nikaloge?

   <details><summary>Answer</summary>

   `addr2line -e myprog.debug -f -C -i 0x3f2a` (or point it at `myprog` if
   the debug info is still in it; `-i` also lists inlined frames). If the
   crash address is a runtime address and the binary is PIE/ASLR'd, you first
   need the **load base** (from `/proc/<pid>/maps` at crash time, or the core
   dump) and subtract it to get the file offset, *then* `addr2line`. You can
   also `objdump -dS -M intel myprog.debug` and search for `3f2a:` in the
   left column to see the instruction + the interleaved source line
   directly, plus the surrounding context (which register held what, whether
   it's a `mov [reg]` that could fault on a bad pointer). `gdb myprog
   core` → `bt` does all of this for you if the core dump is available.
   </details>

2. `perf report` kehta `hotfunc` 60% time leta hai. `perf annotate -s
   hotfunc` mein ek `mov rax, [rbx+0x18]` pe 45% samples. Diagnosis?

   <details><summary>Answer</summary>

   That single load is a **cache miss** dominating the function. `[rbx +
   0x18]` — `rbx` is a callee-saved register holding a long-lived pointer
   (an object / node / iterator), and offset 0x18 (24) is a field within it.
   45% of the function's samples parked on this one load = the CPU is
   frequently **stalled waiting for that data to come from L2/L3/DRAM**
   (folder 32). Next steps: (1) is `rbx` walking a pointer chain (a
   `->next`/tree)? → flatten the structure (folder 32 lesson 08). (2) is the
   access strided / random over a big working set? → SoA / smaller types /
   blocking (folder 32 lessons 09/13/15). (3) can you **prefetch** the next
   `[rbx+0x18]` while processing the current item, if the address is known
   ahead? (folder 32 lesson 06 — measure, it's often marginal). (4) `perf
   stat -e mem_load_retired.l3_miss` to confirm it's DRAM. The asm told you
   *where*; folder 32 tells you *how to fix*.
   </details>

3. `g++ -S` ke output mein tumhari hot function nahi mil rahi. Binary ke
   `objdump -d` mein bhi standalone nahi hai. Kya hua, kaise dekho?

   <details><summary>Answer</summary>

   It was **inlined into all its callers** (and, being `static` / internal
   linkage / used only internally, its standalone body was then deleted —
   dead). At `-O2` a small hot function called from a few places typically
   ceases to exist as a separate symbol. To inspect its codegen: (1) look at
   the **caller's** disassembly — the function's instructions are now embedded
   there (`objdump -dS` on the caller shows the inlined source lines). (2)
   Temporarily add `[[gnu::noinline]]` to it and rebuild — now it's a
   standalone function you can read (and its call sites show a `call`). (3)
   On godbolt, mark it `[[gnu::used]]` / `extern` or reference it from a
   visible `main` so its body is emitted. (4) `-fopt-info-inline` tells you
   which call sites it was inlined at. Remember to remove the `noinline`
   before shipping.
   </details>

---

## Interview questions

1. `g++ -S` vs `objdump -d` — what each shows, when to use which.
2. `objdump -dS` — what `-S` adds, what it requires.
3. `perf annotate` — what the percentages mean, the skid caveat, `:pp`.
4. `gdb disassemble /s` and `info registers` — the crash-analysis workflow.
5. `addr2line` — input, output, `-i` for inlined frames.
6. `llvm-mca` — what it estimates that raw asm doesn't.
7. A hot function missing from the disassembly — why, and how to inspect it.

---

## Next
→ [`13-exercises.md`](13-exercises.md)

# 05 — Debugging optimized (`-O2`) builds

## Prerequisites
- `02-gdb-basics.md`, `04-debugging-crashes.md`
- `33-COMPILER-OPTIMIZATION` (inlining, constant folding, register allocation)
- `34-ASSEMBLY/01-why-read-assembly.md`

## Yeh topic abhi kyun

`-O0` pe debug karna aasaan hai — har variable memory mein, har line
alag, koi inlining nahi. Par:

1. **Bug sirf `-O2` pe hota hai** — optimizer ne UB expose kiya, ya
   timing badli (race). `-O0` pe reproduce hi nahi hota.
2. **Production `-O2`/`-O3` chalta hai** — crash ka core dump `-O2`
   binary ka hai.
3. **HFT mein `-O0` ka koi matlab nahi** — pura point hi speed hai; bug
   `-O0` pe manifest hi nahi hoga (10× dheema code alag timing).

To `-O2 -g` pe debug karna aana chahiye — uske saath jeena bhi.

Real transcripts `examples/01_gdb_practice.cpp` ke `-O2 -g` build se.

---

## `-O2 -g` — dono saath de sakte ho

`-g` **code generation ko nahi badalta**. Yeh sirf debug info (DWARF)
add karta — line tables, variable locations, type info. `-O2 -g` = poori
optimization + jitna debug info bacha paaye. Isse binary bada hota (`.debug_*`
sections), **slow nahi**. Production release: `-O2 -g` compile, phir
`objcopy --only-keep-debug` se debug info alag file mein, binary strip.

```bash
g++ -O2 -g prog.cpp -o prog
objcopy --only-keep-debug prog prog.debug
objcopy --strip-debug --add-gnu-debuglink=prog.debug prog
# ship `prog`; archive `prog.debug` (commit hash ke saath)
```

---

## Kya "toot" jaata hai `-O2` pe

### 1. `<optimized out>` — variable ki jagah hi nahi

```
(gdb) info locals
sum = <optimized out>
```
Variable ek register mein tha jo ab kuch aur hold karta, ya use hi nahi
hua (dead), ya constant mein fold ho gaya. DWARF location list keh sakti
"line 10-14 tak `rbx` mein, 15-20 tak spilled, phir gone". Us line pe
agar woh register kuch aur hai → `<optimized out>`.

### 2. Values galat/garbage function ke shuru mein — real transcript

```
(gdb) break sum_balances
Breakpoint 1 at 0x1400090bf: sum_balances. (2 locations)    # <-- 2 locations!

(gdb) run
Thread 1 hit Breakpoint 1.1, sum_balances (
    accts=std::vector of length 364181, capacity 0 = {...})  # <-- GARBAGE
    at 01_gdb_practice.cpp:43

(gdb) info locals
accts = ... {<error reading variable accts (Cannot access memory at address 0x0)>
i = 0
total = 0
```

`accts` = "length 364181, capacity 0" — bakwaas. Kyun: breakpoint function
ke **pehle instruction** pe laga, par `-O2` pe prologue arguments ko abhi
register/stack pe settle nahi kiya, aur gdb DWARF ko literally padh raha.
Thodi der baad (`next` ke baad) `total`/`i` sahi padhe gaye (`$1 = 500000`).

**Sabak:** `-O2` pe breakpoint ke turant baad ke variable values pe
bharosa mat karo. 1-2 `next` chalao, phir padho. Ya breakpoint aage lagao
(`break 44`, function-start nahi).

### 3. "2 locations" — function clone / partial inline

`Breakpoint 1 ... (2 locations)` — `-O2` ne `sum_balances` ko do jagah
generate kiya (ek inlined copy `main` mein, ek standalone; ya
`.constprop`/`.isra` clone). `info breakpoints` → `1.1`, `1.2`. Dono pe
rukega. Specific chahiye → `break 44` (line, function nahi).

### 4. Inlined functions — `bt` mein `(inlined)` ya gayab

```
(gdb) break factorial
Function "factorial" not defined.
```

`factorial(5)` ko `-O2` ne **compile-time pe 120 mein fold** kar diya —
function binary mein hai hi nahi. `printf("factorial(5) = %ld", 120)`.
Yeh constant folding hai (`33-COMPILER-OPTIMIZATION`).

Jo functions inline hue par eliminate nahi, `bt` mein aise:
```
#0  leaf_fn (x=3) at m.cpp:5
#1  middle_fn (...) at m.cpp:12         # "(inlined)" ho sakta
#2  0x... in caller () at m.cpp:40
```
gdb DWARF `DW_TAG_inlined_subroutine` se inline frames **reconstruct**
karta — par sab nahi. `bt` mein ek function "missing" lage to woh inline
ho gaya, use frame #1 aur #2 ke beech socho.

### 5. Line numbers "koodte" hain

`next` maaro, control line 20 → 15 → 22 → 15 → 23. Optimizer ne
instructions **reorder** kar diye (scheduling), ek source line ke
instructions poori jagah bikhre. `step` do baar same line pe ruk sakta
(loop pieces). Debugging feels drunk.

---

## `-Og` — "optimize for debugging"

`-Og` = woh optimizations jo **debugging ki experience nahi todte**:
inlining kam, variable locations stable, line tables sane — par `-O0` se
kaafi tez (dead code elim, basic reg alloc, simple folding).

Real transcript (`-Og`):
```
(gdb) break sum_balances
Breakpoint 1 at 0x1400017b5: file 01_gdb_practice.cpp, line 43.   # 1 location

(gdb) run
Thread 1 hit Breakpoint 1, sum_balances (accts=std::vector of length 3, capacity 3 = {...})

(gdb) info locals
i = 0
total = 0                    # readable, sahi

(gdb) next ; next ; print total
$1 = 500000                  # sane
```

`-Og` sweet spot jab: bug `-O0` pe gayab ho jaata (kuch timing/UB) par
`-O2` pe debug karna dard hai. **Try `-Og` first.** Note: `-Og` ≠ `-O2` —
ho sakta bug yahan bhi na dikhe. Tab `-O2 -g` hi.

| Flag | Speed | Debug experience | Kab |
|---|---|---|---|
| `-O0 -g` | 1× (slow) | Perfect | Default dev; logic bugs |
| `-Og -g` | ~4-6× | Good | Bug `-O0` pe gayab; still want sanity |
| `-O2 -g` | ~10× | Painful but possible | Prod crash core; bug only at `-O2` |
| `-O2` (no `-g`) | ~10× | `?? ()` only | Never ship this for a service |

---

## `-O2` pe survive karne ki techniques

1. **`-fno-omit-frame-pointer`** — `-O2` frame pointer (`rbp`) ko general
   register bana leta. Yeh flag use `bt` ke liye chain rakhta. HFT prod
   builds aksar isse rakhte (perf `--call-graph fp` ke liye bhi —
   `35-PROFILING`). Cost: 1 register kam, usually <1%.

2. **`volatile` ek probe variable** — `volatile long probe = suspect_value;`
   optimizer ise register/fold nahi kar sakta, gdb mein hamesha readable.
   Debug ke baad hatao.

3. **`__attribute__((noinline, noipa))`** ek function pe — us ek function
   ko standalone rakho taaki `bt`/`break` clean rahe, baaki `-O2`.

4. **`__builtin_trap()`** — `if (bad_condition) __builtin_trap();` →
   turant SIGILL exactly wahan, zero steady-state cost, `-O2` pe bhi
   reliable. Conditional-breakpoint-in-hot-loop ka replacement (`03`).

5. **Assembly padho** — `-O2` pe source-level debugging jhoot bolti hai;
   `layout asm`, `disassemble`, `stepi`/`nexti` (instruction step),
   `info registers`. `34-ASSEMBLY` isi ke liye tha.

6. **`print/x $rdi`, `$rsi` ...** — arguments SysV ABI mein
   `rdi,rsi,rdx,rcx,r8,r9` (`31-CPU` / `34-ASSEMBLY`). `<optimized out>`
   argument bhi function-entry pe register se mil jaata.

---

## UB jo sirf `-O2` pe crash karta — pehchano

`-O0` clean, `-O2` crash/wrong ka **matlab lagbhag hamesha UB hai**, compiler
bug nahi (`01` trap 1). Optimizer ne "yeh UB kabhi nahi hoga" maan ke code
kaat/reorder kiya.

| `-O2`-only symptom | Sambhavit UB | Confirm |
|---|---|---|
| Null check "gayab" ho gaya, phir crash | pointer deref **before** the null check → compiler ne check dead maana | `-fsanitize=undefined`, review |
| Loop kabhi terminate nahi karta | signed integer overflow in loop var (UB → "can't overflow" → infinite) | `-fsanitize=undefined`, `-fwrapv` test |
| Garbage read | uninitialized variable; `-O0` pe zero-ish stack, `-O2` pe live garbage | `-fsanitize=memory` (Clang), `-Wmaybe-uninitialized -O2` |
| Wrong result after a cast | strict aliasing violation | `-fno-strict-aliasing` test; `memcpy`/`bit_cast` fix |
| Crash near an `memcpy`/SIMD | misaligned load `-O2` vectorized | `-fsanitize=alignment` |

**Workflow:** bug `-O2`-only → `-fsanitize=undefined,address` (`-O1` ya
`-O2`) → aksar exact line. Clean? `-fno-strict-aliasing` / `-fwrapv` /
`-fno-delete-null-pointer-checks` ek-ek try — jo "fix" kare woh UB ki
class batata (par **asli fix** UB hatao, flag nahi).

> **HFT relevance:** ek order-book optimization jo `-O2` pe 12% tez thi
> par ek strict-aliasing violation pe khadi thi — 6 mahine baad compiler
> upgrade pe silently wrong prices. `-fsanitize=undefined` CI mein har
> commit pe (ek slow build target) yeh pehle din pakadta. `43/16` E2 isi
> ka judgement exercise hai.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `-O2` pe function-entry values pe bharosa
"length 364181" jaisi garbage. Prologue abhi complete nahi. `next` 1-2
baar, phir `info args`/`print`.

### Trap 2 — "Variable `<optimized out>` hai, gdb toota"
gdb theek hai — woh value uss point pe **exist hi nahi karti** kisi
padhne-yogya jagah. `-Og` try karo, ya `volatile` probe, ya aas-paas ki
line jahan woh live ho.

### Trap 3 — `bt` short dikh raha, frames missing
Inlined. gdb kuch reconstruct karta (`DW_TAG_inlined_subroutine`), sab
nahi. `-fno-inline` / `-Og` se full stack, ya assembly + return-address
se manually.

### Trap 4 — `-O0` pe reproduce na hone par "compiler bug"
Nahi. `-fsanitize=undefined,address`. 99% cases: tumhara UB jo `-O0` pe
"kaam kar raha tha".

### Trap 5 — Core dump `-O2` ka, gdb ko `-O0` binary diya
Symbols/lines match nahi karenge — garbage `bt`. **Exact** build (commit
+ flags) chahiye.

### Trap 6 — `-Og` ko "same as -O2" maan ke ship karna
`-Og` ≠ `-O2` performance. Prod pe `-O2`/`-O3`. `-Og` sirf ek **debugging
build** hai.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`-g` code slow karta" | `-g` sirf debug info; codegen same. Slow = `-O0`. |
| "`-O2` pe debug impossible" | Painful, not impossible. `-Og`, frame-pointer, asm, `volatile` probe. |
| "`-O0` pe theek = code sahi" | `-O0` pe UB chhup sakti. `-O2`+sanitizers real test. |
| "`<optimized out>` = gdb bug" | Value genuinely not stored there. Different build/line/probe. |
| "prod pe `-O0` ship karo debug ke liye" | 10× slow; timing bugs vanish; SLA miss. `-O2 -g` + archived debuginfo. |

---

## Hands-on

```bash
g++ -std=c++20 -g -O2 45-DEBUGGING/examples/01_gdb_practice.cpp -o /tmp/p_O2
g++ -std=c++20 -g -Og 45-DEBUGGING/examples/01_gdb_practice.cpp -o /tmp/p_Og

gdb -q /tmp/p_O2 -ex "break factorial" -ex quit     # "not defined" -- constant folded
gdb -q /tmp/p_O2
  (gdb) break sum_balances
  (gdb) run
  (gdb) info args         # garbage
  (gdb) next
  (gdb) info locals       # ab thoda sane
gdb -q /tmp/p_Og
  (gdb) break sum_balances ; run ; info locals   # clean, like -O0
```

---

## Exercises

1. `-O0` pe program 42 deta hai (sahi), `-O2` pe kabhi 42 kabhi garbage.
   Kya class ka bug, aur pehla diagnostic?
   <details><summary>Answer</summary>
   **Undefined behaviour** (compiler bug nahi) — sabse sambhavit
   uninitialized variable ya signed overflow ya strict-aliasing.
   `-fsanitize=undefined,address` (`-O1`/`-O2` ke saath) — aksar exact
   file:line. `-O0` pe stack "sanyog se" zero-ish tha.
   </details>

2. `bt`:
   ```
   #0  hot_kernel (i=<optimized out>) at k.cpp:20
   #1  0x... in run () at k.cpp:55
   ```
   Tumhe `i` ki value chahiye line 20 pe. Teen tareeke.
   <details><summary>Answer</summary>
   (a) `-Og` ya `-fno-inline -O1` se rebuild, phir `i` readable. (b)
   `volatile int probe = i;` line 20 pe, rebuild. (c) `layout asm` +
   `disassemble` — dekho `i` kaunse register/stack-slot mein map hua us
   PC pe (DWARF loc-list `info address i` bhi bataata), phir `print
   $r14` / `p *(int*)($rsp+12)`. (d) `__builtin_trap()` on `i==suspect`.
   </details>

3. Prod service `-O2` pe crash, core dump mila. Tumhare paas `-O2 -g`
   wala binary **nahi**, sirf `-O2` (stripped) aur source at that commit.
   Kya kar sakte ho?
   <details><summary>Answer</summary>
   Same commit + **same flags** se `-O2 -g` rebuild karo → `prog.debug` →
   `gdb prog core` + `add-symbol-file`/`set debug-file-directory`. Codegen
   `-g` se nahi badalta, to naye `-g` binary ke symbols purane core pe fit
   ho jaate (addresses match). Isliye "commit + exact flags record karo"
   itna zaroori hai.
   </details>

4. `-fno-omit-frame-pointer` prod `-O2` build mein add karne ka trade-off?
   <details><summary>Answer</summary>
   **Cost:** ek callee-saved register (`rbp`) general use se hat jaata hai — typically
   <1% (kabhi 2-3% register-pressure-heavy kernels mein). **Benefit:**
   `bt` bina DWARF CFI ke kaam karta, `perf --call-graph fp` sasta aur
   reliable (vs `dwarf` unwind jo mehnga), production profiling +
   crash triage dono behtar. Zyada HFT/infra teams isse rakhte hain.
   </details>

---

## Interview questions

1. `-g` performance ko affect karta hai? `-O2 -g` valid hai?
2. `<optimized out>` ka matlab exactly kya? gdb ka bug hai?
3. `-Og` kya karta jo `-O2` nahi? Kab use karoge?
4. "Bug sirf `-O2` pe" — 90% cases mein asli wajah? Kaise confirm?
5. Inlined function `bt` mein kaise dikhta (ya nahi dikhta)? gdb use
   kaise reconstruct karta?
6. Prod `-O2` service ke liye debuggability kaise banaye rakhoge (3
   concrete steps)?

---

## Next
→ [`06-sanitizers-practical.md`](06-sanitizers-practical.md)

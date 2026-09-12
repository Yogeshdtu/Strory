# 04 — Debugging crashes: segfaults, signals, core dumps

## Prerequisites
- `02-gdb-basics.md` (`run` / `bt` / `print` / `frame`)
- `12-POINTERS/12-dangling-pointers.md`, `14-MEMORY/06-use-after-free.md`
- `29-LINUX-SYSTEMS/04-signals.md` (SIGSEGV/SIGABRT signals hain)

## Yeh topic abhi kyun

Crash = sabse **aasaan** bug type, kyunki process khud ruk jaata hai aur
tumhe exact jagah deta hai (ya de sakta hai). Silent wrong-answer bugs
usse mushkil hain. Yeh lesson: crash ko ek **address + backtrace** mein
badalna, aur "kaise pahunche" reconstruct karna.

`examples/02_segfault_debug.cpp` — deliberate crash, real transcript
neeche.

---

## Crash ka matlab: ek signal aaya

Program "crash" tab karta jab OS use ek **signal** bhejta jise woh handle
nahi karta (`29-LINUX-SYSTEMS/04`):

| Signal | Number | Kab | Aam wajah |
|---|---|---|---|
| **SIGSEGV** | 11 | invalid memory access | null/dangling pointer, OOB, stack overflow |
| **SIGABRT** | 6 | `abort()` call hui | failed `assert`, `std::terminate` (uncaught exception), glibc "double free / corruption detected", `__stack_chk_fail` |
| **SIGFPE** | 8 | bad arithmetic | integer `/ 0` ya `INT_MIN / -1` (float `/0` = inf, **no** SIGFPE by default) |
| **SIGBUS** | 7 | misaligned / bad mapping | unaligned atomic, `mmap` region shrink, bad `char*` cast |
| **SIGILL** | 4 | illegal instruction | corrupted function pointer, jumped into data, `__builtin_trap()` |

Exit code (shell): `128 + signal`. `139 = 128 + 11` (SIGSEGV),
`134 = 128 + 6` (SIGABRT). `echo $?` yeh turant batata.

---

## SIGSEGV ko debug karna — real transcript

`02_segfault_debug.cpp`: ek linked-list sum jisme loop ki terminating
condition hi nahi hai.

```
$ ./sf
summing list...
Segmentation fault

$ gdb -q .build/02_segfault_debug.exe
(gdb) run
Thread 1 received signal SIGSEGV, Segmentation fault.
0x... in sum_list (head=0x5ffe30) at 02_segfault_debug.cpp:39
39          total += p->value;

(gdb) print p
$1 = (const Node *) 0x0            # <-- p null hai

(gdb) print total
$2 = 60                            # <-- 10+20+30 -- loop teeno node ghoom chuka

(gdb) backtrace
#0  sum_list (head=0x5ffe30) at 02_segfault_debug.cpp:39
#1  main (argc=1) at 02_segfault_debug.cpp:54
```

**Reconstruction:** `total == 60` = 10+20+30. Matlab loop ne teeno valid
nodes add kar liye, phir `p = c.next = nullptr` ho gaya, aur agli iteration
mein `p->value` = **null dereference**. Crash line 39 pe hai; **bug** line
38 pe hai (`for (const Node* p = head; ; p = p->next)` — condition khaali).

Yeh `01`-`03` ka mindset: symptom (line 39) ≠ root cause (line 38). Locals
se raasta reconstruct karo.

### Fault address padhna

```
Cannot access memory at address 0x0        -> null pointer
... at address 0x8                          -> null->second_member (offset 8)
... at address 0x7ffff7a3d000               -> valid-looking -> dangling / OOB
... at address 0xdeadbeef / 0xbebebebe      -> uninitialized pointer (garbage)
... address == $sp roughly                  -> stack overflow (neeche)
```
`0x0` aur chhote addresses (`< 0x1000`, "null page") = null pointer se
member access. Bada plausible address = pointer kabhi valid tha, ab nahi
(use-after-free — `06`, `14-MEMORY/06`).

---

## SIGABRT — assert, terminate, heap corruption

```
(gdb) run
prog: prog.cpp:88: int at(std::vector<int>&, size_t): Assertion `i < v.size()' failed.

Thread 1 received signal SIGABRT, Aborted.
0x... in raise ()
(gdb) bt
#0  raise ()
#1  abort ()
#2  __assert_fail (...)
#3  at (v=..., i=10) at prog.cpp:88        # <-- yahan se dekho: i=10, size=?
#4  main () at prog.cpp:102
```

`bt` mein pehle 3 frames (`raise`/`abort`/`__assert_fail`) noise hain —
**pehla tumhara frame** (#3 yahan) asli info hai. `frame 3` → `print
v.size()` → assertion `i < v.size()` kyun toota.

**Uncaught exception:**
```
terminate called after throwing an instance of 'std::out_of_range'
  what():  vector::_M_range_check: __n (which is 10) >= this->size() (which is 3)
Thread 1 received signal SIGABRT
```
`catch throw` (`03`) se **throw ke waqt** pakdo — tab `bt` woh function
dikhayega jisne `.at(10)` call kiya, `std::terminate` nahi.

**Heap corruption:**
```
free(): invalid pointer
malloc(): corrupted top size
double free or corruption (fasttop)
```
Yeh glibc ka **detection** hai — corruption **pehle** hua (koi OOB write
ne heap metadata tod diya), bas ab pakda gaya. `bt` sirf yeh dikhata ki
kis `free()` pe pakda — **kisne toda woh nahi**. Iske liye ASan/valgrind
(`06`, `07`).

---

## Core dumps — post-mortem debugging

Production server 3am pe crash hua, tum so rahe the. Core dump = crash ke
**exact moment** ka memory snapshot — subah gdb mein khol lo.

### Enable karna (Linux)

```bash
ulimit -c unlimited                 # is shell ke liye core size limit hatao
cat /proc/sys/kernel/core_pattern   # kahan/kaise likhega
# aam distros: systemd-coredump -> `coredumpctl`
./prog                              # -> "Segmentation fault (core dumped)"
```

### Kholna

```bash
gdb -q ./prog core                  # classic: executable + core file
gdb -q ./prog $(coredumpctl -1 --field COREFILE)   # systemd
coredumpctl debug                   # sabse aasaan (gdb khud khol deta)

(gdb) bt full                       # backtrace + har frame ke saare locals
(gdb) thread apply all bt           # multi-threaded crash: sabki stacks
(gdb) info registers
(gdb) frame 2
(gdb) print some_local
```

Core dump ke andar **koi live process nahi** — tum `continue`/`next`/`step`
nahi kar sakte, `print` nahi jo function call kare. Sirf **inspect**: jo
state crash pe thi, wahi. Isi liye:

- Binary **bilkul wahi** hona chahiye jo core banaya (same build, same
  `-O`, same commit). Alag → `bt` mein garbage line numbers.
- `-g` chahiye (ya separate debug info: `objcopy --only-keep-debug`,
  `gdb` mein `set debug-file-directory`).
- Symbol mismatch → gdb warning: "core file may not match specified
  executable file".

> **HFT relevance:** trading systems `ulimit -c unlimited` ke saath chalte
> aur core ko ek dedicated fast disk pe likhte (`core_pattern`). Bade
> RSS (100+ GB) pe full core slow — `coredump_filter` se sirf anonymous +
> modified pages, ya `gcore -o snap <pid>` se on-demand snapshot bina
> process maare. Har crash ka core + build artefact archive hota;
> post-mortem `bt full` + `info registers` se root cause, kyunki live
> reproduction aksar impossible (market data replay ke bina).

### Windows note

Windows pe `ulimit -c` nahi. Options:
- **WER (Windows Error Reporting)** — registry se `LocalDumps` enable →
  `%LOCALAPPDATA%\CrashDumps\prog.exe.<pid>.dmp`.
- `gdb ./prog.exe` → `run` — MinGW gdb crash pakadta live (upar transcript
  isi tarah bana).
- `procdump -e -ma prog.exe` (Sysinternals) → `.dmp` → `gdb prog.exe
  dump.dmp` ya WinDbg.
- **HFT prod Linux pe hota** — Windows crash-dump workflow yahan secondary
  hai.

---

## Stack overflow — ek khaas SIGSEGV

```cpp
void recurse(int n) { char buf[4096]; recurse(n + 1); (void)buf; }   // no base case
```
```
Thread 1 received signal SIGSEGV
#0  recurse (n=261870) at m.cpp:1
#1  recurse (n=261869) ...
#2  recurse (n=261868) ...
... (thousands of identical frames)
```

Clues: (1) `bt` mein hazaaron same frame, (2) fault address `$sp` ke
paas, (3) `n` bahut bada. Wajah: infinite/deep recursion, ya ek function
mein bahut bada local array (`char buf[8*1024*1024]` — default stack
8 MB, `ulimit -s`). Fix: base case; bade buffers heap pe; `-fstack-usage`
se per-function stack dekho; explicit stack (loop + `std::stack`) instead
of recursion. Detail: `08-FUNCTIONS/05-the-call-stack.md`,
`14-MEMORY/02-stack-deep-dive.md`.

---

## `gdb` signal handling

```
(gdb) info signals SIGSEGV
Signal   Stop  Print  Pass to program
SIGSEGV  Yes   Yes    Yes

(gdb) handle SIGPIPE nostop noprint pass     # SIGPIPE pe mat ruko (servers)
(gdb) catch signal SIGSEGV                   # ya explicitly catch
```
Kuch programs signals ko normally use karte (SIGPIPE, SIGCHLD, SIGUSR1).
gdb default mein sab pe rukta — irritating. `handle ... nostop noprint
pass` un signals ko "let through" kar deta.

Crash pe, `pass`/`nopass` matter karta agar program ka apna signal
handler hai: `nopass` se handler skip hoke tumhe raw crash-site milta.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — Crash line ko bug line maan lena
`02_segfault_debug`: crash line 39, bug line 38. Locals (`total==60`) se
reconstruct karo. "Kaise pahunche" hamesha poocho.

### Trap 2 — Core file executable se match nahi
Alag build ka binary + core → `bt` galat symbols/lines. Har release ke
saath uska **stripped binary + separate `.debug`** archive karo, commit
hash ke saath.

### Trap 3 — `bt` ke top frames pe time waste
`raise` / `abort` / `__assert_fail` / `__cxa_throw` / `__libc_message` —
yeh library plumbing hai. **Pehla frame jo tumhari file ka hai** wahan se
socho.

### Trap 4 — "double free detected" wale `free` ko doshi maanna
glibc ne wahan **pakda**, toda nahi. Corruption pehle kisi OOB write se
hua. `bt` sirf detection point deta. ASan/valgrind se asli culprit.

### Trap 5 — `ulimit -c unlimited` set kiya, core nahi bana
Wajah: (a) core `core_pattern` ke hisaab se kahin aur gaya
(`coredumpctl list`), (b) process ne `setuid`/`prctl(PR_SET_DUMPABLE, 0)`
kiya, (c) disk full / no write perm us dir mein, (d) container mein host
ka `core_pattern` apply hota. Check: `cat /proc/sys/kernel/core_pattern`.

### Trap 6 — float `/0` SIGFPE expect karna
IEEE float `1.0/0.0` = `inf`, koi signal nahi (jab tak `feenableexcept`
na ho). SIGFPE **integer** `/0` (ya `INT_MIN / -1`) pe. `23-ERROR/13`.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Segfault = pointer null" | Ho sakta OOB, dangling, stack overflow, bad cast. Address padho. |
| "Crash line = bug" | Symptom. `bt` + locals se root cause reconstruct. |
| "core dump = live debugging" | Sirf inspect — no step/continue/call. Snapshot hai. |
| "SIGABRT = mera abort() nahi" | assert, uncaught exception, glibc heap check, stack canary — sab abort() karte |
| Exit code 139 random hai | `128 + 11` = SIGSEGV. Har signal ka fixed code. |

---

## Hands-on

```bash
./build.ps1 build 45-DEBUGGING/examples/02_segfault_debug.cpp
.build/02_segfault_debug.exe ; echo "exit=$?"        # 139
gdb -q .build/02_segfault_debug.exe
  (gdb) run
  (gdb) bt
  (gdb) print p            # 0x0
  (gdb) print total        # 60  -> loop poora ghoom chuka
  (gdb) frame 1            # main -- kaunse head se call hua
```
Linux/WSL pe: `ulimit -c unlimited; ./sf; gdb ./sf core; bt full`.

---

## Exercises

1. `echo $?` deta hai `136`. Kaunsa signal, aur do sambhavit wajah?
   <details><summary>Answer</summary>
   `136 - 128 = 8` = **SIGFPE**. Integer division by zero, ya `INT_MIN / -1`
   (overflow, bhi UB → SIGFPE on x86). `gdb` → `bt` → us `idiv` line pe
   divisor print karo.
   </details>

2. `bt` mein 40,000 frames, sab `parse_expr (depth=...)`, fault address
   `$sp` ke bilkul paas. Diagnosis + 2 fixes.
   <details><summary>Answer</summary>
   **Stack overflow via unbounded recursion** (deeply-nested / malicious
   input). Fixes: (a) recursion depth limit (`if (depth > 5000) throw`),
   (b) recursion → explicit `std::stack` + loop, (c) short-term:
   `ulimit -s` badhao (band-aid, DoS abhi bhi possible).
   </details>

3. Core dump khola: `bt` mein `?? ()` aur `<optimized out>` har jagah,
   gdb bola "core file may not match executable". Kya galat, kaise fix?
   <details><summary>Answer</summary>
   Jo binary gdb ko diya woh core banane wale se alag (alag commit / `-O`
   / rebuild). Fix: **exact** binary (same commit hash, same flags)
   restore karo — CI artefacts se, ya `git checkout <hash> && rebuild
   with recorded flags`. Ismein se lesson: release binaries + debug info
   ko commit hash ke saath archive karo.
   </details>

4. `free(): invalid pointer` pe crash. `bt` `delete_node()` ko point
   karta. Wahan `delete p;` bilkul theek dikhta (`p` valid, ek hi baar).
   Ab kya?
   <details><summary>Answer</summary>
   Corruption **pehle** hua — kisi ne is heap block ke aas-paas OOB
   likha, `free`'s chunk metadata corrupt ho gaya, ab yeh `delete` uspe
   trip kar raha. Yeh `delete` "victim" hai, culprit nahi. Tool badlo:
   `-fsanitize=address` (Linux/Clang) ya `valgrind --tool=memcheck` —
   woh **likhne wali** line dega. `06`, `07`.
   </details>

---

## Interview questions

1. SIGSEGV vs SIGABRT vs SIGBUS — ek trigger example har ek ka.
2. Shell exit code `139` ka matlab? Formula?
3. Core dump se kya kar sakte ho aur kya nahi (vs live gdb)?
4. "double free or corruption detected" — yeh message kyun aksar asli
   bug se door hota hai?
5. Stack overflow ko `bt` mein kaise pehchanoge? Fix strategies?
6. Production Linux service ke liye crash-dump pipeline kaise setup
   karoge? (core_pattern, ulimit, binary+debuginfo archive, symbol
   server.)

---

## Next
→ [`05-debugging-optimized.md`](05-debugging-optimized.md)

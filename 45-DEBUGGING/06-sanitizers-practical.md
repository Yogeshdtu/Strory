# 06 — Sanitizers: ASan, UBSan, TSan, MSan

## Prerequisites
- `14-MEMORY` (heap, stack, leaks, UAF)
- `25-OBJECT-MODEL/15-undefined-behaviour-catalog.md`
- `27-ATOMICS-MEMORY-MODEL/01-data-race-definition.md`
- `35-PROFILING-BENCHMARKING/15-sanitizers.md` (yeh uska practical companion)

## Yeh topic abhi kyun

gdb tumhe **ek** bug pe le jaata jab tum use dhoondh chuke ho. Sanitizers
alag hain: woh program ko **instrument** karte aur bug ko **jaise hi woh
hota hai** pakadte — exact line, plus allocation/free/other-thread ki
line. Memory aur concurrency bugs ke liye yeh sabse fast raasta hai.

**Rule:** naya C++ code likhte ho → CI mein ek `-fsanitize=address,undefined`
build target rakho. din-ba-din yeh chal jaaye to 90% memory/UB bugs merge
se pehle mar jaate.

> **Is box pe:** MinGW-w64 pe `libasan`/`libubsan`/`libtsan` nahi aate —
> `build.ps1 san` link fail karke `-D_GLIBCXX_ASSERTIONS + -fstack-protector`
> pe fall back hota. Asli sanitizer workflow **Linux (g++/clang++)** ya
> **WSL** pe. `examples/03_memory_bugs.cpp` aur `04_race_debug.cpp` ke
> neeche har mode ka **expected Linux sanitizer output** diya hai.

---

## Ek nazar mein

| Sanitizer | Flag | Kya pakadta | Slowdown | RAM | Compiler |
|---|---|---|---|---|---|
| **ASan** | `-fsanitize=address` | heap/stack/global OOB, UAF, use-after-return/scope, double-free, leaks (LSan) | ~2× | ~3× | gcc, clang |
| **UBSan** | `-fsanitize=undefined` | signed overflow, `/0`, bad shift, misaligned, null deref, bad enum/bool, `-fsanitize=bounds` | ~1.2× | ~1× | gcc, clang |
| **TSan** | `-fsanitize=thread` | data races, lock-order inversion, some deadlocks | 5–15× | 5–10× | gcc, clang |
| **MSan** | `-fsanitize=memory` | reads of uninitialised memory | ~3× | ~2× | **clang only** |
| **LSan** | (part of ASan; or `-fsanitize=leak`) | memory leaks at exit | ~0 | — | gcc, clang |

**Golden build for dev/CI:** `-O1 -g -fsanitize=address,undefined
-fno-omit-frame-pointer -fno-sanitize-recover=all`.
`-O1` (not `-O0`): faster + better warnings, still debuggable.
`-fno-sanitize-recover=all`: UBSan pe first error → **abort** (warna woh
print karke chalta rehta, CI green rehti).

---

## AddressSanitizer (ASan) — deep

### Kaise kaam karta — shadow memory

Har 8 bytes of app memory ke liye ASan 1 byte "shadow" rakhta (`addr >> 3
+ offset`). Shadow byte batata woh 8 bytes "addressable" hain ya nahi
(`0` = all 8 ok, `1–7` = pehle N ok, negative = poisoned/redzone/freed).
Har load/store se pehle compiler ek shadow check inject karta. Allocations
ke aas-paas **redzones** (poisoned) — OOB turant pakda. `free` ki hui
memory **quarantine** mein jaati (turant reuse nahi) taaki UAF pakda jaaye.

Isi liye: ~2× slow, ~3× RAM. Aur isi liye `malloc`-heavy code pe redzone
overhead dikhta.

### Report padhna (`examples/03_memory_bugs.cpp` `./mb uaf`)

```
==12345==ERROR: AddressSanitizer: heap-use-after-free
READ of size 4 at 0x602000000010 thread T0
    #0 ... in mode_uaf(int) 03_memory_bugs.cpp:67      <- galti YAHAN
    #1 ... in main          03_memory_bugs.cpp:...

0x602000000010 is located 0 bytes inside of 32-byte region
freed by thread T0 here:
    #1 ... in mode_uaf(int) 03_memory_bugs.cpp:66      <- FREE yahan hui

previously allocated by thread T0 here:
    #1 ... in mode_uaf(int) 03_memory_bugs.cpp:63      <- ALLOC yahan hui
```

Teen stack traces: **use**, **free**, **alloc**. Yeh gdb se kahin tez —
gdb mein tumhe pehle use-after-free ka shak, phir `watch`, phir alloc site
dhoondhna padta.

### `ASAN_OPTIONS` (env var)

```bash
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1:\
strict_string_checks=1:detect_stack_use_after_return=1:\
print_stats=1:log_path=asan.log ./prog
```
- `detect_leaks=1` — LSan on (Linux default on; macOS off).
- `detect_stack_use_after_return=1` — extra instrumentation, stack-UAR
  (`03_memory_bugs.cpp` `stackuaf` mode). Default on in recent versions.
- `halt_on_error=1` — pehli error pe ruko (default: kuch errors pe
  continue).
- Symbolized trace ke liye `llvm-symbolizer` / `addr2line` PATH pe ho.

### Leak detection (LSan)

```
==12345==ERROR: LeakSanitizer: detected memory leaks
Direct leak of 64 byte(s) in 1 object(s) allocated from:
    #1 ... in mode_leak(int) 03_memory_bugs.cpp:55
```
"Direct" = kisi pointer se reachable nahi. "Indirect" = ek leaked object
ke andar se reachable (jaise leaked list ka tail). Fix direct pehle.
Standalone: `-fsanitize=leak` (bina full ASan).

---

## UndefinedBehaviorSanitizer (UBSan)

Sabse **sasta** (~1.2×), hamesha ASan ke saath chalao. `03_memory_bugs`
`uninit`... nahi, woh MSan. UBSan example: `06_buggy_programs/07_integer_overflow.cpp`:

```
07_integer_overflow.cpp:15:19: runtime error: signed integer overflow:
  1500000000 + 1400000000 cannot be represented in type 'int'
```

Sub-checks (`-fsanitize=undefined` inmein se zyaadatar deta):
`signed-integer-overflow`, `shift` (`x << 40` for 32-bit), `divide-by-zero`,
`null` (deref/`this`), `alignment`, `bounds` (needs `-fsanitize=bounds`,
constant-size arrays), `enum`, `bool`, `return` (missing return),
`vptr` (bad polymorphic call — needs RTTI), `float-cast-overflow`,
`unreachable` (`__builtin_unreachable` hit).

`-fsanitize=undefined -fno-sanitize-recover=all` → CI mein har UB = red.
Note: `-fsanitize=integer` (Clang) `unsigned` wrap bhi flag karta (jo UB
**nahi** hai) — sirf tab jab tum us wrap ko bug maante ho.

---

## ThreadSanitizer (TSan)

`examples/04_race_debug.cpp` (default mode):
```
WARNING: ThreadSanitizer: data race (pid=12345)
  Write of size 8 at 0x... by thread T2:
    #0 worker(bool)  04_race_debug.cpp:45
  Previous write of size 8 at 0x... by thread T1:
    #0 worker(bool)  04_race_debug.cpp:45
  Location is global 'g_counter_racy' of size 8
```

TSan **happens-before** track karta (vector clocks): har lock/unlock,
atomic, thread create/join ek ordering edge banata. Do accesses same
location pe, ≥1 write, aur **koi edge nahi** → data race report, dono
stack traces ke saath.

- Race **intermittent hota hai, TSan nahi** — ek race-y run mein woh
  data race ko structurally pakad leta, chahe is baar timing "lucky" ho.
- Lock-order inversion bhi: "deadlock (cycle)" warning bina hang hue.
- `TSAN_OPTIONS=halt_on_error=1:history_size=7:second_deadlock_stack=1`.
- **5–15× slow, 5–10× RAM** — isliye TSan alag CI job, main build nahi.
- Sab code (libs included) instrumented hona chahiye — ek non-instrumented
  `.so` jo locks leta → false negatives/positives. `-fsanitize=thread`
  poore link pe.

---

## MemorySanitizer (MSan) — Clang only

Uninitialised **reads** (na ki sirf declarations). `03_memory_bugs.cpp`
`./mb uninit` isko chahiye — ASan/g++ ise **miss** karta:
```
==12345==WARNING: MemorySanitizer: use-of-uninitialized-value
    #0 ... in mode_uninit(int) 03_memory_bugs.cpp:106
```
Catch: **poori** program (libc++ included) MSan-instrumented honi chahiye,
warna false positives. `-stdlib=libc++` + instrumented libc++ build. Isi
setup cost ki wajah se MSan kam use hota — practically valgrind memcheck
ya `-Wmaybe-uninitialized -O2` pehle try karte.

---

## Kaunse combine kar sakte

| Combo | OK? |
|---|---|
| ASan + UBSan | ✅ **haan** — standard dev build |
| ASan + LSan | ✅ (LSan ASan ka part) |
| UBSan + TSan | ✅ |
| **ASan + TSan** | ❌ — dono memory-map/intercept clash |
| **ASan + MSan** | ❌ |
| **TSan + MSan** | ❌ |

To practically: **do build targets** — `asan+ubsan` aur `tsan`. Dono CI
mein alag jobs.

---

## Sanitizer vs valgrind (`07`)

| | Sanitizers | valgrind (memcheck) |
|---|---|---|
| Recompile chahiye | Haan (`-fsanitize=`) | **Nahi** — koi bhi binary |
| Slowdown | ~2× (ASan) | ~20–30× |
| Stack/global OOB | ✅ ASan | ❌ (memcheck sirf heap) |
| Uninit reads | MSan (Clang) | ✅ memcheck (default) |
| Data races | ✅ TSan | ✅ helgrind/DRD |
| CI ke liye | ✅ (fast) | slow, nightly |
| Third-party binary | ❌ | ✅ |

Dono complementary. **Naya code: sanitizers.** Legacy/opaque binary,
ya "recompile impossible": valgrind.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `-O0` ke saath sanitizer
Chalega, par slow aur kabhi worse diagnostics. `-O1` sweet spot: fast,
still `-g`-debuggable, `-Wmaybe-uninitialized` jaise `-O1+` warnings bhi.

### Trap 2 — UBSan lag gaya par CI green
Default UBSan error print karke **chalta rehta** (exit 0). CI ke liye
`-fno-sanitize-recover=all` (abort) ya `UBSAN_OPTIONS=halt_on_error=1`.

### Trap 3 — ASan ne "kuch nahi" bola, matlab clean
ASan **hardware/stack-OOB within a struct** (member se adjacent member),
uninit reads, aur races ko **miss** karta. "ASan clean" ≠ "bug-free".
UBSan + TSan + valgrind bhi chalao.

### Trap 4 — TSan main build mein
15× slow. Alag job. Aur production mein kabhi nahi (RAM + latency).

### Trap 5 — Partial instrumentation (TSan/MSan)
Ek non-instrumented library → false results. Poora program (+ its libs)
instrument karo, ya us library ko suppress karo (`suppressions=` file).

### Trap 6 — Sanitizer ko benchmark ke saath chalana
ASan ke numbers meaningless hain (`35-PROFILING`). Sanitizer = correctness
tool. Benchmark altar `-O2`, no sanitizer.

### Trap 7 — `new[]`/`delete` mismatch ko "double free" samajhna
ASan `alloc-dealloc-mismatch` alag report deta (`new[]` + `delete`, ya
`malloc` + `delete`). Fix: matching pair.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Sanitizer = valgrind" | Recompile chahiye, 10× faster, stack/global bhi. CI ke liye. |
| "ASan clean = memory-safe" | Uninit reads, races, intra-struct — miss. Layer tools. |
| "UBSan ne warn kiya, chalta to hai" | UB hai. Agla compiler/`-O3` pe silently wrong. Fix. |
| "TSan ne pakad liya, ab prod pe on rakhta hoon" | 15× slow. Dev/CI only. |
| "MinGW pe sanitizer nahi to skip" | WSL/Linux/CI pe chalao. Ek baar setup, hamesha ka faayda. |

---

## Hands-on

```bash
# MinGW box: yeh sirf compile-check hai (libasan nahi)
./build.ps1 build 45-DEBUGGING/examples/03_memory_bugs.cpp

# Linux / WSL -- asli:
g++ -std=c++20 -O1 -g -fsanitize=address,undefined \
    45-DEBUGGING/examples/03_memory_bugs.cpp -o mb
./mb leak ; ./mb uaf ; ./mb oob ; ./mb double ; ./mb stackuaf   # har ek: ek clean ASan report

g++ -std=c++20 -O1 -g -pthread -fsanitize=thread \
    45-DEBUGGING/examples/04_race_debug.cpp -o race && ./race    # TSan data-race report

g++ -std=c++20 -O1 -g -fsanitize=undefined -fno-sanitize-recover=all \
    45-DEBUGGING/examples/06_buggy_programs/07_integer_overflow.cpp -o o && ./o
```

---

## Exercises

1. Tumhare paas ek intermittent crash hai multi-threaded server mein.
   Kaunsa sanitizer pehle, aur kyun ASan+TSan ek saath nahi?
   <details><summary>Answer</summary>
   **TSan** pehle — "intermittent + multi-threaded" = data race ka strong
   signal, aur TSan ise ek race-y run mein bhi structurally pakad leta.
   ASan+TSan ek saath nahi kyunki dono process ki memory map aur
   allocator/interceptors ko apne tareeke se control karte — clash. Do
   alag build targets: `asan+ubsan`, `tsan`.
   </details>

2. UBSan lagaya, output: `runtime error: shift exponent 40 is too large
   for 32-bit type 'int'`. Kya bug, kya fix?
   <details><summary>Answer</summary>
   `x << 40` jahan `x` `int` (32-bit) — shift `>= width` = **UB**. Fix:
   agar 64-bit chahiye `static_cast<uint64_t>(x) << 40`; agar `40` galti
   hai to sahi count; agar variable count hai to `count &= 63` / clamp.
   (`05-OPERATORS/05`.)
   </details>

3. ASan report: `stack-buffer-overflow` in function `parse()`, "Address
   is ... in frame ... at offset 44 in `char buf[40]`". Ek line ka
   diagnosis.
   <details><summary>Answer</summary>
   `buf` 40 bytes ka hai, offset 44 likha/padha gaya — 4 bytes (ya zyada)
   OOB. Sambhavtah `buf[len]` jahan `len >= 40`, ya `memcpy(buf, src, n)`
   with `n > 40`, ya missing NUL space (`strcpy` of 40-char string needs
   41). Fix: bound check / `snprintf` / `std::array` + `.at()` / bada buf.
   </details>

4. `-fsanitize=address` build ne startup pe hi ye diya: "Shadow memory
   range interleaves with ...". Kya ho raha, ek fix idea?
   <details><summary>Answer</summary>
   ASan ki fixed shadow-memory region kisi aur cheez (custom `mmap` at
   fixed addr, ya non-PIE + ASLR interaction, ya another sanitizer) se
   takra rahi. Fixes: `-fPIE -pie` (ya `-no-pie` consistently),
   `ASAN_OPTIONS=protect_shadow_gap=0`, ya conflicting fixed `mmap` hatao.
   Container/old-kernel pe bhi hota — `disable_coredump=0` ya kernel
   `vm.mmap_rnd_bits` tweak.
   </details>

---

## Interview questions

1. ASan kaise OOB pakadta — shadow memory + redzones ek line mein.
2. ASan `free` ki memory ko turant reuse kyun nahi karta?
3. TSan ka "happens-before" kya hai? Race ki definition (`27/01`)?
4. ASan + TSan ek saath kyun nahi? Practical setup?
5. "ASan clean" ka matlab "bug-free" kyun nahi?
6. UBSan itna sasta (~1.2×) kyun ASan (~2×) ke muqable?
7. CI mein sanitizer target kaise configure karoge (flags + options)?

---

## Next
→ [`07-valgrind.md`](07-valgrind.md)

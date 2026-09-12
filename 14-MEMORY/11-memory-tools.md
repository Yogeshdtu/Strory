# 11 — Memory tools (Valgrind, ASan, LSan, MSan, heaptrack, …)

## Prerequisites
- [`05-memory-leaks.md`](05-memory-leaks.md), [`06-use-after-free.md`](06-use-after-free.md)

## Yeh topic abhi kyun
Memory bugs (leak, UAF, OOB, uninitialized read) aankh se nahi dikhte — code
"chalta hai". Tools inhe deterministically pakadte hain. Konsa tool kya pakadta,
kaise chalao, aur — is repo ke liye important — **MinGW-w64 pe kya available
nahi aur uska alternative kya**.

---

## Tool → bug matrix

| Tool | Leak | Heap UAF / double-free | Heap OOB | Stack OOB / UAR | Uninitialized read | Slowdown |
|---|---|---|---|---|---|---|
| **AddressSanitizer** (ASan) | ✅ (LSan) | ✅ | ✅ | ✅ | ❌ | ~2-3x |
| **LeakSanitizer** (LSan) | ✅ | ❌ | ❌ | ❌ | ❌ | ~0 |
| **MemorySanitizer** (MSan) | ❌ | ❌ | ❌ | ❌ | ✅ | ~3x (Clang only) |
| **UBSan** | ❌ | partial | partial | partial | ❌ | small |
| **Valgrind memcheck** | ✅ | ✅ | ✅ | partial | ✅ | ~10-30x |
| **heaptrack** | ✅ + profiling | ❌ | ❌ | ❌ | ❌ | ~1.5-2x |
| **Dr. Memory** (Win) | ✅ | ✅ | ✅ | partial | ✅ | ~10x |

**Rule of thumb:** dev/CI mein **ASan+UBSan** (fast, catches most); ek MSan
build (Clang) uninitialized ke liye; Valgrind jab recompile na kar sako ya
ASan miss kare; heaptrack allocation *profiling* (kitna, kahan se, kab) ke liye.

---

## AddressSanitizer — primary tool

```bash
g++ -std=c++20 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer prog.cpp -o prog
./prog
```

Kya deta hai (har error pe abort + report):
- **heap-use-after-free** — use ka stack, "freed by" stack, "allocated by" stack.
- **heap-buffer-overflow** — kis allocation ke kitne bytes aage/peeche.
- **stack-buffer-overflow**, **stack-use-after-return** (`ASAN_OPTIONS=detect_stack_use_after_return=1`,
  ab default), **stack-use-after-scope**.
- **global-buffer-overflow**.
- **double-free / invalid-free / alloc-dealloc-mismatch** (`new[]` vs `delete`).
- **memory leaks** (LSan built-in on Linux, exit pe).

Kaise: har allocation ke around **redzones** (poisoned bytes), freed memory
**quarantine** (turant reuse nahi), aur har load/store se pehle **shadow memory**
(1 shadow byte per 8 app bytes) check. Isliye ~2-3x slow + ~2x RAM.

Tuning: `ASAN_OPTIONS=halt_on_error=0:detect_leaks=1:strict_string_checks=1`.
Suppress known leaks: `LSAN_OPTIONS=suppressions=lsan.supp`.

---

## Valgrind memcheck — no recompile

```bash
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./prog
```

- Har allocation shadow-track; har byte "addressable?" + "defined?" (initialized).
- **Uninitialized value use** pakadta hai (ASan nahi karta) — `--track-origins=yes`
  batata hai woh uninitialized byte kahan bana.
- Leak categories: **definitely lost** (fix karo), **indirectly lost** (parent
  leaked), **possibly lost** (interior pointer), **still reachable** (exit pe
  live — usually OK).
- ~10-30x slow, single-threaded emulation. Bade/realtime programs pe impractical
  — chhote repros ke liye best.

⚠️ Valgrind **Linux/macOS** (macOS support flaky new versions). **Windows nahi**.

---

## Uninitialized reads

- **MSan** (Clang only, Linux): `clang++ -fsanitize=memory -fPIE -pie -g` — har
  uninitialized read + origin. Requires **poori dependency chain** (libc++
  included) instrumented — isliye setup mushkil, par sabse precise.
- **Valgrind** — no rebuild, catches most, `--track-origins`.
- **`-ftrivial-auto-var-init=pattern`** (GCC/Clang) — uninitialized locals ko
  `0xAA...` se fill → bug deterministic (crash/wrong value consistently), aur
  security hardening. Production mein `=zero` bhi (chhota cost).
- Compiler: `-Wmaybe-uninitialized` / `-Wuninitialized` (`-Wall -O2`) — static,
  simple cases.

---

## heaptrack — allocation profiler (Linux/KDE)

```bash
heaptrack ./prog
heaptrack_gui heaptrack.prog.12345.zst      # ya heaptrack_print
```

Batata hai: **total allocations**, **peak RSS** aur woh kab hua, **top allocation
call sites** (kitni bar, kitne bytes), **leaks**, **temporary allocations**
(allocate then quickly free — churn), allocation **timeline**. Optimization ke
liye ("yeh function har call pe 3 allocate kar raha") ASan/Valgrind se behtar —
woh *correctness*, yeh *cost*.

macOS: **Instruments** (Allocations, Leaks). Windows: **VMMap**, WPA/xperf heap
traces.

---

## MinGW-w64 pe kya karein (is repo)

`C:\mingw64` pe **`libasan`/`libubsan` nahi** → `-fsanitize=address,undefined`
link fail (`cannot find -lasan`). Valgrind bhi Windows pe nahi.

| Chahiye | MinGW workaround | Asli fix |
|---|---|---|
| Leak detect | counted `operator new`/`delete` (examples mein) → count mismatch | WSL/Linux + ASan/LSan; ya Visual Studio `_CrtDumpMemoryLeaks`; Dr. Memory |
| UAF / double-free | counted new/delete (double-free count), crash on double-free | WSL/Linux + ASan; Application Verifier page-heap |
| OOB (STL) | `./build.ps1 san` → `-D_GLIBCXX_ASSERTIONS` (container `[]`/`at` bounds) | WSL/Linux + ASan for raw arrays |
| Stack smash | `./build.ps1 san` → `-fstack-protector-all` (canary → `__stack_chk_fail`) | ASan `stack-buffer-overflow` |
| Uninitialized | `-Wmaybe-uninitialized`, `-ftrivial-auto-var-init=pattern` | Valgrind / MSan on Linux |

**Recommended:** bug ko **WSL** (`wsl --install`, phir Ubuntu) ya kisi Linux box
pe reproduce karo aur wahan `-fsanitize=address,undefined` chalao — 5 min setup,
poora diagnosis. Windows-native ke liye **Dr. Memory** (open source, ASan-ish)
ya **Application Verifier + WinDbg** (page-heap: har allocation apni page pe,
UAF/OOB = instant fault).

`./build.ps1 san <file>` is repo mein exactly yeh fallback karta hai (dekho
`build.ps1` ka `san` branch).

---

## CI mein kya

```
- normal build         (-O2, -Wall -Wextra -Werror)
- ASan + UBSan build   -> test suite chalao (Linux runner)
- LSan                 -> ASan ke saath free
- MSan build           (Clang) -> uninitialized (optional, setup-heavy)
- soak test            -> ghanton chalao, RSS flat hona chahiye (leak/frag)
- Valgrind             -> chhote critical tests pe (slow)
```

HFT: production binary **no sanitizer** (overhead), par **har commit** ASan/UBSan
CI se guzarta, aur ek nightly soak.

---

## Andar kya hota hai (ASan)

- **Compile:** har memory access ke aage compiler ek shadow-check inject karta:
  `shadow = (addr >> 3) + offset; if (*shadow != 0 && slow_check(addr)) report();`
- **Runtime:** ASan apna `malloc` install karta — allocation ke around redzones
  poison (shadow mein non-zero), `free` pe poora block poison + quarantine
  (default 256 MB) mein daale (reuse delay → UAF window bada).
- **Leak:** exit pe stop-the-world, roots (globals/stack/registers/TLS) se
  reachable heap blocks mark; unmarked = leak, allocation stack print.
- **Cost:** shadow = 1/8 address space reserved; ~2x RAM, ~2-3x CPU. Isliye dev/CI,
  production nahi.

> **HFT relevance:** memory bugs production mein sabse mehnge (non-deterministic,
> load-dependent corruption). Discipline: (1) ASan+UBSan har CI run pe green,
> (2) nightly soak test RSS-flat, (3) heaptrack se hot path ki allocation count
> literally **zero** verify, (4) custom arenas ko manual ASan poisoning
> (`__asan_poison_memory_region`) taaki woh bhi covered ho, (5) `-ftrivial-auto-
> var-init=zero` production hardening. Tool output "0 errors, 0 leaks, RSS flat"
> ek release gate hoti hai.

---

## Hands-on

```bash
# is repo (MinGW): counted-new leak report
./build.ps1 14-MEMORY/examples/04_memory_leak.cpp

# WSL / Linux: asli ASan
g++ -std=c++20 -fsanitize=address,undefined -g 14-MEMORY/examples/04_memory_leak.cpp -o leak && ./leak
g++ -std=c++20 -fsanitize=address,undefined -g 14-MEMORY/examples/05_use_after_free.cpp -o uaf && ./uaf
valgrind --leak-check=full ./leak
```

Compare karo: counted-new sirf "6 blocks leaked" kehta; ASan/Valgrind **kahan
allocate hue** (stack trace) bhi.

---

## ⚠️ Traps

### Trap 1 — ASan aur Valgrind ek saath
Dono apna `malloc` install karte → conflict. Ek time pe ek.

### Trap 2 — `-O0` pe sanitizer chalana aur "slow" bolna
`-O1`/`-O2` + ASan use karo — `-O0` bekaar slow aur bug patterns alag.

### Trap 3 — production binary mein sanitizer chhod dena
2-3x overhead + 2x RAM + attack surface. Dev/CI only.

### Trap 4 — "ASan clean = bug-free"
ASan uninitialized reads, logic races (kuch), custom-arena bugs miss karta.
MSan + TSan + review bhi chahiye.

### Trap 5 — MinGW pe `-fsanitize=address` likh ke commit
Link fail. Is repo pe `./build.ps1 san` fallback, ya WSL.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Ek tool sab pakadta hai" | ASan (heap/stack/leak), MSan (uninit), TSan (races) — alag |
| "Valgrind aur ASan same cheez" | Valgrind: no rebuild, uninit, 10-30x. ASan: rebuild, faster, no uninit |
| "Sanitizer production mein rakh do" | Overhead + RAM + surface — dev/CI only |
| "MinGW pe ASan `-fsanitize` se chal jaayega" | libasan nahi — link fail. WSL/Dr. Memory |
| "heaptrack bhi bug-finder hai" | Woh allocation *profiler* — cost, count, timeline (not correctness) |

---

## Exercises

1. **Tool pick:** har bug ke liye best tool? (a) leak, (b) `delete` ke baad
   read, (c) `int x; if (x) ...`, (d) "kya har event allocate kar raha?", (e)
   `arr[10]` on `int arr[10]`.

   <details><summary>Answer</summary>

   (a) LSan/ASan/Valgrind. (b) ASan (UAF). (c) MSan / Valgrind (uninit). (d)
   heaptrack. (e) ASan (stack-buffer-overflow) / `_GLIBCXX_ASSERTIONS` if
   `std::array`.
   </details>

2. **ASan on 05:** WSL/Linux pe `05_use_after_free.cpp` ASan se chalao. BUG 1 pe
   kitne stack traces, kya label? (concept)

   <details><summary>Answer</summary>

   `heap-use-after-free`: 3 traces — "READ ... here" (`*p` line), "freed by ...
   here" (`delete p` line), "previously allocated by ... here" (`new int` line).
   </details>

3. **Counted-new vs ASan:** `04_memory_leak.cpp` — counted-new report vs ASan
   report. Kya extra ASan deta hai?

   <details><summary>Answer</summary>

   Counted-new: sirf "N blocks, M bytes leaked". ASan/LSan: **har leak ka
   allocation stack trace** (kaunsi line ne `new` kiya) + exact byte count per
   leak + direct/indirect classification.
   </details>

4. **`-ftrivial-auto-var-init`:** ek function `int f(){ int x; return x; }` —
   normal `-O2` vs `-ftrivial-auto-var-init=pattern`. Return value predictable
   hui? Yeh debugging + security dono kaise help?

   <details><summary>Answer</summary>

   Normal: garbage (jo bhi stack pe tha). `=pattern`: `x` = `0xAAAAAAAA`
   consistently → bug deterministic (same wrong value har run) + koi stale
   secret leak nahi (security). `=zero` production mein chhote cost pe.
   </details>

5. **Soak test:** ek program jo 1e7 baar ek function call kare jo (buggy) 100
   bytes leak kare. RSS ko 1e5 iterations ke intervals pe log karo (`/proc/self/
   statm` / `GetProcessMemoryInfo`). Graph? Fixed version ka graph?

   <details><summary>Answer</summary>

   Buggy: RSS linear upward (~1 GB by end). Fixed: RSS initial ramp phir **flat
   plateau** (steady state). Yeh soak-test signature — CI mein automate.
   </details>

---

## Interview questions

1. ASan kya-kya pakadta, kya nahi? Kaise kaam karta (shadow, redzone, quarantine)?
2. ASan vs Valgrind — trade-offs (speed, rebuild, uninit)?
3. Uninitialized reads ke liye kaunse tools/flags?
4. heaptrack ASan se kaise alag purpose?
5. MinGW-w64 pe sanitizers nahi — alternatives?
6. CI mein memory-tool strategy — kaunse builds, soak test kyun?

---

## Next
→ [`12-exercises.md`](12-exercises.md)

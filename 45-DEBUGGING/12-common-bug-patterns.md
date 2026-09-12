# 12 — Common bug patterns: the catalog

## Prerequisites
- Folder 45 ke `01`–`11`
- Reference: `12-POINTERS/13-pointer-bugs-catalog.md`,
  `25-OBJECT-MODEL/15-undefined-behaviour-catalog.md`,
  `26-CONCURRENCY/17-concurrency-bugs.md`

## Yeh topic abhi kyun

Debugging tez hoti hai jab tum bug ko **pehchan** lo — "yeh signature to X
family ka hai" → seedha right tool. Yeh lesson ek **reference catalog**
hai: har common C++ bug, uska **symptom**, **kaunsa tool** turant pakadta,
aur **fix + prevention**. Ise skim karo; jab koi bug aaye, `Ctrl-F`.

Har row: **Symptom → Tool → Fix**. Cross-refs poore course mein.

---

## A. Memory bugs

### A1. Heap buffer overflow (OOB read/write)
- **Symptom:** garbage values / crash / "corrupted" heap error; often
  far from the real write. Non-deterministic.
- **Tool:** ASan (`heap-buffer-overflow`, exact line + alloc site) ·
  valgrind memcheck (`Invalid write ... N bytes after a block`) ·
  `-D_GLIBCXX_ASSERTIONS` (`vector::operator[]`) · `-fsanitize=bounds`.
- **Fix:** index `[0, size)`; `.at()` in non-hot paths; `std::span` with
  size; loop bounds `< size` not `<= size`.
- Refs: `09-ARRAYS/11`, `12-POINTERS/13`, examples `03_memory_bugs.cpp`
  (`oob`), `06_buggy_programs/01_off_by_one.cpp`.

### A2. Stack buffer overflow
- **Symptom:** local variables corrupt, return address smashed → crash on
  `ret`, `__stack_chk_fail` (SIGABRT), wild `bt`.
- **Tool:** ASan (`stack-buffer-overflow`, "in frame ... `char buf[N]`") ·
  `-fstack-protector-all` (canary → abort) · gdb (corrupt `bt` = smell).
- **Fix:** bounds; `snprintf` not `sprintf`; `std::array`; don't take
  sizes from untrusted input without checking.
- Refs: `08-FUNCTIONS/05`, `14-MEMORY/02`.

### A3. Use-after-free (heap)
- **Symptom:** works first time, wrong/crash later; value "was right a
  moment ago"; intermittent.
- **Tool:** ASan (`heap-use-after-free` + **use / free / alloc** stacks) ·
  valgrind (`Invalid read ... free'd`) · gdb `watch -location` +
  `reverse-continue` (`09`).
- **Fix:** clear/null pointers after free; `unique_ptr`/`shared_ptr` for
  ownership; don't return/store pointers to things that outlive the
  pointer holder... wait — that outlive the *pointee*.
- Refs: `14-MEMORY/06`, `17-RAII`, examples `03_memory_bugs.cpp` (`uaf`).

### A4. Use-after-return / use-after-scope (stack)
- **Symptom:** pointer/reference to a local used after the function
  returned (or block ended); garbage, corruption, "random on different
  machine".
- **Tool:** ASan (`stack-use-after-return` / `-after-scope`; on by
  default) · `-Wreturn-local-addr`, `-Wdangling-pointer` (compile-time,
  simple cases) · `-Wdangling-reference` (GCC 13+, `const&` binding).
- **Fix:** return by value; caller-provided buffer/reference; heap +
  clear ownership.
- Refs: `13-REFERENCES/09`, examples `03_memory_bugs.cpp` (`stackuaf`),
  `06_buggy_programs/04_dangling_view.cpp`.

### A5. Double free
- **Symptom:** `free(): double free detected` / `attempting double-free`
  (SIGABRT), often at scope exit or shutdown.
- **Tool:** ASan (`attempting double-free` + both free stacks) · valgrind
  (`Invalid free ... already freed`) · GCC `-O2 -Wuse-after-free`.
- **Fix:** one owner; null after free (`free(nullptr)` is safe); Rule of
  Zero (`std::vector`/smart pointers); if raw: deep-copy or `=delete`
  copy.
- Refs: `14-MEMORY`, `17-RAII`, `18-COPY-MOVE`, examples
  `03_memory_bugs.cpp` (`double`), `06_buggy_programs/08_double_free_rule_of_three.cpp`.

### A6. `new`/`delete` / `new[]`/`delete[]` / `malloc`/`delete` mismatch
- **Symptom:** heap corruption, occasional crash, ASan
  `alloc-dealloc-mismatch`.
- **Tool:** ASan · valgrind (`Mismatched free() / delete / delete []`).
- **Fix:** match the pair; better — don't use raw `new[]`, use
  `std::vector` / `std::make_unique<T[]>`.

### A7. Memory leak
- **Symptom:** RSS grows without bound; OOM after hours/days; no crash.
- **Tool:** ASan/LSan (`detected memory leaks` + alloc stack) · valgrind
  `--leak-check=full` (`definitely lost`) · massif / DHAT for
  "reachable-but-growing".
- **Fix:** RAII (own nothing raw); match every `new`/`malloc`; break
  `shared_ptr` cycles with `weak_ptr`; bound caches (LRU/eviction).
- Refs: `14-MEMORY/05`, `17-RAII`, `07-valgrind.md`.

### A8. Uninitialised read
- **Symptom:** works on your machine, fails elsewhere; different result
  per run/build; `-O2` worse.
- **Tool:** `-Wmaybe-uninitialized` (`-O1`/`-O2`) · Clang MSan
  (`use-of-uninitialized-value`) · valgrind memcheck
  (`Conditional jump ... uninitialised`) · `-ftrivial-auto-var-init=pattern`
  (makes it deterministic → reproducible).
- **Fix:** initialise every variable at declaration (`{}` / a real value);
  `-Werror=uninitialized`.
- Refs: `03-VARIABLES/03`, examples `06_buggy_programs/02_uninitialized.cpp`.

### A9. Dangling `string_view` / `span` / iterator / pointer to container data
- **Symptom:** view/span points to freed or moved data; garbage / crash;
  common with temporaries (`sv = get_string().substr(...)`).
- **Tool:** ASan · `-Wdangling-pointer` (some cases) · review discipline.
- **Fix:** views never outlive their owner; store the owning type if
  lifetime is unclear; be wary of `auto x = f().c_str();`.
- Refs: `10-STRINGS`, `13-REFERENCES/09`, `22-MODERN-CPP`.

### A10. Iterator / reference invalidation
- **Symptom:** crash or skipped/duplicated elements after
  `push_back`/`insert`/`erase` in a loop; `-O2` changes behaviour.
- **Tool:** `-D_GLIBCXX_DEBUG` (libstdc++ debug mode → exact message) ·
  ASan (post-realloc OOB) · MSVC iterator debugging.
- **Fix:** use `erase`'s return value; `std::erase_if` / erase-remove;
  reserve up front; re-acquire iterators after mutation; index instead of
  iterator when the container reallocates.
- Refs: `19-STL`, `09-ARRAYS/11`, examples `06_buggy_programs/06_iterator_invalidation.cpp`.

### A11. Stack overflow
- **Symptom:** SIGSEGV; `bt` shows thousands of identical frames; fault
  address near `$sp`.
- **Tool:** gdb (`bt` pattern) · ASan (`stack-overflow`) · `-fstack-usage`
  (per-function stack size) · `ulimit -s`.
- **Fix:** recursion base case / depth guard; recursion → explicit stack
  + loop; large buffers on heap not stack.
- Refs: `08-FUNCTIONS/05`, `14-MEMORY/02`, `04-debugging-crashes.md`.

### A12. Null pointer dereference
- **Symptom:** SIGSEGV at address `0x0` or small offset (`0x8`, `0x20`).
- **Tool:** gdb (`print p` → `0x0`) · UBSan (`-fsanitize=null`) · ASan
  (`SEGV on unknown address 0x000...`) · `-Wnull-dereference`.
- **Fix:** check before deref; `optional`/`expected` instead of "null
  means absent"; references where "never null" is the contract.
- Refs: `12-POINTERS`, `23-ERROR-HANDLING`.

---

## B. Concurrency bugs

### B1. Data race
- **Symptom:** wrong/garbage shared value; non-deterministic; "works with
  fewer threads / at `-O2`" (register promotion can hide it).
- **Tool:** **TSan** (`data race` + both access stacks + var) · valgrind
  helgrind / DRD · reasoning (shared mutable + no sync).
- **Fix:** `std::atomic` (with the right memory order) · mutex (every
  access, reads too) · sharding / per-thread state · immutability.
- Refs: `26-CONCURRENCY/05`, `27-ATOMICS/01`, `08-debugging-multithreaded.md`,
  examples `04_race_debug.cpp`, `06_buggy_programs/10_data_race.cpp`.

### B2. Deadlock (lock-order inversion)
- **Symptom:** hang, ~0% CPU, no progress.
- **Tool:** gdb `-p <pid>` → `thread apply all bt` (≥2 threads in
  `__lll_lock_wait`, waiting on each other's held locks) · helgrind /
  TSan (potential, no hang needed).
- **Fix:** global lock order; `std::scoped_lock(m1, m2)` (deadlock-avoiding);
  lock hierarchies; hold fewer locks; lock-free.
- Refs: `26-CONCURRENCY/09`, examples `05_deadlock_debug.cpp`.

### B3. Deadlock (missing unlock)
- **Symptom:** hang; exactly one thread in `lock_wait`, others idle; a
  mutex whose `__owner` thread is off doing something else / looping.
- **Tool:** gdb `thread apply all bt` + `print *(pthread_mutex_t*)addr`
  (`__owner`).
- **Fix:** **always** RAII lock guards — never raw `.lock()`/`.unlock()`;
  no early `return`/`throw` between manual lock/unlock.

### B4. Livelock
- **Symptom:** hang-like but **100% CPU**; threads active, no progress
  (mutual retry / back-off collisions).
- **Tool:** `perf top` / `perf record` (spin in CAS-retry / `while(!flag)`) ·
  gdb repeated `bt` (same tight loop).
- **Fix:** randomised backoff; fairness; a real lock; bounded retries.
- Refs: `28-LOCK-FREE`, `26-CONCURRENCY/17`.

### B5. Lost wakeup / missed notification
- **Symptom:** thread stuck in `cv.wait()` forever though the condition
  is true; intermittent.
- **Tool:** gdb (`bt` → `pthread_cond_wait`) + inspect the predicate ·
  code review (notify before wait? no predicate loop?).
- **Fix:** **always** `cv.wait(lk, [&]{ return predicate; })` (loop,
  handles spurious wakeups + notify-before-wait); hold the lock when
  changing the predicate and notifying.
- Refs: `26-CONCURRENCY` (condition variables).

### B6. ABA problem
- **Symptom:** lock-free structure corrupts under contention; a CAS
  "succeeds" because the pointer value matches but the object changed
  (freed + reallocated).
- **Tool:** TSan (sometimes) · stress + `rr --chaos` · reasoning about
  the CAS.
- **Fix:** tagged pointers / generation counters (`44` `ObjectPool`
  handles), hazard pointers, RCU, epoch-based reclamation.
- Refs: `28-LOCK-FREE`, `44-HFT-PROJECTS` (generation-checked handles).

### B7. TOCTOU (time-of-check to time-of-use)
- **Symptom:** `if (!map.count(k)) map[k] = f();` from two threads → double
  insert / race; or file `stat` then `open` races.
- **Tool:** TSan · reasoning (gap between check and act on shared state).
- **Fix:** atomic check-and-act (`map.try_emplace`, `insert` returns
  "inserted?"), hold a lock across both, or `O_CREAT|O_EXCL` for files.

### B8. `std::shared_ptr` misuse across threads
- **Symptom:** race reported on the control block or the `shared_ptr`
  object itself.
- **Tool:** TSan.
- **Fix:** the **refcount** is atomic; the **`shared_ptr` instance** is
  not — give each thread its own copy, or guard the shared instance
  (`std::atomic<std::shared_ptr>` / mutex).
- Refs: `17-RAII`, `26-CONCURRENCY`.

### B9. False sharing (performance, not correctness)
- **Symptom:** correct output, but a multi-threaded loop is 10–100×
  slower than expected; scales *negatively* with threads.
- **Tool:** `perf c2c` (HITM on one cache line, different offsets) ·
  `perf stat` (high cache coherence traffic).
- **Fix:** `alignas(64)` hot per-thread data; pad structs; separate
  read-mostly from write-hot fields.
- Refs: `43-HFT-OPTIMIZATION/08`, `31-CPU`, `32-CACHE`.

---

## C. Logic bugs

### C1. Off-by-one
- **Symptom:** last/first element missing or one extra; OOB by one;
  `< vs <=`, `size vs size-1`, `begin vs begin+1`.
- **Tool:** unit tests with boundary inputs (empty, 1 elem, full) · ASan
  (if it reads OOB) · careful reading.
- **Fix:** half-open ranges `[begin, end)`; range-for; `i < size`.
- Refs: `07-LOOPS`, examples `06_buggy_programs/01_off_by_one.cpp`.

### C2. `=` vs `==`
- **Symptom:** `if (x = 5)` assigns then tests truthiness → always taken;
  `x` clobbered.
- **Tool:** `-Wparentheses` / `-Wall` ("suggest parentheses around
  assignment used as truth value") · clang-tidy.
- **Fix:** `if (5 == x)` (Yoda) or just `-Werror=parentheses`; compilers
  accept `if ((x = f()))` as "I meant it".

### C3. `&`/`|` vs `&&`/`||`, and precedence
- **Symptom:** `if (a & b == c)` parses as `a & (b == c)`; bitwise where
  you wanted logical (both operands evaluated, no short-circuit).
- **Tool:** `-Wparentheses` (`&`/`|` with `==`) · `-Wbitwise-instead-of-logical`
  (Clang).
- **Fix:** parentheses; correct operator; don't rely on the precedence
  table — write it explicit.
- Refs: `05-OPERATORS/09`, examples `06_buggy_programs/09_precedence.cpp`.

### C4. `switch` fallthrough
- **Symptom:** a `case` without `break` falls into the next; extra work
  done.
- **Tool:** `-Wimplicit-fallthrough` (`-Wextra`) · `[[fallthrough]]` to
  mark the intentional ones.
- **Fix:** `break;` / `return;` each case; `[[fallthrough]]` where
  deliberate.
- Refs: `06-CONDITIONS/04`.

### C5. Copy-paste error
- **Symptom:** `rect.w = a.w; rect.h = a.w;` (should be `a.h`); one branch
  of an if/else identical to the other; wrong index in an unrolled block.
- **Tool:** `-Wduplicated-branches`, `-Wduplicated-cond` (GCC) · review ·
  tests per field.
- **Fix:** loop instead of unroll; helper function; read every pasted line.

### C6. Wrong operator / inverted condition
- **Symptom:** `>` where `<`, `!` missing/extra, `<=` vs `<`; feature
  "backwards".
- **Tool:** unit tests (the only reliable catch) · property-based tests.
- **Fix:** name booleans positively (`is_ready` not `not_done`); test
  both sides.

### C7. Float equality / accumulation error
- **Symptom:** `if (x == 0.1)` never true; sums drift; `!=` loop never
  terminates.
- **Tool:** UBSan `float-cast-overflow` (partial) · reasoning · tests
  with tolerance.
- **Fix:** compare with epsilon (relative); integer/fixed-point for money
  (`43/09`); never `float` loop counters.
- Refs: `03-VARIABLES` (float), `43-HFT-OPTIMIZATION/09`.

---

## D. Integer bugs

### D1. Signed integer overflow (UB)
- **Symptom:** wrong result on large inputs; a bounds check passes when
  it shouldn't; `-O2` "optimises away" a check (compiler assumes no
  overflow).
- **Tool:** `-fsanitize=undefined` (`signed integer overflow: A + B
  cannot be represented`) · `-ftrapv` · `-Wstrict-overflow`.
- **Fix:** check via subtraction (`a > LIMIT - b`); wider type; `<numeric>`
  `add_overflow` builtins (`__builtin_add_overflow`).
- Refs: `05-OPERATORS/01`, `23-ERROR-HANDLING/13`, examples
  `06_buggy_programs/07_integer_overflow.cpp`.

### D2. Unsigned wraparound
- **Symptom:** `size() - 1` on empty container → huge; `for (size_t i =
  n-1; i >= 0; --i)` never ends; `a - b` where `a < b` → giant.
- **Tool:** reasoning about unsigned types · ASan (if the huge value
  indexes) · `-fsanitize=unsigned-integer-overflow` (Clang; not UB but
  flaggable).
- **Fix:** `i + 1 < size()`; check `a >= b` before `a - b`; signed types
  for values that go negative; `ssize()` (C++20).
- Refs: `03-VARIABLES`, examples `06_buggy_programs/03_signed_unsigned.cpp`.

### D3. Narrowing / truncation
- **Symptom:** `int n = big_size_t;` loses high bits; `char c = 200;`
  (implementation-defined / negative if signed); `float f = huge_int;`
  loses precision.
- **Tool:** `-Wconversion -Wsign-conversion` · `{}` brace-init (narrowing
  is an error) · `-Wnarrowing`.
- **Fix:** `static_cast` with intent + range check; `gsl::narrow`; use
  `size_t` for sizes throughout.

### D4. Shift bugs
- **Symptom:** `x << 32` on 32-bit `int` (UB); `1 << 31` for `int`
  (UB — sign bit); shift by negative.
- **Tool:** UBSan (`shift exponent N is too large` / `left shift of ...
  by 31 places cannot be represented`).
- **Fix:** unsigned types for bit work (`1u << 31`, `1ull << 40`); mask
  the shift count; `std::rotl`/`rotr` for rotations.
- Refs: `05-OPERATORS/05`.

### D5. Division / modulo by zero, and `INT_MIN / -1`
- **Symptom:** SIGFPE (integer); `INT_MIN % -1` / `INT_MIN / -1` UB.
- **Tool:** UBSan (`division by zero`, `division ... cannot be
  represented`) · gdb on the `idiv`.
- **Fix:** guard the divisor; special-case `INT_MIN`; unsigned if
  applicable.

### D6. Mixed signed/unsigned comparison
- **Symptom:** `-1 < 1u` is **false** (`-1` converts to huge unsigned);
  loops and bounds checks silently wrong.
- **Tool:** `-Wsign-compare` (`-Wextra`) · `-Wsign-conversion` ·
  `std::cmp_less` / `cmp_greater` (C++20) for safe comparison.
- **Fix:** keep types consistent; `std::cmp_*`; cast with a range check.

---

## E. Lifetime & object-model bugs

### E1. Return reference/pointer to local
- Covered A4. `-Wreturn-local-addr`; return by value.

### E2. Dangling reference from a temporary
- **Symptom:** `const std::string& s = obj.name_or_default();` where the
  function returns by value → `s` dangles after the full expression
  (unless bound directly to the temporary, which extends life — but not
  through a function return of a reference to an internal).
- **Tool:** `-Wdangling-reference` (GCC 13+) · ASan · review.
- **Fix:** bind by value (`auto s = ...`), or ensure the referent
  genuinely outlives the reference.
- Refs: `25-OBJECT-MODEL/05` (temporaries), `13-REFERENCES`.

### E3. Use-after-move
- **Symptom:** a moved-from object read → empty/unspecified value; silent
  wrong data (usually not a crash).
- **Tool:** clang-tidy `bugprone-use-after-move` · Clang
  `-Wpessimizing-move` (adjacent) · review.
- **Fix:** after `std::move(x)`, only assign to or destroy `x`; read
  what you need **before** moving.
- Refs: `18-COPY-MOVE`, `22-MODERN-CPP`, examples
  `06_buggy_programs/05_use_after_move.cpp`.

### E4. Rule of Three/Five violation
- **Symptom:** shallow copy of an owning class → double free / use-after-
  free when both copies destruct; or move leaves both usable.
- **Tool:** ASan / valgrind (double free) · `-Weffc++` (noisy) · GCC
  `-O2 -Wuse-after-free` (sometimes).
- **Fix:** Rule of Zero (members that manage themselves — `vector`,
  `unique_ptr`); or define all of copy/move ctor+assign (+dtor)
  correctly (copy-and-swap); or `=delete` the ones you don't want.
- Refs: `17-RAII`, `18-COPY-MOVE`, examples
  `06_buggy_programs/08_double_free_rule_of_three.cpp`.

### E5. Static initialization order fiasco
- **Symptom:** a global depends on another global in a **different**
  translation unit; crash / zero value at startup, order-dependent.
- **Tool:** gdb (`break` in constructor, check the other global) ·
  reasoning · `-Wglobal-constructors` (Clang).
- **Fix:** "construct on first use" (function-local `static`); avoid
  cross-TU global dependencies; `constinit` for constant init.
- Refs: `25-OBJECT-MODEL/02`, `24-COMPILATION-LINKING`.

### E6. Temporary lifetime / `std::initializer_list` dangling
- **Symptom:** `std::initializer_list` member outlives its backing
  array; `auto&& x = f();` then `x` used after the temp's full-expression
  in some cases.
- **Tool:** ASan · Clang `-Wdangling` (GCC has no such flag; `-Wdangling-reference` covers only some cases) · review.
- **Fix:** don't store `initializer_list`; copy into a `vector`; be
  explicit with `auto` (value) vs `auto&&`.
- Refs: `25-OBJECT-MODEL/05`.

### E7. Slicing
- **Symptom:** `Base b = derived;` copies only the `Base` part; virtual
  dispatch "stops working"; missing derived data.
- **Tool:** `-Wslicing` (Clang, some cases) · tests on polymorphic
  behaviour · review (pass-by-value of a polymorphic type).
- **Fix:** pass/store by reference or pointer (`Base&`, `Base*`,
  `unique_ptr<Base>`); delete the `Base` copy ctor if it should never be
  copied.
- Refs: `16-OOP`, `25-OBJECT-MODEL`.

### E8. Strict aliasing violation
- **Symptom:** reinterpret a `float*` as `int*` (or via `union`
  type-punning in ways C++ doesn't bless) → wrong values at `-O2`, works
  at `-O0`.
- **Tool:** `-fsanitize=alignment` (partial) · `-Wstrict-aliasing` ·
  `-fno-strict-aliasing` (bug goes away → confirms) · review.
- **Fix:** `std::memcpy` / `std::bit_cast` (C++20) for type punning;
  `std::launder` in rare cases.
- Refs: `25-OBJECT-MODEL/10`, `31-CPU`, `43-HFT-OPTIMIZATION/16` (E2).

---

## Symptom → likely family (quick index)

| Symptom | Look at |
|---|---|
| Different result per run / per machine | A8 uninit · B1 race · E5 static init · D (impl-defined) |
| Works at `-O0`, breaks at `-O2` | A8 · D1 signed overflow · E8 strict aliasing · B1 (register promotion the other way) |
| Works first time, breaks later | A3 use-after-free · A10 invalidation · stale/cached state |
| Hang, 0% CPU | B2/B3 deadlock · B5 lost wakeup |
| Hang, 100% CPU | B4 livelock · C infinite loop (bad bound / D2 unsigned) |
| Crash far from the bad code | A1/A2 overflow (corrupts, detected later) · A5 double free |
| "Corrupted"/"invalid" heap message | A1 OOB write · A5 · A6 mismatch |
| Silent wrong number | C logic · D integer · E3 use-after-move · C7 float |
| Slow but correct | `11-perf` · B9 false sharing · accidental syscall/alloc |
| Fails only under load / many threads | B1 race · B6 ABA · B7 TOCTOU · B9 |

---

## Prevention checklist (write these into the build)

- `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
  -Wnull-dereference -Wdouble-promotion` (this repo's set) + `-Werror` in CI
- A CI target: `-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all`
- A CI target: `-fsanitize=thread` for concurrent code
- `-D_GLIBCXX_ASSERTIONS` in debug builds; `-D_GLIBCXX_DEBUG` for
  container-heavy debugging
- Unit tests with **boundary** inputs (empty, one, max) — catches C1/D2
- Initialise every variable; RAII for every resource; Rule of Zero
- `std::cmp_*` for mixed-sign comparisons; fixed-point for money
- Static analysis: `clang-tidy` (`bugprone-*`, `cppcoreguidelines-*`),
  `cppcheck`

---

## Exercises

1. "Different result every run, single-threaded." Two families to check
   first, and the one tool for each.
   <details><summary>Answer</summary>
   (A8) **Uninitialised memory** → `-Wmaybe-uninitialized -O2` / valgrind
   memcheck / MSan / `-ftrivial-auto-var-init=pattern` (makes it
   deterministic). (E5) **Static init order** (if globals involved) →
   gdb breakpoint in the constructors. Also plain **UB** (D1 signed
   overflow) → `-fsanitize=undefined`. Single-threaded rules out races.
   </details>

2. Container-loop crash that changes with `-O2`. Which build flag pins
   it down fastest?
   <details><summary>Answer</summary>
   `-D_GLIBCXX_DEBUG` (libstdc++ debug mode) — it turns iterator
   invalidation / out-of-range / bad-range into an immediate, precise
   runtime abort with the exact operation. ASan is the backup (post-
   realloc OOB). (A10.)
   </details>

3. A bounds check `if (offset + len > buf_size) return error;` lets a
   huge `len` through. Family, tool, fix.
   <details><summary>Answer</summary>
   **D1/D2 integer overflow/wrap** — `offset + len` overflows (signed →
   UB, unsigned → wraps small) so the sum looks `<= buf_size`. Tool:
   `-fsanitize=undefined` (signed) or reasoning (unsigned). Fix: check
   without adding — `if (len > buf_size - offset)` (after ensuring
   `offset <= buf_size`), or `__builtin_add_overflow`.
   </details>

4. Polymorphic type, `virtual` function "not being called" for objects
   in a `std::vector<Base>`. Name it and fix it.
   <details><summary>Answer</summary>
   **E7 slicing** — `std::vector<Base>` stores `Base` **values**; adding
   a `Derived` copies only the `Base` sub-object, vtable is `Base`'s.
   Fix: `std::vector<std::unique_ptr<Base>>` (or `Base*`), so the full
   `Derived` lives on the heap and virtual dispatch works. Consider
   `Base(const Base&) = delete` to catch accidental slicing at compile
   time.
   </details>

---

## Interview questions

1. "Works at `-O0`, breaks at `-O2`" — list every family that fits and
   how you'd distinguish them.
2. Given a hang: how do you tell deadlock from livelock from infinite
   loop, and what does each need?
3. Signed vs unsigned overflow — which is UB, which wraps, and why that
   matters for the optimizer.
4. Rule of Zero vs Rule of Five — when each, and the bug the Rule of
   Three prevents.
5. Iterator invalidation: which operations invalidate what for `vector`
   vs `deque` vs `list` vs `unordered_map`? (High level.)
6. Static initialization order fiasco — what it is and the standard fix.

---

## Next
→ [`13-exercises.md`](13-exercises.md)

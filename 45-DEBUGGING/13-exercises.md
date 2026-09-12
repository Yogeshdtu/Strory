# 13 — Exercises: fix the bugs, choose the tool, read the transcript

## Prerequisites
- Folder 45 ke `01`–`12`

Folder 45 ka last file. Teen parts:
- **Part A** — `examples/06_buggy_programs/` ke 10 programs formally.
- **Part B** — "yeh symptom diya, kaunsa tool / kya family".
- **Part C** — gdb / sanitizer transcripts padho aur diagnose karo.

Answers `<details>` mein. Buggy-programs ke poore answers unki apni files
ke neeche (`// BUG / SYMPTOM / TOOL / FIX`) mein bhi hain — yahan sirf
approach.

---

## Part A — fix the 10 buggy programs

Har file ke liye: (1) compile + run, actual vs expected note karo.
(2) hypothesis. (3) **tool chuno** aur justify. (4) confirm. (5) fix (ek
change, root cause). (6) verify — non-deterministic hai to loop mein
50–100 baar.

```bash
cd 45-DEBUGGING/examples/06_buggy_programs
g++ -std=c++20 -Wall -Wextra -g -O0 01_off_by_one.cpp -o t && ./t
# sanitizer (Linux/WSL/Clang):
g++ -std=c++20 -g -O1 -fsanitize=address,undefined 01_off_by_one.cpp -o t && ./t
```

### A1 `01_off_by_one.cpp`
<details><summary>Approach</summary>
Symptom: sum kabhi 350+garbage kabhi crash → **OOB read** family (A1).
Tool: ASan (`heap-buffer-overflow READ ... 0 bytes to the right`), ya
`-D_GLIBCXX_ASSERTIONS` (`vector::operator[]` bounds), ya gdb `watch i` →
`i == size` pe ruko → `print scores.size()`. Bug: `i <= size`. Fix:
`i < scores.size()` (ya range-for). Verify: `for i in $(seq 1 50)` clean.
</details>

### A2 `02_uninitialized.cpp`
<details><summary>Approach</summary>
Symptom: kabhi 91 kabhi bada garbage, per-run/machine alag → **uninit
read** (A8). Tool: `g++ -O2 -Wall` → `-Wuninitialized` warn; MSan runtime;
valgrind "uninitialised value". Bug: `int running_max;` unseeded. Fix:
`int running_max = v.empty() ? INT_MIN : v[0];` (`<climits>`) ya
`std::max_element`. Verify: 100 runs same answer.
</details>

### A3 `03_signed_unsigned.cpp`
<details><summary>Approach</summary>
Symptom: non-empty theek, empty pe crash/hang → **unsigned wrap** (D2).
`v.size()` unsigned; empty pe `size()-1` = ~1.8e19 → loop OOB. Tool:
reasoning + ASan (pehli iteration OOB). Fix: `for (size_t i = 0; i + 1 <
v.size(); ++i)` (no subtraction) ya `if (v.size() < 2) return 0;`. Verify:
empty + 1-elem + normal.
</details>

### A4 `04_dangling_view.cpp`
<details><summary>Approach</summary>
Symptom: kabhi "hello" kabhi garbage/empty/crash → **dangling view** (A9/A4).
`first_word` local `std::string` ke buffer ka `string_view` return karta.
Tool: ASan (`stack-use-after-return` / `heap-use-after-free` on `w.data()`).
Fix: `std::string` return karo (owning), ya input string ka hi view lautao.
Verify: ASan clean + repeated runs.
</details>

### A5 `05_use_after_move.cpp`
<details><summary>Approach</summary>
Symptom: `last=''` (empty), no crash → **use-after-move** (E3), silent.
`push` `std::move(o)` ke baad `o.sym` padhta. Tool: clang-tidy
`bugprone-use-after-move`; ya reasoning; ASan yahan kuch nahi bolega
(moved-from is *valid*). Fix: `last_symbol_ = o.sym;` ko move se **pehle**.
Verify: `last='GOOG'`.
</details>

### A6 `06_iterator_invalidation.cpp`
<details><summary>Approach</summary>
Symptom: crash / galat count / ek even bach jaata → **iterator
invalidation** (A10). `v.erase(it)` `it` ko invalid karta, phir `++it` UB.
Tool: `-D_GLIBCXX_DEBUG` → "invalidated iterator" exact line. Fix:
`it = v.erase(it)` else `++it`; ya `std::erase_if(v, ...)`. Verify:
"kept 3 of 6: 1 3 5".
</details>

### A7 `07_integer_overflow.cpp`
<details><summary>Approach</summary>
Symptom: bade numbers pe check "ACCEPT" jab "REJECT" chahiye → **signed
overflow** (D1, UB). `existing + add` (int) overflow → wrap negative → `>
kCap` false. Tool: `-fsanitize=undefined` → "signed integer overflow: ...
cannot be represented in type 'int'". Fix: `if (add > kCap - existing)`
ya wider type. Verify: UBSan clean + REJECT prints + book size 0.
</details>

### A8 `08_double_free_rule_of_three.cpp`
<details><summary>Approach</summary>
Symptom: `buf[0]=7` phir crash ("double free") on return → **Rule-of-Three
violation** (E4/A5). `Buffer` raw `int*` own karta, copy ctor nahi → `use(
original)` ka `copy` aur `original` same pointer, dono dtor `delete[]`.
Tool: ASan ("attempting double-free" + dono stacks); `-O2 -Wuse-after-free`.
Fix: `std::vector<int> data_;` (Rule of Zero) — best. Ya deep-copy
copy-ctor/assign, ya `=delete`. Verify: "done" prints, ASan clean.
</details>

### A9 `09_precedence.cpp`
<details><summary>Approach</summary>
Symptom: "net pay = 2025", expected 1025 — silent, no crash → **precedence**
(C3/C1). `rate*hours + bonus/2` = `(rate*hours) + (bonus/2)`, chahiye
`(rate*hours + bonus)/2`. Tool: **unit test** with hand-computed expected;
gdb `print` dono forms. No sanitizer (well-defined, just wrong). Fix:
`return (rate * hours + bonus) / 2;`. Verify: 1025.
</details>

### A10 `10_data_race.cpp`
<details><summary>Approach</summary>
Symptom: `-O0` pe ~15-20% runs "INCONSISTENT", har run alag; `-O2` chhup
jaata → **data race** (B1). 4 threads, `g_stats.*  += 1` no sync. Tool:
**TSan** (`-fsanitize=thread`) → "data race ... global 'g_stats'" dono
stacks; helgrind. Fix: `std::atomic<long>` fields + `fetch_add(...,
relaxed)`, ya per-thread local `Stats` merge at end. Verify: `for i in
$(seq 1 100)` → 100/100 consistent, TSan clean.
</details>

---

## Part B — symptom → tool / family

### B1.
"Program crashes with `free(): invalid pointer`. gdb `bt` points at a
`delete p;` that looks completely correct — `p` is valid, deleted once."
<details><summary>Answer</summary>
Heap **corruption happened earlier** — some OOB write trampled this
block's allocator metadata; this `delete` is the victim, not the culprit.
`bt` only shows *detection*, not cause. Tool: ASan or valgrind memcheck —
they report the **writing** line. Family: A1 (heap OOB write).
</details>

### B2.
"Unit tests pass. In production, one specific customer sees wrong totals.
Can't reproduce locally with the same inputs."
<details><summary>Answer</summary>
Inputs same but **environment** differs. Suspects: A8 uninit (their stack
garbage differs), B1 race (their load/core-count triggers it), D3
narrowing / platform type sizes, E5 static-init order (their link order),
locale/timezone. First moves: run their exact input under
`-fsanitize=address,undefined` **and** `-fsanitize=thread`; check for
`size_t`/`int` assumptions; capture more logging (`10`) from prod.
</details>

### B3.
"A hang. `htop` shows the process at 0% CPU."
<details><summary>Answer</summary>
0% CPU → threads **blocked**, not spinning. Deadlock (B2/B3) or lost
wakeup (B5) or a blocking syscall waiting forever (socket with no
timeout). `gdb -p <pid>` → `thread apply all bt`. ≥2 in `__lll_lock_wait`
on each other → lock-order deadlock. One in `lock_wait`, rest idle →
missing unlock. One in `pthread_cond_wait` with predicate already true →
lost wakeup.
</details>

### B4.
"Loop over a `std::vector`, sometimes segfaults, sometimes prints
duplicates, only when the vector has >~16 elements."
<details><summary>Answer</summary>
Size threshold → **reallocation**. Iterator/reference invalidation (A10):
`push_back` inside the loop reallocates past capacity, invalidating the
iterator. `<16` fits initial capacity so it "works". Tool:
`-D_GLIBCXX_DEBUG`. Fix: `reserve()`, or index-based, or collect
additions and append after.
</details>

### B5.
"`perf stat` shows 3,000 `major-faults` and 400,000 `context-switches`
for a 2-second run. Output is correct."
<details><summary>Answer</summary>
Not a correctness bug — a **performance/config bug** (`11`). Major faults
= swapping or `mmap` demand paging in the hot path → `mlockall` + prefault
(`29/13`). 400k ctx-switches = lock contention or oversubscription or a
blocking call in a loop → `perf record` for the futex call-site, shrink
critical sections / pin threads (`29/11`).
</details>

### B6.
"After adding `-O3`, a numerical routine returns different (wrong)
results. `-O0` and `-O1` are fine."
<details><summary>Answer</summary>
Families: E8 **strict aliasing** (type punning via pointer casts — test
`-fno-strict-aliasing`; fix with `memcpy`/`bit_cast`), D1 **signed
overflow** (`-fsanitize=undefined`), A8 **uninit** exposed by aggressive
inlining, or **floating-point reassociation** (if `-ffast-math` / `-O3`
vectorized a reduction — check with `-fno-fast-math`; not UB but changes
results). Confirm each with the matching flag; the real fix removes the
UB / pins FP semantics, not the flag.
</details>

---

## Part C — read the transcript

### C1. gdb
```
Thread 1 received signal SIGSEGV, Segmentation fault.
0x... in apply_discount (order=0x0, pct=10) at cart.cpp:44
44        order->total -= order->total * pct / 100;
(gdb) bt
#0  apply_discount (order=0x0, pct=10) at cart.cpp:44
#1  checkout (u=...) at cart.cpp:88
#2  main () at cart.cpp:120
```
Diagnose + next command.
<details><summary>Answer</summary>
`order == 0x0` — null passed into `apply_discount` from `checkout`
(cart.cpp:88). Crash is at :44 but the bug is at :88 (or wherever `order`
was supposed to be set). Next: `frame 1` → `list 88` / `info locals` —
why is the order pointer null there? (lookup returned null and wasn't
checked? a `find` miss? an earlier `delete`?). Fix at the source, not an
`if (order)` at :44.
</details>

### C2. ASan
```
==1==ERROR: AddressSanitizer: heap-use-after-free
READ of size 8 at 0x60300000eff8 thread T0
    #0 Session::send() session.cpp:210
freed by thread T0 here:
    #0 operator delete(void*)
    #1 Registry::drop(int) registry.cpp:55
previously allocated by thread T0 here:
    #1 Registry::create() registry.cpp:31
```
What happened, and the fix direction.
<details><summary>Answer</summary>
A `Session` was created (`Registry::create`), then destroyed
(`Registry::drop`), then `Session::send()` was called on the freed object
(A3 use-after-free). Something still holds a raw pointer/reference to a
`Session` after `drop()` removed it. Fix: ownership model — `drop()`
shouldn't free while references exist; use `shared_ptr` + `weak_ptr` for
observers, or a generation-checked handle (`44` `ObjectPool`) so a stale
handle's `get()` returns null instead of a dangling pointer.
</details>

### C3. TSan
```
WARNING: ThreadSanitizer: data race
  Write of size 4 at 0x5648... by thread T3:
    #0 Metrics::incr() metrics.cpp:20
  Previous read of size 4 at 0x5648... by main thread:
    #0 Metrics::snapshot() metrics.cpp:33
  Location is global 'g_metrics' ...
```
Diagnose + two valid fixes.
<details><summary>Answer</summary>
Data race (B1): worker threads `incr()` (write) a counter while the main
thread `snapshot()` (read) it, with no synchronization / happens-before.
Fixes: (1) make the counter `std::atomic<uint32_t>` — `fetch_add` on
write, `load` on read (relaxed is fine for a stats counter). (2) guard
both with a mutex (heavier; use if `snapshot()` reads many fields that
must be mutually consistent). (3) per-thread counters summed in
`snapshot()` — no shared write at all.
</details>

### C4. valgrind
```
==1== 40 bytes in 1 blocks are definitely lost in loss record 1 of 3
==1==    at 0x484...: operator new(unsigned long)
==1==    by 0x10A...: Parser::parse_node(char const*) parser.cpp:77
==1==    by 0x10B...: Parser::parse() parser.cpp:40
==1== LEAK SUMMARY:
==1==    definitely lost: 40 bytes in 1 blocks
==1==    indirectly lost: 360 bytes in 9 blocks
```
What's the structure, and the fix.
<details><summary>Answer</summary>
`parse_node` `new`s a node; that node is "definitely lost" (no pointer to
it at exit) and it **owns** 9 more nodes ("indirectly lost 360 bytes / 9
blocks" — likely a subtree). So: a parse tree whose root is never freed
(missing `delete`, or an early `return`/exception skipping cleanup).
Fix: RAII — nodes hold `std::unique_ptr<Node>` children, root is a
`unique_ptr`; the whole tree frees itself. Fixing the root leak takes the
9 indirect ones with it.
</details>

---

## Self-check

Tum ready ho jab bina dekhe:

1. Debugging loop ke 5 steps (`01`), aur "reproduce" pehle kyun.
2. `git bisect` kab aur kaise; kitne steps for N commits.
3. gdb: `break`/`watch`/`bt`/`frame`/`finish` — kab kaunsa.
4. Conditional breakpoint hot loop mein slow kyun; 2 alternatives.
5. SIGSEGV vs SIGABRT vs SIGFPE — trigger + first gdb move.
6. `-O2` pe `<optimized out>` kyun; `-Og` kya deta.
7. ASan / UBSan / TSan / MSan — kya pakadta, kaunse combine ho sakte.
8. valgrind vs sanitizers — 3 cases each wins.
9. Deadlock ka gdb signature; livelock se fark.
10. `rr` + `watch` + `reverse-continue` = kaunsa forward workflow replace.
11. HFT async logger: hot path kya kare, queue full pe kya.
12. `perf` se performance-**bug** (off-CPU / syscall storm / false
    sharing) kaise pakdoge.
13. Bug catalog (`12`): symptom "different per run" → kaunse families.

---

**Folder 45 complete.**

## Next
→ [`../46-INTERVIEW-PREP/00-README.md`](../46-INTERVIEW-PREP/00-README.md)

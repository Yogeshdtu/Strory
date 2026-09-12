# Examples — Folder 45 (Debugging)

> Sab portable `.cpp` — MinGW/Windows x86-64 pe **compile** hoti hain.
> `./build.ps1 folder 45-DEBUGGING` → **5/5 OK** (standalone) under strict
> warnings; `06_buggy_programs/` `checkall` (recursive) se hit hote hain.
>
> Kaafi examples **jaan-boojh kar galat chalte hain** (crash / leak / race
> / wrong answer) — compile pe koi error nahi, bug **runtime** pe. Yehi
> point hai: debug karke pakdo.

## The box these transcripts came from

**AMD Ryzen 7 4700U** (Zen 2), **Windows x64 + MinGW-w64 GCC 15.1.0**,
**GDB 16.3**. gdb transcripts lessons `02`/`03`/`04`/`05` mein is box ke
**real** output hain — tum wahi commands `.build/*.exe` pe chala ke milaao.

**Is box pe NAHI:** `libasan`/`libubsan`/`libtsan` (MinGW), `valgrind`,
`rr` (Linux-only). Un tools ke expected output har relevant file ke neeche
comment mein diya hai (Linux/Clang shapes). Chalana ho → WSL / Linux.

## Standalone examples (`examples/*.cpp`)

| File | Kya | Chalta hai? | Lesson |
|---|---|---|---|
| `01_gdb_practice.cpp` | Bug-free program — gdb commands practice (break/step/watch/bt/finish) | ✅ normally | `02`, `03` |
| `02_segfault_debug.cpp` | ⚠️ NULL-deref crash (linked-list loop, no null check) | ❌ SIGSEGV (exit 139) | `04` |
| `03_memory_bugs.cpp` | ⚠️ Menu-driven: `leak` / `uaf` / `oob` / `double` / `uninit` / `stackuaf` | ⚠️ UB per mode | `06`, `07` |
| `04_race_debug.cpp` | ⚠️ Data race: 4 threads `++` a plain `long`; `--safe` = atomic | ⚠️ wrong total (racy) / correct (safe) | `06`, `08` |
| `05_deadlock_debug.cpp` | ⚠️ AB/BA deadlock; **default = safe (ordered)**, `--deadlock` hangs | ✅ default / ⏸ `--deadlock` | `08` |

```bash
./build.ps1 build 45-DEBUGGING/examples/01_gdb_practice.cpp
gdb -q .build/01_gdb_practice.exe          # lesson 02/03 ke transcripts try karo

./build.ps1 build 45-DEBUGGING/examples/02_segfault_debug.cpp
.build/02_segfault_debug.exe ; echo $?     # 139
gdb -q .build/02_segfault_debug.exe -ex run -ex bt -ex "print p" -ex "print total"

g++ -std=c++20 -O2 -g -pthread 45-DEBUGGING/examples/04_race_debug.cpp -o race
for i in $(seq 1 20); do ./race; done       # har baar alag < 2000000
./race --safe                                # 2000000

g++ -std=c++20 -O0 -g -pthread 45-DEBUGGING/examples/05_deadlock_debug.cpp -o dl
./dl                                         # safe: completes
./dl --deadlock & gdb -p $! -batch -ex "thread apply all bt" -ex kill
```

### Linux / WSL — sanitizers

```bash
g++ -std=c++20 -O1 -g -fsanitize=address,undefined 45-DEBUGGING/examples/03_memory_bugs.cpp -o mb
./mb leak ; ./mb uaf ; ./mb oob ; ./mb double ; ./mb stackuaf     # ek-ek clean ASan report
clang++ -std=c++20 -O1 -g -fsanitize=memory 45-DEBUGGING/examples/03_memory_bugs.cpp -o mb_m && ./mb_m uninit

g++ -std=c++20 -O1 -g -pthread -fsanitize=thread 45-DEBUGGING/examples/04_race_debug.cpp -o race_t && ./race_t
```

## `06_buggy_programs/` — 10 "find and fix" drills

Har ek mein **theek ek bug**, compile clean, runtime pe misbehave. `13-exercises.md`
Part A inhe formally assign karta. Answers har file ke neeche
(`// BUG / SYMPTOM / TOOL / FIX`) + `06_buggy_programs/README.md`.

| # | File | Family | "Best tool" |
|---|---|---|---|
| 01 | `01_off_by_one.cpp` | OOB read (A1) | ASan · `-D_GLIBCXX_ASSERTIONS` |
| 02 | `02_uninitialized.cpp` | uninit read (A8) | `-Wuninitialized -O2` · MSan · valgrind |
| 03 | `03_signed_unsigned.cpp` | unsigned wrap (D2) | reasoning · ASan |
| 04 | `04_dangling_view.cpp` | dangling `string_view` (A9) | ASan (use-after-return) |
| 05 | `05_use_after_move.cpp` | use-after-move (E3) | clang-tidy · review |
| 06 | `06_iterator_invalidation.cpp` | invalidation (A10) | `-D_GLIBCXX_DEBUG` · ASan |
| 07 | `07_integer_overflow.cpp` | signed overflow (D1) | `-fsanitize=undefined` |
| 08 | `08_double_free_rule_of_three.cpp` | Rule of Three (E4) | ASan · `-O2 -Wuse-after-free` |
| 09 | `09_precedence.cpp` | operator precedence (C3) | unit test · gdb `print` |
| 10 | `10_data_race.cpp` | data race (B1) | TSan · helgrind |

```bash
cd 45-DEBUGGING/examples/06_buggy_programs
g++ -std=c++20 -Wall -Wextra -g -O0 07_integer_overflow.cpp -o t && ./t
g++ -std=c++20 -g -O1 -fsanitize=undefined -fno-sanitize-recover=all 07_integer_overflow.cpp -o t && ./t
g++ -std=c++20 -g -O0 -D_GLIBCXX_DEBUG 06_iterator_invalidation.cpp -o t && ./t
```

## Verify

```bash
./build.ps1 folder 45-DEBUGGING     # 5 standalone: 5/5 OK (strict warnings)
./build.ps1 checkall                # + 10 buggy_programs compile clean (-Wall -Wextra, -O0)
```

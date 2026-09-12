# 06_buggy_programs — inhe fix karo

10 chhote programs. **Har ek mein theek ek bug hai.** Yeh folder 45 ka
practice range hai — `13-exercises.md` inhe formally assign karta hai.

## Kaam ka tareeka (har file ke liye)

1. **Compile karo aur chalao** — dekho actual output vs expected (file ke
   top comment mein diya).
2. **Hypothesis banao** — kya galat lag raha, kyun. (`01-debugging-mindset.md`)
3. **Tool chuno** — reasoning? gdb? sanitizer? `-D_GLIBCXX_DEBUG`?
   `-Wmaybe-uninitialized -O2`? Har bug ka "best tool" alag hai — yehi
   seekhna hai.
4. **Confirm karo** — tool ne exact line/cause dikhaya?
5. **Fix karo** — ek change. Root cause, symptom nahi.
6. **Verify** — output ab sahi? Non-deterministic bug tha to **loop mein**
   50–100 baar chalao (`for i in $(seq 1 100); do ./t || break; done`).

**Answers file ke sabse neeche `// BUG / SYMPTOM / TOOL / FIX` comment
mein hain — pehle KHUD try karo.**

## Files

| # | File | Bug family | "Best tool" |
|---|---|---|---|
| 01 | `01_off_by_one.cpp` | logic / OOB read | ASan · `-D_GLIBCXX_ASSERTIONS` · gdb `watch` |
| 02 | `02_uninitialized.cpp` | uninitialised read | `-Wmaybe-uninitialized -O2` · MSan · valgrind |
| 03 | `03_signed_unsigned.cpp` | unsigned underflow | reason about types · ASan · gdb |
| 04 | `04_dangling_view.cpp` | lifetime / dangling `string_view` | ASan (use-after-return) |
| 05 | `05_use_after_move.cpp` | lifetime / moved-from read | clang-tidy `bugprone-use-after-move` · review |
| 06 | `06_iterator_invalidation.cpp` | container invalidation | `-D_GLIBCXX_DEBUG` · ASan |
| 07 | `07_integer_overflow.cpp` | signed overflow (UB) | `-fsanitize=undefined` · `-ftrapv` |
| 08 | `08_double_free_rule_of_three.cpp` | rule-of-three / double free | ASan · valgrind · `-O2 -Wuse-after-free` |
| 09 | `09_precedence.cpp` | operator precedence (silent) | unit test · gdb `print` two ways |
| 10 | `10_data_race.cpp` | data race (UB) | TSan · helgrind · reason |

## Build

```bash
# ek file
g++ -std=c++20 -Wall -Wextra -g -O0 01_off_by_one.cpp -o t && ./t

# sanitizer (Linux / Clang -- MinGW box pe libasan/tsan nahi)
g++ -std=c++20 -g -O1 -fsanitize=address,undefined 07_integer_overflow.cpp -o t && ./t
g++ -std=c++20 -g -O1 -pthread -fsanitize=thread   10_data_race.cpp        -o t && ./t

# libstdc++ debug mode -- iterator/bounds bugs turant
g++ -std=c++20 -g -O0 -D_GLIBCXX_DEBUG 06_iterator_invalidation.cpp -o t && ./t
```

> `./build.ps1 checkall` in sabko **compile** karta (`-Wall -Wextra`, `-O0`)
> aur "OK" ginta — bug **runtime** pe hai, compile pe nahi. `02` aur `08`
> `-O2` pe GCC se ek warning bhi dete (wahi bug, statically pakda) — woh
> intentional hai, `checkall` ke `-O0` pe clean.

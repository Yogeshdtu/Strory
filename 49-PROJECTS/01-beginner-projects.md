# 01 — Beginner projects

## Prerequisites
- Folders `01`–`11` (basics → structs). File I/O: folder `04`. `std::vector` /
  `std::string`: folders `09`, `10`.

## Yeh file kya hai
6 chhote **complete programs** — snippet nahi, poora `main()` jo kuch asli karta
hai. Har project: **spec → milestones (v1 → v2 → v3) → kaunse concepts →
tests → extensions.**

Pehle spec padho, khud banane ki koshish karo (`./build.ps1 myfile.cpp`), phir
`examples/beginner/` ka reference dekho. Reference sab strict warnings ke neeche
compile hota hai aur assertions/demo chalata hai.

---

## P1 — Calculator (`examples/beginner/01_calculator.cpp`)

**Spec:** ek expression string leta hai (`"3 + 4 * 2"`) aur sahi answer deta hai,
operator precedence ke saath. Integer aur floating dono.

**Milestones:**
- **v1** — sirf `a op b` (do operands, ek operator). `switch` on the operator.
- **v2** — multiple operators, left-to-right, no precedence (`3 + 4 * 2 = 14`).
- **v3** — precedence + parentheses. Shunting-yard ya recursive-descent parser.
  Divide-by-zero → error, not crash.

**Concepts:** `switch` (06), loops (07), functions (08), `std::string` tokenizing
(10), `std::optional` / error handling (23), a tiny **stack** for shunting-yard (20/07).

**Tests:** `"2+2" → 4`, `"3+4*2" → 11`, `"(3+4)*2" → 14`, `"10/0" → error`,
`"7 - 3 - 2" → 2` (left-assoc), empty / garbage input → error.

**Extensions:** unary minus; `^` (right-assoc); variables (`x = 5`); a REPL loop.

---

## P2 — Number guessing game (`examples/beginner/02_number_guess.cpp`)

**Spec:** program ek random number `[1, 100]` sochta hai; player guesses; program
"higher"/"lower" batata hai; win pe attempts count print.

**Milestones:**
- **v1** — fixed secret, `std::cin >> guess`, higher/lower loop.
- **v2** — `<random>` proper (`std::mt19937` + `uniform_int_distribution`, seeded
  from `std::random_device`). Input validation (non-numeric → reprompt).
- **v3** — difficulty levels (range + max attempts); "play again?"; track best score.

**Concepts:** `<random>` done right (not `rand() % 100` — bias + weak), `std::cin`
failure states + `clear()`/`ignore()` (04), loops (07), binary-search intuition
(optimal strategy ≤ 7 guesses) (20/05).

**Tests:** feed a scripted guess sequence via `std::istringstream`, assert the
hint direction and the final attempt count. Non-numeric input recovers.

**Extensions:** the program *guesses your* number (binary search); a 2-player mode.

---

## P3 — Student manager (`examples/beginner/03_student_manager.cpp`)

**Spec:** in-memory list of students (`{id, name, marks[subjects]}`). Add,
remove, search by id, list sorted by average, class topper, subject-wise average.

**Milestones:**
- **v1** — `struct Student`, a `std::vector<Student>`, add + print-all.
- **v2** — search by id (linear), remove by id (erase-remove), per-student average.
- **v3** — sort by average (`std::sort` + comparator), topper, subject averages
  (`std::accumulate`), pretty table output (`<iomanip>`).

**Concepts:** `struct` + aggregate init (11), `std::vector` operations (09/19),
`std::sort` + lambda comparator (19/20), `std::accumulate` (19), `<iomanip>`
formatting (04).

**Tests:** add 3 students, assert count; remove one, assert gone; averages
computed right; topper is the max-average student; empty list handled.

**Extensions:** persist to a CSV file (→ P2 of intermediate); grade letters;
weighted subjects; undo last operation (command stack).

---

## P4 — Expense tracker (`examples/beginner/04_expense_tracker.cpp`)

**Spec:** record expenses (`{date, category, amount, note}`). Total, total by
category, monthly total, top-3 categories, over-budget warning.

**Milestones:**
- **v1** — `struct Expense`, add + total.
- **v2** — group by category (`std::map<std::string, long long>` in **paise/cents**,
  not `double` — money is exact), monthly filter.
- **v3** — top-3 categories (`std::partial_sort` / a size-3 heap), budget per
  category + warnings, a simple bar chart with `#` characters.

**Concepts:** **integer money** (why not `double` — 03/float pitfalls), `std::map`
(19), `std::partial_sort` / `nth_element` (19/20), date as `int yyyymmdd` or
`std::chrono::year_month_day` (22).

**Tests:** amounts stored as integers round-trip exactly; category totals sum to
the grand total; top-3 is actually the 3 largest; a category over budget triggers
exactly one warning.

**Extensions:** CSV import/export; recurring expenses; currency; a "this month vs
last month" delta.

---

## P5 — File reader / word stats (`examples/beginner/05_file_word_stats.cpp`)

**Spec:** ek text file padho; lines, words, characters count karo; top-N most
frequent words (case-insensitive, punctuation stripped); average word length;
longest line.

**Milestones:**
- **v1** — open a file, count lines + chars; handle "file not found".
- **v2** — tokenize into words (`std::istringstream` / manual scan), count them
  in an `std::unordered_map<std::string, int>`.
- **v3** — normalize (lowercase, strip non-alnum), top-N by frequency
  (`std::partial_sort` on pairs), stable tie-break (alphabetical).

**Concepts:** `std::ifstream` + RAII + error checking (04, 17), `std::getline`,
`std::unordered_map` (19), `std::transform` + `std::tolower` (**cast to
`unsigned char` first!** — 11/UB), sorting pairs (19).

**Tests:** run on a known small fixture string (via `std::istringstream`), assert
line/word/char counts and the top-3 words. Missing file → clean error, non-zero
exit.

**Extensions:** read from `stdin` if no filename (Unix filter style); regex word
matching; a `--ignore-stopwords` flag; Zipf's-law plot.

---

## P6 — Text / config parser (`examples/beginner/06_config_parser.cpp`)

**Spec:** parse an INI-ish config (`key = value`, `[section]` headers, `#`
comments, whitespace-tolerant) into a lookup structure. Typed getters
(`get_int`, `get_bool`, `get_string` with defaults). Report the line number on a
parse error.

**Milestones:**
- **v1** — `key=value` lines only, into `std::map<std::string, std::string>`.
- **v2** — sections (`[db]` → keys become `db.host` etc.), comments, blank lines,
  trim spaces around `=`.
- **v3** — typed getters with defaults; `get_int("port", 8080)`; a strict mode
  that throws with `"line 12: expected '='"`; round-trip (parse → serialize → parse).

**Concepts:** string trimming / splitting (10), `std::map` nested keys, `std::optional`
+ `std::from_chars` for typed parse (10/23), error messages with context (23),
RAII file handling (17).

**Tests:** a fixture config string parses to the expected map; `get_int` returns
the value / the default / errors on `"abc"`; a malformed line reports the right
line number; comments and blank lines are ignored.

**Extensions:** environment-variable interpolation (`${HOME}`); include another
file; JSON output (→ advanced P6); a schema/validation pass.

---

## How to work these

1. Read the spec, write your own **v1** — smallest thing that runs.
2. Add a test (even just `assert`s in `main`, or a scripted `std::istringstream`).
3. v2, v3 — each a small, testable step. Commit between.
4. Compare with `examples/beginner/` — not "did it run" but *edge cases, error
   handling, and the data-structure choice*.
5. Pick one extension and do it.

See [`04-project-guidelines.md`](04-project-guidelines.md) for the full checklist.

## Next
→ [`02-intermediate-projects.md`](02-intermediate-projects.md)

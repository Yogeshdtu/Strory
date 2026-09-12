# 04 — Project guidelines

Kaise ek project ko shuru se khatam tak le jaana hai — approach, testing, aur ek
code-review checklist. Yeh har project (`01`–`03`) pe apply hota hai.

---

## 1. Approach — v1 first, always

```
Spec padho → sabse chhoti cheez jo RUN karti hai (v1) → ek test →
v2 (ek naya feature + test) → v3 → measure → refactor → extension
```

- **Don't design the whole thing up front.** Write the crappy version, feel where
  it hurts, then fix *that*.
- **A test before the next feature.** Even `assert`s in `main`. It pins the
  behaviour so v2 can't silently break v1.
- **Commit between milestones.** `git commit -m "P1 v2: transfer is atomic"`.
- **One data-structure decision at a time**, and write down *why* (`std::map` for
  ordered scan; `unordered_map` reserved for point lookup; flat array for a dense
  key range — cheatsheet `02`).

---

## 2. Structure

```
project/
  main.cpp            # arg parsing, wiring, the demo/REPL
  core.hpp / core.cpp # the data model + operations (no I/O here)
  io.hpp / io.cpp     # file/parse/format — the edges
  tests.cpp           # assertions; run it in CI
```

- **Keep I/O at the edges.** The core (`Bank`, `Inventory`, `JsonValue`) takes
  values / spans, returns values / results — no `std::cout`, no `std::ifstream`
  inside it. This is what makes it testable.
- One class = one responsibility + one invariant it guards.
- Free functions over member functions when there's no state.

---

## 3. Error handling

| Situation | Use |
|---|---|
| "no value" is normal (key not found) | `std::optional<T>` |
| operation can fail with a *reason* the caller handles | `enum class Result` / `std::expected<T,E>` |
| programmer error / broken invariant | `assert` (debug) — should never fire |
| truly exceptional, caller usually can't recover | `throw` |
| hot path | error codes, never exceptions |

- Parse errors carry **context**: `"line 12 col 4: expected '='"`, not `"parse error"`.
- Never `catch (...)` and swallow. Never `catch` just to `log-and-rethrow`.
- Validate at the boundary (input parse); trust the core.

---

## 4. Money, time, IDs

- **Money = integer** paise/cents. `double` for money is a bug (rounding).
  Rational rates (`6% = 6/100`), integer math (43/14).
- **Time**: `std::chrono` types, or an explicit `int64_t` epoch-ns / `yyyymmdd`.
  Never parse dates with `sscanf` and hope.
- **IDs**: a distinct type (`enum class Sku : uint64_t` or a `struct`), not a bare
  `int` you can accidentally add.

---

## 5. Testing

- **Invariant checks** are the highest-value test: `Σ balances == Σ deposits −
  Σ withdrawals`, `book is never crossed`, `qty ≥ 0`, `parse → serialize → parse
  is identity`.
- **Property tests**: run 10 000 random valid operations, assert the invariant
  after each. Cheap, finds the weird interleavings.
- **Edge cases first**: empty, one element, full/at-capacity, duplicate,
  max value (overflow), malformed input, file-not-found.
- **Golden tests**: a fixed input file → a fixed expected output; diff.
- **Determinism**: seed every RNG; if you have a log, replaying it must
  reproduce the exact state (this is also your crash-recovery test).
- Run under **sanitizers** (Linux/WSL): `-fsanitize=address,undefined`. On MinGW:
  `-D_GLIBCXX_ASSERTIONS -fstack-protector-all`.
- This repo: `./build.ps1 <file>` (debug + run), `./build.ps1 fast <file>`
  (`-O2`, for benchmarks), `./build.ps1 checkall` (whole-repo compile check).

---

## 6. Measuring (advanced projects)

- `-O2`, realistic input size, **min of several runs**, stop dead-code
  elimination from deleting your result (`asm volatile("" :: "r"(x) : "memory")`).
- `-O0` numbers are meaningless — say so if you ever quote one.
- Report **ns/op** and **p99.9**, not just the mean (tail matters — 35, 43/16).
- Profile before optimizing (`perf stat`, `perf record`) — cheatsheet `07`.
- **One change, then re-measure, then explain** *why* it moved (cache miss?
  branch? syscall? lock contention? — cheatsheet `07` signal→fix table).
- If a result surprises you, that's the lesson — write it down, don't bury it.

---

## 7. Code-review checklist

**Correctness**
- [ ] Every `new`/`malloc` has an owner and a matching free (or is in an RAII type)
- [ ] No UB: signed overflow, OOB, uninitialized read, dangling ref/iterator/`string_view`, data race (cheatsheet `11`)
- [ ] Integer conversions are intentional (`-Wconversion` clean)
- [ ] Error paths tested, not just the happy path
- [ ] Invariants hold after *every* public operation (assert them in tests)
- [ ] Move leaves the source valid; self-assignment / self-move safe
- [ ] `switch` covers all `enum` values (or has a `default` that errors)

**Design**
- [ ] I/O is at the edges; the core is pure and testable
- [ ] One responsibility per class; the invariant is named
- [ ] Rule of 0 (prefer) — special members only when a resource is owned directly
- [ ] The right container for the access pattern, and a comment saying why
- [ ] Public API is minimal; implementation details are `private` / in the `.cpp`

**Clarity**
- [ ] Names say what, not how; no `data2`, `tmp3`
- [ ] Functions do one thing; ~1 screen
- [ ] Comments explain *why*, not *what* the code already says
- [ ] No commented-out code, no dead code

**Robustness**
- [ ] Bad input → a clear error with context, not a crash or silent wrong answer
- [ ] Resource limits considered (huge file, deep recursion, full disk, many connections)
- [ ] `assert`s are for "can't happen"; real errors are handled

**Build**
- [ ] Compiles clean at `-Wall -Wextra -Wshadow -Wconversion` (this repo's bar)
- [ ] Runs clean under a sanitizer (or the MinGW fallback)
- [ ] A test target that CI can run

---

## 8. When it's "done"

A project is done when: it handles the edge cases in its spec, has an invariant
test and a golden test, compiles warning-clean, runs sanitizer-clean, and you can
explain every data-structure choice. Then pick one extension — that's where the
learning compounds.

## Next
→ [`05-hft-projects-link.md`](05-hft-projects-link.md)

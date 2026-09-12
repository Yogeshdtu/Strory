# Examples — Folder 08 (Functions)

| File | Kaunse lesson se | Kya dikhata hai |
|---|---|---|
| `01_first_functions.cpp` | 01, 02 | Basic functions, `void`, function-calls-function, declare-before-use, DRY |
| `02_call_stack_trace.cpp` | 05, 06 | Stack frames ke addresses (ghatate hain), recursion frame size, bada local = bada frame |
| `03_overloading.cpp` | 08 | Overload resolution — exact/promotion/conversion, count-based, `const`-based, ambiguity (comments mein) |
| `04_recursion.cpp` | 09 | Factorial, **Fibonacci naive vs memo vs iter — measured exponential**, sumDigits/power/gcd, depth |
| `05_stack_overflow.cpp` | 05, 09 | ⚠️ **Jaan-boojh kar CRASH** — no base case → stack overflow. Compile OK, run pe crash |
| `06_inline_asm_check.cpp` | 05, 10 | Inlining ka assembly + benchmark — **inline vs real-call ~6x, ~1.9 ns/call** |
| `07_command_line_args.cpp` | 13 | `argc`/`argv`, `argv[argc] == nullptr`, `--sum` flag parsing, `from_chars` |
| `08_namespaces.cpp` | 14 | Do exchanges ke `lot_size` (naam ki takkar nahi), namespace reopen, nested `a::b::c` + alias, `::global`, using-declaration vs directive, anonymous namespace (internal linkage), inline namespace (versioning), **ADL** (`getline` bina `std::`, `std::operator<<`). ⚠️ `-Wshadow` warning jaan-boojh kar (global `limit` ko chhupana) |

## Compile karne ka tarika

```bash
# repo root se (Windows / PowerShell):
./build.ps1 08-FUNCTIONS/examples/01_first_functions.cpp

# Linux / Mac / Git-Bash:
make FILE=08-FUNCTIONS/examples/01_first_functions.cpp

# manual (debug):
g++ -std=c++20 -Wall -Wextra -Wshadow -g 01_first_functions.cpp -o out && ./out
```

**Benchmarks — `-O2` (ya zyada):**
```bash
./build.ps1 fast 08-FUNCTIONS/examples/04_recursion.cpp        # fib exponential
./build.ps1 fast 08-FUNCTIONS/examples/06_inline_asm_check.cpp  # call overhead

# 06 ka asli demo -- assembly:
./build.ps1 asm 08-FUNCTIONS/examples/06_inline_asm_check.cpp   # `call` hai ya inline?
```

## ⚠️ `05_stack_overflow.cpp` — deliberately crashes

Yeh file **jaan-boojh kar crash hoti hai** (jaise folder 02 ka
`06_broken_on_purpose.cpp`, par yeh **runtime** crash hai, compile nahi):

- **Compile:** clean OK. `make folder` / `checkall` isse "OK" dikhayenge (woh sirf compile karte hain).
- **Run:** stack overflow → Linux/Mac `Segmentation fault` (exit 139), Windows exit
  `0xC00000FD`. **Koi exception nahi** — OS process ko maar deta hai.
- Isse **haath se** chalao aur crash dekho:
  ```bash
  g++ -std=c++20 -O0 -g 08-FUNCTIONS/examples/05_stack_overflow.cpp -o so && ./so
  echo $?
  ```
- Depth (kitni door pahuncha) OS/shell ke stack size pe depend karta hai (Windows
  ~1 MB / ~1000 frames; Linux ~8 MB). Point: **crash hota hai**.

## Jaan-boojh kar warning

- **`05_stack_overflow.cpp`** — `-Winfinite-recursion` warning deti hai (part of
  `-Wall`). Wahi lesson hai — compiler base-case-missing recursion ko khud
  pakadta hai. Baaki 6 files clean compile karti hain.

## Measured results (aapke machine pe alag ho sakte hain)

GCC 15.1.0, `-O2`, x86-64:

| Benchmark | Result |
|---|---|
| `04` — `fibNaive(30 / 35 / 40)` | ~2.2 / ~23 / ~290 ms (**~11x per +5** — exponential) |
| `04` — `fibMemo(90)` / `fibIter(90)` | ~0.001 ms / instant (O(n)) |
| `06` — `addInline` vs `addNoInline` (200M calls) | **~6x** (~75 ms vs ~448 ms), **~1.9 ns/call** overhead |
| `05` (lesson) — `-O0` prologue/epilogue vs `-O2` fully inlined | frame + `call` gayab at `-O2` |

## Sab ek saath (05 chhod ke)

```bash
for f in 08-FUNCTIONS/examples/0[1234670]*.cpp; do
    echo "======== $f ========"
    g++ -std=c++20 -O2 -Wall -Wextra "$f" -o "/tmp/$(basename "$f" .cpp)" \
      && "/tmp/$(basename "$f" .cpp)"
done

# quick compile-check (saari 8):
./build.ps1 folder 08-FUNCTIONS          # Windows
make folder DIR=08-FUNCTIONS             # Linux/Mac/Git-Bash
```

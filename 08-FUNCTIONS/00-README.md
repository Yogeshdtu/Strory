# 08 — FUNCTIONS (PHASE 3)

## Prerequisites
`07-LOOPS`, `06-braces-blocks-and-scope.md` (folder 02)

## Yeh folder kyun
Programming ka paanchva building block — **reuse**.

Aur yahan **call stack** samjhenge, jo pointers, recursion, aur debugging ka foundation hai.

## Is folder ki files

| # | File | Kya seekhoge |
|---|------|--------------|
| 01 | `01-what-is-a-function.md` | Function kya hai, kyun chahiye, DRY principle |
| 02 | `02-declaration-vs-definition.md` | Prototype, definition, header/source split, kyun aur kaise |
| 03 | `03-parameters-and-arguments.md` | Parameter vs argument, pass by value, copies ki cost |
| 04 | `04-return-values.md` | Return type, multiple returns, `void`, `[[nodiscard]]` |
| 05 | `05-the-call-stack.md` | **Stack frames deep dive** — prologue/epilogue, return address, locals, stack overflow |
| 06 | `06-scope-and-lifetime.md` | Local scope, `static` locals, globals, lifetime rules |
| 07 | `07-default-arguments.md` | Default args, rules, gotchas (declaration mein hi, ek baar) |
| 08 | `08-function-overloading.md` | Overloading, **overload resolution** ka poora process, ambiguity errors |
| 09 | `09-recursion.md` | Recursion, base case, stack depth, tail recursion, iteration se comparison |
| 10 | `10-inline-functions.md` | `inline` ka asli matlab (ODR, not 'make it fast'), compiler inlining |
| 11 | `11-constexpr-functions.md` | `constexpr` functions deep, compile-time evaluation |
| 12 | `12-function-attributes.md` | `noexcept`, `[[nodiscard]]`, `[[maybe_unused]]`, `[[deprecated]]` |
| 13 | `13-main-arguments.md` | `int main(int argc, char* argv[])` — command line arguments |
| 14 | `14-exercises.md` | Practice + call stack tracing |

## Examples

| File | Kya |
|---|---|
| `examples/01_first_functions.cpp` | Basic functions |
| `examples/02_call_stack_trace.cpp` | Stack frames dekhna |
| `examples/03_overloading.cpp` | Overload resolution demo |
| `examples/04_recursion.cpp` | Factorial, fibonacci, stack depth |
| `examples/05_stack_overflow.cpp` | ⚠️ Deliberately crash — stack limit dekho |
| `examples/06_inline_asm_check.cpp` | Inlining ka assembly mein asar |
| `examples/07_command_line_args.cpp` | argc/argv |

## Time
1–2 hafte

## Status
✅ **COMPLETE** (Batch 3, PHASE 3 ka aakhri folder). 13 lessons + exercises + 7
compile-verified examples. Highlights: **call stack deep dive** (frames,
prologue/epilogue, real assembly), `05_stack_overflow.cpp` (deliberate crash),
measured — fib exponential (~11x per +5), inline vs real-call (~6x, ~1.9 ns/call).

## Next
→ [`../09-ARRAYS/00-README.md`](../09-ARRAYS/00-README.md)

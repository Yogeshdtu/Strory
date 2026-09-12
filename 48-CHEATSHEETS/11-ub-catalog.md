# 11 — Undefined behaviour quick list

UB = the standard imposes **no requirements**. The compiler may assume it never
happens and optimize on that assumption → "impossible" code paths, deleted
checks, time-travel. Deep: folders `03/05`, `12`, `13`, `45/12`, `46/17`.
Catch it: `-fsanitize=undefined,address`, `-Wall -Wextra`, `-D_GLIBCXX_ASSERTIONS`.

> UB ≠ *implementation-defined* (documented per-impl, e.g. `sizeof(int)`) and
> ≠ *unspecified* (impl chooses, need not document, e.g. arg eval order).

---

## Memory & pointers

- Dereferencing `nullptr` or a dangling / uninitialized / freed pointer
- Reading an **uninitialized** scalar (`int x; use(x);`)
- Out-of-bounds array / `vector::operator[]` / pointer access (even forming a
  pointer >1 past the end; computing `&a[-1]`)
- Use-after-free / use-after-return (pointer/ref to a local that's gone)
- Use-after-move only if you rely on the value (moved-from = valid but unspecified)
- `delete` mismatched with `new[]` (or `free` vs `new`); double `delete`
- Accessing an object through an incompatible type (**strict aliasing**) — use
  `memcpy` / `std::bit_cast` instead of `reinterpret_cast` punning
- Misaligned access via a pointer cast to a stricter-aligned type
- Modifying a `const` object (that was really `const`) or a string literal
- Data race: two threads, same memory, ≥1 write, no synchronization
- `this` used after the object's lifetime; calling a virtual in a ctor/dtor
  reaching a not-yet/already-destroyed override is *defined* but usually wrong

---

## Integers & arithmetic

- **Signed** integer overflow (`INT_MAX + 1`) — unsigned wraps, is defined
- `INT_MIN / -1` and `INT_MIN % -1` (overflows)
- Division / modulo by zero (integer). FP `/0.0` is defined (→ inf/nan) unless
  `-ffast-math`
- Shift by ≥ width, or by a negative amount (`x << 32` for 32-bit `x`)
- Left-shifting a negative value (`<< ` on negative; C++20 relaxed this)
- Converting a floating value out of the target integer's range
- Narrowing that overflows in `{}`-init is an *error*, elsewhere just wraps/UB
- `char` may be signed → `std::toupper((char)c)` with c>127 is UB; cast to
  `unsigned char` first

---

## Language rules

- Reaching the end of a non-`void` function without `return` (except `main`)
- Modifying a variable twice with no sequence point between (`i = i++;`,
  `a[i] = i++;`) — mostly fixed for assignments in C++17, still avoid
- Infinite loop with no side effects and no I/O (compiler may assume it terminates)
- Calling a function through a pointer of the wrong type
- `va_arg` with the wrong type; `printf("%d", 3.0)` / `%s` on a non-string
- Recursion / allocation that overflows the stack (no diagnostic required)
- ODR violation: two different definitions of the same entity across TUs
- Accessing a `union` member that isn't the active one (type-pun via union is a
  GCC/Clang *extension*, not standard C++)
- Falling off the end of `std::string::operator[](size())` write (reading the
  `'\0'` is fine; writing anything but `'\0'` is UB)

---

## STL-specific

- Dereferencing `end()`, or any invalidated iterator (see `02-stl-containers.md`)
- `vector::front()/back()/pop_back()` on an empty vector
- `std::sort` / `lower_bound` with a comparator that isn't a **strict weak
  ordering** (`<=`, `>=`, or non-transitive) — OOB access, crash, loop
- Erasing while iterating without using `erase`'s return value
- `std::string_view` outliving its backing string; passing a non-null-terminated
  `sv.data()` to a C API
- Self-move / self-swap that you didn't make safe
- `std::optional::operator*` / `value()` on an empty optional (`value()` throws,
  `*` is UB)
- `std::variant` `get<T>` on the wrong alternative (`get` throws, `get_if`
  returns null — that's the safe one)
- Detaching a `std::thread` and touching captured locals after they die
- `std::vector<bool>` — not a container of `bool&`; taking `&v[i]` doesn't give
  a `bool*`

---

## The "it worked on my machine" traps

| Looks fine because | Actually UB |
|---|---|
| x86 is strongly ordered | missing `acquire`/`release` → breaks on ARM |
| `-O0` didn't reorder | `-O2` deletes your null check after a deref |
| small overflow "just wrapped" | signed overflow → loop bound assumption removed |
| the pointer "was still valid" | UAF the allocator hadn't reused yet |
| the test passed 1000× | data race that surfaces 1-in-1e6 under load |

**Rule:** if a sanitizer or `-Wall -Wextra` flags it, it's real — fix it, don't
suppress it.

## Next
→ [`12-hft-glossary.md`](12-hft-glossary.md)

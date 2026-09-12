# Examples — Folder 15 (Classes)

| File | Lesson | Kya dikhata hai |
|---|---|---|
| `01_first_class.cpp` | 01, 02, 03 | `BankAccount` — private data + public interface + invariant (`balance >= 0`), `this->` |
| `02_constructors.cpp` | 04 | default / param / delegating / copy ctors ka call order; `= delete` (non-copyable `Unique`) |
| `03_initializer_list.cpp` | 05 | init list vs body assignment (Tracer prints); const/ref members; **member ORDER trap** (2 intentional warnings) |
| `04_const_methods.cpp` | 07 | `const` method + `mutable` cache/counter; `const`/non-`const` `at()` overload; `const Report&` param |
| `05_static_members.cpp` | 06, 08 | `inline static` counters (`s_liveCount`/`s_totalEver`/`s_nextId`); static methods; dtor decrements |
| `06_operator_overload.cpp` | 09 | `Money` — `+=` member, `+`/`-`/`*` free, unary `-`, prefix/postfix `++`, `<<` free, `<=>`/`==` default, `explicit` ctor |
| `07_class_layout.cpp` | 13 | `sizeof`: Empty=1, methods-only=1, 2-int class=8; Bad(24) vs Good(16) padding; `offsetof`; `is_polymorphic`=false |
| `08_order_class.cpp` | 01, 15 | HFT-style `Order` — encapsulated fields, ctor validation, state machine (fill/cancel), `static_assert` trivially-copyable + `sizeof==32` |

## Compile / run

```bash
./build.ps1 15-CLASSES/examples/01_first_class.cpp        # Windows
make FILE=15-CLASSES/examples/01_first_class.cpp          # Linux/Mac/Git-Bash
```

## Jaan-boojh kar warnings

- **`03_initializer_list.cpp`** — **jaan-boojh kar** 2 warning categories:
  - `-Wreorder` — `BadOrder(int x) : b_(x), a_(b_ + 1)` (init-list order ≠
    declaration order).
  - `-Wuninitialized` — `a_(b_ + 1)` reads `b_` before it is initialized (`a_`
    is declared first, so it's initialized first, when `b_` is still garbage).

  Wahi lesson hai: **members declaration order mein init hote hain**, init-list
  ka likha order cosmetic hai. Baaki 7 files clean compile karti hain (full
  `-Wall -Wextra -Wshadow -Wconversion -Wsign-conversion -Wpedantic`).

## Notes

- No `broken_on_purpose` file — `03` compiles and runs (garbage `a_` visible in
  output), it's a warning-demo not a compile failure.
- `06_operator_overload.cpp` needs `<compare>` for `operator<=>`.
- `07_class_layout.cpp` uses `struct` for the two padding-demo types so
  `offsetof(...)` can name members from outside — the lesson point (access
  doesn't change layout) is made via the `Accessors` **class** (8 bytes, 4
  methods, private data) and `std::is_polymorphic`.
- `08_order_class.cpp` — `static_assert(sizeof(Order) == 32)` locks the layout;
  add a member or `virtual` and it fails on purpose.

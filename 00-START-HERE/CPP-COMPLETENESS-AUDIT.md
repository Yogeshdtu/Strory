# C++ Completeness Audit

Yeh file iss course ka **honest coverage tracker** hai.

**Legend**
- `[✓]` — Fully covered: theory + examples + exercises + common mistakes likhe ja chuke
- `[~]` — Partially covered: mention hai ya folder README mein plan hai, par full lesson nahi
- `[ ]` — Missing: abhi tak nahi likha

**Important rule:** Folder ka exist karna ≠ topic ka covered hona.
Ek topic tabhi `[✓]` hoga jab uske paas: explanation + kam se kam 2 examples +
common mistakes + exercises + interview questions ho.

Last updated: **Batch 2 (Phase 0/1/2 complete)**

---

## 1. LANGUAGE FUNDAMENTALS

| Status | Topic | Kahan |
|---|---|---|
| [✓] | Program structure, `main()`, statements | 02 |
| [✓] | Comments | 02 |
| [✓] | `#include` aur headers (basic) | 02 |
| [✓] | Compilation pipeline (preprocessor→linker→CPU) | 02 |
| [✓] | Variables aur data ka concept | 03 |
| [✓] | Declaration vs definition vs initialization | 03 |
| [✓] | Fundamental types (`int`, `char`, `bool`, floating point) | 03 |
| [✓] | Signed/unsigned, integer sizes aur ranges | 03 |
| [✓] | Integer overflow aur UB | 03 |
| [✓] | Floating-point representation aur precision traps | 03 |
| [✓] | Fixed-width types (`<cstdint>`) | 03 |
| [✓] | Initialization forms (copy/direct/brace) aur narrowing | 03 |
| [✓] | `auto` aur type deduction (basic) | 03 |
| [✓] | `const` aur `constexpr` (intro) | 03 |
| [✓] | Type conversions aur integer promotions | 03 |
| [✓] | `sizeof`, `<limits>` | 03 |
| [✓] | Scope aur lifetime (intro) | 03 |
| [ ] | Namespaces | 24 |
| [ ] | Storage class specifiers (`static`, `extern`, `thread_local`, `mutable`) | 14, 24 |
| [ ] | Linkage (internal/external/module) | 24 |
| [✓] | Expressions aur full operator set | 05 |
| [✓] | Evaluation order aur sequencing rules | 05 |
| [✓] | Bitwise operations | 05 |
| [ ] | Control flow (`if`/`switch`/loops) | 06, 07 |
| [ ] | `if constexpr`, `if` with initializer | 06, 21 |
| [ ] | Functions, overloading, overload resolution | 08 |
| [ ] | Default arguments | 08 |
| [ ] | Recursion aur call stack | 08 |
| [ ] | `inline`, `constexpr` functions | 08 |
| [ ] | Lambdas (all capture forms, generic, mutable) | 22 |
| [ ] | Arrays aur array decay | 09 |
| [ ] | Multidimensional arrays | 09 |
| [ ] | `std::array`, `std::span` | 09, 19 |
| [ ] | C-strings aur `std::string` | 10 |
| [ ] | `std::string_view` | 10 |
| [ ] | Structs aur aggregate initialization | 11 |
| [ ] | Designated initializers (C++20) | 11, 22 |
| [ ] | Unions aur `std::variant` | 11, 19 |
| [ ] | Enums aur `enum class` | 11 |
| [ ] | Bitfields | 11 |
| [ ] | Alignment aur padding | 11, 25 |
| [ ] | Pointers (full) | 12 |
| [ ] | Pointer arithmetic | 12 |
| [ ] | `const` pointer vs pointer-to-const | 12 |
| [ ] | Function pointers | 12 |
| [ ] | References (lvalue/rvalue) | 13, 18 |
| [ ] | Value categories (lvalue/prvalue/xvalue/glvalue) | 18 |
| [ ] | Classes, constructors, destructors | 15 |
| [ ] | Member initializer lists | 15 |
| [ ] | `this`, `const` member functions, `static` members | 15 |
| [ ] | Operator overloading (full) | 15 |
| [ ] | Friend functions/classes | 15 |
| [ ] | Inheritance (all access levels) | 16 |
| [ ] | Virtual functions, vtable, vptr | 16 |
| [ ] | Abstract classes, pure virtual | 16 |
| [ ] | Virtual destructors | 16 |
| [ ] | Multiple + virtual inheritance, diamond | 16 |
| [ ] | Object slicing | 16 |
| [ ] | CRTP / static polymorphism | 16, 21 |
| [ ] | RAII | 17 |
| [ ] | Rule of 0/3/5 | 18 |
| [ ] | Copy semantics | 18 |
| [ ] | Move semantics, `std::move` | 18 |
| [ ] | Perfect forwarding, `std::forward` | 18 |
| [ ] | Copy elision, RVO/NRVO | 18 |
| [ ] | Templates (function + class) | 21 |
| [ ] | Template specialization (full + partial) | 21 |
| [ ] | Non-type template parameters | 21 |
| [ ] | Variadic templates aur fold expressions | 21 |
| [ ] | SFINAE aur `enable_if` | 21 |
| [ ] | Concepts aur `requires` (C++20) | 21 |
| [ ] | CTAD | 21 |
| [ ] | Two-phase lookup | 21 |
| [ ] | Exceptions, `throw`/`catch`, unwinding | 23 |
| [ ] | Exception safety guarantees | 23 |
| [ ] | `noexcept` | 23 |
| [ ] | RTTI aur `dynamic_cast` | 25 |
| [ ] | All four casts | 25 |
| [ ] | Object lifetime aur storage duration | 25 |
| [ ] | Trivial / standard-layout / POD | 25 |
| [ ] | Placement new, `std::launder` | 25 |
| [ ] | Strict aliasing, type punning, `std::bit_cast` | 25 |
| [ ] | Undefined behaviour (full catalog) | 25 |
| [ ] | `constexpr` / `consteval` / `constinit` | 22 |
| [ ] | Structured bindings | 22 |
| [ ] | Three-way comparison `<=>` | 22 |
| [ ] | Attributes (`[[nodiscard]]`, `[[likely]]`, etc.) | 22, 33 |
| [ ] | Modules (C++20) | 22 |
| [ ] | Coroutines (C++20) | 22 |
| [ ] | Deducing `this` (C++23) | 22 |

---

## 2. STANDARD LIBRARY

| Status | Area | Kahan |
|---|---|---|
| [✓] | `<iostream>` basics | 02, 03 |
| [✓] | Full iostreams, manipulators, stream state | 04 |
| [✓] | `std::format` / `std::print` | 04 |
| [ ] | `<string>` deep + SSO | 10 |
| [ ] | `<string_view>` | 10 |
| [~] | `<charconv>` (`to_chars`/`from_chars`) | 04 (used), full in 10 |
| [ ] | `<array>`, `<vector>`, `<deque>`, `<list>`, `<forward_list>` | 19 |
| [ ] | `<map>`, `<set>`, `<unordered_map>`, `<unordered_set>` | 19 |
| [ ] | Container adapters (`stack`, `queue`, `priority_queue`) | 19 |
| [ ] | `<span>` | 09, 19 |
| [ ] | Iterators aur `<iterator>` | 19 |
| [ ] | `<algorithm>` (full audit) | 19 |
| [ ] | `<numeric>` | 19 |
| [ ] | `<ranges>` | 19, 22 |
| [ ] | `<memory>` aur smart pointers | 17 |
| [ ] | Allocators | 19 |
| [ ] | `<memory_resource>` (PMR) | 19, 36 |
| [ ] | `<utility>`, `<tuple>`, `pair` | 19 |
| [ ] | `<optional>` | 19 |
| [ ] | `<variant>` | 19 |
| [ ] | `<any>` | 19 |
| [ ] | `<expected>` (C++23) | 19, 23 |
| [ ] | `<chrono>` | 19, 35 |
| [ ] | `<filesystem>` | 19 |
| [ ] | `<random>` | 19 |
| [ ] | `<regex>` | 19 |
| [ ] | `<functional>` (`std::function`, `bind`, invocables) | 19 |
| [ ] | `<type_traits>` | 21 |
| [ ] | `<concepts>` | 21 |
| [✓] | `<bit>` (C++20 bit utilities) | 05 |
| [✓] | `<limits>`, `<cstdint>` | 03 |
| [ ] | `<thread>`, `<jthread>` | 26 |
| [ ] | `<mutex>`, `<shared_mutex>`, `<condition_variable>` | 26 |
| [ ] | `<future>`, `<async>`, `promise`, `packaged_task` | 26 |
| [ ] | `<latch>`, `<barrier>`, `<semaphore>` | 26 |
| [ ] | `<atomic>` | 27 |
| [ ] | `<coroutine>` | 22 |
| [ ] | `<exception>`, `<stdexcept>`, `<system_error>` | 23 |
| [ ] | `<cmath>`, `<numbers>` | 19 |
| [ ] | C library headers (`<cstring>`, `<cstdlib>`, `<cstdio>`) | 10, 19 |

---

## 3. MEMORY + OBJECT MODEL

| Status | Topic | Kahan |
|---|---|---|
| [~] | Memory ka basic concept (RAM, addresses) | 01 |
| [ ] | Process memory layout (text/data/bss/heap/stack) | 14 |
| [ ] | Stack deep dive aur stack frames | 08, 14 |
| [ ] | Heap aur dynamic allocation | 14 |
| [ ] | `new`/`delete`, `new[]`/`delete[]` | 14 |
| [ ] | Memory leaks, double-free, use-after-free | 14 |
| [ ] | Static aur thread-local storage | 14, 25 |
| [ ] | Alignment aur padding | 11, 25 |
| [ ] | Object representation | 25 |
| [ ] | Temporaries aur lifetime extension | 25 |
| [ ] | Construction/destruction order | 15, 25 |
| [ ] | Static initialization order fiasco | 25 |
| [ ] | Placement new | 25 |
| [ ] | Aliasing aur strict aliasing | 25 |
| [ ] | Undefined behaviour catalog | 25 |
| [ ] | ABI | 25 |

---

## 4. CONCURRENCY

| Status | Topic | Kahan |
|---|---|---|
| [ ] | Process vs thread | 26, 29 |
| [ ] | `std::thread`, join/detach | 26 |
| [ ] | Race conditions aur critical sections | 26 |
| [ ] | Mutexes aur lock guards | 26 |
| [ ] | Deadlock aur avoidance | 26 |
| [ ] | Condition variables | 26 |
| [ ] | Futures/promises/async | 26 |
| [ ] | Thread pools | 26 |
| [ ] | C++20 sync primitives | 26 |
| [ ] | Data races (formal definition) | 27 |
| [ ] | `std::atomic` aur atomic ops | 27 |
| [ ] | CAS aur `compare_exchange` | 27 |
| [ ] | Memory ordering (all 6) | 27 |
| [ ] | happens-before / synchronizes-with | 27 |
| [ ] | Fences aur barriers | 27 |
| [ ] | Hardware memory models (x86 TSO vs ARM) | 27, 31 |
| [ ] | Lock-free data structures | 28 |
| [ ] | ABA problem | 27, 28 |
| [ ] | Memory reclamation (hazard pointers, RCU, epochs) | 28 |
| [ ] | Seqlock | 28 |
| [ ] | False sharing | 28, 32 |

---

## 5. SYSTEMS PROGRAMMING

| Status | Topic | Kahan |
|---|---|---|
| [ ] | Linux architecture, kernel vs userspace | 29 |
| [ ] | Syscalls aur unki cost | 29 |
| [ ] | Processes, `fork`/`exec`/`wait` | 29 |
| [ ] | Signals | 29 |
| [ ] | File descriptors aur file I/O | 29 |
| [ ] | `mmap` aur shared memory | 29 |
| [ ] | Pipes, FIFOs, Unix sockets | 29 |
| [ ] | Scheduling, real-time priorities | 29 |
| [ ] | CPU affinity aur isolation | 29 |
| [ ] | Huge pages, `mlock`, page faults | 29 |
| [ ] | NUMA | 29, 31 |
| [ ] | Clocks aur timers | 29, 35 |
| [ ] | Sockets aur TCP/UDP | 30 |
| [ ] | Multicast | 30, 42 |
| [ ] | `epoll` | 30 |
| [ ] | Zero-copy I/O | 30 |
| [ ] | Kernel bypass | 30, 42 |

---

## 6. BUILD + TOOLING

| Status | Topic | Kahan |
|---|---|---|
| [✓] | Compiler basics aur flags | 00, 02 |
| [ ] | Translation units aur ODR | 24 |
| [ ] | Header/source separation | 08, 24 |
| [ ] | Linkage aur name mangling | 24 |
| [ ] | Static vs dynamic libraries | 24 |
| [ ] | Make | 24 |
| [ ] | CMake | 24 |
| [ ] | Binary tools (`nm`, `objdump`, `readelf`, `ldd`) | 24, 34 |
| [ ] | LTO, PGO | 33 |
| [ ] | GDB | 45 |
| [ ] | Sanitizers | 45 |
| [ ] | Valgrind | 45 |
| [ ] | `perf` | 35 |
| [ ] | Google Benchmark | 35 |
| [ ] | Compiler Explorer workflow | 33, 34 |

---

## 7. PERFORMANCE

| Status | Topic | Kahan |
|---|---|---|
| [ ] | CPU architecture (pipelines, OoO, branch prediction) | 31 |
| [ ] | SIMD | 31 |
| [ ] | Cache hierarchy aur cache lines | 32 |
| [ ] | Locality aur cache-friendly design | 32 |
| [ ] | AoS vs SoA / data-oriented design | 32 |
| [ ] | TLB aur huge pages | 32 |
| [ ] | Compiler optimizations | 33 |
| [ ] | Reading assembly | 34 |
| [ ] | Benchmarking methodology | 35 |
| [ ] | Percentiles aur tail latency | 35, 36 |
| [ ] | Profiling with perf/VTune | 35 |
| [ ] | Allocation avoidance | 36 |
| [ ] | Memory pools aur object pools | 36 |
| [ ] | Branch-free programming | 36 |
| [ ] | Virtual dispatch elimination | 36 |
| [ ] | Ring buffers | 36, 41 |
| [ ] | Batching aur syscall avoidance | 36 |

---

## Current score

| Section | ✓ | ~ | ☐ | % done |
|---|---|---|---|---|
| Language fundamentals | 20 | 0 | 57 | 26% |
| Standard library | 5 | 1 | 31 | ~14% |
| Memory + object model | 0 | 1 | 15 | ~3% |
| Concurrency | 0 | 0 | 21 | 0% |
| Systems | 0 | 0 | 17 | 0% |
| Build + tooling | 1 | 0 | 14 | 7% |
| Performance | 0 | 0 | 17 | 0% |

**Overall: roughly 15% complete.**

Phase 0, 1, aur 2 poore ho chuke hain — matlab foundation (variables, types, I/O,
operators) solid hai. Ab tak ka har topic **deeply** covered hai:
explanation + multiple examples + traps + exercises + interview questions.

Aage ka bada hissa: control flow (06-08), pointers/memory (09-14), OOP (15-18),
STL (19), aur poora systems + HFT track.

Jab bhi naya batch aaye, yeh file update hogi.

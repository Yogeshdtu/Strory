# C++ Completeness Audit

Yeh file iss course ka **honest coverage tracker** hai.

**Legend**
- `[✓]` — Fully covered: theory + examples + exercises + common mistakes likhe ja chuke
- `[~]` — Partially covered: mention hai ya folder README mein plan hai, par full lesson nahi
- `[ ]` — Missing: abhi tak nahi likha

**Important rule:** Folder ka exist karna ≠ topic ka covered hona.
Ek topic tabhi `[✓]` hoga jab uske paas: explanation + kam se kam 2 examples +
common mistakes + exercises + interview questions ho.

Last updated: **Gap-fix pass (post-PHASE 34, GCC 16.2)** — namespaces got a dedicated lesson (`08/14`); C++23 compile-verified + measured (`22/15`), closing the last two `[~]` rows (deducing `this`, `std::generator`); new C++23 library row; score table recounted from rows (230 ✓ / 0 ~ / 0 ☐). Prev: **Batch 11 part 6 COMPLETE (PHASE 34: the FINAL GAP AUDIT) — 🏁 course structurally complete.** Verified + fixed 8 stale `[~]`/`[ ]` markers in this file (content was already present in earlier folders): **Namespaces** `[ ]→[✓]` (24/05) · **`if constexpr`** `[~]→[✓]` (dedicated 21/07) · **branch prediction** `[~]→[✓]` (31/04, measured ~6–7×) · **scope & lifetime** `[~]→[✓]` (full storage-duration + `std::launder` in 25) · **`<cmath>`/`<numbers>`** `[ ]→[✓]` (used throughout; 22/04) · **C library headers** `[ ]→[✓]` (`<cstring>` 10/01 + 47, `<cstdlib>` 14/09, `<cstdio>` dedicated 04/11–12) · **Google Benchmark** `[ ]→[✓]` (35/08–09 + shim) · **Compiler Explorer** `[ ]→[✓]` (dedicated 33/02). Two rows stay `[~]` **by scope, not as gaps** — deducing `this` (C++23) and `<coroutine>`→`std::generator`: the machinery is taught lesson-level in folder 22, only the `-std=c++23` compile step is deferred (repo default c++20); both now cross-reference the `WHAT-I-STILL-NEED-TO-LEARN.md` **SPECIALIZED** list (which got an explicit new row). `std::expected` is the exception — compile-verified both ways (23/05). All 5 `WHAT-I-STILL` sections now read **NONE**. Prev: Batch 11 part 5 COMPLETE (PHASE 33: 49 PROJECTS) — every content folder (00–49) is now built. 5 lesson files (17 project specs, each spec → milestones v1→v2→v3 → concepts → tests → extensions; + `04` a v1-first approach / testing / **code-review checklist** doc; + `05` a pointer to folder 44) + **17 verified reference implementations** (`examples/{beginner,intermediate,advanced}/`, one per project, each a complete assertion-tested program). Beginner: calculator (shunting-yard) · number-guess (I/O split from logic, `<random>` right, binary-search optimal) · student manager · expense tracker (integer money) · word stats · INI parser (line-numbered errors). Intermediate: inventory (**append-only log + `replay()`**) · bank (**atomic transfer**, integer interest, 20k-op invariant property test) · CSV parser (**4-state field machine**) · log analyzer · KV store (**WAL + CRC + compaction via `rename` + torn-tail recovery**). Advanced: custom `Vector<T>` (placement `new`, Rule of 5 copy-and-swap, `move_if_noexcept` relocate, **strong exception guarantee**) · 3 allocators (arena/pool/segregated — **measured ~20–28× vs `::operator new` at `-O2`**) · thread pool (`packaged_task`/`future`, exception propagation) · concurrent queues (mutex bounded · **lock-free SPSC, no CAS** · **MPSC Vyukov**) · epoll echo server (Linux only, `*.linux.cpp`) · JSON parser (recursive descent, `std::variant` tree, **`line:col` errors**, **depth limit**, round-trip). All 16 non-linux examples compile strict-clean and run + assert (verified). Examples are in subdirs → `build.ps1 folder` N/A; `checkall` covers them recursively. **No new C++-language rows** — this folder *applies* folders 01–35 in connected programs. Mojibake: 0. **Folders 00–49 all built — only the final gap audit remains** (a cross-repo review to drive every `WHAT-I-STILL-NEED-TO-LEARN.md` section to "NONE" / explicit "SPECIALIZED"). Prev: Batch 11 part 4 COMPLETE (PHASE 33: 48 CHEATSHEETS). The **quick-reference layer** — 13 markdown sheets distilling folders `01`–`47`, each cross-linking its deep source folder: `01` syntax · `02` STL containers (memory / complexity / **iterator+ref invalidation** per container) · `03` `<algorithm>`/`<numeric>`/ranges by job · `04` complexity tables (+ the amortized-vs-worst-case p99.9 trap) · `05` compiler flags (warnings / `-O` / `-march` / sanitizers / codegen trade-offs / the 3 builds) · `06` GDB · `07` perf/valgrind/sanitizers (`perf stat` signal→fix table) · `08` HFT tuning checklist (BIOS → kernel cmdline → IRQs → memory → pinning → network → warm-up) · `09` memory ordering (6 orders, release/acquire handoff, ABA) · `10` latency numbers (consistent with 46/12) · `11` UB catalog · `12` HFT glossary · `13` 13 one-page interview-revision sheets + behavioural + make-a-market. **No `.cpp`, no new C++-language rows** — this folder *condenses* folders 01–47. Mojibake: 0. Prev: Batch 11 part 3 COMPLETE (PHASE 33: 47 CODING-PROBLEMS). A graded **practice bank** — 10 themed problem files (**250 problems**: basics 30 · arrays/strings 40 · pointers/memory 25 · OOP/design 20 · STL 35 · templates 20 · concurrency 25 · lock-free 15 · optimize-this 20 · HFT 20), each problem = statement + `Pattern:` hint + `<details>` (approach + complexity); a `11-solutions/` directory (README + 10 per-category worked-code writeups for the problems that need real code); and **10 verified runnable examples** (`examples/*.cpp`, one per theme — `./build.ps1 folder 47-CODING-PROBLEMS` → 10/10 OK strict, every binary runs + asserts). File `09` ("optimize this") targets are grounded in folders 32/43's *measured* numbers; `09_optimize_row_vs_col.cpp` measures **~45–70× at `-O2`** here (Rule 2: bigger than the textbook 7× — cache + TLB + vectorization stack, taught not rounded down). **No new C++-language rows** — this folder *drills* folders 01–46. Mojibake: 0. Prev: Batch 11 part 2 COMPLETE (PHASE 33: 46 INTERVIEW-PREP). A layered question bank (`02`–`14` = Layer 1 types → Layer 13 tick-to-trade architecture, every answer cross-refs its source course folder) + interview strategy (`01`), system design (`15`), brainteasers/probability (`16`), C++ trick questions (`17` — 30+ traps: unsequenced modification, signed-overflow UB, dangling `const&`/`string_view`, slicing, most-vexing-parse, `vector<bool>`, `reserve` vs `resize`, `printf` format mismatch, macro precedence, one-past-the-end — each with the right answer + why), behavioural (`18`), mock scripts + rubrics (`19`), resume (`20`). 20 lessons + 8 **verified** coding examples (`./build.ps1 folder 46-INTERVIEW-PREP` → 8/8 OK, strict) + 4 design docs + 4 mock transcripts. **No new C++-language rows** — this folder *tests* folders 01–45; it completes `HFT-COMPLETENESS-AUDIT.md` Section L (interview readiness). Mojibake 6 → 0. Prev: Batch 11 part 1 COMPLETE (PHASE 33: 45 DEBUGGING — a skill folder). 13 lessons + 5 standalone examples + 10 "find & fix" buggy programs (`./build.ps1 folder 45-DEBUGGING` → 5/5 OK under strict warnings; buggy programs compile-clean in `checkall`). The full debugging toolchain: mindset (hypothesis→test, reproduce-first, `git bisect`) · GDB basics + advanced (**real transcripts** captured on this box — MinGW GCC 15.1.0, GDB 16.3) · crashes + core dumps + signals · `-O2` debugging (`<optimized out>`, constant-folded functions, inlined frames, `-Og`) · sanitizers (ASan shadow-memory internals / UBSan / TSan happens-before / MSan; combine matrix; CI golden build) · valgrind (memcheck / helgrind / DRD / massif) · multithreaded (`thread apply all bt`, deadlock signature) · reverse debugging (`rr`, `watch` + `reverse-continue`) · logging strategy (levels, structured, **HFT async logging** — POD enqueue ~20–40 ns to an SPSC ring) · `perf` for bugs (off-CPU, syscall storms, false sharing) · a 40+-entry **symptom → tool → fix** bug catalog. **No new C++-language rows** — the underlying bug families were already tracked under folders 09/12/13/14/17/18/22/25/26/27/28; Section 6 (Tooling) rows GDB / Sanitizers / Valgrind / perf / Core dumps / Reverse debugging / Logging / Debugging-mindset / Bug-catalog all move to `[✓]`. **Rule-2 carried through:** `10_data_race.cpp` races ~15–20% of `-O0` runs but **hides at `-O2`** (RMW register promotion) — taught, not hidden. Mojibake sweep: 34 → 0. **Folders 00–45 now done — only wrap-up folders 46–49 + the final gap audit remain.** Prev: Batch 10 part 9 COMPLETE (PHASE 32: 44 HFT-PROJECTS — the CAPSTONE). Folder 44 assembles everything from folders 01–43 into one deterministic `MiniHftEngine` (MarketData → Parser → L2Book → Strategy → Risk → OMS → Venue → fills → PnL) — 15 lessons + 12 example drivers + 11 shared `mh_*.hpp` headers, `./build.ps1 folder 44-HFT-PROJECTS` → 12/12 OK. **Reuse over rewrite**: `#include`s folder-40's `MatchingEngine` (as the venue) and folder-41's `SpscQueue` (the hand-off); book/pools/strategy follow folder 39/14/36/43 patterns. The folder-43 optimization loop is applied end-to-end: `template <class Venue>` — `NaiveEngine` (std::map `MatchingEngine` venue) vs `OptimizedEngine` (`FastVenue` flat-array + per-level FIFO sweep), proven byte-identical across 5 configs (correctness gate) **before** the ~1.5–1.6× end-to-end speedup (book stage ~150 → ~67 ns/msg). **No new C++-language rows** — every technique already tracked under folders 14/19/21/25/27/28/36/39/40/41/43; this is the applied capstone integration proof, completing `HFT-COMPLETENESS-AUDIT.md` Section K. **Folders 00–44 are now all done — the HFT build track (36–44) is COMPLETE. Section 7 (Performance) stays ~100% from folder 36. Only wrap-up folders 45–49 + the final gap audit remain.** Prev: Batch 10 part 8 COMPLETE (PHASE 31: 43 HFT-OPTIMIZATION). Folder 42 covers the full wire-to-wire networking path (17 lessons + 8 examples) — an `INetworkReceiver` bypass abstraction, the full kernel-bypass ladder (kernel-socket → SO_BUSY_POLL → Onload → ef_vi → DPDK), a TX+RX same-clock-domain hardware-timestamping fix, NIC/IRQ tuning, TCP order-gateway tuning, and a wire-to-wire capstone — but every C++/POSIX technique involved (socket API usage, syscalls) was **already** checked off in folders 29/30 (Systems programming, "done at the lesson level"). So it adds **no new rows to this file**; what's new is a **measured (where possible), applied, networking-architecture proof** — including two genuine corrections found and fixed: 30/09's clock-domain limitation (now fixed with matched TX+RX kernel timestamps) and a mid-development factual error about writev's relationship to the Nagle/delayed-ACK stall (corrected before shipping). **Majority of this folder's code is Linux-only** (`.linux.cpp`, no WSL on this dev box) — hand-reviewed carefully instead of compile-verified, with real bugs (sign-compare, unbounded blocking, empty-vector UB) caught in review. Full coverage tracked in `HFT-COMPLETENESS-AUDIT.md` Section J (now complete, 9/9). **Folders 00–42 are now all done — Section 7 (Performance) stays ~100% from folder 36.** Prev: Batch 10 part 6 (PHASE 29: 41 HFT-CONCURRENCY) — Section I complete, also no new rows here.

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
| [✓] | Namespaces — **dedicated lesson `08/14`** (own namespaces, reopen, C++17 nested `a::b::c`, alias, `::global`, using-declaration vs using-directive + the header rule, anonymous namespace with `nm`-verified internal linkage, inline namespace + ABI versioning, **ADL** via `getline`/`operator<<`, `std` is off-limits; example `08/08_namespaces.cpp`); linkage depth: internal/external/**module**, `static` vs anonymous namespace, `const`/`constexpr` default-internal (24/05) | 08, 24 |
| [✓] | Storage class specifiers — `static` (3 meanings), `extern` decl/def, `inline` variables (C++17), `thread_local` (+ TLS access cost), `constinit`, static-init-order fiasco + construct-on-first-use | 14, 24 |
| [✓] | Linkage (internal / external / module) — anon namespaces vs `static`, `const` default internal, `-fvisibility=hidden` + explicit exports, module linkage | 24 |
| [✓] | Expressions aur full operator set | 05 |
| [✓] | Evaluation order aur sequencing rules | 05 |
| [✓] | Bitwise operations | 05 |
| [✓] | Control flow — `if`/`else`/`switch` (06), `while`/`do-while`/`for`/range-`for` (07) | 06, 07 |
| [✓] | `if` with initializer (06); `if constexpr` — **dedicated lesson** `21-TEMPLATES/07-if-constexpr.md` (both branches parsed, only one instantiated) | 06, 21 |
| [✓] | `switch` — fallthrough, `[[fallthrough]]`, jump tables, `enum` completeness | 06 |
| [✓] | Guard clauses / early return / nesting-flatten | 06 |
| [✓] | Loops — all forms, range-`for` (`auto`/`auto&`/`const auto&`), `break`/`continue`, `goto` avoid | 07 |
| [✓] | Loop bugs — off-by-one, infinite, unsigned underflow, iterator invalidation (preview) | 07 |
| [✓] | Branch prediction — intro + measured (06); full treatment (pipeline, BTB, TAGE, sorted-vs-unsorted **measured ~6–7×**, branchless-isn't-always-faster, `[[likely]]`) in `31-CPU-ARCHITECTURE/04` | 06, 31 |
| [✓] | Functions — declaration/definition, ODR, header/source, parameters (value/ref/ptr), return + RVO | 08 |
| [✓] | Function overloading + overload resolution (phases, conversion ranks, ambiguity, name mangling) | 08 |
| [✓] | Default arguments (trailing rule, per-call eval, vs overloading) | 08 |
| [✓] | Recursion (base case, exponential fib measured, tail-call/TCO, → iteration) | 08 |
| [✓] | Call stack — frames, prologue/epilogue, calling convention, stack growth, overflow (real asm) | 08 |
| [✓] | `inline` (ODR meaning), compiler inlining, `constexpr` functions, `consteval`, `if constexpr` | 08 |
| [✓] | Scope aur lifetime — static locals / dangling refs & views (08); **full storage duration + object lifetime + `std::launder` + implicit-lifetime types + the 4 lifetime-extension/dangling cases** in `25-OBJECT-MODEL` | 03, 08, 25 |
| [✓] | Lambdas — desugaring (class + captured-state struct), all capture forms (`[=]`/`[&]`/`[this]`/`[*this]`/init-capture/move-capture/pack), `mutable`, generic + `[]<class T>`, trailing return, `constexpr`/`consteval` lambda, function-pointer decay, cost (template param inlines vs `std::function`) | 21, 22 |
| [✓] | Arrays aur array decay | 09 |
| [✓] | Multidimensional arrays — row-major layout, flat 1D, `int(*)[N]` | 09 |
| [✓] | `std::array`, `std::span` — no-decay, zero-overhead, modern array param | 09 |
| [✓] | C-strings aur `std::string` — null terminator, `<cstring>` dangers, full API | 10 |
| [✓] | `std::string_view` — zero-copy views, dangling rules, no-alloc `substr` | 10 |
| [✓] | Structs aur aggregate initialization — DMI, value semantics, `= default` compare | 11 |
| [✓] | Designated initializers (C++20) | 11 |
| [✓] | Unions aur `std::variant` — active member, `bit_cast`, `visit`, closed vs open | 11 |
| [✓] | Enums aur `enum class` — scoped, underlying type, flags, `-Wswitch` | 11 |
| [✓] | Bitfields — syntax, impl-defined layout, masks-instead-of for portability | 11 |
| [✓] | Alignment aur padding — deep dive, member reorder (measured), `alignas`, packed | 11, 25 |
| [✓] | Pointers (full) — `&`/`*`/`nullptr`, `->`, `int**`, `void*`, arrays↔pointers, dangling/UAF, bug catalog, memory diagrams | 12 |
| [✓] | Pointer arithmetic — scaled by `sizeof`, in-bounds rule, one-past-end, `p2-p1` | 12 |
| [✓] | `const` pointer vs pointer-to-const — all 3 combos, reading-rule | 12 |
| [✓] | Function pointers — syntax, callbacks, dispatch table, → virtual dispatch preview | 12 |
| [✓] | References (lvalue) — alias model, vs pointer, `const T&` + lifetime-extension rules, returning refs, ref members, `reference_wrapper`, bug catalog; rvalue `T&&` intro (full move in 18) | 13, 18 |
| [✓] | Value categories (lvalue/prvalue/xvalue/glvalue) — 2-question model (identity? movable?), `decltype(x)` vs `decltype((x))`, "named rvalue reference is an lvalue" | 13, 18 |
| [✓] | Classes, constructors, destructors — encapsulation/invariants, default/param/delegating/`=default`/`=delete`, dtors + Rule of 3/5 preview | 15 |
| [✓] | Member initializer lists — vs body assignment, const/ref members, **declaration-order trap** (`-Wreorder`) | 15 |
| [✓] | `this`, `const` member functions (+ `mutable`, const/non-const overload), `static` members (`inline static`, static methods) | 15 |
| [✓] | Operator overloading — `+=`/`+`/`<<`/unary/`++`, member vs free, `<=>` + `==` (C++20), hidden-friend idiom | 15 |
| [✓] | Friend functions/classes — when needed, hidden friend, not-transitive/inherited, C++20 reduced need | 15 |
| [✓] | Inheritance (public/protected/private), layout (base at offset 0), upcast, name hiding + `using` | 16 |
| [✓] | Virtual functions, **vtable/vptr deep dive** (2 loads + indirect call), `override`/`final`, devirtualization, ctor/dtor virtual-call trap | 16 |
| [✓] | Abstract classes, pure virtual (`=0`), interfaces, pure-virtual body / pure-virtual dtor | 16 |
| [✓] | Virtual destructors — measured leak demo, "public+virtual OR protected+non-virtual", `unique_ptr` vs `shared_ptr` | 16 |
| [✓] | Multiple inheritance (interfaces safe, pointer adjustment/thunks), virtual inheritance + diamond (measured layout cost) | 16 |
| [✓] | Object slicing — what's cut, 4 places it happens, `=delete` copy / `clone()` prevention | 16 |
| [✓] | CRTP / static polymorphism — zero-cost dispatch (measured ~1x direct), mixins, `friend D` guard, EBO | 16, 21 |
| [✓] | RAII — acquire-in-ctor/release-in-dtor, stack unwinding, the 5 dtor-skip gaps, move-only wrapper shape, RAII for fd/lock/FILE | 17 |
| [✓] | Rule of 0/3/5 — generation rules (declared dtor suppresses implicit move; `=delete` copy w/o `=default` move → immovable), Rule of Zero default | 17, 18 |
| [✓] | Copy semantics — deep vs shallow, self-assignment guard, allocate-before-free, copy-and-swap idiom | 18 |
| [✓] | Move semantics, `std::move` — steal+null, "valid but unspecified", `std::move` = `static_cast<T&&>` (const→copy, `return std::move` pessimization), `noexcept` move + `move_if_noexcept` (measured ~3x) | 18 |
| [✓] | Perfect forwarding, `std::forward` — forwarding refs, reference collapsing, canonical `make<T>(Args&&...)` factory | 18 |
| [✓] | Copy elision, RVO/NRVO — RVO guaranteed C++17 (unaffected by `-fno-elide-constructors`), NRVO best-effort, guaranteed prvalue elision needs no copy/move ctor | 18 |
| [✓] | Templates (function + class) — argument deduction (by-value/`const&`/`T&&` rules, no implicit conversion), explicit args, `auto`/`decltype(auto)` return, out-of-class members, member function templates, overload resolution (exact non-template beats template), `extern template` | 21 |
| [✓] | Template specialization (full + partial) — primary + `template<>` full spec, partial spec (`T*`, `pair<A,B>`, patterns), "most specialized wins", function templates specialize-by-overload (no partial spec), `std::hash` for user types | 21 |
| [✓] | Non-type template parameters — integral/enum/pointer-with-linkage, `auto` NTTP (C++17), float + structural class NTTP (C++20); NTTP → inline storage + unrolled loops; template-template params | 21 |
| [✓] | Variadic templates aur fold expressions — parameter packs, `sizeof...`, expansion patterns, recursion vs the 4 fold forms, perfect-forwarding a pack, vs C varargs | 21 |
| [✓] | SFINAE aur `enable_if` — "substitution failure not an error", immediate context, `enable_if` in 3 places, expression SFINAE (`decltype` + `declval`), detection idiom (`void_t`), tag dispatch, why concepts replaced it | 21 |
| [✓] | Concepts aur `requires` (C++20) — define (from traits / `requires`-expr with the 4 requirement kinds), 4 constraint syntaxes, **subsumption** (more-constrained overload wins), one-line errors, the standard concepts | 21 |
| [✓] | CTAD — deduce class template args from the constructor, deduction guides, no partial deduction | 21 |
| [✓] | Two-phase lookup — definition-time (non-dependent) vs instantiation-time (dependent); `typename` / `.template` disambiguators; dependent-base `this->`; ADL for dependent calls (the `using std::swap` idiom); MSVC non-conformance history | 21 |
| [✓] | Exceptions, `throw`/`catch`, unwinding — catch by `const&` + order, rethrow (`throw;` vs `throw e;`), `exception_ptr`; **stack unwinding** (ctor-mid throw → only constructed members' dtors; `noexcept` boundary → `terminate`); custom hierarchy from `std::runtime_error`, `throw_with_nested`; `-fno-exceptions` (why HFT, the toolkit) | 23 |
| [✓] | Exception safety guarantees — no-throw / strong / basic / none; commit-or-rollback + copy-and-swap idioms; leak-on-throw vs RAII rollback (measured via live-object counter); `noexcept` move ⇒ `std::vector` realloc moves not copies | 23 |
| [✓] | `noexcept` — contract not hint; violation → `terminate` (no search phase); `move_if_noexcept`; conditional `noexcept(expr)`; where to apply / not; `throw()` history; C++17 part of the function type | 23 |
| [✓] | RTTI, `typeid`, `dynamic_cast` (ptr null / ref throw), cost, `-fno-rtti`, hot-path alternatives (tag/variant) | 16, 25 |
| [✓] | All four casts — `static_cast` (numeric / known-hierarchy downcast = no check, UB if wrong), `dynamic_cast` (runtime-checked, **internals**: vtable → `type_info` → `__dynamic_cast` graph walk; cost; `dynamic_cast<void*>` = most-derived), `const_cast` (cv only; write to truly-const = UB), `reinterpret_cast` (ptr↔int, byte views; deref-as-other-type = UB); C-style cast silently picks `reinterpret_cast`; hot-path alternatives to `dynamic_cast` | 16, 25 |
| [✓] | Object lifetime — ctor-complete → dtor-start; trivial vs non-trivial; **storage duration vs lifetime** (raw buffer + placement new); storage reuse + `std::launder`; implicit-lifetime types (C++20 `malloc`+use); out-of-lifetime access = UB | 25 |
| [✓] | Trivial / trivially-copyable / standard-layout / POD / `has_unique_object_representations` — what each requires, which unlocks `memcpy` the bytes / `offsetof` / `memcmp` for equality / `malloc`+use; POD removed as a concept in C++20; measured `memcmp == -1` without `memset` first | 25 |
| [✓] | Placement new & manual lifetime — `alignas(T) unsigned char[sizeof(T)]`, `::new (p) T(...)`, explicit `p->~T()` (no `delete`), storage reuse, `std::launder` (const/ref/vtable members), pools / `FixedOptional<T>`; `std::vector`/`optional` internals | 25 |
| [✓] | Strict aliasing & type punning — the sanctioned glvalue types (actual type / cv+sign variants / enclosing / `char`/`byte`); every punning method + verdict (`bit_cast` ✅ / `memcpy` ✅ / byte-ptr ✅ / union-inactive-read ❌UB in C++ / `reinterpret_cast`+deref ❌UB); TBAA — measured `-O2` load-reuse divergence (`aliasing_trap` `delta == 0`); `-fno-strict-aliasing` is a crutch | 19, 25 |
| [✓] | Undefined behaviour (full catalog) — UB vs unspecified vs impl-defined; "compiler assumes UB never happens" (null-check removal, overflow-loop assumptions); catalog by category (memory / integer / lifetime / sequencing / concurrency / library); detection (UBSan/ASan/TSan, `_GLIBCXX_ASSERTIONS`, warnings); defensive coding | 23, 25 |
| [✓] | `constexpr` / `consteval` / `constinit` — three guarantees (can / must / is-initialized-at compile time); relaxed `constexpr` history; `constexpr` containers; `.rodata` tables → zero-init deterministic startup; kills init-order fiasco + init guard | 08, 21, 22 |
| [✓] | Structured bindings — unpack tuple/struct/array, by-value vs `auto&`, `map` iteration, `if`-with-initializer, `std::tie` | 22 |
| [✓] | Three-way comparison `<=>` — `= default` gives all six (`==` separate, member-wise), custom `<=>` needs hand-written `==`, comparison categories (strong/weak/partial), NaN → all-false, rewrite + reversed candidates | 15, 22 |
| [✓] | Attributes — standard (`[[nodiscard]]`/`[[maybe_unused]]`/`[[fallthrough]]`/`[[likely]]`/`[[noreturn]]`/`[[no_unique_address]]`/`[[assume]]`) diagnostic-vs-codegen split; vendor `[[gnu::hot/cold/const/pure/flatten]]`; branch layout / i-cache; `[[assume]]` = unchecked axiom | 08, 22, 33 |
| [✓] | Modules (C++20) — `export module` / `import` vs textual `#include` (per-TU re-parse, macro leakage, include-order, ODR); interface/impl units, partitions, header units, `import std;`; BMI (`.gcm`); build-order + tooling state; zero runtime/ABI effect. Working multi-file demo (`22/examples/05_modules_demo/`) | 22 |
| [✓] | Coroutines (C++20) — `co_await`/`co_yield`/`co_return`, the `promise_type` hooks, `coroutine_handle`, the **frame** (heap unless HALO), generators (pull) vs async tasks (await), per-element suspend/resume cost, C++20 has no `std::generator` (C++23), frame pooling. Hand-rolled `Generator<T>` example | 22 |
| [✓] | Deducing `this` (C++23) — explicit object parameter, one member template replaces `&`/`const&`/`&&` overloads, `std::forward<Self>` requirement, recursive lambdas, not-virtual; survey `22/05`, **compile-verified deep dive `22/15`** (`examples/09_cpp23_in_practice.cpp23.cpp`, GCC 16.2) | 22 |

---

## 2. STANDARD LIBRARY

| Status | Area | Kahan |
|---|---|---|
| [✓] | `<iostream>` basics | 02, 03 |
| [✓] | Full iostreams, manipulators, stream state | 04 |
| [✓] | `std::format` / `std::print` — spec mini-language, positional args, compile-time-checked format string, custom `std::formatter<T>`, `format_to_n` (zero-alloc), vs `printf`/iostreams (04 intro; deep dive 22 file 13) | 04, 22 |
| [✓] | `<string>` deep + SSO — measured 15-char threshold, capacity growth | 10 |
| [✓] | `<string_view>` | 10 |
| [✓] | `<charconv>` (`to_chars`/`from_chars`) — measured ~8x vs `stoi`, strictness | 04, 10 |
| [✓] | `<array>`, `<vector>`, `<deque>`, `<list>`, `<forward_list>` — layout, growth/`reserve`/invalidation table, deque chunk map, list/flist cache cost (measured ~27x), decision table | 19 |
| [✓] | `<map>`, `<set>`, `<unordered_map>`, `<unordered_set>` — RB-tree guarantees, buckets/load-factor/rehash, custom hash + `hash_combine`, open-addressing vs chaining, measured map 1049 / sorted-vec 357 / unordered 113 ns/lookup | 19 |
| [✓] | Container adapters (`stack`, `queue`, `priority_queue`) — restricted interface, default underlying, `pop()` void, max-heap default, heap-ordered vector, event-loop use | 19 |
| [✓] | `<span>` — non-owning view, static/dynamic extent, subspan | 09, 19 |
| [✓] | Iterators aur `<iterator>` — 6 categories + tag dispatch, `advance`/`next`/`distance` (O(n) traps), inserter/stream iterators, full invalidation table, writing your own | 19 |
| [✓] | `<algorithm>` (full audit) — non-modifying / modifying + erase-remove idiom / sorting family (introsort, `nth_element`, `partial_sort`, `partition`) / binary-search + set ops + heap ops + permutations; 30+ in `03_algorithms_tour` | 19 |
| [✓] | `<numeric>` — `accumulate` (init-type trap) vs `reduce` (assoc/commut, FP reorder), `transform_reduce`/`inner_product`, `partial_sum`/scans/`adjacent_difference`, `iota`, `gcd`/`midpoint` | 19 |
| [✓] | `<ranges>` — lazy non-owning views, `\|` composition (`filter`/`transform`/`take`/`iota`/…), lazy-eval proof, range algorithms + projections, dangling/single-pass caveats | 19, 22 |
| [✓] | `<memory>` aur smart pointers — `unique_ptr`/`shared_ptr`/`weak_ptr`, control block + atomic refcount, `make_shared` 1 vs 2 allocs, custom deleters + EBO, `enable_shared_from_this`, measured shared_ptr copy ~93x | 17 |
| [✓] | Allocators — Allocator concept (C++17 minimal surface), converting ctor + `operator==`, `std::allocator`, `allocator_traits`, arena/bump + pool, EBO; `09_custom_allocator` | 19 |
| [✓] | `<memory_resource>` (PMR) — `memory_resource` vs `polymorphic_allocator`, monotonic/pool/null resources, upstream chaining, `release()`, stack-buffer zero-heap (measured), non-propagation on move | 14, 19, 36 |
| [✓] | `<utility>`, `<tuple>`, `pair` — `get`/structured bindings, `apply`/`tuple_cat`, "named struct > pair in interfaces" | 19 |
| [✓] | `<optional>` — no heap (`T` + `bool`), `value()` throws vs `*` UB, `value_or`, monadic (C++23), no `optional<T&>` | 19 |
| [✓] | `<variant>` — inline storage + tag, `visit` jump-table dispatch, `get_if`/`holds_alternative`, `valueless_by_exception`, overload-set exhaustiveness | 11, 19 |
| [✓] | `<any>` — type-erased, may allocate (SBO), `any_cast` needs the concrete type, why `variant`/interface is usually better | 19 |
| [✓] | `<expected>` (C++23) — full model + monadic API (`and_then`/`transform`/`or_else`/`transform_error`), `std::unexpected`, `expected<void,E>`, `E` choice (cheap enum vs `error_code`), layout/zero-cost; example builds under **`-std=c++20`** (hand-rolled `Expected<T,E>`, same API) **and `-std=c++23`** (real `std::expected`) | 19, 23 |
| [✓] | `<system_error>` — `std::error_code` = `{int, category*}` (throw/alloc-free), `error_code` vs `error_condition` (exact vs portable bucket), custom `error_category` (`message`, `default_error_condition`, `make_error_code`, `is_error_code_enum`), `errno` → `error_code`, `std::system_error` | 23 |
| [✓] | `<chrono>` — `duration<Rep,Period>` unit safety, `steady_clock` vs `system_clock`, `time_point` arithmetic, `duration_cast` truncates, correct microbenchmark discipline (warm-up/min/sink/-O2), clock resolution | 19, 35 |
| [✓] | `<filesystem>` — `path` (string-only ops, `operator/`), disk queries, `directory_iterator` + cached `entry`, mutating ops, throwing vs `error_code` forms, TOCTOU | 19 |
| [✓] | `<random>` — engine vs distribution split, `mt19937`/`mt19937_64`, distributions, `seed_seq` seeding, `thread_local` engines, why `rand() % n` is bad (modulo bias) | 19 |
| [✓] | `<regex>` — 3 ops, grammars/flags, **why slow** (µs construction, backtracking/ReDoS, ~10x PCRE/RE2), `static const` hoisting, alternatives (`from_chars`, CTRE, RE2) | 19 |
| [✓] | `<functional>` (`std::function`, `bind`, invocables) — `std::invoke`/`invocable`, `std::function` cost (type-erased indirect no-inline ~5 ns + SBO/heap alloc — measured), template vs fn-ptr vs `function_ref`, `bind_front`, `std::ref` | 19 |
| [✓] | `<type_traits>` — full practical catalog (19 file 21) + **internals** (21 file 08): `integral_constant`/`true_type`, predicate vs transformation traits, writing your own via partial specialization, `void_t` + `declval` detection idiom, `conditional_t`, compiler intrinsics; `if constexpr` fast paths + `static_assert` contracts | 19, 21 |
| [✓] | `<concepts>` — the standard concepts (`same_as`/`convertible_to`/`integral`/`equality_comparable`/`invocable`/`regular`/…), defining your own, `requires`-expressions, subsumption, one-line errors; ranges/iterators are concept-defined | 21 |
| [✓] | `<bit>` (C++20 bit utilities) — `bit_cast`, `popcount`/`countl_zero`/`countr_zero`, `has_single_bit`/`bit_ceil`/`bit_width`, `rotl`/`rotr`, `endian`/`byteswap`; maps to single CPU instructions | 05, 19 |
| [✓] | `<limits>`, `<cstdint>` | 03 |
| [✓] | `<thread>` / `<jthread>` — construct=start, join-or-`std::terminate`, move-only, decay-copied args + `std::ref`, `native_handle`; `std::jthread` (RAII join + `stop_token`); `std::this_thread` (sleep/yield/id) | 26 |
| [✓] | `<mutex>` / `<shared_mutex>` / `<condition_variable>` — `mutex` family (recursive/timed/shared), uncontended CAS vs contended futex; `lock_guard`/`unique_lock`/`scoped_lock` (RAII, multi-lock deadlock-free), `std::lock`, `std::call_once`; `shared_mutex` (when it helps vs snapshot); CV predicate pattern, lost/spurious wakeup, `notify_one`/`all` | 26 |
| [✓] | `<future>` / `<async>` / `<latch>` / `<barrier>` / `<semaphore>` — `future`/`promise`/`shared_future`, `std::async` policies (+ blocking dtor), `packaged_task`, exception propagation via `get()`; `std::latch` (one-shot), `std::barrier` (reusable + completion fn), `std::counting_semaphore` / `binary_semaphore`; `<stop_token>` | 26 |
| [✓] | `<atomic>` — `std::atomic<T>` (trivially-copyable, non-copyable, brace-init, `is_lock_free`/`is_always_lock_free`, `atomic_flag`), `load`/`store`/`exchange`/`fetch_*` + x86 mapping, `compare_exchange_weak`/`strong` (spurious failure, `expected` overwrite, CAS loops), all 6 `memory_order` + when-which, `atomic_thread_fence` / `atomic_signal_fence`, `std::atomic_ref` (C++20), C++20 wait/notify; used lock-free in folder 28 | 27, 28 |
| [✓] | `<coroutine>` — machinery (`coroutine_handle`, `promise_type`, `suspend_always`/`suspend_never`, hand-rolled `Generator<T>`, frame/HALO — `22/09`) **+ C++23 `std::generator` compile-verified** (`22/15`: lazy infinite replay, body-starts-at-`begin()` verified in `examples/10`); executors/`std::execution` are C++26 → SPECIALIZED | 22 |
| [✓] | C++23 library additions — `std::flat_map`/`flat_set` (**measured vs `std::map`**: map faster at 55 keys, flat ~5× at 232k, random insert ~22× slower), `std::mdspan`, `std::move_only_function`, `std::print`/`println` (+ `-lstdc++exp` on MinGW), `ranges::to`, `views::enumerate/zip/pairwise/chunk`, `ranges::fold_left`, `std::expected` monadic chain, `std::byteswap`, `std::to_underlying`, `std::unreachable`, `string::contains`; `import std;` setup + measured compile cost; 26-feature probe on GCC 16.2 (`std::stacktrace` absent in this build → SPECIALIZED) — `22/15` | 22 |
| [✓] | `<exception>` / `<stdexcept>` / `<system_error>` — `std::exception` hierarchy, `exception_ptr` / `current_exception` / `rethrow_exception`, `throw_with_nested` / `rethrow_if_nested`, `terminate` / `set_terminate`, `<stdexcept>` class family, `std::error_code` / `error_condition` / custom `error_category`, `std::system_error` | 23 |
| [✓] | `<cmath>` — used + explained in context throughout (`03` float pitfalls, `20` DSA, `47`/`49` katas: `sqrt`/`pow`/`fabs`/`isqrt`); `<numbers>` (C++20 named constants `pi`/`e`/…) — `22/04` | 03, 19, 22 |
| [✓] | C library headers — `<cstring>` (`10/01` C strings: `strlen`/`strcpy`/`strcmp`; `memmove` implemented in `47`), `<cstdlib>` (`14/09` `malloc`/`free`; `atoi`), `<cstdio>` (**dedicated** `04/11` printf-family + `04/12` I/O perf; `fopen`/`fread` in `49`) | 04, 10, 14 |

---

## 3. MEMORY + OBJECT MODEL

| Status | Topic | Kahan |
|---|---|---|
| [✓] | Memory ka basic concept (RAM, addresses) — 01 intro + 14 file 01 revisit (segments, per-variable region) | 01, 14 |
| [✓] | Process memory layout (text/data/bss/heap/stack) — per-variable region, `.bss` demand-zero, Win vs Linux caveat | 14 |
| [✓] | Stack deep dive aur stack frames — frames, prologue/epilogue, ABI, growth, overflow, real asm (08); limits/`alloca`/thread stacks (14) | 08, 14 |
| [✓] | Heap aur dynamic allocation — allocator fast/slow path, `malloc` internals, `brk`/`mmap`, `new` vs `malloc`, allocation cost (measured tail) | 14 |
| [✓] | `new`/`delete`, `new[]`/`delete[]` — 2-step semantics, forms, `nothrow`, mixing = UB, RAII replacement | 14 |
| [✓] | Memory leaks, double-free, use-after-free — patterns, counted-new detection, prevention (RAII/ownership); ASan/Valgrind (tools file) | 14 |
| [✓] | Static aur thread-local storage — 4 durations, 2-phase init, init-order fiasco + fix, `thread_local` cost/use | 14, 25 |
| [✓] | Alignment aur padding — deep dive, member reorder (measured), `alignas`, packed; arena/pool alignment (14 file 10) | 11, 14, 25 |
| [✓] | Object representation vs value representation — `sizeof` = object rep (incl. indeterminate padding); why `struct{char;int;}` is 8 not 5; alignment + tail padding; field-reorder to cut padding (measured ~30–45% in folder 11); `memcmp`/hash need `has_unique_object_representations`; `-Wpadded`; bit-fields impl-defined | 25 |
| [✓] | Temporaries & lifetime extension — full-expression rule; `const T&`/`T&&`/`auto&&` local binding extends (transitive to subobjects); the 4 non-extension cases (through a return, ctor member-init, `string_view`/pointer into a temp, C++20 range-for over `f().member`); `-Wdangling-reference` | 13, 25 |
| [✓] | Construction/destruction order — bases→members→body, reverse dtor, exception-during-construction | 15, 16 |
| [✓] | Static initialization order fiasco — cross-TU unspecified, "construct on first use", destruction-order fiasco | 14 |
| [✓] | Placement new & manual lifetime — `alignas` raw storage, `::new (p) T(...)`, explicit `p->~T()` (no `delete`), storage reuse + `std::launder` (const/ref/vtable), pools / `FixedOptional<T>`; how `std::vector`/`std::optional` are built on it (25 file 09) | 14, 25 |
| [✓] | Aliasing & strict aliasing — the sanctioned glvalue types + the `char`/`byte` exception; `std::bit_cast` / `memcpy` vs `reinterpret_cast`+deref / union-inactive-read (UB); TBAA — the optimizer reuses loads across incompatible-typed stores (measured `aliasing_trap` `delta == 0` at `-O2`); `-fno-strict-aliasing` cost (25 files 10, 11, 16) | 19, 25 |
| [✓] | Undefined behaviour catalog — UB vs unspecified vs impl-defined; **how the optimizer exploits UB** (null-check removal, `x+1>x`→1, overflow-loop assumptions, load reuse — real `-O2` examples, 25 file 16); catalog by category (memory / integer / lifetime / sequencing / aliasing / library / concurrency); UBSan/ASan/TSan + `_GLIBCXX_ASSERTIONS`; defensive coding (23 file 13, 25 file 15) | 23, 25 |
| [✓] | ABI — Itanium C++ ABI; covers mangling + calling convention + struct/vtable layout + `type_info` + stdlib types + `_GLIBCXX_USE_CXX11_ABI` dual ABI; the **ABI-break catalog** (data member add/reorder, virtual add/reorder, default-arg change, `inline`-body change); detection (`abidiff`, `-Wodr`, `static_assert(sizeof/offsetof)`); **stable-API design** (`extern "C"` + opaque handles + versioning) (25 file 14, 24 file 07) | 24, 25 |

---

## 4. CONCURRENCY

| Status | Topic | Kahan |
|---|---|---|
| [✓] | Process vs thread — what's shared (address space, fds) vs per-thread (stack, registers, TLS); creation cost (~10× cheaper), context switch (+ TLB flush for processes); `fork()` COW + multi-thread hazard; isolation → process-per-component + shm ring buffers | 26, 29 |
| [✓] | `std::thread`, join/detach — see `<thread>` row (§2); `native_handle` for affinity/priority | 26 |
| [✓] | Race conditions aur critical sections — race condition vs data race (data race = UB), the `++counter` interleaving (measured ~70% lost), cached-flag / torn-read / compiler-assumption consequences; critical section = 1 thread, keep it tiny (Amdahl); no I/O/alloc under the lock | 26 |
| [✓] | Mutexes aur lock guards — see `<mutex>` row (§2) | 26 |
| [✓] | Deadlock aur avoidance — Coffman's 4 conditions; break any one: consistent lock order / `std::scoped_lock` / lock hierarchy / `try_lock`+backoff (livelock risk) / don't take two locks; self-deadlock, callback/`join`/blocking-I/O under a lock; detection (gdb bt, TSan lock-order, `timed_mutex`) | 26 |
| [✓] | Condition variables — the predicate pattern (`cv.wait(lk, pred)`), state-change-under-lock-then-notify, lost wakeup + spurious wakeup, `notify_one` vs `notify_all` (thundering herd), `wait_for`, `condition_variable_any`; producer-consumer with 2 CVs | 26 |
| [✓] | Futures/promises/async — see `<future>` row (§2) | 26 |
| [✓] | Thread pools — mutex+cv task queue, `submit` → `future` (via `shared_ptr<packaged_task>` / `move_only_function`), run job outside the lock, drain-vs-cancel shutdown, sizing (CPU vs I/O bound), work-stealing + bounded queue for production. **Built** (`26/examples/07`, measured ~5.7× on 8 cores) | 26 |
| [✓] | C++20 sync primitives — `jthread` + `stop_token` (cooperative cancellation, CV integration), `latch` (one-shot countdown, start-gun), `barrier` (reusable phase sync + completion fn), `counting_semaphore`/`binary_semaphore` (bound concurrency / signalling); often a single futex — faster than `mutex`+`cv` for their pattern | 26 |
| [✓] | Data races — the **formal definition** (same memory location, ≥1 write, neither happens-before the other, ≥1 non-atomic, different threads ⇒ **whole-program UB**); "same location" (struct members vs adjacent bit-fields); what breaks each condition; why the *whole* program (compiler/CPU assume race-free — cached flags, coalesced loops); TSan; `-O0` hides it | 27 |
| [✓] | `std::atomic` aur atomic ops — atomicity = indivisible (non-atomic RMW = load-modify-store; torn/misaligned reads; x86 aligned ≤8B hw-atomic but still a data race); `std::atomic<T>` (trivially-copyable, non-copyable, `is_always_lock_free`, `atomic_flag`); `load`/`store`/`exchange`/`fetch_add`/`fetch_or`/… (returns OLD; x86 `mov`/`xchg`/`lock xadd`/`lock cmpxchg`; no `fetch_max`); measured per-op ns | 27 |
| [✓] | CAS aur `compare_exchange` — `weak` (spurious failure on LL/SC → loop) vs `strong` (single try); `expected` is overwritten with the current value on failure; the CAS-loop pattern for arbitrary RMW; success vs failure memory order (failure ≤ success, no release); livelock/retry-storm; `fetch_add` beats a CAS loop where it fits | 27, 28 |
| [✓] | Memory ordering (all 6) — `relaxed` (atomicity only, no ordering) / `acquire` / `release` / `acq_rel` / `seq_cst` (+ dead `consume`); default is `seq_cst`; the message-passing pattern; measured store `relaxed`/`release` ~0.7 ns vs `seq_cst` ~13 ns (~18×) on x86, loads & RMW order-insensitive; downgrade only with a happens-before argument | 27 |
| [✓] | happens-before / synchronizes-with / sequenced-before — the three relations; the synchronizes-with edge list (release↔acquire on the same var, thread create/join, `mutex::unlock`↔`lock`, `promise`↔`future`, C++20 sync); transitivity; the value a non-atomic read sees; non-edges (wall-clock time, relaxed pairs, different-variable release/acquire) | 27 |
| [✓] | Fences aur barriers — `std::atomic_thread_fence` (real, x86: only `seq_cst` → `mfence`; acquire/release fences = compiler barrier only) vs `std::atomic_signal_fence` (compiler-only, no instruction — signal-safety, useless across cores); release fence + relaxed store; acquire fence after relaxed load; the `seq_cst` fence trick to keep flags relaxed | 27 |
| [✓] | Hardware memory models (x86 TSO vs ARM/POWER) — x86-TSO allows only store→load reordering (store buffer); ARMv8/POWER allow all four; multi-copy atomicity (x86 & ARMv8 yes, POWER/old-ARM no → IRIW); C++→ARM instruction mapping (`ldar`/`stlr`/`dmb`, per-op cost unlike x86); "passes on x86" ≠ correct; CPU-arch deep dive in 31 | 27, 31 |
| [✓] | Lock-free data structures — SPSC ring buffer (release-store/acquire-load of two indices, no CAS, wait-free in practice, no ABA); Vyukov bounded MPMC (per-cell `seq` turnstile, monotonic positions); Michael-Scott queue (dummy node, helping); Treiber stack (tagged head); progress guarantees (wait-free ⊂ lock-free ⊂ obstruction-free); **lock-free ≠ fast** (measured: contended Treiber ~5× *slower* than `mutex+vector`; SPSC ~4–6× *faster*) | 28 |
| [✓] | ABA problem — CAS checks value equality, not "nothing changed"; the Treiber-pop A→B→A interleaving; needs a recycled resource + a CAS on it; fixes: tagged/versioned word (`{idx:32,tag:32}` in one `uint64_t` — lock-free everywhere, no DWCAS), reclamation schemes, or monotonic-counter designs (no reuse → no ABA); tag width; non-pointer ABA (cycling enums, bounded seq); reproduced deterministically (`27/07`, `28/04`) | 27, 28 |
| [✓] | Memory reclamation — the use-after-free window (a thread holds a raw internal-node pointer loaded before the unlink); schemes: fixed pool / never-free (HFT default, + ABA tags), atomic per-node refcount, **hazard pointers** (publish-then-revalidate, bounded garbage `T·R`), **epoch-based / RCU / QSBR** (cheap read side, unbounded-memory failure mode under a stalled reader); why `atomic<shared_ptr>` is wrong on a hot path | 28 |
| [✓] | Seqlock — 1-writer/N-reader consistent snapshot; writer odd/even `seq` + release fences; reader two `seq` reads bracketing the copy + acquire fence; the payload is formally a data race (use relaxed atomics / `atomic_ref`); multiple writers → serialize separately; payload size = retry window; measured **~80–100× faster reads** than `std::shared_mutex`, torn=0; *the* HFT market-data primitive | 28 |
| [✓] | False sharing & cache-line padding — MESI line-bouncing on independent writes to one 64-byte line; **measured ~10×** (`26/08`: packed vs `alignas(64)` counters) and **~3.5–4×** (`28/07`: pure false sharing on an SPSC index pair, adjacent ~24 ns/op vs padded ~6 ns/op); `std::hardware_destructive_interference_size` (+ the ABI/`-Winterference-size` gotcha — hardcode 64/128); `alignas` field + trailing pad; **padding alone doesn't fix an SPSC ring** if the opposite index is reloaded every op → cache the opposite index (`28/02` V2); "accumulate locally + combine" beats perfect padding; `perf c2c` | 26, 28, 32 |

---

## 5. SYSTEMS PROGRAMMING

> **Nota bene:** folders 29–30 ke examples sab `*.linux.cpp` hain — is repo ka
> toolchain MinGW-on-Windows hai, to woh yahan compile nahi hote (`build.ps1`
> unhe `SKIP (linux-only)` karta hai). Lessons + Linux code complete hain;
> Linux/WSL pe verify karo. Benchmark numbers examples ke `EXPECTED` blocks mein
> "typical, not measured on your machine" labelled (Rule 2).

| Status | Topic | Kahan |
|---|---|---|
| [✓] | Linux architecture, kernel vs userspace — ring 0/3, hardware privilege enforcement, kernel subsystems (sched/mm/VFS/net/time/IPC), `strace`, program birth (fork→execve→ld.so→libc→main) | 29 |
| [✓] | Syscalls aur unki cost — `syscall` instr mechanics, ~300 ns fixed overhead (mode switch + KPTI/retpoline mitigations), **vDSO** (`clock_gettime` ~20 ns, no trap), count-reduction techniques (buffering/mmap/shm/`recvmmsg`/`io_uring`/busy-poll), `errno` thread-local, `seccomp` cost | 29 |
| [✓] | Processes, `fork`/`exec`/`wait` — COW page-table copy, `exec` replaces image, `waitpid` status macros, zombie vs orphan, `clone()` flags = process/thread/namespace, `posix_spawn`/`vfork`, `pidfd`, multi-threaded-`fork` hazard | 29 |
| [✓] | Signals — `sigaction` (not `signal`), **async-signal-safety** (only `write`/atomics/`_exit`), "flag + main loop" pattern, `signalfd`/self-pipe, per-thread mask, hot-thread signal masking, `SIGPIPE`/`EINTR` | 29 |
| [✓] | File descriptors aur file I/O — fd table 3 levels (per-proc / open-file / inode), short read/write loops, `O_CLOEXEC`, `dup2`/pipe redirection, `RLIMIT_NOFILE`, `/proc/pid/fd`, `pread`/`writev`, `close` return + EINTR | 29 |
| [✓] | `mmap` aur shared memory — file/anonymous × shared/private, lazy allocation + first-touch fault, `MAP_POPULATE`/`madvise`, `shm_open`+`mmap` cross-process, layout rules (no pointers → offsets, `PROCESS_SHARED` mutex or lock-free), TLB shootdown on `munmap` | 29 |
| [✓] | Pipes, FIFOs, Unix sockets — anonymous pipe + `fork`, named FIFO rendezvous, `AF_UNIX` (bidir, `SOCK_SEQPACKET`, `SCM_RIGHTS` fd passing, `SO_PEERCRED`), `PIPE_BUF` atomicity, latency: shm ~ns vs UDS/pipe ~µs vs loopback TCP ~10s µs, `eventfd` | 29 |
| [✓] | Scheduling, real-time priorities — CFS/EEVDF (`vruntime` + `nice`), `SCHED_FIFO`/`RR`/`DEADLINE`, RT hazards (starvation/inversion/`sched_rt_runtime_us` throttle), **"isolated core + CFS busy-poll often beats FIFO"**, `perf sched` | 29 |
| [✓] | CPU affinity aur isolation — `sched_setaffinity`/`cpu_set_t`, `isolcpus`/`nohz_full`/`rcu_nocbs`/`irqaffinity` boot params, `cpuset` cgroup, SMT sibling idling, topology-aware pinning, startup self-check | 29 |
| [✓] | Huge pages, `mlock`, page faults — minor/major/COW fault cost, `mlockall(MCL_CURRENT\|MCL_FUTURE)` + pre-fault + dry-run = zero-fault steady state, `getrusage` fault counts, TLB reach, THP vs hugetlbfs, `khugepaged` collapse stall | 29 |
| [✓] | NUMA — local vs remote latency, **first-touch** placement + the "init thread allocates everything" bug, `numactl`/`mbind`/`set_mempolicy`, `kernel.numa_balancing=0`, NIC's NUMA node, `numastat`/`numa_maps` | 29, 31 |
| [✓] | Clocks aur timers — `CLOCK_MONOTONIC` (vDSO) vs `_RAW` (trap) vs `_COARSE`, `rdtsc`/`rdtscp` + calibration + invariant-TSC flags, `clocksource=tsc` mandatory, `timerfd`, busy-spin to deadline for sub-µs "act at T" | 29, 35 |
| [✓] | Sockets aur TCP/UDP — call sequences, `recv` 0 vs <0 + errnos, **framing** (length-prefix), `getaddrinfo` at startup, TCP deep (handshake/window/RTO vs fast-retransmit/SACK/cwnd/TIME_WAIT), Nagle + delayed-ACK ~40 ms, UDP loss/reorder handling + A/B feeds + kernel drops + `recvmmsg` | 30 |
| [✓] | Multicast — `IP_ADD_MEMBERSHIP` (kernel filter + IGMP report), IGMP + snooping failure modes, explicit `imr_ifindex` on multi-NIC, SSM (`232/8`), TTL/LOOP, **A/B feed arbitration** by sequence number | 30, 42 |
| [✓] | `epoll` — O(ready) vs `select`/`poll` O(N) + `FD_SETSIZE`, level vs edge-triggered (drain-to-`EAGAIN`), event-loop skeleton, `EPOLLOUT` add/remove discipline, `EPOLLONESHOT`/`EXCLUSIVE`/`RDHUP`, `timerfd`/`signalfd`/`eventfd` in one loop, "hot path busy-polls / bypasses instead" | 30 |
| [✓] | Zero-copy I/O — `sendfile` (file→socket, +`TCP_CORK` header), `splice`/`vmsplice`/`tee` (capture/replay/proxy), `MSG_ZEROCOPY` (pin + ERRQUEUE completion, ~10 KB threshold), `io_uring` SQ/CQ rings + SQPOLL = zero syscalls, "hot path copies are noise" | 30 |
| [✓] | Kernel bypass — DPDK / OpenOnload / ef_vi / VMA / **AF_XDP**, what's removed (syscall+copy+stack+softirq+wakeup, ~1–5 µs → ~100–300 ns), what you take on (your own protocol/TCP, a spinning core, hugepages, NIC seizure, `tcpdump` blind), the adoption ladder | 30, 42 |

---

## 6. BUILD + TOOLING

| Status | Topic | Kahan |
|---|---|---|
| [✓] | Compiler basics aur flags | 00, 02 |
| [✓] | Translation units aur ODR — TU = source + includes post-preprocessing; 4-phase model; separate compilation; **ODR deep** (loud "multiple definition" vs silent IFNDR, `-Wodr` needs `-flto`, `_GLIBCXX` flag drift, COMDAT/vague linkage) | 24 |
| [✓] | Header/source separation — declarations vs definitions, ODR, `undefined reference`; include guards vs `#pragma once`; IWYU / self-contained headers; forward-decl + pImpl for build time | 08, 24 |
| [✓] | Linkage aur name mangling — internal/external/module linkage; Itanium mangling grammar, `c++filt`, `extern "C"` (no overload, C ABI shim); ABI + `_GLIBCXX_USE_CXX11_ABI` | 24 |
| [✓] | Static vs dynamic libraries — `.a` = archive + member selection; `.so`/`.dll` load-time linking, `-fPIC`, PLT/GOT indirection cost, soname/RUNPATH/`$ORIGIN`; **HFT: static (no PLT, LTO reach, determinism)** | 24 |
| [✓] | Make — target/prereq/recipe/timestamp model; `:=`/`=`/`?=`, automatic vars, functions, pattern rules; **`-MMD -MP` auto dependency tracking**; order-only prereqs; `.PHONY`; `-j` | 24 |
| [✓] | CMake — meta-build model; targets + `PUBLIC`/`PRIVATE`/`INTERFACE` usage requirements; `option()`, `target_compile_definitions`; `find_package` imported targets; generator expressions; out-of-source; `install`/`export` | 24 |
| [✓] | Binary tools — `file`/`size`/`nm -C`/`c++filt`/`objdump -d,-h,-p,-r`/`readelf`/`ldd`/`strings`/`strip`/`objcopy`/`addr2line`/`bloaty`; task-oriented recipes (undefined ref, missing lib, size hunt, crash → line) | 24, 34 |
| [✓] | LTO, PGO — LTO removes the TU optimization boundary (IR in `.o`, whole-program at link), ThinLTO, costs (link time, ODR louder, toolchain match, DCE of runtime-only symbols); PGO 3-step flow, what it improves (block layout / inlining / function ordering), representative training; **LTO+PGO+static+`-march=native` = the HFT release config** | 24, 33 |
| [✓] | GDB — **full folder 45 files 02–03** (+ real transcripts on this box, GDB 16.3): `break`/`run`/`next`/`step`/`finish`/`continue`, `print` (any expr) / `backtrace` / `frame` / `info locals`/`args` / `list` / STL pretty-printers; **advanced** — conditional breakpoints (`break f if …`), `ignore`, `tbreak`, `commands`+`silent`+`printf` (tracepoint-by-hand), `display`, `set var`/`return`, hardware vs software **watchpoints** (`watch`/`rwatch`/`awatch`, `-location`), `catch throw`/`catch syscall`/`catch signal`, `.gdbinit` + `define` + `-x`/`-batch` scripting, `attach`/`-p`; multithreaded (`info threads`, `thread apply all bt`, `set scheduler-locking`); `-O2` reality (`<optimized out>`, garbage prologue values, inlined frames, "2 locations") | 45 |
| [✓] | Sanitizers — ASan/UBSan/MSan/LSan tool→bug matrix, usage, MinGW limits + workarounds (14 file 11); UB-detection role in the UB catalog (23 file 13); **full practical folder 45 file 06** — ASan internals (shadow memory 1/8, redzones, quarantine), `ASAN_OPTIONS`/`UBSAN_OPTIONS`/`TSAN_OPTIONS`, LSan direct/indirect, UBSan sub-checks + `-fno-sanitize-recover`, TSan happens-before + slowdown, MSan (Clang-only) setup cost, **combine matrix** (ASan+UBSan ✅ / ASan+TSan ❌), CI golden build, sanitizer-vs-valgrind table | 14, 23, 45 |
| [✓] | Valgrind — memcheck usage, leak categories, `--track-origins`, vs ASan (14 file 11); **full folder 45 file 07** — memcheck (Invalid r/w, uninit propagation + "observable use", 5 leak kinds + action per kind), helgrind (races + lock-order), DRD, **massif** + `ms_print` (heap-growth diagnosis), DHAT, suppressions + `--gen-suppressions`, `--error-exitcode` for CI, valgrind-vs-sanitizers decision table | 14, 45 |
| [✓] | `perf` for debugging — profiling covered in folder 35 (§7); **folder 45 file 11** adds the *bug*-finding lens: `perf stat` anomaly reading (major-faults / cpu-migrations / context-switches → concrete misconfig), `perf trace -s` (syscall storms — accidental `clock_gettime`/`write` in a hot loop), `perf top -p` (live runaway), **off-CPU** analysis (`offcputime`, `perf sched timehist` — latency spikes that on-CPU sampling misses), `perf c2c` (false sharing) | 35, 45 |
| [✓] | Core dumps & post-mortem — **folder 45 file 04**: `ulimit -c` / `core_pattern` / `coredumpctl` / `gcore`, `gdb ./prog core` → `bt full` / `thread apply all bt` / `info registers`, "core must match the exact binary + flags", separate debuginfo (`objcopy --only-keep-debug`), Windows WER / `procdump` `.dmp` note; signals (SIGSEGV/SIGABRT/SIGFPE/SIGBUS/SIGILL), shell exit `128+sig`, fault-address reading, stack-overflow signature | 04, 29, 45 |
| [✓] | Reverse / time-travel debugging — **folder 45 file 09**: `rr record`/`rr replay` (~2× record, near-native deterministic replay), `reverse-continue`/`reverse-next`/`reverse-finish`, `watch` + `reverse-continue` = "who set this value" in one step, `rr --chaos` for races, `gdb` native `record full` vs `rr`, `checkpoint`/`restart`; Linux x86-64 + PMU only (`perf_event_paranoid`), why most VMs can't | 45 |
| [✓] | Logging strategy — **folder 45 file 10**: levels + runtime knob, structured (key=value/JSON) logging, what to log (decisions + inputs, correlation IDs), antipatterns (log-and-throw, `std::endl` per line, `localtime` in hot path), **HFT async logging** — hot thread enqueues a fixed POD (~20–40 ns) to an SPSC ring, background thread formats+writes, queue-full → drop + count (never block), binary log + mmap ring + pinned logger core; sync-vs-async trade-off table | 41, 43, 45 |
| [✓] | Debugging mindset & workflow — **folder 45 file 01**: hypothesis→test loop (mirrors 43's optimization loop), reproduce-first (deterministic/fast/minimal/automated), intermittent-bug → likely-cause table, **bisection** (code + `git bisect run`), localise (where-then-why), one change at a time, regression test, "it's your bug not the compiler" | 45 |
| [✓] | Bug pattern catalog — **folder 45 file 12**: 40+ patterns across Memory (OOB, UAF, use-after-return, double-free, leak, uninit, dangling view, invalidation, stack overflow, null) / Concurrency (race, deadlock ×2, livelock, lost wakeup, ABA, TOCTOU, `shared_ptr` misuse, false sharing) / Logic (off-by-one, `=`/`==`, `&`/`&&` + precedence, fallthrough, copy-paste, float eq) / Integer (signed overflow, unsigned wrap, narrowing, shift, `/0`, mixed-sign compare) / Lifetime (return-local, temp-lifetime, use-after-move, Rule-of-3/5, static-init fiasco, slicing, strict aliasing) — each **symptom → tool → fix**, plus a symptom→family quick index | 45 |
| [✓] | Google Benchmark — `35/08` microbenchmarking (fixtures, `DoNotOptimize`/`ClobberMemory`, `benchmark::State`, `Args`/`Range`, `Complexity`) + `35/09` pitfalls; a self-contained shim (`35/examples/`) so the lessons run without the lib; the repo's own `-O2` + `asm volatile` sink harness taught alongside | 35 |
| [✓] | Compiler Explorer (godbolt) — **dedicated lesson `33/02`**: the interface, diff view, multi-compiler/multi-flag panes, `#define` toggles, traps (default flags, `-march=native`, missing sink), and when it beats / loses to the repo's `./build.ps1 asm` | 33 |

---

## 7. PERFORMANCE

| Status | Topic | Kahan |
|---|---|---|
| [✓] | CPU architecture (pipelines, OoO, branch prediction) — **full folder 31** (15 lessons): fetch/decode/execute/retire, registers + renaming + spilling, ISA/µops/microcode, **pipelining** (latency vs throughput, hazards, flush/refill), **superscalar** (ports, ILP, IPC), **out-of-order** (ROB/RS/PRF, speculation, why pointer-chasing can't be hidden), **branch prediction** (gshare/TAGE, BTB/RAS, ~6–7× measured mispredict cost — ex 03), **speculative execution** (Spectre/Meltdown, KPTI cost, `mitigations=off` trade-off), instruction latency/throughput + critical-path, SIMD, SMT, frequency/turbo/C-states, NUMA hardware, Intel-vs-AMD. Examples 01–08 measured (ILP ~4×, misprediction ~6–7×, `rdtsc` self-cost) | 06, 31 |
| [✓] | SIMD / auto-vectorization — **folder 31 files 10–11 + examples 05–06** (scalar→SSE 4.1×→AVX2 ~9.5× float sum; same-code auto-vectorized ~2.5–3×; `target("avx2")` + CPUID dispatch; AVX-512 downclock; tail handling); **folder 32** (SoA is the prerequisite; roofline — SIMD wasted on a memory-bound loop); **folder 33 files 05, 12 + example 03** (the auto-vec conditions; `-fopt-info-vec[-missed]`; **a float reduction does NOT vectorize at `-O2` — reassociation — measured ~4× only with `-ffast-math`/`#pragma omp simd`, plain map ~3.5×**; `ivdep`/`assume_safety` as unchecked promises; `-march`=v2/v3/v4 for width; function multi-versioning); **folder 34 lesson 09** (reading the vectorized-loop asm signature — `ymm` + packed suffix + `add ptr,32` + horizontal reduce + tail; `vgather` is slow); ranges pipeline (22 `03`, ~1.5×) | 07, 22, 31, 32, 33, 34 |
| [✓] | Cache hierarchy aur cache lines — **full folder 32 files 01–03** (15-lesson folder): registers→L1→L2→L3→DRAM ladder + this box's geometry, 64-B line = unit of transfer (why 64, `hardware_destructive_interference_size`, line straddle), set-associative organization (index/tag/offset, ways, **critical stride** = size/assoc, power-of-two-dims poison, VIPT), the 3 C's + coherence + a diagnosis table. Measured: stride ramp (ex 01), row vs column ~10× (ex 03), latency cliff ~1.2→~95 ns (ex 08) | 07, 20, 32 |
| [✓] | Locality aur cache-friendly design — **folder 32 files 05, 08 + examples 02, 08**: spatial/temporal, loop interchange/fusion/fission/tiling, prefetcher-friendly vs hostile patterns; flat vs pointer-based structures (`map`/`unordered_map`/`btree_map`/`flat_hash_map` miss counts, arena+index links, CSR). Measured: sequential vs random line access ~7× (14 vs 2 GB/s), pointer-chase dependency chain ~95 ns/hop; plus 20's cache-aware-DSA synthesis (list vs vector ~40×, flat vs pointer tree 4–11×) | 07, 20, 32 |
| [✓] | AoS vs SoA / data-oriented design — **folder 32 files 09–10 + example 05**: the three access patterns → three winners (scan-few-fields **SoA ~2.0×**, scan-most-fields **SoA ~2.4×** — gap doesn't shrink, SoA vectorizes; random whole-record **AoS ~3×**), AoSoA hybrid; DOD philosophy (`vector<Base*>`+`virtual` hot-loop cost, existence-based processing, handles + generation counters, "group by what you do"). Plus 20's textbook→cache-aware transforms | 07, 20, 32 |
| [✓] | TLB aur huge pages — **folder 32 file 11 + example 08**: 4-level page walk (up to 4 dependent mem accesses), L1 dTLB / L2 STLB reach math (4 KiB → ~256 KiB / ~6 MiB), 2 MiB huge pages → 512× reach, explicit hugetlbfs (`MAP_HUGETLB`) vs THP downsides (`khugepaged` jitter → HFT uses explicit + pre-fault + `mlockall`), Windows large pages, NUMA first-touch. Measured: latency cliff at ~8 MiB working set (STLB + L3 coincide with 4 KiB pages — honest limitation stated) | 29, 32 |
| [✓] | Compiler optimizations — **full folder 33** (15 lessons): `-O` levels (`-O0→-Ofast`, the `-O0→-O1` cliff ~6× measured, per-file override, `-g` on release), godbolt/`-fopt-info-*`/`objdump`/`llvm-mca` workflow, **inlining** (`inline`=ODR not "inline me", heuristics, `always_inline`/`noinline`/`flatten`, inlining as the *enabling* optimization), **loop opts** (LICM/strength-reduction/unroll+"one accumulator" trap/fusion/fission/interchange-`-O3`-only/unswitch/rotation/IV-elimination), **auto-vec** (conditions + why float reduction needs `-ffast-math`), **const-fold/prop/DCE** (whole functions vanish; `constexpr`/`consteval`/`constinit`/`if constexpr`/`[[assume]]`), **devirtualization** (exact-type/`final`/speculative-LTO/PGO; the honest "don't rely on it"), **branch hints** (`[[likely]]`/`__builtin_expect` = *layout* not prediction, ~1.3× measured, PGO better), **aliasing/`__restrict`** (blocks LICM/CSE/vectorize; TBAA UB; `bit_cast`; local-copy; **~3.5× measured** ex 04), **LTO** (cross-TU inline/const-prop/devirt/DCE; ThinLTO; ODR-exposure; **~2.3× measured** ex 06 throughput loop, *no* gain on a carried loop), **PGO** (3-step + AutoFDO; representative-profile hazard), **`-march`/`-mtune`** (v1..v4, `native` wrong for shipping, AVX-512 downclock, multi-versioning), **`-ffast-math` dangers** (8 sub-flags, `-ffinite-math-only` deletes NaN guards, safe scoped alternatives), **preventing optimization** (`DoNotOptimize`/`ClobberMemory` = zero instructions), **reading optimized output** (the verification checklist). Rule 2 three times (float-reduction refusal / aliasing-only-with-noinline / LTO-only-on-throughput) | 07, 31, 32, 33 |
| [✓] | Reading assembly — **full folder 34** (13 lessons): why read (verification/optimization/debugging), x86-64 registers + sub-register zeroing + `xmm`/`ymm`/`zmm` + rflags, **AT&T vs Intel** (the 5 differences, which tool gives which), the ~20 common instructions (`lea` vs `mov [..]`, `cmp`/`test`, recognize `div`/`rep movs`/`lock`), addressing modes (recover `sizeof`/field offsets from a loop), **stack frames** (prologue/epilogue ± frame pointer, spills = register pressure, tail-call → `jmp`, Win64 shadow space), **calling conventions** (System V vs Windows x64 — `this`, large-struct return, varargs `al`), **pattern recognition** (`if`/`cmov`/loop/`while`/`switch` dense & sparse/call/virtual `call [reg+off]`/constant-fold/division), **SIMD asm** (scalar vs packed suffix, width from `add ptr,16/32`, horizontal-reduce cluster, `vzeroupper`, `vgather`), **inline asm** (4 sections, missing-clobber = `-O2` corruption, when to use vs intrinsics = almost never), **`rdtsc` timing** (fencing, ticks≠cycles≠ns, core hopping), **disassembly tools** (`-S` vs `objdump -dS` vs `perf annotate` vs `gdb` vs `addr2line` vs `llvm-mca`). 10 "which C++ made this asm?" puzzles. Plus the earlier asm touches (06/08/21/22) | 06, 08, 21, 22, 33, 34 |
| [✓] | Benchmarking methodology — **full folder 35** (lessons 01–04, 08–09): `-O2` requirement, "kill the optimizer" (`DoNotOptimize`/`ClobberMemory` = zero instructions — folder 33/14 + 35/09), **every pitfall demoed** (DCE / const-fold / hoist / cold-start / timer-overhead>op / one-run / **alignment & layout noise** / **frequency scaling → report cycles/op** / denormals / bench≠reality — `04_benchmark_mistakes.cpp` BUG/FIX pairs), warm-up + min-of-N + distribution, Amdahl (35/01), Google Benchmark (`State` loop, args, fixtures, `PauseTiming` — 35/08 + shim); **surprising-result honesty** carried through (22 `03`; folder 32 `-O3` interchange / prefetch 3× *slower*; folder 33 float-reduction / aliasing / LTO; **folder 35: BUG benchmarks really report 0.000 ns/op, cold-start only 1.2× on Windows, tail numbers run-to-run unstable on an unpinned box** — all taught); `rdtsc` fencing + calibration + self-cost subtraction + core-pin (34/11 + 35/03) | 06, 07, 19–22, 31–35 |
| [✓] | Percentiles aur tail latency — **full folder 35** (lessons 04–07, 16): mean-lies / median / min / σ-useless-for-latency / bimodal (35/04), **p50/p90/p99/p99.9/p99.99** nearest-rank + "nines" + **fan-out tail amplification** ("the tail at scale") + **coordinated omission** + HdrHistogram interval-correction + percentiles-don't-average (35/05), **jitter** (SW: timer/scheduler/IRQ/page-fault/malloc/syscall/lock; HW/firmware: freq/C-states/**SMI**/SMT/NUMA; the quiet-core recipe — 35/06), **histograms** (linear buckets fail, log-linear / HdrHistogram, relative-error ↔ `SUB_BITS`, CDF plot — 35/07 + `08_latency_recorder.cpp` ~2 ns/record, ≤1.3% error), production always-on measurement (35/16); measured allocation tail (14/08) | 14, 35, 36 |
| [✓] | Profiling with perf/VTune — **full folder 35** (lessons 10–14): **`perf`** stat (IPC, cache-miss rates, top-down Retiring/Frontend/Backend/Bad-Spec) / record / report / annotate + **skid → `:pp` (PEBS/IBS)** / `perf mem` (per-access latency + source) / **`perf c2c` (false sharing / HITM)** / LBR call graphs → AutoFDO / counter multiplexing (35/10–11); **flame graphs** (axes, top plateau, on- vs off-CPU, differential, `stackcollapse`→`flamegraph.pl` — 35/12); **Valgrind** cachegrind `Ir` (deterministic CI gate) / callgrind + KCachegrind / massif / **DHAT** (35/13); **VTune** recursive top-down (Backend→Memory→DRAM→Bandwidth-vs-Latency) + `toplev.py` + AMD uProf / Instruments (35/14). `06_perf_workflow.sh` + `07_flamegraph.sh` = self-contained Linux workflows | 32, 33, 35 |
| [✓] | Allocation avoidance — **full folder 36** (lessons 04–05, 08–09): the measured allocation tail (`01_allocation_cost.cpp`: mixed-size churn p50 80 / p99.9 **2585 ns** — the disaster), **every "hidden" allocation catalogued** (vector grow / string SSO / map node / `std::function` capture / `throw` / `std::to_string` / page fault), **pre-allocation discipline** (warm-up vs steady state; sizing from historical peak; **proving** zero-alloc with `null_memory_resource` upstream / a global-`new` hook / `perf stat -e page-faults`), arenas / PMR monotonic on a stack buffer (`05_pmr_containers.cpp`: `pmr::vector` p50 40 vs `std::vector` 270 ns, 0 global `new`), `from_chars`/`to_chars` for alloc-free number↔text; plus 14/19 foundations | 14, 19, 36 |
| [✓] | Memory pools aur object pools — **full folder 36** (lessons 06–07): `FixedPool` built + benchmarked (`02_memory_pool.cpp`: `new[64]` p99.9 180 ns vs **`FixedPool::allocate` p99.9 30 ns — flat**), intrusive free list (next-ptr in the free slot, zero overhead), `O(1)` `allocate`/`deallocate`, placement new + explicit dtor, cross-thread "goes home to be freed" pattern, double-free detection (bitmap / free-list walk); **object pool construct-on-acquire vs recycle-pre-constructed** (`03_object_pool.cpp`: `new Order()` p50 50 / construct 30 / **recycle+reset 20 ns** — recycle skips the ctor, its danger is stale fields), `std::launder` with placement new; plus 14/19 foundations | 14, 19, 36 |
| [✓] | Branch-free programming — branchless intro + benchmark (06); branchless binary search + `[[likely]]`/`[[unlikely]]` (20 file 05, 22 file 12); ranges masked-SIMD vs branchy hand loop (22 `03`); **folder 31 files 04/07/08 + examples 03/04**: measured — unpredictable branch ~6–7× slower (ex 03), branchless ~6–7× faster on random data / ~1.2× *slower* on predictable data (ex 04), the `-O2`-auto-`cmov` observation, `x & -(cond)` masking, SIMD predication; hot-path posture in 36 | 06, 20, 22, 31, 36 |
| [✓] | Virtual dispatch elimination — CRTP / `std::variant`+`visit` / tag-switch / templates, **measured** (16: virtual ~23 vs CRTP ~2.2 ns; 21: virtual (real boundary) ~2.43 vs CRTP ~0.56 ns, virtual ~2.51 vs template ~1.14 vs `variant` ~1.13 ns); devirtualization caveat; full in 36 | 16, 21, 36 |
| [✓] | Ring buffers — **full folder 36 lesson 15** (the clean final form): power-of-two `& mask` (no `%`/divider), **monotonic counters** (mask only on index → no ABA, no wasted slot), release-store/acquire-load of the two indices (no CAS), **cached opposite index** (the ladder's biggest win — no cross-core coherence miss per op), `alignas(CL)` on `head_`/`tail_`/`buf_` (no false sharing), producer-on-full = drop+count never block, + **4 situations where a ring is the wrong tool** (`09_ring_buffer.cpp`: try_push/pop p50 20 ns, p99.9 30 ns); plus 20/28 foundations (naive→cached ladder, Vyukov MPMC) | 20, 28, 36, 41 |
| [✓] | Batching aur syscall avoidance — **full folder 36 lessons 16–17**: batching amortizes a fixed per-op cost (syscall / lock / cache-cold touch / RPC) but adds **head-of-line latency** — the measured curve (`10_batching.cpp`: B=1 6.7 ns/item HoL 6.7 ns → B=1024 1.5 ns/item HoL **3095 ns**, knee ~B=16–32), **opportunistic batching** (take what's queued, never wait to fill); syscall cost (100–300× a userspace call, blocking → reschedule), **busy-poll** (and its 100%-CPU-per-core cost), `SO_BUSY_POLL`, **`io_uring` SQ/CQ + `SQPOLL`** (zero syscalls on submit) + registered buffers/files, **kernel bypass** (DPDK/Onload/ef_vi — what you take on), moving hot-path syscalls to a housekeeping thread; counting with `strace -c` / `perf trace` | 16, 17, 29, 30, 36 |

---

## Current score

Recounted directly from the rows above (not hand-maintained):

| Section | ✓ | ~ | ☐ |
|---|---|---|---|
| Language fundamentals | 98 | 0 | 0 |
| Standard library | 41 | 0 | 0 |
| Memory + object model | 16 | 0 | 0 |
| Concurrency | 21 | 0 | 0 |
| Systems | 17 | 0 | 0 |
| Build + tooling | 20 | 0 | 0 |
| Performance | 17 | 0 | 0 |
| **Total** | **230** | **0** | **0** |

**Every tracked row is `[✓]`.** The last two `[~]` rows (deducing `this`, `<coroutine>`→`std::generator`)
closed in the post-PHASE-34 gap-fix pass when `22/15` compile-verified C++23 on GCC 16.2. The previous
version of this table (81/4/0, "~89%") was stale — it had not been updated after folders 37–49 and
the PHASE 34 audit. Topics outside this file's scope (C++26, `std::stacktrace` on this build, FPGA,
DPDK apps, ...) are listed explicitly in `WHAT-I-STILL-NEED-TO-LEARN.md` → SPECIALIZED.

### Batch 11 part 5 (PHASE 33) — folder 49 (PROJECTS) — LAST content folder

**49 PROJECTS** — connected, difficulty-ordered projects (spec's "concepts
ko projects mein jodo"). 5 lesson files + **17 verified reference
implementations** in `examples/{beginner,intermediate,advanced}/`, each a
complete assertion-tested program with a TALKING POINTS footer.
Beginner (6): calculator (shunting-yard, `/0` handled) · number-guess
(logic split from I/O, `<random>` done right) · student manager · expense
tracker (integer money) · word stats · INI parser (line-numbered errors).
Intermediate (5): inventory (append-only log + `replay()`) · bank (atomic
transfer, integer interest, 20k-op invariant property test) · CSV parser
(4-state field machine) · log analyzer · KV store (WAL + CRC + compaction
+ torn-tail recovery). Advanced (6): custom `Vector<T>` (placement `new`,
Rule of 5, strong exception guarantee) · 3 allocators (**measured ~20–28×
vs `::operator new` at `-O2`**) · thread pool · concurrent queues
(mutex / lock-free SPSC / MPSC Vyukov) · epoll server (Linux only) · JSON
parser (recursive descent, `line:col` errors, depth limit, round-trip).
All 16 non-linux examples compile strict-clean + run + assert.
`04-project-guidelines.md` is a v1-first approach + testing +
**code-review checklist** doc. `05` maps each advanced project onto its
folder-44 HFT counterpart. **No new C++-language rows** — this folder
*applies* folders 01–35 in connected programs. **Mojibake:** 0. Examples
in subdirs → `checkall` covers them recursively (`build.ps1 folder` N/A).

### Batch 11 part 4 (PHASE 33) — folder 48 (CHEATSHEETS)

**48 CHEATSHEETS** — the **quick-reference layer**: 13 markdown sheets
distilling folders `01`–`47` into scannable tables + terse bullets +
code snippets, each cross-linking its deep source folder. `01` syntax ·
`02` STL containers (with the per-container **iterator + reference
invalidation** rules) · `03` `<algorithm>`/`<numeric>`/ranges grouped by
job · `04` complexity tables (+ the amortized-vs-worst-case p99.9 trap) ·
`05` compiler flags (this repo's strict warning set, `-O` levels,
`-march`, sanitizers, codegen knobs with their trade-offs, the 3 builds
you actually use) · `06` GDB (run/step, breakpoints + watchpoints +
`commands`, `-O2` debugging reality, `.gdbinit`) · `07`
perf/valgrind/sanitizers (`perf stat` signal→fix table, the sanitizer
matrix, a no-dependency benchmark harness) · `08` HFT production tuning
checklist (BIOS → kernel cmdline → IRQs → memory → pinning → network →
warm-up → continuous verification) · `09` memory ordering (the 6 orders,
the release/acquire handoff, `cmpxchg` weak/strong, ABA + fixes,
lock-free vs wait-free) · `10` latency numbers (the core ladder,
bandwidth, sizes, derived rules of thumb — consistent with 46/12) · `11`
UB catalog · `12` HFT glossary · `13` 13 one-page interview-revision
sheets + the 6 behavioural stories + the make-a-market drill.
**No `.cpp`, no new C++-language rows** — this folder *condenses* folders
01–47. **Mojibake:** 0.

### Batch 11 part 3 (PHASE 33) — folder 47 (CODING-PROBLEMS)

**47 CODING-PROBLEMS** — a graded **practice bank**: 10 themed problem
files totalling **250 problems** (basics 30 · arrays/strings 40 ·
pointers/memory 25 · OOP/design 20 · STL 35 · templates 20 · concurrency
25 · lock-free 15 · optimize-this 20 · HFT 20). Every problem is
statement + `Pattern:` hint + `<details>` (approach + complexity) —
"solve it yourself first". A `11-solutions/` directory holds a README +
10 per-category writeups with full worked code for the problems that
actually need it (allocators, `unique_ptr`, `memmove`, Treiber stack +
ABA, SPSC ring, seqlock, flat_map, the order book, the latency
histogram, …). **10 runnable examples** (`examples/*.cpp`, one per theme,
`./build.ps1 folder 47-CODING-PROBLEMS` → **10/10 OK** strict, every
binary runs + asserts): `01_basics_kata` · `02_arrays_strings_kata` ·
`03_arena_and_pool` · `04_scope_guard_and_result` · `05_flat_map` ·
`06_crtp_and_detect` · `07_blocking_queue` (N-prod/M-cons checksum) ·
`08_seqlock` (2M writes, torn reads = 0, run at `-O2`) ·
`09_optimize_row_vs_col` · `10_order_book_ops` (add/cancel/match +
`crossed()` invariant). **Rule 2 carried through:**
`09_optimize_row_vs_col` measures **~45–70× at `-O2`** on this box — far
bigger than folder 32's "~7×" pointer-chase figure; the file explains why
(cache lines + 4K-page TLB thrash + row-major auto-vectorization stack)
and reports the measured number rather than rounding to the textbook one.
**No new C++-language rows** — this folder *drills* folders 01–46.
**Mojibake:** 0.

### Batch 11 part 2 (PHASE 33) — folder 46 (INTERVIEW-PREP)

**46 INTERVIEW-PREP** — 20 lessons (`01`–`20`) + 8 verified coding
examples + 4 design docs + 4 mock transcripts (`./build.ps1 folder
46-INTERVIEW-PREP` → **8/8 OK** under strict warnings). A **layered**
question bank per the spec's "build toward HFT interviews, don't jump":
`02`–`14` are Layer 1 (types/control/functions) through Layer 13
(feed handler / order book / risk / OMS / tick-to-trade architecture),
each answer cross-referencing the source course folder. Plus `01` (how
HFT interviews work — firm types, pipeline, the 3 grading axes),
`15` (system-design arc + 4 worked designs + rubric), `16`
(brainteasers/probability — EV, the make-a-market game, Monty Hall / 100
seats / HH-vs-HT / 25 horses / ants, Fermi, mental math, Kelly), `17`
(30+ C++ trick questions with the right answer + why), `18` (behavioural
— STAR + the 6 stories + discussing this course's projects), `19` (4 mock
scripts with strong/weak tracks + rubrics), `20` (HFT resume & the 7
projects that matter — all folders 39–44). Coding examples: reverse list,
LRU cache, **lock-free SPSC ring**, fixed-point price parse, object pool,
L2 top-of-book, atoi + overflow clamp, O(1) moving average + division-
free signal — each with an assertion `main()`, complexity notes, and the
HFT follow-up. **No new C++-language rows** — every concept was already
tracked under folders 01–45; this folder is the applied **verbal /
whiteboard readiness** layer, completing `HFT-COMPLETENESS-AUDIT.md`
Section L. **Mojibake sweep:** 6 → 0.

### Batch 11 part 1 (PHASE 33) — folder 45 (DEBUGGING, skill folder)

**45 DEBUGGING** — 13 lessons (`01`–`13`) + 5 standalone examples + 10
"find & fix" buggy programs (`./build.ps1 folder 45-DEBUGGING` → **5/5 OK**
under strict warnings; the 10 buggy programs compile-clean in `checkall`,
misbehave only at runtime). A **skill folder** (like 20-DSA / 35-PROFILING):
skim once, return per-bug. Section 6 (Tooling) rows **GDB / Sanitizers /
Valgrind / perf(-for-debugging) / Core dumps / Reverse debugging / Logging
strategy / Debugging mindset / Bug catalog** all move to `[✓]` here.
Content: mindset (hypothesis→test, reproduce-first, `git bisect`, root
cause ≠ symptom) · GDB basics + advanced with **real transcripts on this
box** (MinGW GCC 15.1.0, GDB 16.3) · crashes + core dumps + signals ·
`-O2` debugging (`<optimized out>`, constant-folded functions, inlined
frames, `-Og`) · sanitizers (ASan/UBSan/TSan/MSan internals + combine
matrix + CI build) · valgrind (memcheck/helgrind/DRD/massif) ·
multithreaded (`thread apply all bt`, deadlock signature, heisenbugs) ·
reverse debugging (`rr`, `watch` + `reverse-continue`) · logging (levels,
structured, **HFT async logger** — POD enqueue ~20–40 ns to an SPSC ring)
· `perf` for bugs (off-CPU, syscall storms, false sharing) · a 40+-entry
**symptom → tool → fix** bug catalog. **No new C++-language rows** — the
underlying bug families were already covered under folders
09/12/13/14/17/18/22/25/26/27/28; this folder is the applied **tooling /
diagnosis** layer. **Rule-2 carried through:** `10_data_race.cpp` races
~15–20% of `-O0` runs but **hides at `-O2`** (RMW register promotion) —
taught, not hidden. **Mojibake sweep:** 34 → 0.

### Batch 10 part 9 (PHASE 32) — folder 44 (HFT-PROJECTS, the CAPSTONE)

**44 HFT-PROJECTS** — 15 lessons (`01`–`15`) + 12 example drivers + 11
shared `mh_*.hpp` headers (`./build.ps1 folder 44-HFT-PROJECTS` →
**12/12 OK** under strict warnings). The course climax: everything from
folders 01–43 assembled into one `MiniHftEngine` — MarketData → Parser →
L2Book → Strategy → Risk → OMS → Venue → fills → PnL — single-threaded and
fully deterministic (no wall clock anywhere; every timestamp comes from
the event stream, so replay is byte-identical). **Reuse over rewrite:**
`mh_types.hpp` `#include`s folder-40's `matching_engine.hpp` (the `Price`/
`Qty`/`OrderId` types **and** the `MatchingEngine` itself, used as the
venue via `ExecutionSimulator`); `07_spsc_queue.cpp` `#include`s
folder-41's `spsc_queue.hpp`; the book/pools/strategy follow folder
39/14/36/43 patterns. **The capstone optimization** applies the folder-43
loop to the whole engine: `MiniHftEngine` is `template <class Venue>` —
`NaiveEngine` (std::map `MatchingEngine` venue) vs `OptimizedEngine`
(`FastVenue` = flat-array aggregate book + per-level FIFO + IOC sweep).
Profiling found the venue mirror was the `book` stage's bulk (~150 ns/msg);
`FastVenue` halves it (~67 ns/msg) → ~1.5–1.6× end-to-end.
`12_integration_tests.cpp` proves naive == optimized byte-for-byte across
5 seed/config combos **before** the speedup is reported, both deterministic,
invariants held. **No new C++-language rows** — every technique (fixed-point
integer arithmetic, `constexpr`, templates/`template <class Venue>`
dispatch, flat-array data structures, generation-checked handles, intrusive
free lists, SPSC lock-free hand-off, `bit_cast`/`memcpy` parsing, RAII
pools) was already covered under folders 14/19/21/25/27/28/36/39/40/41/43.
This folder is the **applied capstone integration proof** — it exercises
`HFT-COMPLETENESS-AUDIT.md` Section K (now complete). Section 7
(Performance) stays ~100% from folder 36. **Mojibake sweep:** 16 → 0.

### Batch 10 part 8 (PHASE 31) — folder 43 (HFT-OPTIMIZATION)

**43 HFT-OPTIMIZATION** — 17 lessons (`01`–`17`) + 8 examples
(`pipeline.hpp` shared header + `02_profile_analysis.sh` Linux/`perf`
workflow + 7 portable `.cpp`; `./build.ps1 folder 43-HFT-OPTIMIZATION` →
**7/7 OK** under strict warnings). The end-to-end optimization
*methodology* folder: measure → profile → hypothesize → change (one) →
re-measure → **explain what changed and why**, with a **correctness gate
that runs before the speedup gate**. The spine is `pipeline.hpp` — two
full tick-to-order pipelines on one deterministic feed: `PipelineV0`
(naive: `substr`+`std::stod` parse, `std::map<double>`+`std::list` book,
`std::deque` re-sum SMA, `std::string` encode) and `PipelineV3`
(hand integer parse + fixed-point price, flat-array book + cached
top-of-book + dense-id direct index, ring-buffer running-sum SMA with
**zero division**, fixed-layout POD encode). `04_before_after.cpp` runs
both in one process and verifies the order-fire tick stream is identical
(110/110) **before** reporting the ~60–80× speedup. Measured this box
(Zen 2, unpinned — ratios): end-to-end ~60–80× (v0 ~1.9–2.7 µs/tick →
v3 ~25–45 ns/tick); division `div`→shift ~17× / →magic-multiply ~13× /
→reciprocal-multiply ~7× (only the divide timed — the CLAUDE.md warning
about a prior benchmark hiding `%` in a loop explicitly heeded); struct
field-reorder 24→16 B, fat→SoA ~8×; fixed-point `0.1×10 =
0.99999999999999988898 ≠ 1.0`; **hot/cold split ~1% — an honest Rule-2
null result** (frontend isn't the bottleneck at this scale, carries
36/12; `[[gnu::cold]]`→`.text.unlikely` verified in asm, attribute kept
because its cost is 0). **No new C++-language rows** — every technique
(fixed-point/integer arithmetic, `constexpr` tables, template/CRTP
dispatch, struct layout, `[[likely]]`/`[[gnu::cold]]`, `__restrict`,
false-sharing padding) was already tracked under folders 19/21/25/27/28/
32/33/36. This folder is the **applied optimization-methodology proof**;
it deepens `HFT-COMPLETENESS-AUDIT.md` Sections C/D. Section 7
(Performance) stays ~100% from folder 36. **Mojibake sweep:** 298 stray
Devanagari/Cyrillic homoglyphs found across the drafts and fixed → 0.

### Batch 10 part 7 (PHASE 30) — folder 42 (HFT-NETWORKING)

**42 HFT-NETWORKING** — 17 lessons (`01`–`17`) + 8 examples (`./build.ps1
folder` → **0 real fail, 6/6 correctly skipped as linux-only**). The full
wire-to-wire path: an `INetworkReceiver` abstraction making the kernel-
bypass backend (kernel-socket now, Onload/ef_vi/DPDK config-swappable
later) a deployment decision, not an application rewrite; the full
bypass ladder explained with an honest trade-off table; a TX+RX same-
clock-domain hardware-timestamping fix for 30/09's relative-jitter-only
limitation; NIC/IRQ tuning as a real (syntax-checked) script; TCP
order-gateway tuning (`writev`, keepalive, `TCP_USER_TIMEOUT`) with a
mid-development factual correction (writev's win is NOT 30/03's ~40ms
Nagle stall — that needs the sender's Nagle actually on, which
`TCP_NODELAY` here prevents); and a full wire-to-wire capstone chaining
kernel timestamps across 2 network hops + processing. **Majority of
this folder's code is genuinely Linux-only** (multicast sockets,
`SO_BUSY_POLL`, `SO_TIMESTAMPING`, `MSG_ERRQUEUE`, TCP tuning) — this
dev box is Windows/MinGW with no WSL, so these files could not be
compiled or run here (the established `.linux.cpp` pattern from 29/30);
they were hand-reviewed carefully instead (several real sign-compare,
unbounded-blocking-recv, and empty-vector bugs caught and fixed), with
every number presented as an explicit unmeasured estimate. No new
C++-language rows (POSIX socket/syscall API usage was already tracked
under Systems programming via folders 29/30, which are "done at the
lesson level") — this folder is the **applied networking-architecture
proof**, tracked fully in `HFT-COMPLETENESS-AUDIT.md` Section J (now
9/9 `[✓]`).

### Batch 10 part 6 (PHASE 29) — folder 41 (HFT-CONCURRENCY)

**41 HFT-CONCURRENCY** — 14 lessons (`01`–`14`) + 8 examples + 2 shared
headers (`spsc_queue.hpp`, `seqlock.hpp`) (`./build.ps1 folder` → **8/8
OK**). Turns 40's single-threaded matching engine into a real
multi-threaded, thread-per-stage pipeline. A production `SpscQueue<T,
Capacity>` (28's padded + cached-index design, packaged as a reusable
template and reused across 3 more examples), a working simplified LMAX
Disruptor built from scratch (single-producer, N *independent*
fan-out consumers — not work-distribution like an SPSC queue —
gating sequences, natural batching, correctness-verified over 4M events
with 0 mismatches), a `Seqlock<T>` snapshot, and a genuine capstone
(`08_pipeline_demo.cpp`) — 3 real threads (feed/match/sink) carrying
40's ACTUAL `MatchingEngine`, not a toy stand-in, with end-to-end
latency measured via a timestamp-at-source-propagate-through pattern.
**Two headline measured findings**: seqlock beats `std::shared_mutex`
by **~2500–3500×** under an adversarial max-rate-writer stress test
(far more dramatic than 28's original ~80–100× under gentler
conditions — both `torn=0`), and core-pinning (`SetThreadAffinityMask`)
reduced scheduling-hiccup *frequency* (511 vs 782 / 20M iterations) but
produced a *worse* single max-delay outlier (15.6M vs 829K ticks) — an
honest demonstration that thread affinity is not the same thing as core
isolation. A real MinGW toolchain quirk was found and fixed along the
way: `std::thread::native_handle()` on this posix-threading build
returns a `pthread_t`, not a Win32 `HANDLE`, so measuring real per-thread
CPU time (needed for the busy-spin-vs-blocking trade-off — 28/06 had
only measured the latency half) required `DuplicateHandle(GetCurrentThread())`
called from inside the thread. Async logging measured ~14× hot-path
cost reduction (sync format+write vs lock-free enqueue). No new C++-
language rows (atomics, memory ordering, and the seqlock/SPSC/hazard-
pointer mechanics were already `[✓]` from folders 27/28; `atomic<shared_ptr>`'s
hot-path cost was already flagged in 28) — this folder is the
**applied, multi-threaded, real-pipeline proof**, tracked fully in
`HFT-COMPLETENESS-AUDIT.md` Section I (now complete, including 2
extension rows beyond the original checklist: wait-free reads and
priority inversion).

### Batch 10 part 5 (PHASE 28) — folder 40 (MATCHING-ENGINE)

**40 MATCHING-ENGINE** — 17 lessons (`01`–`17`) + 8 examples + 2 shared
headers (`matching_engine.hpp`, `engine_workload.hpp`) (`./build.ps1
folder` → **8/8 OK**). Price-time-priority matching on top of 39's
order book: all 4 order types (Limit/Market/IOC/FOK), 3 self-trade-
prevention modes (CancelNewest/CancelOldest/CancelBoth), deterministic
event sequencing, and event-sourcing replay. **Central correctness
finding**: a naive FOK precheck ("sum total resting qty at crossable
levels") silently violates FOK's all-or-nothing contract once combined
with self-trade prevention — STP can skip or abort on self-owned
liquidity that the naive sum still counted, producing a partial fill on
an order that promised "full or nothing." Fixed with an STP-mode-aware
precheck that walks the exact priority order the real match uses;
verified by a hand-crafted scripted test (naive sum 70 vs true-available
30, target 50 — demonstrates the exact divergence) AND a 30000-command
fuzz run against an independent `O(n)` reference engine (`RefEngine`,
plain `vector` + linear scan) whose own FOK precheck uses a *completely
different* strategy (copy the whole state, run a practice match on the
copy, discard it) — **0 disagreements, 0 self-trade leaks, 0 FOK
violations across 3774 FOK orders hit**. Determinism proven, not
claimed: `05_event_sourcing.cpp` replays an identical 20000-command log
into two independent fresh engines and diffs every trade field-by-field
— byte-identical. 43/43 scripted unit tests pass. Also documents a
dangling-reference trap caught during development (reading a resting
order's `qty` after `std::list::erase()` had already freed its node).
No new C++-language rows (all techniques — templated map-type dispatch,
`std::list`/`std::map` mechanics, RAII-style resource handling — were
already `[✓]` from folders 19/21/39) — this folder is the **algorithm/
correctness applied proof**, tracked fully in `HFT-COMPLETENESS-AUDIT.md`
Section H.

### Batch 10 part 4 (PHASE 27) — folder 39 (ORDER-BOOK)

**39 ORDER-BOOK** — 17 lessons (`01`–`17`) + 9 examples + 5 shared headers
(`./build.ps1 folder` → **9/9 OK**). **"Yeh HFT ka sabse classic interview
question aur sabse important data structure hai"** — built three times on
an *identical* operation workload, measured each time: V1 (`std::map` +
`std::list` + `unordered_map`, p50 140.3 / p99.9 1502.9 ns) → V2 (sorted
`std::vector` + `std::deque` — **OVERALL WORSE** than V1, p50 150.3 /
p99.9 2745.2 ns, despite Add improving 3.9×; root cause: the order-id
index can't safely store a vector iterator across mutation, so it falls
back to an `O(level size)` linear scan on cancel/execute — a genuine
Rule-2 finding, not a contrived one) → V3 (tick-indexed flat `std::array`
+ intrusive arena-linked-list + tombstone-based open-addressed flat hash
— wins every metric, p50 120.2 / p99.9 661.3 ns, ~2-4× better than both
V1 and V2). Correctness: all three proven behavior-identical via
cross-version equivalence checked *after every operation* (20000+
checkpoints across 4 seeds) and a 30000-op fuzz run injecting ~3000
deliberately-invalid operations (duplicate ids, ops on nonexistent ids,
over-execute clamping, replace-of-already-gone-orders) against an
independent `O(n)` reference model — zero disagreements. A real bug
(`best_bid()` called without checking `has_bid()`) crashed during
test-writing itself on an out-of-bounds array read, and is documented as
an explicit precondition rather than swept away. Lesson 15 gives the full
mechanism-level "what changed and why" accounting the spec requires — not
just a before/after numbers table.

### Batch 10 part 3 (PHASE 26) — folder 38 (MARKET-DATA)

**38 MARKET-DATA** — 16 lessons (`01`–`16`) + `17-exercises.md` + 10
examples + a shared `wire_protocol.hpp` (`./build.ps1 folder` → **10/10
OK**). **The domain track's first build**: an ITCH-style binary
market-data feed handler, taken through CLAUDE.md's full process —
build simple/correct (03) → measure (04: naive parser **p99.9 771.5 ns**)
→ optimize (05: overlay-cast reads, zero allocation, fixed sink) →
re-measure (06: **p99.9 ratio 22.2× — 771.5→40.1 ns, p50 unchanged at
30.1**) → integrate (10: end-to-end `FeedHandler` == standalone optimized
parser numbers, proving framing+gap-detection added no extra tail).
Also: byteswap cost measured (**0.753 ns/swap**) — used to correct a
common oversimplification ("binary wins because swap is avoided"; the
real win is eliminating text-to-number parsing) and to explain SBE's
native-endian choice honestly (codegen-simplicity, not raw speed); A/B
feed arbitration measured (**~49.5× loss reduction, zero round-trips**);
gap detection sanity-verified exact against injected drops (96/20000).
No new C++-language rows (all techniques were already `[✓]` from folders
19/25/36) — this folder is the **applied proof**, tracked fully in
`HFT-COMPLETENESS-AUDIT.md` Section F (14/14).

### Batch 10 part 2 (PHASE 25) — folder 37 (HFT-FUNDAMENTALS)

**37 HFT-FUNDAMENTALS** — 16 lessons (`01`–`16`) + `17-exercises.md` + 4
examples (`./build.ps1 folder` → **4/4 OK**). This is **domain knowledge, not
C++-language content** — so it adds nothing to the section tables below
(scores unchanged from folder 36). It covers what HFT is/isn't, exchange
architecture, market microstructure + adverse selection, order types,
bid-ask economics (spread/mid/**microprice**/imbalance — measured), order
book concept, **price-time priority vs pro-rata matching (measured
side-by-side)**, tick/lot/price-band constraints, maker-taker fees, strategy
categories (no alpha), colocation, the end-to-end system architecture
diagram, risk systems (fail-closed), tick-to-trade latency budgeting
(measured tool), India-vs-US market structure, and regulatory basics. Full
tracking is in `HFT-COMPLETENESS-AUDIT.md` Section E (now 14/14 `[✓]`).
**Folders 00–37 are now all complete.** Next (38–44) is the build track —
market data parsing, order book, matching engine, HFT concurrency/
networking/optimization/projects — which *will* add new rows here
(zero-copy wire parsing, binary protocols, more lock-free patterns).

### Batch 10 part 1 (PHASE 24) — folder 36 (LOW-LATENCY-CPP)

**36 LOW-LATENCY-CPP** — 24 lessons (`01`–`24`) + `25-exercises.md` + 12 examples
(`./build.ps1 folder` → **12/12 OK**). **The synthesis folder**: everything from
folders 12–35 applied to hot-path engineering, and — per CLAUDE.md spec Rule
12–13 — **every technique paired with its hidden cost and an explicit "when NOT
to"** (lesson 24 is a whole trade-off catalogue + the measure→profile→one
change→re-measure→explain process). Contents: **latency/throughput/jitter** (three
axes, budget thinking) · **tail latency** (p99.9 is the scorecard; **C++ has no
GC** — the tail sources are allocator internals / faults / syscalls / locks, all
eliminable) · **jitter-source audit checklist** (eliminate / bound / make-rare) ·
**allocation avoidance** (measured: mixed-churn `new` p99.9 **2585 ns**; the
hidden allocations — vector grow, string SSO, map node, `std::function` capture,
`throw`, `std::to_string`, page fault) · **pre-allocation** (warm-up vs steady
state; sizing from historical peak; *proving* zero-alloc — `null_memory_resource`
upstream / a global-`new` hook / `perf stat -e page-faults`) · **memory pools**
(build `FixedPool` — intrusive free list, `O(1)`, `p99.9` **30 ns flat** vs `new`
180) · **object pools** (construct-on-acquire vs **recycle-pre-constructed** —
p50 30 vs 20 ns; recycle's danger = stale fields; `std::launder`) · **arenas /
monotonic buffer / PMR** (`04`: ~16× vs new/delete; `05`: `pmr::vector` on a
stack buffer, **0 global `new`**) · **custom allocators** (PMR vs classic
`Allocator<T>`; `scoped_allocator_adaptor`; why a single-size pool can't back a
`pmr::unordered_map`) · **cache locality** (hot/cold field split, AoS/SoA, flat
structures — recap of 32 + practice) · **false sharing** (measured 3.5–44×,
run-to-run — a *jitter* source; `perf c2c` / HITM; pad / per-thread + combine) ·
**branch-free** (when it wins, when it *loses* on predictable branches; `-O2`
if-conversion — `06` measured the "branchy" `if` becoming `cmov`; switch-vs-table
**~12×** on random data) · **virtual dispatch elimination** (CRTP / variant /
switch / fn-table, homo vs hetero — `07`: virtual **7.0 ns** vs CRTP **0.6 ns**
hetero; bucket-by-type) · **`std::function` cost** (`function_ref` (2 words, no
alloc), the SBO cliff — `08`: a 64-B capture → `operator new` in the ctor,
proven) · **ring buffers** (pow-2 `& mask`, monotonic counters, release/acquire,
**cached opposite index** = the ladder's biggest win, `alignas(CL)` on
head/tail/buf, drop-don't-block, + 4 "wrong tool" cases) · **batching** (the
throughput vs head-of-line curve — `10`: B=1 6.7 ns HoL 6.7 → B=1024 1.5 ns HoL
**3095 ns**; opportunistic batching) · **syscall avoidance** (busy-poll + its
100%-CPU cost, `SO_BUSY_POLL`, **`io_uring` SQ/CQ + `SQPOLL`** = zero syscalls,
kernel bypass) · **page-fault avoidance** (`MAP_POPULATE` / `mlockall` / write-
touch / stack pre-fault; huge pages & THP jitter — `11`: COLD p99 **2875 ns** vs
WARM **30 ns**) · **CPU pinning** (`isolcpus` / `nohz_full` / `rcu_nocbs`, a
concrete core plan, SMT, NUMA first-touch, the `SCHED_FIFO` hang hazard) ·
**cache warming** (cold start vs decay; dry-run + the `dry_run`-flag layout
requirement) · **I-cache** (Frontend Bound; hot/cold split — `12` measured
**nothing** in a micro-bench, kept honest; PGO/LTO/BOLT; why `always_inline`
everything *hurts*) · **zero-copy** (views / spans / overlays; `from_chars` /
`to_chars`; the 4 overlay caveats — alignment / endianness / lifetime / aliasing)
· **compile-time dispatch** (`if constexpr`, non-type params, startup fn-pointer
pick; 2^N instantiation bloat → Frontend Bound) · **honest trade-offs** (each
technique's hidden cost table; six "when NOT to"; documenting a floor you can't
beat).

**Measured (AMD Ryzen 7 4700U, Zen 2, ~2 GHz, Windows/MinGW, unpinned, SSE2 —
ratios/shapes port, tail absolutes don't; per-op-timed loops' `max` = OS-
interrupt noise, p50/p99/p99.9 are the signal):** see the per-example numbers
above.

**⚠️ CLAUDE.md Rule 2 in folder 36 (three times, all taught):** (a) `06` — the
"branchy" `if` gave *identical* sorted vs unsorted times → `-O2` if-converted it
to `cmov`, so there was no branch to mispredict (verify in the asm — 33/08);
(b) `08` — templated / raw-fn-ptr / `function_ref` **tie at ~1.5 ns** because the
loop is *latency-bound* on a carried hash (OoO overlaps the call overhead behind
the dependency chain); a *throughput* loop would let templated pull ahead
(inline → vectorize) — *what the loop is bound by* decides whether inlining
matters; (c) `12` — hot/cold split A/B/C measured **no difference** (~1.49 ns
each) because the micro-bench's hot loop already fits L1i, so layout had nothing
to fix. Kept in the folder deliberately: a technique that "should" help showing
nothing in a given context is the norm, not the exception — measure in situ
(35/11).

**Mojibake sweep:** 11 → **0** stray Devanagari tokens.

### Batch 9 part 5 (PHASE 23) — folder 35 (PROFILING-BENCHMARKING) — BATCH 9 DONE

**35 PROFILING-BENCHMARKING** — 16 lessons (`01`–`16`) + `17-exercises.md` + 8
examples (5 portable `.cpp` — `./build.ps1 folder` → **5/5 OK** — +
`05_google_benchmark/` multi-file `.cxx`/`.hpp` Google-Benchmark **shim** +
`06_perf_workflow.sh` / `07_flamegraph.sh` self-contained Linux scripts).
Contents: **why measure** (intuition fails, **Amdahl's law** with the numbers
table, premature-optimization's real meaning, throughput vs latency trade-off) ·
**`<chrono>`** (steady vs system vs high_resolution — the alias trap;
resolution/precision/accuracy/self-cost; `duration_cast` truncation; `clock()`
CPU-time) · **`rdtsc`** (fencing recipes, ticks→ns calibration via busy-wait not
sleep, core-hop mitigations, self-cost subtraction, when `chrono` instead) ·
**statistics** (mean lies on right-skewed latency, median/min/max, σ useless for
non-normal + CV/MAD/IQR, bimodal = two code paths, sample-size vs percentile) ·
**percentiles** (nearest-rank vs interpolation, "nines" language, **fan-out tail
amplification** — Google's "tail at scale", **coordinated omission** + fix,
percentiles-don't-average → merge histograms) · **jitter** (SW sources table:
timer tick / scheduler / IRQ / page fault / `malloc` / syscall / lock; HW &
firmware: frequency scaling / C-states / **SMI** / SMT sibling / NUMA / thermal;
`cyclictest`/`rtla`/`hwlatdetect`; the "quiet core" recipe — BIOS + kernel
cmdline `isolcpus`+`nohz_full`+`rcu_nocbs` + runtime pin/`mlockall`/no-alloc) ·
**histograms** (why linear buckets fail for latency, **log-linear /
HdrHistogram**, relative-error ↔ `SUB_BITS`, CDF / percentile plot > PDF bars,
mergeable + O(1) + CO-correction, sampling vs full record) · **Google Benchmark**
(`State` loop model, `DoNotOptimize`/`ClobberMemory`, `Range`/`Args`/
`SetBytesProcessed`, `PauseTiming` + its own cost, fixtures, `--benchmark_
repetitions`/`_cv`, the 10 mistakes the library can't fix) · **benchmark
pitfalls** (DCE / constant-fold / loop-invariant hoist / cold-start-4-components
/ timer-overhead>op / one-run / **alignment & code-layout noise** — trust only >
noise / **frequency scaling** — report cycles/op / **denormals** — FTZ/DAZ /
bench≠reality) · **`perf` basics** (counting vs sampling; `perf stat` IPC + cache
rates + branch-miss; **top-down** Retiring/Frontend/Backend/Bad-Spec with fix
directions; `perf record -g` frame-pointer vs dwarf vs lbr; `perf report`
self/children; `perf list`; `perf_event_paranoid`; sample skid) · **`perf`
advanced** (`perf annotate` per-instruction % + skid → **`:pp` PEBS/IBS**;
`cycle_activity.stalls_l3_miss` to quantify memory-bound; **`perf mem`**
per-access latency + served-from; **`perf c2c`** false sharing / HITM; LBR →
AutoFDO; `perf script`; counter multiplexing) · **flame graphs** (axes:
width=time / X=alphabetical / Y=depth; top plateau vs distributed; `stackcollapse`
→ `flamegraph.pl`; on-CPU vs **off-CPU**; differential red/blue; icicle;
`hotspot`/Firefox Profiler/Speedscope) · **Valgrind** (DBI model — deterministic
but 20–100× + simulated cache; **cachegrind `Ir`** = deterministic CI gate;
callgrind + KCachegrind; massif heap-over-time; **DHAT** per-alloc-site
size-vs-usage / churn / realloc) · **VTune / top-down** (recursive tree
Backend→Memory→DRAM→**Bandwidth-vs-Latency** with different fixes; analysis types;
**`toplev.py`** for the same on plain `perf`; AMD uProf / Instruments / Streamline)
· **sanitizers** (ASan / UBSan / TSan / MSan — what each catches + slowdowns;
`volatile` vs `std::atomic` for a cross-thread flag; **never for perf numbers** —
redzones/shadow change cache & timing; ASan+UBSan+TSan CI matrix; MinGW fallback
= `_GLIBCXX_ASSERTIONS` + stack-protector) · **production measurement** (6
requirements; inline `rdtsc` + per-thread HdrHistogram snapshot by a housekeeping
thread; **SPSC ring → aggregator** for zero hot-path math; sampling strategies
1-in-N / time-gated / exceedance; **coordinated omission in prod** = timestamp at
arrival not dequeue; per-thread → merge, never average p99s; white box vs black
box + NIC HW timestamps; USDT / uprobe / eBPF / LTTng).

**Measured (AMD Ryzen 7 4700U, Zen 2, ~2 GHz, Windows/MinGW, unpinned — ratios
and shapes port, tail absolutes don't):** `01` chrono resolution **100 ns**,
`clock()` **1 ms** (MinGW `CLOCKS_PER_SEC=1000`), TSC calib **~1.996 ticks/ns**,
self-cost `steady_clock::now()` **~38 ns** / plain rdtsc **~9 ns** / fenced
**~18 ns**, 94 µs workload chrono vs rdtsc agree to **0.02%**. `02` 200k samples:
mean ≈ median (bulk symmetric) yet **max/median 60–175×**, p99.99/median **7–26×
run-to-run**. `03` identical work: **min 200 ns both phases**; CLEAN spikes(>2×)
**~2800/300k**, NOISY **~6300/300k** — spike-count + p99.9 robust, single `max`
noisy. `04` (`-O2`): DCE / const-fold / hoist all **BUG 0.000 ns/op** vs FIX
~0.85–1.0; timer-per-op **36.8** vs batched **2.99**; one-run spread **35%**.
`05` shim: `BM_reduce_NO_barrier` **0.49 ns/iter (DELETED)** vs `_WITH_barrier`
**1030**; `memcpy/4096` **32 ns (128 GB/s)** vs `manual_copy` **1003 ns
(4 GB/s)**; `StringCopy/8` **3 ns (SSO)** → `/64` **49 ns** (heap). `08` recorder:
**~2 ns/record**, **29.5 KB fixed**, percentiles vs exact **≤1.3%**, log-plot
**bimodal** (bulk ~276 ns + spike hump ~10–22 µs).

**⚠️ CLAUDE.md Rule 2 in folder 35:** the "BUG" benchmarks in `04`/`05` really
do report **0.000 / 0.49 ns/op** at `-O2` — that's the taught result next to its
FIX. `04` uses a `volatile` global sink + source (not asm `keep()`) because
`keep()`'s `"+r,m"` hits "impossible constraint" on a constant-folded value.
`04` case 4 "cold start" is only **~1.2×** here (Windows commits pages eagerly) —
kept honest with the note that it scales with working-set / backing store.
`02`/`03` tail numbers are deliberately **run-to-run unstable** — the lesson is
that this is an unpinned Windows desktop; a pinned Linux isolated core would be
tight (03/06 teach the contrast). Mojibake sweep: 52 → **0**.

### Batch 9 part 4 (PHASE 22) — folders 33 (COMPILER-OPTIMIZATION), 34 (ASSEMBLY)

**33 COMPILER-OPTIMIZATION** — 15 lessons + `16-exercises.md` + 8 examples (6
portable `.cpp` — `./build.ps1 folder` → 6/6 OK — + `06_lto_demo/` multi-file
`.cxx` + `07_pgo_workflow.sh`). Contents: **`-O` levels** (`-O0`→`-Ofast`,
per-file/-function override, `-g` on release; measured **-O0 80 ms → -O1 13.4 ms
= the ~6× cliff**, -O1..-O3..-Os equal for a scalar reduction) · **godbolt
workflow** (the hot-asm checklist, `-fopt-info-*`, `objdump`, `llvm-mca`) ·
**inlining** (`inline` = ODR permission not a directive; size/hotness heuristics;
`always_inline`/`noinline`/`flatten`; the cross-TU / no-LTO wall; inlining as the
*enabler* of const-fold/CSE/vectorize/devirt across the boundary; measured
noinline **1.31 ns/iter** vs inlined ~1.0) · **loop optimizations** (LICM,
strength reduction, unroll + the "one accumulator → serial chain" trap, fusion/
fission, interchange = `-O3`-only, unswitch, rotation, IV elimination; blockers +
`-fopt-info-loop`) · **auto-vectorization** (the conditions; **a float reduction
does NOT vectorize at `-O2`** — reassociation not allowed — measured **~4× only
with `-ffast-math` / `#pragma omp simd`**, plain map **~3.5×**; `-fopt-info-vec
[-missed]`; `ivdep`/`assume_safety` = unchecked promises; SoA prereq) ·
**constant folding / propagation / DCE** (whole functions → `mov eax, const`;
inter-procedural clones; `constexpr`/`consteval`/`constinit`; `if constexpr`;
`[[assume]]`; why benchmarks need barriers) · **devirtualization** (exact-type /
`final` / speculative-via-LTO / PGO; the honest "won't fire for a heterogeneous
container"; measured elsewhere virtual ~23 ns vs CRTP ~2.2 ns) · **branch hints**
(`[[likely]]`/`[[unlikely]]`/`__builtin_expect` = **code layout, not
prediction**; measured **~1.3×** — the HW predictor already nails a 1/1000
branch; wrong hint = regression; PGO does it better) · **aliasing & `__restrict`**
(how it blocks LICM/CSE/vectorize; TBAA / strict-aliasing UB; `std::bit_cast`
not `*(T*)`; the local-copy fix; **measured ~3.5×** (ex 04) — and the pessimism
**only appears with `[[gnu::noinline]]`**, inlining/LTO lets the compiler prove
non-aliasing from the call site) · **LTO** (the boundary it removes; cross-TU
inline/const-prop/devirt/DCE/ICF; ThinLTO; flag consistency; the ODR-exposure
risk; **measured ~2.3×** on a *throughput* loop (ex 06), **no gain** on a
*carried* loop — Rule 2 nuance) · **PGO** (the 3-step + AutoFDO; what it improves
— branch layout / inlining / function ordering; the representative-profile
hazard; HFT = a replayed session) · **`-march`/`-mtune`** (x86-64-v1..v4;
`native` is wrong for a shipped binary; AVX-512 downclock; function
multi-versioning) · **`-ffast-math` dangers** (the 8 sub-flags; **`-ffinite-math
-only` compiles your `isnan` guard to `false`**; `-fassociative-math` changes
results; `-ffp-contract`; safe scoped alternatives — N accumulators in source /
`#pragma omp simd`) · **preventing optimization** (`DoNotOptimize`/`ClobberMemory`
= zero instructions; `volatile` sink cost; latency-vs-throughput barrier
placement; measured **no-barrier loop = 0.00 ms**) · **reading optimized output**
(the verification checklist; vectorized vs scalar asm signature; `idiv` in a
loop; right-asm-unchanged-time = memory-bound). **CLAUDE.md Rule 2 fired 3× and
is taught:** the float-reduction refusal, the aliasing-only-with-noinline, and
LTO-only-on-throughput.

**34 ASSEMBLY** — 13 lessons (`01`–`12` + `13-exercises.md`) + 7 examples (5
`.cpp` — `./build.ps1 folder` → 5/5 OK — + `01_simple_functions.s` +
`07_asm_puzzles.md`). Goal: **read** x86-64 asm (never write it) for
verification / optimization / debugging. Contents: registers + the sub-register
zeroing rule + `xmm`/`ymm`/`zmm` + rflags · **AT&T vs Intel** (the 5 mechanical
differences; `gcc -S`/`objdump`/`gdb`/`perf` = AT&T, godbolt/`./build.ps1 asm` =
Intel) · the ~20 common instructions (`lea` computes an address ≠ `mov [..]`;
`cmp`/`test` set flags only; recognize `div`/`idiv` = slow non-constant divisor,
`rep movs` = a copy idiom, `lock`/`xchg`/`mfence` = atomics) · **addressing
modes** (`base + index*scale + disp` → recover `sizeof(struct)` and field offsets
from a loop) · **stack frames** (prologue/epilogue with & without a frame
pointer; spills = register pressure; tail call → `jmp` loop; Win64 32-byte
shadow space; stack canary) · **calling conventions** (System V — args
`rdi,rsi,rdx,rcx,r8,r9` — vs Windows x64 — `rcx,rdx,r8,r9` — where `this` is,
large-struct return via a hidden pointer, `extern "C"`, SysV varargs `al`) ·
**pattern recognition** (`if` / if-converted `cmov` / loop (backward jXX + entry
guard) / `while` / dense `switch` (jump table) & sparse (if-chain) / call /
virtual `call [reg+off]` / constant-folded `mov eax,const` / reciprocal-multiply
division) · **SIMD asm** (scalar `ss`/`sd` vs packed `ps`/`pd`/`d`; width from
`add ptr, 16/32`; the horizontal-reduce cluster; `vzeroupper`; `vgather` is
slow) · **inline asm** (the 4 sections; `"=r"`/`"+r"`/`"=m"`/`"memory"`/`"cc"`;
a missing clobber = silent `-O2`-only corruption; use it only for the barrier /
`cpuid` / `rdtsc` / `pause` / MSRs — everything else → intrinsics) · **`rdtsc`
timing** (invariant-TSC counts reference ticks not core cycles; fencing with
`lfence`/`rdtscp`; calibrate for ns; core hopping; `clock_gettime` via vDSO for
≥ 1 µs) · **disassembly tools** (`g++ -S` vs `objdump -dS` vs `perf annotate`
`:pp` vs `gdb disassemble /s` vs `addr2line -i` vs `llvm-mca` — which for which
question). Measured (`05_rdtsc.cpp`, ~2 GHz): calibration **~2.0 ticks/ns**;
`__rdtsc` self-cost **~1 tick (~0.5 ns)**; `lfence;rdtsc;lfence` / `rdtscp+lfence`
**~20 ticks (~10 ns)**. 10 "which C++ made this asm?" puzzles with answers.

### Batch 9 part 3 (PHASE 21) — folder 32 (CACHE-MEMORY-PERFORMANCE)

Full folder: 15 lessons + `16-exercises.md` + 8 **portable** `.cpp` examples
(all run on x86-64 incl. MinGW/Windows — 8/8 OK under strict flags) +
`09_perf_analysis.sh` (Linux `perf` workflow, not compiled). Contents: **memory
hierarchy** (registers→L1→L2→L3→DRAM→disk, the "1 second = L1" latency ladder,
this box's geometry, a `load`'s full path) · **cache lines** (64 B = unit of
transfer / coherence, why 64, `hardware_destructive_interference_size`, line
straddle, `alignas(64)`) · **cache organization** (direct-mapped → N-way,
index/tag/offset, pseudo-LRU/RRIP, **critical stride** = size/assoc, power-of-two
dimensions poison, VIPT / page coloring) · **the 3 C's** (compulsory / capacity /
conflict + coherence, a symptom→cause table, **MLP** — which misses OoO hides,
which it can't) · **locality** (spatial/temporal, loop interchange / fusion /
fission / tiling, prefetcher-friendly vs hostile patterns) · **prefetching** (HW
prefetchers + limits; `__builtin_prefetch` rw/locality/distance; ⚠️ **measured
marginal-or-harmful on a big OoO core**; `PREFETCHNTA`) · **false sharing deep**
(MESI ping-pong, `perf c2c`, padding / per-thread-local, constructive sharing,
and why it's *jittery*) · **cache-friendly data structures** (flat vs
pointer-based; `map` ~20 serial misses vs `btree_map` ~3–4 vs `flat_hash_map`
~1; arena + `uint32` index links; CSR vs adjacency lists; SBO types) · **AoS vs
SoA deep** (three access patterns → three winners, AoSoA, SoA as the
vectorization prerequisite) · **data-oriented design** (philosophy, `vector<Base*>`
+ `virtual` hot-loop cost breakdown, existence-based processing, handles +
generation counters, where OOP still fits) · **TLB & huge pages** (4-level page
walk, dTLB/STLB reach math, 2 MiB → 512× reach, explicit hugetlbfs vs THP
downsides, pre-fault + `mlockall`, Windows large pages, NUMA first-touch) ·
**store buffers** (RFO = writes cost a read, store-to-load forwarding stalls,
write-combining, non-temporal stores + `sfence`) · **memory bandwidth**
(latency- vs bandwidth-bound, Little's Law → one core ≈ 18 % of socket peak,
STREAM, roofline / arithmetic intensity, "SIMD a memory-bound loop = wasted",
the add-threads test) · **measuring** (`perf stat` + MPKI, `--topdown` /
toplev, `perf record`/`annotate`, `perf c2c`, cachegrind, VTune/uProf, a
7-step workflow) · **11 optimization recipes** in impact order, each with its
trade-off, and the apply-one-remeasure-explain discipline.

Benchmarks measured at `-O2` on **AMD Ryzen 7 4700U (Zen 2, ~2 GHz throttled)** —
shapes/ratios port, absolutes don't. **CLAUDE.md Rule 2 fired three times and is
taught explicitly, not hidden:** (a) example `03` — `-O2` column-major traversal
~10× slower, but `-O3 -march=native`'s `-ftree-loop-interchange` swaps the nest
→ ratio ~1.0 (the compiler fixed the cache-hostile loop); (b) example `06` —
`__builtin_prefetch` on an independent gather is **~1.1×** (MLP already overlaps
~10 misses) and on a memory-saturated heavy loop **~0.33× = 3× slower** (prefetch
requests contend for LFBs/bandwidth); (c) example `07` — a naive 64×64 blocked
matmul is **~10–15 % *slower* than a plain `ikj` loop order** (ikj already
auto-vectorizes and L3 absorbs the reuse; blocking needs a tuned microkernel to
pay). Other measured: sequential vs random line access ~7× (14 vs 2 GB/s, ex 02);
false sharing **~6× to ~44×, run-to-run** — the variance is the lesson (ex 04);
AoS/SoA — SoA ~2.0–2.4× on sequential scans (gap doesn't shrink, SoA vectorizes),
AoS ~3× on random whole-record access (ex 05); TLB/cache latency cliff ~1.2 →
~95 ns at ~8 MiB working set (ex 08). Mojibake sweep 139 → 0.

### Batch 9 part 2 (PHASE 20) — folder 31 (CPU-ARCHITECTURE)

Full folder: 15 lessons + `16-exercises.md` + 8 **portable** `.cpp` examples
(SIMD + `rdtsc` + CPUID run on x86-64 including MinGW/Windows — 8/8 OK).
Contents: fetch-decode-execute-retire + cycles/ns · registers (sub-register
scheme, `rflags`, `xmm`/`ymm`/`zmm`, architectural vs physical + renaming,
spilling, `mxcsr` flush-to-zero) · ISA / µops / macro-fusion / **microcode**
(`div`, denormals, `gather`, transcendentals) / `-march=x86-64-v2/v3/v4` ·
**pipelining** (latency vs throughput, 4 hazard types, ~15–20 cyc flush/refill) ·
**superscalar** (execution ports, ILP, IPC, limiting-port analysis) ·
**out-of-order** (ROB / register renaming / reservation stations, speculation +
rollback, the OoO window, why a dependent chain of cache misses can't be hidden)
· **branch prediction** (gshare/TAGE, BTB/RAS, indirect/virtual, `[[likely]]`,
warm-up) · **speculative execution** (Spectre/Meltdown one-paragraph, KPTI /
retpoline cost, `mitigations=off` preconditions + when it barely helps a
busy-poll loop) · **instruction latency/throughput** (Agner Fog / uops.info /
`llvm-mca`, computing the critical path, the division problem: constant →
compiler, invariant → hoist reciprocal, struct-time → `libdivide`) · **SIMD
basics** (SSE2→AVX→AVX2+FMA→AVX-512, good/bad-for, SoA prerequisite, alignment,
AVX-512 downclock) · **SIMD intrinsics** (`_mm256_*` naming, the core patterns,
tail handling, auto-vec vs manual vs Highway/xsimd) · **hyperthreading** (SMT
shares L1/ports/ROB/predictor → jitter → HFT disables it or isolates the
sibling) · **frequency & power** (P-states/turbo, C-states + ~tens-µs wake,
thermal throttle, AVX frequency offset, uncore) · **NUMA hardware** (sockets +
UPI/IF, chiplet/CCX & Intel SNC = NUMA within a socket, the latency ladder,
NIC's node) · **CPU differences** (Intel vs AMD per-CCX L3, `pdep` microcoded on
Zen 1/2, ARM64/Graviton re-audit, "benchmark on the deployment target,
frequency-locked").

**CLAUDE.md Rule 2:** examples `01`/`03` first showed *no effect* — `-O2` DCE'd
the ADD/DIV dependency chains and if-converted the branch to `cmovge`. Fixed
with `keep()` inline-asm barriers, `#pragma GCC optimize("no-if-conversion")`,
and a threaded `carry` to defeat call-hoisting; the "compiler already made it
branchless" fact is taught explicitly. Measured (AMD Zen 2, ~2 GHz throttled):
ILP serial→4-parallel **~4×**; unpredictable branch **~6–7×** slower; branchless
**~6–7×** on random data; float sum scalar→SSE **4.1×**→AVX2 **~9.5×**;
same-code auto-vectorized **~2.5–3×**; `rdtsc` self-cost ~20 cyc vs serialized
~40–90 cyc. Numbers illustrate **shapes/ratios** (frequency-locked production
box gives different absolutes — files 13/15).

### Batch 9 part 1 (PHASE 18–19) — folders 29 (LINUX-SYSTEMS), 30 (NETWORKING)

The first genuinely **Linux-only** folders in the course. This repo builds on
MinGW-w64 / Windows with no WSL distro, so most of what these folders teach
(`fork`, `mmap`, `sched_setaffinity`, `epoll`, POSIX sockets, `SO_TIMESTAMPING`,
`SCHED_FIFO`, huge pages, NUMA syscalls) **cannot compile on this box**. The
user chose *"all real Linux code, marked"*: every example is correct, idiomatic
Linux C++20 with a header banner and an `EXPECTED OUTPUT` comment block, and a
new `build.ps1` skip-hook treats `*.linux.cpp` as `SKIP (linux-only)` in both
`folder` and `checkall` (a dedicated bucket, not `FAIL (real)`).

- **29 LINUX-SYSTEMS** (18 lessons): kernel vs userspace / ring 0-3 · **syscall
  cost** (~300 ns fixed, KPTI/retpoline; vDSO ~20 ns; count-reduction) ·
  **processes** (`fork` COW, `exec`, `wait`, zombie/orphan, `clone`,
  `posix_spawn`, `pidfd`) · **signals** (async-signal-safety — only `write` +
  atomics + `_exit`; "flag + main loop"; `signalfd`; hot-thread masking) · **fds**
  (3-level table, short read/write, `O_CLOEXEC`, `dup2`, `RLIMIT_NOFILE`,
  `writev`) · **`/proc` & `/sys`** (introspection + sysctl tuning) · **`mmap`**
  (file/anon × shared/private, lazy fault, `MAP_POPULATE`, TLB shootdown) ·
  **shared memory** (`shm_open`+`mmap`, cross-process SPSC rings = folder 28's
  ring in `/dev/shm`, no shared locks) · **pipes/FIFO/UDS** (`SCM_RIGHTS` fd
  passing, latency: shm ~ns vs UDS/pipe ~µs vs loopback TCP ~10s µs) · **CPU
  scheduling** (CFS/EEVDF, `SCHED_FIFO` hazards, "isolated core + CFS busy-poll
  often beats FIFO") · **CPU affinity** (`sched_setaffinity`, `isolcpus`/
  `nohz_full`/`rcu_nocbs`, SMT siblings, startup self-check) · **huge pages**
  (TLB reach, THP vs hugetlbfs, `khugepaged` stall) · **page faults & mlock**
  (`mlockall` + pre-fault + dry-run = zero-fault steady state; `getrusage`
  counts) · **NUMA** (first-touch, the "init thread allocates everything" bug,
  `numactl`/`mbind`, `numa_balancing=0`, NIC node) · **IRQ affinity** (hard IRQ
  → softirq, `smp_affinity`, `irqbalance` off, coalescing) · **clocks** (vDSO
  `CLOCK_MONOTONIC` vs `_RAW` trap vs `_COARSE`, `rdtscp` + calibration,
  `clocksource=tsc`, busy-spin for sub-µs "act at T") · **cgroups/rlimits**
  (`RLIMIT_MEMLOCK/NOFILE/RTPRIO`, `cpu.max` throttle = spike, container-
  awareness) · **HFT tuning checklist** (BIOS / kernel cmdline / sysctl /
  runtime / systemd unit / app-side / verify-by-effect — the deployable
  reference). 10 `*.linux.cpp` examples (all SKIP on this box): `01_syscall_cost`,
  `02_fork_exec`, `03_file_io_raw`, `04_mmap_demo`, `05_shared_memory`,
  `06_cpu_affinity`, `07_page_faults`, `08_hugepages`, `09_clock_comparison`,
  `10_realtime_thread`. Numbers in each `EXPECTED` block are **typical published
  Linux figures, explicitly "not measured on your machine"** (Rule 2 honest
  treatment — no fabricated "measured" values); the learner takes real numbers
  in `19-exercises.md` Part D on Linux/WSL. Lessons teach **ratios** (syscall vs
  userspace ~100–300×, cold vs warm fault ~20–60×, unpinned tail vs pinned
  ~5–50×).

- **30 NETWORKING** (16 lessons): OSI/TCP-IP + encapsulation + wire overhead ·
  **IP** (subnets/routing/MTU, fragmentation, PMTUD black hole, ARP) · **TCP
  deep** (handshake, seq/ack/window, RTO ~200 ms vs 3-dup-ACK fast retransmit,
  SACK, cwnd, TIME_WAIT) · **TCP latency issues** (Nagle + delayed-ACK ~40 ms
  deadlock — `TCP_NODELAY` + one `writev`/msg; `TCP_CORK`;
  slow-start-after-idle; head-of-line blocking) · **UDP** (why market data is
  UDP — stale data + retransmit-blocks-newer; loss/reorder handling; **A/B
  feeds**; kernel drops; `recvmmsg`) · **multicast** (`IP_ADD_MEMBERSHIP` =
  kernel filter + IGMP report; snooping failure modes; explicit `imr_ifindex`;
  SSM; **A/B arbitration** by sequence) · **sockets API** (`recv` 0 vs <0,
  framing = length prefix, `getaddrinfo` at startup, `SIGPIPE`/`MSG_NOSIGNAL`) ·
  **blocking vs non-blocking** (`EAGAIN`, busy-poll, `SO_BUSY_POLL`, ET
  draining) · **select/poll** (O(N) + `FD_SETSIZE` → why epoll) · **epoll deep**
  (LT vs ET, event-loop skeleton, `EPOLLOUT` add/remove discipline,
  `EPOLLONESHOT`/`EXCLUSIVE`, timerfd/signalfd/eventfd in one loop) · **socket
  options** (the HFT short list; `SO_RCVBUF` cap + 2× readback; `SO_REUSEPORT`;
  keepalive + `TCP_USER_TIMEOUT`) · **zero-copy** (`sendfile` + `TCP_CORK`;
  `splice` for capture/replay; `MSG_ZEROCOPY` + ERRQUEUE; `io_uring` SQ/CQ +
  SQPOLL = zero syscalls) · **kernel bypass** (DPDK / Onload / ef_vi / VMA /
  **AF_XDP** — ~1–5 µs kernel RX path → ~100–300 ns; what you take on; the
  adoption ladder) · **timestamping** (`SO_TIMESTAMPING` HW/SW, clock domains,
  PTP + `ptp4l` + `phc2sys`, MiFID II) · **network tuning** (NIC rings,
  coalescing, GRO/LRO off, RSS/RPS, qdisc, sysctl) · **building servers** (the
  **V1 blocking echo → V2 epoll echo → V3 UDP-multicast feed handler**
  progression — each version removes the previous one's bottleneck). 10
  `*.linux.cpp` examples (all SKIP): `01`/`02` (TCP server + client RT
  histogram), `03_nagle_demo` (OFF ~40 ms vs ON ~µs), `04`/`05` (UDP
  send/receive + seq/loss/reorder + `recvmmsg`), `06_multicast_receiver`,
  `07_epoll_server` (full edge-triggered), `08_socket_options`,
  `09_timestamping`, `10_latency_measure` (blocking vs busy-poll). Same Rule 2
  treatment for the loopback numbers.

- **Mojibake sweep:** 186 (folder 29) + 30 (folder 30) stray Devanagari/Cyrillic
  tokens from Hinglish typing (verb suffixes like "karta"/"deta"/"dikhta" typed
  with Devanagari matras, and a few mixed Devanagari+Cyrillic clusters) → all
  fixed → 0 residual across both folders + all four audit files.

### Batch 8 part 3 (PHASE 17–18) — folders 27 (ATOMICS-MEMORY-MODEL), 28 (LOCK-FREE) — **BATCH 8 DONE**

- **27 ATOMICS-MEMORY-MODEL** ("course ka sabse mushkil folder"): the **formal
  data-race definition** (same location, ≥1 write, no happens-before, ≥1
  non-atomic, different threads ⇒ **whole-program UB** — not just a wrong number)
  · **atomicity** (indivisible; non-atomic RMW = load-modify-store; torn /
  misaligned; x86 aligned ≤8B is hw-atomic but still a data race — the compiler
  reorders/caches/coalesces) · `std::atomic<T>` (trivially-copyable, non-copyable,
  brace-init, `is_always_lock_free`, `atomic_flag`, C++20 wait/notify) · **atomic
  operations** (`load`/`store`/`exchange`/`fetch_*`; x86 `mov`/`xchg`/`lock
  xadd`/`lock cmpxchg`; `fetch_add` returns OLD; no `fetch_max`) · **CAS**
  (`weak` vs `strong`, spurious failure, `expected` overwrite, the CAS-loop
  pattern, success/failure orders, retry storms) · **why ordering exists**
  (compiler + CPU reorder; the store buffer) · `relaxed` (atomicity only — exact
  count, zero ordering) · **acquire/release** (synchronizes-with →
  happens-before; publish/subscribe; free on x86) · **seq_cst** (single total
  order all threads agree on; the default; the store-side `mfence`) ·
  **happens-before / synchronizes-with / sequenced-before** (the formal
  vocabulary + the edge list; non-edges) · **fences** (`atomic_thread_fence` vs
  compiler-only `atomic_signal_fence`; x86: only the `seq_cst` fence emits an
  instruction) · **x86-TSO vs ARM/POWER** (x86 allows only store→load; ARM allows
  all four; multi-copy atomicity → IRIW; per-op cost on ARM) · `std::atomic_ref`
  (C++20 — a temporary atomic view over a plain object) · **lock-free / wait-free
  / obstruction-free** (exact progress guarantees; "no OS lock can block another";
  lock-free ≠ fast) · **ABA** (value equality ≠ "unchanged"; the tagged-word fix)
  · **litmus tests** (SB / MP / LB / IRIW — the shapes real ordering bugs take,
  and which order kills each). Measured (8/8 examples OK, all strict-warnings
  clean): non-atomic counter **~3M / 16M WRONG** (`volatile` used *only* to defeat
  `-O2` coalescing so the race stays visible); **store buffering observed on
  x86** — `r1==r2==0` for `relaxed` ~70–330/200k, `release/acquire`
  **~1000–4200/200k** (rel/acq does *not* stop store→load — *more*, not fewer),
  `seq_cst` **always 0**; store `relaxed`/`release` ~0.72 ns vs **`seq_cst` ~13 ns
  (~18×)**, load ~0.74 ns and `fetch_add` ~13 ns **at every order**, contended
  `fetch_add` ~27 ns/op (absolute ns drift with machine state — the *ratios* are
  the lesson, per CLAUDE.md Rule 2); ABA reproduced deterministically and the
  `{idx:32,tag:32}` fix rejects the stale CAS; MP + IRIW: **0** bad reads / **0**
  disagreements at every order on x86 (expected — x86 forbids all litmus
  reorderings except SB and is multi-copy-atomic).
- **28 LOCK-FREE:** why lock-free (contended-mutex costs: futex syscall + 2
  context switches + preemption stall; **priority inversion / convoying /
  deadlock** all structurally impossible without a held lock) · **rules** (no
  locks/alloc/blocking syscalls on the shared path; pre-allocate pools/rings;
  deliberate memory order per access; ABA-proof; "helping") · **cache-line
  padding & false sharing** (measured ~3.5–4× on a pure index-pair case) ·
  **SPSC ring buffer** (release-store/acquire-load of two indices, no CAS,
  wait-free in practice, no ABA — monotonic counters) · **SPSC optimizations**
  (power-of-two mask, padding, **cached opposite index**, batching, zero-copy
  slot API) · **Vyukov bounded MPMC** (per-cell `seq` turnstile, monotonic
  positions, `relaxed` position CAS + `release`/`acquire` cell `seq`) ·
  **Michael-Scott queue** (dummy node, helping a lagging `tail`, the two hard
  problems: ABA + reclamation) · **Treiber stack** + ABA-in-practice (tagged
  head, 32-bit index + 32-bit tag — no DWCAS) · **the memory reclamation
  problem** (a raw internal-node pointer held past the unlink; fixed pool /
  refcount / hazard pointers / epochs / QSBR; why `atomic<shared_ptr>` is wrong
  hot) · **hazard pointers** (publish-then-revalidate; bounded garbage `T·R`) ·
  **epoch-based reclamation / RCU / QSBR** (near-free read side; the
  unbounded-memory failure mode under a stalled reader; copy-swap publication) ·
  **seqlock** (1-writer/N-reader consistent snapshot — odd/even `seq`, bracketing
  reads, the formally-racy payload → relaxed atomics / `atomic_ref`; *the* HFT
  market-data primitive) · **testing lock-free** (unit → invariant stress
  (oversubscribed, `-O2`+) → TSan → model checkers herd7/GenMC (the only thing
  that *proves*) → forced-interleaving regression tests; ABA is invisible to
  TSan) · **when NOT to go lock-free** (correctness risk, reclamation burden,
  retry storms, portability, composition; when a `std::mutex` or **sharding** is
  the right answer; the decision checklist). Measured (7/7 examples OK): SPSC ring
  naive **~19 M msg/s / ~52 ns/msg**; the optimization ladder — V0 naive ~25–27
  ns/msg → V1 **+padding ≈ noise / sometimes worse** (the producer still reloads
  `tail_` every push, so padding alone doesn't remove the cross-core read) → V2
  **+cached index ~16–20 ns/msg (~1.3–1.6×, the real win)** — a CLAUDE.md Rule 2
  result; Vyukov MPMC **~1.6× vs `mutex+deque`**; ⚠️ a correct, ABA-safe,
  contended lock-free **Treiber stack ~5× SLOWER than `mutex + std::vector`**
  (retry storm on one head; the mutex baseline is tiny and cache-friendly —
  **lock-free ≠ fast**, taught not hidden); **seqlock ~80–100× faster reads than
  `std::shared_mutex`** (torn=0); SPSC hand-off p50 ~0.4 µs / ~15 M msg/s vs
  `mutex+queue` ~1.2 µs and `mutex+cv` ~6 µs (futex wake) — here lock-free clearly
  wins; pure false sharing on an index pair **~3.5–4×** (adjacent ~24 ns/op vs
  `alignas(64)` ~6 ns/op).

Concurrency section **~45% → ~97%** (all 12 remaining rows ☐/~→✓: formal data
race, `std::atomic`, CAS, all 6 memory orders, happens-before, fences, hardware
memory models, lock-free structures, ABA, memory reclamation, seqlock, false
sharing). Standard library **~82% → ~89%** (`<atomic>` ✓, `<exception>` /
`<stdexcept>` / `<system_error>` ✓, duplicate `<latch>/<barrier>` row removed).
**Batch 8 is complete** (PHASE 13–18, folders 23–28). Every benchmark real and
reproducible via `./build.ps1 fast`; two anti-intuitive results (padding-alone,
lock-free-slower) taught explicitly per CLAUDE.md Rule 2.

Batch 9 progress (PHASE 18–23): **29 (Linux systems) + 30 (networking) + 31 (CPU
architecture) + 32 (cache & memory performance) + 33 (compiler optimization) +
34 (assembly) + 35 (profiling & benchmarking) DONE — BATCH 9 COMPLETE (folders
29–35).** Systems section (5) covered at the lesson level (examples Linux-only,
skipped on this box); folder 31 lifted "CPU architecture" + "branch-free
programming" to ✓; folder 32 lifted "Cache hierarchy", "Locality", "AoS vs SoA /
DOD", "TLB & huge pages" to ✓; folders 33–34 lifted "Compiler optimizations",
"Reading assembly", and "SIMD / auto-vectorization" to ✓; **folder 35 lifted
"Benchmarking methodology", "Percentiles / tail latency", and "Profiling with
perf/VTune" to ✓; **folder 36 (LOW-LATENCY-CPP, PHASE 24, Batch 10 part 1)
lifted the last four — "Allocation avoidance", "Memory / object pools", "Ring
buffers", "Batching & syscall avoidance" — completing the Performance section
(~53% → ~100%, overall ~89%).** Folder 36 is the synthesis of folders 12–35 for
hot-path engineering with every technique's trade-off made explicit. **Next:
the HFT domain track (folders 37–44) — no remaining C++/systems knowledge gaps.**

---

### Batch 8 part 2 (PHASE 15–16) — folders 25 (OBJECT-MODEL), 26 (CONCURRENCY)

- **25 OBJECT-MODEL:** what an object *is* per the standard (region of storage,
  type/lifetime/value, subobjects, `sizeof ≥ 1`) · **object lifetime**
  (ctor-complete → dtor-start; **storage duration vs lifetime**; storage reuse +
  `std::launder`; implicit-lifetime types; out-of-lifetime = UB) · four storage
  durations · **static init order fiasco** (reproduced — link order B prints
  `BADLOG[1/(null)]`, the `std::string` member's ctor hadn't run) +
  construct-on-first-use + `constinit` · **temporaries & lifetime extension** (the
  4 non-extension / dangling cases) · **trivial / trivially-copyable /
  standard-layout / POD / `has_unique_object_representations`** — which unlocks
  `memcpy` / `offsetof` / `memcmp` / `malloc`+use (measured `memcmp == -1` without
  `memset` first) · **object vs value representation**, padding, `-Wpadded` ·
  **alignment deep** (`alignas`, over-aligned types, aligned `new` vs `malloc`,
  `std::align`, `hardware_destructive_interference_size`) · **placement new** &
  manual lifetime (pools, `FixedOptional<T>`) · **strict aliasing** (the
  sanctioned glvalue types + `char`/`byte` exception; `bit_cast`/`memcpy` vs
  `reinterpret_cast`+deref/union — measured `aliasing_trap` `delta == 0` at
  `-O2 -fstrict-aliasing`) · **type punning** (every method + verdict) · **the
  four casts deep** (`dynamic_cast` internals: vtable → `type_info` →
  `__dynamic_cast` graph walk; hot-path alternatives) · **vtable layout exact**
  (slots, `offset-to-top`, `type_info`, MI → two vptr + thunks, virtual
  inheritance) · **ABI** (Itanium; the ABI-break catalog; `abidiff` /
  `static_assert`; stable-API design) · **the UB catalog** (50+ by category) ·
  **how the compiler exploits UB** (`overflow_check(INT_MAX)` = `1` at every `-O`,
  `0` only with `-fwrapv`; null-check removal; load reuse — real `-O2` examples).
- **26 CONCURRENCY:** concurrency vs parallelism (+ Amdahl) · process vs thread
  (shared vs per-thread, creation cost ~10×, context switch + TLB, `fork()` COW
  hazard, isolation → process-per-component + shm rings) · `std::thread`
  (join-or-`terminate`, move-only, decay-copied args + `std::ref`,
  `native_handle`) · **race conditions & data races** (race condition vs data
  race = UB; `++counter` interleaving — **measured ~70–75% lost updates, different
  every run**; cached-flag / torn-read / compiler-assumption consequences) ·
  critical sections (1 thread, keep tiny, no I/O/alloc under the lock) ·
  `std::mutex` (uncontended CAS ~15 ns vs contended futex + context switch; the
  family; `std::call_once`) · **lock guards** (`lock_guard` / `unique_lock` /
  `scoped_lock`, the unnamed-temporary-guard trap) · **deadlock** (Coffman's 4;
  consistent order / `scoped_lock` / hierarchy / `try_lock`+backoff-livelock;
  self-deadlock; detection) · `std::shared_mutex` (2–5× costlier acquire,
  reader-count cache bouncing → the **snapshot + atomic pointer swap** pattern is
  usually better) · **condition variables** (the predicate pattern, lost/spurious
  wakeup, `notify_one` vs `all` thundering herd, 2-CV producer-consumer) ·
  futures & promises (`async` policies + **blocking dtor**, `packaged_task`,
  exception via `get()`) · **build a thread pool** (measured serial ~271 / pool(8)
  ~48 ms, fib(30)×64) · **C++20 sync** (`jthread`+`stop_token`, `latch`,
  `barrier`+completion fn, `counting_semaphore`) · `thread_local` (TLS access
  cost — initial-exec `%fs:[K]` vs general-dynamic `__tls_get_addr`; hoist in hot
  loops; `Context&` vs TLS) · **false sharing** (**measured ~10×** — packed vs
  `alignas(64)` counters; MESI; SPSC head/tail) · concurrency bug catalog +
  detection (TSan / Helgrind / stress+asserts / `perf c2c`).

Concurrency section **0 → ~45%** (9 rows ☐→✓: process/thread, `std::thread`,
races, mutexes, deadlock, CVs, futures, thread pools, C++20 sync; false sharing
☐→~). Memory + object model **78 → ~97%** (object lifetime / representation /
placement new / strict aliasing / ABI / casts). Language fundamentals **91 →
~95%** (the four casts, trivial/std-layout, temporaries). Standard library **74 →
~82%** (`<thread>`/`<mutex>`/`<future>`+ the C++20 sync headers). Every benchmark
real and reproducible via `./build.ps1 fast`.

Aage (Batch 8 rest, PHASE 17–18): `<atomic>` + the C++ memory model (27),
lock-free data structures (28).

---

### Batch 8 part 1 (PHASE 13–14) — folders 23 (ERROR-HANDLING), 24 (COMPILATION-LINKING)

- **23 ERROR-HANDLING:** the whole landscape (return codes / `errno` / exceptions
  / `expected` / `error_code` / assertions — *the two questions that pick the
  mechanism*) · exceptions mechanics (catch by `const&` + order, `throw;` vs
  `throw e;`, `exception_ptr`) · **stack unwinding** (ctor-mid throw → only
  constructed members' dtors run, `~T` doesn't; `noexcept` boundary → `terminate`;
  RAII still works under `-fno-exceptions`) · **exception safety** (no-throw /
  strong / basic / none; commit-or-rollback + copy-and-swap; `noexcept` move ⇒
  `std::vector` realloc moves not copies) · `noexcept` deep (contract not hint;
  `move_if_noexcept`; conditional `noexcept(expr)`) · custom exception hierarchy
  (`std::runtime_error` base, `throw_with_nested`) · **measured exception cost**
  · **`-fno-exceptions`** (latency determinism / code size / optimization /
  auditability; the toolkit) · **`std::expected`** (monadic
  `and_then`/`transform`/`or_else`/`transform_error`, `E` choice) ·
  `std::error_code` / `error_condition` / custom `error_category` · assertions /
  `static_assert` / `std::unreachable` / `[[assume]]` / contracts (C++26) · **UB
  catalog** (UB vs unspecified vs impl-defined; how the optimizer exploits UB;
  by category; sanitizers). Measured: happy-path try/catch vs return-code
  **~1.0×** (zero-cost model — the `try` region is free when nothing throws);
  **~6000+ ns per throw+catch vs ~1.5 ns per return-code check → ~4000×**; at a
  0.1% error rate try/catch ~8 ns/iter vs ~1.5 ns → **~5×**. `05_expected` builds
  under **`-std=c++20`** (hand-rolled `Expected<T,E>`, identical API) **and
  `-std=c++23`** (real `std::expected`); `07_no_exceptions` compiles + runs
  **with and without `-fno-exceptions`**.
- **24 COMPILATION-LINKING:** translation units + the 4-phase model ·
  preprocessor deep (macros, `#`/`##`, `__VA_OPT__`, X-macros, predefined macros,
  `_Pragma`, the missing-parens / double-eval traps — measured) · include guards
  / `#pragma once` / IWYU / self-contained headers / pImpl · **ODR deep** (loud
  "multiple definition" vs **silent IFNDR** — `sizeof(Config)` 12 vs 16 links
  fine, caught only by `-flto -Wodr`; `_GLIBCXX_USE_CXX11_ABI` / `_GLIBCXX_DEBUG`
  drift) · linkage (internal / external / module, anon namespaces vs `static`,
  `const` default-internal, `-fvisibility=hidden` + explicit exports) · storage
  (`static` 3 meanings, `extern` decl/def, `inline` variables, `thread_local` +
  TLS cost, `constinit`, init-order fiasco + construct-on-first-use) · name
  mangling (Itanium grammar) / `extern "C"` / ABI / `c++filt` · object files &
  ELF (`.text`/`.rodata`/`.data`/`.bss`, `.symtab` codes, relocations link-time
  vs load-time) · **static vs dynamic linking** (`.a` member selection — `nm app`
  proves `calc::huge_unused` isn't pulled; `.so` PLT/GOT indirection cost,
  `-fPIC`, RUNPATH/`$ORIGIN`; **HFT static-links for no-PLT + LTO-reach +
  determinism**) · every common linker error + fix (incl. "function exists but
  undefined reference" = signature/ABI mismatch; "undefined reference to vtable")
  · Make (`-MMD -MP` auto-deps — `touch header` → all `.o` rebuild; order-only
  prereqs; `.PHONY`; OS-aware `.exe` suffix) · CMake 4.0.2 (targets +
  `PUBLIC`/`PRIVATE`/`INTERFACE` usage requirements — `-DENGINE_FAST_PATH=ON`
  flips a compile definition; `find_package` imported targets; generator
  expressions; `install`/`export`) · build performance (header hygiene as the
  biggest free lever, PCH, unity builds + their ODR/name-clash risk, ccache,
  ninja, `mold`/`lld`) · binary tools (`nm`/`objdump`/`readelf`/`ldd`/`strings`/
  `size`/`strip`/`objcopy`/`addr2line`/`bloaty` — task-oriented recipes) · **LTO
  and PGO** (LTO removes the TU optimization boundary — IR in the `.o`,
  whole-program at link; ThinLTO; costs; PGO 3-step flow, block-layout / inlining
  / function-ordering wins, representative training; **LTO + PGO + static +
  `-march=native` = the HFT release config**). Directory examples use `.cxx`/
  `.hpp` so the repo `*.cpp` checker skips them; each has its own `build.sh` /
  `build.ps1` and was verified manually.

Language fundamentals **87%→91%** (exceptions, exception safety, `noexcept`, UB
catalog, storage specifiers, linkage — 6 rows ☐/~→✓). Build + tooling **20%→63%**
(TU/ODR, linkage/mangling, static-vs-dynamic libs, Make, CMake, binary tools,
LTO/PGO — 7 rows ☐→✓). Standard library +2 (`<expected>` ✓, `<system_error>` ✓).
Every benchmark number real and reproducible via `./build.ps1 fast`.

Aage (Batch 8 rest, PHASE 17–18): atomics + memory model (27), lock-free (28).
[Done: object model (25), concurrency (26) — see the Batch 8 part 2 section above.]

---

### Batch 7 part 2 (PHASE 10–12) — folders 20 (ALGORITHMS-DSA), 21 (TEMPLATES), 22 (MODERN-CPP)

- **20 ALGORITHMS-DSA:** complexity + **its limits** (Big-O lies when the
  constant is a cache miss) · arrays (two-pointer / sliding window / prefix
  sums) · sorting family (implement + bench; a median-of-3 bug found + fixed) ·
  **`std::sort` internals = introsort** · binary search (half-open invariant,
  "search the answer") · linked lists (+ intrusive-arena pattern) · stacks /
  queues (+ ring buffer) · **write your own open-addressing hash table** ·
  trees/BST · heaps · tries · graphs (BFS/DFS/Dijkstra/topo) · greedy (+ how to
  prove/verify it) · DP · bitmask · string algos (KMP / rolling hash / Z) ·
  **cache-aware DSA** (the synthesis: layout beats Big-O). Measured: O(n²) sorts
  ~600–730 ms vs `std::sort` ~1 ms (n=20k); list sum 5M ~70 ms vs vector ~1.7 ms
  (**~40x**, same O(n)); open-addr hash build/lookup ~66/~45 ns vs
  `unordered_map` ~295/~83; fib(40) naive ~244 ms vs memo ~0.02 ms (**~11,500x**);
  flat tree traversal **4–11x** a pointer tree.
- **21 TEMPLATES:** why templates (zero-cost codegen) · function & class
  templates · deduction · non-type / template-template params · full + partial
  specialization · variadic + fold expressions · `if constexpr` · `<type_traits>`
  internals + writing your own · **SFINAE** · **concepts** · **CRTP** +
  policy-based design · tag dispatch · TMP (`constexpr` over recursive templates)
  · two-phase lookup / `typename`/`template` · instantiation model + code bloat +
  `extern template` · **templates in HFT** (compile-time dispatch vs `virtual`).
  Measured: CRTP ~0.56 ns/call vs virtual (real polymorphic boundary) ~2.43 ns
  (`sizeof` unchanged — no vptr); virtual ~2.51 / template ~1.14 /
  `std::variant`+`std::visit` ~1.13 ns per call; devirtualization caveat noted.
- **22 MODERN-CPP:** systematic standard-by-standard audit C++11→23 (every
  feature catalogued with "what / why / where detailed") + deep dives — lambdas
  (desugaring, all captures, cost) · `constexpr`/`consteval`/`constinit` (three
  guarantees) · ranges (concepts, view semantics, lazy-eval subtleties) ·
  coroutines (promise hooks, the frame, HALO, per-element cost) · **modules**
  (`#include` vs `import`, BMI, working multi-file demo) · `<=>` (categories,
  rewrite rules) · attributes (diagnostic vs codegen) · `std::format` (custom
  formatters, `format_to_n`) · legacy→modern migration guide. Measured:
  `sizeof` closure 16 / empty 1 / `std::function` 32; **ranges pipeline ~6.5 ms
  vs hand loop ~10 ms — the pipeline is ~1.5x FASTER** (conditional-accumulate
  hand loop doesn't vectorize; pipeline → branchless masked SIMD — taught, not
  hidden, per CLAUDE.md Rule 2).

Language fundamentals **69%→87%** (16 rows ☐→✓ — all of templates, lambdas,
`constexpr` family, structured bindings, `<=>`, attributes, modules, coroutines),
standard library 68%→73% (`<type_traits>` internals, `<concepts>`). Every
benchmark number real and reproducible via `./build.ps1 fast`.

Aage (Batch 7 last folder, PHASE 13): error handling — exceptions, unwinding,
exception-safety guarantees, `noexcept`, `<system_error>`, `std::expected`,
`-fno-exceptions` trade-off (23).

---

### Batch 7 part 1 (PHASE 7–9) — folders 17 (RAII), 18 (COPY-MOVE), 19 (STL)

- **17 RAII:** the resource concept · the RAII idiom (acquire-in-ctor /
  release-in-dtor) · why it works (stack unwinding + the 5 dtor-skip gaps:
  `abort`/`exit`/`longjmp`/uncaught-at-`main`/never-deleted) · `unique_ptr`
  (zero overhead, move-only, `sizeof`==8 via EBO) · `shared_ptr` (control block,
  **atomic** refcount, `make_shared` 1 alloc vs `shared_ptr(new)` 2) · `weak_ptr`
  (breaks cycles) · custom deleters (stateless struct→EBO→8, fn-ptr→16;
  `shared_ptr` deleter type-erased→always 16) · ownership-semantics spectrum +
  sink idiom · RAII for fd/lock/FILE · Rule of Zero · smart-pointer perf.
  Measured: `shared_ptr` copy+destroy ~33.6 ns ≈ **93x** a raw-pointer copy;
  `unique_ptr` create/destroy == raw.
- **18 COPY-MOVE:** copy ctor / copy assignment (self-guard, allocate-before-free,
  copy-and-swap) · Rule of Three · **value categories** (lvalue/prvalue/xvalue,
  2-question model, `decltype((x))`) · rvalue refs + forwarding refs + reference
  collapsing · move ctor / move assign (steal+null, "valid but unspecified") ·
  `std::move` = `static_cast<T&&>` (const→copy, `return std::move` pessimization,
  `-Wpessimizing-move`) · Rule of Five · Rule of Zero · **copy elision** (RVO
  guaranteed C++17, NRVO best-effort) · **perfect forwarding** (`std::forward`,
  `make<T>` factory) · **`noexcept` move** (`move_if_noexcept`, `std::vector`
  growth). Measured: `noexcept` move → vector MOVES ~152 ms vs non-`noexcept` →
  COPIES ~468 ms (**~3x**); copy 1M strings ~177 ms vs move the vector ~0.0001 ms.
- **19 STL:** 26 lessons — STL architecture (M+N, half-open ranges, iterator
  categories) · every container (`vector`/`array`/`span`/`deque`/`list`/
  `forward_list`/`map`/`set`/unordered/adapters) with layout + complexity +
  **measured** cache cost · iterators + full invalidation table · `<algorithm>`
  ×4 (non-modifying / modifying + **erase-remove idiom** / sorting family:
  introsort, `nth_element`, `partial_sort`, `partition` / binary-search + set +
  heap + permutations) · `<numeric>` (`accumulate` init-type trap vs `reduce`
  FP-reorder, scans) · `<ranges>` (lazy views, `|`, projections) · utility types
  (`optional`/`variant`+`visit`/`any`/`expected`) · `<functional>` (**`std::
  function` cost measured**) · `<chrono>` (correct microbenchmarking) ·
  `<random>` · `<filesystem>` · `<regex>` (**why slow**) · `<type_traits>` ·
  `<bit>` · allocators · **PMR** · container performance table · **STL in HFT**
  (the synthesis: 3 tiers, what's kept / replaced / "nothing"). Measured:
  `map` 1049 / sorted-vec 357 / `unordered_map` 113 ns/lookup (N=200k);
  `std::function` templated ~1.48 ns vs erased ~5.11 ns (+1 heap alloc for a
  `std::string` closure); iterate 1M: `vector` 0.67 / `list` 18.0 / `set` 197 ms;
  stack-buffer `pmr::vector` → **0** global `new`.

Language fundamentals ~69%, standard library **16%→68%** (the big jump — 17
stdlib rows ☐→✓), memory + object model ~72%. Every benchmark number real and
reproducible via `./build.ps1 fast`.

(Part 2 continued this jump — see the Batch 7 part 2 section above: language
fundamentals **69%→87%**.)

---

Phase 0–5 poore ho chuke hain (folders 00–14). **Batch 6 (PHASE 6) COMPLETE** —
folders 15 (CLASSES), 16 (OOP):
- 15: encapsulation + invariants, members/`this`, access specifiers, constructors
  (default/param/delegating/`= default`/`= delete`), **member init lists** (+
  declaration-order trap), destructors (+ Rule of 3/5 preview), `const` methods +
  `mutable`, static members (`inline static`), operator overloading
  (`+=`/`+`/`<<`/`<=>`), `friend` (hidden-friend idiom), `explicit`,
  nested/local classes, class layout (EBO, `is_standard_layout`, no-vptr).
  Encapsulation = **zero runtime cost** throughout.
- 16: inheritance (access modes, layout, name hiding), ctor/dtor order (+
  virtual-call-in-ctor trap), virtual functions, **vtable/vptr deep dive**,
  `override`/`final`, abstract classes/interfaces, **virtual destructors**
  (measured leak demo), object slicing, multiple inheritance, virtual
  inheritance/diamond, RTTI/`dynamic_cast`, **virtual dispatch cost (measured)**,
  **CRTP** (zero-cost static poly), composition vs inheritance, SOLID (+ HFT
  reconciliation). Measured: virtual ~23 ns vs direct/CRTP ~2.2 ns vs
  `variant`+`visit` ~16 ns per call (`07_dispatch_benchmark.cpp`).

Language fundamentals section ab ~61% — is batch ka bada jump (13 rows ☐→✓). Har
number real, measured. Har topic deeply covered: explanation + multiple examples
+ traps + exercises + interview questions.

Jab bhi naya batch aaye, yeh file update hogi.

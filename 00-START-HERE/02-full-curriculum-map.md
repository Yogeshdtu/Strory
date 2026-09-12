# Poora Curriculum Map

Yeh poore course ka syllabus hai — folder by folder, lesson by lesson.
Isse aapko pata rahega ki aap kahan ho aur aage kya aa raha hai.

Legend: ✅ = likha ja chuka · ⏳ = planned (batch pending)

---

## PHASE 0 — Computer + Programming Basics

### `01-PROGRAMMING-BASICS/` ✅
Computer kya hai · program kya hai · programming language · source code ·
compiler vs interpreter · C++ ka itihaas aur kyun · editor/IDE · terminal ·
file extension · program vs executable · "Run dabane pe kya hota hai" ·
bits/bytes/binary · RAM vs disk vs CPU · exercises

---

## PHASE 1 — Absolute C++ Beginner

### `02-CPP-FIRST-STEPS/` ✅
Hello World ka har character · `#include` · headers · `main()` · statements aur `;` ·
`{}` blocks · comments · string literals aur escape sequences · **compilation pipeline**
(preprocessor → compiler → assembler → linker → loader → CPU) · pehle compile errors ·
exercises

---

## PHASE 2 — Core C++ Fundamentals

### `03-VARIABLES-DATA-TYPES/` ✅
Data kya hai · variable kya hai · declaration vs definition vs initialization ·
assignment `=` · `int` deep dive (signed/unsigned, size, range, overflow) ·
floating point (`float`/`double`, IEEE-754, precision traps) · `char` aur ASCII ·
`bool` · fixed-width types (`<cstdint>`) · `auto` · `const`/`constexpr` intro ·
initialization forms (copy/direct/brace, narrowing) · type conversions aur promotions ·
`sizeof`/`<limits>` · scope aur lifetime intro · naming conventions · exercises

### `04-INPUT-OUTPUT/` ⏳
`std::cout` deep · `std::cin` · `std::cerr`/`clog` · buffering aur flushing ·
`std::endl` vs `\n` (aur kyun `endl` slow hai) · `getline` · input validation ·
stream states (`fail`, `eof`, `bad`) · manipulators (`setw`, `setprecision`, `hex`) ·
formatted output · `std::format` (C++20) · file streams intro · `printf` family ·
I/O performance (`sync_with_stdio`) · exercises

### `05-OPERATORS/` ⏳
Arithmetic · integer division aur modulo traps · increment/decrement (pre vs post) ·
comparison · logical aur short-circuit · **bitwise operators deep dive** (AND/OR/XOR/NOT/shifts) ·
compound assignment · ternary · comma operator · `sizeof` · precedence aur associativity ·
**evaluation order aur sequence points** · operator table cheatsheet · exercises

---

## PHASE 3 — Control Flow + Functions

### `06-CONDITIONS/` ⏳
`if` · `else` · `else if` · nested conditions · `switch` aur fallthrough ·
`[[fallthrough]]` · `if` with initializer (C++17) · `if constexpr` (preview) ·
truthiness aur implicit conversion to bool · common condition bugs (`=` vs `==`) ·
branchless thinking (preview of folder 36) · exercises

### `07-LOOPS/` ⏳
`while` · `do-while` · `for` · range-based for · nested loops · `break`/`continue` ·
infinite loops · loop invariants · off-by-one errors · iteration vs recursion preview ·
loop performance basics (preview of folder 32) · exercises + patterns

### `08-FUNCTIONS/` ⏳
Function kya hai aur kyun · declaration vs definition · parameters vs arguments ·
return values · pass by value · scope aur lifetime of locals · **call stack deep dive** ·
stack frames · default arguments · function overloading aur overload resolution ·
recursion · `inline` · `constexpr` functions · `static` functions · header/source split ·
`[[nodiscard]]`, `noexcept` intro · exercises

---

## PHASE 4 — Arrays + Strings + Structs

### `09-ARRAYS/` ⏳
C-style arrays · memory layout · indexing aur zero-based logic · out-of-bounds = UB ·
array decay to pointer · `std::array` · multidimensional arrays · row-major order ·
arrays as function parameters · `std::span` (C++20) · VLA kyun nahi hain C++ mein ·
cache implications preview · exercises

### `10-STRINGS/` ⏳
C-strings aur null terminator · `<cstring>` functions · `std::string` deep dive ·
SSO (Small String Optimization) — **HFT relevant** · capacity vs size · `std::string_view` ·
string ke saath common bugs · conversions (`stoi`, `to_string`, `from_chars`/`to_chars`) ·
string performance · exercises

### `11-STRUCTS/` ⏳
`struct` kya hai · members · dot operator · initialization aur aggregate init ·
designated initializers (C++20) · nested structs · arrays of structs vs struct of arrays
(**AoS vs SoA — HFT critical**) · **padding aur alignment** · `alignas`/`alignof` ·
`#pragma pack` · `union` · `enum` aur `enum class` · bitfields · exercises

---

## PHASE 5 — Pointers + References + Memory

### `12-POINTERS/` ⏳
Memory addresses · pointer kya hai · `&` address-of · `*` dereference ·
pointer declaration syntax · `nullptr` (aur `NULL`/`0` kyun nahi) · pointer arithmetic ·
pointers aur arrays · `const` pointers vs pointer-to-const · double pointers ·
void pointers · function pointers · dangling pointers · pointer bugs catalog ·
exercises + memory diagrams

### `13-REFERENCES/` ⏳
Reference kya hai · lvalue references · reference vs pointer · pass by reference ·
`const` references · return by reference (aur uske dangers) · reference collapsing preview ·
rvalue references intro · dangling references · reference members · exercises

### `14-MEMORY/` ⏳
Process memory layout (text/data/bss/heap/stack) · stack deep dive · heap deep dive ·
static storage duration · thread-local storage · `new`/`delete` · `new[]`/`delete[]` ·
memory leaks · double free · use-after-free · fragmentation · allocator basics ·
stack overflow · **allocation cost — HFT critical** · Valgrind/ASan intro · exercises

---

## PHASE 6 — Classes + OOP

### `15-CLASSES/` ⏳
Class kya hai · `struct` vs `class` · members aur methods · access specifiers ·
constructors (default, parameterized, delegating) · member initializer lists (aur kyun matter
karti hain) · destructor · `this` pointer · `const` member functions · `static` members ·
friend functions · operator overloading basics · nested classes · exercises

### `16-OOP/` ⏳
Encapsulation · abstraction · inheritance (public/protected/private) · `virtual` functions ·
**vtable aur vptr deep dive** · polymorphism · `override`/`final` · abstract classes ·
pure virtual · virtual destructors (aur kyun mandatory hain) · multiple inheritance ·
diamond problem aur virtual inheritance · slicing · **virtual dispatch cost — HFT relevant** ·
CRTP (static polymorphism) · composition vs inheritance · SOLID principles · exercises

---

## PHASE 7 — RAII + Resource Management

### `17-RAII/` ⏳
Resource kya hai · RAII idiom · constructor acquires, destructor releases ·
scope-based cleanup · exception safety aur RAII · `std::unique_ptr` · `std::shared_ptr` ·
`std::weak_ptr` · `make_unique`/`make_shared` · custom deleters · ownership semantics ·
`std::lock_guard` preview · file handle RAII · **RAII in HFT (deterministic destruction)** ·
Rule of Zero · exercises

### `18-COPY-MOVE/` ⏳
Copy constructor · copy assignment · shallow vs deep copy · Rule of 3 ·
**value categories deep dive** (lvalue, prvalue, xvalue, glvalue, rvalue) ·
rvalue references · move constructor · move assignment · `std::move` (aur woh kya
NAHI karta) · Rule of 5 · Rule of 0 · copy elision · RVO/NRVO · guaranteed copy elision
(C++17) · perfect forwarding aur `std::forward` · universal/forwarding references ·
`noexcept` move aur `vector` growth · exercises

---

## PHASE 8 — STL + DSA + Templates

### `19-STL/` ⏳
STL architecture (containers + iterators + algorithms) ·
**Sequence:** `vector` (deep — growth, reserve, invalidation), `deque`, `list`,
`forward_list`, `array` ·
**Associative:** `map`, `set`, `multimap`, `multiset` (red-black trees) ·
**Unordered:** `unordered_map`/`set` (hashing, buckets, load factor, custom hash) ·
**Adapters:** `stack`, `queue`, `priority_queue` ·
**Iterators:** categories, invalidation rules, `<iterator>` utilities ·
**Algorithms:** poora `<algorithm>` audit · `<numeric>` ·
**Utilities:** `pair`, `tuple`, `optional`, `variant`, `any`, `expected` (C++23) ·
`<functional>` · `<chrono>` · `<random>` · `<filesystem>` · `<regex>` · `<bit>` ·
`<type_traits>` · `<ranges>` · `<memory_resource>` (PMR) · allocators ·
container performance table · **HFT: kaunsa container kab, aur kyun aksar koi nahi** · exercises

### `20-ALGORITHMS-DSA/` ⏳
Complexity aur Big-O · arrays/two-pointer/sliding window · prefix sums ·
sorting (all major algorithms + `std::sort` internals: introsort) · binary search ·
linked lists · stacks/queues · hash tables (implement your own) · trees · BST ·
heaps · tries · graphs (BFS/DFS/Dijkstra/topological) · greedy · DP · bit manipulation ·
string algorithms (KMP, rolling hash) · **cache-aware DSA (HFT flavour)** · problem sets

### `21-TEMPLATES/` ⏳
Function templates · class templates · template argument deduction · CTAD ·
explicit specialization · partial specialization · non-type template parameters ·
variadic templates · fold expressions · `if constexpr` · SFINAE · `enable_if` ·
type traits deep dive · **concepts (C++20)** · requires clauses · CRTP · tag dispatch ·
template metaprogramming · compile-time computation · two-phase lookup ·
template instantiation aur code bloat · **templates in HFT (zero-cost abstraction)** · exercises

---

## PHASE 9 — Modern C++ + Errors + Build

### `22-MODERN-CPP/` ⏳
C++11/14/17/20/23 feature-by-feature audit · `auto` deep · lambdas (captures, generic,
`mutable`, init-capture) · structured bindings · `constexpr`/`consteval`/`constinit` ·
`std::optional`/`variant`/`expected` idioms · `[[attributes]]` · `<ranges>` deep ·
**coroutines deep dive** · **modules deep dive** · three-way comparison `<=>` ·
designated initializers · `std::span` · `std::format` · `std::print` (C++23) ·
deducing `this` (C++23) · migration guide (purana code → modern)

### `23-ERROR-HANDLING/` ⏳
Error handling ke saare tarike · return codes · `errno` · exceptions (throw/catch/
stack unwinding) · exception safety guarantees (basic/strong/nothrow) · `noexcept` ·
custom exception types · **exception cost aur zero-cost model** ·
**HFT: exceptions kyun aksar disabled hote hain** (`-fno-exceptions`) ·
`std::expected` · `std::error_code` · assertions · `static_assert` · contracts ·
undefined behaviour catalog · defensive programming

### `24-COMPILATION-LINKING/` ⏳
Translation units · preprocessor deep · include guards vs `#pragma once` ·
**ODR (One Definition Rule)** · declaration vs definition · internal/external linkage ·
`static`, `extern`, `inline` variables · name mangling · object files (ELF format) ·
symbol tables · static vs dynamic linking · `.a` vs `.so` · linker errors decode karna ·
compile time optimization · precompiled headers · **Make** · **CMake** (proper deep dive) ·
Ninja · Bazel intro · cross-compilation · `nm`, `objdump`, `readelf`, `ldd` ·
LTO · unity builds

### `25-OBJECT-MODEL/` ⏳
Object kya hai (standard ke hisaab se) · object lifetime rules · storage duration ·
initialization order · static initialization order fiasco · temporary objects ·
lifetime extension · **trivial / standard-layout / POD types** · object representation ·
alignment aur padding deep · `std::launder` · placement new · **strict aliasing rule** ·
type punning aur `std::bit_cast` · `memcpy` idiom · ABI · vtable layout ·
`dynamic_cast` aur RTTI · `static_cast`/`const_cast`/`reinterpret_cast` ·
**undefined behaviour ka poora catalog** · compiler assumptions

---

## PHASE 10 — Concurrency

### `26-CONCURRENCY/` ⏳
Concurrency vs parallelism · process vs thread · `std::thread` · join/detach ·
passing data to threads · race conditions · critical sections · `std::mutex` ·
`lock_guard`/`unique_lock`/`scoped_lock` · deadlock (aur 4 conditions) ·
`shared_mutex` · `condition_variable` · `std::future`/`promise`/`async`/`packaged_task` ·
thread pools (build one) · `jthread` aur `stop_token` (C++20) · `latch`/`barrier`/
`semaphore` (C++20) · thread-local storage · false sharing intro · exercises

### `27-ATOMICS-MEMORY-MODEL/` ⏳
Data race ki exact definition · `std::atomic` · atomic operations · CAS ·
`compare_exchange_weak` vs `strong` · **memory ordering deep dive**: `relaxed`,
`acquire`, `release`, `acq_rel`, `seq_cst` · happens-before · synchronizes-with ·
sequenced-before · memory barriers/fences · CPU reordering aur compiler reordering ·
x86 TSO vs ARM weak ordering · `atomic_ref` · lock-free vs wait-free vs obstruction-free ·
`is_lock_free` · ABA problem · exercises + litmus tests

### `28-LOCK-FREE/` ⏳
Lock-free programming ke rules · SPSC ring buffer (**build it, benchmark it**) ·
MPMC queue · Michael-Scott queue · hazard pointers · epoch-based reclamation ·
RCU basics · seqlock (**HFT mein bahut use hota hai**) · memory reclamation problem ·
cache-line padding · false sharing elimination · lock-free stack ·
`std::hardware_destructive_interference_size` · testing lock-free code · benchmarks

---

## PHASE 11 — Systems + Networking

### `29-LINUX-SYSTEMS/` ⏳
Linux architecture · kernel vs userspace · **syscalls (aur unki cost)** ·
`strace` · processes (`fork`, `exec`, `wait`) · signals · file descriptors ·
`open`/`read`/`write`/`close` · `/proc` aur `/sys` · **mmap** · shared memory ·
pipes aur FIFOs · Unix domain sockets · **CPU scheduling** (CFS, `SCHED_FIFO`,
`SCHED_RR`) · **CPU affinity aur isolation** (`taskset`, `isolcpus`, `nohz_full`) ·
priorities aur `nice` · huge pages · `mlock` aur page faults · cgroups ·
NUMA (`numactl`) · IRQ affinity · kernel bypass intro · timers aur clocks ·
`clock_gettime` aur TSC

### `30-NETWORKING/` ⏳
OSI/TCP-IP model · IP · **TCP deep** (handshake, congestion, Nagle, delayed ACK) ·
**UDP** · multicast (**market data ka backbone**) · sockets API ·
blocking vs non-blocking · `select`/`poll`/**`epoll` deep dive** · edge vs level triggered ·
socket options (`TCP_NODELAY`, `SO_REUSEPORT`, buffer sizes) · zero-copy
(`sendfile`, `splice`, `MSG_ZEROCOPY`) · **kernel bypass: DPDK, Solarflare/Onload,
ef_vi, RDMA** · NIC offloads · timestamping (SO_TIMESTAMPING, hardware timestamps) ·
build karo: echo server → epoll server → UDP multicast receiver

---

## PHASE 12 — CPU + Cache + Compiler + Profiling

### `31-CPU-ARCHITECTURE/` ⏳
CPU kaise kaam karta hai · instruction cycle · registers · ISA (x86-64 basics) ·
**pipelines** · superscalar · **out-of-order execution** · register renaming ·
**branch prediction** deep · **speculative execution** · µops aur µop cache ·
ports aur execution units · latency vs throughput of instructions · **SIMD**
(SSE/AVX/AVX-512) · intrinsics · hyperthreading (aur HFT mein kyun band karte hain) ·
frequency scaling, turbo, C-states/P-states · **NUMA architecture** · Intel/AMD differences

### `32-CACHE-MEMORY-PERFORMANCE/` ⏳
Memory hierarchy aur latency numbers · **cache lines (64 bytes)** · L1/L2/L3 ·
associativity · cache misses (compulsory/capacity/conflict) · **spatial aur temporal
locality** · prefetching (hardware + software) · **false sharing** deep ·
cache-friendly data structures · **AoS vs SoA** · **data-oriented design** ·
TLB aur huge pages · store buffers aur write combining · memory bandwidth ·
`perf stat` se cache misses measure karna · cache-line alignment · benchmarks

### `33-COMPILER-OPTIMIZATION/` ⏳
`-O0`/`-O1`/`-O2`/`-O3`/`-Os`/`-Ofast` · inlining aur inlining heuristics ·
loop unrolling · vectorization (auto + manual) · constant folding · dead code elimination ·
tail call optimization · devirtualization · **`__builtin_expect` / `[[likely]]`/`[[unlikely]]`** ·
`restrict` aur aliasing · PGO (Profile-Guided Optimization) · LTO · `-march=native` ·
compiler barriers · **Compiler Explorer (godbolt) workflow** · optimization ko rokne wali
cheezein · `-ffast-math` ke khatre

### `34-ASSEMBLY/` ⏳
Assembly kyun padhein · x86-64 registers · AT&T vs Intel syntax · basic instructions ·
addressing modes · stack frames aur calling conventions (System V ABI) ·
function prologue/epilogue · reading compiler output · common patterns pehchanna ·
inline assembly · SIMD assembly · `rdtsc` aur cycle counting · disassembling with `objdump` ·
`perf annotate` · exercises: "yeh assembly kis C++ se aayi?"

### `35-PROFILING-BENCHMARKING/` ⏳
Measure kyun karein (aur intuition kyun galat hoti hai) · **timing correctly** ·
`std::chrono` · `rdtsc`/`rdtscp` · clock sources · **statistics: mean vs median vs
percentiles** · **p50/p90/p99/p99.9/p99.99** · **jitter aur tail latency** ·
histograms (HdrHistogram) · **Google Benchmark** · micro vs macro benchmarks ·
benchmarking pitfalls (dead code elimination, warm-up, alignment noise) ·
`perf record`/`report`/`stat`/`annotate` · flame graphs · VTune · Cachegrind ·
Valgrind/Callgrind · sanitizers (ASan/UBSan/TSan/MSan) · **latency measurement in
production** · exercises

---

## PHASE 13 — Ultra-Low-Latency C++

### `36-LOW-LATENCY-CPP/` ⏳
Latency vs throughput vs jitter · tail latency kyun sabse important hai ·
**allocation avoidance** · preallocation · **memory pools aur object pools (build them)** ·
arena/bump allocators · custom allocators · `pmr` ·
**cache warming** · **branch-free programming** · lookup tables ·
**virtual dispatch elimination** (CRTP, `std::variant`, function tables) ·
`std::function` ki cost aur alternatives · small buffer optimization ·
**ring buffers** · batching aur amortization · **syscall avoidance** ·
busy-polling vs blocking · **page faults aur `mlockall`** · huge pages ·
**CPU pinning aur isolation** · NUMA-aware allocation · warm-up strategies ·
compile-time dispatch · zero-copy patterns · **trade-offs ka honest discussion**

---

## PHASE 14 — HFT TRACK

### `37-HFT-FUNDAMENTALS/` ⏳
HFT kya hai (aur kya nahi hai) · exchanges kaise kaam karte hain ·
**market microstructure** · orders (market/limit/IOC/FOK/stop) · bid/ask/spread ·
depth aur liquidity · **price-time priority** · tick size · lot size ·
market makers vs takers · maker-taker fees · latency arbitrage · **co-location** ·
market data feeds · direct feeds vs consolidated (SIP) · exchange protocols overview ·
**HFT system architecture** (feed handler → book builder → strategy → risk → OMS → gateway) ·
regulatory basics · Indian markets (NSE/BSE) vs US markets

### `38-MARKET-DATA/` ⏳
Market data kya hai · L1/L2/L3 data · **incremental updates vs snapshots** ·
sequence numbers aur gap detection · **ITCH protocol deep dive** · FIX/FAST ·
SBE (Simple Binary Encoding) · binary parsing (**zero-copy parsing**) ·
endianness · message framing · **A/B feed arbitration** · recovery aur retransmission ·
timestamps aur clock sync (PTP/NTP) · conflation · **BUILD: market data simulator +
ITCH-style parser + feed handler**

### `39-ORDER-BOOK/` ⏳
Order book kya hai · data structure design ka trade-off space ·
naive `std::map` version → sorted vector → **array-of-price-levels (flat book)** ·
intrusive linked lists for orders · order ID → order lookup (hash vs slab) ·
**BUILD: full limit order book** (add / cancel / modify / execute) ·
price-time priority · top-of-book fast path · book snapshots · **benchmark:
naive vs optimized, before/after numbers** · cache-line analysis

### `40-MATCHING-ENGINE/` ⏳
Matching engine kya karta hai · order types handling · **BUILD: matching engine**
(limit orders, market orders, partial fills, IOC/FOK) · trade events ·
self-trade prevention · sequencing aur determinism · **event sourcing aur replay** ·
single-threaded design kyun · state machine design · testing a matching engine ·
fuzzing · benchmark suite

### `41-HFT-CONCURRENCY/` ⏳
HFT threading model · **single-writer principle** · shared-nothing design ·
**SPSC queue between threads (build + benchmark)** · **LMAX Disruptor pattern** ·
seqlock for market data snapshots · **busy-spin vs condition variables** ·
core pinning strategy · thread-to-core mapping · lock-free logging ·
**wait-free reads** · time-sensitive design · avoiding priority inversion ·
NUMA-aware thread placement

### `42-HFT-NETWORKING/` ⏳
Multicast market data receive path · **kernel bypass deep** (Onload, ef_vi, DPDK, VMA) ·
busy-poll sockets · `SO_BUSY_POLL` · **hardware timestamping** ·
NIC tuning aur IRQ affinity · TCP tuning for order gateways · `TCP_NODELAY` ·
switch latency · FPGA offload intro · **wire-to-wire latency measurement** ·
packet capture aur analysis · **BUILD: low-latency UDP multicast receiver
(kernel + bypass-ready abstraction)**

### `43-HFT-OPTIMIZATION/` ⏳
End-to-end optimization methodology · **measure → profile → hypothesize → change →
re-measure** · hot path vs cold path separation · `__attribute__((hot/cold))` ·
instruction cache locality · code layout aur BOLT · avoiding false sharing in the
hot path · struct layout tuning · integer vs float in the hot path ·
fixed-point arithmetic for prices · **avoiding division** · lookup tables ·
compile-time strategy dispatch · **case studies: har HFT project ko optimize karna
with real numbers**

### `44-HFT-PROJECTS/` ⏳
**Capstone projects:**
1. Market Data Simulator (messages, timestamps, sequence numbers, snapshots)
2. Feed Handler + Binary Parser
3. Full Limit Order Book
4. Matching Engine
5. Memory Pool + Object Pool (with allocation benchmarks)
6. SPSC Ring Buffer
7. Strategy Simulator + Backtester
8. Risk Engine
9. Order Manager + Execution Simulator
10. **MINI HFT ENGINE** — sab kuch jodkar:
    `Market Data → Parser → Order Book → Strategy → Risk → OMS → Execution`
    Har project: simple version → measure → profile → optimize → re-benchmark

---

## PHASE 15 — Polish + Interview + Reference

### `45-DEBUGGING/` ⏳
GDB deep dive · breakpoints/watchpoints/conditional breakpoints · backtraces ·
core dumps · reverse debugging (rr) · LLDB · sanitizers in practice ·
Valgrind suite · debugging multithreaded code · debugging optimized builds ·
debugging crashes in production · logging strategy · `perf` for debugging ·
common bug patterns catalog

### `46-INTERVIEW-PREP/` ⏳
Layered question banks: beginner C++ → pointers/refs/memory → OOP → STL →
templates → move semantics → concurrency → atomics/memory model → Linux →
networking → CPU/cache → performance → HFT architecture · system design rounds ·
brain teasers aur probability (Optiver/Jane Street style) · behavioural ·
"trick questions" catalog · mock interview scripts · resume tips for HFT

### `47-CODING-PROBLEMS/` ⏳
Graded problem sets: basics → arrays/strings → pointers/memory → OOP design →
STL usage → templates → concurrency → lock-free → performance ("optimize this") →
HFT-specific (order book ops, parsing, latency) · solutions with explanations

### `48-CHEATSHEETS/` ⏳
Syntax cheatsheet · STL container/algorithm cheatsheet · complexity tables ·
compiler flags · GDB commands · perf commands · Linux tuning checklist ·
memory ordering cheatsheet · **latency numbers table** · UB catalog ·
HFT glossary · pre-interview 1-page revision sheets

### `49-PROJECTS/` ⏳
Full end-to-end projects, difficulty order mein:
**Beginner:** calculator · number guessing · student manager · expense tracker ·
file reader · text parser
**Intermediate:** inventory system · bank simulation · CSV parser · log analyzer ·
mini key-value database
**Advanced:** custom `vector` · memory allocator · thread pool · concurrent queue ·
epoll network server · JSON parser
**HFT:** (folder 44 se link)

---

## Aap kahan ho?

`00-START-HERE/BUILD-STATUS.md` mein progress tracker hai — usme apna progress mark karte raho.

# Examples — Folder 14 (Memory Management)

| File | Lesson | Kya dikhata hai |
|---|---|---|
| `01_memory_layout.cpp` | 01 | `.text` / `.rodata` / `.data` / `.bss` / heap / stack ke addresses — segment order |
| `02_stack_vs_heap.cpp` | 02, 03, 08 | **Benchmark** — stack "alloc" vs `new[]`+`delete[]`: ~0.85 ns vs ~71 ns → **~83x** |
| `03_new_delete.cpp` | 04 | `new`/`delete`, `new[]`/`delete[]`, ctor+dtor timing, `nothrow`, mixing = UB (comment) |
| `04_memory_leak.cpp` | 05 | ⚠️ **Deliberate leaks** — counted `operator new`/`delete` override → "1006 blocks never freed" at exit |
| `05_use_after_free.cpp` | 06 | ⚠️ **Deliberate UAF + double-free** — stale read (`1234` → garbage), block-reuse, double-free (commented) |
| `06_allocation_benchmark.cpp` | 08 | **Latency distribution** p50/p90/p99/p99.9/max via `rdtsc` — the tail problem |
| `07_simple_pool.cpp` | 10 | Fixed-size free-list pool, O(1) alloc/free, placement `new` — vs `new`/`delete` **~192x** throughput |

## Compile / run

```bash
./build.ps1 14-MEMORY/examples/03_new_delete.cpp          # Windows (debug)
make FILE=14-MEMORY/examples/03_new_delete.cpp            # Linux/Mac/Git-Bash
```

Benchmarks ko **`-O2`** pe:

```bash
./build.ps1 fast 14-MEMORY/examples/02_stack_vs_heap.cpp
./build.ps1 fast 14-MEMORY/examples/06_allocation_benchmark.cpp
./build.ps1 fast 14-MEMORY/examples/07_simple_pool.cpp
```

## Measured (GCC 15.1, `-O2`, x86-64, Windows/MinGW-w64)

### `02_stack_vs_heap.cpp`
```
block = 256 bytes, 2,000,000 iterations
stack  per iter : ~0.85 ns
heap   per iter : ~71   ns   (new[] + delete[])
ratio           : ~83x
```

### `06_allocation_benchmark.cpp` (rdtsc, ns) — ek sample run
```
                 mean    p50    p90    p99   p99.9        max
A) new+delete     ~50    ~45    ~55    ~60   ~120     ~45,000 - ~105,000   (run-variable)
B) new, retained  ~90    ~50    ~55   ~130   ~450    ~120,000 - ~1,200,000 (run-variable)
C) fixed buffer   ~10    ~10    ~10    ~40    ~40        (measuring noise only)
```
Median theek dikhta hai — **tail** (p99.9 / max) allocator ka asli kharcha
dikhata hai: free-list miss, heap lock, `mmap`/`VirtualAlloc` se nayi memory,
first-touch page faults. Yeh spike HFT hot path pe allowed nahi.

### `07_simple_pool.cpp`
```
pool  alloc+free : ~0.43 ns/op
new   alloc+free : ~83   ns/op
speedup          : ~190x
```
Numbers machine/allocator pe depend karte hain — magnitude reproduce hoga.

## Jaan-boojh kar warnings / UB

- **`04_memory_leak.cpp`** — koi compile warning nahi; **runtime pe** chup-chaap
  ~1 MB leak karta hai. Custom counted `operator new`/`new[]` se exit pe
  `LEAK: 1006 block(s) never freed` print hota hai. **`operator new[]` alag se
  override kiya hai** — MinGW libstdc++ pe woh default `operator new` ko route
  nahi karta, warna `new T[]` allocations count mein chhup jaate.
- **`05_use_after_free.cpp`** — runtime UB demo (folder 12/13 ke dangling
  examples jaisa), compile-failure nahi. BUG 2 (block reuse) allocator-dependent
  hai — kabhi same block, kabhi alag; output dono handle karta hai. BUG 3
  (double-free) comment-out — enable karke crash/abort dekho.

## Sanitizers / tools

⚠️ **MinGW-w64 (`C:\mingw64`) pe ASan / LSan / Valgrind nahi.** Asli leak/UAF
diagnosis **Linux / macOS / WSL** pe:

```bash
# leak (04) — LeakSanitizer exit pe report, ya Valgrind
g++ -std=c++20 -fsanitize=address -g 04_memory_leak.cpp -o leak && ./leak
valgrind --leak-check=full ./leak

# use-after-free / double-free (05) — exact line + "freed by" stack
g++ -std=c++20 -fsanitize=address -g 05_use_after_free.cpp -o uaf && ./uaf
```

`./build.ps1 san <file>` MinGW pe `-D_GLIBCXX_ASSERTIONS + -fstack-protector-all`
pe fall back karta hai (heap UAF/leak nahi pakadta — sirf STL bounds + canaries).

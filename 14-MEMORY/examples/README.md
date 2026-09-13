# Examples — Folder 14 (Memory Management)

| File | Lesson | Kya dikhata hai |
|---|---|---|
| `01_memory_layout.cpp` | 01 | `.text` / `.rodata` / `.data` / `.bss` / heap / stack ke addresses — segment order |
| `02_stack_vs_heap.cpp` | 02, 03, 08 | **Benchmark** — stack local vs `new[]`+`delete[]` har iteration: <1 ns vs ~37 ns |
| `03_new_delete.cpp` | 04 | `new`/`delete`, `new[]`/`delete[]`, ctor+dtor timing, `nothrow`, mixing = UB (comment) |
| `04_memory_leak.cpp` | 05 | ⚠️ **Jaan-boojh kar leaks** — counted `operator new`/`delete` override → exit pe "1006 blocks never freed" |
| `05_use_after_free.cpp` | 06 | ⚠️ **Jaan-boojh kar UAF + double-free** — stale read (`1234` → garbage), block-reuse, double-free (commented) |
| `06_allocation_benchmark.cpp` | 08 | **Latency distribution** p50/p90/p99/p99.9/max `rdtsc` se — tail ki problem |
| `07_simple_pool.cpp` | 10 | Fixed-size free-list pool, O(1) alloc/free, placement `new` — 64-order burst pe `new`/`delete` se **~21x** |

## Compile / run

```bash
./build.ps1 14-MEMORY/examples/03_new_delete.cpp          # Windows (debug)
make FILE=14-MEMORY/examples/03_new_delete.cpp            # Linux/Mac/Git-Bash
```

Benchmarks **`-O2`** pe:

```bash
./build.ps1 fast 14-MEMORY/examples/02_stack_vs_heap.cpp
./build.ps1 fast 14-MEMORY/examples/06_allocation_benchmark.cpp
./build.ps1 fast 14-MEMORY/examples/07_simple_pool.cpp
```

## Naapa hua (GCC 16.2, `-O2`, Windows x64 / MinGW-w64 UCRT, AMD Zen 2)

⚠️ Allocator = **Windows UCRT heap** (`malloc`/`free` `api-ms-win-crt-heap-l1-1-0.dll` se import — `objdump -x`
se dekho), glibc nahi. Linux pe absolute numbers alag, shape same.

### `02_stack_vs_heap.cpp` (3 runs)
```
block = 256 bytes, 2,000,000 iterations
stack  per iter : 0.54 – 0.88 ns   (loop mein allocation instruction hi nahi -- frame entry pe ek baar)
heap   per iter : 36.1 – 37.2 ns   (new[] + delete[])
ratio           : ~41 – 68x        (stack number itna chhota ki ratio hilta hai)
```

### `06_allocation_benchmark.cpp` (rdtsc, ns) — 2 runs
```
                 mean    p50    p90    p99   p99.9          max
A) new+delete    ~34     30     40     90    110–120    7,500 – 25,000     (run-variable)
B) new, retained 63–67   30     40    100    410–430   67,000 – 109,000    (run-variable)
C) fixed buffer  ~9.5    10     10     10     20       20 ya 32,500 (preemption outlier)
```
Median theek dikhta hai — **tail** (p99.9 / max) allocator ka asli kharcha dikhata hai: free-list miss, heap
lock, `VirtualAlloc` (Windows) / `brk`/`mmap` (Linux) se nayi memory, first-touch page faults. C ka 10–20 ns
sirf do `rdtsc` ka apna cost hai. Yeh spike HFT hot path pe allowed nahi.

### `07_simple_pool.cpp` — 64 orders ka burst (3 runs)
```
pool  alloc+free : 1.70 – 1.85 ns/pair   (placement new + read samet)
new   alloc+free : 38.1 – 39.0 ns/pair
speedup          : ~21 – 22x
```
**Purana "~190x" galat tha:** pehla benchmark ek hi slot lekar turant wapas deta tha; GCC ne poora pop+push ek
`mov` bana diya (assembly mein dekha) — khaali loop vs asli `new`/`delete`. Ab burst workload hai. `-O0` pe yahi
~5x dikhata hai — benchmark hamesha `-O2`.

## Jaan-boojh kar warnings / UB

- **`04_memory_leak.cpp`** — `-O0` pe koi compile warning nahi; **runtime pe** chup-chaap ~1 MB leak karta hai.
  Counted `operator new`/`new[]` se exit pe `LEAK: 1006 block(s) never freed` print hota hai.
  - **`operator new[]` alag se override kiya hai** — MinGW pe default `operator new[]` `libstdc++-6.dll` ke andar
    hai, aur DLL ke andar ki call exe ke replaced `operator new` tak nahi pahunchti (naapa: normal build count 0,
    `-static` build 1). Lesson 05.
  - `-O2` pe GCC 16.2 ek warning deta hai: `'void free(void*)' called on pointer returned from a mismatched
    allocation function [-Wmismatched-new-delete]` — replaced `operator delete` ke andar `std::free`. Yeh false
    positive hai: hamara `operator new` `std::malloc` hi use karta hai, to `free` sahi pair hai.
- **`05_use_after_free.cpp`** — runtime UB demo (folder 12/13 ke dangling examples jaisa), compile failure nahi.
  BUG 2 (block reuse) allocator pe depend — kabhi same block, kabhi alag; output dono sambhalta hai (is machine pe
  dono runs mein alag block mila). BUG 3 (double-free) comment-out — enable karke crash/abort dekho.

## Sanitizers / tools

⚠️ **MinGW-w64 (`C:\mingw64`) pe ASan / LSan / Valgrind nahi.** Asli leak/UAF diagnosis **Linux / macOS / WSL**
pe:

```bash
# leak (04) — LeakSanitizer exit pe report, ya Valgrind
g++ -std=c++20 -fsanitize=address -g 04_memory_leak.cpp -o leak && ./leak
valgrind --leak-check=full ./leak

# use-after-free / double-free (05) — exact line + "freed by" stack
g++ -std=c++20 -fsanitize=address -g 05_use_after_free.cpp -o uaf && ./uaf
```

`./build.ps1 san <file>` MinGW pe `-D_GLIBCXX_ASSERTIONS + -fstack-protector-all` pe fall back karta hai (heap
UAF/leak nahi pakadta — sirf STL bounds + canaries).

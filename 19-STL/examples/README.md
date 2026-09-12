# Examples — Folder 19 (Standard Library)

| File | Lesson(s) | Kya dikhata hai |
|---|---|---|
| `01_vector_deep.cpp` | 02, 08 | Capacity doubling (1→2→4→8→16→32…); reserve → **1** allocation vs ~17 reallocations; reallocation → `&v[0]` changes → dangling pointers (`%p` compare); `reserve` vs `resize`; `clear()` keeps capacity; `shrink_to_fit` / swap-with-empty; timed reserved vs unreserved fill |
| `02_map_vs_unordered.cpp` | 05, 06, 12, 25 | **Measured** lookup: `std::map` vs `std::unordered_map` (reserved) vs sorted `std::vector` + `std::lower_bound`; same-O(log n) map vs sorted-vector gap is pure cache |
| `03_algorithms_tour.cpp` | 01, 09–13 | 30+ `<algorithm>`/`<numeric>` calls on one vector: count/all_of/find/minmax/mismatch/equal/accumulate/inner_product/partial_sum/adjacent_difference/iota/transform/replace/reverse/rotate/sort/nth_element/partial_sort/partition/binary_search/lower_bound/upper_bound/equal_range/set_* |
| `04_erase_remove.cpp` | 10 | `std::remove` alone (size **unchanged**, tail is junk); erase-remove one-liner; `remove_if`; C++20 `std::erase`/`std::erase_if`; `std::unique` (consecutive only → sort first); erase-in-loop O(n²) contrast |
| `05_ranges_demo.cpp` | 14 | `filter \| transform \| take` lazy pipeline (0 intermediate vectors); drop/reverse/take_while/drop_while/iota; lazy proof (transform call counter); `std::ranges::sort/find/count_if`; materialize into a container |
| `06_optional_variant.cpp` | 15 | `std::optional<int> parsePositive`; `std::variant<nullptr_t,bool,double,std::string>` + generic-lambda `std::visit` with `if constexpr`; `holds_alternative`/`get`/`get_if`; `sizeof` of each; `std::expected` in a comment (C++23) |
| `07_chrono_timing.cpp` | 17 | Clock resolution probe (~100 ns here); **wrong way** (no warm-up / no sink → 0 / nonsense) vs **right way** (warm-up + min of 10 reps); duration arithmetic with `chrono_literals`; `system_clock` for timestamps note; sum-of-`sqrt` ≈ 6.35 ns/elem |
| `08_std_function_cost.cpp` | 16 | **Measured**: templated callable / fn-ptr ≈ 1.48 ns/call (inlined); `std::function` small capture ≈ 5.11 ns (no inline); `std::function` holding a `std::string` closure → **1 heap allocation** + ≈ 6.00 ns; counted `operator new` |
| `09_custom_allocator.cpp` | 23 | `LoggingAllocator<T>` (forwards to `::operator new`/`delete`, prints every vector-growth allocation); `ArenaAllocator<T>` (bump-allocate, `deallocate` no-op, shared `Arena` via `std::shared_ptr`, `reset()` rewinds) on `std::vector<int, Alloc>` |
| `10_pmr_demo.cpp` | 24 | Counted **global** `operator new`: plain `std::vector<int>` → ~11 heap allocs; `std::pmr::vector<int>` + `std::pmr::vector<std::pmr::string>` on a 64 KB **stack** buffer → **0**; `null_memory_resource()` upstream → `std::bad_alloc` on overflow; `pool.release()` reuse across 100 iterations → still 0 |
| `11_container_benchmark.cpp` | 04, 05, 06, 25, 26 | **Measured** suite (N = 1,000,000): sequential iterate, build via push/insert, membership lookup, for `vector`/`deque`/`list`/`set`/`unordered_set`; `vector` linear `std::find` sub-sampled (`SAMPLE=2000`) since N×N is minutes |

## Compile / run

```bash
./build.ps1 19-STL/examples/01_vector_deep.cpp        # Windows (debug -O0 + -Wall -Wextra -Wshadow …)
make FILE=19-STL/examples/01_vector_deep.cpp          # Linux/Mac/Git-Bash
```

Benchmarks at **`-O2`** (mandatory — `-O0` numbers are meaningless):

```bash
./build.ps1 fast 19-STL/examples/02_map_vs_unordered.cpp
./build.ps1 fast 19-STL/examples/07_chrono_timing.cpp
./build.ps1 fast 19-STL/examples/08_std_function_cost.cpp
./build.ps1 fast 19-STL/examples/10_pmr_demo.cpp
./build.ps1 fast 19-STL/examples/11_container_benchmark.cpp
```

## Measured (GCC 15.1.0, `-O2`, x86-64, this box) — sample runs

### `02_map_vs_unordered.cpp` (N = 200,000)
```
std::map              : ~1049 ns / lookup
sorted vector + lower_bound : ~357 ns / lookup     (same O(log n) as map -> 3x is pure cache)
std::unordered_map (reserved) : ~113 ns / lookup   (~1-2 cache misses, flat vs n)
```

### `08_std_function_cost.cpp` (50,000,000 calls of `a + b`)
```
templated callable (lambda)  [inlines]   ~1.48 ns/call
raw function pointer                      ~1.48 ns/call
std::function (small capture)             ~5.11 ns/call     (type-erased indirect, no inline)
std::function (std::string capture)       ~6.00 ns/call  + 1 heap allocation on construction
```

### `11_container_benchmark.cpp` (N = 1,000,000)
```
container         iterate(ms)   build(ms)    membership
vector               0.67          2.86       std::find linear ~0.28 ms/lookup (O(n) -- don't)
deque                2.28          4.09        -
list                18.02         86.43        -            (~27x vector on iterate, ~30x on build)
set (RB-tree)      197.02       1616.95       1065.74 ms for all N (count)
unordered_set       66.61        230.48         49.30 ms for all N (count)   (~22x faster than set)
```

### `07_chrono_timing.cpp`
```
steady_clock resolution here: ~100 ns
wrong way (no warm-up, no sink): often prints 0 ns or garbage
right way  (warm-up + min of 10 reps, -O2, sink): sum-of-sqrt ~6.35 ns/element
```

The **shapes** reproduce everywhere: contiguous + `<algorithm>` ≈ hand code;
node containers lose 20–300x on iteration/lookup to cache misses; `reserve`
turns ~log(n) reallocations into one; `std::function` adds an indirect call and
maybe a heap allocation; PMR + a stack buffer = zero heap.

## Jaan-boojh kar cheezein / notes

- **No `broken_on_purpose` file** in this folder.
- `06_optional_variant.cpp` uses only `optional` + `variant` + `visit`
  (C++17/20-safe). `std::expected` needs `-std=c++23`, so it appears only as a
  comment and is discussed in lesson 15.
- `08_std_function_cost.cpp` and `10_pmr_demo.cpp` override global
  `operator new` / `operator delete` to **count** allocations (folder 14
  technique — MinGW-w64 has no ASan). `08` counts both `operator new` and
  `operator new[]` because MinGW doesn't route `new[]` through `new`.
- `03/04/05` use `%lld` (not `%ld`) for iterator differences and `std::count`
  returns — on Win64 `long` is 4 bytes but `ptrdiff_t` / the return types are 8.
- `11_container_benchmark.cpp` sub-samples the `vector` linear-find
  (`SAMPLE = 2000`) and reports **ms per lookup**; doing all N linear finds over
  1M elements is 10¹² ops (minutes).
- All files compile clean under `-Wall -Wextra -Wshadow` (`./build.ps1 folder
  19-STL`).

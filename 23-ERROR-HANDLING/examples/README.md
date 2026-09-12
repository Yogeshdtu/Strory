# Examples — Folder 23 (Error handling)

| File | Lesson(s) | Kya dikhata hai |
|---|---|---|
| `01_exceptions_basics.cpp` | 03, 04 | `throw`/`try`/`catch` mechanics: catch **by const&**, catch **order** (derived→base), rethrow (`throw;` vs `throw e;`), `catch(...)`, `std::exception_ptr` (`current_exception`/`rethrow_exception`), aur `deep(3)` throw se **stack unwinding** (har `~Guard` reverse order mein) |
| `02_exception_safety.cpp` | 05, 06 | 4 guarantee levels. `BadVec` — grow ki copy-loop mein throw → buffer **leak** (live-object counter se DIKHTA hai, count 0 pe nahi aata). `GoodVec` — `RawBuf` RAII holder → **rollback**, count 0. Plus **copy-and-swap** assignment (`noexcept` swap, self-assign safe) |
| `03_raii_unwinding.cpp` | 04 | manual cleanup **leak** vs RAII; nested scopes → **reverse release order**; ctor ke beech throw (`HalfBuilt` — sirf constructed members ke dtors, `~HalfBuilt` nahi); `std::uncaught_exceptions()`; `DangerousDtor` (dtor-throws-during-unwinding → `std::terminate`, demo **OFF** by default) |
| `04_exception_cost.cpp` | 08 | **Measured** (`-O2`): (1) happy path — try/catch vs return-code **~1.0x** (zero-cost model), (2) throw every iter — **~6000+ ns per throw+catch** vs ~1.5 ns return (**~4000x**), (3) 0.1% error rate — try/catch ~8 ns/iter vs ~1.5 ns (**~5x**) |
| `05_expected.cpp` | 10 | `std::expected<T,E>` patterns: `parse_int → validate_price → transform` **monadic pipeline**, `value_or`, `or_else`. Builds under **`-std=c++20`** (hand-rolled `Expected<T,E>`, same API) **and `-std=c++23`** (real `std::expected`) — prints which is active |
| `06_error_code.cpp` | 11 | `errno` → `std::error_code` + portable `== std::errc::...` check; a **custom `error_category`** (`OrderErr` enum, `message()`, `default_error_condition()` → `std::errc` bucket, `make_error_code`, `is_error_code_enum`); `error_code` vs `error_condition` |
| `07_no_exceptions.cpp` | 09, 02 | The **`-fno-exceptions` toolkit**: `[[nodiscard]] enum class Status`, `FATAL()`/`abort`, fixed-capacity `FixedVec` (overflow → return, not throw), checked `get() → optional`, `parse_u32` (Status + out-param), `new(std::nothrow)` + null check. Compiles + runs **both** with and without `-fno-exceptions` (no `throw` in the file) |

## Compile / run

```bash
./build.ps1 23-ERROR-HANDLING/examples/01_exceptions_basics.cpp    # Windows (debug -O0 + heavy warnings)
make FILE=23-ERROR-HANDLING/examples/01_exceptions_basics.cpp      # Linux/Mac/Git-Bash
```

Benchmark at **`-O2`** (zaroori — `-O0` pe exception cost numbers meaningless):
```bash
./build.ps1 fast 23-ERROR-HANDLING/examples/04_exception_cost.cpp
```

`05_expected.cpp` — dono standard modes try karo:
```bash
./build.ps1 23-ERROR-HANDLING/examples/05_expected.cpp                                   # c++20 (mini Expected)
g++ -std=c++23 -O2 -Wall -Wextra 23-ERROR-HANDLING/examples/05_expected.cpp -o exp && ./exp   # real std::expected
```

`07_no_exceptions.cpp` — `-fno-exceptions` build:
```bash
g++ -std=c++20 -fno-exceptions -fno-rtti -O2 -Wall -Wextra \
    23-ERROR-HANDLING/examples/07_no_exceptions.cpp -o ne && ./ne
```

## Measured (GCC 15.1.0, `-O2`, x86-64, this box) — sample run of `04_exception_cost.cpp`

```
benchmark                                   total       per-iter
-----------------------------------------------------------------
1. happy: try/catch (0 throws)           ~31.5 ms     ~1.58 ns
1. happy: return-code                    ~31.5 ms     ~1.57 ns
   -> ratio ~1.00x            zero-cost model: try region free when no throw

2. throw: every iteration              ~1240 ms     ~6200 ns    <- per throw+catch
2. return-code: every iteration           ~0.30 ms     ~1.50 ns
   -> ratio ~4000x

3. 0.1% errors: try/catch                ~155 ms      ~7.7 ns
3. 0.1% errors: return-code               ~31 ms      ~1.55 ns
   -> ratio ~5x
```

Takeaway: **happy path pe exceptions ~0 overhead; ek `throw` ~µs (nanoseconds
nahi).** Hot path / expected failures → error values. Exceptions → startup +
genuinely-rare + constructors.

## Notes / jaan-boojh kar cheezein

- **No `broken_on_purpose` file.** `03_raii_unwinding.cpp` mein ek commented block
  hai (`DangerousDtor{true}` + outer throw) jo uncomment karne pe `std::terminate`
  dikhata hai — deliberately off so the folder check passes.
- `05_expected.cpp` — repo default `-std=c++20` mein `<expected>` nahi hota, isliye
  file `__cpp_lib_expected` guard ke peeche ek chhota `Expected<T,E>` (same API)
  rakhti hai. Yeh exactly woh hai jo C++20 codebases `tl::expected` / Boost.Outcome
  se karte hain. `-std=c++23` pe real `std::expected` chalti hai.
- `07_no_exceptions.cpp` — koi `throw`/`try`/`catch` nahi, isliye dono modes
  (`-fexceptions`, `-fno-exceptions`) mein compile+run hoti hai. Exception-guarded
  print (`#if defined(__cpp_exceptions)`) batata hai kaunsa build hai.
- Sab `.cpp` `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
  -Wcast-align -Wunused -Wnull-dereference -Wdouble-promotion` pe clean
  (`./build.ps1 folder 23-ERROR-HANDLING`).

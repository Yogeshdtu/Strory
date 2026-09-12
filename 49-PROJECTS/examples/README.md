# Examples — Folder 49 (End-to-end projects)

**17 reference implementations** — complete, self-contained programs (not
snippets), one per project in `../01`–`../03`. Each has an assertion-based
`main()` that runs, edge-case tests, and a **TALKING POINTS** footer.

```
examples/
  beginner/       6 programs   (01-beginner-projects.md)
  intermediate/   5 programs   (02-intermediate-projects.md)
  advanced/       6 programs   (03-advanced-projects.md)   -- 05 is Linux-only
```

---

## Verification

The examples live in **subdirectories**, so `./build.ps1 folder 49-PROJECTS`
does not apply (it only scans `examples/*.cpp` directly). They are covered by:

```bash
./build.ps1 checkall          # recursive -- compiles all 16 non-linux files
```

Every non-linux file compiles clean under the repo's strict set
(`-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wcast-align
-Wnull-dereference -Wdouble-promotion`) and, when run, prints `"... ALL PASS"`.

Per file:
```bash
g++ -std=c++20 -Wall -Wextra -Wshadow -Wconversion -g -O0 beginner/01_calculator.cpp -o t && ./t
```

**Running threaded / benchmark examples on this MinGW box:** `advanced/03_thread_pool`
and `advanced/04_concurrent_queue` need `C:\mingw64\bin` on `PATH` (or a
`-static` link) to *launch* — the loader wants `libstdc++-6.dll` /
`libwinpthread-1.dll`. They **compile** clean everywhere; this only affects
running them here. `advanced/02_allocator` prints real numbers only at `-O2`
(`./build.ps1 fast`).

**`advanced/05_epoll_server.linux.cpp`** — Linux syscalls (`epoll`, `accept4`,
sockets). `checkall` / `build.ps1 folder` **skip** `*.linux.cpp`. Verify on
Linux/WSL (command in the file header).

---

## Beginner (`beginner/`)

| File | Project | What it exercises |
|---|---|---|
| `01_calculator.cpp` | expression evaluator | shunting-yard, precedence + parens, `std::optional` errors, `/0` handled |
| `02_number_guess.cpp` | guessing game | I/O split from logic (scripted `istringstream`), `<random>` done right, binary-search optimal play |
| `03_student_manager.cpp` | student records | `struct` + `vector`, class invariants, `std::sort`/`accumulate`, `std::erase_if` |
| `04_expense_tracker.cpp` | expense tracker | **integer money (paise)**, `std::map` by category, `partial_sort` top-N, budget warnings |
| `05_file_word_stats.cpp` | word statistics | `istream&` interface, `unordered_map` frequency, `tolower` on `unsigned char`, tie-broken top-N |
| `06_config_parser.cpp` | INI config parser | state-tolerant parse, sections, typed getters + defaults, **line-numbered errors**, round-trip |

## Intermediate (`intermediate/`)

| File | Project | What it exercises |
|---|---|---|
| `01_inventory.cpp` | warehouse inventory | class invariants (qty ≥ 0), **append-only log + `replay()`** (event sourcing) |
| `02_bank.cpp` | bank simulation | **atomic transfer** (both legs or neither), rational-rate integer interest, a global invariant checked in a 20k-op property test |
| `03_csv_tool.cpp` | CSV parser + query | **4-state field parser** (quotes, embedded commas/newlines), filter/sort/aggregate, byte-offset errors |
| `04_log_analyzer.cpp` | log analyzer | robust line parsing (malformed → skip+count), per-minute buckets, top-N, running-baseline spike flag |
| `05_kv_store.cpp` | persistent KV store | **write-ahead log** (len + CRC), replay on open, **compaction** via temp-file + `rename`, torn-tail recovery |

## Advanced (`advanced/`)

| File | Project | What it exercises |
|---|---|---|
| `01_vector.cpp` | custom `Vector<T>` | raw storage + placement `new`, Rule of 5 (copy-and-swap), `move_if_noexcept` relocate, **strong exception guarantee**, contiguous iterators |
| `02_allocator.cpp` | 3 allocators | arena (O(1) reset), fixed-size pool (intrusive free list), segregated size classes; **measured ~20–28× vs `new` at `-O2`** |
| `03_thread_pool.cpp` | thread pool | mutex+cv task queue, `packaged_task`/`future`, exception propagation, idempotent shutdown, submit-after-stop throws |
| `04_concurrent_queue.cpp` | 3 queues | mutex bounded queue; **lock-free SPSC ring** (no CAS); **MPSC Vyukov** (`XCHG` tail); stress tests prove in-order/no-loss |
| `05_epoll_server.linux.cpp` | epoll echo server | non-blocking I/O, `EPOLLET` drain loops, partial-write buffering, self-pipe shutdown — **the OS event loop a feed handler sits on** (Linux only) |
| `06_json_parser.cpp` | JSON parser | recursive descent, `std::variant` tree, **`line:col` errors**, **depth limit** (stack-overflow guard), serializer + round-trip test |

---

## Where these lead

The advanced projects are folder [`44-HFT-PROJECTS`](../../44-HFT-PROJECTS/00-README.md)'s
prerequisites in general form — see [`../05-hft-projects-link.md`](../05-hft-projects-link.md).
The custom vector → the flat book; the allocator → `FixedPool`/`ObjectPool`; the
SPSC ring → the wire→engine hand-off; the epoll loop → the feed handler; the WAL
+ replay → the deterministic engine.

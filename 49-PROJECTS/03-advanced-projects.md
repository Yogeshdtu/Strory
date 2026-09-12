# 03 — Advanced projects

## Prerequisites
- Folders `24`–`35` (compilation → profiling). Especially: object model (25),
  concurrency (26–28), Linux (29), and the measurement discipline (35, 43).

## Yeh file kya hai
6 projects jahan **implementation quality** hi asli seekh hai — allocators,
containers, concurrency primitives, a parser, a server. Har ek ke saath: **build
simple → measure → profile → optimize → explain** (spec §2.12).

Reference: `examples/advanced/`. `05_epoll_server` **Linux-only** (`.linux.cpp`,
`checkall`/`folder` skip karte — WSL/Linux pe verify karo).

---

## P1 — Custom `Vector<T>` (`examples/advanced/01_vector.cpp`)

**Spec:** a growable array with the `std::vector` core API: `push_back`,
`emplace_back`, `pop_back`, `operator[]`, `at`, `size`, `capacity`, `reserve`,
`clear`, `begin`/`end`, copy + move ctor/assign, iterators, exception safety.

**Milestones:**
- **v1** — `T* data_; size_; cap_;`. `push_back` with geometric growth (×2).
  `operator[]`, `size`, dtor. **Use raw storage + placement `new`**, not
  `new T[n]` (which default-constructs everything).
- **v2** — Rule of 5: copy (deep), move (steal + null), copy-and-swap assign.
  `reserve` (relocate: **move if `noexcept`, else copy** — the strong-guarantee
  rule). `emplace_back` with perfect forwarding.
- **v3** — iterators (contiguous → random-access tag, `std::sort` works),
  `at()` throws `std::out_of_range`, `shrink_to_fit`, exception safety
  (element ctor throws mid-relocate → original intact). Compare growth factor
  1.5 vs 2.0 (measure reallocs + total bytes copied).

**Concepts:** placement `new` + explicit `~T()` (14), Rule of 5 + copy-and-swap
(18), `std::move_if_noexcept` (18), iterator design (19/21), the amortized-O(1)
proof and the p99 realloc spike (04-cheatsheet, 43/16).

**Tests:** push 1e6 ints, assert `size`/values; copy → independent; move → source
empty & valid; `reserve` doesn't change `size`; a throwing element type leaves
the vector unchanged on a failed relocate; `std::sort` on the iterators works.
Bench: vs `std::vector`, expect within ~1–1.2×.

**Extensions:** small-buffer optimization (inline N elements); a custom allocator
parameter; `std::vector<bool>`-style bit packing (and why it's controversial).

---

## P2 — Memory allocator (`examples/advanced/02_allocator.cpp`)

**Spec:** three allocators behind a common `allocate(size, align)` /
`deallocate` interface: (a) **bump/arena** (no individual free, O(1) reset),
(b) **fixed-size pool** (free list, O(1) acquire/release), (c) **segregated
free-list** (size classes → per-class pool, falls back to `malloc` for big).
Benchmark each vs `malloc`/`new` for an allocation-heavy workload.

**Milestones:**
- **v1** — the arena: align-up, bump `cur_`, bounds check, `reset()`.
- **v2** — the fixed-size pool: intrusive free list through the free slots;
  `acquire`/`release`; exhaustion → `nullptr`.
- **v3** — size classes (16/32/64/128/256/…): `bit_ceil` to pick a class,
  per-class pool, refill a class from a bigger slab page. Benchmark: 1e6
  `acquire`/`release` of mixed sizes, arena vs pool vs segregated vs `malloc`;
  report ns/op and p99.9. Explain *why* the pool wins (no syscall, no lock, hot
  cache line).

**Concepts:** alignment (`(p + a - 1) & ~(a - 1)`), intrusive free lists (12/14),
placement `new`, `std::pmr` comparison (19), the "no `malloc` on the hot path"
rule (36), measurement (35, 43).

**Tests:** every allocation is correctly aligned; arena `reset` reclaims
everything; pool `release` then `acquire` returns the same slot (LIFO); pool
exhaustion returns `nullptr` (no `malloc` fallback); no leaks under ASan/valgrind.

**Extensions:** a `std::pmr::memory_resource` wrapper (use it with `pmr::vector`);
thread-local arenas; a debugging mode (poison freed memory, guard pages).

---

## P3 — Thread pool (`examples/advanced/03_thread_pool.cpp`)

**Spec:** `ThreadPool(n)`. `submit(callable) -> std::future<R>`. Fixed workers
pulling from a shared task queue. Clean shutdown (drain or cancel). Optional:
per-worker queues + work stealing.

**Milestones:**
- **v1** — `n` `std::thread`s, one `std::mutex` + `std::condition_variable` +
  `std::queue<std::function<void()>>`. `submit` wraps a `std::packaged_task`.
- **v2** — graceful shutdown (`stop` flag + `notify_all` + join in dtor);
  exceptions in tasks propagate through the `future`; `submit` after shutdown
  throws.
- **v3** — per-worker deques + **work stealing** (Chase-Lev-ish: owner pops the
  back, thieves steal the front); measure throughput vs the single-queue version
  for many tiny tasks (contention on the one mutex is the bottleneck).

**Concepts:** `mutex` + `cv` + predicate wait (26), `packaged_task`/`future` (26),
`jthread`/`stop_token` (26), false sharing between per-worker state → `alignas(64)`
(32), work-stealing deque (28).

**Tests:** submit 10 000 tasks that increment an atomic → total is exact;
a task that throws → `future.get()` rethrows; dtor joins all workers (no
detached threads); `submit` after `shutdown()` throws. Bench: tiny-task
throughput, single-queue vs work-stealing.

**Extensions:** priorities; a bounded queue with back-pressure; `co_await`-able
tasks (coroutines, 22); pinning workers to cores (29/36).

---

## P4 — Concurrent queue (`examples/advanced/04_concurrent_queue.cpp`)

**Spec:** three queues behind one test harness: (a) **mutex + cv** bounded
blocking queue, (b) **lock-free SPSC** ring (one producer, one consumer,
acquire/release, power-of-two mask), (c) **lock-free MPSC** (Vyukov intrusive,
`XCHG` on the tail). A stress test proving in-order, no-loss delivery for each.

**Milestones:**
- **v1** — the mutex+cv bounded queue (2 CVs, predicate waits, `close()`).
- **v2** — the SPSC ring: `alignas(64)` head/tail, cached opposite index, no CAS.
  2-thread blast of 2e6 items, assert strict in-order.
- **v3** — the MPSC queue: stub node, producers `tail.exchange`, consumer walks
  `head->next`; N producers × M items, assert the multiset of received == sent
  and each producer's items arrive in its own order.

**Concepts:** `condition_variable` predicate form (26), memory ordering —
acquire/release handoff, why SPSC needs no CAS (27/28), `XCHG` vs CAS-loop (28),
false sharing on head/tail (32), ABA (only if you free nodes — 28).

**Tests:** mutex queue: producer/consumer checksum balances, `close()` drains
then returns empty. SPSC: 2e6 `uint64_t`, receiver asserts `v == expected++`.
MPSC: 4 producers × 500k, received count exact, per-producer order preserved.
Run each at `-O2` (reordering hides at `-O0`).

**Extensions:** MPMC (the hard one — Vyukov bounded array with per-slot
sequence numbers); batched pop; latency histogram of enqueue→dequeue.

---

## P5 — epoll echo/line server (`examples/advanced/05_epoll_server.linux.cpp`)

**Spec (Linux):** a single-threaded, non-blocking TCP server using `epoll`.
Accepts many clients, echoes lines back, handles partial reads/writes, backs off
on `EAGAIN`, cleans up on disconnect. No thread per connection.

**Milestones:**
- **v1** — `socket`/`bind`/`listen`; `epoll_create1`; level-triggered; accept
  loop + per-fd read→echo. `O_NONBLOCK` on everything.
- **v2** — edge-triggered (`EPOLLET`): drain reads until `EAGAIN`; a per-fd
  write buffer + `EPOLLOUT` only when there's pending output; `EPOLLRDHUP`.
- **v3** — a small connection struct pool (no per-conn `malloc`); handle the
  "thundering accept" with `accept4` in a loop; `SO_REUSEPORT` sketch for
  multi-acceptor; graceful shutdown on `SIGTERM` (`signalfd` / a self-pipe).

**Concepts:** non-blocking I/O + readiness model (30), `epoll` level vs edge (29),
partial read/write handling, `EAGAIN`/`EWOULDBLOCK`, `SIGPIPE` (ignore it),
back-pressure, a connection pool (14/36). **This is the OS-event-loop core that a
feed handler / order gateway sits on (42).**

**Tests (Linux/WSL):** `nc`/a script opens 100 connections, sends lines,
asserts identical echoes; a client that half-closes is cleaned up; a slow
client (never reads) doesn't block others; `valgrind --tool=helgrind` clean
(single-threaded → trivially).

**Extensions:** `io_uring` version; TLS via a state machine; an HTTP/1.1
subset; multi-threaded with `SO_REUSEPORT` + one epoll per thread.

---

## P6 — JSON parser (`examples/advanced/06_json_parser.cpp`)

**Spec:** parse JSON (RFC 8259) into a `Value` variant
(`null|bool|double|string|array|object`). Recursive-descent. Precise errors
(`"line 3 col 12: expected ',' or '}'"`). Then: a serializer (round-trip),
and a tiny path query (`obj["a"][0]["b"]`).

**Milestones:**
- **v1** — a tokenizer (`{ } [ ] : ,`, strings with `\n \t \" \\ \uXXXX`,
  numbers, `true/false/null`), then a recursive-descent parser building the
  `Value` tree. `std::variant` + `std::unique_ptr` for the recursive members
  (or `std::map`/`std::vector` of `Value`).
- **v2** — full error reporting with line/column; reject trailing junk, control
  chars in strings, duplicate keys policy, number edge cases (`-0`, `1e10`,
  leading zeros rejected); depth limit (stack-overflow guard).
- **v3** — serializer (compact + pretty); round-trip property test (parse →
  serialize → parse → equal); a path accessor; optional: a SAX-style callback
  parser for large inputs (no tree).

**Concepts:** recursive-descent parsing, `std::variant` + recursive types (22),
`std::unique_ptr` for tree ownership (14), UTF-8 / `\u` surrogate pairs (10),
error context, **depth limiting** (recursion + attacker input = stack overflow —
11/UB, 45), `std::from_chars` for numbers.

**Tests:** the JSON.org test suite categories (`y_` accept, `n_` reject) — pass
all reasonable ones; round-trip on a nested fixture; a 10 000-deep `[[[…` is
rejected cleanly, not a crash; every `n_` malformed input yields an error with a
sane position.

**Extensions:** JSON5 / comments; streaming/SAX; a JSON Pointer (RFC 6901)
implementation; a schema validator; benchmark vs a real lib (nlohmann / RapidJSON).

---

## The process, every project

```
1. simplest correct version
2. a test that pins the behaviour + the invariant
3. measure (ns/op, throughput, p99.9) — realistic input, -O2
4. profile (perf) — where is the time?
5. one change
6. re-measure — ratio
7. EXPLAIN what changed and why (cache? branch? syscall? contention?)
```

## Next
→ [`04-project-guidelines.md`](04-project-guidelines.md)

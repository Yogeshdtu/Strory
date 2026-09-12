# 10 — HFT problems

## Prerequisites
- `36-LOW-LATENCY-CPP/` … `44-HFT-PROJECTS/` (poora HFT track)
- `37-HFT-FUNDAMENTALS/`, `38-MARKET-DATA/`, `39-ORDER-BOOK/`, `40-MATCHING-ENGINE/`
- `08-lock-free-problems.md`, `09-optimization-problems.md`

## Yeh file kya hai
20 problems — real HFT building blocks: wire parsing, order book ops, matching,
feed gap handling, risk gates, SPSC handoff, latency measurement, timer wheels.
Yeh woh cheezein hain jo onsite pe whiteboard pe likhwaate hain.

Har problem: `<details>` mein approach + latency budget + the trap. Poora code →
[`11-solutions/10-hft-solutions.md`](11-solutions/10-hft-solutions.md).

Several map onto `44-HFT-PROJECTS/` aur `46-INTERVIEW-PREP/examples/`.

---

## Part A — Parsing & wire format

### 1. Parse a fixed-width binary message
`struct WireQuote { char type; uint32_t id; int64_t px; uint32_t qty; }` — a
byte buffer se, no allocation, endianness handle.
`Pattern:` `memcpy` into a POD, byte-swap if needed.
<details><summary>Approach</summary>

`WireQuote q; std::memcpy(&q, buf, sizeof(q));` — `memcpy` handles alignment
(don't `reinterpret_cast` a misaligned buffer). Wire is usually big-endian /
network order → `q.px = bswap64(q.px)` (`__builtin_bswap64` / `std::byteswap`
C++23). `#pragma pack` ya explicit field offsets if the wire has no padding.
Budget: < 10 ns.
</details>

### 2. ASCII decimal price → int64 ticks
`"12345.67"` → `1234567` (scale 100), single pass, overflow guard, no float.
`Pattern:` walk chars, accumulate, track decimal position.
<details><summary>Approach</summary>

Optional `-`; digits before `.` → `int_part = int_part*10 + (c-'0')` with guard
`int_part > (INT64_MAX - d)/10`; after `.` count up to `kDecimals`, pad missing,
truncate extra. Float parsing = rounding error → exact compares tootte. Example:
`46-INTERVIEW-PREP/examples/04_parse_fixed_point_price.cpp`. Budget: < 15 ns.
</details>

### 12. Encode an order into a fixed buffer
`new_order(id, side, px, qty)` → bytes in a caller-provided buffer, return
length. No `stringstream`, no alloc.
<details><summary>Approach</summary>

`char* p = buf; *p++ = 'N'; write_be32(p, id); p += 4; *p++ = side; write_be64
(p, px); p += 8; …; return p - buf;`. For ASCII protocols (FIX-like) → a fast
`itoa` into the buffer, `SOH` separators. Precompute the fixed header. Budget:
< 20 ns.
</details>

### 17. Symbol → dense id interning
`"AAPL"`, `"MSFT"`, … → `0, 1, 2, …` at startup; hot path only uses the int.
<details><summary>Approach</summary>

Startup: read the symbol list, build `unordered_map<string, uint32_t>` **once**;
also a `vector<string>` for id→symbol. Hot path: `int id = symbol_id[sym];` (or
the message already carries a numeric instrument id — even better). For a fixed
known universe → a perfect hash (gperf) or a sorted array + branchless binary
search. Never `std::string` compares on the hot path.
</details>

---

## Part B — Order book & matching

### 3. Add a limit order, O(1) BBO
Price-indexed book. `add(side, tick, qty)` updates the level and the cached best
bid/ask in `O(1)`.
<details><summary>Approach</summary>

`int64_t bid_qty[N], ask_qty[N];` indexed by `tick - base_tick`. `add`:
`bid_qty[t] += qty; if (side==Bid && t > best_bid_tick) best_bid_tick = t;`.
No search. Range `N` covers realistic price band; outside → reject / rebase.
Folder 39; `46-INTERVIEW-PREP/examples/06_top_of_book.cpp`. Budget: < 20 ns.
</details>

### 4. Cancel by order id, fix BBO
`cancel(OrderId)` — `O(1)` via an id→location index; if the level emptied and it
was the BBO, re-walk to the next non-empty level (bounded).
<details><summary>Approach</summary>

`struct Loc { Side side; int32_t tick; uint32_t slot; };  std::vector<Loc> loc_`
indexed by id (dense). `cancel`: `qty[loc.tick] -= o.qty;` if now `0` and
`loc.tick == best_*_tick` → `while (qty[--best] == 0 && best > floor);`. Bounded
because books are dense near the top. Budget: < 30 ns typical, worst = the rewalk.
</details>

### 5. Match an aggressive order
Incoming marketable order matches the opposite side by price-time priority; emit
fills; leftover rests (limit) or cancels (IOC).
<details><summary>Approach</summary>

Walk opposite side from best toward the limit price. At each level, FIFO through
the resting orders: `fill = min(incoming_qty, resting_qty)`, emit `(taker,
maker, px, fill)`, decrement both, pop the maker if filled. Stop when
`incoming_qty == 0` or price crosses back. IOC → cancel remainder; FOK → precheck
total available **before** matching (the classic bug: matching first, then
discovering you can't fill, then unwinding). Folder 40.
</details>

### 6. Top-N levels for a UI snapshot
Every tick, publish the N best bids/asks — without a full sort of the level
array.
<details><summary>Approach</summary>

From the cached BBO tick, walk outward collecting the first N non-empty levels —
`O(N + gaps)`, and books are dense so gaps are few. If levels live in an
unordered structure → `std::partial_sort` / `std::nth_element` for the N best
(`O(L log N)` / `O(L)`), not `std::sort` (`O(L log L)`). Coalesce to a timer
(#15) — the UI doesn't need every tick.
</details>

---

## Part C — Feed handling & risk

### 7. Sequence-gap detection + A/B arbitration
Two redundant UDP multicast lines, sequence-numbered. Detect a gap, request
retransmit, dedupe across A and B.
<details><summary>Approach</summary>

Track `expected_seq`. Packet `s`: `s == expected` → process, `++expected`;
`s < expected` → duplicate (already got it from the other line), drop;
`s > expected` → gap of `s - expected`; buffer `s`, start a short timer, if the
missing ones don't arrive on either line → send a retransmit request for
`[expected, s)`. A/B: process whichever line delivers a given seq first, the
other line's copy is the drop case. Budget: per-packet check < 10 ns.
</details>

### 8. Pre-trade risk gate — 5 O(1) checks, ordered
Before an order goes out: max order qty, max position, max notional, price band,
message rate. Order them by cost / likelihood of rejecting.
<details><summary>Approach</summary>

Cheapest & most-likely-to-fire first (fail fast): (1) `qty <= max_qty` (one
compare), (2) price band `|px - ref| <= band` (one compare), (3) `msg_count_this_
window < rate_limit` (counter), (4) `abs(position + signed_qty) <= max_pos`
(add + compare), (5) `notional = px*qty; notional + exposure <= max_notional`
(the multiply — costliest). All branch-predictable (almost always pass). Rate
limit ≠ kill switch: rate-limit throttles, kill switch latches off. Budget:
< 20 ns total.
</details>

### 9. SPSC handoff feed → strategy
The feed thread parses; the strategy thread decides. Hand messages across without
a lock. Sizing, back-pressure.
<details><summary>Approach</summary>

Lock-free SPSC ring (`08-lock-free-problems.md` #5), power-of-two capacity sized
for the worst micro-burst (e.g. 64K slots). Payload = a fixed POD (parsed event),
not a pointer (no lifetime/alloc). Full → **drop + increment a counter**, never
block the feed thread (a blocked feed thread = missed market). Strategy thread
spins on `try_pop` (pinned core) or sleeps if latency budget allows. Budget:
push ~20–40 ns.
</details>

### 15. Coalesce a burst of book updates
Many updates for the same instrument within one "frame"; downstream only needs
the latest. Collapse, flush on a timer.
<details><summary>Approach</summary>

`std::array<Update, NUM_INSTRUMENTS> pending; std::bitset<NUM_INSTRUMENTS> dirty;`
— each update overwrites `pending[id]`, sets `dirty[id]`. On the flush tick, walk
the set bits (`_Find_first` / `_Find_next` or word-scan), emit `pending[id]`,
clear. Last-value-wins, `O(1)` per update, `O(dirty)` per flush. Bounded, no
allocation.
</details>

---

## Part D — Measurement & lifetime

### 10. Latency histogram on the hot path
Record tick-to-trade latency per event in `O(1)`; print percentiles off-path.
<details><summary>Approach</summary>

HdrHistogram-style: bucket by the high bits of the value (log-linear) — `idx =
(msb(v) << k) | next_k_bits`. `++counts[idx]` — one array bump, `O(1)`, no
branchy logic. Off-path: sum `counts` to find p50/p99/p99.9. Fixed array
(~few KB), zero allocation on the hot path. Don't compute percentiles inline.
Folder 35.
</details>

### 11. rdtsc timing harness
Measure a code region in cycles → ns. The pitfalls.
<details><summary>Approach</summary>

`uint64_t t0 = __rdtsc(); /* region */ uint64_t t1 = __rdtsc();` — but: (a) OoO
execution reorders `rdtsc` around your code → use `__rdtscp` (partial
serialization) or `lfence; rdtsc; lfence`. (b) TSC is a fixed-frequency counter
on modern x86 (invariant TSC) — `ns = cycles * 1e9 / tsc_hz`, calibrate `tsc_hz`
once vs `clock_gettime`. (c) the probe itself costs ~20–40 ns — for sub-10-ns
regions the probe dominates (folder 43 `02`: v3 per-stage numbers are "≈ 2×
(lfence+rdtsc)"). Measure a loop of N, divide.
</details>

### 16. Fixed-capacity order pool
`ObjectPool<Order, N>` — `acquire(args...)` / `release(Order*)` both `O(1)`, no
`new` after construction, exhaustion policy.
<details><summary>Approach</summary>

`union Slot { uint32_t next; alignas(Order) std::byte bytes[sizeof(Order)]; };
Slot slots[N]; uint32_t free_head;`. `acquire`: pop `free_head`, placement-new.
`release`: `~Order()`, push. Exhausted (`free_head == NIL`) → return `nullptr`,
increment a counter (caller rejects the order — never allocate as a fallback on
the hot path). `46-INTERVIEW-PREP/examples/05_object_pool.cpp`.
</details>

### 18. Timer wheel for order TTLs
Thousands of orders with expiry times (IOC, GTD); `O(1)` insert and `O(1)`
per-tick expiry processing.
<details><summary>Approach</summary>

Hashed timing wheel: `std::array<IntrusiveList, WHEEL_SIZE> buckets;` — an order
expiring at tick `t` goes in `buckets[t & (WHEEL_SIZE-1)]`, with a "rounds"
counter for `t >> log2(WHEEL_SIZE)`. Each tick: advance the cursor, walk that
bucket, expire the rounds-0 entries, decrement the rest. `O(1)` amortized.
Hierarchical wheels for a wide range of timeouts (like the Linux kernel).
</details>

---

## Part E — Discussion (no single answer)

### 13. Moving VWAP / SMA over a fixed window, O(1)
<details><summary>Approach</summary>

Ring buffer of the last `W` samples + a running `sum` (and for VWAP a running
`sum_px_qty` and `sum_qty`). On each new sample: `sum += x - buf[head]; buf
[head] = x; head = (head+1) & (W-1);`. `O(1)`, no rescan. Float drift over
millions of updates → periodically recompute from the ring, or use integer
fixed-point. `46-INTERVIEW-PREP/examples/08_moving_average.cpp`.
</details>

### 14. A division-free trading signal
"Enter if `mid > sma * (1 + threshold)`" without an `idiv` or float divide.
<details><summary>Approach</summary>

`threshold = num/den` as a rational (e.g. 5 bps = 5/10000). The test `mid > sma
* (den + num) / den` → cross-multiply: `mid * den > sma * (den + num)` — all
integer multiplies, no divide, no float. Watch the multiply's range → `int64_t`
or `__int128`. Folder 43; `46-INTERVIEW-PREP/examples/08_moving_average.cpp`.
</details>

### 19. Warm the path (pre-market checklist as code)
<details><summary>Approach</summary>

Before the open: `mlockall(MCL_CURRENT|MCL_FUTURE)` (no page faults mid-session);
prefault every pool/ring/buffer (touch one byte per page, or `memset`); pin each
thread (`sched_setaffinity`), isolate those cores (`isolcpus`, `nohz_full`);
run a few thousand **synthetic** ticks through the whole pipeline so every branch
predictor / I-cache / D-cache line / TLB entry is hot; set the logger to async/
drop mode; disable turbo transitions (fixed frequency). The first real tick must
hit an entirely warm machine. Folder 36.
</details>

### 20. Defend a p99.9
Tick-to-trade "median 900 ns, p99.9 4.2 µs" — where do you look, in what order?
<details><summary>Approach</summary>

The tail is a different bug than the median. Check, roughly in order: (1)
allocations on the hot path (a `malloc` that usually hits the free list but
occasionally `mmap`s / contends) → pool everything. (2) page faults → `mlockall`
+ prefault. (3) the logger blocking when its queue fills → drop mode. (4) a
`std::map`/`unordered_map` rehash or a `vector` realloc → `reserve`, fixed
capacity. (5) NUMA / cross-socket traffic → pin. (6) scheduler preemption / IRQs
on the trading core → isolate, `nohz_full`. (7) TLB misses on huge working sets →
huge pages. (8) contended locks / false sharing → per-core state. Measure each
with `perf` (folder 35) — one change at a time (folder 43).
</details>

---

## Next
→ [`11-solutions/README.md`](11-solutions/README.md)

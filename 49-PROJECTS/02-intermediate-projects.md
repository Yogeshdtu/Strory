# 02 — Intermediate projects

## Prerequisites
- Folders `12`–`23` (pointers → error handling). Especially: classes + RAII
  (15–17), copy/move (18), STL (19), templates basics (21), error handling (23).

## Yeh file kya hai
5 projects jo **multiple subsystems jodte hain** — ek data model, ek storage
layer, ek query/report layer. Ab `class` design, invariants, aur file formats
matter karte hain.

Format wahi: **spec → milestones → concepts → tests → extensions.** Reference:
`examples/intermediate/`.

---

## P1 — Inventory system (`examples/intermediate/01_inventory.cpp`)

**Spec:** a warehouse inventory. `Item {sku, name, qty, reorder_level,
unit_price_cents}`. Operations: receive stock, ship stock (reject if insufficient),
adjust, list low-stock items, total inventory value, a transaction log.

**Milestones:**
- **v1** — `class Inventory` wrapping `std::unordered_map<Sku, Item>`; add / receive
  / ship with a bool result.
- **v2** — invariants enforced (qty never negative, price non-negative);
  `ship` returns a typed result (`enum class ShipResult`); an append-only
  `std::vector<Txn>` log with timestamps.
- **v3** — low-stock report (qty ≤ reorder_level), total value (integer cents),
  "reorder suggestions", replay the log to rebuild state (event sourcing).

**Concepts:** class invariants + encapsulation (15), `enum class` results vs
exceptions (23), integer money (03), `unordered_map` (19), event log / replay
(a determinism idea from 43/44), Rule of 0 (18).

**Tests:** receive 100, ship 30 → qty 70; ship 100 → rejected, qty unchanged;
value = Σ qty·price; replaying the log reproduces the exact final state; a
low-stock item appears in the report iff qty ≤ level.

**Extensions:** multiple warehouses + transfers; expiry dates + FIFO/FEFO
picking; concurrent access (→ advanced P3/P4); persist the log to disk.

---

## P2 — Bank simulation (`examples/intermediate/02_bank.cpp`)

**Spec:** accounts with balances (integer paise). `open`, `deposit`, `withdraw`
(no overdraft), `transfer` (atomic — both legs or neither), statement, interest
accrual. Every mutation is a logged, timestamped transaction.

**Milestones:**
- **v1** — `class Bank`, `open` → account id, deposit/withdraw with bounds checks.
- **v2** — `transfer(from, to, amt)` — must be all-or-nothing; a `Transaction`
  record per leg; per-account statement (filtered log).
- **v3** — monthly interest (rate as a rational, integer math — no `double`);
  a daily-balance minimum rule; overdraft protection linking a savings account;
  invariant check `Σ balances == Σ deposits − Σ withdrawals`.

**Concepts:** atomicity at the application level (transfer = 2 writes that must
not partially apply), integer money + rational interest (03, 43/14), invariants,
`std::optional` / result types (23), the transaction-log pattern again (reuse from P1).

**Tests:** transfer succeeds → both balances change by ±amt; transfer that would
overdraft → *neither* balance changes; the global invariant holds after a random
sequence of 10 000 ops; interest on ₹10 000 at 6%/yr for one month is exact.

**Extensions:** concurrency (a mutex per account + `std::scoped_lock` for
transfer — the deadlock lesson, 26); double-entry ledger; currencies + FX; undo.

---

## P3 — CSV parser & query tool (`examples/intermediate/03_csv_tool.cpp`)

**Spec:** parse RFC-4180-ish CSV (quoted fields, embedded commas/newlines/quotes,
a header row) into rows of columns. Then: select columns, filter rows by a
predicate, sort by a column, aggregate (count / sum / avg / min / max), print a
table.

**Milestones:**
- **v1** — split on `,` and `\n` (no quoting); `std::vector<std::vector<std::string>>`.
- **v2** — a proper state-machine field parser: `"a,b"`, `"he said ""hi"""`,
  fields with newlines. Header → column-name → index map.
- **v3** — a mini query: `select name, marks where marks > 80 order by marks desc`
  (parse a tiny DSL or take structured args); aggregates; typed columns
  (`std::from_chars` to detect int/double/string per column).

**Concepts:** state-machine parsing (the right way vs `split`), `std::string_view`
for zero-copy field slices (careful: lifetime — 10/19), column-oriented vs
row-oriented (32 AoS/SoA idea), `std::from_chars` (10), stable sort by a key (19).

**Tests:** a fixture with every quoting edge case round-trips (parse → serialize →
parse identical); a field with an embedded newline stays one field; filter +
sort + sum on a known dataset gives known numbers; a malformed row (unterminated
quote) reports an error with the byte offset.

**Extensions:** streaming parse (don't hold the whole file); TSV / custom
delimiter; join two CSVs on a key; write results back out.

---

## P4 — Log analyzer (`examples/intermediate/04_log_analyzer.cpp`)

**Spec:** parse web/app access logs (`timestamp level source message`, or a
common-log-format line). Report: lines per level, error rate over time (buckets),
top-N error messages, top-N sources, requests/sec peak, slowest N requests (if a
duration field is present), a simple anomaly flag (error rate > k× the baseline).

**Milestones:**
- **v1** — read lines, classify by level substring, count per level.
- **v2** — parse the timestamp into a comparable value; bucket by minute; per-
  bucket counts; top-N messages via `unordered_map` + `partial_sort`.
- **v3** — rolling baseline + spike detection; latency percentiles from the
  duration field (a histogram — reuse the idea from `47/10 #10`); a summary report.

**Concepts:** line parsing + tokenization, time bucketing, `unordered_map`
frequency + top-N (19), histograms / percentiles (35, 47), rolling window
(sliding-window idea from 20), robust parsing (skip/flag bad lines, don't crash).

**Tests:** a fixture log with known counts → exact per-level totals; the minute
buckets line up; top-3 errors are right; a synthetic spike is flagged and a
flat log is not; p50/p99 of a known duration set are correct.

**Extensions:** `tail -f` mode (follow a growing file); regex-configurable
formats; JSON-lines input; output a Grafana-ish ASCII sparkline.

---

## P5 — Mini key-value database (`examples/intermediate/05_kv_store.cpp`)

**Spec:** a persistent KV store. `put(key, value)`, `get(key)`, `del(key)`,
`scan(prefix)`. Durability via an **append-only log** (write-ahead); an in-memory
index (`std::map` for ordered `scan`); crash-safe (replay the log on startup);
periodic compaction (rewrite the log dropping dead entries).

**Milestones:**
- **v1** — in-memory `std::map<std::string, std::string>` only; put/get/del/scan.
- **v2** — append every mutation to a log file as `op key len value`; on startup,
  replay the log to rebuild the map. Fsync policy (per-write vs batched) as a knob.
- **v3** — compaction: write a fresh log containing only live keys, atomically
  swap it in (`rename`); a CRC per record to detect torn writes; a size trigger.

**Concepts:** write-ahead logging / durability, replay = determinism (43/44),
`std::map` for ordered scan vs `unordered_map` (19), binary record framing
(length-prefixed), atomic file replace (`rename`), CRC / integrity (11/UB —
torn write is a real failure mode).

**Tests:** put/get/del round-trip; `scan("user:")` returns exactly the matching
keys in order; kill the process mid-run (simulate: truncate the log) and reopen —
committed writes survive, the torn tail is dropped; compaction preserves all
live values and shrinks the file.

**Extensions:** an LSM-ish memtable + SSTable flush; TTL / expiry; range
delete; a tiny network protocol in front (→ advanced P5 epoll server);
snapshots.

---

## Working notes

- Each of these is ~200–400 lines done well. Build v1, **write the invariant
  check**, then iterate.
- The transaction-log / replay pattern shows up in P1, P2, P5 — that's
  deliberate. It's how you get determinism and crash-safety, and it's the same
  idea folder 43/44 use for the HFT engine.
- Money is always integer (paise/cents). `double` for money is a bug.

## Next
→ [`03-advanced-projects.md`](03-advanced-projects.md)

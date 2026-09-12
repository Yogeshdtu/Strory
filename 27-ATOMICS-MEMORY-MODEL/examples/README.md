# Examples — Folder 27 (Atomics & the memory model)

| File | Lesson(s) | Kya dikhata hai |
|---|---|---|
| `01_atomic_counter.cpp` | 01, 02, 03 | ⚠️ **Data race.** 8 threads × 2M non-atomic `counter = counter + 1` (`volatile` **only** to stop `-O2` coalescing — `volatile` ≠ atomic) → **~3M instead of 16M**, different every run, formally UB. Then `atomic<long>` `++` / `fetch_add(relaxed)` → exact 16000000. `is_always_lock_free`: `int`/`long`/`void*` = 1, 24-byte struct = 0 (hidden lock) |
| `02_cas_loop.cpp` | 05 | `template<class T,class Fn> T atomic_update(atomic<T>&, Fn)` CAS loop; CAS-loop **atomic max**; `weak` vs `strong` single-try + the **`expected` gets overwritten on failure** behaviour; a CAS **spinlock**. All results verified (`4000000`, `99999`, `ok=1 v=20`, `400000`) |
| `03_relaxed_ordering.cpp` | 07 | **Demo 1:** 8 threads × relaxed `fetch_add` → **exact** count (relaxed = full atomicity, no ordering). **Demo 2:** relaxed publish/subscribe → **0 mismatches on x86** — and the output *says* that's not proof of correctness (formally UB; ARM/TSan would show it). `kRounds = 5000` (per-round thread create/join — kept small) |
| `04_acquire_release.cpp` | 08, 10 | **Demo 1:** `release` store + `acquire` load publish → **0 mismatches, guaranteed** (contrast `03` demo 2 which only *happens* to pass). **Demo 2:** a real **SPSC hand-off** — `static Message buf[N]`, `atomic<uint64_t> widx/ridx`, release/acquire — **2,000,000 msgs, checksum OK**. This is the folder-28 SPSC ring in miniature |
| `05_reordering_demo.cpp` | 06, 09, 12, 16 | ⚠️ **Store buffering, observed.** Persistent threads + `go`/`done` handshake, 200000 rounds, `r1==r2==0` counted for each order. **Measured (varies run-to-run): `relaxed` ~70–330, `release`/`acquire` ~1000–4200 (*more*, not fewer — acq/rel does NOT stop store→load), `seq_cst` always 0.** A real hardware+language memory-model demonstration |
| `06_memory_order_bench.cpp` | 04, 06, 09, 14 | Per-op ns, N = 100M single-threaded + a contended run. **Measured (this box, `-O2`): store `relaxed`/`release` ~0.72 ns vs `seq_cst` ~13 ns (~18×, the `mfence`/`xchg`); load ~0.73 ns *all* orders; `fetch_add` ~13 ns *all* orders; contended `fetch_add` (8 threads, 1 atomic) ~27 ns/op regardless of order.** Absolute ns drift with machine state — the *ratios* are the lesson |
| `07_aba_problem.cpp` | 15 | ⚠️ **ABA, reproduced deterministically.** `static Node g_pool[8]`, index free-list. BROKEN: a `phase` handshake forces `pop(0) pop(1) push(0)` between thread A's read and its CAS → **stale `CAS(head, 0, 1)` succeeds**, head points at an already-popped node. FIX: pack `{idx:32, tag:32}` in `atomic<uint64_t>`, bump tag every push → stale CAS **fails** (tag 0→3). (No DWCAS on this MinGW — hence 32+32 in a 64-bit word, not pointer+tag) |
| `08_litmus_tests.cpp` | 16 | **MP** and **IRIW** with persistent worker threads + `gen`/`ack` per-round handshake, 1,000,000 rounds. `mp_test<StoreMO,LoadMO>` / `iriw_test<StoreMO,LoadMO>` (separate params — `store` can't take `acquire`). **x86: 0 bad reads / 0 disagreements at every order** — the expected, informative result: x86 forbids all litmus reorderings *except* SB (see `05`) and is multi-copy-atomic (no IRIW) |

## Compile / run

```bash
./build.ps1 27-ATOMICS-MEMORY-MODEL/examples/01_atomic_counter.cpp     # Windows (MinGW)
make FILE=27-ATOMICS-MEMORY-MODEL/examples/01_atomic_counter.cpp        # Linux/Mac/Git-Bash (-pthread)
```

Benchmarks / reordering demos at **`-O2`** (required — `-O0` under-optimizes *and*
hides reordering behind slower code):
```bash
./build.ps1 fast 27-ATOMICS-MEMORY-MODEL/examples/05_reordering_demo.cpp
./build.ps1 fast 27-ATOMICS-MEMORY-MODEL/examples/06_memory_order_bench.cpp
```

Whole folder:
```bash
./build.ps1 folder 27-ATOMICS-MEMORY-MODEL       # 8/8 OK
```

**Race detection** (Linux — MinGW has no libtsan):
```bash
g++ -std=c++20 -O1 -g -fsanitize=thread 27-ATOMICS-MEMORY-MODEL/examples/01_atomic_counter.cpp -o ac && ./ac
```

## Measured (GCC 15.1.0, `-O2`, 8 logical cores, this box) — sample runs

### `01_atomic_counter.cpp`
```
non-atomic : 3104418  WRONG (lost updates)   ~160 ms   <- different every run (~3M / 16M)
atomic ++  : 16000000  OK                     ~432 ms   (seq_cst)
fetch_add  : 16000000  OK                     ~432 ms   (relaxed)
```

### `05_reordering_demo.cpp` (store buffering, 200000 rounds — run-to-run spread)
```
relaxed store + relaxed load  r1==r2==0 :   ~70 – 330 / 200000
release store + acquire load  r1==r2==0 : ~1000 – 4200 / 200000   <- MORE than relaxed
seq_cst store + seq_cst load  r1==r2==0 :          0 / 200000     <- always
```

### `06_memory_order_bench.cpp` (N = 100M single-thread; 8-thread contended)
```
store  relaxed :  0.73 ns      load relaxed : 0.73 ns      fetch_add relaxed : 13.0 ns
store  release :  0.73 ns      load acquire : 0.73 ns      fetch_add acq_rel : 13.1 ns
store  seq_cst : 13.0 ns       load seq_cst : 0.73 ns      fetch_add seq_cst : 13.0 ns
contended fetch_add relaxed : 27.1 ns/op    contended fetch_add seq_cst : 26.8 ns/op
```
(An earlier run on this same box measured the `seq_cst` store and the RMWs at
~6 ns. Absolute ns move with machine state — the **ratios** are stable and are
the point: `seq_cst` store ≈ 18× a relaxed store; loads and RMW *order* are free
on x86; contention dwarfs the order choice.)

### `08_litmus_tests.cpp` (MP + IRIW, 1,000,000 rounds)
```
MP   relaxed / rel-acq / seq_cst : 0 bad reads      (x86 forbids the MP reordering)
IRIW rel-acq / seq_cst           : 0 disagreements  (x86 is multi-copy-atomic)
```

## Notes / jaan-boojh kar cheezein

- **No `broken_on_purpose` file.** `01` (race), `05` (store buffering), `07`
  (ABA broken path), `08` (litmus) are *correct programs* that **demonstrate** a
  bug/effect you can observe — lost updates, `r1==r2==0`, a stale CAS succeeding,
  0-on-x86-but-model-allows.
- **`01` uses `volatile`** on the non-atomic counter **only** to stop `-O2` from
  coalescing `for(k) ++counter` into `counter += N` (which would hide the race).
  `volatile` is **not** atomic and fixes nothing — each `++` still races.
- **`03`/`04` demos use small round counts (`kRounds = 5000`)** because each round
  creates+joins threads (~µs each). `05`/`08` use **persistent worker threads +
  an atomic handshake**, so they run 200k–1M rounds cheaply.
- **DWCAS (`__atomic_*_16` / `cmpxchg16b`) does not link on this MinGW** —
  `std::atomic<16-byte struct>` fails to link (`is_always_lock_free` still works
  as a compile-time constant and returns 0). `07` therefore packs a 32-bit index
  + 32-bit tag into a `std::atomic<uint64_t>` for the ABA fix, not pointer+tag.
- **TSan / UBSan not available on this MinGW build.** The race example runs and
  shows the *effect*; on Linux `-fsanitize=thread` names the two `counter =
  counter + 1` accesses (command above).
- All 8 compile clean under `-Wall -Wextra -Wpedantic -Wshadow -Wconversion
  -Wsign-conversion -Wcast-align -Wunused -Wnull-dereference -Wdouble-promotion`
  (`./build.ps1 folder 27-ATOMICS-MEMORY-MODEL`).

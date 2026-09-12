# 10 — Latency numbers every programmer should know

Order-of-magnitude, modern x86-64 (~3 GHz → **1 cycle ≈ 0.3 ns**). Memorize the
*ratios*, not the digits. Consistent with folder `46/12 A1`, `31`, `32`, `37/14`.
Always **measure your box** — these are for back-of-envelope only.

---

## The core ladder

| Operation | Time | ≈ cycles | ×L1 |
|---|---|---|---|
| register / L0 | ~0 | <1 | — |
| **L1 cache hit** | **~1 ns** | ~4 | 1× |
| branch mispredict | ~3–5 ns | ~15–20 | ~4× |
| **L2 cache hit** | **~4 ns** | ~12 | ~4× |
| L3 cache hit | ~10–20 ns | ~40–60 | ~15× |
| atomic RMW / `LOCK` (uncontended) | ~5–20 ns | ~15–60 | — |
| mutex lock+unlock (uncontended) | ~15–25 ns | | — |
| **main memory (DRAM)** | **~60–100 ns** | ~200–300 | ~80× |
| DRAM + TLB miss (page walk) | ~100–300 ns | | — |
| `std::this_thread::yield` / syscall | ~0.1–0.3 µs | ~300–1000 | — |
| context switch (+ cold cache after) | ~1–5 µs | | ~2000× |
| kernel-bypass NIC wire→app | ~1 µs | | — |
| mutex lock (contended, wake a waiter) | ~1–3 µs | | — |
| **SSD random read (NVMe)** | ~10–100 µs | | ~50 000× |
| same-datacenter network RTT | ~10–100 µs | | |
| HDD seek | ~5–10 ms | | ~5 000 000× |
| cross-country (US) RTT | ~30–70 ms | | |
| internet round trip (intercontinental) | ~100–250 ms | | |

---

## Throughput / bandwidth (single core, modern box)

| | ~rate |
|---|---|
| sequential DRAM read | ~10–20 GB/s per core, ~40–100 GB/s aggregate |
| `memcpy` (in cache) | ~30–60 GB/s |
| L1 bandwidth | ~2× loads/cycle → ~100+ GB/s |
| simple ALU ops | ~1e9–1e10 / s (3–4 IPC × 3 GHz) |
| divisions (`idiv` 64-bit) | ~20–40 cycles each, **not** pipelined |
| `std::sort` 1e6 ints | ~30–60 ms at `-O2` |
| hash lookup (`unordered_map`, warm) | ~20–100 ns |
| `map` lookup (1e6 entries) | ~200 ns–1 µs (log n scattered misses) |

---

## Sizes to keep in your head

| Thing | Size |
|---|---|
| cache line | **64 B** (x86); 128 B (Apple M) |
| page | 4 KiB (huge: 2 MiB / 1 GiB) |
| L1d / L1i | ~32–48 KiB each, per core |
| L2 | ~256 KiB–2 MiB, per core |
| L3 (LLC) | ~1–4 MiB **per core**, shared |
| TLB (L1 dTLB) | ~64 entries (4K) → covers ~256 KiB |
| register file (visible) | 16 GPR + 16–32 vector |
| `int` / `long` (LP64) | 4 / 8 B — but use `int32_t`/`int64_t` when it must be exact |
| `void*` | 8 B (48-bit VA in practice) |

---

## Derived rules of thumb

- **A cache miss ≈ 100 simple instructions.** One pointer-chase per element in a
  hot loop caps you at ~10 M elem/s.
- **A syscall ≈ a cache miss ×3.** `std::endl` (flush = `write`) in a loop is death.
- **A context switch ≈ 20 cache misses + cold cache** (can be 10 000+ ns effective).
- **Branch mispredict ≈ one L2 hit.** A 50/50 unpredictable branch in a tight
  loop ≈ 4× slowdown (folder 31 ex 03: measured ~6–7×).
- **DRAM is ~80–100× L1.** Whether your working set fits in L2/L3 usually matters
  more than your algorithm's constant factor.
- **Same-DC RTT ≈ 1000× a local DRAM access.** Never chat over the network on a
  latency path if you can help it.
- **1 ns of light ≈ 30 cm** (~20 cm in fibre). NY↔Chicago one-way ≈ 6–7 ms over
  production fibre vs ~4 ms line-of-sight microwave — a ~2–3 ms edge, which is
  why microwave/millimetre-wave links exist for that route.

---

## Unit sanity

```
1 s  = 1e3 ms = 1e6 µs = 1e9 ns
1 GHz → 1 cycle = 1 ns ;  3 GHz → 1 cycle ≈ 0.33 ns
1 ns ≈ 3 cycles ≈ 0.3 m of light
```

## Next
→ [`11-ub-catalog.md`](11-ub-catalog.md)

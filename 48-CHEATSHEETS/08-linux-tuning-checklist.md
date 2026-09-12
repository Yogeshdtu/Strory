# 08 — HFT production tuning checklist

Deep: folders `29-LINUX-SYSTEMS`, `36-LOW-LATENCY-CPP` (18–20), `31/12–14`,
`35/06`, `42-HFT-NETWORKING`. Example box: 12-core, 1 socket, isolate cores 2–9,
keep 0–1 for the OS.

> Golden rule: the **first real tick must hit a fully warm, un-preemptible
> machine.** Tune the box, pin the threads, prefault the memory, warm every
> branch/cache line, then open.

---

## 1. BIOS / firmware

- [ ] Turbo / Turbo Boost **off** (or locked all-core) — no P-state transitions mid-burst
- [ ] C-states limited to **C1** (or C0) — deep sleep exit is µs of jitter
- [ ] Hyper-Threading / SMT **off** on trading cores (or leave siblings idle)
- [ ] Power profile = "max performance" / "OS control disabled"
- [ ] SpeedStep / Cool'n'Quiet off; fixed frequency
- [ ] NUMA: interleave off; prefer 1 socket for the hot path
- [ ] Disable unused devices (audio, extra NICs) — fewer IRQ sources

---

## 2. Kernel command line (`/etc/default/grub` → `GRUB_CMDLINE_LINUX`)

```
isolcpus=2-9 nohz_full=2-9 rcu_nocbs=2-9 irqaffinity=0,1
intel_pstate=disable processor.max_cstate=1 intel_idle.max_cstate=1 idle=poll
transparent_hugepage=never mce=ignore_ce audit=0 nosoftlockup
```

- `isolcpus` — scheduler won't put runnable tasks here (but timers/RCU still do)
- `nohz_full` — stop the 1 kHz scheduler tick on these cores
- `rcu_nocbs` — move RCU callbacks off these cores
- `irqaffinity` / `irqaffinity=0,1` — device IRQs default to the OS cores
- `idle=poll` — never enter an idle state on isolated cores (burns power, kills wake latency)

Verify after boot: `cat /sys/devices/system/cpu/isolated`, `cat /proc/cmdline`.

---

## 3. IRQs & kernel housekeeping

- [ ] `systemctl stop irqbalance && systemctl disable irqbalance`
- [ ] Pin every device IRQ to the OS cores:
      `for i in /proc/irq/*/smp_affinity_list; do echo 0-1 > $i; done` (best-effort)
- [ ] NIC RX/TX queue IRQs → OS cores (`/proc/irq/<n>/smp_affinity`)
- [ ] Move RCU/kworker threads: `tuna` / `taskset -p 3 <pid>` for `ksoftirqd`, `rcuo*`
- [ ] `echo 1 > /sys/devices/system/machine_check/...` — throttle MCE polling
- [ ] Disable `numad`, `tuned` auto-profiles, `thermald` on the hot box (or use `tuned-adm profile latency-performance` as a baseline)
- [ ] Writeback / dirty-page flush away from hot cores; `vm.stat_interval` up

---

## 4. Memory

- [ ] `mlockall(MCL_CURRENT | MCL_FUTURE)` at startup — no swap, no reclaim
- [ ] Raise `RLIMIT_MEMLOCK` (`/etc/security/limits.conf`: `* - memlock unlimited`) or `mlockall` fails
- [ ] **Prefault every buffer** — `mmap(..., MAP_POPULATE)` and/or touch one byte per 4 KiB page (a `memset` of the whole pool)
- [ ] Prefault the thread stacks (recurse/`alloca` a few KiB then return, or `pthread_attr_setstacksize` + touch)
- [ ] Explicit **huge pages** via hugetlbfs (`vm.nr_hugepages`, `mmap(MAP_HUGETLB)`) — **not** THP (`khugepaged` scan is a jitter source; `transparent_hugepage=never`)
- [ ] `vm.swappiness=0`; ideally no swap device at all
- [ ] `numactl --membind` / first-touch on the hot core's local node
- [ ] Zero allocation on the hot path — pools, arenas, ring buffers, fixed-capacity everything

Verify steady state: `perf stat -e page-faults ./engine` → **0** in the trading loop.

---

## 5. Threads & scheduling (runtime)

- [ ] `sched_setaffinity` — one isolated core per hot thread, alone
- [ ] Hot cores' SMT siblings idle (a sibling running anything steals L1/ports/ROB)
- [ ] `SCHED_FIFO` (prio ~80) **only if** you understand the hazards: a spinning FIFO thread that never yields can starve `rcuo`/`kworker` on that core → system hang. Pair with `nohz_full`+`rcu_nocbs`, and never `SCHED_FIFO` a thread that can spin forever without a `nanosleep(0)` escape hatch.
- [ ] `prctl(PR_SET_TIMERSLACK, 1)` for any thread that does sleep
- [ ] NIC RX-poll, feed-decode, strategy, risk+TX each on their **own** core
- [ ] Logging thread + aggregator on the **OS cores**, never a trading core
- [ ] Don't oversubscribe — total hot threads ≤ isolated cores

---

## 6. Network (folder 42)

- [ ] `TCP_NODELAY` on order-gateway sockets (kill Nagle)
- [ ] Kernel-bypass for market data: `SO_BUSY_POLL` → Onload → `ef_vi` → DPDK (pick the rung the latency budget needs)
- [ ] NIC: disable interrupt coalescing on the hot RX queue (`ethtool -C ethN rx-usecs 0 rx-frames 1`) — or pure busy-poll, no IRQ
- [ ] `ethtool -G` ring sizes tuned; `ethtool -K` offloads considered (LRO/GRO **off** for latency)
- [ ] Hardware RX **and** TX timestamping in the **same clock domain** (folder 42; fixes 30/09's relative-only jitter)
- [ ] A/B feed arbitration + sequence-gap → snapshot recovery wired before go-live
- [ ] `recvmmsg` / batched RX only off the critical path
- [ ] Static ARP / no DNS / no DHCP renew on the trading path

---

## 7. Warm-up (last thing before the open)

- [ ] Run **thousands of synthetic ticks** through the *whole* pipeline — every branch predictor, BTB, I-cache line, D-cache line, TLB entry hot
- [ ] Pre-touch every pool, ring, lookup table, and the log ring
- [ ] Open sockets, join multicast groups, complete the handshake — before the first real message
- [ ] Set the logger to **async / drop mode** (never block the hot thread)
- [ ] Confirm clocks: PTP/`phc2sys` locked, TSC invariant, `clock_gettime` vs `rdtsc` calibrated
- [ ] Freeze config — no file reads, no `getenv`, no lazy-init on the hot path after this point

---

## 8. Continuous verification

```bash
perf stat -e page-faults,context-switches,cpu-migrations -p <pid>   # want ~0 in steady state
perf sched latency -p <pid>                                          # scheduler wait
perf c2c record -p <pid> ; perf c2c report                           # false sharing (HITM)
cyclictest -m -p 95 -t -a 2-9 -i 200 -h 400                          # per-core wake jitter
grep -E 'ctxt|processes' /proc/stat                                  # trend
```

Track a **latency histogram** on the hot path (HdrHistogram-style, `O(1)` record,
percentiles off-path). Watch p99.9, not the mean. (folder 35, `10-latency-numbers.md`)

---

## Common gotchas

| "I did X" | but | fix |
|---|---|---|
| `mlockall` succeeded | untouched pages still fault once | + prefault (`MAP_POPULATE` / touch each page) |
| `isolcpus` set | timers, RCU, kworkers still on the core | + `nohz_full` + `rcu_nocbs` + IRQ affinity |
| pinned the thread | SMT sibling steals resources | idle the sibling / SMT off |
| THP enabled ("free perf") | `khugepaged` scan = periodic stall | `transparent_hugepage=never` + explicit hugetlbfs |
| `SCHED_FIFO` for priority | spinning FIFO thread starves per-core kernel work | `nohz_full`+`rcu_nocbs`, or don't use FIFO |
| turbo on for "more speed" | P-state transition after idle = jitter | fixed frequency, turbo off |

## Next
→ [`09-memory-ordering.md`](09-memory-ordering.md)

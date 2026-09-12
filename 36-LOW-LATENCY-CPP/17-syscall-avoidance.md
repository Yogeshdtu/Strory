# 17 — Syscall avoidance: busy-poll vs blocking, `io_uring` intro

## Prerequisites
- **`29-LINUX-SYSTEMS/01`** (syscall cost, vDSO), **`30-NETWORKING/08-11-12`**
  (busy-poll, epoll, zero-copy, `io_uring`), **`35-PROFILING-BENCHMARKING/16`**
- `examples/10_batching.cpp` (amortization)

## Yeh topic abhi kyun
Har syscall = ring 3 → ring 0 transition (~hundreds of ns fixed: mode switch
+ KPTI/retpoline + kernel entry), aur agar woh block kare to a context
switch + reschedule (~µs + cache cold). Hot path pe koi bhi syscall ek tail
source hai. Yeh lesson: count them, remove them, and the modern tools.

---

## The cost

| | Approx (this class of box) |
|---|---|
| vDSO call (`clock_gettime(CLOCK_MONOTONIC)`, `getpid`) — **no trap** | ~15–25 ns |
| a real syscall that returns immediately (`recv` with data ready, `write` to a pipe) | ~200–500 ns (mode switch + kernel path) |
| a syscall that **blocks** (`recv` no data, `futex` wait, `epoll_wait`) | µs+ : deschedule + reschedule + cache/TLB cold on return |
| `read`/`write` to disk | µs–ms |

Ratio syscall : userspace ≈ **100–300×** (29/01).

---

## Removing syscalls from the hot path

### 1. Busy-poll instead of block
```cpp
// blocking: recv() sleeps -> wake latency (µs) + cache cold + jitter
n = recv(fd, buf, len, 0);

// busy-poll on an isolated core: no syscall on the common path
while ((n = recv(fd, buf, len, MSG_DONTWAIT)) < 0 && errno == EAGAIN)
    _mm_pause();                          // or spin on a kernel-bypass NIC's RX ring
```
Trade-off: a busy-poll core is **100% CPU** always (power, heat, one core
gone). HFT accepts this on the trading cores. `SO_BUSY_POLL` /
`net.core.busy_poll` is a middle ground (kernel spins briefly before
sleeping). Kernel bypass (below) removes the syscall entirely.

### 2. Batch syscalls (lesson 16)
`sendmmsg`/`recvmmsg` (N packets, one syscall), `writev` (N buffers),
`io_uring` (N ops, one submit). Only on stages that are throughput-bound and
off the latency-critical path (e.g. logging, telemetry) — never batch the
order send.

### 3. Move the syscall off the hot thread
Hot thread pushes a fixed record into an SPSC ring; a **housekeeping thread**
does the `write`/`send`/`log` (35/16). The hot thread pays ~2 ns, not
~500 ns.

### 4. `io_uring` (Linux 5.1+)
Submission Queue (SQ) + Completion Queue (CQ), both mmap'd rings shared with
the kernel:
- You write an SQE (operation descriptor) into the SQ ring — **a memory
  write, not a syscall**.
- `io_uring_enter` submits a batch (one syscall for many ops) — **or**, with
  **`SQPOLL`**, a kernel thread polls the SQ ring and you make **zero
  syscalls** on submit.
- Completions appear in the CQ ring — poll it, no syscall.
- Supports `recv`/`send`/`read`/`write`/`accept`/timeouts/…; fixed
  (pre-registered) buffers and files avoid per-op refcount/lookup cost.
Net: with `SQPOLL` + fixed buffers, network/disk I/O with **no syscalls on
the hot path** — the modern alternative to raw busy-poll where you still
want the kernel's stack.

### 5. Kernel bypass (the extreme)
DPDK / Solarflare Onload / ef_vi / VMA / AF_XDP — the NIC's RX/TX rings are
mapped into your process; you poll them directly. **Zero kernel involvement**
on the packet path. You take on: the driver, the (partial) TCP/IP stack,
buffer management, no kernel firewall/routing. Adopt only when the
kernel path's ~microsecond is genuinely in your budget's way and you have
the engineering to own the stack (30/12).

---

## Counting syscalls

```bash
strace -c -f ./app                 # summary: count + time per syscall
strace -T -f -e trace=network ./app  # per-call timing
perf trace ./app                   # perf's strace, lower overhead
perf stat -e 'syscalls:sys_enter_*' ./app
bpftrace -e 'tracepoint:raw_syscalls:sys_enter /pid == PID/ { @[probe] = count(); }'
```
Goal: **zero syscalls per hot-path iteration** in steady state (after
warm-up). Any that remain → move to a housekeeping thread or `io_uring`+SQPOLL
or bypass.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `clock_gettime` on the hot path (non-vDSO)
`CLOCK_MONOTONIC` is vDSO (~20 ns) — OK. `CLOCK_MONOTONIC_RAW` is a **real
syscall** (~250 ns). For sub-100 ns timestamps use `rdtsc` (35/03).

### Trap 2 — logging with a `write` per line
~1 µs syscall + maybe a lock + maybe `fsync`. Ring → logger thread; `fsync`
on a timer only.

### Trap 3 — busy-poll on a shared (non-isolated) core
100% CPU on a core the OS also needs → starves housekeeping → box-wide
jitter. Busy-poll only on an isolated, dedicated core (lesson 19).

### Trap 4 — `io_uring` without `SQPOLL` and calling `enter` per op
That's just a syscall per op again. Batch submits, or use `SQPOLL`.

### Trap 5 — kernel bypass by default
It's a large engineering commitment (own the stack, no kernel services).
Only when measured to be necessary.

### Trap 6 — `malloc`/`free` doing `mmap`/`madvise` under the hood
A "userspace" call that becomes a syscall on the slow path (lesson 04).
Pools/arenas eliminate it.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "a syscall is ~like a function call" | 100–300× a userspace call; blocking one = µs + reschedule |
| "`clock_gettime` is a syscall" | `CLOCK_MONOTONIC` is vDSO (~20 ns); `_RAW` is a real syscall |
| "busy-poll anywhere for low latency" | only on an isolated dedicated core (100% CPU cost) |
| "`io_uring` = automatically no syscalls" | need `SQPOLL` + CQ polling; else it's submit-per-op |
| "kernel bypass is the goal" | only when the ~µs kernel path is truly in the budget |

---

## Exercises

1. Ek strategy thread har market-data message process karne ke baad
   `clock_gettime(CLOCK_MONOTONIC_RAW, &ts)` call karta hai timestamping ke
   liye, aur `write(log_fd, line, len)` agar ek signal fire hota (~1% msgs).
   `strace -c` dikhata do syscalls dominate kar rahe. Dono fix.

   <details><summary>Answer</summary>

   (1) **`clock_gettime(CLOCK_MONOTONIC_RAW)` per message** — `_RAW` bypasses
   the vDSO fast path and is a genuine syscall (~250 ns) *every* message. If
   the timestamp is for latency instrumentation, use **`rdtsc`** (fenced,
   calibrated once at startup — 35/03) → ~1–20 ns, no kernel entry. If you
   need a real wall-clock ns value, use plain **`CLOCK_MONOTONIC`** (vDSO,
   ~20 ns, no trap) — the `_RAW` variant (not NTP-adjusted) is rarely
   needed and never worth the syscall on a hot path.
   (2) **`write()` on the 1% signal path** — even at 1% of messages, that's
   a ~1 µs syscall (plus buffering, plus a possible page cache stall) on the
   *latency-critical* path (a signal fired → you're about to trade). Move it
   off: push a fixed-size log record into an **SPSC ring**; a housekeeping
   thread drains and writes (batched — lesson 16). The strategy thread pays
   ~2 ns for the ring push. The log still lands on disk within milliseconds,
   which is fine for a log. `fsync` on a timer / at shutdown, never inline.
   After both fixes `strace -c` on the strategy thread should show ~zero
   syscalls in steady state.
   </details>

2. Ek team `io_uring` adopt karti hai socket I/O ke liye aur expect karti
   hai "no more syscalls," par `perf trace` abhi bhi ~1 `io_uring_enter` per
   `recv` dikhata. Kya missing hai, aur alternative?

   <details><summary>Answer</summary>

   By default, after placing SQEs in the submission ring you must call
   `io_uring_enter()` to tell the kernel to process them — that's **one
   syscall per submit**. If they submit one `recv` SQE and immediately call
   `enter`, it's exactly one syscall per `recv`, same as calling `recv()`
   directly (slightly worse, even, due to the ring bookkeeping).
   What's missing:
   (1) **Batch submissions** — queue many SQEs, one `io_uring_enter` for all
   of them (amortize — lesson 16). Works when you have multiple I/Os in
   flight.
   (2) **`IORING_SETUP_SQPOLL`** — a kernel thread polls the SQ ring, so
   placing an SQE is just a memory write and **you never call
   `io_uring_enter` at all** on submit. The kernel thread costs one core
   (it can be pinned and shared across rings, or idled after inactivity).
   (3) **Poll the CQ ring** for completions (a memory read of the ring head/
   tail) instead of `io_uring_enter(..., GETEVENTS)` or `io_uring_wait_cqe`
   (which blocks / syscalls).
   (4) **Register fixed buffers and files** (`IORING_REGISTER_BUFFERS`/
   `_FILES`) so per-op there's no fd lookup / buffer pin-unpin cost.
   With SQPOLL + CQ polling + fixed buffers/files, the steady-state hot path
   has **zero syscalls** — the goal. Trade-off: SQPOLL burns a core; on a
   pure market-data ingest path many shops go all the way to kernel bypass
   (ef_vi / Onload) instead.
   </details>

---

## Interview questions

1. Syscall cost breakdown — mode switch, KPTI, blocking → reschedule; the 100–300× ratio.
2. Busy-poll vs blocking — the trade-off, and where busy-poll is acceptable.
3. `io_uring` — SQ/CQ rings, `SQPOLL`, fixed buffers; how to get to zero syscalls.
4. `clock_gettime` variants — which is vDSO, which is a syscall.
5. Moving a hot-path syscall to a housekeeping thread — the pattern and the cost.
6. Kernel bypass — what you gain, what you take on, when it's justified.

---

## Next
→ [`18-page-fault-avoidance.md`](18-page-fault-avoidance.md)

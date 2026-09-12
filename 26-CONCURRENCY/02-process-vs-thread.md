# 02 — Process vs thread

## Prerequisites
- `01-concurrency-vs-parallelism.md`, `14-MEMORY` (address space, stack/heap)
- `24-COMPILATION-LINKING` file 09 (loader, `.so`)

## Yeh topic abhi kyun
Concurrency ke do units hain: **process** (apna address space) aur **thread**
(shared address space, apni stack). Inka farq — kya shared hai, creation kitni
mehngi, communication kaisa — decide karta hai kaunsa use karein, aur kyun threads
mein "shared mutable data" itni badi problem hai.

---

## Process

Ek **process** = ek running program instance. Uske paas apna:
- **Address space** — apna virtual memory (code, data, heap, stacks). Doosre
  process ki memory dikhti hi nahi (kernel isolation).
- **File descriptors**, signal handlers, working directory, user/group id.
- **PID**.
- **Kam se kam ek thread** (the "main" thread).

Do processes communicate karte hain via **IPC**: pipes, sockets, shared memory
(`shm_open` / `mmap`), message queues, signals.

## Thread

Ek **thread** = ek process ke andar ek independent execution flow. Ek process ke
threads **share** karte hain:
- **Poora address space** — same heap, same globals, same code. Ek thread jo
  pointer likhe, doosra padh sakta.
- File descriptors, signal dispositions, cwd, pid.

Har thread ka apna:
- **Stack** (usually 1–8 MB, `ulimit -s` / `pthread_attr_setstacksize`).
- **Registers** / program counter (context).
- **`thread_local` storage** (file 15).
- **TID** (thread id).

```
Process
├── shared: code, .data/.bss, heap, fds, cwd
├── Thread 1 ──► own: stack, registers, TLS
├── Thread 2 ──► own: stack, registers, TLS
└── Thread 3 ──► own: stack, registers, TLS
```

---

## Kya shared, kya private — the table

| Resource | Processes | Threads (same process) |
|---|---|---|
| Address space (heap, globals, code) | **isolated** | **shared** |
| Stack | own | **own** (per thread) |
| Registers / PC | own | **own** |
| File descriptors | own | shared |
| `thread_local` | own | own |
| Signal handlers | own | shared (delivery is per-process-ish) |
| PID | own | shared (same PID, different TID) |

**"Shared address space" is why threads are powerful (cheap communication —
just a pointer) and dangerous (data races, files 05–09).**

---

## Creation cost

| | Process (`fork` / `posix_spawn`) | Thread (`pthread_create` / `std::thread`) |
|---|---|---|
| What's created | new address space (COW page tables), new PCB | new stack + TCB, entry in the process |
| Rough cost | ~50–500 µs | ~10–50 µs |
| Memory | page tables + kernel structs; COW so data pages lazily | ~stack reservation (often lazy) + small kernel struct |
| Context switch to it | address space switch → **TLB flush** (unless PCID/ASID) | no address-space switch → cheaper |

Threads are ~10× cheaper to create and cheaper to switch between (no TLB flush).
But **both are too expensive to create per-request** — use a **pool** (file 13).

`fork()` — COW: child gets a copy-on-write snapshot of the parent's pages;
actual copies happen only on write. `vfork` / `posix_spawn` — faster when you'll
`exec` immediately.

---

## Context switch — what it costs

- **Thread → thread (same process):** save/restore registers, kernel bookkeeping,
  scheduler run. ~1–3 µs direct. Plus **indirect**: the new thread's working set
  isn't in L1/L2 → cache misses for a while (can be 10s of µs of effective cost).
- **Thread → thread (different process):** + address space switch → **TLB flush**
  (or tagged-TLB miss), page-table base swap. More expensive.
- **Voluntary** (blocking on a lock / I/O) vs **involuntary** (timeslice expired,
  higher-prio task).

`perf stat` shows `context-switches` and `cpu-migrations`. On the HFT hot path you
want **zero** of both (isolated core, `SCHED_FIFO`, no blocking — folder 41).

---

## Kab process, kab thread

**Thread when:**
- Tasks need to share large mutable data cheaply (a shared cache, an order book).
- Low-latency communication (a pointer, a lock-free queue).
- Same trust domain (a bug in one thread can corrupt the whole process).

**Process when:**
- **Isolation / fault containment** — one crash shouldn't take down the others
  (browser tab per process; a market-data feed handler per exchange as a separate
  process so one bad feed can't wedge the engine).
- **Security boundary** — different privileges.
- **Independent lifecycle / deploy** — restart one without the others.
- Different languages / runtimes.

Common HFT shape: **multiple processes** (feed handlers, strategy engine, order
gateway, risk), each **single- or few-threaded**, communicating over **shared
memory ring buffers** — isolation *and* speed.

---

## Andar kya hota hai

- `std::thread` → `pthread_create` (Linux) / `CreateThread` (Windows) → the kernel
  allocates a task struct, a kernel stack, and (lazily) the user stack; adds it to
  the run queue.
- All threads of a process share the same `mm_struct` (memory descriptor) — same
  page tables → a write by one is immediately visible to another (modulo caching
  and the memory model — folder 27).
- Scheduler treats each thread as a schedulable entity; `hardware_concurrency()`
  = logical CPUs (cores × SMT).
- `fork()` → new `task_struct` + a **copied** `mm_struct` with all pages marked
  read-only for COW; a write faults and copies that one page.

---

## > **HFT relevance**
> - **Process-per-component for isolation** — feed handler, book builder, strategy,
>   order gateway, risk each a separate process. A crash / stall / memory
>   corruption in one is contained; you can restart it while the rest run.
> - **Shared-memory ring buffers between processes** (`shm_open` + `mmap` + a
>   lock-free SPSC queue in that region) — you get process isolation *and*
>   thread-speed communication (folder 28, 41, 42).
> - **Threads within a process for the tightest coupling** — the hot thread + a
>   few helpers sharing an arena, communicating lock-free.
> - **Never create a thread or process per event** — pools, or a fixed set of
>   long-lived pinned threads. Creation + context switches are jitter.
> - **Core pinning + isolation** (`isolcpus`, `taskset`, `SCHED_FIFO`) so the hot
>   thread/process is never involuntarily switched out (folder 29, 41).

---

## Hands-on

```bash
./build.ps1 26-CONCURRENCY/examples/01_first_thread.cpp
```

- `std::thread::hardware_concurrency()` on your machine.
- (Linux) `cat /proc/self/status | grep Threads` before/after spawning.
- (Linux) `perf stat -e context-switches,cpu-migrations ./your_prog`.
- `ulimit -s` — default thread stack size.

---

## ⚠️ Traps

### Trap 1 — "threads are free"
~10–50 µs to create, ~1–3 µs to switch (+ cache effects). Per-request threads =
death by a thousand switches. Pool them.

### Trap 2 — assuming a `fork()` child has copied all memory eagerly
COW — copies happen on write. But `fork()` in a multi-threaded process is
dangerous (only the calling thread survives in the child; locks held by other
threads stay held → deadlock). `fork` + `exec` only, or `posix_spawn`.

### Trap 3 — sharing a `std::mutex` / `std::atomic` across processes via naive shared memory
`std::mutex` isn't process-shared by default (needs `PTHREAD_PROCESS_SHARED`).
`std::atomic` **is** usable in shared memory if it's lock-free and lives in the
mapped region.

### Trap 4 — thread stack overflow
Deep recursion or a big `alloca`/VLA on an 8 MB stack → silent corruption / crash.
Heap for big buffers; bound recursion.

### Trap 5 — signals + threads
A signal can be delivered to any thread (unless masked). Signal handlers +
threads = a minefield; prefer `signalfd` / a dedicated signal thread.

### Trap 6 — expecting process isolation from threads
A wild pointer write in one thread corrupts the whole process. Threads share
everything; only processes isolate.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "threads have separate memory" | Shared address space; only stack/registers/TLS are per-thread |
| "processes and threads cost about the same" | Thread ~10× cheaper to create, cheaper to switch (no TLB flush) |
| "context switch is basically free" | ~1–3 µs direct + cache/TLB warm-up cost after |
| "`fork()` copies all memory" | Copy-on-write — pages copied lazily on write |
| "one thread crashing kills just that thread" | It's UB in the whole process — often a full crash |
| "`std::mutex` works across processes" | Not by default — needs process-shared attribute / a different primitive |

---

## Exercises

1. **Shared or private:** for two threads of one process — the heap, a global
   `int`, each thread's `int local;`, a file descriptor, a `thread_local int`.

   <details><summary>Answer</summary>

   Heap — shared. Global `int` — shared. `local` — private (each thread's stack).
   Fd — shared. `thread_local int` — private (per-thread).
   </details>

2. **Cost order:** rank by creation cost: `std::thread`, `fork()`, a pool
   `submit()`, `posix_spawn` + `exec`.

   <details><summary>Answer</summary>

   Pool `submit()` (~ns–µs, just enqueue) < `std::thread` (~10–50 µs) < `fork()`
   (~50–200 µs, page tables) ≈ `posix_spawn`+`exec` (fork-ish + loading a new
   binary). The pool wins because it reuses threads.
   </details>

3. **Isolation:** why might an HFT shop run each exchange's feed handler as a
   separate process rather than a thread?

   <details><summary>Answer</summary>

   Fault containment: a malformed feed, a parser bug, or a memory corruption in
   one handler can't crash or corrupt the strategy engine / other feeds. You can
   also restart / redeploy one handler independently. Communication is still fast
   via a shared-memory ring buffer.
   </details>

4. **`fork()` in a threaded process:** you `fork()` while another thread holds a
   `malloc` lock. What can happen in the child?

   <details><summary>Answer</summary>

   The child has only the calling thread, but the `malloc` lock is still marked
   held (by a thread that doesn't exist in the child) → the next `malloc` in the
   child deadlocks. That's why `fork()` in a multi-threaded program must be
   followed immediately by `exec` (which replaces the address space), or use
   `posix_spawn`.
   </details>

5. **Context switches:** on the HFT hot path, what's your target for
   `context-switches` and `cpu-migrations` per second, and how do you get there?

   <details><summary>Answer</summary>

   Zero (or as close as possible). Pin the thread to an isolated core (`isolcpus`
   / `taskset`), run it `SCHED_FIFO`/`SCHED_RR` so the scheduler won't preempt it
   for normal tasks, keep the sibling hyperthread idle or disabled, and never
   block (no locks that can sleep, no syscalls) on that thread.
   </details>

---

## Interview questions

1. Process vs thread — kya shared, kya private.
2. Thread creation vs process creation cost — kyun farq (address space, TLB)?
3. Context switch cost — direct aur indirect.
4. `fork()` COW — kya matlab, multi-threaded process mein kya khatra?
5. Kab process choose karein over thread (isolation, security, lifecycle)?
6. Threads shared address space — power aur danger dono kyun?
7. HFT: process-per-component + shared-memory ring buffer — kyun best of both?

---

## Next
→ [`03-std-thread.md`](03-std-thread.md)

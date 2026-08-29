# 29 — LINUX SYSTEMS PROGRAMMING (PHASE 18)

## Prerequisites
`26-CONCURRENCY`, `14-MEMORY`

## Yeh folder kyun
HFT Linux pe chalta hai. Bas. Windows pe koi trading system nahi hai.

Yahan hum OS ke saath **seedha** baat karna seekhenge — aur samjhenge ki har syscall
kitni mehngi hai.

## Is folder ki files

| # | File | Kya seekhoge |
|---|------|--------------|
| 01 | `01-linux-architecture.md` | Kernel vs userspace, ring 0 vs ring 3, kernel ka role |
| 02 | `02-syscalls.md` | **Syscall kya hai**, user→kernel transition, `strace`, **cost measured** |
| 03 | `03-processes.md` | `fork`, `exec`, `wait`, process tree, zombie/orphan processes |
| 04 | `04-signals.md` | Signal handling, async-signal-safety, `sigaction`, HFT mein signals ka use |
| 05 | `05-file-descriptors.md` | fd table, `open`/`read`/`write`/`close`, fd inheritance, `dup2` |
| 06 | `06-proc-and-sys.md` | `/proc` aur `/sys` — process introspection, tuning knobs |
| 07 | `07-mmap.md` | **`mmap`** — file mapping, anonymous memory, shared mappings, huge pages |
| 08 | `08-shared-memory.md` | POSIX shm, `shm_open`, inter-process communication, **HFT: shm rings** |
| 09 | `09-pipes-and-fifos.md` | Pipes, named pipes, Unix domain sockets |
| 10 | `10-cpu-scheduling.md` | CFS, `SCHED_FIFO`, `SCHED_RR`, priorities, `nice`, real-time threads |
| 11 | `11-cpu-affinity.md` | **`taskset`, `sched_setaffinity`, `isolcpus`, `nohz_full`** — core isolation |
| 12 | `12-huge-pages.md` | 4KB vs 2MB vs 1GB pages, TLB pressure, `transparent_hugepage`, explicit hugepages |
| 13 | `13-page-faults-and-mlock.md` | **Page faults ki cost**, `mlockall`, memory pre-faulting/warming |
| 14 | `14-numa.md` | NUMA architecture, `numactl`, local vs remote memory, allocation policy |
| 15 | `15-irq-affinity.md` | Interrupt handling, IRQ pinning, softirqs, NIC interrupts |
| 16 | `16-clocks-and-timers.md` | `clock_gettime`, `CLOCK_MONOTONIC` vs `REALTIME`, TSC, vDSO |
| 17 | `17-cgroups-and-limits.md` | cgroups, `ulimit`, resource isolation |
| 18 | `18-hft-tuning-checklist.md` | **Poora production tuning checklist** — kernel params, BIOS, boot flags |
| 19 | `19-exercises.md` | Practice + system programming tasks |

## Examples

| File | Kya |
|---|---|
| `examples/01_syscall_cost.cpp` | Syscall latency measure karo |
| `examples/02_fork_exec.cpp` | Process creation |
| `examples/03_file_io_raw.cpp` | Raw fd I/O vs iostream |
| `examples/04_mmap_demo.cpp` | File mapping aur anonymous mmap |
| `examples/05_shared_memory.cpp` | Two-process shared memory |
| `examples/06_cpu_affinity.cpp` | Thread ko core pe pin karna |
| `examples/07_page_faults.cpp` | Page fault latency + mlockall se fix |
| `examples/08_hugepages.cpp` | Huge pages ka TLB pe asar |
| `examples/09_clock_comparison.cpp` | chrono vs clock_gettime vs rdtsc |
| `examples/10_realtime_thread.cpp` | SCHED_FIFO thread |

## Time
3–4 hafte

## Status
⏳ **Yeh folder abhi syllabus stage pe hai.** Upar ki file list poori plan hai —
content agle batch mein aayega.

## Next
→ [`../30-NETWORKING/00-README.md`](../30-NETWORKING/00-README.md)

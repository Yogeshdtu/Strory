# Examples — Folder 29 (Linux systems programming)

> **Sab examples `*.linux.cpp` hain — Linux-only.** Yeh repo Windows/MinGW pe
> develop hota hai, par folder 29/30 ke topics (syscalls, `mmap`, `fork`,
> `epoll`, POSIX sockets, `sched_setaffinity` ...) MinGW pe compile hi nahi
> hote. `./build.ps1 folder 29-LINUX-SYSTEMS` inhe **SKIP** karta hai
> (`SKIP (linux-only)`), aur `./build.ps1 checkall` bhi. Verify karne ke liye
> Linux ya WSL chahiye.

## Compile / run (Linux ya WSL)

```bash
cd 29-LINUX-SYSTEMS/examples
g++ -std=c++20 -O2 -Wall -Wextra -pthread 01_syscall_cost.linux.cpp -o syscall_cost && ./syscall_cost
# kuch ko extra chahiye:
g++ ... 05_shared_memory.linux.cpp -o shm -lrt          # POSIX shm (purane glibc)
sudo ./10_realtime_thread            # SCHED_FIFO ke liye CAP_SYS_NICE / root
echo 512 | sudo tee /proc/sys/vm/nr_hugepages          # 08 ke liye pool
```

`strace` / `perf` har example ke saath: `strace -c ./x`, `perf stat ./x`.

## Examples

| File | Lesson(s) | Kya dikhata hai |
|---|---|---|
| `01_syscall_cost.linux.cpp` | 02 | userspace call vs vDSO `clock_gettime` vs raw `syscall(SYS_getpid)` trap. **Ratio sikhata:** syscall trap ~100–300× a userspace call, ~12–25× a vDSO call. `mitigations` ka asar. |
| `02_fork_exec.linux.cpp` | 03 | `fork()` (one call, two returns) + COW cost (best-of-200 `fork`+`_exit`), `execvp`, `waitpid` status, orphan → init reparent. |
| `03_file_io_raw.linux.cpp` | 05, 02 | 200k lines 3 ways: raw `write()` per line (~200k syscalls) vs userspace-buffered `write()` (~10 syscalls) vs `std::ofstream`. **~20–40× gap.** `strace -c` se count. |
| `04_mmap_demo.linux.cpp` | 07, 13 | file mapping (file = array, no `read`/`write` calls); anonymous mapping + **lazy fault cost** (first touch ~200–800 ns/page vs warm ~10 ns/page); `MAP_POPULATE` shifts faults to `mmap` time. |
| `05_shared_memory.linux.cpp` | 08 | `shm_open`+`mmap`+`fork` → two processes, one `std::atomic<uint64_t>` counter hand-off, monotonic-verify (gaps == 0). Cross-process `release`/`acquire`. ~15 ns/update (cache-line bounce). |
| `06_cpu_affinity.linux.cpp` | 11, 10 | fixed work, unpinned vs `pthread_setaffinity_np` core 1 vs core 3. **p50 barely moves; p99.9/max tighten a lot** — pinning kills the tail. Best with an isolated core. |
| `07_page_faults.linux.cpp` | 13 | 128 MiB buffer: cold pass (minor-fault per page, `getrusage` count) vs warm pass (0 faults) vs `mlockall`+`MAP_POPULATE`+`memset` (first real touch already fault-free). |
| `08_hugepages.linux.cpp` | 12 | 512 MiB single-cycle (Sattolo) pointer-chase, 4 KiB (`MADV_NOHUGEPAGE`) vs 2 MiB (`MAP_HUGETLB`, THP `madvise` fallback). **~1.1–1.5× speedup** from reduced TLB misses (workload-dependent). |
| `09_clock_comparison.linux.cpp` | 16, 02 | ns/read for `CLOCK_MONOTONIC` / `_RAW` / `_COARSE` / `REALTIME` / `steady_clock` / `rdtscp`, + min observable step. **`_RAW` is a syscall (~250+ ns); the rest are vDSO (~20 ns); `_COARSE` ~6 ns but ~1 ms granular.** |
| `10_realtime_thread.linux.cpp` | 10 | periodic 200 µs wake (`clock_nanosleep` ABSTIME), `SCHED_OTHER` vs `SCHED_FIFO` prio 80 (pinned). **FIFO tightens p99/p99.9/max**; note the starvation caveat + "isolated CFS busy-poll often does the same". |

## Expected numbers — sab TYPICAL, aapke box pe NAHI nape gaye

Har `.linux.cpp` file ke neeche ek `EXPECTED OUTPUT` comment block hai with
representative Linux x86-64 numbers **explicitly labelled "not measured on your
machine"**. Yeh CLAUDE.md Rule 2 ka honest treatment hai: is Windows box pe
Linux syscalls chal hi nahi sakte, to inhe fabricate karke "measured" nahi
bola gaya — published/typical ranges diye gaye hain, saaf disclaimer ke saath.
Linux/WSL pe khud chalao aur asli numbers Part D exercises (`19-exercises.md`)
mein likho.

**Kyun ratios pe focus, absolute pe nahi:** absolute ns kernel version,
mitigations, `clocksource`, CPU, aur load se badalte hain. Lesson hamesha ratio
mein hai — syscall vs userspace (~100–300×), cold vs warm fault (~20–60×),
unpinned tail vs pinned tail (~5–50×), vDSO vs trap (~12–25×).

## Notes / jaan-boojh kar cheezein

- **`.linux.cpp` naming** ek naya convention hai (yeh folder + folder 30). Isse
  `build.ps1` `folder`/`checkall` mein `SKIP (linux-only)` bucket milta —
  `FAIL (real)` nahi. Change `build.ps1` mein ~4 lines (see its header NOTE).
- **`#define _GNU_SOURCE 1`** har file ki pehli line hai — strict `-std=c++20`
  ke saath glibc `MAP_POPULATE` / `MAP_HUGETLB` / `MAP_ANONYMOUS` /
  `CLOCK_MONOTONIC_RAW` / `pthread_setaffinity_np` jaise extensions ko expose
  karne ke liye. (`-std=gnu++20` use karo to yeh `#define` optional hai.)
- **No `broken_on_purpose` file** yahan. Har example ek correct program hai jo
  ek OS property demonstrate/measure karta.
- **`01`, `09`** `rdtscp` use karte — x86-64 only. Non-x86 pe compile fail hoga
  (aur waise bhi HFT x86-64/ARM64; yeh examples x86-64 assume karte).
- **`08` ka `build_chase`** Sattolo's algorithm se ek single n-cycle banata
  (naive shuffle multiple cycles / fixed points de sakti thi → poora working
  set cover nahi hota).
- **`10` SCHED_FIFO** ke liye privilege chahiye — bina root/`CAP_SYS_NICE` ke
  woh sirf `SCHED_OTHER` wala number deta (aur ek `perror` line).
- **`05` `-lrt`** kuch distros pe chahiye (`shm_open`/`clock_*` in librt on
  older glibc); modern glibc mein librt empty stub hai, `-lrt` harmless.
- **WSL2 caveat:** examples chalenge, par timer/scheduler/IRQ behaviour host
  Windows pe depend karta — `06`, `10` ke jitter numbers wahan bharosemand
  nahi. `01`, `03`, `04`, `07`, `09` reasonably representative.

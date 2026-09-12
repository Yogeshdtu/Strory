# 14 — NUMA architecture: multi-socket, interconnect, memory locality

## Prerequisites
- `29-LINUX-SYSTEMS` file 14 (NUMA from the OS/API side — `numactl`, first-touch, `mbind`)
- `01-how-cpu-works.md` (memory hierarchy)

## Yeh topic abhi kyun
Folder 29 file 14 ne NUMA ko **OS/software** angle se cover kiya (first-touch,
`numactl`, `numa_balancing=0`, pipeline ko ek node pe rakho). Yeh lesson wahi
cheez **hardware** angle se: sockets, interconnects (UPI/Infinity Fabric),
memory controllers, aur *why* remote access mehnga hai — plus modern
single-socket chips ke andar bhi NUMA-jaisa behaviour (chiplets, sub-NUMA
clustering). Yeh folder 32 (cache) ke saath milke "memory kahan se aati hai aur
kitni door hai" ka poora picture deta.

---

## The hardware picture

```
   +================ SOCKET 0 =================+   UPI / IF   +================ SOCKET 1 =================+
   |  core core core core   L3 (LLC, shared)   |<==========>|  core core core core   L3 (shared)       |
   |  core core core core   ring / mesh        |  ~30-40    |  core core core core   ring / mesh       |
   |                                            |  GB/s     |                                          |
   |  integrated Memory Controller -> DDR5      |  link(s)  |  integrated Memory Controller -> DDR5     |
   |  (NUMA node 0 memory)                       |           |  (NUMA node 1 memory)                     |
   +============================================+           +==========================================+
      local access ~80-100 ns                                 remote access (node0 core -> node1 mem)
                                                              ~130-200 ns + shared-link contention
```

- Each socket has its **own memory controller** and DIMMs → its own **NUMA
  node**. A core reading its socket's DRAM = **local** (~80–100 ns).
- A core reading the *other* socket's DRAM = **remote**: the request crosses the
  **interconnect** (Intel **UPI** — Ultra Path Interconnect; AMD **Infinity
  Fabric**), the remote memory controller services it, the data comes back
  across the link → ~1.5–2.2× local latency, plus the link is **shared
  bandwidth** so it degrades under load.
- **Cache coherence** spans sockets too (a line can be Modified in socket 1's
  L3; socket 0 asking for it triggers a cross-socket snoop) — folder 32.

---

## NUMA inside one socket (modern reality)

You don't need two sockets to have NUMA effects:

| Design | NUMA-like behaviour |
|---|---|
| **AMD chiplets (CCD/CCX)** | a Zen CPU is several "core complex dies" glued by Infinity Fabric on-package. Cross-CCD cache-to-cache and memory access is slower than within-CCD. Zen 2 (this box's family) has separate CCXs sharing L3 within a CCX only. |
| **Intel Sub-NUMA Clustering (SNC)** | a big monolithic die is split into 2 (or 4) NUMA nodes, each "owning" half the L3 slices + memory channels — lower latency to the local half. Enabled in BIOS; the OS sees 2 nodes per socket. |
| **Intel cluster-on-die / mesh** | large mesh dies have non-uniform core→L3-slice→memory-controller distances even without SNC. |
| **CXL memory** (emerging) | memory attached over CXL is a *further* NUMA tier (~200–400+ ns) |

So "keep the hot pipeline on one NUMA node" (folder 29 file 14) sometimes means
"one CCX" or "one SNC cluster", not just "one socket". `numactl -H` +
`lscpu -e=CPU,NODE` tell you the real topology.

---

## The costs, concretely

| Access | Latency (ballpark) | Notes |
|---|---|---|
| L1 hit | ~4–5 cycles | folder 32 |
| L2 hit | ~12–15 cycles | |
| L3 hit (local slice) | ~40 cycles | |
| L3 hit (remote slice / other CCX) | ~60–100 cycles | on-package fabric hop |
| **local DRAM** | ~80–100 ns (~300–400 cycles) | |
| **remote DRAM** (other socket) | ~130–200 ns | + shared UPI/IF bandwidth |
| cross-socket cache-to-cache (snoop + transfer) | ~100–300 ns | a "dirty" line owned by the other socket |
| CXL memory | ~200–400+ ns | |

For HFT: a remote-DRAM access on the hot path is ~2× a local one, and a
cross-socket coherence miss (two threads on different sockets touching the same
cache line — folder 28 false sharing, now cross-socket) is brutal *and* jittery
because it contends for the interconnect.

---

## The NIC is on a socket too

A PCIe device (the NIC) is attached to **one socket's** PCIe lanes → it has a
NUMA node (`/sys/class/net/eth0/device/numa_node`). Incoming packet data is
DMA'd into memory near *that* socket. If your feed handler runs on the *other*
socket, every packet read is a remote access + interconnect traffic on the
RX-hot path (folder 30 file 15). **Rule: NIC, hot cores, and their memory all on
one node.**

---

## What you do about it (recap folder 29 file 14, hardware lens)

| Action | Why (hardware) |
|---|---|
| Pin the whole feed→book→strategy→gateway pipeline to **one node's cores** | no interconnect hop between stages |
| Bind their memory to that node (`numactl --membind`, `mbind`, per-node arenas) | loads stay local ~100 ns not ~180 ns |
| **Parallel first-touch** — the owning thread zeroes/warms its buffers (folder 29 file 13) | first-touch places the page on the toucher's node |
| NIC on that node's PCIe (or a 2-NIC box, one per socket) | RX DMA and the feed handler on the same node |
| `kernel.numa_balancing = 0` (folder 29 file 14) | no surprise page migrations (a migration = copy + TLB shootdown IPI + stall) |
| Huge pages reserved **per node** (folder 29 file 12) | huge-page-backed structures stay local |
| Second socket → non-latency work (research, recording, risk batch) or a mirrored independent trading stack | keep the hot path off the link entirely |
| Check for **SNC / CCX** boundaries, not just sockets | the "node" might be half a socket |

Verify: `numastat -p <pid>` (all pages on one node?), `/proc/<pid>/numa_maps`,
`perf stat -e node-loads,node-load-misses` (`node-load-misses` = remote loads).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — "single socket, no NUMA"
AMD chiplets and Intel SNC create NUMA-like tiers within one socket. `numactl -H`
shows `available: 2 nodes` on many single-socket servers. Plan for the real
topology.

### Trap 2 — init thread allocates everything (folder 29 file 14)
First-touch on the init thread's node → half your workers are remote. Each thread
first-touches its own buffers.

### Trap 3 — NIC on socket 1, pipeline on socket 0
Every RX packet: DMA to node 1, read by a node 0 core = remote + link traffic on
the hottest path. Match NIC ↔ cores ↔ memory to one node.

### Trap 4 — `numa_balancing` on
The kernel periodically unmaps pages, faults on next access, and *migrates* pages
toward the accessing node — each migration is a copy + a TLB shootdown IPI to
every core mapping the page + a stall. Random µs spikes. `= 0`; bind explicitly.

### Trap 5 — cross-socket false sharing
Two hot threads on different sockets `fetch_add`ing adjacent atomics → the cache
line ping-pongs across UPI/IF (~100–300 ns per bounce, contended). Worse than the
same-socket case (folder 28). Pad *and* co-locate the threads.

### Trap 6 — `--interleave` for a latency workload
`numactl --interleave=all` spreads pages round-robin across nodes for
**bandwidth** (good for a streaming batch job). For a *latency* workload it means
~half your accesses are remote by design. Use `--membind` to one node.

### Trap 7 — `MPOL_BIND` to a nearly-full node → OOM
Strict bind + node full → allocation fails / OOM-kill even if the other node has
free memory (folder 29 file 14). Plan node capacity; `--preferred` is softer.

---

## > **HFT relevance**

> - **The whole hot pipeline lives on one NUMA node** — cores, memory, and the
>   NIC. feed decode → book build → strategy → order encode, all on node X's
>   cores; every pool/ring/table `mbind`'d to node X; NIC on node X's PCIe.
>   Cross-node on the hot path = a ~2× memory tax + interconnect contention +
>   jitter.
> - **Know if "node" means a socket, a CCX, or an SNC cluster** on your hardware
>   (`numactl -H`, `lscpu -e`). Pin within the smallest coherent locality.
> - **`kernel.numa_balancing = 0`**, per-node huge pages, parallel first-touch
>   (folder 29 files 12–14).
> - **Second socket = non-latency work** (research, recording, risk) or a
>   mirrored independent stack for a second NIC — never split one hot pipeline
>   across the interconnect.
> - **Verify:** `numastat -p`, `/proc/pid/numa_maps`, `perf stat -e
>   node-load-misses` (should be ~0 on the hot thread).

---

## Hands-on

```bash
# the real topology (sockets, CCX/SNC nodes, per-node cores + memory + distances)
numactl -H
lscpu -e=CPU,CORE,SOCKET,NODE,L3
cat /sys/devices/system/node/node0/distance          # "10 21" -> local=10, remote=2.1x

# NIC's NUMA node
cat /sys/class/net/eth0/device/numa_node

# is a process's memory local?
numastat -p $(pidof trader)
perf stat -e node-loads,node-load-misses ./trader    # node-load-misses = remote accesses

# launch bound to one node
numactl --cpunodebind=0 --membind=0 ./trader
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "one socket = no NUMA" | chiplets (CCX) / SNC → NUMA tiers within a socket |
| "remote DRAM is a bit slower" | ~1.5–2.2× + shared interconnect bandwidth + jitter |
| "the NIC's location doesn't matter" | NIC is on one socket's PCIe; RX DMA lands there |
| "`numa_balancing` optimizes for me" | random migration spikes; bind explicitly, set 0 |
| "`--interleave` is a good default" | bandwidth trick; for latency it makes half your accesses remote |
| "coherence is per-socket" | spans sockets; cross-socket snoop/transfer ~100–300 ns |

---

## Exercises

1. 2-socket box. feed handler pinned to node 0, order gateway pinned to node 1,
   dono ek shared-memory ring se communicate (folder 29 file 8), ring node 0 pe
   allocated. Kya latency cost, fix?

   <details><summary>Answer</summary>

   The gateway (node 1) reads/writes the ring on every message → each access is
   **remote DRAM / cross-socket coherence** (~130–300 ns vs ~100 ns local),
   *and* the ring's cache line ping-pongs across UPI/IF between the two sockets
   under contention → high, jittery per-message cost on the hottest hand-off.
   Fix: put **both** processes on **node 0** (feed handler + gateway share the
   node), allocate the ring on node 0, and the NIC used by the gateway on node
   0's PCIe if possible. If the gateway genuinely must be on node 1 (e.g. its
   NIC is there), then it's a 2-node design — accept one deliberate remote hop
   and size the ring / batch to amortize it, or use a second ring per direction
   each local to its consumer.
   </details>

2. `numactl -H` ek single-socket AMD box pe `available: 4 nodes` dikhata hai.
   8-thread trading pipeline kaise layout karo?

   <details><summary>Answer</summary>

   Four NUMA nodes on one socket = the CPU has 4 CCDs/CCXs, each with its own L3
   slice and a share of the memory channels; cross-CCX access is slower. Put the
   **entire latency-critical pipeline on ONE node** (one CCX): e.g. node 0's
   cores for feed decode, book build, strategy, order encode (one pinned thread
   each), all their memory `mbind`'d to node 0, huge pages reserved on node 0.
   That keeps every hand-off within one L3 and one memory domain. Housekeeping
   (OS, IRQs, logging) on node 1; non-latency work (research/recording) on nodes
   2–3. Verify with `numastat -p` that the trader's pages are ~all on node 0 and
   `perf stat -e node-load-misses` ~0.
   </details>

3. `perf stat -e node-loads,node-load-misses` par tumhare feed handler ka
   `node-load-misses` ratio ~30% hai. Kya matlab, aur kahan dekhoge?

   <details><summary>Answer</summary>

   ~30% of the loads that reached DRAM went to a **remote** NUMA node instead of
   the local one — each ~1.5–2× slower + interconnect traffic. On a hot path
   that's a big, jittery tax. Where to look: (1) `numastat -p <pid>` /
   `/proc/<pid>/numa_maps` — are the process's pages split across nodes? (2) Was
   the memory first-touched by a thread on a *different* node than the one it
   runs on (folder 29 file 14, the "init thread allocates everything" bug)? (3)
   Is the thread pinned to a node whose cores it's actually on, and is its memory
   `mbind`'d to that node? (4) Is the NIC on a different node (packet DMA lands
   remote)? (5) `numa_balancing` still on, migrating pages around? Fix the
   placement and re-measure — target ~0% node-load-misses.
   </details>

4. Cross-socket false sharing (folder 28 ka concept, ab 2 sockets par) — same-
   socket false sharing se kitna bura, aur kyun?

   <details><summary>Answer</summary>

   Same-socket false sharing: the contended line bounces between two cores'
   private caches via the on-die ring/mesh + shared L3 — ~40–80 ns per bounce
   (folder 32). Cross-socket: the line must travel over UPI/Infinity Fabric on
   every bounce — ~100–300 ns each, *and* that link is a shared, finite-bandwidth
   resource, so under load the bounces queue and the latency inflates further and
   becomes jittery. It's often 3–5× worse than the same-socket case and much
   less predictable. Fix: `alignas(64)` / `alignas(128)` to eliminate the false
   sharing (folder 28), **and** co-locate the two threads on the same socket
   (ideally same CCX) so even true sharing stays cheap.
   </details>

5. Ek team `numactl --interleave=all` se apna trading process launch kar rahi
   "taaki memory bandwidth maximize ho". Kya galat?

   <details><summary>Answer</summary>

   `--interleave=all` round-robins page allocations across all NUMA nodes. That
   maximizes aggregate *bandwidth* for a workload that streams huge arrays (a
   backtest, a big matrix op) — but for a *latency*-sensitive trading process it
   guarantees that ~(1 − 1/nodes) of the hot working set is on a **remote** node,
   so a large fraction of hot-path loads pay the remote penalty (~1.5–2×) and hit
   interconnect contention. The right policy for latency is `--membind=<one
   node>` (or `--preferred`) with the pipeline pinned to that node's cores —
   every hot load stays local. Interleaving is a bandwidth tool, not a latency
   tool.
   </details>

---

## Interview questions

1. NUMA hardware — why remote DRAM is slower (memory controller per socket + interconnect).
2. UPI / Infinity Fabric — what crosses it, why it's a shared bottleneck.
3. NUMA within a single socket — AMD CCX / Intel SNC; how you detect it.
4. Latency ladder: L1 → L2 → L3-local → L3-remote → local DRAM → remote DRAM (rough numbers).
5. The NIC's NUMA node — why it must match the hot cores and their memory.
6. Cross-socket false sharing vs same-socket — why it's worse.
7. `--membind` vs `--interleave` — latency vs bandwidth.

---

## Next
→ [`15-cpu-differences.md`](15-cpu-differences.md)

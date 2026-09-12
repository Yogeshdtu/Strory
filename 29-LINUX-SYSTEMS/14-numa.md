# 14 — NUMA: local vs remote memory, numactl, allocation policy

## Prerequisites
- `11-cpu-affinity.md` (pinning, sockets, topology)
- `13-page-faults-and-mlock.md` (first-touch allocation)
- `06-proc-and-sys.md` (`/sys/devices/system/node`)

## Yeh topic abhi kyun
Multi-socket server = **NUMA** (Non-Uniform Memory Access). Har socket ka apna
memory controller + RAM. Ek core apne socket ki RAM ("local") ~1× latency pe
padhta, doosre socket ki RAM ("remote", interconnect ke paar) ~1.5–2.2× pe.
HFT boxes aksar 2-socket hote — agar tumhara hot thread node 0 pe hai aur uska
data node 1 pe, har access ek silent tax. Yeh lesson: NUMA ko dekhna, aur
allocation ko cores ke paas rakhna.

---

## NUMA topology

```
    +---------- Socket 0 ----------+        +---------- Socket 1 ----------+
    |  cores 0-9, 20-29 (SMT)      |  UPI   |  cores 10-19, 30-39         |
    |  L3 cache                    |<====>|  L3 cache                    |
    |  Memory controller -> 128 GB |        |  Memory controller -> 128 GB|
    |  (NUMA node 0)               |        |  (NUMA node 1)              |
    +-----------------------------+        +-----------------------------+
```

- Node 0 ke core → node 0 RAM: **local**, ~80–100 ns.
- Node 0 ke core → node 1 RAM: **remote**, ~130–200 ns (UPI/QPI hop) + shared
  interconnect bandwidth.
- Big single-socket AMD/Intel bhi internally multi-NUMA ho sakte (chiplets /
  sub-NUMA clustering) — `numactl -H` sach batata.

```bash
numactl -H              # nodes, per-node cores, per-node free MB, distance matrix
lscpu | grep -i numa
cat /sys/devices/system/node/node0/cpulist
cat /sys/devices/system/node/node0/distance     # "10 21" -> local=10, remote=21 (relative)
```

---

## First-touch allocation policy

Linux ka default: ek page us NUMA node pe allocate hota jahan ka core usse
**pehli baar touch** karta — **`malloc` ke waqt nahi, first write ke waqt**.

```cpp
char* buf = new char[1 << 30];         // abhi koi node assign nahi
// jo thread pehli baar har page likhega, us thread ke core ka node -> woh page
init_thread_on_node0(buf);              // saare pages node 0 pe
// ab agar node-1 ka strategy thread buf use kare -> har access remote
```

**Common bug:** ek "init"/main thread (node 0) saara data allocate + zero karta
(`memset`, ya `std::vector` fill), phir worker threads (spread across nodes) use
karte → saara data node 0 pe, half the workers remote. Fix: **har worker apna
data khud first-touch kare** (parallel init on the same threads that will use
it), ya explicit binding.

---

## Explicit control

### `numactl` (process launch)
```bash
numactl --cpunodebind=0 --membind=0 ./trader      # cores + memory dono node 0
numactl --physcpubind=2-9 --membind=0 ./trader    # specific cores
numactl --interleave=all ./research               # pages round-robin (bandwidth, not latency)
numactl --preferred=0 ./x                          # node 0 try; fail pe koi bhi
```

### In-code
```cpp
#include <numa.h>          // -lnuma
if (numa_available() < 0) { /* not NUMA */ }
numa_set_localalloc();                       // "jis core pe hoon uska node"
void* p = numa_alloc_onnode(size, 0);       // node 0 se
numa_free(p, size);

// ya libnuma ke bina, mbind():
#include <numaif.h>
unsigned long nodemask = 1UL << 0;          // node 0
mbind(p, len, MPOL_BIND, &nodemask, 64, MPOL_MF_MOVE);
set_mempolicy(MPOL_BIND, &nodemask, 64);    // is thread ki default policy
```

### Verify placement
```bash
numastat -p <pid>                       # per-node pages for a process
cat /proc/<pid>/numa_maps               # har mapping: N0=<pages> N1=<pages>
```

---

## `numa_balancing` — automatic page migration (turn it OFF for HFT)

`kernel.numa_balancing = 1` (default): kernel periodically pages ko "unmap"
karta, next access pe fault trigger hota, kernel dekhta kaunse node se access
aaya, aur page ko wahan **migrate** kar deta (agar consistently remote).

Idea achha (long-running batch jobs self-tune), par HFT ke liye **poison**:
- Har "NUMA hint fault" ek minor fault + possible page copy + TLB shootdown =
  random µs spike.
- Migration decisions ms-scale pe hote, tumhare access pattern se lag.

```bash
echo 0 > /proc/sys/kernel/numa_balancing        # HFT: off
```
Instead, **explicitly** bind (upar) — tum jaante ho layout, kernel ko guess mat
karne do.

---

## Internal working

- Har node ka apna buddy allocator free-list. Page fault handler current CPU ka
  node dekhta (`numa_node_id()`), us node ke free-list se page (policy allow
  kare to).
- Policies: `MPOL_DEFAULT` (first-touch/local), `MPOL_BIND` (strict — fail/OOM
  agar node full), `MPOL_PREFERRED` (try, fallback), `MPOL_INTERLEAVE`
  (round-robin pages across nodes — bandwidth workloads).
- Remote access hardware-transparent — koi error nahi, bas cache-miss ka cost
  higher (interconnect hop). `perf stat -e node-load-misses` se dikhta.
- Huge pages per-node pool (`12` trap 6).
- IPC / shared memory (`08`): jo node pe allocate hui, wahan rehti — dono
  processes ko us node ke cores pe rakho.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — init thread pe saara data allocate + zero
First-touch → sab node 0 pe. Node-1 workers remote. Fix: parallel first-touch
on the using threads, ya `numa_alloc_onnode` per worker.

### Trap 2 — `numa_balancing` on
Random page migrations = random µs spikes. `echo 0`. Bind explicitly instead.

### Trap 3 — cores pin kiye, memory nahi
`numactl --physcpubind=2-9` bina `--membind=0` → cores node 0 pe, par memory
policy abhi bhi default → kuch pages node 1 pe first-touch ho sakti (agar koi
node-1 thread ne chhui). Dono bind karo.

### Trap 4 — cross-node shared memory ring
Feed handler node 0, strategy node 1, ring node 0 pe → strategy har ring op
remote. Poora pipeline **ek node** pe rakho.

### Trap 5 — `MPOL_BIND` strict → OOM on a nearly-full node
`--membind=0` aur node 0 lagbhag full → allocation fail / OOM-kill, chahe node
1 free ho. `--preferred=0` softer, ya node capacity plan karo.

### Trap 6 — NIC doosre node pe
NIC socket 1 ke PCIe pe, hot thread node 0 pe → har incoming packet DMA node 1
memory mein, phir node 0 core use padhta = remote. NIC ko us node ke cores +
memory ke saath match karo (`lspci -vv` → NUMA node; `/sys/class/net/eth0/
device/numa_node`). Multi-NIC boxes: ek NIC per socket.

### Trap 7 — `std::pmr` / custom allocator NUMA-oblivious
Ek global arena node 0 pe → sab allocations node 0. Per-node arenas + thread ko
apne node ka arena de.

---

## > **HFT relevance**

> - **Keep the whole hot pipeline on one NUMA node.** feed→decode→book→strategy
>   →order-encode threads on cores of node X; all their buffers, pools, rings,
>   lookup tables allocated on node X; the NIC on node X's PCIe.
> - **`numactl --cpunodebind=0 --membind=0`** as the launch wrapper (or `mbind`
>   in code) — plus per-thread pinning within the node (`11`).
> - **`kernel.numa_balancing = 0`** always — you know the layout; don't let the
>   kernel migrate pages under you.
> - **Parallel first-touch:** the thread that will own a buffer is the thread
>   that zeroes/warms it (`13`), so first-touch places it locally.
> - **Second socket** = non-latency work (research, backtests, market-data
>   recording) or a mirrored independent trading stack — never split one hot
>   pipeline across the UPI link.
> - **Verify:** `numastat -p <pid>` should show ~all pages on the expected
>   node; `/proc/<pid>/numa_maps` per-region.

---

## Hands-on

```bash
# topology + distances
numactl -H
cat /sys/devices/system/node/node0/distance

# NIC ka NUMA node
cat /sys/class/net/eth0/device/numa_node        # -1 = unknown/single, warna 0/1

# ek process ka page placement
numastat -p $(pidof trader)
cat /proc/$(pidof trader)/numa_maps | head

# launch bound
numactl --cpunodebind=0 --membind=0 ./trader

# balancing off (HFT)
cat /proc/sys/kernel/numa_balancing        # 1 = on (default) -> set 0

# remote-access misses (folder 35 preview)
perf stat -e node-loads,node-load-misses ./trader
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "RAM sab jagah same speed" | remote node ~1.5–2.2× latency + shared interconnect BW |
| "`malloc` decide karta node" | first **touch** decides (default policy) |
| "`numa_balancing` madad karta" | HFT: random migration spikes; bind explicitly, set 0 |
| "cores bind kiye, kaafi" | memory policy bhi bind — warna remote pages |
| "shared ring kahin bhi chalega" | jis node pe allocate, wahin — dono processes us node pe |
| "single socket = no NUMA" | AMD chiplets / sub-NUMA clustering — `numactl -H` check |

---

## Exercises

1. Ek pool `main()` (node 0 core) mein `new`+`memset` hota, phir 8 worker
   threads (4 node 0, 4 node 1) use karte. Kya galat, fix?

   <details><summary>Answer</summary>

   `memset` `main` pe hua → saare pages first-touch node 0 → 4 node-1 workers
   har access remote (~1.5–2×). Fix: (a) pool ko chunks mein baanto, har worker
   apna chunk khud first-touch/zero kare (parallel init on the using thread), ya
   (b) `numa_alloc_onnode` per worker, ya (c) simplest — sab workers ek node pe
   rakho (HFT way) aur node-1 ko dusre kaam ke liye.
   </details>

2. `numactl --membind=0` ke saath process OOM-killed ho gaya, par `free -h` 60
   GB free dikha raha tha. Kaise?

   <details><summary>Answer</summary>

   `--membind=0` strict — allocations sirf node 0 se. Node 0 full ho gaya
   (say 128 GB used) jabki node 1 pe 60 GB free tha. Strict bind → node 0 pe
   nahi mila → allocation fail / OOM, node 1 use nahi kar sakta. `--preferred=0`
   (soft) ya do-node bind (`--membind=0,1` with node 0 priority via
   `MPOL_PREFERRED`), ya node 0 ka footprint kam karo.
   </details>

3. `/proc/<pid>/numa_maps` mein ek `[heap]` line `N0=200000 N1=180000` dikha
   rahi hai. Interpretation, HFT context?

   <details><summary>Answer</summary>

   Heap ke pages dono nodes pe bikhre — ~200k pages (780 MB) node 0, ~180k node
   1. Matlab allocations mixed threads/nodes se first-touch hue. HFT hot pool
   ke liye yeh bura: koi bhi given access 45% chance remote. Chahiye:
   `N0=<all> N1=0` (ya vice-versa) for the hot arena — via per-node arena +
   `mbind` + node-pinned threads.
   </details>

4. `kernel.numa_balancing = 1` ke saath ek periodic ~3–10 µs spike har few
   hundred ms. Mechanism?

   <details><summary>Answer</summary>

   NUMA balancer periodically kuch pages ko PT mein "not present" mark karta
   (PROT_NONE-ish). Agla access → NUMA hint fault (minor fault cost) → kernel
   note karta access kaunse node se. Agar page consistently remote-accessed →
   kernel page ko **migrate** (copy to other node + PT update + TLB shootdown
   IPI to every core mapping it). Woh fault + possible copy + IPI = tumhara
   spike. Fix: `= 0`, bind explicitly.
   </details>

5. HFT box mein NIC `/sys/class/net/eth0/device/numa_node` = `1`, par tumne
   pipeline node 0 pe rakha. Impact + options.

   <details><summary>Answer</summary>

   NIC DMA incoming packets ko node 1 memory mein likhta (NIC ke paas ki RAM),
   phir node 0 core unhe padhta = remote read per packet + UPI traffic on the
   RX-hot path. Options: (a) pipeline ko node 1 pe move karo (NIC ke saath
   match). (b) NIC ke RX ring buffers ko node 0 memory pe allocate karne ki
   koshish (driver-dependent, `ethtool`/driver params — not always possible).
   (c) 2-NIC box: node 0 ke PCIe wala NIC use karo. Rule: NIC, cores, memory
   sab ek node.
   </details>

---

## Interview questions

1. NUMA — local vs remote memory latency, kyun.
2. First-touch allocation — page kis node pe, kab decide hota?
3. "Init thread allocates everything" bug — kya hota, fix.
4. `MPOL_BIND` vs `MPOL_PREFERRED` vs `MPOL_INTERLEAVE`.
5. `numa_balancing` — kya karta, HFT box pe kyun off.
6. NIC ka NUMA node kyun matter karta RX path pe.
7. `numastat` / `numa_maps` se placement kaise verify.

---

## Next
→ [`15-irq-affinity.md`](15-irq-affinity.md)

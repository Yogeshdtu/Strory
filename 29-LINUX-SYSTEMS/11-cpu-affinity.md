# 11 — CPU affinity: taskset, sched_setaffinity, isolcpus, nohz_full

## Prerequisites
- `10-cpu-scheduling.md` (preemption, migration, CFS vs RT)
- `06-proc-and-sys.md` (`/sys` topology, `/proc/interrupts`)

## Yeh topic abhi kyun
File `10` ne kaha "preemption aur migration = jitter". Iska sabse strong ilaaj:
**core isolation + pinning** — kuch cores ko OS se poori tarah chheen lo, aur
apne hot threads ko un par baandh do. Yeh HFT latency tuning ki **sabse
important** technique hai. Example `06` measure karta hai ki pinning tail (p99.9)
ko kaise girata.

---

## Do alag cheezein: affinity vs isolation

| | **Affinity (pinning)** | **Isolation** |
|---|---|---|
| Kya | "yeh thread sirf in cores pe chal sakta" | "yeh core scheduler ke general pool se hata do" |
| Kaise | `taskset`, `sched_setaffinity`, `cpuset` | `isolcpus=` boot param, `cpuset` with `sched_load_balance=0` |
| Akela kaam karta? | thoda — thread migrate nahi hoga, par doosre threads phir bhi us core pe aa sakte | poori tarah — core pe sirf woh threads jo tumne explicitly pin kiye |
| HFT | dono chahiye — pin **into** isolated cores | — |

**Sirf pinning kaafi nahi:** tumne hot thread ko core 3 pe pin kiya, par kernel
abhi bhi core 3 pe cron job, `kworker`, doosre app threads schedule kar sakta.
Isolation woh rok deta.

---

## Pinning: `sched_setaffinity` / `pthread_setaffinity_np`

```cpp
#define _GNU_SOURCE
#include <sched.h>
#include <pthread.h>

cpu_set_t set;
CPU_ZERO(&set);
CPU_SET(3, &set);                                    // sirf CPU 3
// current thread:
pthread_setaffinity_np(pthread_self(), sizeof(set), &set);
// ya kisi TID par (0 = current):
sched_setaffinity(0, sizeof(set), &set);

// verify:
CPU_ZERO(&set);
sched_getaffinity(0, sizeof(set), &set);
for (int c = 0; c < CPU_SETSIZE; ++c)
    if (CPU_ISSET(c, &set)) printf("allowed: %d\n", c);
```

External: `taskset -c 3 ./trader`, `taskset -cp 3 <pid>`, `taskset -cp 2,3
<pid>`.

**Pin karne ke turant baad thread ko ek dummy loop chala do** taaki woh actually
target core pe migrate ho jaaye (affinity set hone se migration turant nahi
hota — agla scheduling decision pe hota). Ya `sched_yield()` ek baar.

---

## Isolation: `isolcpus`, `nohz_full`, `rcu_nocbs`

Boot cmdline (`/etc/default/grub` → `GRUB_CMDLINE_LINUX`, ya systemd-boot entry):

```
isolcpus=2,3,18,19  nohz_full=2,3,18,19  rcu_nocbs=2,3,18,19  irqaffinity=0,1
```

| Param | Kya karta |
|---|---|
| **`isolcpus=2,3`** | in cores ko scheduler load-balancer se hata do. Koi thread yahan **tab tak** nahi jaayega jab tak tum explicitly `sched_setaffinity` se na bhejo. (Deprecated-ish; `cpuset` preferred, par abhi bhi widely used.) |
| **`nohz_full=2,3`** | "tickless" — agar core pe sirf 1 runnable task hai, timer interrupt (~1000 Hz) bhi band. Ek periodic ~1 µs jitter source gone. Needs `CONFIG_NO_HZ_FULL`. |
| **`rcu_nocbs=2,3`** | RCU callback processing in cores se hata ke "housekeeping" cores pe. Warna RCU grace-period work tumhare hot core pe chal sakta. |
| **`irqaffinity=0,1`** | default IRQ handling sirf cores 0,1 pe (`15` mein detail). |

Reboot ke baad verify:
```bash
cat /sys/devices/system/cpu/isolated          # 2-3,18-19
cat /sys/devices/system/cpu/nohz_full         # 2-3,18-19
cat /proc/cmdline
```

### `cpuset` (cgroup) — dynamic isolation

`isolcpus` static hai (reboot chahiye). `cpuset` cgroup se runtime pe:

```bash
mkdir /sys/fs/cgroup/hot
echo 2-3 > /sys/fs/cgroup/hot/cpuset.cpus
echo 0   > /sys/fs/cgroup/hot/cpuset.mems           # NUMA node 0
# system.slice ko baaki cores pe restrict karo (isolate 2-3):
echo 0-1,4-17 > /sys/fs/cgroup/system.slice/cpuset.cpus
echo $$ > /sys/fs/cgroup/hot/cgroup.procs           # is process ko hot cpuset mein
```

Modern HFT setups `cpuset` + `nohz_full` (nohz abhi bhi boot-time) combine karte.

---

## Topology-aware pinning

`lscpu -e`, `/sys/devices/system/cpu/cpuN/topology/`:

```
CPU  CORE  SOCKET  SMT-sibling
  0     0       0   (0, 16)
  2     2       0   (2, 18)      <- physical core 2
 18     2       0   (2, 18)      <- SAME physical core (hyperthread)
```

Rules:
- Hot thread ko ek physical core do. Uske **SMT sibling** (yahan CPU 18) ko
  **idle** rakho (isolate karo, kuch mat pin karo) — warna dono logical CPUs
  ek core ke execution units/L1/L2 share karte, IPC girta.
- Ek NUMA node ke andar raho — feed, strategy, gateway threads **same socket**
  (`14`).
- Housekeeping (OS, IRQ, logging, monitoring) ko socket ke doosre cores ya
  doosre socket pe.

**Example `06`** dikhta hai: unpinned vs pinned — min/p50 ~same, par
p99/p99.9/max pinned pe kaafi tight (no migration = warm cache + no scheduler
interference).

---

## Internal working

- `sched_setaffinity` thread ke `cpus_allowed` mask set karta. Agla scheduling
  decision pe (ya turant, agar current core ab allowed nahi) thread migrate.
- `isolcpus` cores ko kisi `sched_domain` mein nahi rakhta → load balancer
  unhe consider hi nahi karta → koi automatic migration in/out.
- `nohz_full` core: kernel per-CPU `tick_sched` ko "stop" mode mein rakhta jab
  `nr_running <= 1`. Wapas 2nd task aane pe tick resume. **1 task rakho** warna
  faayda nahi.
- Kernel per-CPU threads (`ksoftirqd/N`, `migration/N`, `rcuc/N`) abhi bhi
  technically un cores pe "exist" karte par idle rehte agar kaam un tak na
  aaye (`rcu_nocbs`, `irqaffinity` se ensure karo).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — pin kiya, isolate nahi kiya
Core 3 pe pinned hot thread + core 3 pe OS ne `kworker` / doosra app thread
schedule kiya → preemption jaari. `isolcpus`/`cpuset` se core ko khaali karo
pehle.

### Trap 2 — SMT sibling pe kuch chal raha
Core 3 (CPU 3 + CPU 19). Tumne CPU 3 isolate/pin kiya par CPU 19 pe OS
scheduling kar raha → ~10–30% IPC loss on your hot thread. Sibling bhi isolate,
ya SMT off (`nosmt` / BIOS).

### Trap 3 — `nohz_full` core pe 2+ threads
`nohz_full` sirf tab tick rokta jab `nr_running == 1`. Do busy threads ek nohz
core pe → tick wapas → faayda zero. Ek core, ek hot thread.

### Trap 4 — `isolcpus` bhoolke sirf `taskset`
`taskset -c 3` sirf ek process ki affinity. System ke baaki 500 threads abhi
bhi core 3 pe schedulable. Isolation boot/cpuset level pe.

### Trap 5 — affinity set kiya par thread abhi bhi purane core pe
Migration agli scheduling decision pe. Verify: `cat /proc/<tid>/stat | awk
'{print $39}'` (last CPU), ya `ps -o psr`. Ek `sched_yield()`/dummy-loop se
force.

### Trap 6 — container mein `isolcpus` expect karna
Container host kernel ka hai — `isolcpus` host boot param. Container ko sirf
`cpuset.cpus` se restrict kar sakte; asli isolation host pe configure hoti.

### Trap 7 — housekeeping cores ko bhi over-pack
Cores 0–1 pe OS + IRQ + logging + monitoring + cron — agar woh saturate ho gaye
to network softirqs late honge → tumhare "isolated" hot path pe bhi indirect
latency. Housekeeping ko bhi enough cores do.

---

## > **HFT relevance**

> - **The canonical layout** (2-socket, 20 cores/socket, SMT on):
>   - Socket 0: cores 2–9 = hot trading threads (1 per physical core, siblings
>     idle). Core 0–1 = housekeeping + NIC IRQs for socket 0.
>   - Socket 1: mirror, or non-latency workloads (research, replay).
>   - Boot: `isolcpus=2-9,22-29 nohz_full=2-9,22-29 rcu_nocbs=2-9,22-29
>     irqaffinity=0-1,20-21`.
> - **One thread, one core.** feed-decode, book-build, strategy, order-encode —
>   har ek apne dedicated isolated physical core pe, pinned at startup.
> - **Pin by physical core, not logical CPU number.** Read topology at startup;
>   don't hardcode "CPU 3".
> - **Verify in production:** a startup self-check that reads `sched_getaffinity`,
>   `/sys/devices/system/cpu/isolated`, and asserts the hot thread's core is
>   isolated and its SMT sibling is not in use. Fail loudly if not.
> - **Result:** `nonvoluntary_ctxt_switches ≈ 0`, migrations `= 0`, p99.9 loop
>   time within a few % of p50 (example `06`).

---

## Hands-on

```bash
# Linux pe -- example 06: unpinned vs pinned jitter (p50 vs p99.9 vs max)
g++ -std=c++20 -O2 -pthread 29-LINUX-SYSTEMS/examples/06_cpu_affinity.linux.cpp -o /tmp/aff
/tmp/aff
taskset -c 3 /tmp/aff              # aur behtar agar core 3 isolated ho

# topology
lscpu -e=CPU,CORE,SOCKET,NODE
cat /sys/devices/system/cpu/cpu3/topology/thread_siblings_list

# isolation status
cat /sys/devices/system/cpu/isolated
cat /sys/devices/system/cpu/nohz_full
cat /proc/cmdline | tr ' ' '\n' | grep -E 'isol|nohz|rcu_nocb|irqaff'

# ek running thread ka current core + migrations
watch -n0.5 "cat /proc/$(pidof trader)/task/*/stat | awk '{print \$1, \$39}'"
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "pinning = isolation" | pin thread migrate nahi hoga; doosre threads phir bhi aa sakte — isolcpus/cpuset chahiye |
| "SMT sibling se koi farak nahi" | dono logical CPUs ek core ke units share; sibling idle rakho |
| "`nohz_full` se tick hamesha band" | sirf jab `nr_running == 1` |
| "`taskset` se core isolate ho gaya" | sirf ek process ki affinity; baaki system unaffected |
| "affinity set = turant migrate" | agli scheduling decision pe; verify `/proc/.../stat` field 39 |
| "container mein isolcpus set kar dunga" | host boot param; container mein sirf cpuset |

---

## Exercises

1. Tumne hot thread ko `taskset -c 3` se pin kiya par p99.9 abhi bhi bura hai.
   Do cheezein check karo.

   <details><summary>Answer</summary>

   (1) Kya core 3 isolated hai? `cat /sys/devices/system/cpu/isolated` — agar
   3 nahi dikhta, to OS abhi bhi core 3 pe kworkers/cron/other-threads schedule
   kar raha. Boot `isolcpus`/`cpuset`. (2) Core 3 ka SMT sibling — `cat
   .../cpu3/topology/thread_siblings_list` → say `3,19`. Kya CPU 19 pe kuch
   chal raha? Agar haan, IPC loss. Sibling bhi isolate.
   </details>

2. `nohz_full=3` set hai. Tumne core 3 pe hot thread + ek "helper" thread dono
   pin kiye. Tick band hui?

   <details><summary>Answer</summary>

   Nahi. `nohz_full` core pe tick tabhi rukti jab `nr_running == 1`. Do pinned
   busy threads → `nr_running == 2` → timer tick har ~1ms firing → jitter
   source wapas. Helper thread ko doosre core pe bhejo; ek isolated core = ek
   hot thread.
   </details>

3. Startup self-check likhna hai jo confirm kare hot thread sahi jagah hai.
   Kya-kya assert karoge?

   <details><summary>Answer</summary>

   (a) `sched_getaffinity` mask mein exactly 1 CPU, aur woh expected core.
   (b) `/sys/devices/system/cpu/isolated` mein woh core present.
   (c) `/sys/.../cpuN/topology/thread_siblings_list` se sibling nikaalo, assert
   sibling bhi isolated (ya SMT off: `/sys/devices/system/cpu/smt/active` == 0).
   (d) `/sys/.../nohz_full` mein core present.
   (e) optional: ek dummy loop chala ke `sched_getcpu()` == expected.
   Koi fail → loud error, exit (better than silent latency bug in prod).
   </details>

4. `isolcpus` vs `cpuset`-based isolation — ek advantage har ek ka.

   <details><summary>Answer</summary>

   `isolcpus`: sabse thorough (core kisi sched_domain mein nahi), simple, well-
   understood; downside — reboot chahiye, static, aur (officially) deprecated.
   `cpuset`: dynamic (runtime add/remove cores), per-cgroup, no reboot, plays
   with systemd; downside — thoda kam airtight (some kernel threads/timers
   still reach), more moving parts. Practice: `nohz_full`+`rcu_nocbs` boot-time
   + `cpuset` (ya `isolcpus`) for the scheduler part.
   </details>

5. Housekeeping cores (0–1) saturate ho gaye monitoring + logging se. Isolated
   hot core pe indirect asar kaise?

   <details><summary>Answer</summary>

   NIC RX softirqs, RCU callbacks, timer work, aur kernel threads jo tumne
   housekeeping cores pe bhej diye — woh ab late chalte hain. Market-data
   packet NIC pe aaya, softirq core 0 pe queued, par core 0 busy → packet
   processing delayed → tumhare hot thread ko data late milta, chahe woh core
   khud idle-isolated ho. Fix: housekeeping ko enough cores; logging/monitoring
   ko low priority (`SCHED_IDLE`) ya alag socket.
   </details>

---

## Interview questions

1. Affinity vs isolation — do alag cheezein, dono kyun chahiye.
2. `sched_setaffinity` / `cpu_set_t` — API aur "migrate kab hota".
3. `isolcpus`, `nohz_full`, `rcu_nocbs`, `irqaffinity` — har ek kya karta.
4. SMT sibling ko idle kyun rakhte ho?
5. `nohz_full` core pe kitne threads, kyun?
6. `cpuset` vs `isolcpus` — trade-offs.
7. Ek startup self-check jo pinning verify kare — kya assert karega?

---

## Next
→ [`12-huge-pages.md`](12-huge-pages.md)

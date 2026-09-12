# 01 — Linux architecture: kernel vs userspace

## Prerequisites
- `26-CONCURRENCY` (threads, scheduling ka basic idea)
- `14-MEMORY` (virtual address space, pages)
- `24-COMPILATION-LINKING` (ELF binary, `main` se pehle kya hota)

## Yeh topic abhi kyun
Ab tak humne C++ likha aur `./a.out` chala diya — beech mein kya hai, socha nahi.
HFT engineering ka aadha kaam **OS ke saath baat karna** hai: memory maang na,
core pin karna, timestamp lena, network se bytes uthana. Har baar jab tumhara
code "OS se kuch karwata hai", woh ek **user → kernel transition** hai, aur har
transition ki ek keemat hai. Poora folder isi keemat ke baare mein hai. Pehle
map: kaun kahan baithta hai.

---

## Do duniya: userspace aur kernel

```
        +--------------------------------------------------+
        |                 USERSPACE (ring 3)               |
        |  tumhara process: strategy, feed handler, shell  |
        |  libc, libstdc++, malloc, tumhara main()         |
        |  apni virtual address space, restricted CPU      |
        +------------------------|-------------------------+
                                 |  syscall instruction
                                 |  (ret to user on done)
        +------------------------v-------------------------+
        |                  KERNEL (ring 0)                 |
        |  scheduler, memory manager, VFS, TCP/IP stack,   |
        |  device drivers, page fault handler, timers      |
        |  full hardware access, ek shared address space   |
        +------------------------|-------------------------+
                                 |
        +------------------------v-------------------------+
        |     HARDWARE: CPU cores, RAM, NIC, NVMe, PMU     |
        +--------------------------------------------------+
```

- **Userspace** = tumhare programs. Har process ko lagta hai uske paas poori
  machine hai — apni 0-se-shuru virtual memory, apne "CPU". Yeh **illusion**
  kernel banata hai.
- **Kernel** = ek hi program jo boot pe load hota hai aur machine band hone tak
  chalta hai. Sab processes iske through hardware ko touch karte hain. Kernel
  ke paas **sab kuch** ka access hai; isi liye woh chhota, paranoid, aur heavily
  reviewed hota hai.

Tum kernel ko "call" nahi karte jaise ek function. Tum ek **special CPU
instruction** (`syscall` x86-64 pe) chalate ho jo CPU ko ring 3 se ring 0 mein
le jaati hai, ek fixed entry point pe. (File `02` isko detail mein.)

---

## Rings: CPU hardware-level protection

x86 CPU ke paas 4 privilege levels hain (ring 0–3). Linux sirf do use karta:

| Ring | Kaun | Kya kar sakta |
|---|---|---|
| **0** | kernel | har instruction: `hlt`, `lgdt`, port I/O, page tables likhna, `cli`/`sti` (interrupts off) |
| **3** | tumhara code | sirf "unprivileged" instructions. `cli` chalao → **#GP fault** → process ko `SIGSEGV` |

Yeh software policy nahi, **hardware enforcement** hai. Ring 3 mein rehke tum
physical memory ko seedha nahi chhoo sakte, dusre process ki memory nahi padh
sakte, NIC ke register nahi likh sakte. Har aisi cheez ke liye kernel se
**maangna** padta — aur woh check karta ki tumhe allowed hai ya nahi.

> **HFT relevance:** kernel-bypass networking (DPDK, Solarflare Onload, ef_vi —
> folder `30` file `13`) ka poora point yeh hai ki NIC ki queues ko **ring 3
> mein map** kar do, taaki packet bhejne/lene ke liye ring 0 mein jaana hi na
> pade. Ek `sendto()` syscall ~1–3 µs kha jaata; ef_vi se woh ~100–300 ns ho
> jaata. Yeh folder samajhne ke baad woh chapter obvious lagega.

---

## Kernel kya-kya manage karta (tumhare liye)

| Subsystem | Kya karta | Tumhare liye iska matlab |
|---|---|---|
| **Scheduler** | kaunsa thread kaunse core pe, kab | context switch ~1–5 µs; `nice`/`SCHED_FIFO`/affinity se control (`10`, `11`) |
| **Memory manager** | virtual→physical mapping, page faults, swap | `malloc` turant RAM nahi deta; pehli touch pe fault (`13`) |
| **VFS + block layer** | files, directories, disk I/O | har `read()`/`write()` ek syscall; page cache beech mein (`05`) |
| **Network stack** | Ethernet→IP→TCP/UDP, sockets | `recv()` = syscall + copy from kernel buffer (folder `30`) |
| **Time** | wall clock, monotonic, timers | `clock_gettime` — par vDSO se sasta (`16`) |
| **IPC** | pipes, signals, shared memory, futex | mutex ka sleep-path futex syscall hai (folder `26`) |

Har row mein ek **syscall boundary** chhupa hai. Woh boundary hi is folder ka
central kirdaar hai.

---

## Ek program ka janm (recap + OS lens)

Jab tum `./strategy` chalate ho:

1. Shell `fork()` karta → apni copy banata (`03`).
2. Copy `execve("./strategy", argv, envp)` karta → kernel current image ko
   **fenk** deta, ELF padhta, `.text`/`.data` ko `mmap` karta, stack banata,
   dynamic linker (`ld-linux.so`) ko entry point banata.
3. `ld-linux.so` shared libs (`libc.so`, `libstdc++.so`) ko `mmap` karta,
   relocations fix karta.
4. libc ka `__libc_start_main` chalta → constructors, phir **tumhara `main()`**.
5. `main` return → `exit()` → kernel process ke saare resources (fds, memory,
   page tables) reclaim karta, parent ko `SIGCHLD` bhejta.

Har step mein syscalls: `fork`, `execve`, `mmap` (bahut baar), `openat`,
`read`, `mprotect`, `brk`... `strace ./strategy` chala ke poori list dekho.

---

## `strace` — tumhari sabse achhi dost

`strace` har syscall ko intercept karke print karta:

```bash
strace -f -T -tt ./myprog        # -f: child threads bhi, -T: har call ka time, -tt: timestamp
strace -c ./myprog               # summary: kaunsi syscall kitni baar, total time
strace -e trace=network ./myprog # sirf network syscalls
```

`strace -c` ka output HFT profiling ka pehla step hai — agar hot loop mein
`clock_gettime` ya `read` ki laakhon calls dikhein, wahi tumhara bottleneck hai.
(Dhyan: `strace` khud bahut slow kar deta — measurement ke liye `perf` behtar,
folder `35`.)

---

## ⚠️ Traps / Common mistakes

### Trap 1 — "syscall bhi bas ek function call hai"
Nahi. Ek normal call ~1 ns. Ek syscall ~250–700 ns (mode switch, register
save/restore, kernel entry, Spectre/Meltdown mitigations). Example `01` isse
naapta hai. Hot path pe syscall = P99 spike.

### Trap 2 — "kernel ek process hai jo main dekh sakta hoon `ps` mein"
Kernel threads (`[kworker]`, `[ksoftirqd]`) dikhte hain, par kernel khud koi
PID nahi. Woh har process ke address space ke "upar wale half" mein mapped hota
(pre-Meltdown; ab KPTI se alag), aur syscall/interrupt pe activate hota.

### Trap 3 — "root hone ka matlab ring 0"
Nahi. `root` (uid 0) bhi ring 3 mein chalta. `root` sirf **kernel se zyada
cheezein maang sakta** (raw sockets, `mlock` bina limit, dusre users ke
process). Ring 0 sirf kernel code aur loaded kernel modules.

### Trap 4 — "container = light VM, apna kernel"
Container **host ka hi kernel** share karta (namespaces + cgroups se isolated
view). Isi liye Linux container Windows pe seedha nahi chalte, aur isi liye
container ke andar `/proc/sys` tuning host ko affect kar sakti (`17`).

### Trap 5 — WSL2 ≠ real Linux tuning target
WSL2 ek asli Linux kernel chalata hai (lightweight VM mein), par timers,
`clocksource`, IRQ affinity, isolcpus — yeh sab host Windows ke rahmo-karam pe.
Seekhne ke liye badhiya; latency numbers ke liye bharosemand nahi. Is folder ke
examples WSL pe *chalenge*, par unke benchmark numbers ko production sach mat
maano.

---

## > **HFT relevance** (poore folder ka thesis)

> - **Har trading system Linux pe hai.** Windows pe colocation nahi milti, real-time
>   tuning knobs nahi, kernel-bypass drivers nahi. Isi liye yeh folder Linux-only.
> - **Latency = syscalls + cache misses + scheduling jitter.** Yeh folder pehle
>   do ko attack karta: syscall count ghatao (`02`, `05`), aur scheduler ko hot
>   thread se door rakho (`10`, `11`, `15`).
> - **Predictability > throughput.** Ek page fault (`13`) ya ek preemption (`10`)
>   trading hours mein ek P99.9 spike hai. Warm-up + isolation + pinning se yeh
>   spikes steady state se hat jaate hain.
> - **Production HFT box ka look:** `isolcpus`/`nohz_full` se kuch cores OS se
>   chheene, un par pinned busy-poll threads, `mlockall`, huge pages, IRQs
>   dusre cores pe, `mitigations=off`, C-states locked. File `18` poori checklist.

---

## Hands-on

```bash
# 1. Ek trivial program ki syscalls dekho
printf 'int main(){}\n' > /tmp/empty.c && gcc /tmp/empty.c -o /tmp/empty
strace -c /tmp/empty         # dynamic linker ki mmap/openat/mprotect/read dikhengi
gcc -static /tmp/empty.c -o /tmp/empty_s && strace -c /tmp/empty_s   # bahut kam

# 2. Ring 3 se privileged instruction -> crash
cat > /tmp/ring.c <<'EOF'
int main(){ __asm__ __volatile__("cli"); return 0; }   /* interrupts off = ring 0 only */
EOF
gcc /tmp/ring.c -o /tmp/ring && /tmp/ring; echo "exit: $?"   # 132 = SIGILL/SIGSEGV
```

Example `01_syscall_cost.linux.cpp` (Linux pe) chala ke syscall vs userspace
call ki keemat ka number lo.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "kernel call = function call" | `syscall` instruction se ring 3→0 transition; ~100–300× mehnga |
| "malloc ne RAM de diya" | virtual reservation; physical page pehli touch pe fault (`13`) |
| "root = ring 0" | root abhi bhi ring 3; sirf zyada permissions |
| "container ka apna kernel" | host kernel share; namespaces se isolated *view* |
| "WSL2 pe latency-tune kar sakta hoon" | asli kernel, par timers/IRQ/cores host ke haath |

---

## Exercises

1. `strace -c /bin/true` chala. Kaunsi syscall sabse zyada baar? Static-link
   karke (`gcc -static`) dubara — kya farak?

   <details><summary>Answer</summary>

   Dynamic binary mein `mmap`, `openat`, `read`, `mprotect`, `close` — yeh sab
   `ld-linux.so` shared libs load kar raha hai. Static binary mein bas
   `execve`, `brk`, `arch_prctl`, `exit_group` — kyunki koi runtime linking
   nahi. HFT log: static (ya minimal-deps) binaries ka startup deterministic
   hota.
   </details>

2. Ring 3 se `cli` (interrupts disable) chalane pe process ko kaunsa signal
   milta, aur kyun?

   <details><summary>Answer</summary>

   `SIGSEGV` ya `SIGILL` (`#GP` general protection fault → kernel → signal).
   `cli` ring 0-only hai; CPU hardware trap maarta, kernel dekhta ki faulting
   code ring 3 mein tha → process ko maar deta.
   </details>

3. Tumhara `main()` `return 0` karta hai. `return` ke baad kernel exactly kya
   reclaim karta? 4 cheezein.

   <details><summary>Answer</summary>

   (1) Poora virtual address space + page tables + resident physical pages.
   (2) Saare open file descriptors (files, sockets, pipes). (3) Thread stacks
   aur kernel-side task structs. (4) Timers, `mmap` regions, locks
   (`mlock`ed memory unlock), SysV/POSIX IPC attach counts. Phir parent ko
   `SIGCHLD` + exit status queue karta (`03`).
   </details>

4. Container ke andar `cat /proc/sys/vm/nr_hugepages` badal doge — kya host
   affect hoga? Kyun / kyun nahi?

   <details><summary>Answer</summary>

   Haan, `vm.nr_hugepages` global kernel resource hai — container namespaces
   isse virtualize nahi karti (yeh koi namespaced sysctl nahi). Ek privileged
   container host ka hugepage pool badal sakta. Isi liye shared hosts pe
   containers ko `--privileged` aur host `/proc` mounts se door rakho.
   </details>

5. HFT box pe kyun chahenge ki hot thread ke core par **koi dusra runnable
   thread hi na ho**, sirf `nice` se low priority dena kaafi kyun nahi?

   <details><summary>Answer</summary>

   `nice` sirf CFS ke andar CPU-share ka weight badalta — low-nice thread phir
   bhi hot thread ko preempt kar sakta (timeslice, load balance, softirq).
   `isolcpus`/`cpuset` se core ko scheduler ke general pool se hi hata do → us
   core pe sirf woh threads chalte hain jinhe tumne explicitly pin kiya. Tab
   preemption ka source hi khatam (`11`, `15`).
   </details>

---

## Interview questions

1. Userspace aur kernel space mein farak — memory, privilege, address space?
2. `syscall` instruction kya karta, ring transition kaise hota?
3. `strace` kaise kaam karta (`ptrace`), aur woh timing ke liye kyun bura hai?
4. Container aur VM mein kernel ke perspective se farak?
5. Root user ring 0 mein chalta hai — sahi ya galat, samjhao.
6. Ek dynamically-linked "hello world" itni saari syscalls kyun karta hai?
7. HFT ke liye Linux hi kyun, Windows kyun nahi — 3 concrete wajah.

---

## Next
→ [`02-syscalls.md`](02-syscalls.md)

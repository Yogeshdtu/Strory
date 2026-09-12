# 02 — Syscalls: user → kernel transition ki keemat

## Prerequisites
- `01-linux-architecture.md` (ring 0/3, kernel vs userspace)
- `24-COMPILATION-LINKING` (libc ek wrapper layer hai)

## Yeh topic abhi kyun
File `01` ne kaha "har syscall mehnga hai". Ab **kitna** mehnga, **kyun** mehnga,
aur usse **kaise bachte** hain — yeh teenon. Yeh number tumhare har design
decision ko drive karega: logging kaise, timestamp kaise, I/O batching kyun.

---

## Syscall kya hai (mechanically)

Ek **syscall** = userspace se kernel ke ek specific function ko invoke karne ka
tareeka, ek controlled entry point ke through.

x86-64 Linux pe:

```
   userspace                          kernel
   ---------                          ------
   rax = syscall number  (e.g. 1 = write)
   rdi, rsi, rdx, r10, r8, r9 = args
   syscall            ----------->    CPU: ring 3 -> ring 0
                                      rip <- MSR_LSTAR (entry_SYSCALL_64)
                                      swapgs, kernel stack pe switch
                                      register state save
                                      sys_write(fd, buf, len) chalao
                                      return value -> rax
                     <-----------     sysret: ring 0 -> ring 3
   rax = result (ya -errno)
```

Tum yeh khud nahi likhte — **libc ka wrapper** likhta. `write(fd, buf, n)`
call karo, glibc `write` ek chhota stub hai jo `rax=1` set karke `syscall`
chalata aur `-errno` ko `errno` mein convert karta.

`man 2 syscall`, aur poori list `/usr/include/asm/unistd_64.h` mein.

---

## Keemat kahan se aati hai

Ek `getpid()` — kernel side pe literally `return task->tgid;`, ek memory read.
Phir bhi ~300 ns. Kyun?

| Cost | Kya |
|---|---|
| **Mode switch** | ring 3→0→3, `swapgs` ×2, MSR reads, stack switch |
| **Register save/restore** | kernel entry pe caller state save, exit pe restore |
| **Pipeline flush-ish** | `syscall`/`sysret` serialize karte; speculative work discard |
| **Spectre/Meltdown mitigations** | KPTI: entry/exit pe CR3 (page-table root) swap → TLB flush-ish; retpoline; IBRS/STIBP MSR writes. Yeh 2018 ke baad ~2–5× cost badha gaye |
| **Kernel ka apna kaam** | audit hooks, `seccomp` BPF filter (agar set), tracepoints |

`getpid` par yeh sab **fixed overhead** hai — kaam 1 ns ka, transition 300 ns ka.
Isi liye "chhoti-chhoti baar-baar syscalls" = death by a thousand cuts.

```bash
# Mitigations ka asar khud dekho:
cat /sys/devices/system/cpu/vulnerabilities/*      # kya mitigated hai
# boot param `mitigations=off` (isolated trading box pe) -> syscall ~2-4x sasta
```

---

## vDSO — kuch "syscalls" jo actually syscall nahi

Kernel kuch read-only, non-secret data (current time, CPU number) ko har
process ke address space mein ek chhoti shared page — **vDSO** (virtual dynamic
shared object) — ke through expose karta. `clock_gettime`, `gettimeofday`,
`getcpu`, `time` in par **koi ring transition nahi** — glibc seedha vDSO ka
code chalata jo woh shared page padhta.

```bash
cat /proc/self/maps | grep vdso        # [vdso] region har process mein
ldd /bin/ls | grep vdso                # linux-vdso.so.1 => (kernel provides)
```

Natija: `clock_gettime(CLOCK_MONOTONIC)` ~15–25 ns (vDSO), jabki `getpid()`
~300 ns (real trap). **Example `01` aur `09` dono isse naapte hain.**

> **HFT relevance:** yeh ek bada deal hai. Tum hot path mein `clock_gettime`
> chala sakte ho (timestamps for latency measurement) bina 300 ns kharch kiye.
> Par `read`, `write`, `send`, `recv`, `getrandom`, `futex` — yeh sab asli
> traps hain. Hot loop mein inko ginti se dekho.

---

## Syscall count ghatane ke tareeke

| Technique | Kaise madad karta | File |
|---|---|---|
| **Buffering** | 1000 chhoti writes → 1 badi write | `03` |
| **`mmap` file I/O** | file = memory; `read`/`write` calls hi nahi | `07` |
| **Shared memory** | 2 process ek RAM; hand-off pe koi syscall nahi | `08` |
| **`readv`/`writev` (scatter-gather)** | ek call mein multiple buffers | `05` |
| **`sendmmsg`/`recvmmsg`** | ek call mein multiple packets | folder `30` |
| **`io_uring`** | batched async I/O, ek `io_uring_enter` mein 100s ops | `30/12` |
| **Busy-poll** | `epoll_wait`/`recv` block karne ki jagah spin | `30/10` |
| **vDSO** | time/cpu reads bina trap | yahan |
| **`getpid` cache** (apna) | ek baar padho, variable mein rakho | — |

**Rule:** hot path pe naya syscall add karne se pehle poochho "iska batched ya
vDSO version hai?"

---

## `errno` — thread-local, aur ek trap

Syscall wrapper fail hone pe `-1` return karta aur `errno` set karta. `errno`
ek **thread-local** macro hai (`*__errno_location()`), global variable nahi —
warna multi-threaded code mein race hota.

```cpp
ssize_t n = ::read(fd, buf, len);
if (n < 0) {
    if (errno == EINTR)  { /* signal aaya, retry */ }
    if (errno == EAGAIN) { /* non-blocking fd, abhi data nahi */ }
    // errno ko turant padho -- agli koi bhi libc call use overwrite kar sakti
}
```

---

## Internal working: `seccomp` aur syscall filtering

Kernel entry pe, agar process ne `seccomp` BPF filter lagaya hai (sandboxing —
Chrome, systemd services, containers), to **har syscall par ek BPF program
chalta** jo decide karta allow/deny/kill. Yeh syscall cost mein aur ~20–100 ns
jodta. HFT processes usually `seccomp` nahi lagate (overhead + koi untrusted
code nahi), par container runtimes lagate hain — jaan lo.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — hot loop mein `gettimeofday`/`clock_gettime` "free" maan lena
vDSO se sasta zaroor, par ~20 ns still. 10M iterations ke loop mein har
iteration pe time padhna = 200 ms sirf timekeeping. Batch measurement karo:
start pe ek read, end pe ek read, beech mein counter.

### Trap 2 — `write(fd, buf, 1)` per byte
Har call ek trap. `printf` without buffering, ya `std::endl` (flushes!) — yehi
galti. Line-buffered stdout ek terminal pe theek, pipe/file pe disaster. `'\n'`
use karo, `std::endl` nahi (folder `04`).

### Trap 3 — `errno` ko der se padhna
```cpp
if (::close(fd) < 0) { log("close failed"); /* log() ne errno overwrite kar diya */ 
                       printf("%s\n", strerror(errno)); }   // galat errno
```
Fail ke turant baad `int e = errno;` capture karo.

### Trap 4 — `EINTR` handle na karna
Blocking syscall (`read`, `write`, `accept`, `nanosleep`) beech mein signal aane
pe `-1`/`EINTR` de sakti (agar `SA_RESTART` set nahi). Loop mein retry karo, ya
`SA_RESTART` use karo (`04`).

### Trap 5 — syscall number ko portable maan lena
`SYS_write` = 1 x86-64 pe, par ARM64 pe alag, 32-bit x86 pe alag. Kabhi raw
number hardcode mat karo; `<sys/syscall.h>` ke `SYS_*` macros use karo, aur
better — libc wrapper use karo.

### Trap 6 — "`strace` se latency naapunga"
`strace` `ptrace` use karta — har syscall pe do context switches (tracer ko).
Program 10–100× slow. Count ke liye `strace -c` theek; timing ke liye `perf
trace` ya apna `clock_gettime` bracket.

---

## > **HFT relevance**

> - **Syscall budget** ek real design constraint hai. Ek strategy jo har tick pe
>   `recv` + `clock_gettime` + `write`(log) + `send` karti hai — woh ~4 traps ×
>   ~300 ns (recv/send/write) = ~1 µs sirf transitions mein. Kernel bypass +
>   async logging + vDSO time se yeh ~100 ns ho jaata.
> - **`mitigations=off`** isolated trading boxes pe common — syscall aur context
>   switch ~2–4× sasta. Security trade-off soch-samajh ke (box internet-facing
>   nahi, koi untrusted code nahi).
> - **Timestamp strategy:** `clock_gettime(CLOCK_MONOTONIC)` (vDSO, ~20 ns) ya
>   `rdtscp` (~10 ns, calibrate karke). `CLOCK_MONOTONIC_RAW` — trap, avoid.
> - **Logging:** hot thread bytes ko ek lock-free ring mein daalti, alag thread
>   `writev` se bade chunks disk pe. Hot thread kabhi `write()` nahi maarti.

---

## Hands-on

```bash
# Linux pe -- example 01 syscall vs vDSO vs userspace call naapta
g++ -std=c++20 -O2 29-LINUX-SYSTEMS/examples/01_syscall_cost.linux.cpp -o /tmp/sc && /tmp/sc

# getpid ki laakhon calls -- strace -c se count
cat > /tmp/gp.c <<'EOF'
#include <unistd.h>
int main(){ for(int i=0;i<1000000;i++) getpid(); }
EOF
gcc -O2 /tmp/gp.c -o /tmp/gp && strace -c /tmp/gp    # ~1e6 getpid calls, total time dekho

# vDSO wali call trace mein dikhti hi nahi:
cat > /tmp/ct.c <<'EOF'
#include <time.h>
int main(){ struct timespec t; for(int i=0;i<1000000;i++) clock_gettime(CLOCK_MONOTONIC,&t); }
EOF
gcc -O2 /tmp/ct.c -o /tmp/ct && strace -c /tmp/ct    # clock_gettime ki 0 ya ~1 calls!
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "syscall = normal call" | ring transition; ~300 ns fixed overhead |
| "`clock_gettime` bhi syscall, mehnga" | vDSO se serve; ~20 ns, koi trap nahi |
| "libc ke bina syscall nahi kar sakta" | `syscall(SYS_x, ...)` se seedha; par portability jaati |
| "`errno` global hai" | thread-local macro (`*__errno_location()`) |
| "mitigations ka overhead myth hai" | KPTI/retpoline syscall+ctx-switch ~2–5× mehnga karte |

---

## Exercises

1. `getpid()` glibc mein cache hota tha (glibc < 2.25), ab nahi. Kyun hataya
   gaya cache?

   <details><summary>Answer</summary>

   `fork()` ke baad child ka PID badal jaata; glibc ka cache stale ho jaata
   agar kisi ne raw `clone`/`syscall` se child banaya (glibc `fork` wrapper
   cache update karta tha, par sab raste nahi). Correctness > 300 ns. Agar
   tumhe apni loop mein PID chahiye, khud ek baar padho aur rakh lo.
   </details>

2. Ek program `read(fd, buf, 1)` se 1 MiB file padhta. Kitni syscalls? Fix?

   <details><summary>Answer</summary>

   ~1,048,576 `read` calls (+ EOF pe ek aur) = ~1M traps ≈ 300+ ms sirf
   transitions. Fix: bada buffer — `read(fd, buf, 65536)` → ~16 calls. Ya
   `mmap` → 0 read calls. Ya `fread` (stdio ka 4–8 KiB buffer).
   </details>

3. `clock_gettime(CLOCK_MONOTONIC)` vDSO se aata, par `CLOCK_MONOTONIC_RAW`
   aksar nahi. Kyun raw slow?

   <details><summary>Answer</summary>

   `CLOCK_MONOTONIC` NTP adjustments (frequency slewing) apply karta jo kernel
   ne vDSO data page mein pre-compute kiya — userspace bas ek TSC read + scale.
   `_RAW` ko woh adjustments nahi chahiye, par uska code path historically vDSO
   mein nahi tha (kernel-version dependent) → real syscall. Latency-critical
   code `CLOCK_MONOTONIC` use kare.
   </details>

4. `strace -f -c ./server` chalaya, "restart_syscall" aur "futex" bahut dikhte
   hain. Kya bata rahe hain?

   <details><summary>Answer</summary>

   `futex` = threads mutex/condvar pe sleep/wake kar rahe (contention ya
   idle-wait). `restart_syscall` = signal ke baad ek blocking syscall
   resume hui. Bahut `futex` = lock contention ya poorly-tuned condvar loop →
   folder `26`/`28` (lock-free) dekho.
   </details>

5. `seccomp` filter syscall cost mein ~50 ns jodta. HFT process ise kyun nahi
   lagata, par ek web browser kyun lagata?

   <details><summary>Answer</summary>

   Browser untrusted JS/WASM chalata — sandbox zaroori, 50 ns/syscall acceptable
   price. HFT process poori tarah trusted code, internet-facing nahi, aur har ns
   matter karta — koi security boundary yahan chahiye hi nahi, to overhead add
   karne ka koi kaaran nahi.
   </details>

---

## Interview questions

1. `syscall` instruction step-by-step — ring transition, register handling.
2. vDSO kya hai, kaunsi calls serve karta, kyun tez?
3. Spectre/Meltdown mitigations ne syscall cost pe kya asar dala?
4. Syscall count ghatane ke 4 tareeke.
5. `errno` — storage class, aur "der se padhne" wali bug.
6. `EINTR` — kab aata, kaise handle?
7. Hot path pe `clock_gettime` OK hai par `read` nahi — kyun?

---

## Next
→ [`03-processes.md`](03-processes.md)

# 03 — Processes: fork, exec, wait

## Prerequisites
- `02-syscalls.md`
- `26-CONCURRENCY` file 02 (process vs thread — memory sharing)

## Yeh topic abhi kyun
Process banana Linux ka sabse basic "kuch aur chala do" primitive hai — shell,
build system, container runtime, sab isi pe khade hain. HFT mein tum hot path
pe process nahi bana te (mehnga), par **startup orchestration**, **watchdog /
supervisor** patterns, aur **shared-memory IPC** (`08`) ke liye yeh samajhna
zaroori. Aur `fork()` ka COW model virtual memory (`13`) ka best teacher hai.

---

## `fork()` — ek call, do return

```cpp
pid_t pid = fork();
// yahan se aage DO processes chal rahe hain, dono isi line pe
if (pid == 0)      { /* CHILD:  fork ne 0 return kiya */ }
else if (pid > 0)  { /* PARENT: fork ne child ka PID return kiya */ }
else               { /* fork FAIL (-1), errno: EAGAIN/ENOMEM */ }
```

`fork()` child ko parent ka **near-exact copy** banata:
- Same code, same open fds, same current working dir, same env.
- **Alag** PID, alag parent, apni copy of memory (COW — neeche).
- Child mein sirf ek thread (jo `fork` call karne wala tha); baaki threads
  gayab. (Isi liye multi-threaded process mein `fork` khatarnak — file `03`
  trap 4.)

### Copy-on-Write (COW) — `fork` ko sasta banata

`fork` poori memory copy **nahi** karta. Woh:
1. Child ke liye naye page tables banata jo **wahi physical pages** point karte.
2. Dono processes ke saare writable pages ko **read-only** mark karta.
3. Jab koi bhi (parent ya child) ek page likhta → **page fault** → kernel us
   ek page ki asli copy banata, writable karta. Baaki pages shared rehte.

Natija: `fork` ki cost ≈ page tables copy karne ki cost (memory map jitna bada,
utni zyada). Chhote process ~40–90 µs; bade RSS wale (GB-scale) ms-scale.
**Example `02` isse naapta hai.**

```
fork ke turant baad:                 child ne payload[0] likha:
  parent PT ---\                       parent PT ---> [page A: original]
                >--> [page A: RO]      child  PT ---> [page A': copy, RW]
  child  PT ---/                       (baaki pages abhi bhi shared+RO)
```

---

## `exec()` family — current image ko replace karo

`fork` ne copy banaya, par tum aksar chahte ho child **koi aur program** bane.
`execve()` (aur wrappers `execl`, `execlp`, `execvp`, `execvpe`):

```cpp
char* const argv[] = { (char*)"grep", (char*)"-c", (char*)"FIX", nullptr };
execvp("grep", argv);           // PATH mein "grep" dhoondho
perror("execvp");               // yahan sirf tab aayenge jab exec FAIL ho
_exit(127);
```

`exec` success pe **return nahi karta** — current process ka poora address
space (code, data, heap, stack) fenk diya jaata, naye ELF se replace hota, aur
execution naye program ke entry point se shuru. PID same rehta, fds (jo
`FD_CLOEXEC` nahi hain) inherit hote.

**"fork + exec" pattern** = "meri copy banao, phir us copy ko doosra program
bana do". Shell har command isi tarah chalata.

### `posix_spawn` / `vfork`

`fork`+`exec` mein `fork` ki COW cost bekaar hai — child turant apni memory fenk
dega. `posix_spawn()` (ya `vfork`) yeh optimize karte: page tables copy kiye
bina seedha exec. Bade parent se chhote helper spawn karne ke liye yeh
**bahut** sasta.

---

## `wait()` / `waitpid()` — child ko reap karo

Jab child `exit()` karta, woh turant gayab nahi hota. Kernel uska **exit status**
rakhta jab tak parent `wait` na kare. Beech ka state = **zombie** (`Z` in `ps`,
`<defunct>`).

```cpp
int status = 0;
pid_t done = waitpid(pid, &status, 0);        // block jab tak child na mare
if (WIFEXITED(status))   int code = WEXITSTATUS(status);   // normal exit(code)
if (WIFSIGNALED(status)) int sig  = WTERMSIG(status);      // signal ne maara
waitpid(pid, &status, WNOHANG);               // non-blocking: 0 = abhi zinda
```

- **Zombie**: child mar gaya, parent ne `wait` nahi kiya. Sirf ek PID slot +
  task struct kha raha. Bahut zombies → PID exhaustion.
- **Orphan**: parent pehle mar gaya. Child ko `init` (PID 1) / systemd **god
  leta** (`getppid()` → 1), aur wahi use reap karta. Orphan koi problem nahi.

`SIGCHLD` signal parent ko milta jab child status badalta — handler mein
`waitpid(-1, &st, WNOHANG)` loop se reap karo (`04`).

---

## Process tree aur IDs

| ID | Kya |
|---|---|
| **PID** | process ka unique id (`getpid()`) |
| **PPID** | parent ka PID (`getppid()`) — orphan pe 1 ho jaata |
| **PGID** | process group (job control; `kill(-pgid, sig)` poore group ko) |
| **SID** | session (terminal se juda; `setsid()` se detach — daemons) |
| **TGID** | thread group id = main thread ka PID; `getpid()` isi ko return karta |
| **TID** | har thread ka apna kernel id (`gettid()`); affinity/scheduling isi pe |

`pstree`, `ps -ef --forest`, `/proc/<pid>/task/` (threads), `/proc/<pid>/status`
(PPID, threads count, `Cpus_allowed`).

---

## Internal working: `clone()`

`fork()`, `pthread_create()`, aur namespace creation — sab andar se ek hi
syscall: **`clone()`**. Flags decide karte kitna share hoga:

| Flags | Result |
|---|---|
| (koi share nahi) | `fork()` — naya address space (COW) |
| `CLONE_VM | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD` | `pthread_create()` — same address space, ek thread group |
| `CLONE_NEWPID | CLONE_NEWNET | ...` | container — naye namespaces |

To "process vs thread" ka farak Linux ke liye bas **kitne `clone` flags** ka
farak hai. Ek thread = ek process jo apna address space, fds, signal handlers
share karta.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — multi-threaded process mein `fork()`
Child mein sirf calling thread survive karta. Agar kisi doosre thread ne
`malloc` ka lock hold kiya tha jab `fork` hua → child mein woh lock **hamesha ke
liye locked**, aur child ka pehla `malloc` deadlock. `fork` ke baad child mein
sirf **async-signal-safe** calls karo, phir turant `exec`. (Isi liye
`posix_spawn` prefer.)

### Trap 2 — zombie leak
```cpp
for (;;) { if (fork()==0) { do_work(); _exit(0); } /* parent kabhi wait nahi karta */ }
```
Har child zombie ban jaata. `SIGCHLD` handler + `waitpid` loop, ya
`signal(SIGCHLD, SIG_IGN)` (Linux: auto-reap), ya `SA_NOCLDWAIT`.

### Trap 3 — `exec` ke baad wala code chal gaya
`exec` return nahi karta — agar aage ka code chal raha hai, `exec` **fail**
hua (`ENOENT` — file nahi, `EACCES` — permission, `ENOEXEC` — bad format).
Hamesha `exec` ke baad `perror` + `_exit(127)`.

### Trap 4 — `exit()` vs `_exit()` child mein
`fork` ke baad child mein `exit()` (ya `return` from `main`) `atexit` handlers
aur `stdio` buffers **flush** karta — parent ke buffered data ka duplicate
print ho sakta. Child (jo exec nahi karega) `_exit()` use kare — direct
`exit_group` syscall, koi cleanup nahi.

### Trap 5 — fd leak into child
Child saare open fds inherit karta. Ek server jo `accept` karke `fork` karta,
agar client sockets pe `FD_CLOEXEC` nahi → har child ke paas har purane client
ka socket → connection kabhi properly close nahi hota. `open(..., O_CLOEXEC)`
ya `fcntl(fd, F_SETFD, FD_CLOEXEC)` (`05`).

### Trap 6 — PID reuse race
Tumhare paas `pid` hai, tumne `kill(pid, SIGTERM)` bheja — par us beech child
mar chuka aur OS ne wahi PID kisi aur ko de diya → tumne galat process maara.
`waitpid` se reap karne se pehle PID "reserved" rehta; reap ke baad `pid` stale.
Modern: `pidfd_open()` se ek stable handle.

---

## > **HFT relevance**

> - **Hot path pe koi `fork`/`exec` nahi.** COW page-table copy + TLB effects =
>   unpredictable pause. Saare helper processes startup pe hi bana lo.
> - **Supervisor pattern:** ek chhota parent process trading engine ko `fork`+
>   `exec` karta, `SIGCHLD`/`pidfd` se uski maut detect karta, restart karta,
>   alerts bhejta. Parent minimal — kam memory to `fork` sasta.
> - **Shared memory IPC:** feed handler aur strategy alag *processes* (crash
>   isolation), par ek `mmap`'d shared region se communicate (`08`) — thread ki
>   tarah fast, par ek ka crash doosre ko nahi le doobta.
> - **`pidfd` + `poll`** modern way to supervise children without `SIGCHLD`
>   handler ki async-signal-safety headache.

---

## Hands-on

```bash
# Linux pe:
g++ -std=c++20 -O2 29-LINUX-SYSTEMS/examples/02_fork_exec.linux.cpp -o /tmp/fe && /tmp/fe
strace -f /tmp/fe 2>&1 | grep -E 'clone|execve|wait'   # fork(=clone)/exec/wait dekho

# Zombie khud banao aur dekho:
cat > /tmp/zomb.c <<'EOF'
#include <unistd.h>
int main(){ if(fork()==0) _exit(0); sleep(30); }   /* parent 30s tak wait nahi karta */
EOF
gcc /tmp/zomb.c -o /tmp/zomb && /tmp/zomb & sleep 1 && ps -el | grep defunct
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`fork` poori memory copy karta" | COW — sirf page tables; pages write pe copy |
| "`exec` naya process banata" | wahi PID, image replace; `fork` naya process banata |
| "zombie = memory leak" | ek PID slot + task struct; reap se cleanup, `wait` karo |
| "orphan bura hai" | init/systemd god leta, reap karta — no problem |
| "child mein `exit()` theek hai" | `_exit()` — warna stdio buffers double-flush |
| "thread aur process bilkul alag mechanism" | dono `clone()`; sirf share-flags ka farak |

---

## Exercises

1. `fork()` ke turant baad `printf("hi\n")` — kitni baar "hi" chhapega, aur
   agar stdout ek **file** ho to?

   <details><summary>Answer</summary>

   Terminal pe: 2 baar (stdout line-buffered, `\n` pe flush — har process ek
   baar). File/pipe pe: stdout fully-buffered → "hi" dono processes ke buffer
   mein baitha → `fork` ne buffer bhi copy kiya → dono exit pe flush → agar
   `fork` se pehle kuch aur buffered tha, woh bhi **do baar** likha jaayega.
   Isi liye `fork` se pehle `fflush(nullptr)`.
   </details>

2. `posix_spawn` `fork+exec` se kab kaafi tez hota, aur kab farak nahi padta?

   <details><summary>Answer</summary>

   Jab parent ka RSS bada ho (bahut memory / bahut mappings) — `fork` ko woh
   saare page-table entries copy + RO-mark karne padte, `posix_spawn` (vfork
   under the hood) yeh skip karta. Chhote parent (few MB) se spawn karne pe
   farak ~kuch µs, ignorable.
   </details>

3. `SIGCHLD` handler mein `waitpid` **loop** mein `WNOHANG` ke saath kyun, ek
   `waitpid` kyun nahi?

   <details><summary>Answer</summary>

   Signals coalesce hote — agar 3 children lagbhag saath mein mare, tumhe sirf 1
   `SIGCHLD` mil sakta. Ek `waitpid` sirf 1 reap karega, baaki 2 zombie reh
   jaayenge. `while (waitpid(-1, &st, WNOHANG) > 0) {}` se saare pending reap.
   </details>

4. `execvp("./trade", argv)` ke baad `std::cerr << "started\n";` chal gaya. Do
   possible wajah.

   <details><summary>Answer</summary>

   `exec` fail hua: (1) `./trade` exist nahi karta / execute permission nahi
   (`ENOENT`/`EACCES`), (2) file ELF/script nahi (bad magic, `ENOEXEC`), ya
   `#!` interpreter missing. `errno`/`perror` check karo. (Third: `argv` array
   `nullptr`-terminated nahi — UB, kabhi-kabhi "kaam kar jaata".)
   </details>

5. Tumhara supervisor `kill(child_pid, SIGTERM)` bhejta hai periodically. PID
   reuse race se kaise bacho?

   <details><summary>Answer</summary>

   `pidfd_open(child_pid, 0)` se ek file descriptor lo jaise hi child bane —
   woh us **specific** process ko refer karta, PID reuse se immune. Phir
   `pidfd_send_signal(pidfd, SIGTERM, ...)` aur `poll(pidfd)` for exit. Ya
   simplest: sirf direct parent hi `kill` kare aur reap bhi wahi kare, taaki
   PID reap tak reserved rahe.
   </details>

---

## Interview questions

1. `fork()` "ek call, do return" — child aur parent kaise distinguish karte?
2. Copy-on-write — mechanism, aur `fork` ki cost kis par depend karti?
3. `exec` family — success pe return kyun nahi karti?
4. Zombie vs orphan — kaise bante, kaun problem, kaun nahi?
5. Multi-threaded process mein `fork` kyun khatarnak?
6. `fork`/`pthread_create`/container — teenon `clone()` pe kaise map hote?
7. `_exit()` vs `exit()` — child process mein kaunsa aur kyun?

---

## Next
→ [`04-signals.md`](04-signals.md)

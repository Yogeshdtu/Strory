# 04 — Signals: async interruption aur async-signal-safety

## Prerequisites
- `03-processes.md` (SIGCHLD, process lifecycle)
- `26-CONCURRENCY` (reentrancy, shared state ka idea)

## Yeh topic abhi kyun
Signal Linux ka "software interrupt" hai — tumhare code ke beech mein, kisi bhi
instruction pe, control ek handler mein chhalang sakta hai. Yeh `Ctrl-C`,
`kill`, timer expiry, segfault, aur child death — sab deliver karta. HFT mein
tum signals ko **hot path se poori tarah door** rakhna chahte ho, aur graceful
shutdown / crash diagnostics ke liye unhe **theek se** handle karna aana chahiye.
Yeh topic reentrancy ki sabse kadak lesson bhi hai.

---

## Signal kya hai

Ek chhota integer notification jo kernel ek process (ya thread) ko deliver
karta. Common:

| Signal | Default action | Kab |
|---|---|---|
| `SIGINT` (2) | terminate | `Ctrl-C` |
| `SIGTERM` (15) | terminate | polite "band ho jao" (`kill <pid>`) |
| `SIGKILL` (9) | terminate | **catch/block/ignore nahi** — kernel turant maarta |
| `SIGSEGV` (11) | core dump | bad memory access |
| `SIGABRT` (6) | core dump | `abort()`, failed `assert`, glibc heap corruption detect |
| `SIGCHLD` (17) | ignore | child mara / stopped / continued |
| `SIGPIPE` (13) | terminate | closed socket/pipe pe `write` |
| `SIGUSR1`/`SIGUSR2` | terminate | tumhare apne — e.g. "stats dump karo" |
| `SIGALRM` (14) | terminate | `alarm()` / `setitimer` timer |
| `SIGSTOP`/`SIGCONT` | stop/resume | job control (`SIGSTOP` bhi uncatchable) |

Deliver hone pe kernel: default action karta, ya (agar tumne handler set kiya)
tumhare handler ko us thread ke stack pe "inject" karta, phir wapas.

---

## Handler set karna: `sigaction` (not `signal`)

```cpp
#include <csignal>
#include <cstring>

extern "C" void on_term(int sig) {
    // yahan bahut kam kaam -- neeche "async-signal-safety" dekho
    g_shutdown = 1;                      // sig_atomic_t / atomic<int>
}

int main() {
    struct sigaction sa{};
    sa.sa_handler = on_term;
    sigemptyset(&sa.sa_mask);            // handler ke dauraan aur kaunse signals block
    sa.sa_flags = SA_RESTART;           // blocking syscalls auto-resume (EINTR na aaye)
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT,  &sa, nullptr);
}
```

**`signal()` kyun nahi:** portability ke across uska behaviour alag (handler
reset hota hai ya nahi, syscalls restart hoti ya nahi). `sigaction` explicit
aur consistent. Hamesha `sigaction`.

`SA_SIGINFO` flag + `sa_sigaction` (3-arg handler) se extra info milti —
`SIGSEGV` pe faulting address (`si_addr`), `SIGCHLD` pe child PID/status,
signal bhejne wale ka PID/UID.

---

## Async-signal-safety — sabse important rule

Handler kisi bhi point pe chal sakta hai — **beech mein `malloc` ke**, beech
mein `printf` ke, beech mein tumhare data structure ke half-updated hone ke.
Agar handler wahi cheez use kare, **corruption / deadlock**.

`malloc` ka lock main code ne hold kiya tha → handler `malloc` (ya `printf`,
`new`, `std::string`, `std::cout`...) call kare → **deadlock**.

**Handler mein sirf yeh allowed:**
- `sig_atomic_t` ya `std::atomic` flag set/read karna
- `write()` (raw syscall — async-signal-safe)
- `_exit()`, `signalfd` read, `sem_post`
- `man 7 signal-safety` mein poori list (~180 functions; NO stdio, NO malloc,
  NO locale, NO most of libc)

### Safe pattern: "flag set karo, main loop handle kare"

```cpp
volatile std::sig_atomic_t g_shutdown = 0;
extern "C" void h(int) { g_shutdown = 1; }          // bas itna

// main loop:
while (!g_shutdown) { do_one_iteration(); }
graceful_cleanup();                                 // yeh normal context, sab allowed
```

### Behtar pattern: `signalfd` ya self-pipe

Async handler ki jagah signal ko ek **fd event** bana lo:

```cpp
sigset_t m; sigemptyset(&m); sigaddset(&m, SIGTERM); sigaddset(&m, SIGINT);
sigprocmask(SIG_BLOCK, &m, nullptr);                // ab async delivery band
int sfd = signalfd(-1, &m, SFD_NONBLOCK);           // signals ab yahan se padho
// epoll/poll loop mein sfd ko baaki fds ke saath handle karo -- koi handler nahi,
// koi reentrancy problem nahi.
```

Yeh HFT-friendly hai: signal handling event loop ka hissa ban jaata, hot path
mein koi surprise jump nahi.

---

## Signals aur threads

- Ek **process-directed** signal (`kill <pid>`, `Ctrl-C`) kisi bhi ek thread ko
  deliver hota jo use block nahi kar raha.
- Har thread ka apna **signal mask** (`pthread_sigmask`). Common pattern: `main`
  saare signals block kare **`pthread_create` se pehle** → sab threads inherit
  karte → phir ek dedicated thread `sigwait()` / `signalfd` se handle kare.
- **Async-signal delivery kisi bhi thread pe** = us thread ki latency spike.
  Hot thread pe signals **mask** karo; ek "control" thread pe handle karo.

```cpp
sigset_t all; sigfillset(&all);
pthread_sigmask(SIG_BLOCK, &all, nullptr);      // main: sab block
// ... spawn hot threads (inherit: sab blocked -- koi handler jump nahi) ...
std::thread ctrl([]{
    sigset_t s; sigemptyset(&s); sigaddset(&s, SIGTERM); sigaddset(&s, SIGUSR1);
    int sig; while (sigwait(&s, &sig) == 0) handle(sig);
});
```

---

## Internal working

- Har task struct mein **pending signals** ka bitmask + **blocked** mask.
- Kernel signal ko tab deliver karta jab woh thread **user mode mein return**
  kar raha ho (syscall se, interrupt se, ya scheduler se). Isi liye ek pure
  compute loop (koi syscall nahi) signal ko thodi der "notice" nahi karta jab
  tak timer interrupt na aaye.
- `SIGKILL` aur `SIGSTOP` scheduler-level pe honte hain — process runnable hi
  nahi banta / turant marta. Isi liye uncatchable.
- Real-time signals (`SIGRTMIN`..`SIGRTMAX`): queue hote hain (coalesce nahi),
  payload (`sigqueue`) carry karte, priority order.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — handler mein `printf` / `std::cout` / `new`
Sabse common. Kabhi-kabhi "kaam karta" → phir random deadlock/crash. Sirf
`write(2, msg, len)` + flag.

### Trap 2 — `SIGPIPE` ignore na karna
Closed socket pe `write` → process **mar jaata** (default SIGPIPE = terminate).
Servers hamesha `signal(SIGPIPE, SIG_IGN)` (ya `send(..., MSG_NOSIGNAL)`, ya
`SO_NOSIGPIPE`), phir `write` ka `EPIPE` errno handle karo.

### Trap 3 — non-atomic flag
```cpp
bool g_stop = false;        // handler set karta, loop padhta
```
`bool` pe torn read theoretically possible; compiler loop mein `g_stop` ko
register mein cache kar sakta (kabhi na padhe). `volatile std::sig_atomic_t` ya
`std::atomic<bool>` (lock-free). `volatile` alone ordering nahi deta par yahan
sig_atomic_t ke saath convention hai.

### Trap 4 — `EINTR` bhoolna
Bina `SA_RESTART`, ek signal `read`/`write`/`accept`/`nanosleep`/`poll` ko
`-1`/`EINTR` se todta. Ya `SA_RESTART` set karo, ya har blocking call ko
`while ((n = read(...)) < 0 && errno == EINTR);` mein wrap.

### Trap 5 — `SIGCHLD` handler mein ek hi `waitpid`
Signals coalesce → multiple dead children, ek signal. `while (waitpid(-1, &st,
WNOHANG) > 0);` loop.

### Trap 6 — hot thread pe signal deliver hona
Tumne signals mask nahi kiye → `kill -USR1` ne exactly tumhare latency-critical
thread ko interrupt kiya → handler jump + return = µs spike, random. Hot threads
pe **sab signals block**.

### Trap 7 — signal handler se `longjmp` / C++ exception throw
Handler se `longjmp` karke main flow resume karna (`sigsetjmp` ke bina) UB.
Handler se exception throw — bhi UB (kernel-injected frame se unwind). Mat karo.

---

## > **HFT relevance**

> - **Hot threads: sab signals blocked.** Ek dedicated control thread (ya
>   `signalfd` in the event loop) handle karta shutdown, stats-dump (`SIGUSR1`),
>   config-reload. Trading threads ko kabhi interrupt nahi.
> - **Graceful shutdown:** `SIGTERM` → flag → main loop current work finish
>   karta, orders cancel karta, positions flatten karta, state persist karta,
>   phir exit. `SIGKILL` ka rasta na aane do.
> - **Crash diagnostics:** `SIGSEGV`/`SIGABRT`/`SIGBUS` handler jo sirf
>   `write()` se ek pre-formatted backtrace (pre-captured symbol table) aur
>   register dump ek fd pe likhta, phir `_exit`. Core dumps bade + slow; ek
>   fast async-safe mini-dump zyada useful.
> - **`SIGPIPE` ignore** — gateway connection drop pe process nahi marna chahiye.

---

## Hands-on

```bash
# Linux pe -- SA_RESTART ka asar
cat > /tmp/sig.c <<'EOF'
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
static volatile sig_atomic_t got=0;
void h(int s){ (void)s; got=1; write(2,"[handler]\n",10); }
int main(){
  struct sigaction sa={0}; sa.sa_handler=h;
  /* sa.sa_flags = SA_RESTART;  <- yeh comment/uncomment karke dekho */
  sigaction(SIGINT,&sa,0);
  char b[8]; ssize_t n = read(0,b,8);
  fprintf(stderr,"read returned %zd, errno-EINTR? got=%d\n", n, got);
}
EOF
gcc /tmp/sig.c -o /tmp/sig && /tmp/sig    # chalte hi Ctrl-C dabao; SA_RESTART off -> read -1/EINTR
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| handler mein `printf` chalega | sirf `write()` + atomic flag; `printf` async-signal-unsafe |
| `signal()` aur `sigaction()` same | `sigaction` — defined behaviour, `SA_RESTART`, mask control |
| `SIGKILL` handle kar lunga | uncatchable; kernel scheduler-level pe maarta |
| signal ek specific thread ko jaata | process-directed signal *kisi bhi* unblocked thread ko |
| flag `bool` kaafi hai | `volatile sig_atomic_t` / `atomic<bool>` — torn/cached se bacho |
| ek `waitpid` SIGCHLD ke liye kaafi | loop with `WNOHANG` — signals coalesce |

---

## Exercises

1. Handler `g_orders_cancelled = cancel_all();` karta hai (`cancel_all` `malloc`
   karta). Kya galat, aur fix?

   <details><summary>Answer</summary>

   `cancel_all` async-signal-unsafe (`malloc`, shared book locks). Agar signal
   tab aaya jab main code `malloc` ka lock hold kiye tha → deadlock; ya book
   half-updated → corruption. Fix: handler sirf `g_shutdown = 1` set kare;
   main loop, next safe point pe, `cancel_all()` chalaye.
   </details>

2. `SA_RESTART` set hai. Ek `poll()` timeout `-1` (infinite) ke saath chal raha,
   `SIGUSR1` aata. Kya hota?

   <details><summary>Answer</summary>

   `SA_RESTART` `poll`/`select`/`epoll_wait` ko restart **nahi** karta (yeh
   exceptions hain), chahe flag set ho. `poll` `-1`/`EINTR` return karega. Isi
   liye event loops ko hamesha `EINTR` pe loop-again karna chahiye. (`read`/
   `write` on regular files/pipes `SA_RESTART` se restart hote.)
   </details>

3. Multi-threaded server. `Ctrl-C` (`SIGINT`) exactly kis thread ko jaata?

   <details><summary>Answer</summary>

   Kisi bhi ek thread ko jo `SIGINT` block nahi kar raha — kernel chunta,
   deterministic nahi. Isi liye pattern: `main` `SIGINT` block kare sabhi
   `pthread_create` se pehle → ek dedicated thread `sigwait(SIGINT)` kare. Tab
   pata hota kaun handle karega.
   </details>

4. `signalfd` async handler se behtar kyun (HFT context)?

   <details><summary>Answer</summary>

   Async handler kisi bhi thread pe, kisi bhi instruction pe jump karta →
   unpredictable latency + async-signal-safety ki poori bandish. `signalfd` +
   masked signals: signal ek normal readable fd event ban jaata, tum use apne
   epoll loop mein, ek known safe point pe, full C++ ke saath handle karte ho.
   Koi jump, koi reentrancy issue.
   </details>

5. `SIGSEGV` handler likhna hai jo backtrace de. Kaun-kaunsi cheez pre-signal
   ready honi chahiye?

   <details><summary>Answer</summary>

   Symbol/line info pehle se resolved ya ek static table mein (dladdr/backtrace
   ke andar malloc hota — technically unsafe, par practically use hota with
   `alternate signal stack`). Ek `sigaltstack()` set (stack corruption pe bhi
   handler chale), output fd pehle se open, message buffers pre-allocated.
   Handler `backtrace()` → `backtrace_symbols_fd()` (fd version, no malloc) →
   `write()` → `_exit(128+sig)`.
   </details>

---

## Interview questions

1. Signal kya hai, kernel use kab deliver karta (user-mode return)?
2. `sigaction` vs `signal` — kyun `sigaction`?
3. Async-signal-safety — kya matlab, handler mein kya allowed?
4. "Flag set karo, main loop handle kare" pattern — kyun safe?
5. `signalfd` / self-pipe trick — problem kya solve karta?
6. Multi-threaded process mein signal delivery + per-thread mask.
7. `SIGKILL`/`SIGSTOP` uncatchable kyun?

---

## Next
→ [`05-file-descriptors.md`](05-file-descriptors.md)

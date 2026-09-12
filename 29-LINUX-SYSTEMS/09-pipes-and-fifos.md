# 09 — Pipes, FIFOs, Unix domain sockets

## Prerequisites
- `05-file-descriptors.md`
- `08-shared-memory.md` (IPC ka comparison base)

## Yeh topic abhi kyun
Pipes/FIFOs/Unix sockets = "ek machine pe do process ke beech ek byte stream".
Yeh shell pipelines, `epoll` wakeups (self-pipe trick — `04`), aur local
control channels ka mechanism hai. HFT hot path pe yeh **nahi** hote (shared
memory rings faster — `08`), par control-plane (config push, commands, health
checks) aur fd-passing (`memfd`, socket handoff) ke liye common. Aur latency
comparison "kyun shared memory" ko concrete banata hai.

---

## Anonymous pipe: `pipe()`

```cpp
int p[2];
::pipe2(p, O_CLOEXEC);          // p[0] = read end, p[1] = write end
// classic: fork ke baad ek end parent, doosra child
if (fork() == 0) {
    ::close(p[1]);              // child sirf padhega
    char buf[256]; ssize_t n;
    while ((n = ::read(p[0], buf, sizeof buf)) > 0) { /* consume */ }
    // n == 0 => write end sab jagah close ho gaya (EOF)
    _exit(0);
}
::close(p[0]);                  // parent sirf likhega
::write(p[1], "hello", 5);
::close(p[1]);                  // isse child ko EOF milega
```

- **Unidirectional**: data sirf `p[1]` → `p[0]`. Bidirectional chahiye → do
  pipes.
- **Sirf related processes** (fork se fd inherit) — anonymous, koi naam nahi.
- Kernel buffer: default 64 KiB (`fcntl(fd, F_SETPIPE_SZ, n)` se badla; max
  `/proc/sys/fs/pipe-max-size`).

### EOF aur SIGPIPE

| Situation | Kya hota |
|---|---|
| Saare **write** ends close, reader `read` kare | `read` returns `0` (EOF) |
| Saare **read** ends close, writer `write` kare | `SIGPIPE` (default: kill), warna `-1`/`EPIPE` |

Isi liye `close` karna important hai — jo end use nahi kar rahe use turant band
karo, warna EOF kabhi nahi milega (koi na koi write end khula reh gaya).

---

## Named pipe (FIFO): `mkfifo`

```bash
mkfifo /tmp/ctl.fifo
```
```cpp
int wfd = ::open("/tmp/ctl.fifo", O_WRONLY);   // BLOCK jab tak koi reader na khole
int rfd = ::open("/tmp/ctl.fifo", O_RDONLY);   // BLOCK jab tak koi writer na khole
// O_NONBLOCK: WRONLY without reader -> ENXIO; RDONLY -> turant milta
```

- Filesystem mein ek naam (special file, `p` type in `ls -l`). **Unrelated**
  processes connect kar sakte.
- Data disk pe nahi jaata — bas kernel pipe buffer, naam ek rendezvous point.
- `open` ki blocking semantics (dono end chahiye) ek classic gotcha.

Use: simple "ek command bhejo" channel — `echo "reload" > /tmp/ctl.fifo` aur
daemon padh raha. Structured protocol / multiple clients ke liye Unix socket
behtar.

---

## Unix domain socket (UDS): `AF_UNIX`

Sabse capable local IPC. Socket API (folder `30`) par network stack ke bina —
sab kuch kernel memory mein.

```cpp
int s = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);   // ya SOCK_DGRAM / SOCK_SEQPACKET
sockaddr_un addr{}; addr.sun_family = AF_UNIX;
std::strcpy(addr.sun_path, "/tmp/trade.sock");              // ya "\0abstract" (Linux abstract ns)
// server: bind + listen + accept ; client: connect
```

UDS ke **superpowers** (pipe/FIFO ke paas nahi):

| Feature | Kaam |
|---|---|
| **Bidirectional** | ek socket, dono taraf read/write |
| **`SOCK_DGRAM` / `SOCK_SEQPACKET`** | message boundaries preserve (stream mein khud framing) |
| **fd passing** (`SCM_RIGHTS`) | ek process doosre ko ek **open fd** de sakta (socket, memfd, epollfd) |
| **credential passing** (`SO_PEERCRED` / `SCM_CREDENTIALS`) | peer ka PID/UID/GID verify (auth) |
| **multiple clients** | `listen` + `accept` — ek server, N connections |
| **abstract namespace** | `\0`-prefixed name — filesystem entry nahi, auto-cleanup |

fd passing HFT-relevant hai: ek "socket manager" process venue connections
kholta aur bane-banaye sockets strategy processes ko `SCM_RIGHTS` se de deta —
strategies ko connection setup / credentials ka jhanjhat nahi.

---

## Latency / throughput: comparison

| Mechanism | Ek small message round-trip (typical) | Note |
|---|---|---|
| **Shared-memory SPSC ring** (`08`) | ~30–150 ns | no syscall on hand-off; busy-poll |
| **Unix domain socket** | ~2–5 µs | 2 syscalls (write+read) + copy + wakeup |
| **Pipe / FIFO** | ~2–5 µs | similar; syscall + kernel buffer copy + sched wakeup |
| **Loopback TCP** | ~10–30 µs | full TCP/IP stack, checksums, timers |

To hot path pe shared memory; control-plane pe UDS/pipe theek. **Example `05`
(shm)** ~15 ns/update dikhta — pipe usse ~100× slow hoga per message.

---

## Internal working

- Pipe/FIFO/`AF_UNIX` teenon andar ek kernel buffer + wait queue. `write`
  buffer mein copy karta (space na ho to block/`EAGAIN`), reader ko wake karta;
  `read` copy out karta, writer ko wake karta agar woh full pe soya tha.
- **Atomicity:** `PIPE_BUF` (Linux: 4096) se chhote `write` atomic hote —
  multiple writers ke messages interleave nahi honge. Bade writes tut sakte.
- `AF_UNIX SOCK_STREAM` = byte stream (framing tumhari); `SOCK_SEQPACKET` =
  reliable + ordered + message boundaries (best of both).
- fd passing: `sendmsg` ke `cmsg` (control message) mein `SCM_RIGHTS` + fd
  array. Kernel receiving process ki fd table mein **naya fd** banata jo **same
  open file description** point karta (dup jaisa, cross-process).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — pipe end close na karna → EOF nahi milta
Parent ne `fork` ke baad `p[1]` (write end) close nahi kiya → child ke `read`
ko kabhi `0` nahi milega (ek write end abhi bhi khula — parent ke paas). Jo
end use nahi karte, close karo.

### Trap 2 — `SIGPIPE` se process ka silent death
Reader chala gaya, writer `write` kare → `SIGPIPE` → process gayab, koi error
nahi (agar handler nahi). `signal(SIGPIPE, SIG_IGN)` + `EPIPE` handle, ya
`send(..., MSG_NOSIGNAL)`.

### Trap 3 — FIFO `open` deadlock
Do process dono `open(fifo, O_RDONLY)` — dono block, koi writer nahi. Ya ek
process pehle `O_WRONLY` khole (reader ke bina) → block. Design: ek end
`O_NONBLOCK`, ya `O_RDWR` (khud ko dono end — hacky par unblocks).

### Trap 4 — stream ko message maan lena
`SOCK_STREAM`/pipe pe `write("ABC"); write("DE")` → reader ek `read` mein
"ABCDE" (ya "AB" phir "CDE") pa sakta. Message boundaries **nahi**. Length-
prefix ya delimiter framing karo, ya `SOCK_SEQPACKET`.

### Trap 5 — `sun_path` overflow
`sockaddr_un.sun_path` ~108 bytes. Lamba path `strcpy` → truncation / stack
smash. Length check karo; abstract namespace (`\0name`) chhote rakho.

### Trap 6 — abstract socket cleanup vs pathname socket
Pathname UDS: process crash → stale socket file reh jaata → agli `bind` `EADDRINUSE`.
`unlink` before `bind`. Abstract (`\0`) sockets auto-cleanup on last close.

### Trap 7 — pipe buffer size ko unlimited maanna
64 KiB default. Fast producer + slow consumer → `write` block (blocking fd) ya
`EAGAIN` (non-blocking). Backpressure design karo; `F_SETPIPE_SZ` se badha
sakte par max limit hai.

---

## > **HFT relevance**

> - **Hot path: shared-memory rings, pipes/UDS nahi.** ~2–5 µs per message vs
>   ~30–150 ns — 20–100× farak. Pipe hot path pe ek anti-pattern.
> - **Control plane: UDS.** Config push, `SIGUSR1`-jaisa "dump stats", position
>   queries, graceful-shutdown commands — ek `AF_UNIX SOCK_SEQPACKET` per
>   component. Structured, multi-client, credential-checked.
> - **fd passing:** ek connection-manager process venue TCP sockets kholta +
>   authenticates, phir `SCM_RIGHTS` se strategy process ko ready socket deta.
> - **Self-pipe / eventfd** for waking an `epoll` loop from a signal handler or
>   another thread (`04`, `05`) — `eventfd` (8-byte counter fd) is the modern,
>   lower-overhead choice.
> - **Loopback TCP** sirf jab component alag machine pe ja sakta ho (location
>   transparency) — warna UDS strictly faster.

---

## Hands-on

```bash
# shell pipeline = pipe + dup2 + fork (05 se yaad)
strace -f -e trace=pipe2,clone,dup2,execve sh -c 'ls | wc -l' 2>&1 | grep -E 'pipe2|dup2'

# FIFO se ek command channel
mkfifo /tmp/ctl.fifo
( while read line < /tmp/ctl.fifo; do echo "got: $line"; done ) &
echo "reload-config" > /tmp/ctl.fifo
echo "flatten-all"   > /tmp/ctl.fifo
rm /tmp/ctl.fifo

# pipe buffer size
cat /proc/sys/fs/pipe-max-size

# UDS latency feel (loopback TCP se compare)
#   socat / netcat se ping-pong, `perf stat` se time -- UDS ~2-5us, TCP ~15-30us
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "pipe bidirectional hai" | unidirectional; 2 pipes ya UDS |
| "`read` returns 0 kabhi-kabhi random" | 0 = EOF = saare write ends closed |
| "stream pe message boundary milega" | nahi; framing khud, ya SEQPACKET |
| "FIFO data disk pe jaata" | kernel buffer only; naam sirf rendezvous |
| "pipe hot path ke liye theek hai" | ~µs/message; shared-mem ~ns; 20–100× slow |
| "UDS aur pipe basically same" | UDS: bidir, fd/cred passing, multi-client, dgram |

---

## Exercises

1. `fork` ke baad parent `close(p[1])` bhool gaya. Child ka `read(p[0])` loop
   kya karega jab parent kuch nahi bhej raha?

   <details><summary>Answer</summary>

   `read` **block** karega hamesha ke liye (ya jab tak koi byte aaye). EOF
   (`read` → 0) tabhi milta jab **saare** write-end fds band ho jaayein. Parent
   ke paas `p[1]` khula hai (bhool gaya close), to reader ko kabhi EOF nahi
   milega → hang. Fix: har process jo end use nahi karta, `fork` ke turant baad
   close kare.
   </details>

2. `write(pipe_fd, buf, 100000)` — atomic hai? Do writers ho to?

   <details><summary>Answer</summary>

   Nahi atomic — `PIPE_BUF` (4096) se bada hai. Do concurrent writers ke bade
   writes **interleave** ho sakte (A ke 4096 bytes, phir B ke kuch, phir A ke
   baaki). ≤ `PIPE_BUF` writes atomic hote. Multi-writer pipe pe har message
   ≤ 4096 rakho, ya ek writer, ya UDS `SOCK_SEQPACKET`.
   </details>

3. Do daemons dono `open("/tmp/x.fifo", O_RDONLY)` karte hain startup pe. Kya
   hota?

   <details><summary>Answer</summary>

   Dono block ho jaate — FIFO `O_RDONLY` `open` tab tak block karta jab tak koi
   `O_WRONLY` se na khole. Koi writer hai hi nahi → dono hang. Fix: `O_RDONLY |
   O_NONBLOCK` (turant return, phir `poll`), ya ek known writer process, ya UDS
   (jahan server `listen` karta, clients `connect`).
   </details>

4. Ek "socket broker" process venue connection banakar strategy ko dena chahta.
   Kaunsa IPC, kaunsa mechanism?

   <details><summary>Answer</summary>

   `AF_UNIX` socket broker↔strategy ke beech. Broker `socket()`+`connect()` se
   venue TCP connection banata, authenticate karta, phir `sendmsg` ke
   `SCM_RIGHTS` control message mein woh fd strategy ko bhejta. Strategy ko ek
   naya local fd milta jo **wahi** TCP connection hai. Strategy ko TLS handshake
   / credentials ka kaam nahi.
   </details>

5. `epoll` event loop mein ho, ek doosre thread se use "wake up, naya kaam hai"
   bolna hai. Pipe self-trick vs `eventfd` — kaunsa aur kyun?

   <details><summary>Answer</summary>

   `eventfd(0, EFD_NONBLOCK)`. Ek fd (do nahi jaise pipe), 8-byte counter
   semantics: waker `write(efd, &one, 8)` (counter += 1), loop `epoll` pe
   readable dekhta, `read(efd, &v, 8)` se counter clear. Kam memory, kam
   syscall overhead than self-pipe, aur multiple wakes coalesce ho jaate ek
   read mein. Self-pipe legacy fallback jab `eventfd` na ho.
   </details>

---

## Interview questions

1. Anonymous pipe vs named FIFO — kya farak, kaun kisse connect kar sakta?
2. Pipe pe EOF kab milta (`read` → 0)? `SIGPIPE` kab?
3. `PIPE_BUF` atomicity guarantee — kya, kyun important multi-writer pe?
4. Unix domain socket ke 3 features jo pipe ke paas nahi.
5. `SCM_RIGHTS` fd passing — kernel kya karta receiving side pe?
6. Latency: shared-mem ring vs UDS vs loopback TCP — order of magnitude.
7. `eventfd` vs self-pipe trick — kyun eventfd behtar.

---

## Next
→ [`10-cpu-scheduling.md`](10-cpu-scheduling.md)

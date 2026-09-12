# 05 — File descriptors: fd table, open/read/write/close, dup2

## Prerequisites
- `02-syscalls.md`
- `03-processes.md` (fd inheritance across fork/exec)

## Yeh topic abhi kyun
Linux mein **"sab kuch file hai"** — regular files, sockets, pipes, devices,
timers (`timerfd`), signals (`signalfd`), events (`eventfd`), even epoll khud.
Sab ka handle ek **file descriptor** — ek chhota non-negative integer. Agla folder
(`30`, networking) poora fd operations pe khada hai. Aur `epoll` (`30/10`) —
HFT event loop ka core — bhi bas fds ka game hai. Yahaan foundation.

---

## fd = ek integer, jo ek kernel object ko point karta

```
process ki fd table              system-wide open file table         inode / socket / pipe
-------------------              --------------------------          ---------------------
 0 -> stdin   ------------------>  [file: /dev/pts/3, offset, flags] -> tty
 1 -> stdout  ------------------>  [file: /dev/pts/3, offset, flags] -> tty
 2 -> stderr  ------------------>  [same as 1 usually]
 3 -> ------------------------->   [file: market.dat, offset=4096,  ] -> inode 91823
 4 -> ------------------------->   [socket: TCP 10.0.0.5:443        ] -> tcp_sock
```

Teen levels:
1. **Per-process fd table**: `fd number → open file description` pointer, +
   per-fd flags (`FD_CLOEXEC`).
2. **System-wide open file table**: har `open()` ek naya entry — **file offset**,
   **status flags** (`O_APPEND`, `O_NONBLOCK`), reference count.
3. **inode / socket / pipe object**: asli cheez.

`dup()`/`fork()` level-1 se level-2 pointer copy karte (offset **share** hota).
Do alag `open()` calls same file pe → do alag level-2 entries (offset **alag**).

---

## Core syscalls

```cpp
int  fd = ::open("market.dat", O_RDONLY | O_CLOEXEC);       // ya openat(dirfd, ...)
if (fd < 0) { perror("open"); return 1; }

char buf[65536];
ssize_t n = ::read(fd, buf, sizeof buf);   // n = bytes padhe (0 = EOF, -1 = error)
                                           // n < sizeof buf ho SAKTA hai -- "short read"
::lseek(fd, 0, SEEK_SET);                   // offset badlo
ssize_t w = ::write(fd, buf, static_cast<size_t>(n));  // "short write" bhi possible
::pread(fd, buf, len, offset);             // offset explicit, fd ka offset nahi chhedta
::close(fd);                               // ref count -1; 0 pe object free
```

**"Short read/write" ka rule:** `read`/`write` jitna maanga utna **karne ke
liye bound nahi**. Socket pe to common. Hamesha loop:

```cpp
size_t done = 0;
while (done < len) {
    ssize_t k = ::write(fd, p + done, len - done);
    if (k < 0) { if (errno == EINTR) continue; break; }   // real error
    done += static_cast<size_t>(k);
}
```

`writev`/`readv` — ek call mein multiple non-contiguous buffers (scatter-gather).
HFT: header + payload ko ek `writev` mein bhejo, memcpy bachao.

---

## `FD_CLOEXEC` — exec pe fd band ho jaaye

Default: fds `exec` ke paar zinda rehte. Ek server jo `fork`+`exec` karta,
child ke paas parent ke saare sockets/log fds inherit ho jaate → fd leak,
security hole, "port already in use" ghost.

- `open(..., O_CLOEXEC)` — atomic, race-free.
- `fcntl(fd, F_SETFD, FD_CLOEXEC)` — baad mein (fork race window).
- Sockets: `socket(..., SOCK_CLOEXEC)`, `accept4(..., SOCK_CLOEXEC)`.

**Rule:** har fd `O_CLOEXEC` ke saath kholo, jab tak inheritance explicitly na
chahiye (jaise stdin/out/err ya ek deliberate fd pass).

---

## `dup` / `dup2` — fd redirection

```cpp
int nfd = ::dup(fd);                 // sabse chhota free number, same file desc
::dup2(logfd, STDOUT_FILENO);        // fd 1 ab logfd ki taraf; agar 1 open tha, close hota
::dup3(logfd, STDOUT_FILENO, O_CLOEXEC);
```

Shell `command > out.txt` aise karta: `fd = open("out.txt", ...)` → `dup2(fd, 1)`
→ `close(fd)` → `exec(command)`. Ab command ka `write(1, ...)` file mein jaata.

Pipe + dup2 = `cmd1 | cmd2`:
```
pipe(p);  // p[0] read end, p[1] write end
fork -> child A: dup2(p[1], 1); close both; exec(cmd1)
fork -> child B: dup2(p[0], 0); close both; exec(cmd2)
```

---

## `/proc/<pid>/fd/` — kaunse fds khule hain

```bash
ls -l /proc/self/fd            # abhi khule fds + kis par point karte
ls -l /proc/$(pidof trader)/fd # trader process ke fds
lsof -p <pid>                  # wahi, human-friendly
cat /proc/<pid>/limits        | grep "open files"   # RLIMIT_NOFILE
```

fd leak debugging: agar `/proc/<pid>/fd/` badhta hi jaa raha (hazaaron entries),
kahin `close()` miss ho raha.

---

## Limits: `RLIMIT_NOFILE`

Har process ka ek cap — kitne fds khol sakta (`ulimit -n`, default aksar 1024
ya 1M systemd pe). Server jo laakhon connections handle karta → yeh raise karo:

```cpp
struct rlimit rl; getrlimit(RLIMIT_NOFILE, &rl);
rl.rlim_cur = rl.rlim_max;                 // hard limit tak
setrlimit(RLIMIT_NOFILE, &rl);
```

`EMFILE` (process limit) vs `ENFILE` (system-wide limit) — errno se pata.

---

## Internal working

- fd number allocation: **sabse chhota available**. `close(3); open(...)` → naya
  fd bhi 3. Isi liye fd numbers reuse hote — stale fd pe operate karna = galat
  object touch karna (sockets ke saath khatarnak).
- `read`/`write` kernel buffer se/mein **copy** karte (user↔kernel). `mmap`
  (`07`) yeh copy bacha sakta.
- `close()` sirf **is process** ka reference hatata. Agar `dup`/`fork` se aur
  refs hain, object zinda. Last close pe hi socket `FIN` bhejta / file flush
  hota.
- **`close()` ka return value check karo** — deferred write errors (NFS,
  quota) yahan surface hote. `EINTR` pe Linux fd already close kar deta —
  retry mat karo (double-close race).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — short read/write ko full maan lena
`write(fd, buf, 1000)` ne 600 return kiya. Baaki 400 tumhari zimmedari. Sockets
aur pipes pe bahut common. Loop likho.

### Trap 2 — fd leak
Har error path pe `close` bhoolna. RAII wrapper banao:
```cpp
struct Fd {
    int fd = -1;
    ~Fd() { if (fd >= 0) ::close(fd); }
    Fd(const Fd&) = delete; Fd& operator=(const Fd&) = delete;
    Fd(Fd&& o) noexcept : fd(o.fd) { o.fd = -1; }
};
```

### Trap 3 — `O_CLOEXEC` bhoolna
`fork`+`exec` karne wale code mein har fd child ko leak. Bhari security/resource
bug. Default `O_CLOEXEC`, exception explicit.

### Trap 4 — stale fd use (reuse race)
Thread A ne `close(fd=7)`, Thread B ne `open()` → mila `7`, Thread A ne purana
`fd=7` variable se `write` kar diya → naye file/socket mein garbage. fd
ownership clear rakho; close ke baad `fd = -1`.

### Trap 5 — `close()` ka result ignore
```cpp
::close(fd);   // NFS/quota ka write error yahan chhup gaya
```
Data-integrity-critical paths pe `if (::close(fd) < 0) handle;` — aur `EINTR`
pe **retry mat karo** (Linux ne fd already free kar diya).

### Trap 6 — `SIGPIPE` from `write` to closed socket
fd valid hai, par peer ne connection band kiya → `write` → `SIGPIPE` → process
mar jaata (default). `MSG_NOSIGNAL` / `signal(SIGPIPE, SIG_IGN)` (`04`).

### Trap 7 — `lseek` + concurrent access
Do threads same fd pe `lseek`+`read` → offset race. `pread`/`pwrite` (offset
argument) use karo — atomic w.r.t. fd offset.

---

## > **HFT relevance**

> - **Pre-open everything.** Market data files, log files, sockets — startup pe
>   kholo. `open()` ~µs-scale syscall + possible disk seek; hot path pe kabhi
>   nahi.
> - **`writev` for framed messages.** Order message = fixed header + variable
>   body. Ek `writev({header, body})` → ek syscall, koi concat memcpy nahi.
> - **fd limits high.** Market data multicast + N venue connections + internal
>   IPC → `RLIMIT_NOFILE` ko systemd unit / `setrlimit` se badhao.
> - **`timerfd`/`eventfd`/`signalfd`** — sab fds → ek `epoll` loop mein timers,
>   wakeups, signals, aur network sab uniformly handle. Koi alag signal handler,
>   koi alag timer thread.
> - **`O_DIRECT`** (page cache bypass) low-latency market-data recording ke liye
>   — par alignment constraints (512/4096-byte buffers + offsets).

---

## Hands-on

```bash
# Linux pe -- example 03: raw fd write per-line vs buffered
g++ -std=c++20 -O2 29-LINUX-SYSTEMS/examples/03_file_io_raw.linux.cpp -o /tmp/io && /tmp/io
strace -c -e trace=write /tmp/io       # per-line version ki ~200k write calls

# fd inheritance dekho:
cat > /tmp/inh.c <<'EOF'
#include <fcntl.h>
#include <unistd.h>
int main(){
  int fd = open("/tmp/x", O_RDWR|O_CREAT, 0644);   /* O_CLOEXEC NAHI */
  char *a[]={"sh","-c","ls -l /proc/self/fd",0};
  execv("/bin/sh", a);                              /* child ko fd dikhega */
}
EOF
gcc /tmp/inh.c -o /tmp/inh && /tmp/inh    # fd 3 -> /tmp/x child mein bhi
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`write` ne saara data likh diya" | short write possible; loop karo |
| "fd ek pointer/handle hai" | chhota int; sabse chhota free number; reuse hota |
| fds `exec` pe band ho jaate | default: zinda rehte; `O_CLOEXEC` chahiye |
| "do `open` same file = same offset" | alag open file descriptions, alag offset; `dup`/`fork` = shared offset |
| `close()` ka return ignore theek | deferred errors yahan; check karo (par EINTR pe retry nahi) |
| "sab kuch file hai" bas ek muhavara | literally: socket, pipe, timerfd, signalfd, eventfd, epollfd sab fds |

---

## Exercises

1. `int a = open("f"); int b = dup(a);` — `a` pe `lseek(a, 100, SEEK_SET)`
   karne ke baad `b` ka offset kya?

   <details><summary>Answer</summary>

   Bhi 100. `dup` ne **open file description** share kiya (level 2), aur offset
   wahin rehta hai. Compare: `int b = open("f")` (alag open) → `b` ka offset
   0 rehta.
   </details>

2. Server har connection pe fd leak kar raha (`/proc/pid/fd` badhta jaa raha).
   Debugging steps?

   <details><summary>Answer</summary>

   `ls -l /proc/<pid>/fd` — kaunse type ke fds jama ho rahe (sockets? files?).
   `lsof -p <pid>` se pattern. Code mein har `accept`/`open` ke against `close`
   audit — error paths, exception paths. RAII `Fd` wrapper introduce karo. Aur
   `strace -e trace=close` se dekho close ho bhi raha ki nahi.
   </details>

3. `cmd > file 2>&1` — shell exactly kaunse `dup2` calls karta, aur order kyun
   maayne rakhta?

   <details><summary>Answer</summary>

   `fd = open("file")`, `dup2(fd, 1)` (stdout→file), phir `dup2(1, 2)` (stderr→
   jahan stdout hai = file). Order ulta (`2>&1` pehle) karo to stderr abhi bhi
   terminal ki taraf point kar raha 1 ko copy karega → stderr terminal pe hi
   rahega. Isi liye `2>&1 >file` aur `>file 2>&1` alag behave karte.
   </details>

4. Ek order message: 24-byte header + 200-byte body, alag buffers. Kaise bhejo
   ek syscall mein bina concat?

   <details><summary>Answer</summary>

   `struct iovec iov[2] = {{hdr, 24}, {body, 200}}; writev(sock, iov, 2);` —
   kernel dono buffers ko ek shot mein socket pe likhta, koi userspace memcpy
   nahi, ek hi syscall. (Short write handle karna phir bhi zaroori — `iov`
   adjust karke retry, ya higher-level framing library.)
   </details>

5. `close(fd)` ne `EINTR` diya. Retry karoge? Kyun / kyun nahi?

   <details><summary>Answer</summary>

   Linux pe **nahi** — `close` `EINTR` return kar sakta hai par fd tab tak
   already deallocated ho chuka. Retry karne pe tum kisi doosre thread ke naye
   fd (jo wahi number mila) ko close kar sakte ho. Bas log karo aur aage badho.
   (HP-UX jaise systems pe behaviour ulta tha — isi liye `close_range`/POSIX
   ne isko finally clarify kiya: fd Linux pe hamesha closed.)
   </details>

---

## Interview questions

1. fd table ke teen levels — per-process, open file table, inode/object.
2. `dup` ke baad shared kya hota (offset), do `open` ke baad kya alag?
3. Short read/write — kab hota, kaise handle?
4. `FD_CLOEXEC` / `O_CLOEXEC` — problem kya solve karta?
5. `pread` vs `lseek`+`read` — concurrency ke liye kaunsa?
6. fd reuse race — scenario aur bachao.
7. "Everything is a file" — 4 non-file cheezein jo fd se expose hoti (socket, pipe, timerfd, eventfd, signalfd, epollfd).

---

## Next
→ [`06-proc-and-sys.md`](06-proc-and-sys.md)

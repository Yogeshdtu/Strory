# 12 — Zero-copy: sendfile, splice, MSG_ZEROCOPY, io_uring intro

## Prerequisites
- `07-sockets-api.md`, `29-LINUX-SYSTEMS` file 02 (syscall cost), file 07 (mmap)

## Yeh topic abhi kyun
Ek normal `send(fd, buf, n)` mein userspace buffer se kernel socket buffer mein
ek **memcpy** hota hai (aur `recv` mein ulta). Bade payloads pe woh copy CPU +
cache bandwidth kha jaata. **Zero-copy** APIs woh copy hata dete. HFT hot path pe
messages chhote hain (copy sasta), to zero-copy ka bada faayda **bulk** paths pe
hai — market-data recording, snapshot distribution, historical replay — aur
`io_uring` general syscall-batching + async I/O ke liye. Kernel bypass (`13`) ise
aur aage le jaata.

---

## Where the copies are (normal path)

**Send:** `user buf → (copy) → kernel skb → (DMA) → NIC`
**Receive:** `NIC → (DMA) → kernel skb → (copy) → user buf`

The DMA is free (hardware). The **user↔kernel memcpy** is the cost: for a 64 KB
payload that's ~64 KB through the CPU + cache eviction, per direction. At high
throughput (GB/s of market data to disk) it's significant; for a 40-byte order
it's noise.

---

## `sendfile(out_fd, in_fd, offset, count)` — file → socket, no userspace

```cpp
sendfile(sock, file_fd, &offset, count);   // kernel copies file page-cache -> socket, no user buffer
```
- Data never enters userspace. Kernel splices file page-cache pages into the
  socket send path (with modern NICs + `SG-DMA`, truly zero-copy; older path is
  one in-kernel copy).
- **Only file → socket** (in ≤ Linux old versions; now `out_fd` can be any file).
  Classic use: a web server sending a static file, or an HFT box serving a
  historical data file / snapshot blob to a client.
- You lose the ability to transform the bytes (no encryption/framing in
  userspace) — pair with `TCP_CORK` to prepend a header (`04`).

## `splice` / `vmsplice` / `tee` — pipe-based plumbing

```cpp
splice(in_fd, NULL, pipe_w, NULL, len, SPLICE_F_MOVE);   // in_fd -> pipe
splice(pipe_r, NULL, out_fd, NULL, len, SPLICE_F_MOVE);  // pipe -> out_fd
```
- Moves data between two fds **through a pipe** without a userspace copy — the
  pipe is just a kernel buffer of page references.
- More general than `sendfile`: socket→pipe→file (capture), file→pipe→socket,
  socket→pipe→socket (a proxy that never touches the bytes).
- `vmsplice` maps user pages into a pipe (gift pages to the kernel) — zero-copy
  *from* userspace, with lifetime rules (don't reuse the buffer until the kernel
  is done).
- `tee` duplicates a pipe's content to another pipe without consuming it — e.g.
  capture-while-forwarding.

**HFT use:** a market-data **capture** process: `splice(nic_socket → pipe →
file)` records the raw feed to disk with no userspace copy and no parsing — a
faithful, cheap tap. A **replay** does the reverse.

## `MSG_ZEROCOPY` (send) — skip the send-side copy for large buffers

```cpp
setsockopt(fd, SOL_SOCKET, SO_ZEROCOPY, &one, sizeof one);
send(fd, big_buf, n, MSG_ZEROCOPY);       // kernel pins user pages, DMAs directly
// later: read completion notifications from the socket error queue (MSG_ERRQUEUE)
// -- they tell you when the kernel is done with big_buf so you can reuse it
```
- The kernel **pins** your pages and DMAs from them directly — no copy.
- **Asynchronous ownership:** you must not modify `big_buf` until the completion
  notification arrives (via `recvmsg(MSG_ERRQUEUE)`), which adds complexity and
  a second syscall.
- **Only worth it above ~10 KB** per `send` — below that, pinning + notification
  overhead exceeds the copy it saves. Kernel even falls back to copying for
  small sends.
- Great for: streaming large snapshots, bulk historical data, log shipping.
  Pointless for order messages.

## `recvmsg` zero-copy / `SO_RCVLOWAT`
Receive-side true zero-copy is harder (the NIC decides where data lands). Options:
`TCP` `MSG_ZEROCOPY`-style receive (`recvmsg` with page-flipping, kernel-version
dependent), `SO_RCVLOWAT` to batch, or — the real answer for RX — **kernel
bypass** (`13`) / `AF_XDP`, where the NIC DMAs into a userspace-visible ring.

---

## `io_uring` — the modern async I/O + syscall-batching interface

Two shared ring buffers between userspace and kernel:
- **SQ (submission queue):** you write I/O requests (read, write, recv, send,
  accept, connect, timeout, ...) into the ring.
- **CQ (completion queue):** the kernel writes results.
- **`io_uring_enter`:** one syscall submits *many* SQEs and/or reaps
  completions. With **SQPOLL** mode, a kernel thread polls the SQ and you make
  **zero syscalls** on the hot path.

```
prep 100 recvs into the SQ ring  ->  io_uring_enter() once  ->  reap 100 CQEs
```

Why it matters:
- **Batches syscalls:** 100 operations, 1 (or 0, with SQPOLL) syscalls instead
  of 100 (`29/02`).
- **Truly async:** submit now, results later — no thread blocked per operation.
- **Registered buffers / fds:** pre-register to skip per-op setup; **fixed
  buffers** enable zero-copy-ish paths.
- **Chained ops, multishot accept/recv:** one submission, many completions.

`io_uring` is becoming the default for high-throughput servers. For HFT it's a
strong fit for the **connection/IO plane** (many venue connections, disk logging,
snapshots) and increasingly for the data path with `IORING_OP_RECV` multishot +
registered buffers — though the absolute lowest-latency feed/order path still
goes to **kernel bypass** (`13`).

> Security note: `io_uring` has had a run of kernel CVEs; some hardened / locked-
> down environments disable it (`io_uring_disabled` sysctl). Check your box
> policy before designing around it.

---

## Decision guide

| Payload / path | Use |
|---|---|
| Order message (tens of bytes), hot path | plain `send`/`writev` (copy is noise); or bypass |
| Market-data RX, hot path | busy-poll `recv` / `recvmmsg`; or bypass / `AF_XDP` |
| File → client (snapshot, historical) | `sendfile` (+`TCP_CORK` for header) |
| Feed → disk (capture), disk → feed (replay) | `splice` through a pipe |
| Large buffer → socket (bulk stream) | `MSG_ZEROCOPY` (with ERRQUEUE completion handling) |
| Many concurrent I/O ops, batching, async | `io_uring` |
| Absolute minimum latency, feed/order | kernel bypass (`13`) — no syscall, no copy, no stack |

---

## ⚠️ Traps / Common mistakes

### Trap 1 — zero-copy for small messages
`MSG_ZEROCOPY` on a 40-byte order: page-pinning + a completion notification cost
more than the memcpy you saved. Threshold is ~10 KB. The kernel may silently
copy anyway.

### Trap 2 — reusing a `MSG_ZEROCOPY` / `vmsplice` buffer too early
The kernel still owns those pages until the completion notification. Modify the
buffer before that → you send corrupt data. Track completions
(`recvmsg(MSG_ERRQUEUE)`), or use a pool and don't recycle a buffer until its
completion lands.

### Trap 3 — `sendfile` + expecting to transform bytes
Data never reaches userspace — you can't encrypt, compress, or frame it there.
Prepend headers with a separate `send` under `TCP_CORK`, or don't use
`sendfile`.

### Trap 4 — assuming `sendfile`/`splice` are always zero *copies*
Depending on kernel version, NIC scatter-gather DMA + checksum offload support,
there may still be one in-kernel copy. It always removes the **user↔kernel**
copy and the syscall-per-chunk, which is the main win.

### Trap 5 — `io_uring` without checking it's allowed
Some hardened distros/containers disable it (CVE history). `cat
/proc/sys/kernel/io_uring_disabled`. Have a fallback path.

### Trap 6 — measuring zero-copy on loopback / small data
Loopback has no real NIC DMA; small data hides the copy cost. Benchmark with the
real interface and realistic payload sizes, or the numbers lie.

---

## > **HFT relevance**

> - **Hot path (order/feed): copies are noise; latency is syscall + stack +
>   wakeup.** Zero-copy APIs don't move the needle there — **kernel bypass
>   (`13`)** does.
> - **Bulk paths win big:**
>   - **Capture:** `splice(feed_socket → pipe → file)` — record the raw feed to
>     disk with no userspace copy, no parsing.
>   - **Replay / snapshot serving:** `sendfile` (file → client socket), or
>     `splice` the other way.
>   - **Bulk streaming** (historical, log shipping): `MSG_ZEROCOPY` with
>     ERRQUEUE completion handling.
> - **`io_uring` for the IO plane:** many venue connections, async disk logging,
>   snapshot transfers — batch syscalls, SQPOLL for zero-syscall submission.
>   Check it's not disabled by policy first.
> - **`AF_XDP`** is the "zero-copy receive" answer that actually applies to the
>   feed — NIC DMAs into a userspace ring; it's on the spectrum toward full
>   bypass (`13`).

---

## Hands-on

```bash
# sendfile: serve a file, no userspace copy (strace shows sendfile, not read+write)
python3 -c "open('/tmp/big','wb').write(b'x'*(64<<20))"
cat > /tmp/sf.c <<'EOF'
#include <sys/sendfile.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
int main(){
  int s=socket(AF_INET,SOCK_STREAM,0); int one=1; setsockopt(s,SOL_SOCKET,SO_REUSEADDR,&one,4);
  struct sockaddr_in a={.sin_family=AF_INET,.sin_port=htons(9600)}; bind(s,(void*)&a,sizeof a); listen(s,1);
  int c=accept(s,0,0); int f=open("/tmp/big",O_RDONLY); off_t o=0;
  sendfile(c,f,&o,64<<20); return 0;
}
EOF
gcc /tmp/sf.c -o /tmp/sf && /tmp/sf & sleep 0.3
strace -e trace=sendfile,read,write -f nc 127.0.0.1 9600 >/dev/null   # server side: strace -p

# io_uring availability
cat /proc/sys/kernel/io_uring_disabled 2>/dev/null; uname -r
liburing-dev / io_uring-based tools (fio --ioengine=io_uring) to experiment
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "zero-copy hot path pe zaroor chahiye" | order/feed latency = syscall+stack+wakeup, not the copy |
| "`MSG_ZEROCOPY` har send pe" | only >~10 KB; needs async completion handling |
| "`sendfile` = transform + send" | bytes never in userspace; can't touch them |
| "zero-copy = literally zero copies always" | removes user↔kernel copy + per-chunk syscall; may keep one in-kernel copy |
| "`io_uring` bas ek naya API" | shared SQ/CQ rings, batched/async, SQPOLL = zero syscalls |
| "loopback benchmark bata dega" | no real DMA; use real NIC + realistic sizes |

---

## Exercises

1. Why is `MSG_ZEROCOPY` a bad choice for sending 60-byte order messages?

   <details><summary>Answer</summary>

   To avoid the copy, the kernel must **pin** your buffer's page(s), set up a
   zero-copy skb, and later post a completion notification you have to read from
   the socket error queue with a second `recvmsg`. That machinery costs more
   than memcpying 60 bytes (which is a handful of nanoseconds). The kernel knows
   this and falls back to a plain copy for small sends. `MSG_ZEROCOPY` pays off
   only above ~10 KB, and even then you need the completion-tracking logic.
   </details>

2. You want to record the raw multicast feed to disk with minimal CPU, no
   parsing. Which API, and roughly how?

   <details><summary>Answer</summary>

   `splice`. Create a pipe. Loop: `splice(feed_socket, NULL, pipe_w, NULL, len,
   SPLICE_F_MOVE)` to move datagrams from the socket into the pipe (kernel page
   references, no userspace copy), then `splice(pipe_r, NULL, file_fd, NULL,
   len, SPLICE_F_MOVE)` to move them to the file. The bytes never enter your
   process's memory; you burn almost no CPU and never touch a parser. (For
   replay, splice file → pipe → socket.)
   </details>

3. A `sendfile`-based snapshot server needs to prepend a 32-byte header to each
   file it sends. How, without giving up `sendfile`?

   <details><summary>Answer</summary>

   `setsockopt(TCP_CORK, 1)` on the socket, `send(sock, header, 32)` (userspace,
   tiny), then `sendfile(sock, file_fd, &off, count)`, then `setsockopt(TCP_CORK,
   0)` (uncork). `TCP_CORK` holds the partial segment so the header and the
   first chunk of file data go out in one segment; the file body still streams
   zero-copy. (`04` covers `TCP_CORK`.)
   </details>

4. What does SQPOLL mode give an `io_uring`-based server on its hot path?

   <details><summary>Answer</summary>

   A dedicated kernel thread polls the submission queue, so when you place SQEs
   in the ring you **don't call `io_uring_enter` at all** — zero syscalls to
   submit I/O. You just write to the SQ ring (a memory store) and read
   completions from the CQ ring (a memory load). Combined with registered
   buffers/fds and multishot recv, the syscall cost of the I/O plane approaches
   zero. Trade-off: the SQPOLL kernel thread burns a core (idle-timeout
   configurable), so you dedicate one like you would for busy-poll.
   </details>

5. Your team benchmarks `sendfile` vs `read`+`write` on loopback with 4 KB
   payloads and sees no difference. Why is the benchmark misleading?

   <details><summary>Answer</summary>

   Two problems: (1) **loopback** has no real NIC DMA — the "transmit" is just a
   memory operation, so the DMA-vs-copy distinction largely vanishes. (2) **4 KB
   is small** — the user↔kernel copy `sendfile` saves is a few µs at most, and
   syscall/setup overhead dominates at that size. Benchmark on a real NIC with
   large payloads (hundreds of KB to MB) to see `sendfile`/zero-copy's actual
   CPU and throughput advantage.
   </details>

---

## Interview questions

1. Where are the copies in a normal `send`/`recv`, and which one zero-copy removes.
2. `sendfile` — what it does, its limitation (bytes not in userspace).
3. `splice` through a pipe — the capture/replay/proxy use cases.
4. `MSG_ZEROCOPY` — the pinning + async-completion model, the ~10 KB threshold.
5. `io_uring` — SQ/CQ rings, batching, SQPOLL = zero syscalls.
6. Why zero-copy barely helps the HFT hot path but helps bulk paths a lot.
7. `AF_XDP` / kernel bypass as the real "zero-copy receive" for a feed.

---

## Next
→ [`13-kernel-bypass-intro.md`](13-kernel-bypass-intro.md)

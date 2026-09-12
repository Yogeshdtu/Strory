# 10 — epoll deep dive: level vs edge triggered, event loop design

## Prerequisites
- `09-select-poll.md` (the O(N) problem epoll solves)
- `08-blocking-vs-nonblocking.md` (non-blocking fds, EAGAIN, draining)

## Yeh topic abhi kyun
`epoll` Linux ka scalable readiness-notification mechanism hai — har server
(nginx, redis, every HFT gateway that isn't kernel-bypass) ka core. HFT hot
receiver aksar busy-polls (`08`) ya bypasses (`13`), par **order gateways,
drop-copy, control connections, N venue links** sab ek `epoll` loop mein hote.
Example `07` ek poora edge-triggered epoll server hai.

---

## The three calls

```cpp
int ep = epoll_create1(EPOLL_CLOEXEC);           // create an epoll instance (itself an fd)

epoll_event ev{};
ev.events  = EPOLLIN | EPOLLET;                   // interest set for this fd
ev.data.fd = conn_fd;                             // or ev.data.ptr = &my_conn_object;
epoll_ctl(ep, EPOLL_CTL_ADD, conn_fd, &ev);       // register (also _MOD, _DEL)

epoll_event out[256];
int n = epoll_wait(ep, out, 256, timeout_ms);     // returns only the READY fds
for (int i = 0; i < n; ++i) handle(out[i].data.fd, out[i].events);
```

**Why it scales:** you register fds **once** (kernel keeps an interest list +
a ready list, updated by the fd's driver on state change). `epoll_wait` just
returns the ready list — **O(number ready)**, not O(total watched). 100k idle
connections cost nothing per call.

`ev.data` is a union (`fd`, `ptr`, `u32`, `u64`) — stash a **pointer to your
connection struct** in `data.ptr` so you don't need a fd→object map.

---

## Level-triggered (LT) vs edge-triggered (ET)

### Level-triggered (default)
`epoll_wait` reports a fd **as long as** the condition holds. `EPOLLIN` stays
reported while there's *any* unread data. You can read a bit, return, and
you'll be told again next loop.
- Forgiving: a partial read is fine.
- Can cause repeated wakeups if you don't drain (busy-ish) — usually fine.

### Edge-triggered (`EPOLLET`)
Reported only on a **transition** — new data arrived, buffer went from
empty→non-empty. **You must drain to `EAGAIN`** on every notification, or the
remaining bytes sit unread until *more* data arrives (a fresh edge).
```cpp
for (;;) {
    ssize_t k = recv(fd, buf, sizeof buf, 0);   // fd is non-blocking
    if (k > 0)            { process(buf, k); continue; }
    if (k == 0)           { close_conn(fd); break; }
    if (errno == EAGAIN)  break;                 // fully drained -- done
    if (errno == EINTR)   continue;
    close_conn(fd); break;
}
```
- Fewer syscalls / wakeups (one per burst, not one per read).
- Requires discipline: **always drain reads, always drain writes** (`EPOLLOUT`).
- The standard high-performance server choice. Example `07` uses it.

**ET + non-blocking is a package deal** — ET on a blocking fd will hang in
`recv` after draining.

---

## Event-loop skeleton (edge-triggered)

```
ep = epoll_create1
add listen_fd (EPOLLIN)              // LT for the listener is fine
loop:
  n = epoll_wait(ep, events, MAX, timeout)
  for each ev in events[0..n]:
    if ev.fd == listen_fd:
      loop: cfd = accept4(listen, NONBLOCK|CLOEXEC)   // drain the accept queue
            if cfd < 0 && EAGAIN: break
            set TCP_NODELAY; epoll_ctl ADD cfd (EPOLLIN|EPOLLET, data.ptr=conn)
    else:
      conn = ev.data.ptr
      if ev.events & (EPOLLHUP|EPOLLERR): close_conn(conn); continue
      if ev.events & EPOLLIN:  drain reads -> parse framed messages -> app
      if ev.events & EPOLLOUT: drain conn->outbuf; if empty -> epoll_ctl MOD (drop EPOLLOUT)
```

**Writing correctly:** try `send` directly first; on `EAGAIN` (or partial),
buffer the remainder in `conn->outbuf` and `epoll_ctl MOD` to add `EPOLLOUT`.
When `EPOLLOUT` fires, flush the buffer; when it empties, **remove `EPOLLOUT`**
(else it fires every loop — a busy spin).

---

## Useful flags

| Flag | Meaning |
|---|---|
| `EPOLLET` | edge-triggered |
| `EPOLLONESHOT` | after one event, the fd is disarmed until you `EPOLL_CTL_MOD` re-arm — great for handing a fd to a worker thread without races |
| `EPOLLEXCLUSIVE` | on a shared listen fd across processes/threads, wake only **one** waiter — fixes the thundering herd on `accept` |
| `EPOLLRDHUP` | peer closed its write side (half-close) — detect it without a `recv` returning 0 |
| `EPOLLWAKEUP` | (with a wakelock) prevent system suspend while handling — mobile, not HFT |

`epoll_pwait` / `epoll_pwait2` — atomic signal mask (like `ppoll`), and
`epoll_pwait2` takes a nanosecond `timespec`.

---

## epoll for timers, signals, eventfd

Everything is a fd (`29/05`): add `timerfd_create` (timeouts, periodic ticks),
`signalfd` (signals as events — `29/04`), `eventfd` (wake the loop from another
thread — `29/09`) to the **same** `epoll`. One loop handles I/O + timers +
signals + cross-thread wakeups uniformly — no separate signal handler, no
separate timer thread.

---

## Latency: epoll_wait still sleeps

`epoll_wait(timeout = -1)` **sleeps** until an event → wakeup latency ~1–5 µs +
scheduler jitter. For the hot path:
- `epoll_wait(timeout = 0)` in a spin loop = busy-poll over multiple fds (burns
  a core, no sleep). Or
- `SO_BUSY_POLL` on the sockets so `epoll_wait` spins in-kernel briefly before
  sleeping (`08`, `15`). Or
- kernel bypass (`13`) — no epoll at all.

So: `epoll` for the **many-connections control/venue plane**; busy-poll or
bypass for the **one latency-critical feed/order path**.

---

## Internal working

- The epoll instance holds an **interest list** (a red-black tree keyed by fd)
  and a **ready list** (a linked list). When an fd becomes ready, its driver's
  callback moves it onto the ready list. `epoll_wait` splices the ready list to
  userspace. That's the O(ready) magic.
- `EPOLL_CTL_ADD` on a fd already registered → `EEXIST` (use `_MOD`).
- A **closed** fd is auto-removed from every epoll instance — *unless* it was
  `dup`'d; the underlying "file description" must be fully closed. A dangling
  registration on a dup'd-then-closed fd is a classic "phantom events" bug.
- Thread-safety: `epoll_ctl` and `epoll_wait` from different threads on the same
  epoll fd is allowed; `EPOLLONESHOT` / `EPOLLEXCLUSIVE` exist to make
  multi-thread designs correct.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `EPOLLET` without draining
Read once per event → leftover bytes stranded until the next arrival. Loop
`recv`/`accept`/`send` to `EAGAIN` every time.

### Trap 2 — `EPOLLOUT` left armed
Register `EPOLLOUT`, flush the buffer, forget to remove it → `epoll_wait`
returns the fd writable every single loop → 100% CPU spin. Remove `EPOLLOUT`
when `outbuf` is empty.

### Trap 3 — `data.fd` vs `data.ptr` confusion
`epoll_event.data` is a **union**. Set `data.ptr` OR `data.fd`, not both, and
read back the same one. Storing a `Conn*` in `data.ptr` (with the fd inside
`Conn`) is the clean pattern.

### Trap 4 — using the fd after close but before removing from epoll
Close the fd → it's auto-removed, but any events already in your `events[]`
array from this `epoll_wait` may still reference it. Track "is this conn still
alive" and skip stale entries in the same batch.

### Trap 5 — thundering herd on a shared listen fd
Many threads/processes `epoll_wait` on the same listen fd → all wake on one
connection, all but one fail `accept`. `EPOLLEXCLUSIVE`, or `SO_REUSEPORT` with
one listen fd + one epoll per thread.

### Trap 6 — treating `epoll_wait` return as "data guaranteed"
Still handle `EAGAIN` after — a spurious/edge race, another thread drained it,
or (LT) the condition cleared between wait and read.

### Trap 7 — one giant `epoll` across hot and cold fds
Mixing the latency-critical feed socket with 500 idle control connections in one
`epoll_wait` means the hot socket's event is delayed behind processing the batch.
Separate loops / threads: hot path its own (busy-poll), cold plane its own epoll.

---

## > **HFT relevance**

> - **`epoll` runs the connection plane:** N venue order connections, drop-copy,
>   risk/limit service, internal control sockets, `timerfd` timers, `signalfd`,
>   `eventfd` wakeups — one edge-triggered loop, `data.ptr` → `Conn*`.
> - **The latency-critical feed/order path is separate** — its own pinned
>   isolated thread that **busy-polls** (`epoll_wait(0)` spin, or `recv(MSG_
>   DONTWAIT)` spin, or kernel bypass). Never behind the same `epoll_wait` as
>   the cold plane.
> - **`EPOLLET` + non-blocking + drain-to-`EAGAIN`**; per-conn outbound buffer +
>   `EPOLLOUT` add/remove discipline.
> - **`EPOLLEXCLUSIVE` / `SO_REUSEPORT`** for multi-thread accept without a
>   thundering herd.
> - **`SO_BUSY_POLL`** on the sockets so even the epoll loop spins briefly
>   before sleeping when you want lower latency without a full spin (`08`, `15`).

---

## Hands-on

```bash
# Linux pe -- example 07: full edge-triggered epoll echo server
g++ -std=c++20 -O2 30-NETWORKING/examples/07_epoll_server.linux.cpp -o /tmp/eps
/tmp/eps 9400 &
for i in $(seq 200); do (printf "hi$i" | nc -q1 127.0.0.1 9400 >/dev/null &); done
wait

# strace: register once, then only epoll_wait + recv/send in the loop
strace -c -p $(pgrep eps)

# compare: a poll-based server doing epoll's job would show huge poll() arg sizes
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "epoll = poll but faster" | O(ready) via register-once + kernel ready-list; not a scan |
| "`EPOLLET`, ek recv, done" | drain to `EAGAIN` every event |
| "`EPOLLOUT` register karke chhod do" | remove when outbuf empty, else 100% spin |
| "epoll_wait never sleeps" | it sleeps (~µs wakeup); spin `timeout=0` or bypass for hot path |
| "one epoll for everything" | hot feed socket stuck behind cold-plane batch — separate |
| "closed fd needs manual `EPOLL_CTL_DEL`" | auto-removed on final close (not if dup'd) |

---

## Exercises

1. An edge-triggered epoll server: a client sends a 50 KB request in one shot.
   The handler `recv`s once (16 KB) and returns to the loop. The connection then
   appears frozen. Why, and the fix?

   <details><summary>Answer</summary>

   `EPOLLET` notifies only on the empty→non-empty transition. The handler read
   16 KB and left 34 KB in the socket buffer; there's no new edge, so
   `epoll_wait` won't report the fd again until the client sends *more* data.
   The 34 KB (and the request) sit unprocessed. Fix: on every `EPOLLIN`, loop
   `recv` until it returns `EAGAIN` — fully drain the socket each time.
   </details>

2. Your server's CPU sits at 100% with a handful of idle connections. `perf`
   shows it's all in `epoll_wait` + `send`. Likely bug?

   <details><summary>Answer</summary>

   An fd has `EPOLLOUT` armed with an empty outbound buffer. `epoll_wait`
   returns it "writable" every iteration (a socket with room in its send buffer
   is always writable), your code either sends nothing or does a no-op `send`,
   and loops immediately. Fix: only arm `EPOLLOUT` when you have buffered
   outbound data (after a partial/`EAGAIN` `send`), and `EPOLL_CTL_MOD` to
   remove `EPOLLOUT` the moment the buffer drains.
   </details>

3. Why put the latency-critical market-data socket in its own thread/loop
   instead of the main `epoll` with all the venue connections?

   <details><summary>Answer</summary>

   `epoll_wait` returns a *batch* of ready fds; you process them in a loop. If
   the feed socket's event is at position 20 in a batch of 30, it waits behind
   the processing of 19 other (cold, non-urgent) connections — added latency and
   jitter proportional to the rest of the batch. Also `epoll_wait` sleeps
   (~µs wakeup). The hot socket wants its own pinned, isolated thread that
   busy-polls just that fd (or a kernel-bypass path), so nothing is ever ahead
   of it.
   </details>

4. What does `EPOLLONESHOT` give a multi-threaded server?

   <details><summary>Answer</summary>

   After an event on that fd, epoll **disarms** it until you `EPOLL_CTL_MOD` to
   re-arm. So when the loop thread hands a ready fd to a worker thread, epoll
   won't hand the *same* fd to another loop iteration / another thread while the
   worker is still processing it — no two threads racing on one connection. The
   worker re-arms (`EPOLL_CTL_MOD` with the interest set) when it's done. Clean
   work distribution without per-fd locks.
   </details>

5. `EPOLLRDHUP` vs relying on `recv` returning 0 — why is `EPOLLRDHUP` useful?

   <details><summary>Answer</summary>

   `EPOLLRDHUP` tells you the peer closed its **write** side (sent FIN /
   half-close) as an epoll event, so you learn it immediately without having to
   attempt a `recv` (which is how you'd otherwise discover it — `recv` returns
   0). Handy when you're not currently trying to read (e.g. you're in a
   request/response phase, or you want to stop `send`ing to a peer that's
   clearly going away). You still `recv` any final buffered bytes, then close.
   </details>

---

## Interview questions

1. `epoll_create1` / `epoll_ctl` / `epoll_wait` — what each does.
2. Why is epoll O(ready) while poll is O(N)? (register-once, kernel ready list)
3. Level-triggered vs edge-triggered — the draining requirement, trade-offs.
4. The `EPOLLOUT` add/remove discipline for buffered writes.
5. `EPOLLONESHOT` and `EPOLLEXCLUSIVE` — the multi-thread problems they solve.
6. Integrating `timerfd`/`signalfd`/`eventfd` into one epoll loop.
7. Why the HFT hot path uses busy-poll/bypass instead of `epoll_wait`.

---

## Next
→ [`11-socket-options.md`](11-socket-options.md)

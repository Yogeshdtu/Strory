# 08 — Blocking vs non-blocking, O_NONBLOCK, EAGAIN

## Prerequisites
- `07-sockets-api.md`
- `29-LINUX-SYSTEMS` file 10 (scheduler, wakeup latency)

## Yeh topic abhi kyun
Ek blocking `recv()` jab data nahi hota tab tumhare thread ko **sula** deta hai
(sleep), aur data aane pe kernel use **wake** karta (~1–5 µs syscall + scheduler
latency). HFT ke liye woh wakeup latency ek P99 spike hai. Non-blocking sockets
+ `epoll` (`10`) event-driven servers ka base hain, aur **busy-poll** (spin,
never sleep) hot path ka pattern. Yeh lesson teenon modes.

---

## Three I/O modes

| Mode | `recv` with no data | CPU while waiting | Latency to react | Use |
|---|---|---|---|---|
| **Blocking** (default) | thread sleeps until data | ~0% | wakeup ~1–5 µs + sched jitter | simple clients, non-hot threads |
| **Non-blocking + readiness** (`epoll`) | returns `EAGAIN` immediately; you `epoll_wait` on many fds | ~0% (still sleeps in `epoll_wait`) | wakeup ~1–5 µs | servers with many connections (`10`) |
| **Busy-poll** (non-blocking + spin) | returns `EAGAIN`; you loop and retry | **100%** of one core | ~0 (no sleep/wake) | HFT hot path on an isolated core |

Non-blocking on its own isn't faster — it just moves the "wait" into `epoll_wait`
(still a sleep) or into your spin loop (no sleep, burns a core).

---

## Making a socket non-blocking

```cpp
// at creation (atomic, preferred):
int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
int c  = accept4(lfd, ..., SOCK_NONBLOCK | SOCK_CLOEXEC);

// or later:
int fl = fcntl(fd, F_GETFL, 0);
fcntl(fd, F_SETFL, fl | O_NONBLOCK);

// or per-call, without changing the fd:
recv(fd, buf, len, MSG_DONTWAIT);
send(fd, buf, len, MSG_DONTWAIT | MSG_NOSIGNAL);
```

`MSG_DONTWAIT` is handy when *most* of your I/O is blocking but one call needs to
be non-blocking.

---

## `EAGAIN` / `EWOULDBLOCK` — "not now, try later"

On a non-blocking socket:
- `recv` → `-1`, `errno == EAGAIN` : no data available right now.
- `send` → `-1`, `errno == EAGAIN` : the send buffer is full (peer/slow link
  not draining). You must **buffer the unsent bytes** and retry when the socket
  is writable again (register `EPOLLOUT`).

`EAGAIN` and `EWOULDBLOCK` are the same value on Linux; check both for
portability. It is **not an error** — it's flow control talking to you.

Also handle `EINTR` (signal interrupted the call) — retry.

---

## Busy-poll — the HFT hot-path pattern

```cpp
// pinned to an isolated core (29/11), this loop owns the CPU
for (;;) {
    ssize_t n = recv(fd, buf, sizeof buf, MSG_DONTWAIT);
    if (n > 0)              { handle(buf, n); continue; }
    if (n == 0)             { on_peer_close(); break; }
    if (errno == EAGAIN)    { cpu_relax(); continue; }   // _mm_pause(); no sleep
    if (errno == EINTR)     { continue; }
    on_error(); break;
}
```

- **No syscall to sleep, no wakeup latency** — the moment a packet lands in the
  socket buffer, the next `recv` gets it (~tens of ns of loop overhead).
- Costs one core at 100%. On an HFT box that core is dedicated and isolated
  anyway, so it's not "wasted" — it's the design.
- `_mm_pause()` (`PAUSE` instr) in the spin: hints the CPU (saves power, helps
  SMT sibling, avoids memory-order-violation stalls). Still not a yield.

### Kernel-assisted busy-poll: `SO_BUSY_POLL` / `net.core.busy_poll`
`setsockopt(fd, SOL_SOCKET, SO_BUSY_POLL, 50)` (µs): a *blocking* `recv` /
`epoll_wait` will **spin in the kernel** polling the NIC driver for up to 50 µs
before actually sleeping. You get near-busy-poll latency without writing the
spin loop, and it falls back to sleeping if idle. `net.core.busy_poll` /
`busy_read` set system-wide defaults (`29/06`, `15`). Middle ground between
blocking and full userspace spin.

---

## Non-blocking `connect` (recap `07`)

```cpp
fcntl(fd, F_SETFL, O_NONBLOCK);
int r = connect(fd, &addr, len);
if (r == 0)                     { /* connected immediately (rare, e.g. loopback) */ }
else if (errno == EINPROGRESS) { /* wait for EPOLLOUT, then check SO_ERROR */ }
else                           { /* real failure */ }
```

Lets one thread manage many pending connections + a timeout, instead of one
blocking `connect` per thread.

---

## Level-triggered vs edge-triggered (preview `10`)

With non-blocking fds under `epoll`:
- **Level-triggered (default):** `epoll_wait` keeps reporting a fd as long as it
  has data. You can read once and come back.
- **Edge-triggered (`EPOLLET`):** reported only on the *transition* (new data
  arrived). You **must drain to `EAGAIN`** or you'll miss the rest until more
  data arrives. Fewer wakeups; more discipline. (`10`)

Edge-triggered + non-blocking is the classic high-performance server combo.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — non-blocking socket in a blocking-style loop
`recv` returns `EAGAIN`, code treats it as an error and closes the connection.
`EAGAIN` = "try again", not "broken".

### Trap 2 — ignoring `EAGAIN` on `send`
Send buffer full, you drop the bytes on the floor → silent data loss. Buffer the
remainder, register `EPOLLOUT`, resume when writable.

### Trap 3 — edge-triggered without draining
`EPOLLET` and you `recv` once → leftover bytes sit unread until the next arrival
wakes you → stalls, "stuck" connections. Loop `recv` until `EAGAIN`.

### Trap 4 — busy-poll on a shared core
A 100% spin loop on a core the OS also schedules other threads on → you starve
them / get preempted anyway. Busy-poll only on an isolated, dedicated core
(`29/11`).

### Trap 5 — blocking `recv` on the hot path "for simplicity"
Every packet costs a wakeup (~µs) + scheduler jitter in the tail. For the
latency-critical receiver, busy-poll or `SO_BUSY_POLL`.

### Trap 6 — forgetting `EINTR`
A signal interrupts a blocking (or even non-blocking, rarely) call → `-1`/
`EINTR`. Retry, don't treat as fatal. (`SA_RESTART` doesn't cover
`epoll_wait`/`poll`.)

### Trap 7 — `O_NONBLOCK` set on the wrong fd
Setting it on the listening socket makes `accept` non-blocking (fine), but the
*accepted* fds are independent — set `SOCK_NONBLOCK` in `accept4` for those too.

---

## > **HFT relevance**

> - **Hot-path receiver: busy-poll** on a pinned isolated core — `recv(...,
>   MSG_DONTWAIT)` in a `_mm_pause()` loop. No wakeup, no scheduler jitter.
> - **`SO_BUSY_POLL` / `net.core.busy_poll`** as a lighter alternative when you
>   want a blocking API but near-spin latency, with graceful fallback to sleep
>   when idle.
> - **Everything non-blocking** — the control plane / many-connection parts use
>   `epoll` (`10`); the hot part spins.
> - **Handle `EAGAIN` on `send`** properly — a per-connection outbound buffer +
>   `EPOLLOUT`; never drop bytes.
> - **The real endgame is kernel bypass (`13`)** — you poll the NIC ring
>   directly; `EAGAIN`/`epoll`/`SO_BUSY_POLL` are all kernel-path concepts.

---

## Hands-on

```bash
# Linux pe -- example 10: blocking recv vs busy-poll recv, latency histogram
g++ -std=c++20 -O2 -pthread 30-NETWORKING/examples/10_latency_measure.linux.cpp -o /tmp/lm && /tmp/lm

# SO_BUSY_POLL needs privilege / sysctl
sysctl net.core.busy_poll net.core.busy_read
sudo sysctl -w net.core.busy_poll=50 net.core.busy_read=50

# watch a busy-poll process eat a core
top -H -p $(pgrep lm)
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "non-blocking = faster" | just moves the wait to `epoll_wait` (sleep) or a spin loop |
| "`EAGAIN` = error" | "not now"; retry / wait for readiness |
| "`send` returning short is fine to ignore" | buffer the rest + `EPOLLOUT`, or lose data |
| "`EPOLLET`, read once, done" | must drain to `EAGAIN` |
| "busy-poll anywhere" | only on a dedicated isolated core |
| "blocking `recv` fine on hot path" | wakeup ~µs + tail jitter — busy-poll instead |

---

## Exercises

1. A non-blocking `send(fd, buf, 4000)` returns 1500. What must you do, and what
   `epoll` event do you register?

   <details><summary>Answer</summary>

   Copy the unsent 2500 bytes into a per-connection outbound buffer, and
   register `EPOLLOUT` for that fd. When `epoll_wait` reports the fd writable,
   `send` from the buffer; when the buffer empties, **deregister `EPOLLOUT`**
   (else you get a busy-loop of writable events). Never discard the unsent
   bytes — that's silent data loss / a corrupted protocol stream.
   </details>

2. Under `EPOLLET`, a client sends 10 KB but your handler `recv`s once (4 KB) and
   returns. What happens to the other 6 KB?

   <details><summary>Answer</summary>

   It sits in the socket receive buffer, unread. Edge-triggered only notifies on
   the **transition** (data arrived); it won't re-notify just because unread
   data remains. Your connection appears "stuck" until the client sends *more*
   data, which triggers a fresh edge and your next `recv` finally drains it.
   Fix: on every readable event, loop `recv` until `EAGAIN`.
   </details>

3. `SO_BUSY_POLL` vs a userspace `recv(MSG_DONTWAIT)` spin loop — trade-offs.

   <details><summary>Answer</summary>

   `SO_BUSY_POLL`: you keep a blocking API (`recv` / `epoll_wait`), the kernel
   spins polling the NIC driver for up to N µs before sleeping, and it **falls
   back to sleeping when idle** (doesn't burn the core 24/7). Less code, plays
   with `epoll`, power-friendly during quiet periods. Userspace spin: lowest
   possible latency (no kernel poll loop overhead, no fallback decision), full
   control, but burns 100% of the core always and you write the loop. HFT hot
   path on a dedicated core → userspace spin (or kernel bypass); mixed / bursty
   workloads → `SO_BUSY_POLL`.
   </details>

4. Why does busy-poll only make sense on an isolated core?

   <details><summary>Answer</summary>

   A 100%-CPU spin loop on a shared core means the scheduler will preempt it to
   run other runnable threads (fairness / timeslice), reintroducing exactly the
   wakeup/scheduling jitter you spun to avoid — and meanwhile you've starved
   those other threads while you *were* running. On an isolated core (`isolcpus`
   / `cpuset` + `nohz_full`, `29/11`) there are no other runnable threads, no
   tick, no preemption — the spin loop genuinely owns the CPU and reacts in tens
   of ns.
   </details>

5. Your client uses blocking `connect` in a loop to 50 venues at startup; total
   startup takes 40 s. Fix.

   <details><summary>Answer</summary>

   Blocking `connect` serializes: each one waits for its handshake (or
   `ETIMEDOUT`, ~min for a dead peer). Make the fds non-blocking, fire all 50
   `connect`s (each returns `EINPROGRESS`), add them all to one `epoll`, wait
   for `EPOLLOUT` per fd with an overall timeout, check `SO_ERROR` on each. All
   50 handshakes proceed in parallel; startup drops to ~one RTT (plus your
   timeout for any that are down). Same pattern for reconnects.
   </details>

---

## Interview questions

1. Blocking vs non-blocking+`epoll` vs busy-poll — CPU and latency trade-offs.
2. `EAGAIN` on `recv` vs on `send` — what each means, how to handle.
3. Level-triggered vs edge-triggered — the draining requirement.
4. `SO_BUSY_POLL` — what it does, how it differs from a userspace spin.
5. Why busy-poll needs an isolated core.
6. Non-blocking `connect` — the full success/failure detection sequence.
7. Handling `EPOLLOUT` for a partially-sent buffer — including when to stop.

---

## Next
→ [`09-select-poll.md`](09-select-poll.md)

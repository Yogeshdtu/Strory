# 09 — select, poll — aur unki limitations

## Prerequisites
- `08-blocking-vs-nonblocking.md`
- `29-LINUX-SYSTEMS` file 05 (fd sets)

## Yeh topic abhi kyun
`select` aur `poll` "kai fds pe ek saath wait karo" ka pehla mechanism the. Aaj
ke Linux servers `epoll` (`10`) use karte, par `select`/`poll` samajhna zaroori:
(a) portable code / small fd counts mein abhi bhi milte, (b) unki **limitations
hi** wajah hain ki `epoll` bana — aur woh limitations HFT scale pe kyun matter
karti, yeh samajhna `epoll` ki value clear karta.

---

## `select`

```cpp
fd_set rfds;
FD_ZERO(&rfds);
FD_SET(sock1, &rfds);
FD_SET(sock2, &rfds);
int maxfd = std::max(sock1, sock2);
timeval tv{1, 0};                       // 1s timeout (NULL = block forever)

int n = select(maxfd + 1, &rfds, nullptr, nullptr, &tv);
if (n > 0) {
    if (FD_ISSET(sock1, &rfds)) { /* sock1 readable */ }
    if (FD_ISSET(sock2, &rfds)) { /* sock2 readable */ }
}
// NOTE: rfds is MODIFIED in place -- rebuild it every call
```

- `fd_set` is a **bitmask** of size `FD_SETSIZE` (**1024** on Linux, compile-time
  fixed). fd ≥ 1024 → undefined behaviour / silent corruption. Hard ceiling.
- You pass `maxfd + 1`; the kernel scans **every bit from 0 to maxfd** each call.
- The sets are **modified in place** to hold the result → you rebuild all three
  from scratch on every iteration.
- Timeout `timeval` (µs granularity); Linux `select` updates it with remaining
  time (non-portable — don't rely).

## `poll`

```cpp
pollfd fds[2];
fds[0] = { sock1, POLLIN, 0 };
fds[1] = { sock2, POLLIN | POLLOUT, 0 };

int n = poll(fds, 2, 1000);            // timeout in ms (-1 = block)
if (n > 0) {
    if (fds[0].revents & POLLIN)  { /* readable */ }
    if (fds[1].revents & POLLHUP) { /* peer hung up */ }
}
```

- No `FD_SETSIZE` limit — pass an array of any size.
- `events` (what you want) is separate from `revents` (what happened) → the
  array isn't destroyed, but you still pass the **whole array every call** and
  the kernel **scans all of it**.
- `revents` extras: `POLLHUP` (peer closed), `POLLERR`, `POLLNVAL` (bad fd) —
  set by the kernel even if you didn't ask.

---

## The O(N) problem — why they don't scale

Both `select` and `poll` are **O(N)** in the number of watched fds, **per
call**:

1. **Userspace → kernel copy** of the entire fd set / pollfd array, every call.
2. **Kernel scans all N fds** to check readiness, every call.
3. **Userspace scans all N** `revents` to find the ready ones.

With 10 connections, negligible. With **10,000 connections where only 5 are
active per iteration**, you copy + scan 10,000 entries to find 5. The cost grows
with the number of *idle* connections — exactly backwards from what you want.

`epoll` (`10`) fixes this: you register fds **once**, and `epoll_wait` returns
**only the ready ones** — O(ready), not O(total).

| | `select` | `poll` | `epoll` |
|---|---|---|---|
| fd limit | 1024 (`FD_SETSIZE`) | none | none |
| per-call cost | O(maxfd) | O(N) | O(ready) |
| set rebuilt each call | yes | no (events kept) | no (kernel keeps state) |
| copy each call | whole set | whole array | nothing (just results out) |
| edge-triggered | no | no | yes (`EPOLLET`) |
| portable | POSIX, everywhere | POSIX, everywhere | Linux only |

---

## When `select`/`poll` are still fine

- **Small, fixed fd count** (a handful) — the O(N) constant is tiny; simpler,
  portable.
- **Portable code** that must run on non-Linux (BSD has `kqueue`, not `epoll`;
  though `poll` works there too).
- **A quick timeout wrapper** around a single fd (`poll(&one, 1, timeout_ms)`) —
  common idiom for "read with a timeout".
- **Not** for a latency-critical HFT path — there you have very few fds and you
  **busy-poll** (`08`) anyway, or you're on kernel bypass (`13`).

---

## `ppoll` / `pselect` — atomic signal mask

`ppoll(fds, n, timeout_ts, sigmask)` atomically swaps the signal mask for the
duration — closes the "signal arrives between `sigprocmask` and `poll`" race for
the self-pipe / signal-handling pattern (`29/04`). Prefer `ppoll` over `poll`
when mixing signals and I/O multiplexing (or use `signalfd` + `epoll`).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `select` with fd ≥ 1024
`FD_SET(2000, &set)` writes past the `fd_set` bitmask → stack/heap corruption,
"random" crashes. Any server that can have >1024 fds must not use `select`.
`poll`/`epoll`.

### Trap 2 — not rebuilding `select` sets
`select` overwrites `rfds` with the result. Next iteration you're waiting on
whatever survived. Rebuild all sets (and recompute `maxfd`) every call.

### Trap 3 — ignoring `POLLHUP`/`POLLERR` in `revents`
The kernel sets these even if you only asked for `POLLIN`. If you only check
`revents & POLLIN`, a hung-up connection spins (poll returns it ready, you read
0/again, loop). Check `POLLHUP | POLLERR | POLLNVAL` and close.

### Trap 4 — O(N) cost with many idle connections
10k connections, 5 active → still copying+scanning 10k every call. This *is* the
limitation; move to `epoll`.

### Trap 5 — `poll` timeout is `int` milliseconds
Max ~24.8 days, and only ms granularity. For sub-ms precision use `ppoll`
(timespec) or `epoll_pwait2` (nanosecond timespec).

### Trap 6 — treating `select`/`poll` return >0 as "data is there"
It means "ready" — `recv` can still return `EAGAIN` (a race: data consumed by
another thread, or a spurious wakeup, or `POLLIN` on a listen socket with a
since-aborted connection). Always handle `EAGAIN` after a readiness signal.

---

## > **HFT relevance**

> - **Not on the hot path.** The latency-critical receiver has one or few fds and
>   **busy-polls** (`08`) or uses kernel bypass (`13`). `select`/`poll`'s
>   sleep + O(N) scan are both wrong there.
> - **`epoll` for the fan-out** — many venue connections, drop-copy, internal
>   control sockets: register once, `epoll_wait` returns only what's ready
>   (`10`).
> - **`poll(&one_fd, 1, timeout)`** is still a fine idiom for "connect/read with
>   a timeout" in setup code, and `ppoll` for signal-safe waits.
> - Know *why* `epoll` exists (the O(N)-per-call, rebuild-every-call, 1024-fd
>   problems) so you can justify it and recognise legacy code that's paying that
>   cost.

---

## Hands-on

```bash
# FD_SETSIZE
getconf FD_SETSIZE 2>/dev/null || echo 1024
python3 -c "import select; print('select max fd_set ~ FD_SETSIZE (1024 on Linux)')"

# poll-based timeout read idiom
cat > /tmp/pt.c <<'EOF'
#include <poll.h>
#include <stdio.h>
#include <unistd.h>
int main(){
  struct pollfd p = { .fd = 0, .events = POLLIN };
  int r = poll(&p, 1, 2000);            /* 2s timeout on stdin */
  printf(r==0 ? "timeout\n" : (p.revents & POLLIN ? "data\n" : "other\n"));
}
EOF
gcc /tmp/pt.c -o /tmp/pt && /tmp/pt

# see a server's fd count (would-be select/poll N)
ls /proc/$(pgrep -n nginx 2>/dev/null || echo self)/fd | wc -l
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`select` unlimited fds" | `FD_SETSIZE` = 1024; fd ≥ 1024 = UB |
| "`poll` scales to 100k connections" | O(N) copy+scan every call; use `epoll` |
| "readiness → `recv` will return data" | still handle `EAGAIN` (races/spurious) |
| "`select` sets persist" | overwritten with results; rebuild each call |
| "only check `POLLIN`" | kernel also sets `POLLHUP`/`POLLERR`/`POLLNVAL` |
| "`select`/`poll` fine for the hot receiver" | sleep + O(N); busy-poll / bypass instead |

---

## Exercises

1. A chat server using `select` works fine, then crashes with heap corruption
   once it has ~1100 connections. Cause?

   <details><summary>Answer</summary>

   `fd_set` is a fixed 1024-bit bitmask (`FD_SETSIZE`). Once accepted fds exceed
   1023, `FD_SET(fd, &set)` writes past the bitmask into adjacent memory →
   corruption, crashes that look unrelated. `select` cannot be fixed for this —
   switch to `poll` (array of any size) or `epoll`.
   </details>

2. Your `poll`-based proxy handles 20k connections but CPU is pegged even when
   traffic is light. Why, and the fix?

   <details><summary>Answer</summary>

   `poll` is O(N): every call copies the 20k-entry `pollfd` array into the
   kernel and the kernel scans all 20k for readiness, even though only a few are
   active. Light traffic = many wakeups (timeouts / a few ready fds) each paying
   the full 20k scan. Fix: `epoll` — register the 20k fds once,
   `epoll_wait` returns only the (say 10) ready ones per call. O(ready), not
   O(total).
   </details>

3. `poll` returns 1, `fds[i].revents` has `POLLHUP` but not `POLLIN`. Your code
   only checks `POLLIN`, so it does nothing — and `poll` immediately returns the
   same fd again. Explain the loop.

   <details><summary>Answer</summary>

   The peer closed the connection. The kernel reports the fd "ready" with
   `POLLHUP` set. Your code checks only `POLLIN`, sees it's not set, and skips —
   but the fd is still in your poll array and still hung up, so the very next
   `poll` returns it again, forever: a busy-loop burning CPU. Fix: check
   `revents & (POLLHUP | POLLERR | POLLNVAL)` and close/remove the fd (you can
   still `recv` first to drain any final buffered bytes — `POLLIN` may also be
   set alongside `POLLHUP`).
   </details>

4. Why is `ppoll` preferred over `poll` when your event loop also handles
   signals?

   <details><summary>Answer</summary>

   `ppoll` atomically installs a signal mask for the duration of the wait and
   restores it after. Without it, the pattern `sigprocmask(unblock);
   poll(...); sigprocmask(block);` has a race: a signal delivered *between*
   unblocking and entering `poll` runs its handler and returns before `poll`
   starts, so `poll` then blocks and misses the wakeup. `ppoll` (like
   `pselect`, `epoll_pwait`) closes that window. Alternatively, `signalfd` +
   `epoll` sidesteps signal masks entirely (`29/04`).
   </details>

5. When is `select`/`poll` genuinely the right choice today?

   <details><summary>Answer</summary>

   (1) A small, fixed number of fds (a handful) — the O(N) constant is trivial
   and the code is simpler/portable. (2) Portable code that must also run on
   systems without `epoll` (though `kqueue` on BSD is the real analogue). (3)
   The one-fd timeout idiom: `poll(&pfd, 1, timeout_ms)` for "read/connect with
   a deadline". Not for a scalable server, and not for an HFT hot path (that's
   busy-poll or kernel bypass).
   </details>

---

## Interview questions

1. `select` vs `poll` — fd limit, set handling, per-call cost.
2. Why are both O(N), and which N (idle connections count!).
3. What does `epoll` change — register-once, O(ready) results.
4. `FD_SETSIZE` — the value, and what happens if you exceed it.
5. `revents` fields the kernel sets that you didn't ask for.
6. `ppoll`/`pselect` — the race they close.
7. Legit modern uses of `poll` (small N, timeout idiom, portability).

---

## Next
→ [`10-epoll-deep.md`](10-epoll-deep.md)

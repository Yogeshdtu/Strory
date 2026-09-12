# 16 — Building servers: echo → epoll → UDP multicast receiver

## Prerequisites
- Poora folder 30 tak (`01`–`15`)
- `29-LINUX-SYSTEMS` (pinning, isolation, warm-up)

## Yeh topic abhi kyun
Ab tak ek-ek concept tha. Yeh lesson unhe **ek connected progression** mein
jodta hai — jaisa CLAUDE.md spec kehta ("connected projects, disconnected
examples nahi"). Teen versions ek hi cheez ke, har version pichhle ka problem
fix karta:
1. **blocking echo server** — simplest, ek client at a time
2. **epoll echo server** — many clients, event-driven, edge-triggered
3. **UDP multicast market-data receiver** — the actual HFT shape: join A/B,
   sequence, gap-detect, busy-poll

Examples `01`, `07`, `05`/`06` in code.

---

## V1 — blocking echo server (example `01`)

```
socket -> setsockopt(SO_REUSEADDR) -> bind -> listen -> accept
loop: recv -> send_all -> (peer FIN -> close, next accept)
```

- One connection at a time. `recv` blocks; while serving client A, client B
  waits in the `listen` backlog.
- Teaches: the call sequence, short-write handling (`send_all` loop),
  `MSG_NOSIGNAL`, `recv` return 0 = orderly close, `TCP_NODELAY` on the
  accepted fd.
- **Limitation:** no concurrency. Thread-per-connection would scale to hundreds;
  beyond that, context-switch overhead and memory dominate → event loop.

## V2 — epoll echo server (example `07`)

```
socket(NONBLOCK) -> SO_REUSEADDR|SO_REUSEPORT -> bind -> listen
ep = epoll_create1 ; add listen_fd (EPOLLIN)
loop:
  epoll_wait
  for each ready:
    listen_fd  -> accept4(NONBLOCK|CLOEXEC) loop to EAGAIN; TCP_NODELAY; add (EPOLLIN|EPOLLET)
    client_fd  -> if HUP/ERR: close
                  else drain recv to EAGAIN; echo (buffer + EPOLLOUT on partial)
```

- Non-blocking + edge-triggered: one thread handles thousands of connections;
  `epoll_wait` returns only the ready ones (`09`, `10`).
- Teaches: `epoll` API, ET draining discipline, `accept4` loop,
  `SO_REUSEPORT` (multi-worker), `EPOLLHUP`/`EPOLLERR`, the `EPOLLOUT`
  add/remove dance for partial writes.
- **Limitation for HFT:** `epoll_wait` still **sleeps** (~µs wakeup + scheduler
  jitter). Fine for the connection plane; not for the latency-critical feed.
  Next version drops the sleep.

## V3 — UDP multicast market-data receiver (examples `05` + `06`)

The real HFT shape. Not "a server" — a **feed handler**:

```
for each of feed A, feed B:
    socket(SOCK_DGRAM) -> SO_REUSEADDR|SO_REUSEPORT -> big SO_RCVBUF
    -> bind(port) -> IP_ADD_MEMBERSHIP(group, imr_ifindex = market-data NIC)
enable SO_TIMESTAMPING (HW RX) on both
pin this thread to an isolated core ; mlock buffers ; warm the code path

loop (busy-poll, no sleep):
    n = recvmmsg(fdA, msgsA, BATCH, MSG_DONTWAIT)   // then fdB
    for each datagram:
        parse header -> seq
        if seq == expected:        deliver(payload); expected = seq + 1
        else if seq > expected:    record gap(expected .. seq-1); deliver; expected = seq + 1
        else:                      // seq <= last delivered -> dup (the other feed) -> drop
    if both feeds returned EAGAIN: cpu_relax()      // _mm_pause(), still no sleep
    service gaps: if a gap is old enough and unfilled by the other feed ->
                  request snapshot / retransmit (off this hot loop)
```

- **A/B arbitration:** feed both A and B into one sequencer; the first copy of
  each sequence wins, the second is a dup and dropped. Most single-path losses
  vanish for free (`06`).
- **Busy-poll on an isolated core** (`08`, `29/11`): no `epoll_wait`, no sleep,
  no wakeup jitter — react in tens of ns.
- **`recvmmsg`** batches the syscall for bursts (`05`); the endgame is **kernel
  bypass** (`13`) which removes the syscall entirely.
- **Gap recovery is off the hot loop** — a separate thread/state machine does
  snapshot/retransmit so the fast path never blocks (`05`).
- **HW RX timestamps** (`14`) for latency measurement: HW-RX-TS → your
  deliver-TS is the number you drive down.

---

## The progression, summarised

| | V1 blocking | V2 epoll | V3 feed handler |
|---|---|---|---|
| Concurrency | 1 conn | thousands (1 thread) | 2 feeds (A/B), 1 pinned thread |
| Wait mechanism | blocking `recv` | `epoll_wait` (sleeps) | busy-poll (no sleep) |
| Transport | TCP | TCP | UDP multicast |
| Loss handling | TCP does it | TCP does it | **you** — seq + A/B + snapshot |
| Latency | ~wakeup + stack | ~wakeup + stack | ~tens of ns (+ bypass → less) |
| HFT role | teaching | order/control plane | the market-data hot path |

Each step removes the previous step's bottleneck: V1's single-connection limit →
V2's event loop; V2's sleep/wakeup → V3's busy-poll; and V3's remaining syscall
→ kernel bypass (`13`).

---

## Putting it in the box

- **Feed handler (V3):** own isolated core, `mlockall`, warm-up, HW timestamps,
  writes decoded ticks into a lock-free SPSC ring (folder `28`) for the
  strategy.
- **Order gateway:** an epoll loop (V2-style) over N venue TCP connections —
  `TCP_NODELAY`, `writev` framing, app heartbeat + `TCP_USER_TIMEOUT`, per-conn
  outbound buffer + `EPOLLOUT` (`10`, `11`). Reads order intents from a ring the
  strategy fills.
- **Control plane:** another epoll loop — `signalfd`, `timerfd`, config socket,
  health checks, drop-copy (`10`, `29/04`).
- **Everything else** (logging, monitoring) on housekeeping cores, kernel stack.

---

## ⚠️ Traps (composite — details in earlier lessons)

### Trap 1 — one epoll loop for hot feed + cold connections
The feed event waits behind the rest of the batch (`10` trap 7). Separate the
hot path onto its own pinned busy-poll thread.

### Trap 2 — V2 for the market-data feed
`epoll_wait` sleeps → wakeup jitter on every packet. V3 (busy-poll) for the
feed; V2 for the connection plane.

### Trap 3 — no A/B arbitration in V3
Single feed → every gap is a snapshot/retransmit round-trip. Join both, dedup by
sequence (`06`).

### Trap 4 — gap recovery inline in the hot loop
A snapshot request in the busy-poll loop blocks the fast path. Hand gaps to a
separate recovery thread.

### Trap 5 — forgetting the box tuning
V3 on an untuned box (no isolation, IRQs on hot cores, THP, cold pages) keeps
its jitter (`29/18`, `15`). Tune first, then measure.

### Trap 6 — ET draining skipped (V2)
`recv`/`accept` once per event under `EPOLLET` → stranded data / connections
(`08`, `10`). Loop to `EAGAIN`.

### Trap 7 — no framing on the TCP paths (V1/V2)
`recv` gives you a byte stream; length-prefix your messages (`07`).

---

## > **HFT relevance**

> This progression *is* the HFT networking stack in miniature:
> - **Feed handler** = V3: UDP multicast A/B, sequence + gap detect,
>   busy-poll on an isolated core, `recvmmsg` (→ kernel bypass), HW timestamps,
>   decoded ticks into a lock-free ring. Recovery off the hot loop.
> - **Order gateway** = V2: epoll over venue TCP connections, `TCP_NODELAY`,
>   `writev` framing, heartbeats, `EPOLLOUT` buffering.
> - **Control plane** = V2: epoll over `signalfd`/`timerfd`/config/health.
> - **The whole thing sits on a box tuned per `29/18` + `15`**, with each hot
>   thread pinned to its own isolated physical core and everything warmed at
>   startup.
> - You build V1 → V2 → V3, measuring at each step, so you know *which* change
>   bought *which* microseconds (CLAUDE.md's performance-engineering process).

---

## Hands-on

```bash
# V1
g++ -std=c++20 -O2 30-NETWORKING/examples/01_tcp_echo_server.linux.cpp -o /tmp/v1 && /tmp/v1 9099 &
g++ -std=c++20 -O2 30-NETWORKING/examples/02_tcp_client.linux.cpp -o /tmp/cli && /tmp/cli 127.0.0.1 9099 20000

# V2
g++ -std=c++20 -O2 30-NETWORKING/examples/07_epoll_server.linux.cpp -o /tmp/v2 && /tmp/v2 9400 &
for i in $(seq 500); do (printf "m$i" | nc -q1 127.0.0.1 9400 >/dev/null &); done ; wait

# V3 pieces
g++ -std=c++20 -O2 -pthread 30-NETWORKING/examples/06_multicast_receiver.linux.cpp -o /tmp/mc && /tmp/mc 239.1.2.3 9300 lo
g++ -std=c++20 -O2 30-NETWORKING/examples/05_udp_receiver.linux.cpp -o /tmp/rx
g++ -std=c++20 -O2 30-NETWORKING/examples/04_udp_sender.linux.cpp   -o /tmp/tx
/tmp/rx 9200 100000 & sleep 0.3 ; /tmp/tx 127.0.0.1 9200 100000 5

# measure at each step
g++ -std=c++20 -O2 -pthread 30-NETWORKING/examples/10_latency_measure.linux.cpp -o /tmp/lm && /tmp/lm
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "epoll server = fastest possible" | still sleeps; feed handler busy-polls / bypasses |
| "market data ke liye bhi TCP server likh do" | UDP multicast + A/B + your sequencing |
| "gap recovery loop mein daal do" | separate thread — never block the fast path |
| "ek epoll loop for everything" | hot feed separate from cold connection plane |
| "server code portable rakho" | HFT feed handler is Linux + NIC-specific by nature |
| "V3 likh diya, ho gaya" | untuned box keeps its jitter — `29/18` + `15` first, then measure |

---

## Exercises

1. You have V2 (epoll echo) working for 5000 connections. What single change
   turns it toward "market-data feed handler", and what must you add?

   <details><summary>Answer</summary>

   Change the transport to UDP multicast (join A/B groups, `recvmmsg`) and drop
   `epoll_wait` for a **busy-poll** loop on an isolated pinned core. Then add:
   per-packet **sequence handling** (expected/gap/dup), **A/B arbitration**
   (feed both into one sequencer, dedup), an **off-hot-path recovery** path
   (snapshot / retransmit request), **HW RX timestamps** for measurement, and
   **`mlock` + warm-up**. TCP's reliability is gone — that machinery is now
   yours.
   </details>

2. In V2, why is the listener added level-triggered but the client sockets
   edge-triggered?

   <details><summary>Answer</summary>

   The listener sees relatively few events and `accept4` in a loop drains the
   queue anyway, so LT is simple and safe (you'll be re-notified if you don't
   fully drain). Client sockets can be numerous and chatty; ET gives one wakeup
   per burst instead of one per readable moment, cutting `epoll_wait` churn — at
   the cost of the "must drain to `EAGAIN`" discipline. Either works for the
   listener; ET is the scaling choice for the many client fds.
   </details>

3. Why must gap recovery (snapshot/retransmit) run off the V3 hot loop?

   <details><summary>Answer</summary>

   The hot loop's job is to consume the live feed with sub-microsecond latency
   and never stall. A snapshot request is a TCP round-trip to a possibly-busy
   server (ms); doing it inline blocks every subsequent live packet behind it —
   you'd fall further behind exactly when you're already behind. Instead, the
   hot loop records the gap (a small note: "missing 1000–1004") and keeps
   consuming; a separate thread services the gap list against the snapshot/
   retransmit channel and merges recovered packets back in by sequence.
   </details>

4. Sketch how decoded ticks get from the V3 feed handler to the strategy without
   a lock on the hot path.

   <details><summary>Answer</summary>

   A **lock-free SPSC ring** (folder `28`): the feed-handler thread is the sole
   producer (`try_push` decoded tick, `release` the head index), the strategy
   thread is the sole consumer (`acquire` the head, read, `release` the tail).
   No mutex, no allocation, no syscall — a bounded ring of pre-allocated tick
   slots. If multiple strategies consume, either one MPSC ring or one SPSC ring
   per strategy (feed handler writes each). Cross-process variant: put the ring
   in shared memory (`29/08`).
   </details>

5. You built V1 → V2 → V3 and measured p50 round-trip at each: 18 µs → 12 µs →
   3 µs. A reviewer asks "which change bought the 9 µs from V2 to V3?" How do
   you answer rigorously?

   <details><summary>Answer</summary>

   Don't attribute it to one thing — decompose. V2→V3 changed *three* things:
   transport (TCP→UDP), wait (epoll sleep → busy-poll), and batching
   (`recvmmsg`). Measure them one at a time: (a) V2 but with `epoll_wait(0)`
   spin instead of blocking → isolates the wakeup cost; (b) swap TCP→UDP with
   the same wait model → isolates the stack difference; (c) add `recvmmsg`
   under load → isolates batching. Use HW RX timestamps (`14`) to split "wire+
   NIC" from "kernel+wakeup" from "app". That's the CLAUDE.md process: change
   one variable, re-measure, explain what moved.
   </details>

---

## Interview questions

1. The V1→V2→V3 progression — what bottleneck each step removes.
2. Blocking vs epoll vs busy-poll — where each belongs in an HFT stack.
3. Why the market-data path is a "feed handler", not "a server".
4. A/B arbitration + sequencing — the V3 receive algorithm.
5. Why gap recovery runs off the hot loop.
6. How decoded data crosses from feed handler to strategy lock-free.
7. The full box: which threads on which cores, which transport, which wait model.

---

## Next
→ [`17-exercises.md`](17-exercises.md)

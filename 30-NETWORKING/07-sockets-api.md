# 07 — Sockets API: socket, bind, listen, accept, connect, send/recv

## Prerequisites
- `29-LINUX-SYSTEMS` file 05 (file descriptors, short read/write)
- `03-tcp-deep.md`, `05-udp.md`

## Yeh topic abhi kyun
Ab tak concepts the — ab actual API. Har networking program in ~8 calls pe khada
hai. Yeh lesson unhe pin karta: kaun kya karta, kaunsa error kya matlab, aur
framing (TCP byte stream ko messages mein todna) kaise. Examples `01`/`02` yeh
skeleton hain.

---

## The call sequence

### TCP server
```
socket()  -> setsockopt(SO_REUSEADDR)  -> bind()  -> listen()  -> accept()  -> recv()/send()  -> close()
```

### TCP client
```
socket()  -> [connect() ]  -> send()/recv()  -> close()
   (optional bind() to pick source IP/port)
```

### UDP (both ends)
```
socket()  -> bind() (receiver)  -> [connect() (sets default peer) ]  -> sendto()/recvfrom() or send()/recv()  -> close()
```

---

## Each call

### `socket(domain, type, protocol)`
```cpp
int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);   // TCP/IPv4
//              AF_INET6                SOCK_DGRAM              UDP
//              AF_UNIX                 SOCK_SEQPACKET         (29/09)
```
`SOCK_CLOEXEC` (and `SOCK_NONBLOCK`) in the type flags — atomic, no `fcntl` race
(`29/05`). Returns an fd or `-1`.

### `bind(fd, addr, addrlen)`
Assigns a local address (IP + port). Server: bind the listen port. Client:
usually skip (kernel picks an ephemeral port + source IP by route); bind
explicitly to pin the source IP on a multi-NIC box or use a fixed source port.
`EADDRINUSE` → port taken (or TIME_WAIT — `SO_REUSEADDR`).

### `listen(fd, backlog)`
Marks a TCP socket passive. `backlog` = max **completed** connections waiting to
be `accept()`ed (Linux; the SYN queue is separate, `net.core.somaxconn` caps the
effective backlog). Too small + slow `accept` → clients get connection drops /
retries under load.

### `accept(fd, addr*, addrlen*)` / `accept4(fd, addr*, addrlen*, flags)`
Pops one completed connection, returns a **new fd** for it. `accept4` with
`SOCK_CLOEXEC | SOCK_NONBLOCK` — set both atomically. On a non-blocking listen
socket, `EAGAIN` = no pending connection.

### `connect(fd, addr, addrlen)`
TCP: initiates the handshake; blocks until ESTABLISHED (or `ECONNREFUSED` /
`ETIMEDOUT` / `EHOSTUNREACH`). Non-blocking: returns `EINPROGRESS`; finish by
waiting for writable in `epoll` then checking `SO_ERROR`. UDP: no packets — sets
the default peer + source filter (`05`).

### `send`/`recv` (and `sendto`/`recvfrom`, `sendmsg`/`recvmsg`, `writev`/`readv`, `sendmmsg`/`recvmmsg`)
```cpp
ssize_t n = recv(fd, buf, len, flags);
//   > 0 : bytes received
//   = 0 : orderly shutdown by peer (TCP FIN) -- NOT an error, connection done
//   < 0 : error; errno: EAGAIN/EWOULDBLOCK (nonblock, no data), EINTR (signal),
//                       ECONNRESET (peer RST), EPIPE (send to closed -- + SIGPIPE)
```
Flags: `MSG_DONTWAIT` (this call non-blocking), `MSG_NOSIGNAL` (no `SIGPIPE` on
send), `MSG_WAITALL` (block until full — dangerous, can hang), `MSG_PEEK` (don't
consume), `MSG_ERRQUEUE` (read the error queue — TX timestamps, `14`).

**Short read/write applies** (`29/05`): `send`/`recv` may transfer less than
asked. Always loop for a known-length payload.

### `close(fd)` / `shutdown(fd, how)`
`close` drops this process's ref; last ref → TCP FIN (or RST with `SO_LINGER
{1,0}`). `shutdown(fd, SHUT_WR)` = "I'm done sending" (send FIN) but keep
receiving — half-close, useful for request/response protocols. `SHUT_RD`,
`SHUT_RDWR`.

---

## Address structures

```cpp
sockaddr_in  a4{};  a4.sin_family = AF_INET;  a4.sin_port = htons(port);
                    inet_pton(AF_INET, "10.0.0.5", &a4.sin_addr);
sockaddr_in6 a6{};  a6.sin6_family = AF_INET6; a6.sin6_port = htons(port);
sockaddr_storage ss;                 // big enough for any family -- use for accept/recvfrom
```

**`getaddrinfo`** — the portable way (name/service → list of `sockaddr`, v4+v6):
```cpp
addrinfo hints{}; hints.ai_socktype = SOCK_STREAM; hints.ai_family = AF_UNSPEC;
addrinfo* res;
getaddrinfo("gateway.exch.com", "9001", &hints, &res);
for (auto* ai = res; ai; ai = ai->ai_next) { /* try socket()+connect() */ }
freeaddrinfo(res);
```
HFT: resolve once at startup, cache the `sockaddr`; never DNS on a hot path.
`inet_ntop`/`inet_pton` for string ↔ binary (never the old `inet_addr`/
`inet_ntoa` — no error signalling / not thread-safe).

---

## Framing — TCP is a byte stream, not messages

`send("HELLO"); send("WORLD")` → the peer may `recv` `"HELLOWORLD"`, or `"HEL"`
then `"LOWORLD"`, or... You **must** delimit messages:

| Scheme | How |
|---|---|
| **Length prefix** | `[u32 len][payload]` — read 4 bytes, then `len` bytes. Simple, robust. Most binary exchange protocols. |
| **Fixed-size records** | every message is N bytes. Simplest; only if the protocol allows. |
| **Delimiter** | `\n` / `\r\n` (text protocols, FIX uses SOH `0x01`). Must handle a delimiter split across reads, and escaping. |
| **Self-describing header** | header has a `msg_type` + `body_len`. Common in ITCH/OUCH. |

Receiver pattern (length-prefixed):
```cpp
// accumulate into a ring/buffer; parse as many complete messages as are present
buf.append(recv_bytes);
while (buf.size() >= 4) {
    uint32_t len = ntohl(read_u32(buf.data()));
    if (buf.size() < 4 + len) break;         // wait for more
    dispatch(buf.data() + 4, len);
    buf.consume(4 + len);
}
```

---

## ⚠️ Traps / Common mistakes

### Trap 1 — treating `recv` return 0 as an error
`0` = peer closed cleanly (FIN). It's the normal end-of-stream. `< 0` is the
error case.

### Trap 2 — no framing
Assuming one `send` = one `recv`. Works on loopback with tiny messages, breaks
under load / on real links / with Nagle. Length-prefix or fixed records.

### Trap 3 — ignoring short `send`
`send(fd, buf, 500)` returns 300 → you must send the remaining 200. Loop.
Especially on non-blocking sockets and slow peers.

### Trap 4 — `SIGPIPE` kills the process
`send` to a peer that closed → `SIGPIPE` (default: terminate). `MSG_NOSIGNAL` on
every `send`, or `signal(SIGPIPE, SIG_IGN)` at startup, then handle `EPIPE`.

### Trap 5 — blocking `connect` with no timeout
Default `connect` can hang ~2 min on `ETIMEDOUT`. Non-blocking connect +
`epoll`/`poll` with your own timeout, then check `SO_ERROR`.

### Trap 6 — DNS on the hot path
`getaddrinfo` can block for **seconds** (network DNS). Resolve at startup, cache
the `sockaddr`.

### Trap 7 — small `listen` backlog under connection storms
`listen(fd, 5)` + a burst of clients → drops/retries. Use `SOMAXCONN` (or a big
number) and `accept` promptly (drain the queue in a loop, `10`).

### Trap 8 — `MSG_WAITALL` deadlock
"Block until `len` bytes" — if the peer sends fewer and stalls (or the message
is shorter than you think), you hang forever. Loop `recv` with your own
completion logic + a timeout.

---

## > **HFT relevance**

> - **Resolve + build all `sockaddr`s at startup**, pre-open every socket, set
>   every option (`11`) before "go live". No `socket`/`connect`/`getaddrinfo`
>   on a hot path.
> - **`accept4(SOCK_CLOEXEC | SOCK_NONBLOCK)`**, `SO_REUSEADDR`/`SO_REUSEPORT`,
>   `TCP_NODELAY` immediately on every accepted fd.
> - **Length-prefixed binary framing**; parse straight out of the receive
>   buffer, zero-copy into your message structs where alignment allows.
> - **`writev({header, body})`** — one syscall, one segment (`04`).
> - **`MSG_NOSIGNAL` on every send**; treat `recv` 0 and `ECONNRESET` as
>   "reconnect" events handled off the hot path.
> - **Non-blocking everything** + `epoll` (`08`, `10`), or busy-poll
>   `recv(MSG_DONTWAIT)` on the isolated core.

---

## Hands-on

```bash
# Linux pe -- examples 01 (server) + 02 (client, measures RTT)
g++ -std=c++20 -O2 30-NETWORKING/examples/01_tcp_echo_server.linux.cpp -o /tmp/es
g++ -std=c++20 -O2 30-NETWORKING/examples/02_tcp_client.linux.cpp      -o /tmp/ec
/tmp/es 9099 & sleep 0.3 ; /tmp/ec 127.0.0.1 9099 20000

# see the framing problem: send two messages, watch them merge
printf 'AAAA' | nc -q1 127.0.0.1 9099 ; printf 'BBBB' | nc -q1 127.0.0.1 9099

ss -tanp | grep 9099           # LISTEN + ESTABLISHED states
cat /proc/sys/net/core/somaxconn
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`recv` 0 = error" | orderly peer close (FIN); `< 0` is error |
| "ek `send` = ek `recv`" | TCP byte stream; frame it (length prefix) |
| "`send` sab bhej dega" | short write; loop |
| "`connect` jaldi fail hoga" | can hang ~2 min; non-blocking + own timeout |
| "`getaddrinfo` sasta hai" | can block seconds (DNS); resolve at startup |
| "backlog choti chalegi" | connection storm → drops; `SOMAXCONN` + prompt accept |

---

## Exercises

1. A client does `send(fd, msg1, 8); send(fd, msg2, 8);`. The server does one
   `recv(fd, buf, 64)`. What can `buf` contain?

   <details><summary>Answer</summary>

   Any of: 16 bytes (`msg1`+`msg2` merged), 8 bytes (`msg1` only, `msg2` in the
   next `recv`), or even fewer (e.g. 5 bytes, rest next time) — TCP is a byte
   stream with no message boundaries. The server must frame: fixed 8-byte
   records here, or a length prefix, and loop parsing complete messages out of
   an accumulation buffer.
   </details>

2. Your server process dies with no error message when a client disconnects
   mid-transfer. Cause and fix.

   <details><summary>Answer</summary>

   `SIGPIPE`: the server called `send()` on a socket the client had closed
   (RST/FIN), and the default `SIGPIPE` disposition is to terminate the process
   silently. Fix: pass `MSG_NOSIGNAL` to every `send`, or `signal(SIGPIPE,
   SIG_IGN)` at startup (or `setsockopt(SO_NOSIGPIPE)` on BSD). Then `send`
   returns `-1`/`EPIPE` and you handle it as "connection gone".
   </details>

3. Non-blocking `connect()` returns `-1`/`EINPROGRESS`. How do you find out when
   (and whether) it succeeded?

   <details><summary>Answer</summary>

   Register the fd for **writable** in `epoll`/`poll` (with your own timeout).
   When it signals writable, call `getsockopt(fd, SOL_SOCKET, SO_ERROR, ...)`:
   `0` = connected; non-zero (e.g. `ECONNREFUSED`, `ETIMEDOUT`) = failed with
   that errno. Don't assume "writable = connected" without checking `SO_ERROR`.
   </details>

4. Design length-prefixed framing for a receiver that may get partial messages
   and multiple messages per `recv`.

   <details><summary>Answer</summary>

   Keep a per-connection accumulation buffer. On each `recv`, append the bytes.
   Then loop: if buffered ≥ 4, read the `u32` length (network order → `ntohl`);
   if buffered ≥ `4 + len`, dispatch that message and consume `4 + len` bytes;
   else break and wait for more data. Guard against absurd `len` (cap it, or
   you'll allocate/DoS). Use a ring buffer or a buffer with a moving read
   offset + periodic compaction to avoid `memmove` per message.
   </details>

5. Why resolve hostnames at startup, not lazily on first use, in an HFT process?

   <details><summary>Answer</summary>

   `getaddrinfo` can block for **seconds** — it may do a network DNS query,
   retry, hit `/etc/nsswitch.conf` modules (LDAP, mDNS), etc. Doing that lazily
   means the first order (or the first reconnect) stalls unpredictably.
   Resolve every endpoint at startup, cache the `sockaddr` structs, and if you
   need refresh-on-failure, do it on a background thread, never inline on the
   trading path. Better still: use IP addresses in config for the hot paths.
   </details>

---

## Interview questions

1. The TCP server call sequence; what `accept` returns.
2. `recv` return values — 0 vs <0, and the key errnos.
3. TCP framing — why needed, three schemes.
4. Short read/write — where it bites, how to handle.
5. Non-blocking `connect` — the `EINPROGRESS` → `SO_ERROR` dance.
6. `SIGPIPE` — when it fires, how to prevent.
7. `shutdown(SHUT_WR)` vs `close` — the half-close use case.

---

## Next
→ [`08-blocking-vs-nonblocking.md`](08-blocking-vs-nonblocking.md)

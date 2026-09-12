# 11 — TCP tuning, TCP_NODELAY, connection warm-up, keepalives

## Prerequisites
- `10-irq-affinity-tuning.md`
- `30-NETWORKING/03-nagle-and-delayed-ack.md` (Nagle's ~40ms interaction,
  already measured — is lesson isi ke upar build karta)

## Yeh topic abhi kyun

Market data multicast/UDP hoti (01-08 ka poora focus), PAR **order
SUBMIT karna** almost hamesha **TCP** hota (exchanges reliability chahte
— ek order LOST nahi hona chahiye, jaisa UDP allow karta). `05_order_
gateway.linux.cpp` TCP-specific tuning ka poora set demonstrate karta.

---

## `TCP_NODELAY` — dono taraf

```cpp
::setsockopt(cfd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
```

30/03 ne measure kiya tha: Nagle ON hone se **~40ms p99 spike** aa
sakta ek "write small, wait for reply" pattern mein — order gateways
EXACTLY yeh pattern hain (chhota order bhejo, chhota ack wapas aata).
`TCP_NODELAY` **SENDER-side** Nagle disable karta — sender ab apne
writes ko coalesce karne ke liye WAIT nahi karta.

**Important**: dono taraf (client AUR server-accepted socket) set karna
chahiye — agar sirf ek taraf set ho, us DIRECTION mein Nagle-delay nahi
hoga, PAR DOOSRI direction (jaisa server ka ack wapas bhejna) mein ho
sakta.

---

## `writev()` — ek write, ek packet

```cpp
iovec iov[2] = {{&header, sizeof header}, {body, sizeof body}};
::writev(fd, iov, 2);
```

Do ALAG `send()` calls (header phir body) **DO SYSCALLS + potentially
DO PACKETS** ban sakte, chahe `TCP_NODELAY` ho — `writev()` (scatter-
gather write) unhe **EK SYSCALL** mein combine karta, jo kernel ko EK
hi TCP segment banane ka BEHTAR mauka deta.

`05`'s measured comparison (A: writev vs B: 2x send) is machine pe
**writev consistently thoda BEHTAR** dikhaata (extra-syscall + potential
extra-packet cost) — **DRAMATIC nahi** (30/03 ka 40ms wala effect NAHI,
kyunki `TCP_NODELAY` wahi problem already solve kar chuka hai). writev
ka fayda "crash-save" nahi hai, ek **consistent, cheap discipline** hai
— khaaskar REAL (non-loopback) networks pe jahan extra packet ka
bandwidth+ACK-round-trip cost zyaada matter karta.

---

## Keepalive tuning

```cpp
int idle = 10, intvl = 3, cnt = 3;
::setsockopt(cfd, IPPROTO_TCP, TCP_KEEPIDLE,  &idle,  sizeof idle);
::setsockopt(cfd, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof intvl);
::setsockopt(cfd, IPPROTO_TCP, TCP_KEEPCNT,   &cnt,   sizeof cnt);
```

Default TCP keepalive (2 GHAṆṬE idle timeout!) HFT ke liye bahut SUST
hai — ek dead/half-open connection **GHANTON** tak "alive" dikhega.
Yeh tuning: 10 second idle → phir har 3 second ek probe → 3 failed
probes → connection dead maano (~19 seconds total tak fail-detect).

## `TCP_USER_TIMEOUT` — aur bhi tez

```cpp
unsigned timeout_ms = 5000;
::setsockopt(cfd, IPPROTO_TCP, TCP_USER_TIMEOUT, &timeout_ms, sizeof timeout_ms);
```

Yeh **unacknowledged data** ke liye ek DIFFERENT timeout hai — agar
tumne kuch bheja aur X milliseconds tak ACK NAHI mila, connection FAIL
maana jaata (keepalive se independent, jo sirf IDLE connections ke
liye hai). Order-gateway jaisi latency-critical connection ke liye,
"fail fast" (seconds mein, minutes nahi) **37/13's fail-closed risk
principle** ka DIRECT application — ek order jo silently kabhi jaayega
hi nahi (dead connection) usse SECONDS mein pata chalna chahiye, GHANTON
mein nahi.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — sirf EK taraf `TCP_NODELAY` set karna
Client set karta, server BHOOL jaata (ya vice versa) — us DIRECTION mein
Nagle-delay abhi bhi possible hai.

### Trap 2 — writev ko "Nagle ka replacement" samajhna
writev syscall-count aur packet-count optimize karta — Nagle KHUD ek
ALAG mechanism hai jo `TCP_NODELAY` se disable hota. Dono independent
fixes hain, dono zaroori.

### Trap 3 — default keepalive (2 ghante) pe rely karna
Production order gateway ek dead connection pe orders bhejta reh sakta
GHANTON tak bina explicit tuning ke — silent, expensive failure mode.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| `TCP_NODELAY` ek taraf set karna kaafi hai | Dono taraf zaroori |
| writev Nagle jaisa DRAMATIC fayda deta | Chhota, consistent fayda — Nagle ka fix ALAG hai (`TCP_NODELAY`) |
| Default TCP keepalive HFT ke liye theek hai | Bahut sust (2 ghante) — explicit tuning zaroori |

---

## Hands-on

```bash
g++ -std=c++20 -O2 -Wall -Wextra -pthread 05_order_gateway.linux.cpp -o gateway
```

---

## Exercises

1. Ek gateway connection "hung" ho jaata (network partition), order
   bheja gaya par kabhi ACK nahi mila. `TCP_USER_TIMEOUT=5000` set hai.
   Kitni der mein failure detect hoga?
   <details><summary>Answer</summary>
   ~5 seconds mein -- `TCP_USER_TIMEOUT` unacknowledged data pe track
   karta, keepalive (jo IDLE connections ke liye hai) se independent.
   Bina isske, default TCP retransmission timeout dhire-dhire badhta
   (exponential backoff) aur MINUTES lag sakte fail-detect hone mein.
   </details>

---

## Interview questions

1. `TCP_NODELAY` dono taraf kyun set karna chahiye?
2. `writev()` ka fayda kya hai, aur yeh Nagle-fix se kaise ALAG hai?
3. Keepalive aur `TCP_USER_TIMEOUT` mein fark, aur dono kyun chahiye.

---

## Next
→ [`12-switch-and-network-latency.md`](12-switch-and-network-latency.md)

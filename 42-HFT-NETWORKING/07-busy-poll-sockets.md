# 07 — SO_BUSY_POLL, kernel busy polling, when it helps

## Prerequisites
- `06-dpdk-intro.md`
- `41-HFT-CONCURRENCY/07-busy-spin-vs-blocking.md` (thread-level busy-
  spin trade-off — yeh lesson USSE socket-level pe apply karta)

## Yeh topic abhi kyun

03's ladder mein `SO_BUSY_POLL` sabse "SASTA" bypass-ADJACENT option
tha (koi hardware/vendor-lock nahi, ek sockopt call). Yeh lesson isse
deep-dive karta — `02_busy_poll_receiver.linux.cpp` aur `03_receiver_
benchmark.linux.cpp` ke through.

---

## Do ALAG "busy-poll" — confuse mat karo

| | App-level busy-poll (30/10, 41/07) | `SO_BUSY_POLL` (yeh lesson) |
|---|---|---|
| Kaun spin karta | APPLICATION thread (`MSG_DONTWAIT` + retry loop) | KERNEL (recv()/epoll_wait() ke ANDAR) |
| Kaam karta kis interface pe | KISI BHI (loopback samet) | Sirf NIC driver `ndo_busy_poll` support kare to |
| App code | Ek explicit spin-loop likhna padta | Normal blocking `recv()`/`epoll_wait()`, KOI app-loop change nahi |
| CPU cost | 100% CONTINUOUSLY (jab tak data na aaye) | BOUNDED budget (microseconds), phir normal wait |

**`02_busy_poll_receiver.linux.cpp`** sirf sockopt set/get karta hai
(no receive loop) — dikhata hai `SO_BUSY_POLL` ek **per-socket hint**
hai, "is socket pe wait karte waqt, X microseconds kernel apna NAPI poll
khud chala le, interrupt ka wait mat kar."

---

## Driver-dependency — is course ka honest caveat

```cpp
int budget_us = 50;
::setsockopt(fd, SOL_SOCKET, SO_BUSY_POLL, &budget_us, sizeof budget_us);
```

Yeh setsockopt call **HAMESHA SUCCEED** karta (modern kernel pe) — PAR
uska **effect** NIC driver pe depend karta. `ethtool -k eth0 | grep
busy-poll` se check hota driver support karta ya nahi. Zyaadatar modern
DATACENTER NICs (Intel ixgbe/i40e, Mellanox mlx5) support karte —
**loopback (`lo`) jaisi VIRTUAL interfaces NAHI karte** (koi real NAPI
poll-routine hai hi nahi wahan).

**Yeh EXACT reason hai** ki `03_receiver_benchmark.linux.cpp`'s
strategy C (`blocking + SO_BUSY_POLL`) is repo ke test environment
(loopback) pe strategy A (plain blocking) ke KAAFI KAREEB expected hai
— NAHI kyunki `SO_BUSY_POLL` "kaam nahi karta," balki kyunki iska
DEPENDENCY (real NIC driver) yahan present nahi hai. **Rule 4**: is
limitation ko explicitly document kiya gaya hai, chhupaya nahi.

---

## `03`'s teen-way comparison

```
A) blocking recv()                    -- baseline
B) app busy-poll (MSG_DONTWAIT spin)  -- 30/10's mechanism, ALWAYS works
C) blocking + SO_BUSY_POLL            -- kernel-assisted, driver-dependent
```

**B jeetega loopback pe bhi** (app-level spin hardware-independent hai).
**C real NIC hardware pe B ke KAREEB aata**, PAR app apna CPU sirf
BOUNDED budget ke liye kharch karta (recv() call ke andar), na ki
HAMESHA 100% (B ka trade-off).

---

## Kab C, B se BEHTAR choice hai

- Tumhare paas **spare cores nahi** hain (B ek POORA core permanently
  spin karta; C sirf recv() call ke andar thodi der).
- Tum `epoll`-based ek event loop chala rahe ho jo MULTIPLE sockets
  handle karta — `SO_BUSY_POLL` `epoll_wait()` ke saath bhi kaam karta,
  poore set ke liye ek hi budget.
- Real NIC hai (loopback/virtual NAHI) jo `ndo_busy_poll` support kare.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `SO_BUSY_POLL` fail hone ki umeed karna jab driver support na ho
`setsockopt` FAIL NAHI hota driver-unsupported case mein — bas silently
NO-OP hota. `getsockopt` se value READ BACK karna (jaisa `02` karta)
sirf yeh confirm karta "kernel ne value STORE ki," yeh CONFIRM NAHI
karta "yeh actually kaam kar rahi hai."

### Trap 2 — loopback pe test karke "SO_BUSY_POLL kaam nahi karta"
conclude karna
Yeh loopback ka LIMITATION hai, `SO_BUSY_POLL` ka nahi. Real NIC pe
alag result milega (03's EXPECTED comment isi ko explicit karta).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| `SO_BUSY_POLL` app-level spin ka replacement hai | Alag mechanism (kernel-side, driver-dependent, bounded) |
| Sockopt call SUCCEED hone ka matlab yeh KAAM kar rahi | Sirf value ACCEPTED hui, driver support alag baat hai |
| Loopback pe test kaafi hai iski effectiveness janne ke liye | Real NIC (driver support ke saath) chahiye |

---

## Hands-on

```bash
g++ -std=c++20 -O2 -Wall -Wextra -pthread 02_busy_poll_receiver.linux.cpp -o busypoll
g++ -std=c++20 -O2 -Wall -Wextra -pthread 03_receiver_benchmark.linux.cpp -o rxbench
```

---

## Exercises

1. Kyun `03`'s strategy C loopback pe strategy A ke kareeb rehne ki
   expectation hai, B ke nahi?
   <details><summary>Answer</summary>
   B (app-level spin) hardware-independent hai -- KISI BHI interface pe
   kaam karta, loopback samet. C (`SO_BUSY_POLL`) driver-dependent hai
   -- loopback ka koi real NAPI poll-routine hai hi nahi, isliye kernel
   ke paas "busy-poll karne" ke liye kuch hai hi nahi, effectively A
   jaisa hi behave karta.
   </details>

---

## Interview questions

1. App-level busy-poll aur `SO_BUSY_POLL` ka fundamental difference.
2. `SO_BUSY_POLL` kyun loopback pe measurable fayda NAHI dega?
3. `SO_BUSY_POLL` kab `epoll`-based systems mein specifically useful hai?

---

## Next
→ [`08-hardware-timestamping.md`](08-hardware-timestamping.md)

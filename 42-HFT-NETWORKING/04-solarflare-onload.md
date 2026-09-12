# 04 — Onload: transparent bypass, LD_PRELOAD model

## Prerequisites
- `03-kernel-bypass-overview.md`

## Yeh topic abhi kyun

03's ladder mein Onload sabse "APPLICATION-FRIENDLY" bypass option hai —
kyun, aur kaise, yeh lesson explain karta.

---

## Mechanism — `LD_PRELOAD`

```bash
LD_PRELOAD=libonload.so ./my_trading_app
```

`LD_PRELOAD` ek Linux dynamic-linker feature hai — specified shared
library ko application ke apni libraries se **PEHLE** load karta, aur
uske symbols APPLICATION ke calls ko intercept kar lete (jaisa
`socket()`, `bind()`, `send()`, `recv()` — poori BSD-socket family).

```
Normal:   app calls socket() -> glibc's socket() -> kernel syscall
Onload:   app calls socket() -> Onload's socket() -> USER-SPACE stack
                                 (koi kernel syscall NAHI, jab tak
                                  Onload-managed NIC involved ho)
```

**Application source code EK LINE bhi NAHI badalta.** `08_bypass_
abstraction.hpp`'s comment isi ko highlight karta: "OnloadReceiver"
class ki zaroorat hi nahi — `KernelSocketReceiver` ka WOHI code, Onload
ke saath run karne pe, transparently fast ho jaata.

---

## Onload apna USER-SPACE TCP/UDP stack rakhta

Onload poora TCP/IP stack **user-space mein REPLICATE** karta —
connection state, retransmission, congestion control, sab kuch. Jab
tumhara Solarflare NIC ek packet DMA karta, woh SEEDHA Onload ke
user-space buffers mein jaata, kernel ko chhue bina — Onload khud us
packet ko parse/route karta (jaise kernel karta, par user-space mein,
zero context-switch ke saath).

---

## Trade-offs

| Fayda | Cost |
|---|---|
| Application code UNCHANGED | Solarflare NIC + license chahiye (proprietary) |
| TCP/UDP dono support (poora socket API) | User-space TCP stack ka apna maintenance-burden (Solarflare ka, tumhara nahi, par dependency hai) |
| Fallback to kernel agar non-Onload interface ho | Debugging tools (tcpdump) directly kaam NAHI karte Onload-accelerated traffic pe (special Onload-aware tools chahiye) |
| Kernel-bypass ka fayda BINA application-rewrite ke | CPU spin (Onload OFTEN busy-polls internally bhi) |

---

## Kab Onload sahi choice hai

- Tumhari existing application **already socket API** use karti (naya
  banane ki zaroorat nahi).
- Tumhe **TCP bhi chahiye** (order gateways — 11), na sirf UDP multicast.
- Solarflare NIC budget mein hai, aur vendor-lock acceptable hai.
- Team ke paas **naya networking API seekhne ka bandwidth NAHI** hai
  (jaisa ef_vi/DPDK maangte, 05/06).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — Onload ko "free speed" samajhna
Onload apna CPU cost rakhta (internal busy-polling) — 41/07's spin-vs-
block trade-off yahan bhi laagu hota, bas Onload ke andar chhupa hota.

### Trap 2 — debugging Onload-accelerated traffic ko normal tcpdump se
`tcpdump` kernel ke through guzarte packets dekhta — Onload-accelerated
traffic KERNEL ko touch hi nahi karta, isliye normal tcpdump usse MISS
kar sakta (Onload apne khud ke diagnostic tools deta, 15 se connect).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Onload sirf UDP ke liye hai | TCP bhi poori tarah support karta |
| Onload = zero cost | Apna CPU/busy-poll cost rakhta, NIC/license cost bhi |
| Application code kuch badalna padta | `LD_PRELOAD` se transparent, source unchanged |

---

## Exercises

1. Ek team `tcpdump` se production traffic debug karne ki koshish karti
   par Onload-accelerated connections dikhti hi nahi. Kyun?
   <details><summary>Answer</summary>
   Onload traffic ko kernel se BYPASS karta -- `tcpdump` kernel-level
   packet capture hai, isliye Onload-managed sockets ka traffic usse
   invisible hai. Onload apne khud ke tools (`onload_stackdump`, etc.)
   dete is gap ko fill karne ke liye.
   </details>

---

## Interview questions

1. `LD_PRELOAD` mechanism explain karo.
2. Onload application code kyun change nahi karta (jabki ef_vi/DPDK karte)?
3. Onload ka hidden CPU cost kya hai?

---

## Next
→ [`05-ef-vi.md`](05-ef-vi.md)

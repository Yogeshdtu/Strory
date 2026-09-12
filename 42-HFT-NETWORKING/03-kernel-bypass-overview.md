# 03 — Kernel bypass: kyun, kaunse options, trade-offs

## Prerequisites
- `02-multicast-receive-path.md`

## Yeh topic abhi kyun

01 ne dikhaya kernel network stack sabse BADA single latency-contributor
hai (~1-5µs). Yeh lesson us stage ko **poori tarah HATANE** ke options
cover karta — "kernel bypass" ka poora landscape, 08's abstraction
header ke through.

---

## Kyun bypass karna padta

Kernel socket API (`socket()`, `recv()`, `send()`) **generic** hai —
routing tables consult karta, firewall rules check karta, multiple
protocols support karta, **interrupt-driven** (NIC → CPU interrupt →
context switch → driver → kernel stack → socket buffer → app) hai. HFT
ko in mein se ZYAADATAR nahi chahiye — ek FIXED destination, ek FIXED
protocol, koi routing decision. Bypass libraries yeh saara overhead
skip karti, packets seedha **user-space memory mein DMA** karti hain.

---

## Options — ek ladder

| Option | Kaisa | Latency (typical) | Trade-off |
|---|---|---|---|
| **Kernel sockets** (abhi tak yeh course) | Standard, portable | ~1-5 µs | Simple, portable, DEBUGGABLE (tcpdump kaam karta) |
| **`SO_BUSY_POLL`** (07) | Kernel khud NIC poll karta, socket API unchanged | Thoda behtar (driver support pe depend) | Zero app-code change, driver-dependent |
| **Onload** (04) | `LD_PRELOAD` se transparent intercept, USER-SPACE TCP/UDP stack | ~1 µs | Application code **UNCHANGED**, Solarflare NIC chahiye |
| **ef_vi** (05) | RAW NIC queue access, koi socket() hi nahi | ~100-300 ns | Application PURA REWRITE (naya API), Solarflare/Xilinx NIC |
| **DPDK** (06) | Poll-mode driver, NIC OS se POORI tarah hataya | ~100-300 ns | Poora networking stack khud likhna padta (TCP/IP bhi agar chahiye), hugepages + dedicated cores |
| **AF_XDP** | eBPF-based, kernel ke ANDAR hi fast-path | ~500ns-1µs | Kernel bypass ka "lightweight" version, kam vendor-lock |

**Neeche jaate jaate: latency kam hoti, PAR application complexity aur
hardware-lock badhta.**

---

## `INetworkReceiver` abstraction — kyun zaroori

```cpp
class INetworkReceiver {
    virtual bool try_receive(RxPacket&) = 0;
    virtual void warmup() = 0;
    virtual const char* backend_name() const = 0;
};
```

Agar application code **directly** `ef_vi_receive_get()` ya
`rte_eth_rx_burst()` calls kare, backend badalna (kernel-socket se
Onload, ya Onload se ef_vi) MEANS rewriting the application. Ek
abstraction interface ke peeche in differences ko chhupa ke, **backend
badalna ek CONFIGURATION decision ban jaata, CODE-REWRITE nahi.**

`08_bypass_abstraction.hpp`'s `KernelSocketReceiver` (REAL, working)
aur stub comments (Onload/ef_vi/DPDK, 04-06 mein detail) isi pattern ko
dikhate.

> **HFT relevance:** production systems OFTEN kernel-sockets se test/dev
> karte (`tcpdump` kaam karta, debugging easy), aur PRODUCTION mein
> Onload/ef_vi/DPDK switch karte — abstraction ke bina, do alag
> CODEBASES maintain karni padti. Ek abstraction se ek hi codebase,
> config-time backend choice.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — bypass ko "hamesha behtar" samajh lena
Bypass debugging KAAFI mushkil banata (`tcpdump` kaam nahi karta jab
packets kernel ko touch hi nahi karte — 15's topic), aur hardware-lock
create karta (Solarflare-specific ya DPDK-specific NIC chahiye). Agar
latency requirement microseconds mein hai (nanoseconds nahi), kernel
sockets + tuning (09-11) SHAYAD kaafi ho.

### Trap 2 — DPDK ko "sirf ek library" samajhna
DPDK NIC ko OS se POORI tarah HATA deta (`vfio-pci` bind) — us NIC pe
ab koi AUR process (kernel ke through) kuch nahi kar sakta. Yeh ek
POORA "commitment," na ki ek optional library import.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Bypass ek chhota tweak hai | Application architecture-level decision, complexity aur hardware-lock ke saath |
| Sab bypass options SAME hain | Onload transparent hai, ef_vi/DPDK application rewrite maangte |
| Bypass ke bina HFT nahi ho sakta | Kernel-socket + tuning (09-11) bhi microsecond-range HFT ke liye kaafi ho sakta |

---

## Exercises

1. Ek team latency-sensitive HFT system banati hai, PAR unke paas team
   ka bandwidth limited hai aur woh Solarflare NIC nahi kharid sakte.
   Kaunsa option (upar ki table se) unke liye realistic hai?
   <details><summary>Answer</summary>
   `SO_BUSY_POLL` (koi hardware/vendor-lock nahi, application code
   change bhi nahi) ya AF_XDP (kernel-level, generic NIC pe kaam karta,
   Onload/ef_vi/DPDK jaisa vendor-specific nahi). Onload/ef_vi Solarflare-
   specific hardware maangte, DPDK poora networking stack khud likhne ki
   commitment maangta.
   </details>

---

## Interview questions

1. Kernel-socket se DPDK tak ka poora ladder, trade-offs ke saath.
2. `INetworkReceiver` abstraction ka business-value kya hai?
3. Bypass ka SABSE BADA hidden cost (debugging ke context mein) kya hai?

---

## Next
→ [`04-solarflare-onload.md`](04-solarflare-onload.md)

# 05 — ef_vi: raw API, lowest latency

## Prerequisites
- `04-solarflare-onload.md`

## Yeh topic abhi kyun

Onload (04) application code UNCHANGED rakhta, POORA socket-API-shape
retain karke. `ef_vi` uske ULTA extreme hai — koi socket API hi nahi,
seedha NIC hardware queues expose karta. Latency ke liye SABSE fast
(~100-300 ns range), par application ko POORA REWRITE maangta.

---

## Kya alag hai

```
Onload:  app calls socket()/recv() -- Onload woh calls INTERCEPT karta,
         apna fast user-space stack use karta, PAR APPLICATION KO
         "socket API" HI dikhta.

ef_vi:   app SEEDHA NIC ke TX/RX queues (virtual interfaces, "VIs") ke
         saath kaam karta -- koi socket() hi NAHI hota, koi IP/TCP/UDP
         parsing "automatically" nahi hoti (agar chahiye, app KHUD
         karta -- raw Ethernet frames ka access milta).
```

`ef_vi` ka NAAM khud isi se aata: **"Virtual Interface"** — NIC hardware
ka ek DIRECT, user-space-mapped queue-pair (RX ring + TX ring), jise
application directly poll karta.

---

## Conceptual shape (08's header mein comment se)

```cpp
ef_vi vi;
ef_vi_receive_post(&vi, buf, id);   // ek buffer PRE-POST karo (NIC ise fill karega)
// ... poll loop ...
ef_vi_receive_get(&vi, &pkt);        // check karo kya NIC ne fill kiya
```

**"Pre-post"** ek important pattern hai: application PEHLE SE HI buffers
NIC ko de deta ("yahan likh dena jab data aaye"), NIC unhe DIRECTLY fill
karta (DMA), application sirf POLL karta "kya bhar gaya." Zero copy, zero
allocation ON THE HOT PATH (36's principle, hardware level pe applied).

---

## Trade-offs

| Fayda | Cost |
|---|---|
| Lowest latency achievable (~100-300 ns) | Application POORA rewrite -- koi standard socket API nahi |
| Zero-copy DMA seedha app buffers mein | IP/TCP/UDP parsing agar chahiye, KHUD likhna (ya minimal libraries use karna) |
| No kernel involvement at all (jab tak chahiye na ho) | Debugging (tcpdump) COMPLETELY invisible -- raw hardware access |
| Direct hardware control (batching, polling policy) | Solarflare/Xilinx NIC + `ef_vi` SDK chahiye |

---

## Kab ef_vi sahi choice hai

- Latency itni critical hai ki HAR nanosecond count karta (market-making,
  arbitrage ka fastest tier).
- Team ke paas **networking internals expertise** hai (Ethernet framing,
  checksums, agar zaroorat ho khud handle karne ke liye).
- Protocol simple/fixed hai (jaisa ek FIXED multicast market-data feed —
  har baar SAME structure, koi dynamic routing/connection-negotiation
  nahi chahiye).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — ef_vi ko "bas ek fast socket()" samajhna
Bilkul nahi — ef_vi mein IP/UDP HEADERS bhi khud parse karne padte agar
chahiye (raw Ethernet frame milta). Yeh EK layer NEECHE hai socket API
se, na ki ek FASTER socket API.

### Trap 2 — poori application ko ef_vi pe migrate karna, jab sirf ek
HOT PATH ko chahiye
Real systems typically SIRF market-data-receive path (sabse latency-
critical) ef_vi pe likhte, baaki (config, logging, monitoring) normal
socket API pe hi rehta — poori application ko rewrite karna zyaadatar
zaroorat se zyaada complexity hai.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| ef_vi "socket() ka fast version" hai | Poora ALAG, lower-level API -- koi TCP/UDP abstraction nahi |
| ef_vi hamesha DPDK se behtar hai | Alag trade-off (NIC-specific vs generic-hardware-poll-mode) -- 06 se compare |
| Poori app ef_vi pe honi chahiye | Typically sirf HOTTEST path (market-data receive) |

---

## Exercises

1. Ek engineer ef_vi use karke ek market-data receiver banata hai. Unhe
   packet ka "price" field chahiye. Yeh kaise milega?
   <details><summary>Answer</summary>
   Raw Ethernet frame se KHUD parse karna padega -- Ethernet header
   skip karo, IP header skip karo (agar UDP/IP encapsulated hai), UDP
   header skip karo, phir application payload (jahan 'price' field hai,
   38-MARKET-DATA jaisa protocol) parse karo. Koi socket-layer isse
   "automatically" nahi karta ef_vi mein.
   </details>

---

## Interview questions

1. ef_vi aur Onload ka fundamental architectural difference.
2. "Pre-post" buffer pattern explain karo.
3. Ef_vi kis specific use-case ke liye sabse appropriate hai?

---

## Next
→ [`06-dpdk-intro.md`](06-dpdk-intro.md)

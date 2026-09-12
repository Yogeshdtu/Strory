# 11 — Co-location, proximity hosting, cross-connects, fair access

## Prerequisites
- [`10-hft-strategies-overview.md`](10-hft-strategies-overview.md)
- `31-CPU-ARCHITECTURE`, `30-NETWORKING` (light bhi chalega — bas
  "network hop = latency" ka concept)

## Yeh topic abhi kyun
07 mein dekha FIFO markets mein "pehle order bhejna" hi jeet hai. Colocation
wahi race jeetna ka **physical infrastructure layer** hai — sirf tumhara
code fast hona kaafi nahi, tumhara server exchange se **physically kitna
door** hai, yeh utna hi bada factor hai.

---

## Colocation kya hai

> **Colocation = tumhara trading server exchange ke apne datacenter mein,
> literally exchange ke matching-engine servers ke **paas** rakhna.**

```
WITHOUT colocation:
  tumhara server (kahin bhi) --[internet/leased line, sainte km]--> exchange
      network latency: milliseconds range

WITH colocation:
  tumhara server (exchange ke DC mein) --[few meters ka fiber]--> exchange
      network latency: microseconds ya kam range
```

Speed of light hi hard limit hai — signal fiber mein ~200,000 km/s (light
in vacuum se dheema, fiber ke refractive index ki wajah se) travel karta.
100 km door hone ka matlab kam se kam ~0.5 ms round-trip **sirf propagation
delay** — chahe tumhara code perfect ho, physical distance khud latency hai
jo koi optimization hata nahi sakti. Isiliye colocation exchange ke jitna
**close** ho sake utna zaroori hota HFT ke liye.

---

## Cross-connects

Colocation datacenter ke andar, exchange tumhe ek **direct cable
connection** (cross-connect) deta apne matching-engine infrastructure tak —
shared network switch ke bajaye, ek dedicated physical link. Yeh:
- Consistent, predictable latency deta (shared infra mein contention se
  jitter aa sakta).
- Har participant ka cross-connect **length bhi matter karta** — isi liye
  kai exchanges cable length ko **equalize** karte (sabko same-length
  cable, chahe rack physically kahin bhi ho) taaki koi participant sirf
  "accidentally close rack mila" ki wajah se advantage na paaye.

---

## Proximity hosting

Agar full colocation available/affordable nahi hai (bahut costly hota,
limited rack space), **proximity hosting** ek alternative hai — exchange
ke datacenter ke **bahar par bahut paas** ek third-party facility, jahan
se exchange tak low-latency connectivity milti (colocation se thoda zyada
latency, par distant remote-connectivity se kaafi behtar).

---

## Fair access — regulatory concern

Colocation bahut expensive hoti (rack space, power, cross-connect fees) —
sirf badi firms afford kar sakti. Regulators aur exchanges is concern ko
address karte:

| Mechanism | Kya karta |
|---|---|
| **Equal cable length** | Sabka physical latency same, chahe rack position kuch bhi ho |
| **Published, non-discriminatory pricing** | Colocation access sab eligible members ko same terms pe available |
| **Speed bumps** (kuch venues) | Jaan-boojh kar ek chhota artificial delay (kuch venues micro/milliseconds range mein) sabke orders pe, taaki raw-speed ka edge kam ho aur signal-quality zyada matter kare — sab venues aisa nahi karte, yeh ek specific design choice hai |

Yeh poori discussion "fair access" ka regulatory/market-structure topic
hai (16-regulatory-basics mein aur context milega) — colocation khud
illegal/unfair nahi hai (publicly available service hai, sab eligible
members access kar sakte), par uski **cost aur complexity** ek natural
barrier hai jo regulators actively monitor karte.

---

## Colocation ke andar bhi optimization

Sirf colocated hona kaafi nahi — us rack ke andar bhi:

| Layer | Optimization |
|---|---|
| NIC | Kernel-bypass NICs (Solarflare/OpenOnload jaisi tech, 42-HFT-NETWORKING) |
| OS | Tuned kernel, `isolcpus`/`nohz_full` (19-cpu-pinning-strategy, folder 36) |
| Code | Poora 36-LOW-LATENCY-CPP jo tumne abhi seekha |
| Even the switch/NIC firmware | Kuch firms FPGA-based NICs/switches use karti extreme cases mein |

**Colocation sirf ek layer hai** — poori chain (physical distance + network
stack + OS + application code) optimize honi chahiye, warna colocation ka
faayda kisi ek slow layer se waste ho jaata.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — sochna colocation "sabse important" ya "akela" solution hai
Colocation ek necessary layer hai kai FIFO-latency-sensitive strategies ke
liye, par tumhara code/OS/network-stack slow ho to colocation ka faayda
kam ho jaata (chain sabse weak link se limited hai).

### Trap 2 — colocation ko "cheating" samajhna
Yeh ek publicly available, priced service hai jo har eligible member
access kar sakta — cost barrier hai, access barrier nahi (assuming
regulated, transparent market).

### Trap 3 — sochna network distance hi poora latency hai
Physical propagation delay ek **floor** hai (isse kam nahi ho sakta), par
tumhara total latency isse zyada hota — NIC, kernel, application processing
sab add hote (14-latency-budget mein poora breakdown).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Colocation = illegal advantage | Publicly priced, non-discriminatory service (regulated markets mein) |
| Sirf colocation kaafi hai fast hone ke liye | Poori chain (code+OS+network) optimize honi chahiye |
| Cable length matter nahi karta agar same building mein ho | Length equalize hoti hai specifically isi wajah se |
| Speed bump har venue pe hota | Kuch specific venues ka design choice hai, universal nahi |

---

## Exercises

1. Do firms same colocation facility mein hain. Firm A ka code 36-
   LOW-LATENCY-CPP techniques use karta (pooling, cache-tuning, syscall-
   avoidance). Firm B ka code naive hai (heap allocation hot path pe,
   syscalls per message). Kya colocation dono ko barabar advantage deta?
   <details><summary>Answer</summary>
   Nahi. Physical network latency dono ke liye roughly same hogi (same
   facility, equalized cables), par TOTAL tick-to-trade Firm B ke liye
   bahut zyada hoga uske application-level inefficiencies (allocation,
   syscalls) ki wajah se — colocation sirf ek layer optimize karta,
   poori chain nahi.
   </details>

2. Physical distance kyun ek "hard floor" hai jo koi bhi software
   optimization hata nahi sakta?
   <details><summary>Answer</summary>
   Signal propagation speed of light (fiber ke refractive index se
   dheemi) se bound hai — yeh physics ka limit hai, engineering ka nahi.
   Isliye colocation (distance minimize karna) hi is specific latency
   source ka **sirf** solution hai; code-level optimization is particular
   component ko touch nahi kar sakta.
   </details>

---

## Interview questions

1. Colocation kya hai, aur yeh kyun specifically FIFO-matching venues
   (07) mein zyada critical hota?
2. Cross-connect cable length equalize kyun ki jaati?
3. Colocation "fair access" concern kyun raise karta, aur kaise address
   hota?
4. Colocation hone ke baad bhi kaunsi layers optimize karni padti?

---

## Next
→ [`12-hft-system-architecture.md`](12-hft-system-architecture.md)

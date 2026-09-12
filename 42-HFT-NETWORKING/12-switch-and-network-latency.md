# 12 — Switch latency, cut-through vs store-forward, cabling

## Prerequisites
- `11-tcp-for-order-gateways.md`
- `37-HFT-FUNDAMENTALS/11-colocation.md` (colocation physics — yeh
  lesson uski TECHNICAL detail hai)

## Yeh topic abhi kyun

01-11 ne NIC-se-app tak ka path optimize kiya. Yeh lesson us path se
**PEHLE** — packet abhi NETWORK mein hai, switches ke through guzar
raha — kya hota hai.

---

## Store-and-forward vs cut-through

```
Store-and-forward (traditional):
  Switch POORA packet RECEIVE karta (buffer mein), CHECKSUM verify
  karta, PHIR forward karna START karta.
  Latency = packet-size-dependent (bada packet = zyaada "store" time)

Cut-through:
  Switch packet ka DESTINATION ADDRESS (header ke sirf pehle kuch bytes)
  padhte hi FORWARD karna SHURU kar deta -- baaki packet abhi bhi aa
  raha hota jab forwarding shuru ho chuki.
  Latency = ALMOST packet-size-independent, bahut kam (~tens of ns per hop)
```

HFT-grade switches (colocation datacenters mein use hote) **cut-through**
hote — yeh EXACTLY woh "colo mein specific low-latency switches" hain
jo 37/11 mention karta.

**Trade-off**: cut-through checksum-verify NAHI karta forward karne se
pehle (speed ke liye) — agar packet corrupt hai, woh AAGE bhi forward
ho jaata (destination isse baad mein detect+drop karta). Store-and-
forward safer hai, SLOWER.

---

## Har switch HOP ek cost hai

```
Exchange -> Switch A -> Switch B -> Your rack -> Your server
```

Har hop (~tens of ns cut-through switches pe, ~microseconds store-and-
forward pe) **ADD** hota. Isliye **colocation** (37/11) itna matter
karta — tumhara server exchange ke JITNA KAREEB (fewer hops, chhoti
cable), utna kam latency.

---

## Cabling — speed of light matter karta

Fiber optic cable mein light ki speed **~5 ns per meter** (refractive
index ki wajah se vacuum se ~1.5x slower) — yeh **physics ki hard limit**
hai, koi engineering isse beat nahi kar sakti. 100 meter cable = ~500ns,
KHUD (koi switch/processing samet nahi).

| Distance | Approx one-way latency (fiber) |
|---|---|
| 10 m (same rack) | ~50 ns |
| 100 m (same datacenter, different racks) | ~500 ns |
| 1 km | ~5 µs |
| Mumbai to Chicago (~13000 km) | ~65 ms (fiber path realistically longer, ~70-100ms via undersea cables) |

**Yehi wajah hai** ki colocation itna valuable hai — 100 meters ka
fark (~500ns) bhi HFT ke context mein HUGE hai (37/14's latency budget
microseconds mein measured hota).

---

## Copper vs fiber

Copper (Ethernet cable) mein electrical signal ki speed KAAFI KAREEB
hai fiber ke, PAR copper **distance-limited** hai (typically ~100m max
for 10G+) aur zyaada **electromagnetic interference**-prone. Fiber
LONGER distances + kam interference deta, colocation racks ke ANDAR
(short distances) dono comparable, cross-datacenter LINKS almost
hamesha fiber.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — sochna "switch latency negligible hai"
Ek naive network diagram mein switches "free" lagte — REALITY mein har
hop measurable cost hai, especially store-and-forward switches pe
(microseconds, jo poore latency budget ka BADA hissa kha sakte).

### Trap 2 — cabling ko "sirf ek wire" samajhna
Cable LENGTH physics-level latency floor set karta — koi bhi software
optimization ISSE beat nahi kar sakti. Colocation (physically kareeb
hona) ek software problem NAHI hai, ek PHYSICS problem hai.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Sab switches same latency dete | Cut-through vs store-and-forward mein order-of-magnitude fark |
| Cable length "negligible" hai | Speed-of-light limit -- 100m = ~500ns, real cost |
| Software optimization se network-physics beat ho sakti | Nahi -- colocation/cabling PHYSICAL constraint hai |

---

## Exercises

1. Do racks ke beech 200 meter fiber cable hai, EK store-and-forward
   switch (add karta ~2µs) beech mein hai. Total added latency (one-way)
   kitna, roughly?
   <details><summary>Answer</summary>
   Cable: ~1000ns (200m * 5ns/m). Switch: ~2000ns. Total ~3000ns (3µs)
   one-way, sirf is EK hop + cable ke liye -- poori path mein aise kai
   hops ho sakte hain, sab additive.
   </details>

---

## Interview questions

1. Cut-through aur store-and-forward switching ka fark, aur trade-off.
2. Fiber mein light ki speed kya hai, aur yeh colocation ko kyun
   justify karta?
3. Copper vs fiber trade-offs.

---

## Next
→ [`13-fpga-offload-intro.md`](13-fpga-offload-intro.md)

# 12 — NUMA-aware placement, memory locality

## Prerequisites
- `11-avoiding-priority-inversion.md`
- `29-LINUX-SYSTEMS` (NUMA first-touch, `mbind`, poora already cover ho
  chuka — yeh lesson ISSE pipeline-placement context mein apply karta)

## Yeh topic abhi kyun

08 ne "kaunsa thread kis CORE pe" decide kiya. Multi-socket servers
(zyaadatar production HFT hardware) mein ek EXTRA dimension hai: **kaunsa
core kis MEMORY se close hai** — NUMA (Non-Uniform Memory Access).

---

## NUMA recap (29 se, 1-line)

Multi-socket server mein har CPU socket ki apni "local" RAM hoti — us
socket ke cores us LOCAL RAM ko FAST access karte, **doosre socket** ki
RAM ko access karna SLOWER hota (cross-socket interconnect ke through
jaana padta). 29's measured numbers (agar padha ho) isi latency-gap ko
quantify karte.

```
Socket 0 (cores 0-7)  <--local-->  RAM bank 0
     |
     +---- cross-socket interconnect (SLOWER) ----+
                                                    |
Socket 1 (cores 8-15) <--local-->  RAM bank 1  <---+
```

---

## Pipeline + NUMA — kya connect hota

Is folder ka pipeline model (01) ek zanjeer hai: Stage A -> queue -> Stage
B -> queue -> Stage C. Har queue **DONO taraf ke threads ISSE FREQUENTLY
touch karte** (producer likhta, consumer padhta) — agar Stage A aur Stage
B **DIFFERENT NUMA nodes** pe pinned hon, HAR queue-hand-off ek
cross-socket memory access ban jaata (slower, ~2-3x typical latency
penalty range, hardware-dependent).

**Rule:** ek PIPELINE (ya jo bhi threads EK SHARED queue/data structure
frequently touch karte) ko **SAME NUMA node** pe rakho jab tak koi
strong reason na ho alag rakhne ka.

```
✅ SAHI:  Stage A (core 2) aur Stage B (core 3) DONO Socket 0 pe
❌ GALAT: Stage A (core 2, Socket 0) aur Stage B (core 9, Socket 1) --
          unke beech ki queue HAR hand-off pe cross-socket traffic karti
```

---

## Symbol-sharding aur NUMA — ek natural fit

40/11's symbol-sharding principle (har symbol apna independent pipeline)
NUMA ke saath BEAUTIFULLY fit hota: **N symbols ko N NUMA nodes ke across
baant do**, har symbol ka POORA pipeline (feed-receive se order-send tak)
EK HI node pe. Symbols ke BEECH koi shared state hai hi nahi (design se),
isliye cross-node cost bhi kabhi nahi aata.

```
Node 0: Symbol A ka poora pipeline (feed, match, risk, send) -- 1 node
Node 1: Symbol B ka poora pipeline -- ALAG node, ALAG memory, NO
        cross-node traffic (kyunki A aur B kabhi communicate hi nahi karte)
```

---

## Memory allocation bhi NUMA-aware honi chahiye

Sirf THREADS pin karna kaafi nahi — jo MEMORY woh threads use karte
(queues, book state, arenas — 36's memory pools) bhi USSI node pe
allocate honi chahiye. 29's "**first-touch**" policy isi ko address
karta: Linux mein memory usually us NODE pe allocate hoti jo use PEHLE
"touch" (write) karta — isliye agar tumhara initialization code (jo
memory allocate/zero karta) SAME thread/core pe chale jo BAAD mein use
karega, memory automatically SAHI node pe ban jaati.

```cpp
// GALAT: main thread (Node 0) allocate karta, Node 1 ka worker use karta
auto* pool = new MemoryPool(...);   // Node 0 pe "touched" ho jaati
std::thread worker(node1_affinity, [pool] { pool->use(); });  // cross-node!

// SAHI: worker thread KHUD apni memory allocate/touch karta
std::thread worker(node1_affinity, [] {
    auto* pool = new MemoryPool(...);   // ab Node 1 pe touch hoti
    pool->use();
});
```

---

## ⚠️ Traps / Common mistakes

### Trap 1 — sirf core-pinning karna, memory-placement ignore karna
Agar thread Node 1 pe pinned hai PAR uski memory Node 0 pe allocate hui
(kisi aur thread ne allocate ki thi), CPU-pinning ka fayda memory-access
cost se KHO jaata. Dono saath mein handle karne chahiye.

### Trap 2 — single-socket desktop pe NUMA-tuning waste karna
Is course ka dev box (AAMD Ryzen 7 4700U, laptop) **single-socket** hai
— NUMA concerns yahan APPLY nahi hoti (koi cross-socket cost hai hi
nahi). Yeh SIRF multi-socket SERVERS pe matter karta — pehle CHECK karo
(`numactl --hardware` Linux pe, 29 mein) apna hardware kaisa hai.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| NUMA sirf "advanced" servers ki cheez hai, ignore kar sakte | Multi-socket production hardware pe DIRECTLY latency-impacting hai |
| Core-pinning kaafi hai NUMA-optimization ke liye | Memory placement (first-touch) BHI zaroori hai, saath mein |
| Symbol-sharding NUMA se unrelated hai | Natural fit -- har symbol ka pipeline EK node pe rakho |

---

## Exercises

1. Ek server 2 sockets rakhta, 4 symbols trade karne hain. Placement
   kaisa design karoge?
   <details><summary>Answer</summary>
   2 symbols Node 0 pe (unka poora pipeline -- threads AUR memory dono),
   2 symbols Node 1 pe. Symbols ke beech koi shared state nahi hoti
   (design se), isliye yeh split ZERO cross-node cost create karta.
   </details>

---

## Interview questions

1. NUMA kya hai, aur yeh multi-socket hardware pe kyun matter karta?
2. "First-touch" memory policy kya hai?
3. Symbol-sharding NUMA placement ke saath kaise naturally fit hota?

---

## Next
→ [`13-timing-and-sequencing.md`](13-timing-and-sequencing.md)

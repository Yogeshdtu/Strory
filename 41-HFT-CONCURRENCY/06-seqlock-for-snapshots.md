# 06 — Seqlock: market data snapshot bina lock ke

## Prerequisites
- `05-lmax-disruptor.md`
- `28-LOCK-FREE/05-seqlock.md`

## Yeh topic abhi kyun

03's "controlled sharing" escape-hatch ke liye ek concrete mechanism —
**seqlock**. Yeh 28-LOCK-FREE mein poori tarah derive + measure ho chuka
tha; is folder mein `seqlock.hpp` ke roop mein package kiya gaya (04's
SPSC queue jaisa hi treatment), aur **ek adversarial stress test** ke
saath re-measure kiya gaya — jo 28's original measurement se KAAFI zyaada
dramatic result deta hai.

---

## Mechanism (28 se recall, 1-line)

```
writer: seq++ (ODD)  ->  write fields (plain)  ->  seq++ (EVEN, publish)
reader: read seq -> agar ODD, retry -> copy fields -> read seq FIR SE ->
        agar SAME (aur even), snapshot CLEAN; nahi to retry
```

Writer KABHI block nahi hota. Readers ek doosre ko KABHI block nahi
karte. Yeh "1-writer, N-reader, frequently-updated, occasionally-read
(ya vice versa)" pattern ke liye SABSE fit hai — top-of-book BBO jaisa
data EXACTLY isi shape ka hai.

---

## `04_seqlock_snapshot.cpp` — measured, is box pe

**Setup:** 1 writer thread MAX RATE pe likhta (koi throttle nahi — yeh
DELIBERATELY adversarial hai, real writers itni fast kabhi nahi likhte),
4 reader threads 300,000 reads har ek.

```
seqlock        :   103-129 M reads/s   torn=0
shared_mutex   :     0.036-0.041 M reads/s   torn=0
```

**~2500-3500x difference** — yeh 28's original measurement (~80-100x)
se **kaafi zyaada dramatic** hai. Kyun? Is stress-test ka writer
GENUINELY max-rate hai (koi pace nahi) — is extreme contention mein
`std::shared_mutex` (is Windows/MinGW box pe) **collapse** kar jaata
(tens-of-thousands reads/sec tak), likely kyunki heavy contention ke
under har lock-acquire kernel-level wait/wake round-trip maangta (user-
space spin nahi, syscall). Seqlock is scenario mein **kabhi block hota
hi nahi** — writer aur readers dono apna kaam karte rehte, sirf occasional
retry (writer beech mein pakda jaana) ka cost.

> **Rule 2 note:** yeh number itna dramatic HAI ki "galat lag sakta" —
> par yeh bilkul REAL, measured hai (isolated smoke-test se bhi confirm
> kiya gaya, ~40K reads/sec `shared_mutex` alag se). Adversarial writer
> rate ek EXTREME case hai — real production mein writer itna aggressive
> shayad na ho, par yeh dikhata hai **WORST-CASE mein** dono approaches
> kitna alag behave karte.

---

## Retry rate — trade-off ka doosra pehlu

```
retry% = 552% se 1800% tak (run-to-run bahut variable)
```

Matlab average **5.5 se 18 retries PER READ** is adversarial setup mein
— writer itni fast likh raha ki reader BAAR BAAR beech mein pakda jaata.
Yeh seqlock ka HONEST trade-off hai: **writer rate badhne se retry rate
badhta**, par har retry itna SASTA hai (bas do atomic loads + ek copy)
ki total throughput phir bhi seqlock ko `shared_mutex` se HAZAARO guna
aage rakhta.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — high retry% ko "seqlock kharab hai" samajh lena
Retry% sirf yeh batata "writer kitni baar reader ko beech mein pakadta"
— yeh seqlock ka BUG nahi, uska EXPECTED behavior hai under contention.
Total throughput/latency dekho, sirf retry% nahi.

### Trap 2 — is adversarial test ke number ko "typical" number samajh lena
40 bytes ka `Bbo`, 4 readers, EK unthrottled writer — yeh WORST-CASE
scenario hai. Production mein writer rate BOUNDED hota (market data
itni hi fast aati jitni exchange bhejta) — real retry% yahan measured
number se KAAFI kam hoga typically.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| `shared_mutex` "readers ke liye theek hai kyunki multiple readers allowed" | Reader-reader theek hai, PAR writer-vs-readers dono taraf LOCK hai -- heavy write rate pe collapse ho sakta |
| Seqlock ka high retry% matlab galat design | Expected under adversarial writer rate; total throughput phir bhi jeet jaata |
| 2500x jaisa number "too good to be true" hai | Real, measured, adversarial-scenario-specific -- context ke saath samjho |

---

## Hands-on

```bash
./build.ps1 fast 41-HFT-CONCURRENCY/examples/04_seqlock_snapshot.cpp
```

---

## Exercises

1. Agar writer thread pace ki jaaye (jaisa real market data — 1 update
   har 10 microseconds), retry% ka kya hoga?
   <details><summary>Answer</summary>
   Kaafi kam hoga -- reader ke paas writer ke DO updates ke beech WAY
   zyaada waqt hoga apna read complete karne ke liye, isliye "writer ne
   beech mein pakda" wala scenario bahut rare ho jaata. Adversarial
   (unthrottled) writer isi retry-rate ko artificially high dikhata.
   </details>

---

## Interview questions

1. Seqlock kis specific access-pattern (1-writer, N-reader) ke liye
   ideal hai?
2. Is folder ka measured number (2500-3500x) 28's original (~80-100x)
   se itna zyaada kyun hai?
3. Retry% kya batata, aur high retry% "bug" kyun nahi hai?

---

## Next
→ [`07-busy-spin-vs-blocking.md`](07-busy-spin-vs-blocking.md)

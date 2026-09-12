# 07 — Busy-spin trade-off: latency vs CPU, measured

## Prerequisites
- `06-seqlock-for-snapshots.md`
- `28-LOCK-FREE/06-mutex-vs-lockfree.md` (latency-only comparison, is
  lesson ka aadha)

## Yeh topic abhi kyun

28/06 ne LATENCY measure ki thi (spin ~0.4us p50 vs mutex+cv ~6us p50).
Woh comparison **AADHA** trade-off dikhata hai. Doosra AADHA: spin apna
speed **poore ek core ko continuously burn karke** paata — yeh cost
kabhi measure nahi hui thi. Yeh lesson DONO axes ek saath dikhata hai,
aur ek **hybrid** teesra option add karta.

---

## Teen strategies

| Strategy | Consumer kya karta jab queue khaali ho |
|---|---|
| **Spin** | Turant loop wapas, `try_pop()` phir call karo (tight loop) |
| **Block** | `condition_variable::wait_for()` -- thread SLEEP karta, OS ise wake karta jab data aaye |
| **Hybrid** | Pehle THODI der spin karo (jaisa 500 iterations), phir agar abhi bhi kuch nahi, block karo |

---

## Measured (`05_busy_spin_vs_cv.cpp`, N=100K paced ~5us apart)

```
           p50        CPU% of wall time
spin      210.4 ns    99.8%   <- ek POORA core continuously busy
block    8997.1 ns    27.9%   <- ~43x WORSE latency, PAR core mostly free
hybrid   7354.0 ns    24.9%   <- median beech mein, CPU bhi kam
```

**Latency:** spin sabse fast (~43x behtar median block se). **CPU cost:**
spin ~100% ek core, block/hybrid ~25-30% (thread zyaadatar SOTA rehta,
wake hone tak).

> **CPU-time measurement kaise hua:** Windows `GetThreadTimes()` (kernel+
> user time) us THREAD ke liye — par MinGW ka `std::thread::native_handle()`
> is (posix-threading) build pe ek `pthread_t` deta hai, REAL Win32
> `HANDLE` nahi (`reinterpret_cast` karke seedha `GetThreadTimes()` ko dena
> `ERROR_INVALID_HANDLE` deta). Fix: thread ke ANDAR se
> `DuplicateHandle(GetCurrentThread())` call karo — yeh pseudo-handle ko
> ek REAL, kisi bhi thread se usable handle mein convert karta. Yeh khud
> ek chhota "OS API quirk" hai jo actual measurement karte waqt mila.

---

## Yeh trade-off KAB matter karta

| Situation | Choice |
|---|---|
| Dedicated/isolated core available (koi aur kaam nahi) | **Spin** — core WAISE bhi idle rehta, latency ke liye use karo |
| Core SHARED hai (baaki kaam bhi chalna hai) | **Block** ya **hybrid** — CPU wapas do jab kaam nahi |
| Latency-CRITICAL path, occasional CPU-cost acceptable | **Spin** |
| Bahut saare aise threads hain (N cores nahi hain sabke liye) | **Block/hybrid** — sab spin karein to system OVERSUBSCRIBED ho jaata |

**HFT ka production answer:** matching-engine/hot-path threads **spin**
karte, **dedicated isolated cores** pe (08) — kyunki latency sabse zyaada
matter karta AUR core "waise bhi kisi aur kaam ke liye nahi hai."
Housekeeping threads (logging consumer, monitoring) **block** karte —
unhe microsecond latency ki zaroorat nahi.

---

## Hybrid — asli fayda kab

Hybrid ka POORA POINT hai: "zyaadatar case mein data JALDI aa jaata (spin
phase mein pakad lo), sirf lambi idle periods mein CPU chhodo (block phase)."
Is measured run mein hybrid ka p50 (7354ns) block se thoda BEHTAR tha
(spin phase kabhi-kabhi kaam aata), CPU bhi kam (~25%) — ek reasonable
middle ground. **Par tail (p99+) yahan sab teeno mein OS-jitter-dominated
hai** (unpinned desktop, 04/08 ka SAME finding) — hybrid ka tail is
specific run mein spin/block se BHI worse dikha, jo strategy ki galti
nahi, is box ke scheduler-noise ki wajah se hai (multiple runs se variance
confirm hui).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — "spin hamesha behtar hai, CPU sasta hai"
Agar N threads spin karte hain par sirf M < N cores available hain,
spinning threads EK DOOSRE se CPU cheenenge (OS unhe alag-alag baari
se time-slice karega) — latency ADVANTAGE poori tarah KHATAM ho jaata,
sirf power/heat waste hoti. Spin sirf tab jeetta jab GENUINELY dedicated
core ho.

### Trap 2 — CPU measurement bhool jaana, sirf latency dekhna
28/06 ne sirf latency dikhaya — is folder ka poora point hai ki BINA
CPU-cost dekhe "spin hamesha jeetta" jaisa galat conclusion nikal sakta.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Spin hamesha behtar choice hai (sirf latency dekho) | Dedicated core na ho to spin poori latency-advantage khatam kar sakta |
| Hybrid hamesha spin aur block dono se behtar hota | Median mein beech mein hota; tail scheduler-noise se dominate ho sakta, guarantee nahi |
| CPU-time measurement "extra detail" hai | Yeh trade-off ka ADHA hissa hai -- bina iske decision incomplete |

---

## Hands-on

```bash
./build.ps1 fast 41-HFT-CONCURRENCY/examples/05_busy_spin_vs_cv.cpp
```

---

## Exercises

1. Ek system mein 16 hot-path threads hain, sirf 8 physical cores. Sab
   spin karte hain. Kya hoga?
   <details><summary>Answer</summary>
   Oversubscription -- OS unhe time-slice karega, har thread ko baari-
   baari CPU milega, matlab BAAKI 15 threads' spin-loops bhi EK doosre
   ki latency badhaate (context switches, cache thrash). Spin ka poora
   fayda TABHI milta jab thread-count <= dedicated-core-count ho.
   </details>

---

## Interview questions

1. Spin, block, hybrid -- teeno ka exact CPU-cost aur latency profile.
2. Kab spin appropriate hai, kab block?
3. `GetThreadTimes()` ke saath MinGW's `native_handle()` ka quirk kya
   tha, aur kaise fix hua?

---

## Next
→ [`08-core-pinning-strategy.md`](08-core-pinning-strategy.md)

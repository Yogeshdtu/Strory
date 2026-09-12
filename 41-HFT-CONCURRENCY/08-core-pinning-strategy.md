# 08 — Which thread where: isolated cores, NUMA nodes

## Prerequisites
- `07-busy-spin-vs-blocking.md`
- `29-LINUX-SYSTEMS/examples/06_cpu_affinity.linux.cpp` (Linux ka
  production API, already built + verified)

## Yeh topic abhi kyun

07 ne dikhaya spin sirf "DEDICATED core" pe fayda karta. Yeh lesson
EXACTLY define karta "dedicated core" ka matlab kya hai — aur affinity
(thread ko ek core se BAND karna) aur isolation (us core se BAAKI sab
ko HATANA) ka fark, measured.

---

## Affinity vs isolation — do ALAG cheezein

| | Affinity | Isolation |
|---|---|---|
| Kya karta | "Yeh thread SIRF is core pe chalega" | "Is core pe SIRF yeh thread chalega" |
| API | `SetThreadAffinityMask` (Windows), `sched_setaffinity`/`pthread_setaffinity_np` (Linux) | `isolcpus`, `nohz_full` (Linux boot params — kernel-level, 29's checklist) |
| Kya guarantee NAHI deta | Kuch nahi ki koi AUR thread/process/interrupt us core pe na aaye | (khud complete hai agar sahi configure ho) |
| Kiska kaam | Application-level (ek syscall/API call) | OS/kernel-config-level (boot-time setup) |

**Dono chahiye production HFT mein** — sirf affinity (thread ko pin karna)
KAAFI NAHI hai agar OS baaki sab kuch (background processes, IRQs, doosre
threads) usi core pe bhi schedule karta rahe.

---

## `06_core_pinning.cpp` — measured (Windows-native affinity, is box pe)

```cpp
DWORD_PTR mask = (DWORD_PTR)1 << (num_cores - 1);
SetThreadAffinityMask(GetCurrentThread(), mask);
```

**Measured** (20M tight-loop iterations, tick-delta between consecutive
iterations — ek bada delta = thread preempt/move hua):

```
unpinned  : p50 60  p99  140  p99.9  160  max     829,657 ticks  hiccups=782/20M
pinned    : p50 60  p99   80  p99.9  140  max  15,600,420 ticks  hiccups=511/20M
```

**Kya hua:** pinned ka p99/p99.9 THODA behtar (140 vs 160, aur `hiccups`
count kam — 511 vs 782), **PAR max WAY worse** (15.6M ticks ≈ ~7.8ms,
vs unpinned ka 829K ticks ≈ ~0.4ms)! Ek SINGLE freak stall pinned run
mein hua.

> **Rule 2:** yeh EXACT wahi result hai jo affinity-vs-isolation ka fark
> predict karta — pinning ne hiccup FREQUENCY kam ki (thread ab wahi
> ek core pe stable rehta, kam migration), PAR isolation ke bina, us EK
> core ko koi AUR process/interrupt kabhi-kabhi le sakta hai — jab woh
> hota, koi "doosra core" bhi nahi hota jahan yeh thread bhaag sake,
> isliye jab hit hota hai, POORA delay us EK thread pe padta (jabki
> unpinned thread OS ko kisi AUR idle core pe move karne ka OPTION deta).
> Yeh **explicitly WORSE max** hona, itna important finding hai ki isse
> chhupaya nahi jaa sakta — yehi is folder ka Rule-2 moment hai.

**Production fix:** `isolcpus`/`nohz_full` (Linux boot params, 29's
checklist) us core ko **poori tarah OS scheduler se HATA dete** — koi
background task, koi timer interrupt wahan schedule NAHI hota. Tab
pinning ka fayda GUARANTEED milta (koi "kabhi kabhi koi aur aa gaya"
wala freak case nahi). Is desktop box pe (NO isolcpus) hum sirf
"AFFINITY" demonstrate kar sakte, "ISOLATION" nahi.

---

## Which thread WHERE — placement strategy

```
Core 0-1:  OS + background processes (kernel housekeeping)
Core 2:    ISOLATED -- matching engine (spin, latency-critical)
Core 3:    ISOLATED -- feed receiver (spin, latency-critical)
Core 4:    pinned (not isolated) -- risk checks (spin okay, less critical)
Core 5-7:  pinned (not isolated) -- logging, monitoring, market-data-out (block okay)
```

Sabse latency-critical threads **isolated** cores pe, kam-critical
threads normal cores pe (block/spin dono chal sakte, 07 ke trade-off
guide se decide karo).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — affinity ko isolation samajh lena
"Maine thread pin kar diya, ab yeh guaranteed low-latency hai" — GALAT
bina OS-level isolation ke, jaisa `06`'s measured result dikhata (max
case ACTUALLY worse ho sakta).

### Trap 2 — SABKO ek hi isolated core pe pin kar dena
Agar 2 latency-critical threads SAME isolated core pe pin ho jaayein,
woh EK DOOSRE ko compete karenge (isolation sirf BAAKI system se
protect karta, ek-doosre se nahi). Har critical thread ka apna DEDICATED
core hona chahiye.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Pinning = guaranteed low jitter | Pinning sirf "yahin rahega" guarantee karta, "akela rahega" nahi (isolation chahiye) |
| Pinned hamesha unpinned se behtar (har metric) | Is measured case mein max WORSE tha -- context-dependent |
| Isolation Windows API se ho sakta jaisa affinity | Isolation boot-time kernel config hai (Linux `isolcpus`), application API se nahi |

---

## Hands-on

```bash
./build.ps1 fast 41-HFT-CONCURRENCY/examples/06_core_pinning.cpp
```

---

## Exercises

1. Kyun pinned thread ka MAX delay unpinned se worse aa sakta, jabki
   uska p99 behtar tha?
   <details><summary>Answer</summary>
   Pinned thread sirf EK core pe chal sakta -- agar us core pe kabhi
   koi AUR (background task/interrupt) aa jaaye, pinned thread ka koi
   "escape route" nahi (kisi doosre idle core pe migrate nahi kar sakta,
   affinity mask ise rokta). Unpinned thread ke paas OS ke paas OPTION
   hota ise turant kisi aur free core pe daal dena. Isliye rare worst-case
   (jab conflict genuinely hota) pinned ke liye WORSE ho sakta bina
   isolation ke.
   </details>

---

## Interview questions

1. Affinity aur isolation ka fark, aur dono kyun chahiye production mein.
2. `06`'s measured result (pinned max worse) ka mechanism explain karo.
3. Different-criticality threads ko cores pe kaise distribute karoge?

---

## Next
→ [`09-lock-free-logging.md`](09-lock-free-logging.md)

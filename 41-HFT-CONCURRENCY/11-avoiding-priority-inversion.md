# 11 — Priority inversion, RT scheduling ke saath

## Prerequisites
- `10-wait-free-reads.md`
- `29-LINUX-SYSTEMS/examples/10_realtime_thread.linux.cpp` (SCHED_FIFO,
  already built + verified)

## Yeh topic abhi kyun

Is folder ne baar-baar "single-writer," "shared-nothing," "wait-free"
jaisi cheezein isliye push ki hain ki woh SIRF latency ke liye nahi —
woh ek **structural bug class** ko bhi avoid karti hain: **priority
inversion**. Yeh classic distributed-systems/RT-systems bug hai (1997
ka Mars Pathfinder incident isi se hua tha), aur samajhna zaroori hai
kyunki iska fix "shared-nothing design already isse avoid karta" hai.

---

## Classic scenario

```
Priority: HIGH > MEDIUM > LOW

1. LOW-priority thread ek shared resource (lock) LETA hai.
2. HIGH-priority thread us SAME resource ka wait karta -- BLOCKED.
3. MEDIUM-priority thread (jo resource se koi lena-dena nahi rakhta)
   ARRIVE karta -- scheduler ise LOW se PEHLE run karta (kyunki
   MEDIUM > LOW priority hai).
4. LOW thread ab RUN hi nahi ho pata (MEDIUM occupy kiye hue hai CPU) --
   isliye woh apna resource kabhi RELEASE nahi kar pata.
5. HIGH-priority thread INDEFINITELY block reh jaata -- effectively
   MEDIUM-priority thread ne HIGH ko "overrule" kar diya, jabki HIGH
   ki priority MEDIUM se zyaada thi!
```

**Yeh "priority INVERSION" hai** — priority order effectively ULTA ho
gaya (MEDIUM ne HIGH ko block kar diya, indirectly).

---

## Kyun HFT mein isse care karna

Agar tumhara matching-engine thread (HIGH priority, latency-critical)
kisi shared resource (lock) ke through ek LOW-priority housekeeping
thread (jaisa metrics collector) se interact karta hai, aur beech mein
ek MEDIUM-priority thread aa jaaye — matching engine **indefinitely
stall** ho sakta. Yeh EXACTLY woh worst-case tail-latency scenario hai
jo 37/14's latency budget destroy kar deta.

---

## Fixes (traditional RT-systems approach)

| Fix | Kaise kaam karta |
|---|---|
| **Priority inheritance** | LOW thread, jab tak HIGH uska resource maang raha hai, TEMPORARILY HIGH priority "udhaar" leta — MEDIUM ab use preempt nahi kar sakta |
| **Priority ceiling protocol** | Har resource ki ek "ceiling priority" hoti (uske sabse HIGH possible user ke barabar) — jo bhi thread resource lock kare, TURANT us ceiling priority pe chala jaata |
| Dono OS/RT-scheduler support maangte (Linux `SCHED_FIFO` + `PTHREAD_PRIO_INHERIT`, 29's RT-scheduling lesson) | |

---

## HFT ka BEHTAR fix: shared resource hi MAT rakho

Traditional fixes **shared locks ko handle karne ke tareeke** hain — HFT
ka approach isse EK level upar solve karta: **agar HIGH aur LOW priority
threads ke beech koi SHARED LOCK hi nahi hai (03's shared-nothing design),
priority inversion STRUCTURALLY impossible hai** — invert karne ke liye
kuch shared hona CHAHIYE jispe dono compete karein.

```
Traditional system:      matching-thread <--[shared lock]--> logging-thread
                          (priority inversion RISK)

Yeh folder ka design:    matching-thread --[SPSC queue, single-writer]--> logging-thread
                          (koi shared LOCK nahi -- producer sirf apni
                           side likhta, consumer apni side; koi ek
                           doosre ko BLOCK nahi kar sakta)
```

`07`'s async logger isi principle ka DIRECT beneficiary hai: matching
engine (HIGH priority) `try_push()` karta (non-blocking, kabhi kisi ka
INTIZAR nahi karta) — LOW-priority logging thread chahe kitna bhi
peeche ho, HIGH-priority thread ISSE KABHI block nahi hota. **Priority
inversion ka structural avoidance, priority-inheritance ka RUNTIME fix
nahi.**

---

## Jahan shared lock avoid NAHI ho sakta

Kabhi-kabhi ek genuinely shared resource chahiye hota (jaisa ek OS-level
resource, file handle, ya third-party library jo apna internal lock
rakhti). Wahan Linux `SCHED_FIFO` + priority inheritance mutex
(`pthread_mutexattr_setprotocol(PTHREAD_PRIO_INHERIT)`) use hota — 29's
`10_realtime_thread.linux.cpp` mein RT scheduling ka foundation already
hai.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — "lock-free code mein priority inversion nahi ho sakta"
Spin-based code mein bhi ek FORM ho sakta: agar LOW-priority thread ek
SPIN-lock hold kar raha ho aur MEDIUM-priority thread usi CPU pe usse
preempt kar de (RT scheduling ke bina), LOW thread apna spin-lock kabhi
release nahi kar pata jab tak MEDIUM khatam na ho. Spin-locks
specifically RT-scheduled (SCHED_FIFO) systems mein use karne se PEHLE
yeh dhyaan rakhna padta.

### Trap 2 — priority inversion ko "rare edge case" samajh ke ignore karna
Mars Pathfinder ka bug PRODUCTION mein hua (spacecraft resets kar raha
tha Mars pe) — yeh THEORETICAL nahi hai, real systems mein REAL impact
rakhta hai.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Priority inversion sirf traditional OS scheduling mein hota | Spin-based systems mein bhi (RT scheduling ke bina) ho sakta |
| Fix hamesha priority inheritance hai | HFT mein BEHTAR fix: shared lock hi avoid karo (shared-nothing) |
| Yeh EK rare/theoretical bug hai | Production incidents (Mars Pathfinder) mein REAL impact dikha chuka |

---

## Exercises

1. Kyun 07's async logger design priority inversion se STRUCTURALLY safe
   hai (traditional priority-inheritance FIX ki zaroorat hi nahi)?
   <details><summary>Answer</summary>
   Kyunki hot-path (HIGH priority) sirf `try_push()` karta -- kabhi kisi
   LOCK ka wait NAHI karta (non-blocking). Priority inversion ki
   PRECONDITION hi yeh hai ki HIGH thread kisi SHARED resource ka wait
   kare jo LOW thread hold kiye ho -- yahan koi aisa wait hi nahi hota.
   </details>

---

## Interview questions

1. Priority inversion ka classic scenario (3 priority levels ke saath)
   explain karo.
2. Priority inheritance aur priority ceiling protocol mein fark.
3. HFT ka "structural avoidance" approach traditional fixes se kaise
   BEHTAR hai?

---

## Next
→ [`12-numa-thread-placement.md`](12-numa-thread-placement.md)

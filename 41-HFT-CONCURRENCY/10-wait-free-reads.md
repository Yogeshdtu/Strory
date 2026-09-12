# 10 — Wait-free read paths for strategy threads

## Prerequisites
- `09-lock-free-logging.md`
- `28-LOCK-FREE/00-README.md` (hazard pointers / epochs / RCU — is
  lesson unhe SEEDHA use karta)

## Yeh topic abhi kyun

06 mein humne seqlock dekha — "lock-free" reads. Yeh lesson ek **sharper**
distinction banata: lock-free aur **wait-free** ALAG guarantees hain, aur
ek strategy thread (jo apna decision LOOP mein baar-baar chalata, kabhi
"thoda wait kar lo" allow nahi kar sakta) ko genuinely wait-free chahiye
ho sakta hai.

---

## Progress-guarantee hierarchy (26/27/28 se recall, sharpened)

| Guarantee | Matlab | Example |
|---|---|---|
| **Blocking** | Ek thread doosre thread ko INDEFINITELY rok sakta | `std::mutex` |
| **Lock-free** | System-WIDE kam se kam EK thread hamesha aage badhta hai (koi thread poore system ko permanently freeze nahi kar sakta), PAR koi EK SPECIFIC thread apni khud ki retry-loop mein theoretically bahut der tak phasa reh sakta | Seqlock reader (06) — agar writer BAAR BAAR beech mein aata rahe, ek UNLUCKY reader baar-baar retry karta reh sakta |
| **Wait-free** | HAR thread apna operation **bounded number of steps** mein complete karta, chahe baaki threads kuch bhi karein | Ek single atomic load (koi retry-loop hi nahi) |

**Seqlock's readers `lock-free` hain, `wait-free` NAHI** — 06's measured
"retry% = 1800%" isi ka proof hai (ek reader kabhi-kabhi KAI baar retry
karta, adversarial writer ke against). Zyaadatar HFT use-cases ke liye
yeh THEEK hai (retries SASTE hain, average case bahut fast). Par kuch
strategy threads (jo latency-BUDGET pe HARD deadline rakhte, 37/14) ko
truly BOUNDED read chahiye ho sakta.

---

## Atomic pointer-swap — genuinely wait-free reads

```cpp
std::atomic<std::shared_ptr<const Snapshot>> current_;   // C++20

// Writer: naya snapshot banao (poora naya object), ATOMICALLY swap karo
void publish(Snapshot s) {
    current_.store(std::make_shared<const Snapshot>(std::move(s)),
                    std::memory_order_release);
}

// Reader: EK atomic load, DONE. Koi retry-loop hi nahi.
std::shared_ptr<const Snapshot> read() const {
    return current_.load(std::memory_order_acquire);
}
```

Reader ka kaam **EK bounded step** mein khatam ho jaata — chahe writer
kitni bhi fast publish kare, reader KABHI retry nahi karta (jo bhi
snapshot us waqt current_ tha, wahi milta — pura consistent, kyunki
`Snapshot` khud IMMUTABLE hai ek baar publish hone ke baad). Yeh
**genuinely wait-free** hai.

**Trade-off:** har `publish()` ek NAYA allocation karta (`make_shared`)
— seqlock (jo EK JAGAH IN-PLACE update karta, zero allocation) se KAAFI
zyaada costly PER WRITE. Aur `shared_ptr`'s refcounting khud EK atomic
RMW hai har copy/destroy pe (28's atomics ka reminder — "atomic" free
nahi hota).

---

## Kab kaunsa

| Situation | Choice |
|---|---|
| Writer bahut FAST hai, chhota struct, retry saste hain | **Seqlock** (06) — zero allocation, in-place |
| Reader ko GENUINELY bounded-step guarantee chahiye (hard real-time-ish) | **Atomic snapshot swap** — allocation cost acceptable trade-off |
| Reads bahut RARE hain (kabhi-kabhi hi query hoti) | Atomic snapshot swap — allocation cost per-write amortize ho jaata |
| Writes bahut FREQUENT hain (millions/sec) | Seqlock — snapshot-swap ka per-write allocation isko impractical bana deta |

---

## Reclamation — asli complexity

Snapshot-swap approach ka HARD part memory reclamation hai: **purana
snapshot kab free karein?** Agar koi reader ABHI bhi purane snapshot ko
hold kiye hue hai (`shared_ptr` copy le chuka), use IMMEDIATELY free
karna use-after-free hota. `std::shared_ptr`'s atomic refcount ISI ko
solve karta (jab tak koi reference hai, object zinda rehta) — yeh EK
practical, correct approach hai, PAR 28's **hazard pointers**/**epoch-
based reclamation**/**RCU** in-depth is EXACT problem ke DEEPER (aur
FASTER, no-atomic-refcount) solutions hain, jab `shared_ptr` ka overhead
bhi bahut zyaada ho jaaye.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — "lock-free" aur "wait-free" ko synonym samajhna
Bahut common confusion — "lock-free" sirf "no mutex" guarantee karta,
"koi retry-loop nahi" NAHI. Interview mein yeh distinction poochha
jaata specifically.

### Trap 2 — snapshot-swap ko har jagah use karna (allocation cost bhool ke)
Agar writer millions/sec update karta, `make_shared` ka allocation cost
(chahe 36's memory pool se bhi optimize karo) seqlock ke plain-write se
KAAFI zyaada rahega. Choice workload-dependent hai.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Lock-free = wait-free | Wait-free STRONGER guarantee hai (bounded steps, no retry) |
| Seqlock wait-free hai | Seqlock lock-free hai, wait-free NAHI (unbounded retry theoretically possible) |
| Atomic snapshot swap "free" hai (bas ek atomic) | Har publish ek ALLOCATION hai; refcounting khud atomic RMW cost rakhta |

---

## Exercises

1. Ek strategy thread ka latency-budget (37/14) itna tight hai ki ek
   SINGLE unlucky retry bhi budget bust kar sakta. Kaunsa approach
   (seqlock vs snapshot-swap) safer hai?
   <details><summary>Answer</summary>
   Snapshot-swap (wait-free) — bounded-step guarantee ka EXACTLY yeh
   matlab hai ki "kabhi bhi ek unlucky retry-cascade" NAHI ho sakta.
   Seqlock ka retry-loop, chahe PRACTICALLY rare ho, THEORETICALLY
   unbounded hai.
   </details>

---

## Interview questions

1. Lock-free aur wait-free ka exact difference batao.
2. Atomic pointer/shared_ptr swap pattern kaise kaam karta, aur uska
   trade-off (seqlock ke against) kya hai?
3. Memory reclamation snapshot-swap mein kyun ek genuine problem hai?

---

## Next
→ [`11-avoiding-priority-inversion.md`](11-avoiding-priority-inversion.md)

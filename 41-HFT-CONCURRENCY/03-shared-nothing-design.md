# 03 — Shared-nothing design: data partitioning, message passing over sharing

## Prerequisites
- `02-single-writer-principle.md`

## Yeh topic abhi kyun

02 ne "single writer" (per-variable rule) diya. Yeh lesson us rule ko
**architecture-level design philosophy** mein extend karta: poori
possible extent tak, threads ke beech **koi mutable state hi share mat
karo** — jo bhi cross karna hai, **message** banao aur queue se bhejo.

---

## Sharing vs message-passing

```
SHARING (traditional multithreading):
  Thread A -----\
                 >---- [shared struct, mutex-protected] ----< Thread B
  Thread C -----/

  Har thread SEEDHA struct ko access karta -- concurrent access
  coordinate karne ke liye lock ZAROORI.

MESSAGE PASSING (shared-nothing):
  Thread A --[message]--> Queue --[message]--> Thread B

  Thread A apna DATA bhejta (COPY, ownership transfer), Thread B apna
  ALAG copy consume karta. Koi shared mutable memory hi nahi -- lock ki
  zaroorat structurally KHATAM.
```

Yeh Go/Erlang jaisi languages ki philosophy ("Do not communicate by
sharing memory; instead, share memory by communicating") ka C++/HFT
version hai — bas yahan queue ek LOCK-FREE SPSC ring hai (04), object
ownership transfer ek `memcpy`/struct-copy hai (heap allocation NAHI —
36's principle), aur "message" ek POD struct hai.

---

## Is course mein already dikhe examples

- `08_pipeline_demo.cpp`: `PipelineOrder`/`PipelineTrade` structs COPY
  hoke queue se guzarte — koi shared `Order` object nahi jise dono
  stages access karte.
- `39-ORDER-BOOK`/`40-MATCHING-ENGINE`: matching engine ki poori state
  (bids_/asks_/index_) sirf UPDATE hoti apne hi thread ke andar; baaki
  system se sirf `Trade` events (messages) BAAHAR jaate.

---

## Data partitioning — jab sharing avoid nahi ho sakti

Kabhi-kabhi "true" shared state chahiye hi hota (jaisa top-of-book
snapshot — bahut readers isse dekhna chahte, ek fresh copy har baar
banana wasteful hota). Yahan pura "shared-nothing" nahi lagta — par
**controlled sharing** (06's seqlock) use hota: EK writer, SNAPSHOT-style
reads, koi lock. Yeh shared-nothing ka "escape hatch" hai — jab data
GENUINELY shared hona CHAHIYE, seqlock/atomic-snapshot use karo, RAW
shared-mutable-struct-with-mutex NAHI.

| Situation | Approach |
|---|---|
| Data ek stage se agle stage tak "flow" karta (event) | Message-passing (queue) |
| Data "current state" hai jo MULTIPLE readers baar-baar dekhte | Controlled sharing (seqlock/atomic snapshot, 06) |
| Data ek hi thread ke andar rehta, kabhi bahar nahi jaata | Normal local variable, koi concurrency concern hi nahi |

---

## ⚠️ Traps / Common mistakes

### Trap 1 — message mein POINTER/REFERENCE bhejna
Agar queue mein ek pointer bhejo (`Order*` ki jagah `Order`), receiving
thread ab SENDING thread ki memory ko access kar raha — yeh sharing ban
gaya, message-passing nahi! Ownership genuinely transfer nahi hui.
`PipelineOrder`/`PipelineTrade` isliye VALUE types hain (COPY hote), na
ki pointers.

### Trap 2 — "shared-nothing" ko "no communication" samajh lena
Shared-nothing ka matlab NO SHARING hai, NO COMMUNICATION nahi — threads
zaroor communicate karte (queues ke through), bas SHARED MEMORY se nahi.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Shared-nothing = threads isolated, kabhi baat nahi karte | Threads queues (messages) se communicate karte, shared MUTABLE memory se nahi |
| Har cheez ko queue se bhejna chahiye, kabhi shared state nahi | Genuinely-shared "current state" (BBO snapshot) ke liye seqlock jaisa controlled-sharing theek hai |
| Message mein pointer bhejna "efficient" hai (copy avoid) | Ownership-sharing bana deta, poori shared-nothing guarantee todta |

---

## Exercises

1. Ek strategy thread ko current top-of-book CHAHIYE (bahut baar padhta,
   milliseconds mein kai baar). Kya isse queue se "message" bhejna chahiye
   har update pe, ya seqlock use karna chahiye?
   <details><summary>Answer</summary>
   Seqlock (06) — yeh "current state jo MULTIPLE readers baar-baar
   dekhte" scenario hai, message-passing yahan HAR update ko queue mein
   daalna wasteful hota (reader sirf LATEST value chahta, poori history
   nahi) — seqlock exactly "latest published snapshot" pattern deta.
   </details>

---

## Interview questions

1. Sharing aur message-passing ka fundamental difference batao.
2. Kab "controlled sharing" (seqlock) appropriate hai, message-passing
   ki jagah?
3. Pointer/reference queue mein bhejna kyun shared-nothing violate karta?

---

## Next
→ [`04-spsc-queue-for-pipeline.md`](04-spsc-queue-for-pipeline.md)

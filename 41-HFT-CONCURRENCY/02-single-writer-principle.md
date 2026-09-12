# 02 — Single writer: ek data ka ek hi malik, no locks needed

## Prerequisites
- `01-hft-threading-model.md`

## Yeh topic abhi kyun

01 ne pipeline model dikhaya. Yeh lesson us model ke ANDAR ka EXACT rule
hai jo locks ki zaroorat hi khatam kar deta: **kisi bhi piece of mutable
state ka EXACTLY ek thread hamesha "owner" (writer) hona chahiye.**

---

## Rule

> Ek variable/structure ko sirf EK thread MODIFY kar sakta, hamesha. Baaki
> threads (agar unhe access chahiye) sirf PADH sakte — aur woh read bhi
> ek defined mechanism se (atomic load, seqlock, ya khud ka copy) hona
> chahiye, direct shared-memory read nahi.

Yeh already is course mein baar-baar dikha hai, bas naam nahi diya gaya
tha:

| Kahan already dikha | Writer | Reader(s) |
|---|---|---|
| `spsc_queue.hpp` (04) | producer thread `head_` likhta | consumer sirf `head_` PADHTA (apna `cached_head_`) |
| `spsc_queue.hpp` (04) | consumer thread `tail_` likhta | producer sirf `tail_` PADHTA |
| `Seqlock<T>` (06) | writer thread `value_` likhta | readers sirf PADHTE (retry-loop se) |
| `MatchingEngine` (40) | matching thread poori book likhta | (koi doosra thread book touch nahi karta — poori tarah single-writer) |
| `DisruptorRing` (05) | producer `cursor_` likhta | consumers sirf `cursor_` PADHTE; har consumer apna `consumer_seqs_[i]` likhta |

**Notice:** kai jagah do "writers" dikhte (producer AUR consumer dono
kuch likhte), par HAR VARIABLE ka apna EK writer hai — `head_` sirf
producer likhta, `tail_` sirf consumer likhta. Yeh single-writer-PER-
VARIABLE hai, "sirf ek thread poori queue likh sakta" nahi.

---

## Kyun yeh locks ki zaroorat khatam karta

Ek lock ISLIYE zaroori hota kyunki **do threads EK saath EK cheez ko
MODIFY** kar sakte the (race condition risk). Agar sirf EK thread kabhi
modify karta hai, doosra thread sirf padh raha hai — yeh ek "single
producer, multiple consumer of a value" scenario hai jo atomics (release/
acquire) se, KOI lock ke bina, safely handle ho jaata:

```cpp
// Writer (single thread):
value_.store(new_val, std::memory_order_release);

// Reader (any thread):
auto v = value_.load(std::memory_order_acquire);
```

Yahan koi CAS-retry-loop nahi chahiye (jaisa multi-writer scenario mein
chahiye hota, 28's Treiber stack) — sirf ek plain store/load, release/
acquire ka pairing consistency guarantee karta.

---

## Ek subtlety — "single writer" ka scope

Single-writer principle **per-variable** scope rakhta hai, poore
program-level nahi. `DisruptorRing` mein producer thread `cursor_`
likhta HAI, PAR `consumer_seqs_[i]` NAHI likhta — woh sirf consumer `i`
likhta. Agar galti se producer `consumer_seqs_[i]` ko bhi likh de (jaisa
"ache se initialize karne" ke bahane), yeh principle TOOT jaata, aur ab
`consumer_seqs_[i]` pe race condition possible ho jaati.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — "single writer" ko "single thread poora program" samajh lena
Single-writer ka matlab **poora single-threaded program** nahi hai —
matlab hai **har specific piece of mutable state ka EK fixed owner**.
Ek program mein 10 threads ho sakte, har ek apni-apni cheez ka single
writer, sabka data ALAG.

### Trap 2 — SPSC queue ko do producers se use karna
`SpscQueue::try_push()` do threads se call karna EXACTLY single-writer
principle violate karna hai — `head_` ab do writers ke beech race karta
(UB). `spsc_queue.hpp` explicitly documents yeh precondition — koi
runtime check nahi hai (checking khud ek cost hoti, aur SPSC ka poora
point yeh cost avoid karna hai).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Single-writer = single-threaded program | Single-writer = har MUTABLE STATE ka ek fixed owner thread |
| Single-writer ka matlab koi doosra thread padh nahi sakta | Readers (multiple) allowed hain — sirf WRITE exclusive hai |
| SPSC queue "2 writer + 1 reader" jaisa flexible hai | Bilkul nahi — 1 writer, 1 reader, strictly, ya UB |

---

## Exercises

1. `DisruptorRing` mein 2 consumer threads hain (0 aur 1). Kya consumer 0
   kabhi `consumer_seqs_[1]` ko likh sakta hai (galti se bhi)?
   <details><summary>Answer</summary>
   Nahi likhna chahiye -- har consumer sirf APNA index likhta
   (`consumed_up_to(idx, seq)` khud apne `idx` ke saath call karta). Agar
   consumer 0 galti se `consumer_seqs_[1]` likh de, single-writer
   principle toot jaata (ab do "writers" — consumer 1 khud, aur consumer
   0 galti se — us EK variable ke liye), race condition ban jaati.
   </details>

---

## Interview questions

1. Single-writer principle ki exact definition batao.
2. Yeh principle locks ki zaroorat kyun khatam karta?
3. Ek example do jahan "do writers dikhte hain" par actually har variable
   ka apna ek writer hai (SPSC queue jaisa).

---

## Next
→ [`03-shared-nothing-design.md`](03-shared-nothing-design.md)

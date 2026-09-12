# 09 — Deterministic execution: same input, same output

## Prerequisites
- `08-self-trade-prevention.md`

## Yeh topic abhi kyun

**Yeh matching engine ka sabse important non-functional property hai** —
zyaada important speed se bhi (01 mein already bola). Yeh lesson EXACTLY
define karta "determinism" ka matlab kya hai, isse kyun chahiye, aur
engine ke design mein isse kaise achieve kiya gaya.

---

## Definition

**Deterministic execution**: agar tum SAME sequence of commands (Submit/
Cancel, exact SAME order mein) do alag baar chalao — kisi bhi machine pe,
kisi bhi din — tumhe **exactly** SAME sequence of Trade events, SAME
final book state, SAME order statuses milne chahiye. Har baar. Bit-for-bit.

```
Commands: [Submit(A), Submit(B), Cancel(C), Submit(D), ...]
       |                                          |
       v                                          v
  Run #1 (Machine X, Monday)              Run #2 (Machine Y, Thursday)
  Trades: [T1, T2, T3, ...]               Trades: [T1, T2, T3, ...]  <- SAME
  Final book: {...}                       Final book: {...}          <- SAME
```

---

## Kyun zaroori hai

| Use case | Determinism ke bina kya toot jaata |
|---|---|
| **Replay/audit** (10) | "Kal 2:35 PM pe kya hua tha" reconstruct nahi ho sakta agar replay se ALAG result aaye |
| **Disaster recovery** | Backup engine (event log se rebuild) primary se DIFFERENT state pe pahunch sakta |
| **Testing** | Ek test jo aaj pass hui, kal fail ho sakti (flaky) -- 14/15 ka poora foundation hilta |
| **Regulatory** (37/16) | Exchanges ko apna matching PROVE karna padta -- non-deterministic engine "prove" nahi kar sakta |
| **Multi-node consistency** | Agar 2 nodes SAME log replay karke DIFFERENT state pe pahunchein, distributed system corrupt |

`05_event_sourcing.cpp` isi property ko literally **proof** karta hai
(claim nahi) — SAME 20000-command log do independent fresh engines mein
chalate, HAR trade field-by-field compare karte.

---

## Non-determinism kahan se aati hai (aur engine unhe kaise avoid karta)

| Source | Kaise avoid kiya |
|---|---|
| **Wall-clock timestamps** matching decisions mein use karna | `seq` (monotonic counter) use hota, `std::chrono::system_clock` NAHI |
| **Multi-threading races** (do threads ek book mutate karein) | Single-threaded core (11 mein poora reasoning) |
| **`unordered_map`/`unordered_set` ITERATION order** pe depend karna | `index_` (unordered_map) sirf O(1) LOOKUP ke liye use hota, kabhi ITERATE nahi hota matching decisions ke liye -- matching hamesha `std::map` (ordered) aur `std::list` (insertion order) pe hoti |
| **Floating point** (rounding, platform-dependent) | `Price`/`Qty` sab INTEGER types (37/08 se recall) |
| **Random numbers** matching logic mein | Koi randomness matching mein NAHI hai (workload GENERATORS mein randomness hai, par woh sirf TEST INPUT banane ke liye, engine ke andar nahi) |
| **Uninitialized memory read** (UB) | Har struct field explicit-initialized (`Order`'s default member initializers) |

**Sabse subtle wala:** `index_` (unordered_map) ka use sirf `find()`/
`count()`/`emplace()`/`erase()` (O(1) point-lookup) tak limited hai —
kabhi bhi `for (const auto& kv : index_)` jaisa iteration matching-relevant
decision ke liye nahi hota (unordered_map ka iteration order platform/
hash-seed/insertion-history pe depend kar sakta, non-deterministic hone
ka risk). Saari ORDER-SENSITIVE traversal `bids_`/`asks_` (std::map,
price-ordered) aur `PriceLevel::orders` (std::list, insertion/FIFO-ordered)
pe hoti — DONO deterministic containers hain.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — "single-threaded hai to determinism free milega"
Single-threaded zaroori condition hai, PAAKAAFI NAHI hai — agar tum
`unordered_map` iterate karke koi decision lo (jaisa "sabse pehla resting
order jo mile"), single-threaded hote hue bhi tumhara result run-to-run
badal sakta (hash-seed randomization kai standard libraries mein hoti,
security ke liye).

### Trap 2 — determinism ko sirf "trades match kiye" tak limit karna
Sirf trades check karna kaafi nahi — FINAL BOOK STATE bhi match hona
chahiye (resting_count, best_bid/ask, saare individual resting orders ki
qty). `05_event_sourcing.cpp` dono check karta.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Determinism sirf "crash na ho" ka matlab hai | Bit-for-bit SAME output, har baar, guaranteed |
| Single-threaded = automatically deterministic | Zaroori condition hai, sufficient nahi (unordered iteration jaisi cheezein bhi avoid karni padti) |
| Determinism sirf testing ke liye useful hai | Replay, disaster-recovery, regulatory, multi-node consistency -- sab isi pe depend karte |

---

## Exercises

1. Ek matching engine `std::unordered_map<Price, Level>` use karta bids
   ke liye (`std::map` ki jagah), aur "best price" nikalne ke liye poora
   map ITERATE karke max dhoondta. Kya yeh deterministic rahega?
   <details><summary>Answer</summary>
   NAHI reliably -- agar do prices ka "max" TIE ho sakta kisi tie-break
   logic ki wajah se jo iteration-order pe depend kare, ya agar
   iteration-order khud hash-seed-dependent ho (jo `unordered_map` mein
   common hai), result run-to-run badal sakta. `std::map` (ordered, price
   se sorted) is risk ko structurally avoid karta.
   </details>

---

## Interview questions

1. Determinism ki exact definition batao (2 runs, same input).
2. Non-determinism ke 5-6 common sources batao, matching engine context mein.
3. `index_` (unordered_map) safe kyun hai yahan, jabki determinism ka
   itna strict requirement hai?

---

## Next
→ [`10-event-sourcing.md`](10-event-sourcing.md)

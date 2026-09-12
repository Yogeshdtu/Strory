# 15 — Fuzzing for invariant violations

## Prerequisites
- `14-testing-matching-engine.md`

## Yeh topic abhi kyun

14 ne fuzzing ka ROLE bataya. Yeh lesson `07_engine_fuzz.cpp` ka EXACT
design deep-dive karta — `RefEngine` kaise banaya gaya, kya invariants
check hote, aur kya mila.

---

## `RefEngine` -- deliberately DUMB, deliberately DIFFERENT

`MatchingEngine` `std::map` + `std::list` + `std::unordered_map` index use
karta (fast lookups, ordered levels). `RefEngine` **plain
`std::vector<Order>`** use karta, koi index NAHI — har operation (best-
price, cancel-by-id, sab) **linear scan** karta:

```cpp
struct RefEngine {
    std::vector<Order> resting;   // BAS itna, koi map/list/hash

    int best_opposite_index(bool incoming_is_buy) const {
        int best_idx = -1;
        for (size_t i = 0; i < resting.size(); ++i) {
            ... // O(n) scan, best-price + FIFO(seq) tie-break
        }
        return best_idx;
    }
    ...
};
```

**Yeh JAAN-BOOJH KAR slow hai** — O(n) per operation. Fuzz-test scale
(30000 ops, resting count kabhi kuch hazaar tak) pe yeh kaafi FAST hai
(seconds mein chalta), aur iski SIMPLICITY hi uski VALUE hai: kam code,
kam jagah bug chhupne ki. Yeh EXACT SAME philosophy hai jo 39's `RefBook`
mein thi — "correctness ke liye optimize mat karo, SIMPLICITY ke liye
optimize karo."

---

## FOK precheck -- ek COMPLETELY ALAG strategy (dry-run)

`MatchingEngine` ka FOK precheck ek **closed-form formula** hai (06's
`available_qty()` — STP-mode-aware sum, bina actually match kiye).
`RefEngine` ka FOK precheck **completely different approach** leta:

```cpp
if (incoming.type == OrderType::FOK) {
    RefEngine dry = *this;          // POORA state COPY karo
    Order tmp = incoming;
    bool dummy = false;
    dry.match(tmp, dummy);          // "PRACTICE" match chalao (copy pe)
    const Qty filled = incoming.orig_qty - tmp.qty;
    if (filled < incoming.orig_qty) return {Rejected, {}};
    // *this abhi tak UNTOUCHED hai (sirf `dry` mutate hua, discard ho gaya)
}
```

**Yeh methodologically IMPORTANT hai**: agar `MatchingEngine`'s formula
(06) mein koi subtle bug hota, `RefEngine`'s dry-run approach (jo
completely alag code-path hai, formula-se-DERIVE nahi hui) us bug ko
REPEAT nahi karta — disagreement fuzzer mein pakda jaata. Do implementations
jo SAME bug share karein sirf tab hoga jab dono ka LOGIC identical ho —
aur yahan woh JAAN-BOOJH KAR NAHI hai.

---

## Invariants checked

```cpp
// 1. Cross-implementation equivalence -- HAR command ke baad
const bool ok = (r_real.status == r_ref.status) && trades_equal(r_real.trades, r_ref.trades);

// 2. FOK all-or-nothing -- kabhi partial nahi
(status==Filled && filled==orig_qty) || (status==Rejected && trades.empty())

// 3. STP -- kabhi self-trade leak nahi
if (stp != None) assert(no trade with aggressor_participant == resting_participant)

// 4. Periodic full-state check (har 500 commands) -- resting_count,
//    has_bid/ask, best_bid/ask dono engines mein match
```

---

## Command generator -- `engine_workload.hpp`

Deterministic (splitmix64, 38/39 ka SAME idiom) — SAME seed = SAME
commands, hamesha. 82% Submit (mix: 55% Limit, 15% Market, 15% IOC, 15%
FOK), 18% Cancel. STP mode per-order random assign (55% None, 15%
CancelNewest, 15% CancelOldest, 15% CancelBoth) — **chhota participant
pool** (default 6) taaki STP collisions REALISTICALLY often hon.

> **HFT relevance:** chhota participant-pool ek DELIBERATE fuzzing
> technique hai — agar pool bada hota (jaisa 1000 participants), STP
> collisions itni RARE hoti ki fuzzer kabhi meaningfully test hi nahi
> karta us path ko. Yeh general fuzzing-design principle hai: **jis path
> ko test karna hai, use fuzzer ke liye REALISTICALLY REACHABLE banao.**

---

## Result (yeh run, N=30000, seed=2024)

```
agree: 30000   disagree: 0
order-type mix hit: FOK=3774 IOC=3757 Market=3683
self-trade leaks despite STP requested: 0 (expect 0)
final resting_count: real=7780 ref=7780
```

**Zero disagreements** across 30000 commands, **zero FOK violations**
across 3774 FOK orders (across saare STP modes mixed in), **zero self-
trade leaks**. Yeh 06's FOK+STP fix ki EMPIRICAL validation hai —
sirf ek HAND-CRAFTED scripted test (14) nahi, balki **automatic, broad,
random coverage**.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — RefEngine ko bhi OPTIMIZE karne ki koshish
Agar tum `RefEngine` ko FAST banane ki koshish karo (index add karo,
etc.), tum uski VALUE khatam kar dete — ab woh `MatchingEngine` jaisa
COMPLEX ho jaata, aur SAME bugs share karne ka risk badh jaata. `RefEngine`
DELIBERATELY dumb rehna chahiye.

### Trap 2 — periodic state-check ko HAR command pe na karna, aur kabhi
NA karna dono galat
`07_engine_fuzz.cpp` HAR 500 commands pe full state check karta (har
command pe nahi — O(n) scan hai, N baar karna slow hota; kabhi nahi
karna — kuch bugs sirf STATE mein manifest hote, trade-output mein
nahi). Balance zaroori hai.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| RefEngine bhi "sahi hona chahiye, jaise MatchingEngine" | RefEngine sirf SIMPLE hona chahiye, obviously-correct-by-inspection |
| RefEngine ka FOK precheck bhi 06's formula use karna chahiye | Jaan-boojh kar ALAG strategy (dry-run) — independent verification ke liye |
| Random seed alag-alag rakhni chahiye har run mein | Deterministic seed (fixed) — reproducible failures, debugging ke liye zaroori |

---

## Hands-on

```bash
./build.ps1 fast 40-MATCHING-ENGINE/examples/07_engine_fuzz.cpp
```

---

## Exercises

1. Agar `RefEngine` bhi `MatchingEngine`'s EXACT FOK-precheck formula
   copy-paste kar leta (dry-run ki jagah), kya fuzzing ki VALUE kam hoti
   is specific bug-class ke liye? Kyun?
   <details><summary>Answer</summary>
   Haan -- agar formula mein koi bug hota, RefEngine BHI wahi bug
   REPEAT karta (kyunki SAME formula copy ki), aur dono engines
   "agree" karte GALAT answer pe -- disagreement KABHI nahi pakda jaata.
   Independent verification ka poora POINT yeh hai ki dono implementations
   ka LOGIC genuinely alag ho.
   </details>

---

## Interview questions

1. `RefEngine` `MatchingEngine` se KYUN aur KAISE deliberately different
   hai?
2. FOK invariant kya hai, aur fuzzer usse kaise check karta?
3. Chhota participant-pool STP-testing ke liye kyun zaroori hai?

---

## Next
→ [`16-benchmarking.md`](16-benchmarking.md)

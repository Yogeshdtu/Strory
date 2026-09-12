# 13 — BUILD: full matching engine, step by step

## Prerequisites
- `12-state-machine-design.md` (poore folder ke saare concepts is point
  tak cover ho chuke — yeh lesson unhe EK build-narrative mein jodta)

## Yeh topic abhi kyun

02-12 ne engine ke individual PIECES cover kiye (matching loop, order
types, STP, determinism, state machine). Yeh lesson `matching_engine.hpp`
ko **ek saath, incremental build-order mein** dikhata — jaisa tum khud
isse SCRATCH se banate.

---

## Step 1 — Data layout decide karo

Resting-order storage `39-ORDER-BOOK`'s V1 jaisa: **simple, correct,
already-proven pattern**, kyunki iss folder ka focus storage-optimization
NAHI hai (woh 39 mein already ho chuka):

```cpp
struct PriceLevel { std::list<Order> orders; Qty total_qty = 0; };
struct Location    { bool is_buy; Price price; std::list<Order>::iterator it; };

std::map<Price, PriceLevel, std::greater<Price>> bids_;   // best = begin()
std::map<Price, PriceLevel>                      asks_;   // best = begin()
std::unordered_map<OrderId, Location>            index_;  // O(1) cancel/reduce by id
```

`std::greater<Price>` bids_ ke liye, default (`std::less`) asks_ ke liye
— dono ki `begin()` "best price" deti (03's map-type trap yahin se aata).

## Step 2 — `Order`/`Trade`/enums decide karo

Yeh 03, 06, 07, 08 mein cover ho chuka: `OrderType` (4 values), `OrderStatus`
(5 values, 12 mein state machine), `StpMode` (4 values, 08), `Order`
struct (`qty`=remaining, `orig_qty`=original — 05), `Trade` struct
(07 — maker's price, shared `seq` timeline).

## Step 3 — `match_against()` -- core matching loop

02's do-nested-loop algorithm (outer: price levels, inner: FIFO orders
per level), crossing-check ka Market-special-case (04), fill-quantity
calculation (05), STP check INSIDE the inner loop, BEFORE fill computation
(08). Yeh single function poore engine ki "matching intelligence" hai.

## Step 4 — FOK precheck -- `available_qty()`

06's STP-aware precheck — `match_against()` ke SAME priority-order/
skip-abort-rules follow karta, bina mutate kiye. **Order matters**: yeh
function `match_against()` ke LOGIC se consistent rehna CHAHIYE (agar
dono kabhi diverge karein, FOK ka guarantee toot sakta — 06's central
bug story).

## Step 5 — `submit()` -- sab kuch jodta

```cpp
SubmitResult submit(Order incoming) {
    incoming.orig_qty = incoming.qty;
    incoming.seq = next_seq_++;                              // 09 -- deterministic sequencing

    if (index_.count(incoming.id)) return {Rejected, {}};     // duplicate id

    if (incoming.type == FOK) {                                // 06
        if (available_qty(...) < incoming.qty) return {Rejected, {}};
    }

    auto trades = match_against(incoming, opposite_side);      // 02-05, 08

    if (incoming.qty == 0)                    return {Filled, trades};        // 12
    if (type == Limit && !stp_aborted) { add_resting(incoming); return {...}; } // 03
    return {Cancelled, trades};                                // 04, 06, 08
}
```

Har line ek pichle lesson se traceable hai — yeh **integration** hai,
koi NAYI logic nahi.

## Step 6 — `cancel()`/`replace()` -- resting-order lifecycle

`cancel()` templated-dispatch pattern (03's map-type trap se bachne ke
liye) use karta. `replace()` = `cancel(old)` (ignore-if-gone) + `submit(new)`
— same-id-chain convention jaisa 38/39.

## Step 7 — Queries -- `has_bid`/`best_bid`/`find_resting`/`ids_at_price`

Simple accessors, `bids_.begin()`/`asks_.begin()` pe based (O(1), map
ki internal tree-structure ki wajah se — root se leftmost/rightmost node
tak already-cached path, practically O(1) amortized for repeated calls
on a balanced tree implementation, though the standard only guarantees
O(log n) for `begin()` on a fresh map -- typically implemented as an O(1)
cached leftmost-pointer in libstdc++). **Precondition**: `has_bid()`/
`has_ask()` check PEHLE (39's exact same precondition-trap, `best_bid()`
empty book pe UB hai).

---

## Poora file padhne ka guide

`matching_engine.hpp` top-se-bottom padhte waqt is order mein dhyaan do:
1. Enums + `Order`/`Trade`/`SubmitResult` (lines ~40-115) — data model
2. Factory helpers (`make_limit` etc.) — readability convenience
3. `MatchingEngine::submit()` — PEHLE yeh padho (high-level flow)
4. `match_against()` — phir yeh (core algorithm)
5. `available_qty()` — FOK precheck (agar FOK samajhna hai)
6. `cancel_impl`/`add_resting_impl` — mechanical helpers (last)

---

## ⚠️ Traps / Common mistakes

### Trap 1 — build-order ko implementation-order samajh lena
Yeh lesson "step 1, 2, 3..." dikhata SAMJHANE ke liye — REAL development
mein tumhe pehle TESTS likhni chahiye (14) har step ke baad, na ki poora
engine likh ke end mein test karna. Is folder ke development mein bhi
`06_engine_tests.cpp` likhte waqt hi asli bugs (STP dangling-reference,
FOK+STP precheck) pakde gaye — "likho phir test karo" ka opposite (test-
driven catching) zyaada effective raha.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Matching engine ek monolithic function hai | 7 clear pieces, har ek independently samajhne-layak |
| `submit()` "naya" logic hai | Pichli 12 lessons ka INTEGRATION hai |

---

## Hands-on

Poora `matching_engine.hpp` ek baar top-se-bottom padho, phir
`01_matching_engine.cpp` chalao aur trace karo har line kaunse step se
aa rahi hai.

```bash
./build.ps1 fast 40-MATCHING-ENGINE/examples/01_matching_engine.cpp
```

---

## Exercises

1. `submit()` ke andar FOK precheck, matching-se-PEHLE kyun hai (step
   order matters kyun)?
   <details><summary>Answer</summary>
   06 se recall: FOK ka "poora ya kuch nahi" guarantee sirf PRE-decide
   karke rakha ja sakta -- agar match SHURU ho jaaye aur beech mein
   qty kam pad jaaye, trades already committed hote, undo nahi ho sakte.
   </details>

---

## Interview questions

1. `matching_engine.hpp` ke 7 major pieces batao, har ek ka responsibility.
2. `submit()` ka flow trace karo -- duplicate-id se lekar final status tak.
3. Poore engine ko test-driven-development se banane ka fayda kya raha
   (real bugs pakadne mein)?

---

## Next
→ [`14-testing-matching-engine.md`](14-testing-matching-engine.md)

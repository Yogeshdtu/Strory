# 05 — Project 4: Matching engine (integrated as the venue)

## Prerequisites
- `04-project-order-book.md`
- `40-MATCHING-ENGINE` (poora — price-time matching, Limit/Market/IOC/FOK,
  STP, deterministic sequencing, fuzz-verified)

## Yeh topic abhi kyun

Ab tak sab hamare **apne** components the. Yeh project folder 40 ka
**tested + fuzzed** `MatchingEngine` (30000-command fuzz, 0 disagreements
vs an independent reference) **as-is** pipeline mein daalta — capstone ki
"jodo" spirit.

Engine ka kaam: "the venue" ban-na. Hamara order jaata, fills wapas aate.

## `ExecutionSimulator` (`mh_order_manager.hpp`)

```cpp
class ExecutionSimulator {
    MatchingEngine engine_;                       // folder 40, unchanged
public:
    void on_market_event(const MdMessage& m) {   // mirror the market's resting liquidity
        if (m.type == MdType::Add)
            engine_.submit(make_limit(m.order_id, kMarketParticipant, is_buy(m.side), m.px, m.qty));
        else if (m.type == MdType::Cancel)
            engine_.cancel(m.order_id);
    }
    std::vector<Fill> send(const OrderRequest& r, ClientOrderId cl, Qty& filled) {
        SubmitResult res = engine_.submit(to_engine_order(r, us_id_base_ + (++us_seq_)));
        // res.trades -> Fill{cl, side, trade.price, trade.qty, ts}
    }
};
```

- Market adds → resting `make_limit` orders with `participant =
  kMarketParticipant` (0).
- Our orders → aggressive orders with `participant = kUsParticipant` (1),
  distinct id space (`1e9 + counter`).
- Sim guarantees non-crossing market adds → engine never self-trades the
  market against itself. Only **our** orders cause trades.
- IOC semantics (40/06): fill at the resting (maker) price, best-first,
  remainder VOID. Non-marketable → 0 fills, status `Cancelled`.

## Measure (`04_matching_engine.cpp`)

```
aggressive IOC probes through the MatchingEngine venue:
  fills=~200  filled_qty=~2900  net_notional(scaled)=~60000
  (marketable buys filled at resting ask; far sells filled 0 -- IOC void)
determinism (same tape x2): IDENTICAL
```

Determinism: same market tape replayed twice, same probe orders → identical
fills. Folder 40 proved this deeply (event-sourcing byte-identical);
here it's an integration smoke test.

## The cost — and why project 11 replaces it

`MatchingEngine` uses 40/39-V1 storage: `std::map<Price, PriceLevel>` +
`std::list` per level + `unordered_map` index. Every market Add = a tree
insert + a list-node `malloc`. Every Cancel = tree lookups + list scan +
frees.

`11_mini_hft_engine.cpp` ka profile:
```
naive     book  ~150 ns/msg   <- ExecutionSimulator::on_market_event dominates
optimized book   ~67 ns/msg   <- FastVenue instead
```

So project 11's optimization = **replace this venue with `FastVenue`**
(flat-array aggregate book + per-level FIFO sweep), which produces the
**same fills** (12_integration_tests proves it byte-identical across 5
configs) at ~2× the venue-stage speed. See `12-project-mini-hft-engine.md`.

**Why keep the `MatchingEngine` version at all?** It's the "correct/simple"
reference (spec step 1). `FastVenue` is validated *against* it. And for a
real venue simulator where per-order fill reports, STP, FOK, and price
improvement matter, you want the full engine — `FastVenue` is an
aggregate-fill approximation that happens to be exact for *our* order flow
(small IOCs against deep aggregate liquidity).

## HFT relevance

- **You are not the exchange.** But you run a venue simulator for
  backtesting/what-if — and it must match the real venue's matching rules
  (price-time, tick size, self-match prevention) or your backtest lies.
- **The exchange's matching engine** is the thing you're racing. Folder 40
  is what one looks like inside.

## ⚠️ Traps

### Trap 1 — mirroring a crossing feed into the engine
If market adds could cross, the engine would *match the market against
itself* and generate phantom trades. Project 1's non-crossing guarantee is
load-bearing here.

### Trap 2 — id space collision
Market ids (1..maxid) and our ids must not overlap, or `engine_.submit`
sees a "duplicate id" and rejects. `us_id_base_ = 1e9`.

### Trap 3 — fill count vs trade count
`SubmitResult.trades` = one per resting order matched. If our order sweeps
a level with 3 resting orders, that's 3 trades. `FastVenue` coalesces to
1 fill per level. Total qty + prices identical; **fill *count* may differ**
— compare qty/pnl, not count.

### Trap 4 — replaying cancels for consumed orders
After our order consumes market liquidity, a later `Cancel` for a
now-filled resting order: `MatchingEngine::cancel` returns false (harmless).
`FastVenue` needs a per-order `live` flag so it doesn't over-subtract —
which it has.

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Capstone = rewrite the matching engine | Reuse folder 40's — it's tested + fuzzed |
| Aggregate-fill venue == full matching engine | Approximation; exact only for simple order flow — validate |
| One trade per order fill | One trade per *resting order matched*; aggregate venue coalesces |

## Exercises

1. `04_matching_engine.cpp` ka probe order `OrderType::IOC` se
   `OrderType::Limit` kar do. Kya farak?
   <details><summary>Answer</summary>
   Limit ka remainder **rest** karega (VOID nahi). Marketable part fills
   same, par ab hamara leftover order engine ki book mein baith jaayega
   as a `kUsParticipant` resting order — aur agle market adds usse cross
   kar sakte (we become passive liquidity). IOC us complication ko avoid
   karta — isliye strategy IOC use karti.
   </details>

2. `us_id_base_` ko `0` kar do. Kya hota?
   <details><summary>Answer</summary>
   Hamara pehla order id = 1, jo pehle hi ek market order ka id hai →
   `MatchingEngine::submit` "duplicate id" → `Rejected`, 0 fills. Har
   order reject. Id spaces alag rakho.
   </details>

3. Sim ko crossing allow karwao. `04_matching_engine.cpp` ke fills pe
   kya asar?
   <details><summary>Answer</summary>
   Engine market adds ko ek doosre se match karega → phantom trades
   (`aggressor_participant == resting_participant == kMarketParticipant`),
   book depleted galat tarah, hamare probe fills unpredictable. Non-crossing
   feed is a hard precondition for the mirror.
   </details>

## Interview questions

1. Venue simulator ko real exchange se match karana kyun zaroori (backtest
   validity)?
2. Market data ko ek matching engine mein mirror karne ke liye kaunse
   preconditions?
3. Aggregate-fill venue kab full matching engine ke barabar, kab nahi?
4. IOC vs Limit — strategy IOC kyun use karti hai (state management)?

## Next
→ [`06-project-memory-pool.md`](06-project-memory-pool.md)

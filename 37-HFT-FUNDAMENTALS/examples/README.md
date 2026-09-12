# Examples — Folder 37 (HFT fundamentals)

> Portable `.cpp` — sab MinGW/Windows x86-64 pe chalte, koi external
> dependency nahi. **Yeh CONCEPT-level demos hain, benchmarks nahi** — koi
> `-O2` requirement nahi, koi latency measurement nahi. Production-grade,
> measured versions **folder 39 (order book)**, **40 (matching engine)**
> mein banenge. `./build.ps1 folder 37-HFT-FUNDAMENTALS` → **4/4 OK** under
> strict flags.

## Compile / run

```bash
./build.ps1 fast 37-HFT-FUNDAMENTALS/examples/01_orderbook_concept.cpp
./build.ps1 fast 37-HFT-FUNDAMENTALS/examples/02_spread_calculator.cpp
./build.ps1 fast 37-HFT-FUNDAMENTALS/examples/03_latency_budget.cpp
./build.ps1 fast 37-HFT-FUNDAMENTALS/examples/04_matching_rules.cpp
```

## Examples

| File | Lesson(s) | Kya dikhata |
|---|---|---|
| `01_orderbook_concept.cpp` | 06 | Simplest order book model — `std::map<Price, Qty>` do taraf (bids `std::greater`, asks default ascending). `Price` **integer ticks** hai, `double` nahi (03-VARIABLES ka floating-point-equality trap avoid karta). Add/reduce operations, best bid/ask/spread/mid, top-of-book change ke baad "fall back" dikhata jab best level poora cancel hota. |
| `02_spread_calculator.cpp` | 05 | 5 L1 quotes pe spread (absolute + bps), mid, **microprice** (size-weighted mid), **imbalance** compute karta. BAL1/BUY-P/SELL-P same `bid_px`/`ask_px`/mid rakhte, sirf qty ratio badalte — dikhata ki mid size-blind hai par microprice/imbalance nahi. TIGHT vs WIDE dikhata ki bps normalization kyun zaroori hai (same-ish absolute spread, bahut alag relative tightness). |
| `03_latency_budget.cpp` | 14 | Tick-to-trade budget calculator tool — 7 illustrative pipeline stages (NIC RX se NIC TX tak), har ek ka budget/p50/p99.9. Automatically sabse bada tail-budget overrun dhoondta (Amdahl-first-fix logic). **Illustrative numbers hain, kisi real firm ke production numbers nahi** — tool method sikhaata, specific numbers nahi. |
| `04_matching_rules.cpp` | 07 | Same resting book (4 orders, arrival order mein), same incoming order — **FIFO** (price-time) vs **pro-rata** matching side-by-side. FIFO: pehle-aaye orders poora fill, baad wale ko kuch nahi milta. Pro-rata: sab size-proportional hissa paate, arrival time irrelevant. |

## Key numbers (is run se — illustrative/deterministic, hardware-independent kyunki koi timing nahi)

```
01: best bid=10048 best ask=10050 spread=2 ticks mid=10049
    naya bid 10049 -> best bid badalta; cancel hone pe wapas 10048 pe fall back

02: BAL1  mid=100.05 spread=10.0bps micro=100.050 imb= 0.00  (balanced)
    BUY-P mid=100.05 spread=10.0bps micro=100.091 imb=+0.82  (bid-heavy -> micro upar)
    WIDE  mid= 50.50 spread=198.0bps                          (chhota price, bada bps)

03: p50 total=940ns (under 1000ns target), p99.9 total=2030ns (OVER)
    sabse bada tail-overrun: "strategy decision" (+700ns) <- yahi fix karo pehle

04: incoming sell qty=180 @ level(100,50,200,75 across 4 orders, total 425)
    FIFO:      101->100, 102->50, 103->30, 104->0
    PRO-RATA:  101->44, 102->21, 103->84, 104->31
```

Yeh numbers **deterministic** hain (koi randomness, koi timing measurement)
— har run pe bilkul same output aayega, kisi bhi machine pe.

# 12 — HFT / trading glossary

Quick lookup. Depth: folder `37-HFT-FUNDAMENTALS` (file numbers in parens),
`38`–`44`. Grouped by topic, not alphabetical.

---

## Order book & prices

| Term | Meaning |
|---|---|
| **Bid / Ask (Offer)** | Best price a buyer will pay / a seller will accept |
| **Spread** | Ask − Bid. Tighter = more liquid. (`37/05`) |
| **Mid** | (Bid + Ask) / 2 |
| **BBO** | Best Bid and Offer — top of book |
| **NBBO** | National Best Bid/Offer — best across all US venues (Reg NMS) |
| **Level 1 / L1** | Top of book only (best bid/ask + sizes) |
| **Level 2 / L2** | Aggregated depth per price level |
| **Level 3 / L3 (MBO)** | Market-by-order — every individual order, full detail |
| **Depth / size** | Quantity resting at a price level |
| **Tick size** | Minimum price increment (`37/08`) |
| **Lot / round lot** | Standard trade unit; **odd lot** = smaller |
| **Price band / limit** | Exchange-imposed max deviation from a reference; rejects outside |
| **Crossed book** | Bid ≥ Ask (invalid — an engine must never produce one) |
| **Locked book** | Bid == Ask |

---

## Order types (`37/04`)

| Term | Meaning |
|---|---|
| **Market order** | Execute now at whatever price; takes liquidity |
| **Limit order** | Execute only at a price ≥/≤ limit; may rest |
| **Marketable limit** | A limit order priced to cross immediately |
| **IOC** (Immediate-or-Cancel) | Fill what you can now, cancel the rest |
| **FOK** (Fill-or-Kill) | Fill the *entire* qty now or cancel all — all-or-nothing |
| **GTC / GTD / DAY** | Good-till-cancel / -date / end of session |
| **Stop / stop-limit** | Activates when a trigger price trades |
| **Iceberg / reserve** | Shows a small "display" qty, hides the rest |
| **Post-only** | Reject/reprice if it would take liquidity (guarantees the maker rebate) |
| **Pegged** | Price tracks a reference (mid/bid/ask) automatically |
| **Hidden** | Not shown in the book; lower queue priority |

---

## Participants & economics (`37/09`)

| Term | Meaning |
|---|---|
| **Maker** | Posts a resting order (adds liquidity); often earns a **rebate** |
| **Taker** | Hits a resting order (removes liquidity); pays a **fee** |
| **Maker-taker** | Fee model: rebate makers, charge takers. **Taker-maker (inverted)** = opposite |
| **Market maker (MM)** | Continuously quotes two-sided; profits the spread, manages inventory |
| **Adverse selection** | Your resting quote gets filled *because* the market is about to move against you (`37/03`) |
| **Toxic flow** | Order flow that is informed / systematically adverse |
| **Inventory / position** | Net long/short; MMs skew quotes to mean-revert it |
| **PnL** | Profit and loss. **Realized** (closed) vs **unrealized** (mark-to-market) |
| **Mark-to-market** | Valuing an open position at current mid |

---

## Microstructure & strategy (`37/03`, `37/10`)

| Term | Meaning |
|---|---|
| **Price-time priority** | Match by best price, then earliest arrival (FIFO) (`37/07`) |
| **Pro-rata** | Allocate a fill across resting orders proportional to size |
| **Queue position** | Where your order sits in a level's FIFO — determines fill probability |
| **Liquidity provision** | Passive market making |
| **Latency arbitrage** | Exploiting stale quotes on a slow venue vs a fast one |
| **Statistical arbitrage** | Mean-reversion / relative-value on correlated instruments |
| **Order anticipation** | Detecting a large order and trading ahead of its impact |
| **Momentum ignition** | (Manipulative) sparking a move to trigger others — illegal |
| **Spoofing / layering** | (Illegal) placing orders you intend to cancel to mislead |
| **Quote stuffing** | Flooding with orders to slow competitors |
| **Fill ratio / hit ratio** | Fraction of quotes that get executed |
| **Slippage** | Difference between expected and executed price |
| **Market impact** | How much your own trading moves the price |
| **VWAP / TWAP** | Volume- / time-weighted average price (benchmarks & execution algos) |
| **Implementation shortfall** | Total cost vs the decision-time price |

---

## Infrastructure (`37/11`, `37/12`, `37/14`, folders 42–44)

| Term | Meaning |
|---|---|
| **Colocation (colo)** | Your servers in the exchange's data center |
| **Proximity hosting** | Near, but not in, the exchange DC |
| **Cross-connect** | Direct cable between your rack and the exchange's |
| **Tick-to-trade (T2T)** | Time from a market data event arriving to your order leaving the NIC |
| **Wire-to-wire** | Packet in on the NIC → packet out on the NIC |
| **Feed handler** | Parses the exchange market-data feed into internal events |
| **Gateway / OE** | Order-entry path to the exchange (usually TCP or a binary protocol) |
| **OMS** | Order Management System — tracks live orders, state machine, acks/fills |
| **EMS** | Execution Management System — routing, algos, venue selection |
| **SOR** | Smart Order Router — splits an order across venues |
| **Matching engine** | The exchange component that pairs buys and sells |
| **Sequenced feed** | Market data with monotonic sequence numbers (gap detection) |
| **A/B lines** | Redundant multicast feeds; arbitrate, dedupe, gap-recover |
| **Snapshot + incremental** | Full book image + a stream of deltas to keep it current |
| **Conflation** | Dropping intermediate updates, sending only the latest (slow consumers) |
| **Kill switch** | Latched hard stop on all trading (vs a rate limiter, which throttles) |
| **Pre-trade risk** | O(1) checks before an order goes out (qty, position, notional, price band, rate) (`37/13`) |
| **Drop copy** | A read-only feed of your own fills for reconciliation/compliance |

---

## Protocols & data

| Term | Meaning |
|---|---|
| **FIX** | Fine-grained text tag=value order protocol; slow, ubiquitous |
| **ITCH / OUCH** | Nasdaq's binary market-data (ITCH) / order-entry (OUCH) protocols |
| **SBE / SBE-style** | Simple Binary Encoding — fixed-layout binary messages, alloc-free parse |
| **Multicast** | One-to-many UDP — how market data is disseminated |
| **PCAP** | Captured packet trace (for replay / analysis) |
| **Reference data** | Static instrument info (symbol, tick size, lot size, expiry) |
| **Corporate action** | Split/dividend/merger — adjusts reference data & historical prices |

---

## Regulation / venues (`37/15`, `37/16`)

| Term | Meaning |
|---|---|
| **Reg NMS** | US equities rulebook — order protection, NBBO, sub-penny rule |
| **MiFID II** | EU markets regulation — reporting, algo controls, clock sync |
| **MAR** | Market Abuse Regulation (EU) — spoofing/manipulation |
| **Circuit breaker / LULD** | Trading halt / limit-up-limit-down bands on big moves |
| **Auction (open/close)** | Batch price-discovery at session boundaries |
| **Dark pool / ATS** | Venue with no pre-trade transparency |
| **Lit venue** | Displayed order book |
| **SEBI** | India's securities regulator; **NSE/BSE** the exchanges (`37/15`) |
| **Co-location fair access** | Regulator-mandated equal-latency colo (India) |

## Next
→ [`13-interview-revision.md`](13-interview-revision.md)

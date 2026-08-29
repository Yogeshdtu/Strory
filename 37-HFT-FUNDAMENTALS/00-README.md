# 37 — HFT FUNDAMENTALS (PHASE 25)

## Prerequisites
`36-LOW-LATENCY-CPP`

## Yeh folder kyun
Ab domain knowledge. Code se pehle **samjho ki business kya hai** — warna aap
sirf code copy karoge, design nahi kar paoge.

## Is folder ki files

| # | File | Kya seekhoge |
|---|------|--------------|
| 01 | `01-what-is-hft.md` | HFT kya hai, kya nahi hai, myths vs reality |
| 02 | `02-how-exchanges-work.md` | Exchange architecture, matching engine, gateways, market data feeds |
| 03 | `03-market-microstructure.md` | Liquidity, price discovery, order flow, adverse selection |
| 04 | `04-order-types.md` | Market, limit, IOC, FOK, stop, iceberg, post-only |
| 05 | `05-bid-ask-spread.md` | Bid, ask, spread, mid, depth, imbalance |
| 06 | `06-order-book-concept.md` | Order book kya hai (concept level), levels, depth |
| 07 | `07-price-time-priority.md` | **Matching rules** — price first, then time; pro-rata variants |
| 08 | `08-tick-size-and-lots.md` | Tick size, lot size, minimum quantity, price bands |
| 09 | `09-market-makers-and-takers.md` | Maker vs taker, fee structures, rebates |
| 10 | `10-hft-strategies-overview.md` | Market making, arbitrage, latency arb — **concepts only, no alpha** |
| 11 | `11-colocation.md` | Co-location, proximity hosting, cross-connects, fair access |
| 12 | `12-hft-system-architecture.md` | **Poora system diagram** — feed handler → book → strategy → risk → OMS → gateway |
| 13 | `13-risk-systems.md` | Pre-trade risk, position limits, kill switches, fat-finger checks |
| 14 | `14-latency-budget.md` | **Tick-to-trade budget** — har component ko kitna time milta hai |
| 15 | `15-indian-vs-us-markets.md` | NSE/BSE vs Nasdaq/NYSE — structure, protocols, regulations |
| 16 | `16-regulatory-basics.md` | SEBI, algo approval, audit trails — overview level |
| 17 | `17-exercises.md` | Concept questions + system design |

## Examples

| File | Kya |
|---|---|
| `examples/01_orderbook_concept.cpp` | Order book ka simple conceptual model |
| `examples/02_spread_calculator.cpp` | Bid/ask/spread/mid calculations |
| `examples/03_latency_budget.cpp` | Budget calculator tool |
| `examples/04_matching_rules.cpp` | Price-time priority demonstration |

## Time
2 hafte

## Status
⏳ **Yeh folder abhi syllabus stage pe hai.** Upar ki file list poori plan hai —
content agle batch mein aayega.

## Next
→ [`../38-MARKET-DATA/00-README.md`](../38-MARKET-DATA/00-README.md)

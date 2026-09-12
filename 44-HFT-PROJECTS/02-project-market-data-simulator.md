# 02 — Project 1: Market Data Simulator

## Prerequisites
- `01-project-overview.md`
- `38-MARKET-DATA` (poora — messages, sequence numbers, timestamps, L1/L2/L3)

## Yeh topic abhi kyun

Pipeline ka source. Baaki sab isi ke output pe chalta, isliye yeh pehle,
aur ise **deterministic + coherent** hona zaroori — warna niche ke stages
ka koi bhi test reproducible nahi rahega.

## Requirements

| # | Requirement | Kyun |
|---|---|---|
| 1 | L3 incremental events: Add / Cancel / (Trade) | real feeds aise hi aate (38/02) |
| 2 | Monotonic sequence number, strictly +1 | gap detection (38/04); niche `gap_count` isi se |
| 3 | Monotonic exchange timestamp | latency measurement + deterministic risk windows |
| 4 | **Non-crossing book** (max bid < min ask, always) | warna "market" garbage; venue/strategy ke paas sane BBO hona chahiye |
| 5 | Deterministic (seed → exact same stream) | replay = byte-identical (project 13) |
| 6 | Binary wire encode (packed, big-endian) | parser project (project 2) ko kuch parse karne ko chahiye |

## Design (`mh_market_data.hpp`)

### The price process
`mid_ticks` ek random walk hai + ek periodically-resampled momentum term
(`trend` ∈ −3..+3 ticks/msg, har ~120–400 msgs pe resample) + ek soft
mean-revert pull (`(mid − 10000) / 200`). 43's generator jaisa. Isse mid
kabhi-kabhi trend karta (signal fire hota) par bounded rehta.

### Keeping the book non-crossing
Sabse important design decision. "Har add non-crossing at insertion" **kaafi
nahi** — jaise mid move karta, purane orders stale ho jaate aur book cross
kar jaata (43's pipeline mein yeh latent bug tha jo surface nahi hua kyunki
woh sirf `best_bid + best_ask` ka SUM use karta tha).

Fix — do cheezein:
1. **Stale-quote pulling**: har step pe, agar koi live order touch se
   `> kMaxDist` (12) ticks door hai, use cancel karo. Real market makers
   yahi karte.
2. **Clamp new orders against the sim's OWN best on the far side**: naya
   bid `< lo_ask − 1` pe clamp; naya ask `> hi_bid + 1` pe. Sim apna
   `hi_bid`/`lo_ask` per-level count arrays (`bcnt_`/`acnt_`) se O(1)
   maintain karta.

Result (`01_market_data_sim.cpp`):
```
book never crossed : yes  (bids always < asks)
final BBO          : ... / ...  (spread ~1 tick)
```

### The wire format
38 bytes, big-endian packed:
```
0  type u8 | 1 side u8 | 2 qty u32 | 6 seq u64 | 14 ts u64 | 22 order_id u64 | 30 px i64
```
`encode(m, out)` writes it. Project 2 parses it. BE (network order) is the
realistic choice (38/10 endianness).

### The internal event queue
`next()` ek `pending_` queue se pull karta. `step()` queue ko refill karta:
mid advance → pull-a-far-order OR random-cancel OR non-crossing-add. Ek
`step()` ek event enqueue karta. Yeh design tab kaam aata jab ek step ko
multiple events emit karne ho (e.g. "mid jumped, cancel 3 crossing orders").

## Measure (`01_market_data_sim.cpp`)

```
messages produced : 100000
  adds=~50000  cancels=~50000  (add:cancel ~ 1.00)
  sequence strictly +1 : yes
  timestamps monotonic : yes
  book never crossed   : yes
determinism : 100000 messages compared, 0 mismatches -> IDENTICAL
```

Determinism check: **do independent `MarketDataSimulator(44)` objects**,
field-by-field compare every message. 0 mismatches. Yeh replay tests ka
foundation hai.

## HFT relevance

Production mein tum sim nahi, ek **recorded PCAP / market-data capture**
replay karte — asli distribution, asli edge cases (gaps, out-of-order,
crossed quotes from the real venue), deterministic. Is sim ka kaam:
development + CI ke liye ek coherent, tweakable, fast source.

> Real feed handlers ko **defensive** hona padta — malformed message, gap,
> crossed book, stale snapshot sab handle karna. Yeh sim woh cheezein
> jaan-boojh kar produce **nahi** karta (clean stream) taaki niche ke
> tests ka focus unke apne logic pe rahe. Gap/recovery handling 38/13 ka
> topic hai.

## ⚠️ Traps

### Trap 1 — "non-crossing add" ≠ "non-crossing book"
Sabse subtle. Har add insertion-time pe non-crossing, par mid move hone pe
stale orders cross kar dete. Book-level invariant enforce karo (clamp +
pull), ya `resolve_cross` (project 3).

### Trap 2 — wall clock ghusa dena
`std::chrono::now()` kahin bhi → determinism gaya. Sim ka apna `ts_`
counter use karo. (Measurement rdtsc alag baat hai — woh output ko affect
nahi karta.)

### Trap 3 — RNG state leak
Ek `static` RNG, ya ek shared global → do sims interfere. Per-sim `Rng`
member, `reset(seed)` sab state clear kare.

### Trap 4 — seq as u32 overflow
Long soak test (billions of messages) mein u32 seq wrap kar jaayega. Yahan
u64. Real feeds often u32/u64 — spec check karo.

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Random input zyada realistic | Deterministic replay — realistic AUR reproducible |
| Book cross nahi karega agar har add non-crossing hai | Mid move + stale orders → cross; book-level invariant chahiye |
| Sim ko production feed handler jaisa robust banao | Sim = clean source; robustness niche ke handler ka kaam |

## Exercises

1. `kMaxDist` 12 se 4 kar do. Book spread pe kya asar? Non-crossing rehta?
   <details><summary>Answer</summary>
   Spread tighter (orders touch ke bahut paas). Non-crossing abhi bhi
   guaranteed (clamp still enforced). Par book patla — strategy ki
   imbalance zyada volatile, aur venue ke paas sweep karne ko kam depth.
   </details>

2. `01_market_data_sim.cpp` mein seed 44 → 7 kar do. Determinism check
   abhi bhi pass hota?
   <details><summary>Answer</summary>
   Haan — dono `MarketDataSimulator(7)` identical stream denge. Seed sirf
   *kaunsa* stream decide karta, determinism nahi. Determinism `reset`/RNG
   discipline se aata.
   </details>

3. Wire format mein `seq` ko u32 kar do (34-byte frame). Kya break hota,
   aur kitne messages tak safe?
   <details><summary>Answer</summary>
   `MdMessage::seq` u64 hai, wire u32 → `encode` truncate karega (top bits
   lost), `parse` zero-extend. Safe until seq > 2^32 ≈ 4.29 billion
   messages. Ek 200k-msg sim ke liye theek. Real long-running feed ke liye
   nahi — spec dekho.
   </details>

## Interview questions

1. L3 vs L2 market data — farak? Yeh sim kaunsa produce karta aur `L2Book`
   use kaise karta?
2. Sequence number ka kaam? Gap detect hone pe kya karte (38/13)?
3. Market data simulator ko deterministic banane ke liye kya-kya rules?
4. "Non-crossing add" caafi kyun nahi book non-crossing rakhne ke liye?

## Next
→ [`03-project-feed-parser.md`](03-project-feed-parser.md)

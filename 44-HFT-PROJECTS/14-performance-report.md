# 14 — Final performance report: the latency budget

## Prerequisites
- `12-project-mini-hft-engine.md`, `13-integration-and-testing.md`
- `37-HFT-FUNDAMENTALS/14-latency-budget.md`,
  `42-HFT-NETWORKING/14-wire-to-wire-measurement.md`

## Yeh topic abhi kyun

Spec (§2.12) ka last step: **explain karo ki kya badla.** Yeh lesson poore
system ka ek latency budget deta — har component ka apna number, ek opaque
total nahi (42/14's "timestamp har hop pe" principle).

## The box

**AMD Ryzen 7 4700U** (Zen 2), ~2.0 GHz, Windows x64 + MinGW-w64 GCC
15.1.0, **unpinned desktop**. `ticks_per_ns` 1.9962. Absolutes ~20%
run-to-run (35/06); **ratios/shapes are the deliverable**, tail `max`
especially jittery.

## Per-component (measured, this box)

| Component | Example | Metric | Number |
|---|---|---|---|
| Market data sim | `01` | invariants | seq +1, ts monotonic, non-crossing, replay identical |
| Feed parse (v1) | `02` | ns/frame | ~1.76 |
| Feed parse (v3) | `02` | ns/frame | ~1.76 (**~1.0×** — `-O2` Rule-2 null, cf. 43/08) |
| L2 book apply | `03` | ns/message | ~17 |
| FixedPool alloc+free | `05` | p50 / p99.9 vs `new`/`delete` | **~2.0× / ~5.0×** |
| ObjectPool acquire+release | `06` | p50 / p99.9 | ~30 / ~110 ns |
| SPSC hand-off | `07` | throughput / paced p50 | ~6 M msg/s / ~350 ns |
| Risk check | `09` (in `11`) | ns/message | ~0.6 (all 5 checks, O(1)) |
| OMS submit+ack+fill | `10` (in `11`) | ns/message | ~0.3–0.4 |

## The mini engine — per-stage budget (`11_mini_hft_engine.cpp`, N=200000)

```
per-stage rdtsc attribution (ns/msg -- probe cost ~40 ns inflates each):
                parse    book    strat   risk    oms
  naive          ~45     ~150     ~30    ~0.6   ~0.4
  optimized      ~47      ~67     ~30    ~0.7   ~0.3

end-to-end (single steady_clock -- the honest number):
  naive     mean ~225 ns/msg   p50 ~215   p99 ~430   p99.9 ~800
  optimized mean ~145 ns/msg   p50 ~130   p99 ~290   p99.9 ~1300
                                                     max: jittery (unpinned -- ignore)
  speedup: ~1.5-1.6x
```

### Reading it (43/02, 43/03)
- **`book` is the only stage that changed** (~150 → ~67 ns/msg). Everything
  else is within noise, and mostly rdtsc-probe cost anyway.
- The per-stage rdtsc absolutes are **probe-cost-limited** for the fast
  stages (`risk`, `oms`, `parse` real work is single-digit ns). Trust the
  *single-clock end-to-end mean* and the *ratio between naive and
  optimized per stage*.
- The `max` outlier on the optimized run is OS-scheduler jitter, not the
  code (41/13). p50/p99/p99.9 are the comparison.
- ⚠️ **Dhyaan se dekho: optimized ka p99.9 naive se KHARAB hai**
  (~1300 vs ~800). Yeh jitter nahi hai — yeh code hai, aur reproducible
  hai. Iska poora accounting neeche "…par tail ulta bigda" mein.

### What changed and why (the required explanation)
The `book` stage's cost was `ExecutionSimulator::on_market_event` — one
`std::map` insert + one `std::list`-node `malloc` per market message
(folder 40's V1 storage). Replaced with `FastVenue`: a flat `int64` array
indexed by `px - base` (aggregate qty), a per-level FIFO `std::vector`
(price-time priority), and a dense-id `loc_` for O(1) cancels. Per market
Add: `arr[idx] += qty` + an amortized-no-alloc `push_back`. Per Cancel:
one direct index + a short FIFO scan. **Same fills** (proven byte-identical
across 5 configs, `12_integration_tests.cpp`), no per-message tree walk,
no per-message allocation → the stage roughly halves and end-to-end
improves ~1.5–1.6×.

### ⚠️ …par tail ulta bigda (the honest part)

p50 aur p99 dono behtar hue. **p99.9 kharab ho gaya** — aur HFT mein
wahi number mean se zyada matter karta hai. Same box, V3 vs naive,
**interleaved** (naive dono binaries mein same code hai, isliye woh
machine-state ka control hai), 5 runs:

```
               p50        p99       p99.9
  naive       ~205 ns    ~530 ns    ~866 ns
  optimized   ~115 ns    ~341 ns   ~1256 ns    <- ~1.45x WORSE tail
```

Yeh noise nahi hai: 5/5 runs mein optimized ka p99.9 **1232–1272 ns** ke
tight band mein aaya, naive ka **831–941 ns** mein. Ek bhi run overlap
nahi hua.

**Kyun:** `FastVenue` ka best-bid/best-ask tracking. Jab best level
khaali hota hai, `rewalk_bb()` / `rewalk_ba()` flat array ko linearly
scan karte hain agla non-empty level dhoondhne ke liye — `kLevels = 2048`
tak. Typical case O(1) (agla level aksar paas hi hota hai), par jab book
ek side pe patla ho jaaye, woh walk lamba ho jaata hai.

Guess mat karo — **count karo**. `rewalk` mein ek step-counter daal ke
(200k messages):

```
  rewalk calls             :    28,030
  total steps              : 2,709,424
  max steps in ONE call    :     1,636      <- 2048 mein se
  calls >= 500 steps       :     1,952
```

Ab arithmetic: p99.9 of 200,000 = **worst 200 messages**. Aur humare paas
**1,952** calls hain jo 500+ steps chale — tail bharne ke liye zaroorat se
~10× zyada. Yehi spikes p99.9 hain.

**`std::map` mein yeh kyun nahi dikhta:** woh *har* message pe uniformly
mehnga hai (node alloc + tree walk). Uska p50 kharab hai, par usme rare
1,636-step scan **nahi** hai. Flat array ne **typical case jeeta aur
worst case becha** — [`39/11`](../39-ORDER-BOOK/11-top-of-book-fast-path.md)
ne yeh trade-off likha tha ("typical O(1), worst-case O(NUM_LEVELS)");
yahan wahi trade-off **measure** ho gaya.

**Fix (V4): bitmap summary index.** Har level ka ek bit — "yahan qty hai
ya nahi". 2048 levels = 32 × `uint64_t`. `rewalk` ab 2048 array-steps ki
jagah **≤32 word-checks** hai:

```cpp
static constexpr std::size_t kWords = kLevels / 64;          // 2048 -> 32
std::array<std::uint64_t, kWords> bidmsk_{}, askmsk_{};

// arr(s)[i] badalne ke BAAD call karo -- bit ko array ke saath sync rakho
void sync(Side s, std::size_t i) {
    const std::uint64_t bit = 1ull << (i % 64);
    if (arr(s)[i] > 0) msk(s)[i / 64] |=  bit;
    else               msk(s)[i / 64] &= ~bit;
}

void rewalk_bb() {                        // sabse ooncha set bit <= start
    const std::size_t start = (bb_ == kNoBid || bb_ >= kLevels) ? kLevels - 1 : bb_;
    std::size_t   w = start / 64;
    std::uint64_t m = bidmsk_[w];
    const unsigned b = static_cast<unsigned>(start % 64);
    if (b < 63) m &= (1ull << (b + 1)) - 1;          // start se upar ke bits hatao
    for (;;) {
        if (m) { bb_ = w * 64 + (63u - static_cast<std::size_t>(std::countl_zero(m))); return; }
        if (w == 0) { bb_ = kNoBid; return; }
        m = bidmsk_[--w];
    }
}
```

⚠️ Poora kaam bit ko array ke saath **sync** rakhne mein hai: jahan-jahan
`bid_`/`ask_` badalta hai (Add, Cancel/Trade, `clear_level`, aur `send()`
ka sweep — 6 jagah), wahan `sync()` call karna padta hai. Ek bhi jagah
chhoot gayi to book silently galat ho jayega — isliye correctness gate
(`12_integration_tests.cpp`) pehle chalao, phir benchmark.

Measured (wahi interleaved harness; fills byte-identical, gate pass):

| | p50 | p99 | **p99.9** |
|---|---|---|---|
| V3 — flat scan | ~115 ns | ~341 ns | **~1256 ns** |
| V4 — bitmap index | ~116 ns | ~317 ns | **~888 ns** |

5/5 pairs mein tail behtar hua. p50 same raha (woh pehle se rewalk-free
tha), p99 ka fark noise ke andar hai, aur optimized ka p99.9 ab naive ke
**neeche** aa gaya — regression khatam.

> **Yeh folder 39 ke V2 lesson ka hi doosra roop hai:** jo optimization
> typical case jeetati hai, woh worst case mein naya cost daal sakti hai.
> Farq sirf itna: V2 ka regression **p50 mein dikh gaya tha**, yeh
> **p99.9 mein chhupa tha**. Agar hum sirf mean/p50 report karte, hum yeh
> **miss kar dete** — aur production mein yehi woh 0.1% hai jo trade
> haarta hai. (Trap 4 neeche, aur 35/06.)

## Where a real tick-to-trade budget goes

This capstone measures the *middle* of the path. A real budget (37/14,
42/14):

```
  wire arrival (NIC)                      0
  NIC -> kernel -> app (kernel socket)    ~2000-5000 ns   (or ~100-300 with kernel bypass, 42/03)
  parse                                   ~5-50 ns
  book update                             ~20-100 ns
  strategy decision                       ~10 ns .. many us  (this is where YOUR logic lives)
  risk check                              ~1-10 ns
  order encode + serialize                ~10-50 ns
  app -> kernel -> NIC -> wire            ~2000-5000 ns   (or bypass)
  ----------------------------------------------------------
  wire-to-wire total                      dominated by the two network hops,
                                          unless you've bypassed the kernel
```

**The lesson (42/14):** with kernel bypass the network hops shrink to
hundreds of ns, and then **your processing becomes the biggest and most
controllable slice** — which is exactly what folders 36/43/44 are about.

## HFT relevance

- A latency budget is a **living document**. Every component has an
  owner and a number; regressions are caught per-commit (43/02).
- You optimize the biggest slice you control. Early on that's often
  parsing/book/serialization; after kernel bypass it's strategy logic;
  eventually it's physics (fiber ~5 ns/m, 42/12 → colocation) and you
  stop (43/16).
- Report **p99.9**, per-component, not a mean total. A mean hides the
  allocation spike that costs you the trade.

## ⚠️ Traps

### Trap 1 — reporting one wire-to-wire number
"Tick-to-trade 10 µs" tells you nothing about *where*. Per-hop breakdown
or you can't optimize (42/14 trap 1).

### Trap 2 — trusting per-stage rdtsc for fast stages
4 probes × ~10 ns each > the work being measured. Single clock for the
total; ratios for attribution.

### Trap 3 — optimizing a slice you don't control
The kernel socket path is ~5 µs and you're shaving ns off parsing. Bypass
the kernel (42) — that's the 50× win.

### Trap 4 — quoting the `max` on an unpinned box
One deschedule in 200k iterations. Meaningless. Pin + isolate for tail
numbers, or quote p99.9 and note the box.

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| One total latency number is the report | Per-component p99.9, each with an owner |
| Per-stage rdtsc gives the truth | Probe cost dominates fast stages; single clock + ratios |
| Optimize the slowest thing | Optimize the slowest thing *you control* |
| ~1.5× is a small win | It's the connected-system number; the changed stage alone ~2× |

## Exercises

1. `11_mini_hft_engine.cpp` mein rdtsc probes hata do, sirf ek
   `steady_clock` around the whole loop. Optimized mean/msg ab? Kya per-
   stage attribution possible?
   <details><summary>Answer</summary>
   Mean/msg girega (~40 ns of probe removed per message... actually more,
   4 probes) — likely to ~60-90 ns/msg optimized. Per-stage attribution
   ab impossible (koi checkpoints nahi) — that's the trade-off. For fast
   code: `perf` sampling or batch timing, not per-op rdtsc (43/03).
   </details>

2. Naive engine ka `book` stage ~150 ns/msg. Agar tum ise 0 kar do,
   Amdahl se end-to-end speedup (naive ~225 total)?
   <details><summary>Answer</summary>
   Naive book ~150 (with probe), real work maybe ~110. `1 / (1 - 110/225)`
   ≈ 1.96× — close to the measured ~1.5-1.6× (which is lower because
   FastVenue isn't free, ~67 ns). Amdahl gives the ceiling.
   </details>

3. Real budget: network 5000 ns, your code 145 ns. You make your code
   0 ns. Wire-to-wire improvement %?
   <details><summary>Answer</summary>
   `145 / (2*5000 + 145)` ≈ 1.4%. The two kernel-socket hops dominate.
   The win is kernel bypass (42/03) — hops → ~200-600 ns total — *then*
   your 145 ns is worth attacking.
   </details>

4. Optimized ka p99.9 naive se kharab hai. Tum kaise **prove** karoge ki
   yeh code hai, OS jitter nahi? (Sirf dobara chalana kaafi nahi.)
   <details><summary>Answer</summary>
   Teen cheezein: **(a)** A/B ko *interleave* karo — ek hi batch mein
   V3, V4, V3, V4 … taaki thermal/background drift dono pe barabar pade;
   **(b)** ek **control** rakho jo badla hi nahi (yahan `naive`, jo dono
   binaries mein same code hai) — agar control stable hai aur sirf
   optimized hilta hai, to machine nahi, code hai; **(c)** mechanism
   **count** karo, guess mat karo — `rewalk` mein step-counter daal ke
   dekho ki 500+ step wali calls (1,952) tail ke messages (200) se kitni
   zyada hain. Tab jaake "yeh rewalk hai" ek claim se fact banta hai.
   Sirf `max` dekh ke mat bolna — woh genuinely jitter hota hai (Trap 4).
   </details>

5. `rewalk` ko bitmap se theek karne ke baad p50 **nahi** badla. Kyun,
   aur yeh expected tha ya suspicious?
   <details><summary>Answer</summary>
   Expected. Typical message pe `rewalk` ya to chalta hi nahi, ya 1-2
   steps ka hota hai — p50 pe uska hissa lagbhag zero tha. Bitmap sirf
   **lambi** walks kaatta hai, aur woh sirf tail mein hain. Yehi is
   lesson ka point hai: **jo cheez tail banati hai, woh median mein
   invisible hoti hai** — isliye fix bhi sirf tail mein dikhega. Agar p50
   bhi bahut gir jaata, to shak karna chahiye ki tumne galti se kuch aur
   badal diya (ya gate fail ho raha hai).
   </details>

## Interview questions

1. Latency budget mein per-component number kyun, ek total kyun nahi?
2. Per-stage rdtsc kab galat picture deta, aur alternative?
3. Kernel bypass ke pehle vs baad — kaunsa slice sabse bada?
4. Kis metric pe report karte (mean vs p99.9) aur kyun?
5. Ek flat-array book O(1) lookup deta hai. Uska worst case kya hai, aur
   woh percentile-wise kahan dikhega?
6. "Amortized O(1)" aur "bounded latency" mein fark — HFT ke liye kaunsa
   chahiye, aur kyun `std::vector::push_back` dono nahi de sakta?

## Next
→ [`15-extensions.md`](15-extensions.md)

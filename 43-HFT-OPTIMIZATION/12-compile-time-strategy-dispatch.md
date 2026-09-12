# 12 — Compile-time strategy dispatch: no runtime branches

## Prerequisites
- `11-lookup-tables.md`
- `21-TEMPLATES/*`, `36-LOW-LATENCY-CPP/13-virtual-dispatch-elimination.md`,
  `23-compile-time-dispatch.md`

## Yeh topic abhi kyun

Trading system mein bahut si "config" decisions hoti hain jo **poore run
ke liye fixed** rehti — kaunsi strategy, kaunsa venue protocol, STP mode
on/off, logging on/off. Agar yeh har tick pe `if` / virtual call se check
hoti — woh branch aur indirection **har message** pe cost hai, jabki answer
kabhi badalta nahi.

**Dispatch once, at the top. Hot loop mein zero.**

---

## Runtime dispatch ki cost

```cpp
struct IStrategy { virtual void on_tick(const Book&) = 0; };   // virtual
IStrategy* strat = make_strategy(cfg.name);
for (auto& tick : feed) strat->on_tick(book);       // indirect call, har tick
```

Har `on_tick`:
- **vtable load** (1 dependent load) + **indirect call** (BTB predict; miss
  = ~15-20 cyc)
- compiler `on_tick` ke **andar inline nahi** kar sakta (kaunsa body?) →
  no cross-call optimization, register spills at boundary
- agar body chhota (2-3 ops) — call overhead > body

`std::function` aur bura: type-erased, possible heap, do indirections.
`36/13`, `36/14` ne measure kiya — chhote callbacks pe 3-10×.

---

## Compile-time dispatch — options

### 1. Template policy parameter

```cpp
template <class Strategy, bool kLogging, StpMode kStp>
void run_pipeline(Feed& feed) {
    Strategy strat;
    for (std::size_t i = 0; i < feed.n(); ++i) {
        parse(...);
        book.apply(...);
        if constexpr (kStp != StpMode::None) { /* stp code, warna GAYAB */ }
        strat.on_tick(book);                  // fully inlined, no vtable
        if constexpr (kLogging) log(...);     // warna zero bytes
    }
}
```

`if constexpr` — false branch ka code **compile hi nahi hota** (I-cache
mein aata hi nahi). `strat.on_tick` concrete type — inlined, cross-optimized.

Dispatch ek baar, `main` mein:
```cpp
if (cfg.strategy == "mm" && cfg.logging)
    run_pipeline<MarketMaker, true, StpMode::CancelNewest>(feed);
else if (cfg.strategy == "mm" && !cfg.logging)
    run_pipeline<MarketMaker, false, StpMode::CancelNewest>(feed);
// ... (combinatorial -- neeche trap)
```

### 2. CRTP (static polymorphism)

```cpp
template <class Derived>
struct StrategyBase {
    void on_tick(const Book& b) { static_cast<Derived*>(this)->on_tick_impl(b); }
};
struct MarketMaker : StrategyBase<MarketMaker> {
    void on_tick_impl(const Book& b) { /* ... */ }   // resolved at compile time
};
```

Interface (base) + concrete dispatch, **no vtable**. `39`/`40` internals
isse milte-julte helpers use karte.

### 3. Tag dispatch / `if constexpr` on an enum template param

```cpp
template <OrderType T> void handle(const Order& o) {
    if constexpr (T == OrderType::Limit) { ... }
    else if constexpr (T == OrderType::Market) { ... }
}
```

`40`'s `matching_engine.hpp` `add_impl<Map>` / `match_against<OppMap>` —
template pe map type, compiler dono sides (bids/asks) ke liye specialized
code banata, koi runtime "which side" branch nahin.

---

## Trade-off — code bloat

Har template instantiation = **alag copy** of the code.

- `run_pipeline<MM, true, CN>` + `<MM, false, CN>` + `<MM, true, None>` +
  ... = 2 (strat) × 2 (log) × 4 (stp) = **16 copies** of the whole loop.
- Binary bada → I-cache/iTLB pressure (`06`) → agar tum ek run mein sirf 1
  variant chalate, baaki 15 dead weight (BOLT `-split-all-cold` inhe door
  daal deta).
- Compile time badhta.

**Balance:**
- Sirf **genuinely hot + genuinely fixed** decisions ko compile-time karo.
- 2-3 axes max. 5 booleans = 32 variants = bloat.
- Ek axis jise tum sach mein sirf ek value pe chalate (STP mode) — us par
  build karo, baaki ko runtime `if` (cold path) rehne do.

---

## Kya compile-time, kya runtime

| Decision | Kab badalta | Dispatch |
|---|---|---|
| Kaunsi strategy | Process start pe, ek baar | **compile-time** (template) ya ek virtual call **outside** loop |
| Venue protocol (ITCH vs SBE) | Per connection, fixed | compile-time per handler, ya dispatch at connect |
| STP mode | Config, fixed for session | compile-time agar sirf 1-2 used; warna runtime field |
| Logging level | Config | compile-time `if constexpr` for the hot trace points |
| Per-order: limit vs market | **Har order** | **runtime** — yeh genuinely varies |
| Buy vs sell side | Har order | runtime (ya template both, `40` style) |

Rule: **jo cheez ek run mein constant hai** → compile-time / hoist outside
loop. Jo har iteration change hoti → runtime (aur usko branch-predict-friendly
ya branchless banao, `36/06`).

---

## Is folder ke pipeline pe

`pipeline.hpp` mein V0/V3 alag **classes** hain (not a runtime flag) —
`04_before_after.cpp` dono ko template function se drive karta
(`true_avg_ns<PipelineV0>` / `<PipelineV3>`). Koi "which version" branch
hot loop mein nahi. Yeh compile-time dispatch ka simplest form: **do
concrete types, template pe select.**

V3 ke andar `type == 'A'` (add vs cancel) — yeh **runtime** hai (har
message alag), toh woh ek predictable branch rehta. Sahi call: woh
genuinely varies.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — combinatorial explosion
5 compile-time booleans → 32 instantiations → binary bloat, compile time,
I-cache. 2-3 axes max; baaki runtime.

### Trap 2 — template everything, including cold paths
Error handler, recovery, admin — inhe template karne ka koi faayda nahi
(rarely run). Sirf hot loop ke andar ka dispatch.

### Trap 3 — `if constexpr` vs plain `if` confuse karna
Plain `if (kLogging)` with `kLogging` a `constexpr bool` — compiler **shayad**
dead-branch eliminate kar de, par guarantee nahi (aur dono sides type-check
hoti). `if constexpr` — guaranteed, aur false side ill-formed reh sakta.

### Trap 4 — virtual call ko loop ke andar se "sirf ek baar" maan lena
`for (...) strat->on_tick()` — "strat toh fixed hai" — phir bhi har
iteration vtable load + indirect call + no-inline. Fixed hone se compiler
ko pata nahi (jab tak devirtualization na ho, jo fragile). Hoist the type
decision out.

### Trap 5 — LTO/devirtualization pe bharosa
"-flto se compiler khud devirtualize kar dega" — kabhi, kabhi nahi
(pointer escapes, multiple impls visible). Agar hot path critical hai,
explicit compile-time dispatch — bharose pe mat chhodo.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Virtual call "bas ek pointer" | vtable load + indirect call + **no inline** across it |
| `std::function` = lightweight callback | Type-erased, maybe heap, 2 indirections |
| Template dispatch hamesha behtar | Bloat cost; sirf hot + fixed decisions |
| `if (constexpr_bool)` == `if constexpr` | `if constexpr` guaranteed elim + false side can be ill-formed |
| Har config ko template param banao | 2-3 axes; explosion warna |

---

## Hands-on

```bash
./build.ps1 fast 36-LOW-LATENCY-CPP/examples/07_dispatch_comparison.cpp   # virtual vs CRTP vs template
./build.ps1 fast 36-LOW-LATENCY-CPP/examples/08_std_function_cost.cpp
./build.ps1 fast 43-HFT-OPTIMIZATION/examples/04_before_after.cpp          # template<Pipeline> dispatch
```

---

## Exercises

1. `04_before_after.cpp` `true_avg_ns` ko template banaya (`<PipelineV0>` /
   `<PipelineV3>`). Isko runtime `bool use_v3` param bana do (ek base class
   + virtual `process_one`). Kya cost aayega?
   <details><summary>Answer</summary>
   Har `process_one` ab virtual → vtable load + indirect call + compiler
   `parse/book/signal` ko inline nahi kar sakta us call boundary pe. v3
   (~25 ns/tick) pe yeh ~5-15 ns overhead = 20-60% regression. Template
   version mein pura loop ek concrete type ke liye specialized + inlined.
   </details>

2. Tumhe 3 strategies × 2 venues × logging on/off chahiye. Sab compile-time
   = kitne instantiations? Better approach?
   <details><summary>Answer</summary>
   3 × 2 × 2 = 12 full-loop copies. Better: **venue** compile-time (feed
   parsing genuinely different + hot), **strategy** ek virtual call
   **outside** the tick loop ya CRTP, **logging** `if constexpr` sirf trace
   points pe (poora loop nahi). Bloat 12 → ~2-3.
   </details>

3. `if constexpr (kStp != None)` vs `if (stp_mode != None)` (runtime field)
   — hot path pe farak?
   <details><summary>Answer</summary>
   Runtime: har order pe ek load + compare + predictable branch (~1-2 cyc,
   usually predicted) **aur** STP code hamesha binary mein (I-cache). If
   STP genuinely off for the whole session → `if constexpr` version mein
   woh code exist hi nahi karta, aur branch bhi nahi. Session-fixed → prefer
   `if constexpr`.
   </details>

---

## Interview questions

1. Virtual call ki hot-loop cost — vtable, BTB, inlining terms mein.
2. `if constexpr` vs `if (constexpr_var)` — do concrete differences.
3. CRTP kya solve karta, vtable ke bina polymorphism kaise?
4. Compile-time dispatch ka main downside? Kaise manage karoge?
5. "Strategy fixed hai toh virtual call free hai" — kyun galat?

---

## Next
→ [`13-case-study-feed-handler.md`](13-case-study-feed-handler.md)

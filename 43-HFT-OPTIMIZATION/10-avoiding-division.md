# 10 — Avoiding division: reciprocal multiply, shifts, tables

## Prerequisites
- `09-fixed-point-arithmetic.md`
- `05-OPERATORS/05-bitwise-operators.md`, `35-PROFILING-BENCHMARKING/*`

## Yeh topic abhi kyun

Signal stage mein 2 divisions thi (`mid = sum/2`, `sma = sum/W`). Division
x86 ki sabse dheemi common integer op hai. Yahan alternatives — **measured**.

---

## Division kitni mehngi

`idiv` / `div` **pipeline nahi hoti** — engine ko block karta jab tak
poora nahi ho jata. Throughput ≈ latency:

| Op | Zen 2 (approx cycles) | Pipelined? |
|---|---|---|
| `add`, `sub`, `and`, shift | 1 | haan (multiple/cycle) |
| `mul` (32/64-bit) | 3 | haan |
| `div` / `idiv` 32-bit | ~14–20 | **nahi** |
| `div` / `idiv` 64-bit | ~20–40+ | **nahi** |
| float `divsd` | ~13–15 | partially |
| float `sqrtsd` | ~20 | partially |

`06_division_elimination.cpp` (is box, 16M elems, ratios):

```
A  a[i] / b[i]      (var / var   -> `div`)        :  ~4.1  ns/elem   (baseline)
B  a[i] / 1000      (const       -> magic mul)     :  ~0.31 ns/elem   (~13x)
C  a[i] >> 10       (pow2        -> shift)         :  ~0.25 ns/elem   (~17x)
D  (a[i]*R) >> 32   (invariant   -> recip mul)     :  ~0.59 ns/elem   (~7x)
```

---

## Technique B — compile-time constant divisor (free!)

Agar divisor `constexpr` / literal hai, compiler **khud** `div` ko
`multiply + shift` mein badal deta ("magic number" — Granlund-Montgomery):

```cpp
x / 1000u    // compiler emits:  mov rax, x; mul <magic>; shr rdx, <s>
```

Tumhe **kuch nahi karna** — bas divisor ko `constexpr` rakho, runtime
variable mat banao. `06`'s B pe ~13× — sirf `1000` ko literal rakhne se.

**Sabak:** `kWindow`, tick size, lot size, bucket count — `constexpr`
declare karo. `int W = cfg.window; ... / W` (runtime) → asli `div`.
`constexpr int W = 64; ... / W` → shift.

---

## Technique C — power-of-two divisor → shift

`x / 2^k` == `x >> k` (unsigned; signed pe compiler ek fixup add karta).
1 cycle, pipelined.

```cpp
pos_ = (pos_ + 1) & (kWindow - 1);   // % kWindow, W power-of-2  -> AND
sma  = ring_sum_ >> 6;               // / 64                     -> shift
idx  = hash & (kBuckets - 1);        // % kBuckets               -> AND
```

`pipeline.hpp` V3 exactly yeh: `kWindow = 64`, `pos_` wrap `& 63`, aur
threshold check integer-only (no `/W` at all — algebra se multiply through
kar diya).

**Design rule:** jahan bhi tumhe `%` ya `/` ek size se chahiye — us size
ko **power of two** banao. Ring buffers, hash tables, batch counts.

---

## Technique D — runtime divisor, but loop-invariant → reciprocal multiply

Divisor runtime pe pata chalta (config se, per-symbol tick size, SMA
window from a param) par **loop ke andar badalta nahi**. Ek baar reciprocal
nikaalo, phir har iteration multiply:

```cpp
// integer "round-up" reciprocal:  R = floor(2^32 / b) + 1
const std::uint64_t R = ((std::uint64_t(1) << 32) / b) + 1;
for (...) q = (std::uint64_t(a[i]) * R) >> 32;    // == a[i] / b
```

`06`'s D: ~7× vs real `div`. Ek `mul` + `shr` per element vs `div`.

**Precondition** (`06` mein noted): yeh simple form `a < 2^16`, `b <= 1024`
ke liye exact hai (error term `a·1/2^32 < 2^-16 < 1/b`, kabhi integer
boundary cross nahi karta). Bade `a` ke liye Granlund-Montgomery ka **full**
form (extra shift/add fixup) chahiye — ya `libdivide` use karo (header-only,
sahi kar deta), ya Lemire's 64-bit `fastmod` (`__int128`).

float version aur simple: `double inv_b = 1.0 / b;` ek baar, phir `a * inv_b`
— par float rounding (`09`), aur price math mein nahi.

HFT use: per-symbol setup pe `tick_reciprocal` cache karo. Phir
"price → tick index" har market-data message pe ek multiply, `div` nahi.

---

## Technique — lookup table (agla lesson, `11`)

Chhote finite domain pe division ka result **precompute** karo.
`price_to_level[px - base]` type. Trade-off: table cache mein jagah leti.
Detail `11-lookup-tables.md`.

---

## Algorithm-level — division hi khatam karo

Sabse achha: aisa formula dhoondo jisme division hai hi nahi.

- `a/b > c`  →  `a > b*c`  (agar `b > 0`) — division ko multiply mein
  badlo, comparison ke liye. `pipeline.hpp` V3 signal check exactly yeh:
  `mid > sma*ratio` ko `sum2*W*kDen > ring_sum*kNum` bana diya.
- Average of a sliding window: running sum rakho (`ring_sum_`), `/W` sirf
  jab actual average chahiye; comparisons ko `sum` pe hi karo (`×W`).
- VWAP: `Σ(p·q) / Σq` — dono running sums, final divide ek baar display pe.
- Normalization: agar sirf argmax chahiye, normalize mat karo (monotonic).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — hidden division (`%`)
`x % n` bhi `div` use karta (remainder). `i % ring_size` har iteration →
`div` har iteration. Power-of-2 size + `& mask`. (`CLAUDE.md` ka note: is
repo mein ek purana division benchmark loop ke andar `%` chhupa raha tha —
yeh bilkul woh galti.)

### Trap 2 — reciprocal multiply ka overflow / range
Simple `(a*R)>>32` bade `a` pe galat (`06`'s pehla attempt MISMATCH tha
jab `a` 32-bit tha). Range check karo, ya `libdivide`, ya full G-M form.

### Trap 3 — signed division ka round-toward-zero
`-7 / 2 == -3` (C++), `-7 >> 1 == -4` (floor). Signed `x / 2` != `x >> 1`.
Compiler `x / 2` ke liye extra `add` + `cmov` daalta (sign fixup) — abhi
bhi `div` se tez, par shift jitna nahi. Prices unsigned/positive rakho jahan
ho sake.

### Trap 4 — `-ffast-math` se float division "fix" karna
`-ffast-math` `a/b` ko `a * (1/b)` (approx `rcpps`) bana sakta — 12-bit
precision, refinement ke bina galat. Trading math mein **kabhi nahi**.
Explicit reciprocal-multiply likho jahan chahiye, poore file pe fast-math
mat.

### Trap 5 — micro-opt jab `div` hot hi nahi
`perf annotate` mein `idiv` pe 0.1% samples → chhod do. Division ~20 cyc
hai, par agar ek tick mein ek baar hoti aur baaki 500 cyc hai — 4% —
irrelevant. `06` measure isliye karta ki tum **ratio** jaano, phir apne
profile se decide karo.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| `/` aur `*` similar cost | `div` ~20-40 cyc, non-pipelined; `mul` ~3, pipelined |
| Compiler har division optimize kar deta | Sirf **constant** divisor; runtime `x/y` = real `div` |
| `x / 2` == `x >> 1` hamesha | Sirf unsigned; signed pe sign-fixup |
| Reciprocal multiply har divisor pe kaam karta | Range-limited; overflow/precision; `libdivide` for general |
| `%` division nahi hai | `%` bhi `div` — power-of-2 pe `& mask` |

---

## Hands-on

```bash
./build.ps1 fast 43-HFT-OPTIMIZATION/examples/06_division_elimination.cpp
g++ -std=c++20 -O2 -S -masm=intel 43-HFT-OPTIMIZATION/examples/06_division_elimination.cpp -o - | c++filt | grep -E "div|mul|shr|sar"
```

---

## Exercises

1. `06` mein `div_const` ka divisor `1000u` se `1024u` kar do. B aur C ab
   same kyun?
   <details><summary>Answer</summary>
   `1024 = 2^10` — compiler `/ 1024u` ko `>> 10` bana deta (magic-multiply
   ki bhi zaroorat nahi). B ≈ C. Non-power-of-2 constant (1000) pe B =
   magic multiply (~3 cyc), C = pure shift (~1 cyc).
   </details>

2. Tumhare paas per-symbol `tick_size` (runtime, config se) hai. Har MD
   message pe `level = price / tick_size`. Kaise optimize?
   <details><summary>Answer</summary>
   Symbol setup pe `tick_recip = reciprocal(tick_size)` compute + cache
   (`libdivide::divider` object ya G-M R). Hot path: `level =
   price * tick_recip >> shift` (ya `libdivide` `/`). Ek multiply vs ek
   `div` per message. Agar `tick_size` power-of-2-in-scaled-units ban
   sakta (venue allow kare) → shift.
   </details>

3. Signal: fire agar `mid / sma > 1.001`. `sma` runtime hai (sliding).
   Bina kisi division ke kaise?
   <details><summary>Answer</summary>
   `mid / sma > 1.001`  ⟺  `mid > sma * 1.001`  ⟺  (integer)
   `mid * 1000 > sma * 1001`. `sma` khud `ring_sum / W` hai → `mid * 1000 *
   W > ring_sum * 1001`. Zero divisions — `pipeline.hpp` V3 ka exact
   approach (`kNum/kDen`, `×W`).
   </details>

---

## Interview questions

1. `div` `mul` se kitna dheema, aur "non-pipelined" ka kya matlab?
2. Compiler `x / 100` ko kaise optimize karta? `x / n` (n runtime) ko
   kyun nahi?
3. Reciprocal multiply — mechanism, aur uski range/precision limitation?
4. `x % ring_size` hot loop mein — do tareeke fix karne ke?
5. Signed `x / 2` == `x >> 1`? Nahi to kyun, aur compiler kya karta?

---

## Next
→ [`11-lookup-tables.md`](11-lookup-tables.md)

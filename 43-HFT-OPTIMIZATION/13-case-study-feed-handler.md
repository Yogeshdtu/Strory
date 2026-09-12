# 13 — Case study: feed handler (parse stage)

## Prerequisites
- `09-fixed-point-arithmetic.md`, `12-compile-time-strategy-dispatch.md`
- `38-MARKET-DATA/16-building-feed-handler.md`

## Yeh topic abhi kyun

Ab poora process ek **stage** pe apply karte — parse. `01`'s profile bola
parse ~45% (rdtsc-inflated ~1.1–1.3 µs/tick). Isko step-by-step girate hain,
har step ka **number aur explanation**.

Endpoints `04_before_after.cpp` se measured; per-technique numbers `05`/`06`
se; intermediate design rationale folder 38 se.

---

## v0 — naive parse (`pipeline.hpp` `PipelineV0::parse_naive`)

```cpp
const std::string line = feed_.blob.substr(a, b - a);   // (1) heap copy
std::size_t p1 = line.find(',', p0);                    // (2) scan
type = line.substr(p0, p1 - p0)[0];                     // (3) another heap copy
...
price = std::stod(line.substr(p0, p1 - p0));            // (4) heap copy + strtod
qty   = std::stoi(line.substr(p0, p1 - p0));            // (5) heap copy + strtol
id    = std::stoul(line.substr(p0, p1 - p0));           // (6) heap copy + strtoul
```

Per message: **5+ heap allocations** (`substr` = `std::string` = `malloc`
for anything > SSO), 3 number-conversions har ek apni locale/error/rounding
machinery ke saath (`std::stod` especially — full `strtod`, rounding modes,
subnormals, hex-float, locale decimal point).

Profile (`perf report` expected): `__strtod_internal`, `operator new`,
`std::string` ctor/dtor, `memcpy`.

**rdtsc attribution:** parse ~1100–1330 ns/tick (of a ~2300–2720 ns total).

---

## Step 1 — no allocation: parse in place on `const char*`

Hypothesis: `substr` copies gaye → `std::string_view` / raw pointer walk →
5 mallocs → 0.

```cpp
const char* p = feed_.blob.data() + feed_.off[i];   // no copy
const char type = p[0];
const char side = p[2];
p += 4;
```

Mechanism: `malloc`/`free` ~15–50 ns each; 5 per message ≈ 100–250 ns gone.
Plus allocator lock/bookkeeping, plus dtor churn, plus cache pollution
(freed blocks). Expected: parse roughly **halves**.

`36/04` (allocation cost) + `36/09` (zero-copy) ne isolate karke measure
kiya — `std::string` short-lived churn 10M/s pe dominant.

---

## Step 2 — hand-rolled integer scan (no `strtod`/`stoi`)

```cpp
std::int32_t px_ticks = 0;
while (*p != '.') { px_ticks = px_ticks * 10 + (*p - '0'); ++p; }
px_ticks *= 100;
++p;                                    // skip '.'
px_ticks += (p[0] - '0') * 10 + (p[1] - '0');   // 2 frac digits
```

`std::stod` ka poora scaffolding (locale lookup, rounding-mode read, hex
detection, overflow to `HUGE_VAL`, `errno`) hataya → ek `*10 + digit` loop.
`06`-style measurement (`std::stod` vs hand-parse in isolation) pe ~5–15×
on the conversion alone.

Yeh **`09-fixed-point`** ka bhi step hai: price ab `int64` (scale 100),
`double` kabhi bana hi nahi → parse ke andar float unit missing = tez, aur
downstream compare exact.

Expected: parse ab v0 ka ~1/5 se ~1/10.

---

## Step 3 — branch-light field walk

Format generator-controlled → known layout → per-field validation nahi
(real handler validates: `38`). Fixed offsets jahan possible (`p += 4`
for `"T,S,"`), delimiter scan sirf variable-length fields (price int part,
qty, id) pe.

Real binary feeds (ITCH/SBE, `38/06`, `38/08`) pe yeh step "free" hai —
fields already fixed-offset integers, "parse" = `memcpy` + `ntohl`. Text
parsing (kuch FIX) sabse mehnga; binary pe parse stage aksar sabse chhota.

---

## Result — measured endpoints (`04_before_after.cpp`, is box)

```
                 parse ns/tick (rdtsc attribution)     ratio
v0 (naive)       ~1100 – 1330
v3 (all steps)   ~45 – 49                              ~24 – 27x
```

> v3 ka ~45 ns mein bhi ~35–40 ns rdtsc-probe cost hai (`02`/`03`) — asli
> v3 parse work < 10 ns/tick. Ratio ~25× is a **lower bound**; true parse
> speedup zyada.

Poore pipeline (A) number: ~1.9–2.7 µs → ~25–45 ns/tick, **~60–80×**
(parse + book dono, `14`).

---

## Explain — har step ne kya kiya (spec requirement §2.12)

| Step | Change | Mechanism | Delta (parse) |
|---|---|---|---|
| 1 | `substr` → `const char*` | 5 malloc/free per msg → 0; no dtor churn, no allocator lock, no freed-block cache pollution | ~2× |
| 2 | `stod`/`stoi` → hand int-scan + fixed-point | `strtod` locale/rounding/overflow scaffolding → `*10+digit`; no `double` | ~5–10× |
| 3 | delimiter-scan every field → fixed offsets where possible | fewer byte compares, fewer unpredictable branches | ~1.2–1.5× |

Multiplicative → ~24–27× measured (probe-cost-limited; true higher).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — hand-parser bina validation, real feed pe
Generator ka output trusted tha. Asli feed pe malformed message = OOB read
/ infinite loop (`while (*p != ',')` jab `,` hi na ho). Real handler:
length-bounded loops, field range checks — cold-path reject. `38/11`
(framing).

### Trap 2 — `std::string_view` bhi allocate karta samajhna
`string_view` = pointer + length, **zero allocation**. Par uspe `std::stod`
call karne ke liye pehle `std::string` banani padegi (`stod` `string`
leta) → allocation wapas. Isliye raw-pointer hand-parse.

### Trap 3 — parse ko isolation mein 10× measure, pipeline mein 2%
Agar book stage 90% hai, parse ka 10× improvement poore pipeline pe ~5%.
`14` (book) ke saath karo. Amdahl (`03`).

### Trap 4 — `char - '0'` signed-char pe
Non-ASCII byte + signed `char` → negative → garbage digit. Feed se aane
wale bytes pe `unsigned char`, ya validate. (Yahan input pure ASCII digits,
theek — par real handler mein trap.)

### Trap 5 — fixed 2-decimal assumption
`px_ticks += (p[0]-'0')*10 + (p[1]-'0')` — exactly 2 frac digits maanta.
Venue jo `100.5` (1 digit) ya `100.500` (3) bheje → galat. Format contract
lock karo ya digit-count handle.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| `std::stod` "bas number parse" | Full `strtod`: locale, rounding mode, hex, overflow, errno |
| `substr` sasta hai (chhoti string) | > SSO (~15 chars) → `malloc`; churn 10M/s pe dominant |
| Ek bada parse rewrite | Step 1 (alloc) → measure → step 2 (conv) → measure — attribution |
| Hand-parser production-ready | Generator-trusted; real feed = bounded loops + validation |

---

## Hands-on

```bash
./build.ps1 fast 43-HFT-OPTIMIZATION/examples/01_baseline_pipeline.cpp    # v0 parse ~45%
./build.ps1 fast 43-HFT-OPTIMIZATION/examples/03_optimized_pipeline.cpp
./build.ps1 fast 43-HFT-OPTIMIZATION/examples/04_before_after.cpp         # endpoint ratio
```

---

## Exercises

1. `pipeline.hpp` `PipelineV3` parse mein se fixed-point hata do — price
   `double` bana do (`std::atof`-style hand parse to double). Parse stage
   aur agreement pe kya?
   <details><summary>Answer</summary>
   Parse thoda dheema (float division/mul in the digit loop). Agreement
   ab float-vs-float — 110/110 rehne ki sambhavna zyada actually (dono
   float), par V0 aur V3 ke rounding paths alag ho to boundary flips
   possible. Asli nuksaan downstream: signal ab float → `10` ka division
   wapas, `09` ka exactness gaya.
   </details>

2. v0 ki 5 allocations ko ek reused `std::string buf` (member, `buf.assign`)
   se replace karo. Kitna milega?
   <details><summary>Answer</summary>
   `assign` capacity reuse karta → steady-state mein ~0 malloc (pehli baar
   grow, phir reuse). Allocation cost lagbhag gaya — parse ka woh ~2× step.
   Par `stod`/`stoi` scaffolding abhi bhi hai (step 2 pending). Ye ek
   achha "cheap first win" hai jab full rewrite se pehle.
   </details>

3. Real feed binary hai: `struct { u8 type; u8 side; u32 px_e4; u32 qty;
   u64 id; }` big-endian. "Parse" ab kya, aur cost?
   <details><summary>Answer</summary>
   `type = p[0]; side = p[1]; px = ntohl(load_u32(p+2)); ...` — bounds
   check + a few `bswap` + loads. ~5–15 ns, koi allocation, koi digit
   loop. Text vs binary: binary feed handlers ka parse stage aksar sabse
   **chhota** stage hota. `38/06`, `38/08`.
   </details>

---

## Interview questions

1. `std::stod` ke andar kya-kya hota (jo hand int-parse skip karta)?
2. `substr` parse loop mein kyun mehnga — kitni allocations, kya side
   effects?
3. Parse ko isolation mein 10× kiya, pipeline pe 3% — kyun ho sakta?
4. Hand-rolled parser production mein safe banane ke 3 cheezein?
5. Text feed vs binary feed — parse stage ka relative cost?

---

## Next
→ [`14-case-study-order-book.md`](14-case-study-order-book.md)

# 16 — When to stop: diminishing returns

## Prerequisites
- `15-case-study-full-pipeline.md`
- `36-LOW-LATENCY-CPP/24-tradeoffs-and-when-not-to.md`

## Yeh topic abhi kyun

Optimization ka ek **ulta** skill hai: **rukna kab hai.** Har agla 5% do
guna mehnga, do guna risky hota. Yeh lesson wahi hai jo junior engineers
skip karte aur seniors se seekhte.

---

## Signal 1 — number noise-floor ke neeche gaya

`02` ne dikhaya: is unpinned box pe per-tick number ~20% run-to-run
jhoolta. Agar tumhari agli optimization 3% ki hai — tum use **measure hi
nahi kar sakte** us noise mein.

Do options:
1. **Ruk jao** — 3% jitter ke neeche hai, prove nahi kar sakte, ship nahi
   karna chahiye.
2. **Pehle jitter fix karo** — core-pin, isolate, DVFS lock (`41/06`,
   `36/19`). Ab noise 1% → 3% wali optimization detect hoti. Par agar
   jitter itna hai to **jitter hi asli problem hai**, mean nahi — usko
   attack karo (`36/02-03` tail).

**Rule:** agar p99.9 tumhare requirement se neeche hai aur variance high
hai → **mean optimization band karo, jitter pe lago.**

---

## Signal 2 — effort/payoff curve ne knee cross kiya

```
speedup
  │        ╭──────────  <- yahan tak: parse+book (structural), ~70x, 2 din
  │      ╭─╯
  │    ╭─╯
  │  ╭─╯                <- yahan: SIMD parse, custom allocator, ~1.3x, 2 hafte
  │ ╱                      + naya bug surface, + maintenance
  │╱______________________ effort
```

Pehle 70× **structural** tha (galat data structure → sahi). Agla 1.3×
chahiye to:
- SIMD parse (AVX2 se 4 messages parallel) — 200 lines intrinsics, portability,
  hard to debug
- custom bump allocator — lifetime bugs ka naya class
- manual prefetch — microarch-specific, agli CPU pe useless ya ulta

**Har technique ka trade-off** (`36/24`, spec §2.13). Likh lo: "yeh 1.2×
dega, cost = 2 hafte + N naye failure modes + har CPU pe re-tune." Phir
business se poocho chahiye ya nahi.

---

## Signal 3 — "fast enough" — venue-defined

"Fast enough" absolute nahi, **competitive** hai:

| Question | Agar haan → |
|---|---|
| Feed ka peak message rate handle ho raha (no queue backlog)? | throughput theek |
| Tick-to-trade p99.9 < strategy ka alpha decay time? | latency theek |
| Competitors se consistently pehle fill mil raha (fill-rate metric)? | competitive |
| Risk checks + encode ka worst case exchange ke timeout se andar? | safe |

Agar sab haan — **aur optimize karne ka business case kya hai?** Engineer
ka instinct "aur tez" hota; business ka metric "aur paisa / kam risk". Woh
align nahi bhi ho sakte.

HFT mein aksar **haan** — ek nanosecond edge = money. Par tab bhi: kaunsa
nanosecond? Profile bata raha hai network 300 ns, tumhara code 40 ns. Code
ka 40 → 30 karne se poore path pe 3% — network vendor / colocation /
FPGA (`42/13`) ka ROI zyada.

---

## Signal 4 — correctness / maintainability risk > gain

- Ek optimization jo ek subtle memory-ordering assumption pe tiki hai (`27`)
  — agar galat, silent wrong trades. 5% speed worth it?
- Ek 500-line hand-vectorized function jise team ka koi aur maintain nahi
  kar sakta — bus factor 1.
- `-ffast-math` poore module pe — 8% faster, par ab har float compare
  suspect (`09`).
- Undefined behaviour "jo abhi kaam karta" (strict aliasing violation,
  signed overflow) — agli compiler version pe toot sakta.

**Rule:** speed ka gain measurable + significant + **worth the risk**
hona chahiye. "Might be faster, definitely riskier" = **no**.

---

## Signal 5 — opportunity cost

2 hafte ek function ko 1.2× karne mein = 2 hafte **nahi** kiye:
- ek naya strategy signal
- ek naya venue integration
- test coverage / observability
- ek asli bug jo P&L kha raha

Optimization addictive hai (clear feedback loop, "number gir gaya" = dopamine).
Team lead ka kaam: "yeh 1.2× worth 2 weeks vs X?" poochna.

---

## A stopping checklist

Ruk jao jab **saare** haan:

- [ ] p99.9 latency requirement ke andar hai (mean nahi — p99.9)
- [ ] Peak load pe queue backlog nahi banta
- [ ] Agli optimization ka expected gain measurement noise se **bada** hai
- [ ] Agli optimization ka gain uske risk + maintenance cost se **bada** hai
- [ ] Profile kehta hai baaki bottleneck **tumhare control mein nahi**
      (network vendor, exchange, physics — `42/12` fiber ~5 ns/m)
- [ ] Us time ka koi behtar use nahi (opportunity cost)

Ek bhi nahi → shayad aur kaam hai. Saare haan → **ship karo, move on.**

---

## ⚠️ Traps / Common mistakes

### Trap 1 — noise mein optimize karte rehna
20% jitter, 2% "improvements" chase karna — har commit "faster" claim,
kul milakar zero (ya regression jo noise mein chhup gaya). Baseline
variance ko pehle pin karo (`02`).

### Trap 2 — micro-opt jab macro-bottleneck bahar hai
Code 40 ns, network 3 µs. Code ko 20 ns karne ka poore path pe ~0.7%
faayda. Effort network pe (`42`) ya strategy pe.

### Trap 3 — "ek aur" infinitely
Har optimization ke baad "bas ek aur 5%". Stopping criteria **pehle se**
likho (requirement number). Us tak pahunche → stop.

### Trap 4 — risk ko discount karna
"Yeh UB hai par kaam karta" / "fast-math theek hai yahan" — future
compiler / edge input pe toot. Speed gain visible, risk invisible-until-it-isn't.

### Trap 5 — stopping ko "giving up" samajhna
Rukna = discipline. "Fast enough, correct, maintainable, shipped" >
"5% faster, fragile, un-reviewed, 2 weeks late."

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Hamesha aur optimize kar sakte | Har agla % mehnga + risky; knee ke baad ruko |
| "Fast enough" = ek fixed number | Venue/competition/requirement-defined; measure against that |
| Optimization = pure win | Time, risk, maintainability, opportunity cost — sab weigh |
| Noise mein 3% improvement ship karna | Prove nahi kar sakte → jitter fix karo ya ruko |
| Rukna = lazy | Rukna = knowing the ROI turned negative |

---

## Hands-on

Koi naya binary nahi — yeh judgement lesson hai. Exercise: apne folder
43 ke kisi bhi example pe, "agla 10%" ke liye ek plan likho, phir uska
cost (lines, risk, portability, maintenance) estimate karo, phir decide.

---

## Exercises

1. Pipeline v3 ~30 ns/tick hai. Requirement: feed peak 2M msg/s handle karo,
   tick-to-trade p99.9 < 5 µs. Aur optimize karen?
   <details><summary>Answer</summary>
   2M msg/s = 500 ns/msg budget; v3 ~30 ns — **25× headroom**. p99.9
   (proper measurement se) likely < 1 µs. **Ruk jao** — requirement se
   bahut andar, aur code ka hissa poore tick-to-trade mein chhota. Effort
   network/risk/strategy pe.
   </details>

2. Ek optimization 6% faster hai par baseline variance 15% hai. Kya karoge?
   <details><summary>Answer</summary>
   6% < 15% noise → measure nahi kar sakte reliably. Do: (a) variance pin
   karo — core-pin, isolate, DVFS lock, best-of-1000 samples, p50 — noise
   ~2% pe le aao, phir 6% dikhega. (b) Agar 15% jitter production-realistic
   hai to **jitter hi problem hai** — mean 6% irrelevant, tail pe lago.
   </details>

3. Team lead ne bola "yeh function 1.15× kar sakte ho AVX2 se, 10 din."
   Kaunse 4 sawaal poochoge decide karne ko?
   <details><summary>Answer</summary>
   (1) Yeh function poore hot path ka kitna % hai? (Amdahl — 1.15× local
   = kitna global?) (2) Requirement already meet ho raha? (3) AVX2 code
   kaun maintain karega, kaunse CPUs pe deploy (downclocking?)? (4) 10 din
   ka best alternative use kya hai (opportunity cost)? Answers → go/no-go.
   </details>

---

## Interview questions

1. Optimization kab rokte ho — 3 concrete signals?
2. "Fast enough" ko kaise define karoge ek trading system ke liye?
3. Ek 8%-faster change jo `-ffast-math` pe based hai — accept? Kyun/kyun
   nahi?
4. Baseline variance 20% hai, optimization 5%. Options?
5. Opportunity cost optimization decisions mein kaise aata?

---

## Next
→ [`17-exercises.md`](17-exercises.md)

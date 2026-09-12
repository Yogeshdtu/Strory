# 14 — Tick-to-trade latency budget (measured tool)

## Prerequisites
- [`13-risk-systems.md`](13-risk-systems.md)
- `36-LOW-LATENCY-CPP/01-latency-throughput-jitter.md` (budget thinking
  ka pehla parichay)
- `examples/03_latency_budget.cpp`

## Yeh topic abhi kyun
12 ka poora pipeline diagram ab ek **number** ban sakta hai — "tick-to-
trade" — aur yeh number hi 43-HFT-OPTIMIZATION mein "kya optimize karna
hai" decide karega.

---

## Tick-to-trade kya hai

> **Tick-to-trade = market data event (exchange se "tick" aana) se lekar
> tumhare order exchange tak pahunchne tak ka total time.**

```
   [exchange se tick aayi] ---- pipeline (12 ka diagram) ---- [order exchange ko bheja]
                |<------------------- tick-to-trade -------------------->|
```

Yeh **the number** hai jo har HFT firm minimize karna chahti — jitna
chhota, utni jaldi tum market-changing info pe react kar sakte (07 ka FIFO
race, 03 ka adverse-selection-avoidance — sab isi number pe depend karte).

---

## Budget banane ka process

1. **Total target decide karo** — kitna tick-to-trade "achha" hai tumhari
   strategy/venue ke liye (07 se: FIFO venue pe zyada critical hoga).
2. **Pipeline ko stages mein todo** (12 ka diagram — feed handler, book,
   strategy, risk, gateway).
3. **Har stage ko ek budget do** — nominal target time.
4. **Measure karo** (35-PROFILING) — actual p50 AUR p99.9 (jitter matters!).
5. **Jo stage sabse zyada budget se upar hai, usse pehle fix karo** (Amdahl
   — chhoti stage shave karne se total mushkil se hilega).
6. **Re-measure, repeat.**

Yeh exactly 24-tradeoffs-and-when-not-to ka "measure -> profile -> one
change -> re-measure -> explain" process hai, ab HFT-specific stages pe
apply.

---

## Measured (`03_latency_budget.cpp` — illustrative numbers)

```
stage                       budget       p50     p99.9  flag
------------------------------------------------------------
NIC RX + kernel bypass         130       120       180  over at tail
feed decode/parse              130       140       300  over at tail
book update                    170       180       250  over at tail
strategy decision              220       190       900  over at tail
risk check                     100        80       110  over at tail
order encode                   100        90       120  over at tail
NIC TX                         150       140       170  over at tail
------------------------------------------------------------
TOTAL                         1000       940      2030

target (wire-to-wire): 1000 ns
measured p50 total:    940 ns  (under target)
measured p99.9 total:  2030 ns  (OVER target)

sabse bada tail-budget overrun: "strategy decision" (700 ns over at p99.9)
```

**Yeh dhyaan se padho — do alag findings hain:**

1. **p50 total (940 ns) budget (1000 ns) ke ANDAR hai.** Typical case mein
   system apne target ko meet kar raha.
2. **Almost HAR stage apne nominal budget ko p99.9 pe THODA cross karta**
   hai (10–150 ns overrun range) — yeh **normal** hai. Nominal budgets
   typically **p50 targets** hote hain, aur tail hamesha kuch zyada hoti
   (35/05 — percentiles don't average, tail alag distribution hai).
3. **`strategy decision` ka overrun (700 ns) baaki sabse kai guna zyada
   hai.** Yehi **actual signal** hai — chhote, "sabki-normal" overruns
   ignore karo, is EK stage ko target karo.

**Nichod:** ek latency-budget table ka goal "sab green dikhna" nahi hai —
goal hai **sabse bada outlier dhoondna**. Agar tum sab 7 "thoda over"
stages ko ek-ek karke chase karte, tum apna time waste karoge chhote gains
pe jab ek bada fix (`strategy decision`) 700 ns wapas de sakta hai.

---

## p50 budget vs p99.9 budget — dono chahiye

```
Sirf p50 track karna:  "average theek hai" -> par 03/HFT ka #1 rule:
  jitter = missed trades (36/02). Ek spike miss = ek missed race.

Sirf p99.9 track karna: agar TYPICAL case bhi slow hai, poori strategy
  ka baseline edge hi weak hai — sirf tail fix karne se root problem
  chhupa rehta.
```

Isliye budget table **dono** columns rakhta — p50 batata "typical
competitiveness", p99.9 batata "kitni baar tum race haarte ho jab sabse
zyada matter karta."

---

## Network latency ko mat bhoolo (11 se)

Upar ka table sirf **tumhare apne system ke andar** ka time hai. Total
wire-to-wire mein network round-trip (exchange tak jaana, wapas aana) bhi
judta hai — jo largely colocation (11) decide karti. Ek perfect internal
pipeline bhi bura wire-to-wire number degi agar colocation nahi hai.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — sirf p50 se budget banana
Jitter/tail ignore karna 35/02, 36/02 ka poora point miss karta — HFT mein
average latency kabhi poora scorecard nahi hoti.

### Trap 2 — har "over budget" stage ko equally priority dena
Jaisa upar dikha, sab stages ka overrun same magnitude ka nahi hota. Amdahl
follow karo — sabse bada overrun pehle.

### Trap 3 — budget ko ek-baar-banaya-forever treat karna
Hardware, code, market conditions badalte rehte. Budget periodically
re-measure aur re-validate hona chahiye — 43-HFT-OPTIMIZATION mein yeh
ongoing process hoga, ek-time exercise nahi.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Har over-budget stage equally important hai | Sabse bada overrun pehle fix karo (Amdahl) |
| p50 budget hi kaafi hai | p99.9 alag, equally zaroori hai (jitter = missed trades) |
| Budget internal pipeline hi poora latency hai | Network/colocation (11) alag se judta |
| Budget ek-baar banega, hamesha valid rahega | Periodically re-measure karna padta |

---

## Exercises

1. Table mein `risk check` ka overrun sabse chhota hai (10 ns). Kya iska
   matlab risk check optimize karne layak hi nahi hai?
   <details><summary>Answer</summary>
   Abhi ke measurement ke hisaab se yeh sabse LOW priority hai (Amdahl —
   chhoti stage). Par 13 ka principle yaad rakho: risk check kabhi SKIP
   nahi hoti (safety), par uski SPEED zaroor optimize ki jaa sakti agar
   future measurement mein yeh bada offender ban jaaye. Abhi priority
   `strategy decision` hai, "risk check optimize mat karo" nahi — "abhi
   PEHLE strategy decision pe focus karo" sahi statement hai.
   </details>

2. `strategy decision` ka p50 (190) budget (220) ke andar hai, par p99.9
   (900) budget se **4x zyada** hai. Yeh kya batata hai iss stage ke
   internal behavior ke baare mein?
   <details><summary>Answer</summary>
   Typical case mein yeh stage fast hai — par kabhi-kabhi (tail mein)
   kuch drastically slow ho raha hai. Yeh signature aksar ek hidden
   allocation, cache miss, ya branch misprediction ki hoti (36-LOW-
   LATENCY-CPP ke topics) jo sirf kuch specific input patterns pe trigger
   hoti — bimodal distribution (35/04 se yaad karo). Agla step: is stage
   ko profile karo (35/10-14) aur dekho tail specifically kab trigger
   hoti.
   </details>

---

## Interview questions

1. Tick-to-trade define karo, aur uska strategy-edge se relation batao.
2. Latency budget banane ka poora process (steps) batao.
3. Kyun sirf p50 budget track karna insufficient hai HFT ke liye?
4. Agar 5 stages "thoda over budget" hain aur 1 stage "bahut zyada over
   budget" hai, kis order mein fix karoge aur kyun?

---

## Next
→ [`15-indian-vs-us-markets.md`](15-indian-vs-us-markets.md)

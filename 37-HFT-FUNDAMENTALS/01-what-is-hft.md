# 01 — HFT kya hai, kya nahi hai

## Prerequisites
- Poora `36-LOW-LATENCY-CPP` (tumhe pata hai ab **kaise** ek system nanoseconds
  mein sochta hai — ab dekhte hain yeh **kis liye** use hota)
- Koi finance background nahi chahiye — sab yahin se shuru

## Yeh topic abhi kyun
Folders 00–36 mein tumne **mechanism** seekha: memory, concurrency, cache,
allocation-avoidance, branch prediction. Ab folder 37–44 mein woh sab ek
**domain** pe apply hoga: High-Frequency Trading. Par code likhne se pehle
yeh saaf hona chahiye — HFT **hai kya**, kyunki iske baare mein internet pe
bahut sa galat/exaggerated content hai (movies, sensational news articles).

---

## Ek line mein

> **HFT = software jo bahut chhote price movements se, bahut chhoti holding
> periods mein, bahut zyada volume/speed ke through profit nikaalta hai —
> jahan "speed khud ek edge hai", sirf ek implementation detail nahi.**

Zyaadatar trading strategies mein speed **helps** (jaldi execute karo, kam
slippage). HFT mein speed **hi strategy ka core hai** — agar tum 2 microsecond
slow ho, tumhara poora edge gayab ho sakta.

---

## Key characteristics

| Characteristic | Typical HFT |
|---|---|
| Holding period | milliseconds se kuch seconds (kabhi-kabhi thoda zyada) |
| Position at day-end | zyaadatar **flat** (koi overnight risk nahi) |
| Order-to-trade ratio | bahut high — bahut order place/cancel hote, thode fill hote |
| Decision automation | 100% algorithmic, koi manual click nahi |
| Infra investment | colocation, kernel bypass, FPGA/custom NICs — lesson 11 |
| Profit per trade | **chhota** — edge paisa chhota hai, volume se compound hota |
| Capital ka use | apna capital (proprietary), client ka nahi (zyaadatar) |

**Nichod:** har individual trade ka edge tiny hai. Business model chalta hai
bahut zyada aisi tiny-edge trades karne se, consistently, bina galti ke — aur
isliye reliability + latency dono equally zaroori hain (galat trade ek bar mein
poora din ka profit khaa sakta — 13-risk-systems mein yeh detail se aayega).

---

## HFT vs algo trading vs quant investing

Yeh teeno **overlap karte hain par same nahi hain** — bahut common confusion:

| | Quant investing | Algo trading (general) | HFT |
|---|---|---|---|
| Decision horizon | din/hafte/mahine | seconds se din | microseconds se seconds |
| Code kyun | signal research, backtesting scale | manual execution automate karna | **speed khud edge hai** |
| Latency sensitivity | low | medium | **extreme** |
| Example | factor-based portfolio rebalance monthly | ek bada order ko chhote pieces mein VWAP se chalana | market making, latency arbitrage |

Algo trading ek **broad umbrella** hai — koi bhi programmatic execution. HFT
uska ek **latency-sensitive subset** hai. Har algo trader HFT nahi karta; har
HFT firm algo trading kar rahi hai.

---

## Myths vs reality

| ❌ Myth | ✅ Reality |
|---|---|
| "HFT front-running hai" | Front-running (kisi doosre ke pending order ko dekh ke unke against trade karna, jab tumhe woh order legally pata nahi hona chahiye) **illegal hai** aur alag cheez hai. Legit HFT sirf public market data pe reacts karta, kisi ke private order ko "dekh" nahi sakta. |
| "HFT guaranteed-profit machine hai" | Firms fail hoti hain, din-be-din losses bhi hote (spread capture galat side pe jaa sakta — adverse selection, lesson 03). Edge tiny hai aur competitive — nayi firm profitable hona guarantee nahi. |
| "HFT market ko manipulate karta hai" | Kuch specific tactics (spoofing — fake orders place karke cancel karna price move karwane ke liye; layering) **illegal aur separately regulated hain** — woh HFT ki defining property nahi, woh fraud hai jo koi bhi kar sakta (16-regulatory-basics). |
| "Zyada speed = zyada profit, hamesha" | Diminishing returns. Agar tumhari strategy ka signal 500 µs pe update hota, 50 ns tak jaana marginal faayda nahi deta us signal ke liye — tumhara "budget" strategy se decide hota (14-latency-budget). |
| "HFT sirf ek strategy hai" | Market making, statistical/latency arbitrage, event-driven — sab alag risk profile, alag tech requirement (10-hft-strategies-overview). |

---

## Kaun karta hai

- **Proprietary trading firms** — apna capital, HFT/market-making inka core business (jaise Jane Street, Optiver, Citadel Securities, Jump Trading — industry mein widely known names, general public knowledge).
- **Designated market makers** — exchange ke saath formal agreement, badle mein rebates/obligations (lesson 09, 11).
- **Kuch hedge funds** — ek strategy sleeve ke roop mein, poora fund nahi.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — "HFT" ko ek single technique samajhna
"HFT karna hai" ek incomplete goal hai — market making alag hai, latency arb
alag hai. Pehle strategy decide hoti, phir uske hisaab se latency budget
(14) aur architecture (12) design hoti.

### Trap 2 — speed ko hi poora edge samajhna
Speed **necessary hai kai strategies ke liye, sufficient kabhi nahi.** Bina
sahi signal/model ke, fast execution sirf fast losses deta.

### Trap 3 — HFT = illegal ya unethical samajh lena
Regulated markets mein HFT firms registered, monitored entities hain (16).
Specific tactics (spoofing) illegal hain — poori category nahi.

---

## Is course mein aage

```
37 (yahan)  -> domain samjho: exchange, order book, matching, strategy concepts
38          -> market data: feed parse karna, tumhara "input"
39          -> order book: production-grade data structure
40          -> matching engine: khud ka simplified exchange banao
41          -> HFT-specific concurrency patterns
42          -> HFT networking (kernel bypass, multicast feeds)
43          -> HFT-specific optimization case studies
44          -> end-to-end project — sab jodo
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| HFT = front-running | Front-running illegal hai, alag definition; HFT sirf public data pe react karta |
| HFT = ek strategy | Market making, arbitrage, event-driven — sab alag (10) |
| Zyada speed = zyada profit hamesha | Diminishing returns; budget strategy decide karti (14) |
| HFT sirf US mein hota | NSE/BSE pe bhi active HFT hai (15) |

---

## Exercises

1. Ek trading strategy roz 50 stocks buy karti, 3 mahine hold karti, quarterly
   rebalance karti. Kya yeh HFT hai? Kyun/kyun nahi?
   <details><summary>Answer</summary>
   Nahi. Holding period (3 mahine) HFT ke typical range (ms-seconds) se
   bahut zyada hai. Yeh algo trading hai (automated) par HFT nahi — speed
   yahan competitive edge nahi hai; ek din late rebalance karne se
   strategy ka thesis nahi badalta.
   </details>

2. Ek firm ka order-to-trade ratio bahut high hai (bahut order place/cancel,
   kam fill). Kya yeh automatically spoofing/manipulation hai?
   <details><summary>Answer</summary>
   Nahi, zaroori nahi. Legit market-making mein high cancel-rate normal
   hai — quotes ko market move ke saath continuously update/replace karna
   padta (stale quote = adverse selection risk, lesson 03). Spoofing hota
   hai jab orders **place hi is niyat se hote** ki genuinely fill honi hi
   nahi (fake intent dikhana), aur regulators specifically iska pattern
   dekhte hain (intent + pattern) — sirf high cancel-rate proof nahi hai.
   </details>

3. Ek naya trader kehta "main sabse fast server khareedunga, sabse
   profitable banunga." Kya galat hai iss soch mein?
   <details><summary>Answer</summary>
   Speed bina signal/strategy ke kuch nahi. Agar tumhare paas koi edge
   (price prediction, market-making model) nahi hai, sabse fast infra bhi
   tumhe sirf "sabse fast tarike se galat" banayegi. Speed ek amplifier
   hai, source nahi.
   </details>

---

## Interview questions

1. HFT ko algo trading se differentiate karo — kya farq hai?
2. "Speed hi HFT ka poora edge hai" — is statement mein kya galat hai?
3. Spoofing kya hai, aur yeh HFT se kaise alag hai?
4. Ek firm ka order-to-trade ratio bahut high hai — yeh HFT ke liye normal
   kyun ho sakta hai?

---

## Next
→ [`02-how-exchanges-work.md`](02-how-exchanges-work.md)

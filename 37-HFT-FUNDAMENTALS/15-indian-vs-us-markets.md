# 15 — NSE/BSE vs Nasdaq/NYSE: structural fark

## Prerequisites
- [`14-latency-budget.md`](14-latency-budget.md)

## Yeh topic abhi kyun
Har venue ka rule (matching, tick-size, protocol) alag hai — 07/08/09 jo
"generic concept" tha, ab do real market families ke context mein dekhte
hain. **Important:** exact numbers (fees, exact hours, exact circuit %)
**badalte rehte hain** — yahan structural/conceptual differences hain,
live trading se pehle hamesha current official spec check karo.

---

## High-level comparison

| | India (NSE/BSE) | US (Nasdaq/NYSE + others) |
|---|---|---|
| Primary regulator | **SEBI** (Securities and Exchange Board of India) | **SEC** (Securities and Exchange Commission) + FINRA |
| Exchange count (equity) | Do dominant (NSE, BSE); NSE zyaadatar volume leta cash+derivatives dono mein | Bahut zyada fragmented — NYSE, Nasdaq, aur kai doosre "lit" exchanges + dark pools/ATSs (alternative trading systems) |
| Matching rule (typical, cash equity) | Price-time priority (07) | Price-time priority (07), venue-specific variants |
| Settlement cycle | T+1 (trade date + 1 business day) — both markets moved to T+1 in recent years; exact dates/history check current regulatory notices | T+1 (moved from T+2 more recently than India) |
| Colocation | NSE offers colocation (11) | Nasdaq, NYSE dono colocation offer karte |
| Market data protocol style | Exchange-proprietary binary protocols | **ITCH** (Nasdaq — publicly documented, market data) aur **OUCH** (Nasdaq — order entry) famous examples; NYSE ka apna "Pillar" platform hai |

---

## Market structure: consolidated vs fragmented

**India:** cash equity trading largely **do exchanges** (NSE, BSE) pe
concentrated — ek relatively simpler picture "kahan liquidity hai" ke
liye.

**US:** trading **bahut fragmented** hai — same stock 10+ lit exchanges
aur bahut saare dark pools/ATSs pe simultaneously trade ho sakta. Isse
**Reg NMS** (Regulation National Market System) jaisi rules aati hain jo
"best price sab venues mein se lena chahiye" mandate karti — jo
**smart order routing** (kis venue pe order bhejna best hai, real-time
decide karna) ko US HFT ka ek significant extra dimension banata hai jo
India mein comparatively simpler hai.

> **HFT connection:** fragmented markets mein "cross-venue arbitrage" (10)
> ka natural surface area zyada hota — same stock, alag venues pe
> temporarily alag price. Consolidated markets mein yeh specific
> opportunity kam hoti, par within-venue latency races (07) equally
> relevant rehti.

---

## Market data protocols — ek concrete example

**Nasdaq ITCH** ek widely-known, **publicly documented** binary market
data protocol hai — messages jaise "AddOrder", "OrderExecuted",
"OrderCancel" — bilkul wahi concepts jo 06 mein seekhe (add/cancel/fill),
ek specific wire format mein. **OUCH** iska order-entry equivalent hai.
38-MARKET-DATA lesson 06 mein tum ek **ITCH-style** protocol khud parse
karoge — yeh industry mein sabse widely-referenced teaching example hai
kyunki spec publicly available hai.

Indian exchanges ke apne proprietary protocols hain (broker/member ko
NSE/BSE se official specification milta membership ke through) — concept
same hai (binary, low-latency, sequenced), exact message format alag.

---

## Regulatory approach — ek high-level fark

| | India | US |
|---|---|---|
| Algo trading approval | SEBI mandates algo strategies ko exchange se **approval/registration** process se guzarna (unique algo ID tagging jaisi requirements) — 16 mein detail | Registered broker-dealers ke through, apna khud ka risk-control/testing regime (SEC/FINRA rules ke ander), approval-flow structurally alag |
| Circuit breakers/price bands | Stock-level aur market-wide dono levels pe (08) | Stock-level (limit up-limit down — LULD mechanism) aur market-wide circuit breakers dono |
| Speed bumps (11) | Kuch discussions/pilots industry mein hote rehte, exact current status verify karo | Kuch specific US venues ne speed-bump-style mechanisms explore kiye hain (venue-specific, sab nahi) |

**Yeh sab evolving hain** — regulatory frameworks regularly revise hote,
is lesson ka goal sirf "kaunse concepts exist karte, kis category mein
sochna hai" hai, current exact rule text nahi (16 mein General approach
milega, apne jurisdiction ka current official text hi authoritative
source hai).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — "HFT sirf US mein hoti hai" samajhna
NSE pe colocation, algo trading, aur significant automated participation
sab exist karte — HFT ek global phenomenon hai jahan bhi electronic
matching engines + colocation available hai.

### Trap 2 — ek venue ka rule doosre pe copy-paste karna
Tick size, price bands, matching-rule details (07) sab **venue-specific**
hain. India mein seekha rule US venue pe blindly assume karna galat
orders/logic generate karega.

### Trap 3 — fragmentation ko sirf "complexity" samajhna, "opportunity"
nahi
US-style fragmentation extra engineering complexity laati (smart order
routing) par saath hi extra strategy surface bhi (cross-venue arb, 10) —
dono sach hain.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| HFT sirf US phenomenon hai | NSE pe bhi significant automated/algo participation hai |
| Sab venues same matching rule use karte | Venue-specific — verify karo |
| US market ek single exchange hai | Highly fragmented — 10+ lit venues + dark pools |
| Regulatory rules fixed/permanent hain | Regularly revise hote — current official spec check karo |

---

## Exercises

1. Ek stock US mein 8 alag exchanges pe simultaneously trade ho sakta hai.
   Yeh kis specific HFT strategy category (10 se) ke liye extra
   opportunity/complexity dono create karta?
   <details><summary>Answer</summary>
   Cross-venue (statistical/latency) arbitrage — jitne zyada venues, utni
   zyada chance temporary price-mismatch ki, par utni zyada complexity
   bhi (sabko simultaneously monitor karna, smart order routing decide
   karna best venue kaunsa hai).
   </details>

2. Kyun ek NSE ke liye likha gaya order-validation module (tick size, lot
   size checks) bina modification ke Nasdaq pe use nahi kiya jaa sakta?
   <details><summary>Answer</summary>
   Tick size, lot size, price-band rules venue-specific hote (08) —
   hardcoded NSE values Nasdaq ke liye galat honge. Module ko configurable/
   venue-parametrized banana padta, ya har venue ke liye alag config load
   karni padti.
   </details>

---

## Interview questions

1. India aur US equity market structure ka sabse bada structural fark
   kya hai (consolidation vs fragmentation)?
2. ITCH/OUCH kya hain, aur woh kyun teaching ke liye acha reference hain?
3. Reg NMS ka high-level idea kya hai, aur yeh smart order routing ko
   kyun necessary banata US mein?
4. Kyun venue-specific rules (tick, lot, protocol) ko apne system mein
   hardcode nahi karna chahiye?

---

## Next
→ [`16-regulatory-basics.md`](16-regulatory-basics.md)

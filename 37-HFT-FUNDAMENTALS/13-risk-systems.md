# 13 — Risk systems: pre-trade checks, position limits, kill switches

## Prerequisites
- [`12-hft-system-architecture.md`](12-hft-system-architecture.md)

## Yeh topic abhi kyun
12 ne kaha "risk check kabhi skip nahi hota." Ab dekhte hain woh **kya**
check karta, aur kyun ek buggy strategy poore firm ko minutes mein doob
sakti agar risk layer na ho.

---

## Kyun risk layer zaroori hai — ek realistic scenario

```
Strategy mein ek bug hai: ek loop condition galat, order-send function
baar-baar call ho jaati (infinite retry jaisa) same signal pe.

BINA risk layer:
  1000s orders/second bhej diye jaate, sab exchange tak pahunchte,
  position runaway ho jaata, losses seconds mein accumulate.

RISK LAYER KE SAATH:
  Pehle hi order pe (ya position limit hit hote hi) reject/halt —
  damage bounded rehta.
```

Yeh koi hypothetical nahi hai — real trading history mein aisi "runaway
algo" events hui hain (publicly known incidents), aur unhi ki wajah se
risk-layer regulation (16) itna strict hai zyaadatar markets mein.

---

## Pre-trade risk checks — order bhejne se PEHLE

Har order, chahe kitna bhi "obviously safe" lage, in checks se guzarta:

| Check | Kya verify karta |
|---|---|
| **Price band (08)** | Order price ek reasonable range mein hai (fat-finger price nahi) |
| **Tick/lot validity (08)** | Order exchange-valid hai (reject-roundtrip se pehle hi pakdo) |
| **Position limit** | Yeh order fill hone se position ek pre-set max se zyada to nahi jaayegi |
| **Order size limit** | Ek single order ka size ek sane max se zyada to nahi |
| **Rate limit** | Kitne orders/second bheje jaa rahe — anomalous burst pakadna |
| **Notional/exposure limit** | Total $ (ya ₹) exposure ek max se zyada to nahi |
| **Self-trade prevention** | Apna hi buy order apne hi sell order se match to nahi karega |

**Sab checks fast honi chahiye** — yeh hot path pe hain (12 ka diagram),
isliye 36-LOW-LATENCY-CPP ki techniques (branch-free comparisons, no
allocation) yahan directly apply hoti — ek slow risk check poora tick-to-
trade budget kha sakta.

---

## Fat-finger check — human/logic error catch karna

> **Fat-finger = ek galti (human ya bug se) jisse order ka price ya size
> galti se kaafi zyada bada/galat ho jaata.**

```
Example: strategy ko qty 100 bhejna tha, ek bug se qty 100000 chala gaya
  (3 zeros extra, jaise ek decimal-point/units bug).

Fat-finger check: "kya yeh order tumhare typical order size se
  10x/100x zyada hai?" -> flag/reject karo.
```

Yeh **anomaly detection** hai — order ko apni khud ki history/typical
range ke against compare karna, sirf absolute limits ke against nahi.

---

## Kill switch — emergency stop

> **Kill switch = ek mechanism jo INSTANTLY sab naye orders block kar
> deta (aur aksar sab open orders cancel kar deta) — manual trigger se, ya
> automated condition se.**

```
Triggers (examples):
  - Manual: koi operator button dabata (kuch galat lag raha hai)
  - Automated: loss limit cross ho gaya (P&L ek threshold se neeche)
  - Automated: order rate anomalous high
  - Automated: risk system khud crash/unresponsive ho gaya (fail-safe
    default: agar risk system respond nahi kar raha, NAYE orders block
    karo — "fail closed", "fail open" nahi)
```

**Design principle:** kill switch **simple aur reliable** hona chahiye —
yeh khud ek complex system nahi honi chahiye jo fail ho sake. Zyaadatar
regulated venues bhi apna khud ka exchange-level kill switch offer karte
members ke liye (16), jo member ke apne internal switch ke upar ek extra
layer hota.

---

## "Fail closed" — ek core design principle

```
Fail OPEN  (risk system down -> orders bina check ke pass ho jaate)  <- DANGEROUS
Fail CLOSED (risk system down -> orders REJECT ho jaate by default)  <- SAFE default
```

Agar risk system khud kisi wajah se fail/timeout ho jaata, default
behavior **hamesha** "order reject karo" hona chahiye, "order allow karo"
nahi. Yeh availability se zyada safety ko priority deta — ek missed trade
ek acceptable cost hai, ek unchecked bad trade nahi.

---

## Position limits — real-time tracking

Risk layer ko **current position** pata hona chahiye (real-time, har fill
ke saath update) taaki "yeh naya order position ko limit se paar le
jaayega kya" turant answer de sake. Yeh state:
- **Fast query-able** honi chahiye (hot path pe check hota).
- **Consistent** rehni chahiye market-data aur order paths dono ke saath
  (12 ka concurrency concern — 41-HFT-CONCURRENCY mein detail).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — risk check ko "strategy ke andar" likhna
Risk logic **strategy code se separate** honi chahiye — ek independent
layer jo strategy ki correctness pe depend nahi karta. Agar risk sirf
strategy ke andar embedded hai, strategy ka koi bug risk check ko khud
bhi bypass kar sakta.

### Trap 2 — "yeh strategy hamesha safe hai, risk overhead skip karo"
Har bug "impossible" lagta hai jab tak hota nahi. Risk overhead (jo
36-style optimization se minimal kiya jaa sakta) ek insurance premium hai
— iski cost latency budget mein pehle se account honi chahiye (14), skip
karne ka option nahi.

### Trap 3 — kill switch ko complex banana
Kill switch khud fail-prone nahi honi chahiye — jitna simple, utna
reliable. Ek complex kill switch jo khud bug ka shikaar ho sakti, poore
purpose ko defeat karti.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Risk check optional/skippable hai "obvious safe" trades ke liye | Kabhi skip nahi hoti, hamesha-on |
| Fail-safe default "orders allow karo" hona chahiye (availability) | Fail CLOSED — default reject, safety > availability |
| Fat-finger sirf "typo" errors ke liye hai | Bug se generate hui anomalous size/price bhi catch karta |
| Risk logic strategy ke andar rakhna theek hai | Independent layer honi chahiye, strategy-bug-proof |

---

## Exercises

1. Ek risk system timeout ho jaata (kisi wajah se unresponsive). Default
   behavior kya hona chahiye naye orders ke liye, aur kyun?
   <details><summary>Answer</summary>
   Fail closed — naye orders **reject** hone chahiye by default jab tak
   risk system wapas responsive na ho. "Fail open" (bina check ke pass
   hone dena) catastrophic ho sakta agar exactly usi waqt koi buggy order
   attempt ho raha ho — yeh precisely woh scenario hai jisse risk system
   protect karta.
   </details>

2. Ek fat-finger check "order size > 1,000,000" jaisa hardcoded absolute
   limit use karta. Kya problem ho sakta iss approach mein?
   <details><summary>Answer</summary>
   Ek strategy jiska typical order size 50 hai, agar bug se 50,000 bhej de
   (1000x zyada, par abhi bhi hardcoded 1,000,000 se kam), yeh check catch
   nahi karega. Behtar approach: order ko us strategy/symbol ke apne
   TYPICAL range ke against compare karna (relative anomaly detection),
   sirf ek global absolute number ke against nahi.
   </details>

---

## Interview questions

1. Pre-trade risk checks ki list batao — kaunsa kya catch karta.
2. Fat-finger check kya hai, aur yeh absolute limit se kaise alag hai?
3. "Fail closed" ka matlab batao aur kyun yeh "fail open" se better default
   hai trading systems mein.
4. Kyun risk logic strategy code se separate honi chahiye?

---

## Next
→ [`14-latency-budget.md`](14-latency-budget.md)

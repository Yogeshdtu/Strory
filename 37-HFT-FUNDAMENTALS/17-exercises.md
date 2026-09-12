# 17 — Exercises: HFT fundamentals

## Prerequisites
- Poora folder `37` (`01`–`16`) + `examples/`

## Kaise use karein
- **Part A** — concept recall: quick definitions, foundational.
- **Part B** — scenario classification: diye scenario ko sahi category/rule
  se match karo (07, 09, 10 particularly).
- **Part C** — system design / gap-spotting: ek pipeline diya hai, kya
  missing hai (12, 13 se).
- **Part D** — hands-on: `examples/` chalao, numbers nikaalo, interpret
  karo.
- Folder-wide interview questions end mein.

---

## Part A — Concept recall

### A1
Maker aur taker ka fark, buy/sell direction involve kiye bina, define karo.
<details><summary>Answer</summary>
Maker = order jo book mein REST karta (turant cross/fill nahi hota) —
liquidity add karta. Taker = order jo existing resting order ko CROSS/fill
karta — liquidity consume karta. Yeh behavior-based hai (resting vs
crossing), buy/sell se independent (09).
</details>

### A2
Tick size aur lot size mein fark batao.
<details><summary>Answer</summary>
Tick size = minimum PRICE increment (price sirf iske multiples le sakta).
Lot size = minimum QUANTITY increment (order size sirf iske multiples le
sakta). Dono venue/instrument-specific hard constraints hain (08).
</details>

### A3
Adverse selection ek line mein define karo.
<details><summary>Answer</summary>
Jab tumhara resting order EXACTLY tab fill hota hai jab price tumhare
against move karne wali hoti — jisne tumhe fill kiya, use tumse zyada/
tezi information thi (03).
</details>

---

## Part B — Scenario classification

### B1
Ek venue pe, ek price level pe 5 resting orders hain alag-alag sizes ke.
Ek naya trader "main jaldi order bhejunga taaki line mein pehle rahoon"
soch raha. Yeh strategy kis matching rule mein sabse zyada faayda degi,
aur kis mein bilkul nahi?
<details><summary>Answer</summary>
FIFO (price-time priority) mein bahut faayda — "pehle aana" hi seedha
zyada fill deta (07). Pro-rata mein yeh strategy bilkul faayda nahi degi
— fill size ke proportion se milta, arrival-order se nahi.
</details>

### B2
Ek strategy do exchanges ke beech ek stock ka price-mismatch dhoondhti,
dono jagah simultaneously trade karti. Kaunsi HFT strategy category, aur
kaunsa order type (FOK ya IOC) zyada appropriate hai har leg ke liye,
kyun?
<details><summary>Answer</summary>
Cross-venue (statistical/latency) arbitrage (10). FOK zyada appropriate
— agar ek leg poora fill na ho paaye, poora us leg ka order cancel hona
chahiye (unhedged position avoid karne ke liye), IOC ka partial-fill
behavior yahan risky hai (04, 10).
</details>

### B3
Ek bade order ko market pe dikhana nahi chahte (information leak se bachna
hai — 03), par order poora bhi execute karwana hai eventually. Kaunsa
order type?
<details><summary>Answer</summary>
Iceberg — sirf ek chhota visible portion dikhta, baaki hidden reveal hota
chunks mein jaise-jaise fill hota (04).
</details>

---

## Part C — System design / gap-spotting

### C1
Ek junior engineer ka HFT pipeline design: `feed handler -> order book ->
strategy -> order gateway`. Kya missing hai, aur missing hone se kya
concretely galat ho sakta hai?
<details><summary>Answer</summary>
**Risk/pre-trade check layer** missing hai (12, 13) — strategy aur order
gateway ke beech. Bina iske: strategy ka koi bug (galat qty/price
calculate karna, infinite loop se repeated orders) seedha exchange tak
pahunch sakta, position limits bina check ke cross ho sakte, fat-finger
errors catch nahi honge. Yeh **catastrophic financial loss** ka direct
path hai (13 ka runaway-algo scenario).
</details>

### C2
Ek firm apna risk system deploy karti hai, aur decide karti hai ki agar
risk system kabhi unresponsive ho jaaye, orders bina check ke pass ho
jaayenge (taaki trading rukey nahi, "availability first"). Yeh design
decision kyun dangerous hai, aur sahi default kya hona chahiye?
<details><summary>Answer</summary>
Yeh "fail open" hai — exactly jab risk system down hai (potentially kisi
bug/issue ki wajah se), sab safety checks bhi off ho jaate, precisely
tab jab woh sabse zyada zaroori ho sakte. Sahi default: **fail closed** —
risk system unresponsive hone pe naye orders reject hone chahiye by
default (13). Ek missed trade acceptable cost hai, unchecked bad trade
nahi.
</details>

---

## Part D — Hands-on

### D1 — `01_orderbook_concept.cpp`
Chalao. Book mein ek naya ask level add karo jo best-bid se **neeche**
hai (crossing price). Predict karo: kya hoga agar `SimpleBook::add()` mein
sirf level add karta hai, matching/crossing logic implement nahi karta?
Phir code padh ke confirm karo.
<details><summary>Answer</summary>
`add()` mein sach mein koi crossing check nahi hai — yeh sirf
`side[px] += qty` karta. Ek crossing ask (best-bid se neeche) bas ek
extra (invalid-looking) level ban jaayega book mein, koi automatic
match/fill nahi hoga. Yeh jaan-boojh kar hai — yeh file sirf **book
storage concept** dikhati hai, matching engine nahi (07 ka matching logic
`04_matching_rules.cpp` mein alag se hai, aur real matching 40-MATCHING-
ENGINE mein poora banega). Production book mein `add()` khud crossing
check karke turant match/fill trigger karta.
</details>

### D2 — `02_spread_calculator.cpp`
Ek naya quote add karo jahan `bid_px == ask_px` (spread zero). Chalao.
Kya crash hota, aur agar nahi, `imbalance()`/`microprice()` ka output
meaningful hai?
<details><summary>Answer</summary>
Crash nahi hoga (koi division by qty-sum zero nahi hai jab tak dono qty
zero na hon) — `spread_bps` zero aayega, `microprice` seedha ek weighted
average ban jaayega (mid ke barabar hi hoga kyunki bid_px==ask_px). Real
market mein `bid_px >= ask_px` (crossed/locked market) rarely aur
briefly hota hai (usually turant match ho jaata) — ek healthy book mein
yeh normal steady-state nahi hai.
</details>

### D3 — `03_latency_budget.cpp`
Stage budgets ko badal ke sabka budget bahut generous (jaise 2x) kar do.
Chalao. Kya `sabse bada tail-budget overrun` calculation ka result badal
jaata? Kyun/kyun nahi is baare mein socho, phir verify karo.
<details><summary>Answer</summary>
Overrun `p99_9 - budget` hai — budgets 2x karne se zyaadatar/sab stages
ka overrun kam ho jaayega (ya negative ho jaayega, matlab ab woh budget
ke andar hain). Par **relative ranking** (kaunsa stage sabse bada
offender hai) waisi hi reh sakti hai agar p99.9 values same rahe —
`strategy decision` ka absolute p99.9 (900) baaki sabse zyada hai, to
generous budgets ke saath bhi yeh relatively sabse "tight" margin wala
reh sakta jab tak budgets proportionally na badle. Chalake exact numbers
dekho.
</details>

### D4 — `04_matching_rules.cpp`
Incoming qty ko `425` (poori resting qty) kar do. FIFO aur pro-rata dono
ka result kya hoga — same ya alag?
<details><summary>Answer</summary>
**Same** — jab incoming qty resting total ke barabar hai, dono rules mein
SAB 4 orders poore fill hote hain (koi allocation-choice bachta hi nahi,
sabko sab kuch milta). FIFO aur pro-rata ka fark sirf tab dikhta hai jab
incoming qty resting total se KAM ho (choice karni padti kisko kitna
milega) — 07 ke examples isi liye 180 (< 425) use karte hain.
</details>

---

## Folder-wide interview questions

1. HFT ko algo trading se differentiate karo.
2. Exchange ke 4 core components batao, har ek ka kaam.
3. Adverse selection kya hai, aur market maker isse kaise defend karta?
4. Market, limit, IOC, FOK, iceberg, post-only — sabka exact behavior
   difference batao.
5. Spread ko bps mein kyun measure karte?
6. Microprice formula aur uski intuition batao.
7. FIFO aur pro-rata matching ka fark, aur kaunsa venue-type mein speed
   ka edge zyada hota.
8. Tick size ko price ko `double` ki jagah integer ticks mein rakhne se
   kya relation hai?
9. Maker-taker fee model ka economic purpose kya hai?
10. Colocation kyun zaroori hai FIFO venues ke liye — physics ka connection
    batao.
11. Poora HFT pipeline diagram banao (feed handler se order gateway tak),
    har box ka kaam.
12. Pre-trade risk checks ki list batao, kaunsa kya catch karta.
13. "Fail closed" ka matlab aur kyun zaroori hai.
14. Tick-to-trade latency budget kaise banate — process ke steps.
15. Ek latency-budget table mein sab stages "thoda over" hain par ek
    "bahut zyada over" hai — priority kaise decide karoge?
16. India aur US market structure ka sabse bada fark kya hai?
17. Audit trail kyun zaroori hai, aur usko latency-budget-friendly kaise
    design karte?

---

## Next
→ [`../38-MARKET-DATA/00-README.md`](../38-MARKET-DATA/00-README.md)

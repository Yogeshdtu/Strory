# 03 — Market microstructure: liquidity, price discovery, adverse selection

## Prerequisites
- [`02-how-exchanges-work.md`](02-how-exchanges-work.md)

## Yeh topic abhi kyun
Yeh lesson **kyun** HFT strategies kaam karti hain (ya fail hoti hain) ka
foundation hai. Bina yeh samjhe, order book (06) aur strategies (10) sirf
mechanical detail lagenge — inka "matlab" nahi milega.

---

## Liquidity kya hai

> **Liquidity = kitni aasaani se, kitni price impact ke saath, tum kharid/bech
> sakte ho.**

- **High liquidity**: bahut resting orders dono taraf, bade size bhi bina
  price bahut hilaye trade ho jaate.
- **Low liquidity**: thode orders, chhota size bhi price hila deta.

Order book depth (06) hi liquidity ka direct measure hai — jitna zyada qty
top levels pe, utni zyada liquidity.

**Liquidity providers** (makers, 09) book mein orders rakh ke liquidity
create karte. **Liquidity takers** wahi orders khaa ke (market order/aggressive
limit) liquidity consume karte.

---

## Price discovery

> **Price discovery = market ka process jisse "sahi" price nikalta — buyers
> aur sellers ke continuous order flow se.**

Har naya order (particularly ek jo book ko cross karta) ek naya data point
hai "market abhi is price ko kaisa dekh rahi." Price koi central authority
set nahi karti — yeh **emergent hai**, order flow se.

Isiliye market data ka har update (38) ek "vote" hai — aur jo system sabse
tez sahi tarike se in votes ko process kar sakta, use market ka "latest
truth" sabse pehle pata chalta.

---

## Order flow

Order flow = market mein aa rahe orders ka stream — kaun buy kar raha, kaun
sell, kitni size, kitni frequency. Do broad categories:

| Type | Kya | Example |
|---|---|---|
| **Informed flow** | Kisi ke paas signal/information hai jo price move karega | Ek fund ne earnings news pe react kiya |
| **Uninformed / noise flow** | Random, portfolio rebalancing, liquidity need — koi specific signal nahi | Retail investor apna SIP order |

Market makers (09) **uninformed flow se paisa banate** (spread capture, dono
taraf trade karke) aur **informed flow se paisa khote** (agla topic:
adverse selection) — poora game yeh hai ki uninformed flow zyada capture ho,
informed flow se jaldi bacha jaaye.

---

## Adverse selection — market making ka sabse bada risk

> **Adverse selection = jab tumhara resting order fill hota EXACTLY tab jab
> price tumhare against move karne wali hoti — kyunki jisne tumhe fill kiya
> use tumse zyada pata tha.**

Example:
1. Tum bid rakhte ho 100.00 pe (buy quote).
2. Koi seller aata hai jisko pata hai (ya jiske paas fast signal hai) ki
   price abhi 99.50 girne wali hai.
3. Woh tumhara bid **turant fill karta** (sell to you at 100.00) — usse pehle
   ki market move ho.
4. Price girti hai. Tum ab 100.00 pe khareeda hua asset hold kar rahe ho jo
   ab 99.50 ka hai. **Loss.**

Yeh koi bug nahi hai — yeh market making ka **fundamental risk** hai. Poora
game hai:
- **Spread** itna wide rakho ki uninformed flow se profit adverse selection
  loss ko cover kare.
- **Quote fast update karo** jab naya info aaye (price move ho raha dikhe)
  taaki stale quote informed traders ko "free option" na de.
- Yeh dusra point hi HFT market making mein **speed** ka #1 use case hai —
  jitni jaldi tum apna stale quote hata sako, utna kam adverse selection.

> **HFT connection:** "stale quote" ka matlab hai tumhara resting order
> abhi bhi purani price pe hai jab market naya info absorb kar chuki. Speed
> = kam time stale rehna = kam adverse selection = market making profitable
> rehta.

---

## Bid-ask spread ka economic meaning

Spread (05 mein numbers ke saath) sirf ek number nahi — yeh **compensation**
hai jo maker leta hai:
1. Inventory risk uthaane ke liye (position hold karna, price move against
   ja sakti).
2. Adverse selection risk ke liye.
3. Order-processing/infra cost ke liye.

Jitna zyada **volatile** ek instrument, ya jitni kam liquidity, utna wide
spread — kyunki risk zyada.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — sochna ki market making "risk-free spread capture" hai
Sabse common misconception. Har fill ek chhota "kya yeh informed flow tha?"
bet hai. Zyaadatar time nahi (profit), kabhi-kabhi haan (loss) — net
positive rehna hi poora skill hai.

### Trap 2 — liquidity aur volume ko confuse karna
Volume = kitna trade hua (past, historical). Liquidity = ABHI kitna aasaani
se trade ho sakta (current book depth). High volume din mein bhi kisi
moment liquidity thin ho sakti (news event ke turant baad).

### Trap 3 — "fast quote update" ko sirf latency ki cheez samajhna
Yeh bhi hai (11, 14), par pehle **signal** chahiye — kab quote stale ho
gaya, yeh pata chalna khud ek (statistical) problem hai, sirf infra
problem nahi.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Market making risk-free hai | Adverse selection ek real, fundamental risk hai |
| Spread sirf profit hai | Spread = inventory + adverse-selection + processing risk ka compensation |
| Volume = liquidity | Volume historical hai, liquidity abhi ki book depth hai |
| Price koi set karta hai | Price emergent hai, order flow se discover hoti |

---

## Exercises

1. Ek market maker ka bid resting hai. Achanak ek bada sell order aata hai
   jo unka poora bid fill kar deta, aur uske turant baad price 2% girti
   hai. Yeh kya hai?
   <details><summary>Answer</summary>
   Classic adverse selection. Seller ke paas (ya uske signal ke paas)
   information thi ki price girne wali hai — usne market maker ka stale
   bid "free option" ki tarah use kiya. Maker ka fix: fast quote update
   (naya info dikhte hi bid hatao/move karo), ya wider spread us
   instrument/regime mein.
   </details>

2. Do stocks: A mein bahut volume hai par abhi book thin hai (kam resting
   orders). B mein kam volume hai par book abhi deep hai. Kis mein tumhara
   ek bada market order kam price-impact karega?
   <details><summary>Answer</summary>
   B — kyunki liquidity (current book depth) matter karta, na ki historical
   volume. A ka volume high hona iska matlab nahi ki ABHI liquidity high
   hai (ho sakta volume kisi aur time pe hua ho, ya bahut chhoti orders
   mein).
   </details>

---

## Interview questions

1. Adverse selection kya hai, aur market makers isse kaise defend karte?
2. Informed vs uninformed order flow ka fark, aur market maker ka profit
   kahan se aata.
3. Bid-ask spread "compensation" kis cheez ka hai?
4. Volume aur liquidity mein fark batao, ek example ke saath.

---

## Next
→ [`04-order-types.md`](04-order-types.md)

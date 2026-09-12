# 16 — Regulatory basics: algo approval, audit trails (overview)

## Prerequisites
- [`15-indian-vs-us-markets.md`](15-indian-vs-us-markets.md)

## Yeh topic abhi kyun
Folder 37 ka aakhri conceptual piece — HFT sirf "fast code likhna" nahi
hai, yeh ek **regulated activity** hai. **Disclaimer:** yeh overview-level
hai, legal advice nahi. Real deployment se pehle current official
regulator text aur apni firm ke compliance team se confirm karo — yeh
rules regularly revise hote hain.

---

## Kyun regulation exist karti

13 mein dekha ek buggy algo poore firm ko doob sakti. Regulation ka poora
maqsad hai — **market-wide** stability, jahan ek firm ki galti poore
market ko affect na kare:

- **Investor protection** — fair, orderly markets sab participants ke liye.
- **Systemic risk control** — ek firm ka runaway algo poori market crash na
  kare (historical incidents ne isi ki wajah se stricter rules laaye hain,
  globally).
- **Market integrity** — manipulation (spoofing, layering — 01 mein noted)
  jaisi practices ko illegal banana aur detect karna.

---

## Algo approval process — concept level

Zyaadatar regulated markets mein, ek algorithmic strategy live jaane se
pehle kisi formal process se guzarti:

```
Strategy design -> testing/simulation -> exchange/regulator ko submission
    -> review (risk controls verify hote) -> approval -> UNIQUE ID milta
    -> live trading (sab orders is ID se TAGGED hote)
```

**Unique algo ID / tagging:** har order jo ek approved algo se aata,
ek identifier ke saath tag hota jo trace karta "yeh order kis algo se
aaya." Yeh:
- Regulator ko post-trade analysis karne deta (kisne kya kiya).
- Firm ko khud bhi apne algos monitor karne mein help karta.
- Kisi incident investigation mein "kaunsa algo zimmedar tha" turant pata
  chalta.

India mein SEBI ki algo trading framework mein yeh concept explicitly hai
(exact process, thresholds, aur category — retail vs institutional algo —
current SEBI circulars mein hai, verify karo). US mein similar spirit ke
controls broker-dealer risk-management rules ke through aate (SEC Rule
15c3-5 jaisa "market access rule" — pre-trade risk controls mandate karta
broker-dealers ke liye, direct-market-access context mein).

---

## Audit trail — sab kuch log hona chahiye

> **Audit trail = har order, modification, cancellation, aur fill ka
> complete, tamper-evident, timestamped record.**

```
Order sent    -> logged (timestamp, price, qty, algo ID)
Order acked   -> logged
Order modified -> logged
Order filled/cancelled -> logged
```

**Kyun zaroori:**
- **Regulatory requirement** — regulators demand kar sakte "iss trade ka
  poora history dikhao" kisi bhi investigation mein.
- **Internal debugging** — 13 ka fat-finger incident hua? Audit trail se
  exactly pata chalta kya hua, kab, kis order se.
- **Dispute resolution** — trade disputes mein authoritative record.

> **HFT connection:** audit-trail logging ek hot-path cheez hai (har order
> ke saath log likhna) — par 36-LOW-LATENCY-CPP ki spirit yahan bhi
> lagti: logging bhi allocation-free, low-latency hona chahiye (17-syscall-
> avoidance jaisi techniques — batched/async writes, ring-buffer se
> aggregator thread tak, 35/16-production-measurement mein isi pattern ka
> ek chhota bhai). Compliance requirement ko hot-path latency budget se
> "free" nahi maan sakte — usko bhi engineer karna padta.

---

## Kill switch — regulatory mandate bhi hai (13 se)

13 mein kill switch ek engineering safety mechanism tha. Kai regulated
venues **exchange-level kill switch bhi mandate karte** members ke liye —
matlab yeh sirf "achhi engineering practice" nahi, kayi jagah **compliance
requirement** bhi hai.

---

## Position limits — sirf risk nahi, regulatory bhi

13 ka "position limit" internal risk control tha. Regulators bhi
**market-wide position limits** set karte kuch instruments pe (jaise kuch
derivatives contracts) — taaki koi ek entity market ko excessively
influence na kar sake. Firm ka internal limit typically regulatory limit
se **tighter** hota hai (extra safety margin).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — regulation ko "engineering se alag" samajhna
Jaisa upar dikha, kai regulatory requirements (audit trail, kill switch,
position limits) **directly system design** ko affect karte — yeh
compliance team ka "unrelated paperwork" nahi, architecture ka part hai
(12 ka diagram).

### Trap 2 — audit-trail logging ko "free" maan lena
Logging bhi latency budget consume karti agar naively synchronous/
blocking implement ki jaaye. Isse async/batched design karna padta.

### Trap 3 — "yeh course mein padha, ab main compliant hoon" soch lena
Yeh overview hai, **legal/compliance advice nahi**. Real deployment se
pehle current official regulatory text aur qualified compliance
professional se confirm hamesha zaroori hai — rules jurisdiction aur time
ke saath badalte hain.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Regulation sirf paperwork hai, code se unrelated | Audit trail, kill switch, position limits — sab system design ko affect karte |
| Audit-trail logging "free" hai | Hot-path cost hai, engineer karni padti (async/batched) |
| Ek jagah seekha rule sab jagah apply hota | Jurisdiction/venue-specific, verify current text |
| Algo approval ek-baar ka kaam hai | Ongoing compliance — monitoring, re-approval kabhi zaroori |

---

## Exercises

1. Kyun ek "unique algo ID" tag har order pe useful hai, sirf regulator
   ke liye nahi, firm ke apne liye bhi?
   <details><summary>Answer</summary>
   Firm khud bhi apne multiple algos ko monitor/debug karti — agar ek
   specific algo mein anomalous behavior dikhe (13 ka fat-finger jaisa),
   ID se turant pata chal jaata kaunsa algo zimmedar hai, bina poore
   order-flow ko manually trace kiye.
   </details>

2. Audit-trail logging ko synchronous (har order ke saath blocking disk
   write) implement kiya gaya. Iska latency budget (14) pe kya asar hoga,
   aur behtar approach kya hoga?
   <details><summary>Answer</summary>
   Synchronous disk write hot path pe seedha add hoti — potentially
   microseconds ka syscall/IO cost (17-syscall-avoidance), jo tick-to-
   trade budget ko significantly bada kar sakta. Behtar: log entry ko
   ek lock-free ring buffer (15) mein push karo (bahut fast, allocation-
   free), ek alag/lower-priority thread us buffer se read karke async
   disk/network ko flush kare — hot path sirf ek chhoti push operation
   dekhta.
   </details>

---

## Interview questions

1. Regulation kyun exist karti HFT context mein (3 broad reasons)?
2. Algo ID tagging kya hai, aur yeh kis-kis ko fayda deta hai?
3. Audit trail logging ko latency-budget-friendly kaise design karoge?
4. Kill switch aur position limits — kaunse "sirf engineering" hain,
   kaunse "regulatory mandate bhi" ho sakte?

---

## Next
→ [`17-exercises.md`](17-exercises.md)

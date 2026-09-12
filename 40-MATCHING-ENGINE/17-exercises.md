# 17 — Exercises: matching engine (extensions + challenges)

## Prerequisites
- Poora folder `40` (`01`–`16`) + `examples/`

## Kaise use karein
- **Part A** — concept recall.
- **Part B** — design/scenario questions.
- **Part C** — hands-on: `examples/` modify karo, predict karo, verify.
- **Part D** — extension challenges (thoda harder, code likhna padega —
  naye order types, auctions).
- Folder-wide interview questions end mein.

---

## Part A — Concept recall

### A1
Ek Limit order jo partially fill hota hai, uska leftover kis SIDE pe
rest hota — apni original side pe, ya jis side se match hua usi pe?
<details><summary>Answer</summary>
Apni ORIGINAL side pe (BUY ka leftover BID banta, ASK nahi) — jis side
se match hua woh sirf OPPOSITE side hai jise woh cross kar raha tha (02).
</details>

### A2
`Trade.price` field kis order ki price hota — aggressor ki ya resting ki?
<details><summary>Answer</summary>
Resting (maker) ki — "maker sets the price" convention, aggressor ko
apni limit se BEHTAR (kabhi badtar nahi) price milti (07).
</details>

### A3
FOK ka naive (self-trade-unaware) precheck kis scenario mein GALAT
guarantee de sakta?
<details><summary>Answer</summary>
Jab STP (self-trade prevention) kuch resting orders ko SKIP (CancelOldest)
ya poora matching ABORT (CancelNewest/CancelBoth) karta — naive sum un
skipped/unreachable quantities ko bhi COUNT kar leta, jabki actual match
unhe kabhi fill nahi karta (06).
</details>

---

## Part B — Design / scenario questions

### B1
Ek naya engineer STP implement karta hai par usse `match_against()` ke
BAAD check karta (fill_qty compute ho chuke, trade already push_back ho
chuka, phir "agar self hai to undo karo" jaisi koi cleanup try karta).
Kya problem hai is approach mein?
<details><summary>Answer</summary>
Ek trade ek baar EMIT (aur potentially downstream broadcast) hone ke
baad "undo" karna real systems mein possible hi nahi hota (09/10's
determinism/event-sourcing guarantee todta — ek event jo already log ho
chuka use retroactively cancel nahi kar sakte). STP check MATCH SE PEHLE
hona chahiye (08's actual design), taaki trade EMIT hi na ho agar self-
trade detect ho.
</details>

### B2
Tumhare matching engine ko MULTI-symbol banana hai (5000 symbols). Kaisa
architecture design karoge (11 se connect karo)?
<details><summary>Answer</summary>
Har symbol ka apna INDEPENDENT `MatchingEngine` instance, alag thread/
core pe ("sharding by symbol," 11) — koi shared state symbols ke beech
nahi, isliye determinism (09) har symbol ke ANDAR intact rehti, aur
poora system embarrassingly-parallel scale karta symbols ke across.
</details>

### B3
Ek auditor tumse poochta "prove karo ki Monday 2:35 PM ka matching sahi
tha." Tumhare paas kya hona CHAHIYE taaki yeh answer ho sake (10 se)?
<details><summary>Answer</summary>
Us din ka poora COMMAND LOG (event source) — Monday 2:35 tak ke saare
Submit/Cancel commands, EXACT order mein. Engine determinism (09) ki
guarantee ke saath, us log ko REPLAY karke EXACT SAME trades/state
reconstruct kiya jaa sakta — yeh "proof" hai, "trust me" nahi.
</details>

---

## Part C — Hands-on

### C1
`04_self_trade_prevention.cpp` mein ek NAYA scenario add karo:
`CancelOldest` mode mein, do consecutive self-orders ho SAME level pe
(SELF, SELF, phir OTHER). Predict karo output, phir verify karo.
<details><summary>Answer</summary>
Dono SELF orders TOUCH hote hi ek-ek karke REMOVE honge (`continue` loop
mein), koi match unse NAHI hoga. Sirf OTHER wala order match hoga (agar
incoming ki baaki qty enough ho). Yeh 08's CancelOldest logic ka natural
extension hai — jitne bhi consecutive self-orders milein, sab skip hote
jaate jab tak koi non-self na mile ya book/qty khatam ho.
</details>

### C2
`06_engine_tests.cpp` mein ek naya test add karo: "Replace ek order ko
DIFFERENT SIDE pe" (jaisa purana BUY tha, naya order SELL hai, same id-
chain). Kya yeh valid hai engine ke design mein?
<details><summary>Answer</summary>
Haan, `replace()` = `cancel(old_id)` + `submit(new_order)` — `new_order`
ka `is_buy` kuch bhi ho sakta, koi constraint nahi ki side SAME rahe.
Real venues mein yeh kabhi-kabhi disallow kiya jaata (side-change ko
"naya order" treat karte, "replace" nahi) — is course ka simplified
engine aisi restriction nahi lagata, jo ek design-choice hai worth noting.
</details>

### C3
`07_engine_fuzz.cpp` ka seed 5-10 baar badlo. Kya `disagree` ya
`self_trade_leaks` kabhi 0 se zyaada aata?
<details><summary>Answer</summary>
Nahi hona chahiye (agar implementation sahi hai) — 09's determinism ka
matlab hai ki agar koi bug exist karta, WOH specific input-pattern jo
usse trigger karta HAR baar (jab bhi hit ho) consistently reproduce
hoga, "kabhi kabhi flaky" nahi hoga.
</details>

---

## Part D — Extension challenges

### D1 — Post-only order type
37/04 mein `Post-only` cover hua tha (order sirf tab accept ho jab woh
TURANT cross na kare). Ek `OrderType::PostOnly` add karo `matching_engine.hpp`
mein — `submit()` mein isse HANDLE karo: agar incoming order price cross
karti opposite best se, POORA `Rejected` return karo (koi partial match
bhi nahi), warna normal Limit jaisa REST karo.

### D2 — Iceberg order
37/04 mein `Iceberg` cover hua tha (bada order, chhota visible portion).
`Order` struct mein `Qty display_qty` field add karo. Jab is order ka
visible portion poora fill ho jaaye, agla chunk AUTO-REVEAL ho (NAYI
seq/priority ke saath — 37/04 ke "revealed chunk naya arrival hai" rule
follow karke). Hint: `add_resting()` aur `match_against()` dono touch
karne padenge.

### D3 — Opening/closing auction (batch matching)
Real exchanges din ki SHURUAAT/END pe "auction" mode use karte —
continuous matching (jo yeh folder abhi karta) ki jagah, ek WINDOW mein
saare orders COLLECT karo (koi immediate match nahi), phir EK SINGLE
"clearing price" compute karo jo MAXIMUM volume match kare, aur SAB
eligible orders us EK price pe EK SAATH match karo. Design karo (code
zaroori nahi): `AuctionEngine` class ka API kaisa hoga, aur clearing-price
compute karne ka algorithm (hint: cumulative supply/demand curves ka
intersection).

### D4 — Latency-optimized resting storage
16's exercise 2 ka poora implementation: `matching_engine.hpp`'s
`bids_`/`asks_`/`index_` ko 39's V3-style (tick-indexed flat array +
intrusive arena-linked-list + tombstone flat hash) se REPLACE karo.
Benchmark (`08_engine_bench.cpp`) re-run karo, before/after numbers
compare karo. Kaunse order-type ka latency SABSE zyaada improve hoga,
aur kyun (16's mechanism-analysis se predict karo pehle, phir verify)?

### D5 (challenging) — Decrement-and-cancel STP mode
Kuch real venues ek 4th STP mode support karte: "**Decrement and
Cancel**" — jab self-trade detect ho, DONO orders (incoming aur resting)
ki qty us OVERLAP-AMOUNT se GHATA do (jaise woh EK trade hua ho, PAR koi
Trade event generate NAHI hota, koi participant ko fill NAHI milta —
sirf dono ki remaining qty kam ho jaati). Implement karo `StpMode::DecrementAndCancel`.
Yeh `CancelBoth` se KAISE alag hai (hint: qty-partial-consumption vs
poora-cancel)?

---

## Folder-wide interview questions

1. Order book (39) aur matching engine (40) ka scope-fark.
2. Price-time priority ka poora algorithm (nested loops) explain karo.
3. Limit/Market/IOC/FOK — charon ka exact behavior, side-by-side.
4. FOK precheck STP-aware kyun hona ZAROORI hai — exact bug scenario
   batao.
5. Teeno STP modes ka behavior (resting-fate + incoming-continuation).
6. "Maker sets the price" convention aur price improvement.
7. `OrderStatus`'s 5 states, terminal vs non-terminal.
8. Determinism ki exact definition, aur non-determinism ke sources
   (matching engine context mein).
9. Event sourcing pattern — kyun matching engines is par depend karte.
10. Kyun ek single-symbol ka matching multi-threaded NAHI hota (sharding-
    by-symbol se contrast karo).
11. Teen testing layers (scripted/invariant/cross-implementation) —
    har ek kis bug-class pakadta.
12. `RefEngine`'s FOK precheck strategy `MatchingEngine`'s formula se
    kyun DELIBERATELY alag hai?
13. Benchmark numbers mein Limit order ka p99.9 itna zyaada kyun tha
    apne p50 se — mechanism batao (39 se connection).
14. Poora `submit()` flow trace karo, duplicate-id se lekar final status
    tak.

---

## Next
→ [`../41-HFT-CONCURRENCY/00-README.md`](../41-HFT-CONCURRENCY/00-README.md)

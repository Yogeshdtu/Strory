# 06 — IOC aur FOK: semantics aur implementation

## Prerequisites
- `05-partial-fills.md`
- `37-HFT-FUNDAMENTALS/04-order-types.md` (IOC/FOK ka basic definition)

## Yeh topic abhi kyun

**Yeh is folder ka sabse deep-technical lesson hai.** IOC seedha hai
(Market ke jaisa hi remainder-void logic, bas price-limited). FOK NAHI
seedha hai — usse ek **precheck** chahiye (poora fill hoga ya nahi, MATCH
karne se PEHLE pata karna), aur jab woh precheck **self-trade prevention**
se combine hota hai, ek genuine, subtle correctness bug possible hai jo
is folder banate waqt actually mila aur fix kiya gaya. Poori kahani neeche.

---

## IOC -- Market ka price-limited version

```
IOC BUY 100 x100, book asks: 100 x30, 105 x400
  -> sirf 100x30 CROSSES (105 > 100, cross nahi karti)
  -> 30 fill, baaki 70 VOID (rest NAHI hota)
```

Code mein: IOC `match_against` mein NORMAL price-check follow karta
(Market ke ulat), par `submit()` ke end mein Market jaisa hi behave karta
— remainder REST nahi hota:

```cpp
if (incoming.type == OrderType::Limit && !stp_aborted) {
    add_resting(incoming);   // IOC yahan NAHI aata
    ...
}
return {OrderStatus::Cancelled, ...};   // Market AUR IOC dono yahan girte
```

Simple. Ab FOK.

---

## FOK -- "poora ya kuch nahi," ek PRECHECK maangta hai

FOK ka contract: **poora qty TURANT fill ho, ya ZERO trades ho (poora
reject)** — koi partial fill valid outcome NAHI hai. Iska matlab hai
FOK ka decision (fill karo ya reject karo) match START karne se PEHLE
lena padta — kyunki agar tum match SHURU kar do aur beech mein qty khatam
ho jaaye (book mein kaafi nahi tha), tum ab "undo" nahi kar sakte (trades
already emit ho chuke, doosre participants ko already pata chal chuka).

```cpp
if (incoming.type == OrderType::FOK) {
    const Qty avail = ...;   // "agar abhi match karta to kitna fill hota?"
    if (avail < incoming.qty) {
        return {OrderStatus::Rejected, {}};   // MATCH SHURU HI NAHI HOTA
    }
}
// yahan tak pahunche matlab precheck ne "haan poora fill hoga" bola --
// ab normal match_against() chalta hai, jaisa Limit
```

**Precheck ka naive (GALAT) version:** "crossable price levels ke `total_qty`
sum karo, `>= target` check karo." Yeh SIMPLE lagta hai aur ZYAADATAR case
mein sahi bhi hai. Par...

---

## Bug: naive precheck STP ke saath GALAT guarantee de sakta

**Scenario** (`06_engine_tests.cpp`'s `test_fok_stp_interaction()`):

```
Book ASK 100: [id1=SELF x40 (FIFO first), id2=OTHER x30 (FIFO second)]
Incoming: FOK BUY 100 x50, participant=SELF, stp=CancelOldest
```

**Naive precheck**: level 100 ka `total_qty` = 40+30 = 70. `70 >= 50` →
**PASS**, match shuru hota.

**Actual match jo hota** (STP=CancelOldest ka matlab: same-participant
resting order TOUCH hote hi CANCEL/REMOVE hota, match NAHI hota, incoming
AGLE order pe try karta):
```
id1 (SELF) touch hota -> CancelOldest fires -> id1 REMOVE (no fill!)
id2 (OTHER) touch hota -> NORMAL match -> fill 30
Book ab khaali (level gone) -> loop ruk jaata
incoming.qty = 50 - 30 = 20  (BAAKI HAI, ZERO nahi!)
```

Incoming FOK hai, `Limit` nahi — toh remainder `submit()` ke end mein
**VOID** ho jaata (Cancelled status), par **30 qty ka trade already ho
chuka hai** — yeh ek **FOK CONTRACT VIOLATION**: "poora ya kuch nahi"
order **PARTIALLY** fill ho gaya! Naive precheck ne galat guarantee di
thi — usne `id1` ki 40 qty ko count kiya jo ACTUAL match mein kabhi
fillable thi hi nahi (STP use skip karne wala tha).

> **Yeh bug is folder banate waqt REAL mein likha gaya, phir test likhte
> waqt (06) pakda gaya** — exactly CLAUDE.md ka Rule 2/4 ka spirit:
> surprising result chhupao mat, teach karo. Naive precheck "usually"
> sahi hota (jab STP fire hi nahi hota, ya sirf `None`/`CancelBoth`-style
> jahan koi skip-and-continue nahi), par yahan specifically GALAT tha.

---

## Fix: STP-mode-AWARE precheck

`matching_engine.hpp`'s `available_qty()` **exact SAME priority-order aur
skip/abort rules** follow karta jo real `match_against()` follow karega
— matlab woh EK "dry walk" hai (bina mutate kiye) jo predict karta match
ACTUALLY kitna fill karega:

```cpp
template <class OppMap>
static Qty available_qty(const OppMap& opp_side, bool is_buy, Price price, Qty target,
                          ParticipantId self, StpMode stp) {
    Qty sum = 0;
    for (const auto& kv : opp_side) {          // best-to-worst level order -- SAME as match
        if (!crosses(price, kv.first)) break;
        const PriceLevel& level = kv.second;
        if (stp == StpMode::None) {
            sum += level.total_qty;
        } else {
            for (const auto& o : level.orders) {   // FIFO order -- SAME as match
                if (o.participant == self) {
                    if (stp == StpMode::CancelOldest) continue;   // skip, AAGE badhta (jaisa real match)
                    return sum;   // CancelNewest/CancelBoth -- real match yahin ABORT karta
                }
                sum += o.qty;
            }
        }
        if (sum >= target) break;
    }
    return sum;
}
```

Same scenario, is precheck se: level 100, `stp=CancelOldest`, id1(SELF)
mile toh `continue` (count nahi hota), id2(OTHER)'s 30 count hota. Sum =
**30**. `30 < 50` → **REJECTED** — sahi! Book bilkul touch nahi hota,
zero trades, FOK ka contract intact.

**Trade-off:** yeh precheck `O(level size)` ban jaata jab STP set ho
(saara level ka FIFO list scan karna padta, sirf `total_qty` field padhna
kaafi nahi). Yeh ACCEPTABLE hai kyunki FOK+STP dono ek saath **rare
combination** hai — matching ka hot path (plain Limit/Market/IOC bina STP
ke) is extra cost se bilkul touch nahi hota.

---

## Verification

`06_engine_tests.cpp`'s `test_fok_stp_interaction()` isi exact scenario
ko hardcode karta, dono outcomes assert karta (reject jab truly kam ho,
fill jab exactly enough ho). `07_engine_fuzz.cpp` isi invariant ko
**automatically** 3774 random FOK orders pe check karta (across saare STP
modes) — ek independent, COMPLETELY different implementation (dry-run
copy-and-simulate strategy, RefEngine mein) ke against — zero violations.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — FOK ko "Limit + IOC + extra check" samajhna
FOK IOC se fundamentally alag hai kyunki uska decision match-SHURU-hone-
SE-PEHLE lena padta (precheck), IOC ka decision match-KHATAM-hone-KE-BAAD
lena padta (jo bhi fill ho gaya woh keep karo). Yeh ordering-fark hi FOK
ko implementation-wise harder banata.

### Trap 2 — precheck ko real-match logic se DUPLICATE/DIVERGE hone dena
Agar precheck aur real match ki price-crossing ya STP logic kabhi bhi
subtly ALAG likhi jaaye (jaisa naive-sum version tha), precheck ka
"guarantee" jhoothi ho sakta. Yeh dono EK common set of rules follow
karne chahiye — is folder mein comments explicitly is invariant ko flag
karte.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| FOK precheck = "book mein kitna total hai" | STP-mode-aware "kitna ACTUALLY match hoga agar abhi try karte" |
| FOK aur IOC same hain, bas ek extra flag | Structurally alag -- FOK PRE-decide karta, IOC POST-decide karta |
| Yeh combination (FOK+STP) itna rare hai ki ignore kar sakte | Rare hona iska matlab "test nahi karna" nahi -- fuzz ise specifically target karta |

---

## Hands-on

```bash
./build.ps1 fast 40-MATCHING-ENGINE/examples/02_order_types.cpp
./build.ps1 fast 40-MATCHING-ENGINE/examples/06_engine_tests.cpp
./build.ps1 fast 40-MATCHING-ENGINE/examples/07_engine_fuzz.cpp
```

---

## Exercises

1. Kyun FOK ka decision match-shuru-hone-se-PEHLE lena padta, IOC ka
   BAAD mein le sakte?
   <details><summary>Answer</summary>
   FOK "poora ya kuch nahi" guarantee karta -- agar match beech mein
   ruk jaaye (poora na ho paaye), trades already committed/broadcast ho
   chuke hote, undo NAHI ho sakte. IOC ko koi aisi guarantee nahi deni --
   "jo bhi fill hua woh keep karo" hamesha ek VALID outcome hai IOC ke
   liye, isliye woh match ke baad simple decide kar sakta.
   </details>

2. `StpMode::CancelBoth` ke saath FOK precheck ka behavior `CancelOldest`
   se kaise DIFFERENT hai (formula mein)?
   <details><summary>Answer</summary>
   `CancelOldest` self-order ko SKIP karta aur AAGE count karta rehta
   (`continue`). `CancelBoth` (aur `CancelNewest`) self-order milte hi
   turant `return sum` kar deta -- kyunki real match bhi wahin ABORT ho
   jaata (koi aur level try nahi hoti), isliye precheck ko bhi wahin
   rukna chahiye, aage ka kuch bhi count NAHI karna chahiye chahe woh
   non-self ho.
   </details>

3. Yeh bug (naive FOK precheck + STP) is folder banate waqt kaise pakda
   gaya?
   <details><summary>Answer</summary>
   Test likhte waqt (`06_engine_tests.cpp`'s
   `test_fok_stp_interaction()`) -- ek deliberately-crafted scenario jahan
   naive sum (70) target (50) se zyaada tha par actual fillable qty (30,
   STP ke baad) target se KAM thi. Yeh exactly wahi discipline hai jo
   14-testing-matching-engine.md mein cover hota -- scripted EDGE-CASE
   tests, sirf happy-path nahi.
   </details>

---

## Interview questions

1. FOK ka precheck kya karta, aur woh Market/IOC se structurally kaise
   alag hai?
2. Ek naive FOK precheck (self-trade-unaware) kis specific scenario mein
   galat guarantee de sakta?
3. STP-aware precheck ka O() complexity trade-off kya hai, aur kyun
   acceptable hai?

---

## Next
→ [`07-trade-events.md`](07-trade-events.md)

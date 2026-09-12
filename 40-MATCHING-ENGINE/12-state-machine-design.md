# 12 — Order state machine, valid transitions

## Prerequisites
- `11-single-threaded-design.md`

## Yeh topic abhi kyun

Poore folder mein humne `OrderStatus` (`New`, `PartiallyFilled`, `Filled`,
`Cancelled`, `Rejected`) baar-baar dekha — yeh lesson unhe ek formal
**state machine** ke roop mein ek jagah consolidate karta.

---

## States aur transitions

```
                    submit()
                       |
                       v
              +-----------------+
              |   (matching      |
              |    happens)      |
              +-----------------+
                /       |        \
               /        |         \
        qty==0     Limit,leftover>0   type!=Limit, leftover>0
         |          !stp_aborted        OR stp_aborted
         v                |                  |
    +---------+           v                  v
    | Filled  |    +-------------+    +-------------+
    +---------+    | New / Partially|  | Cancelled   |
    (terminal)     | Filled (RESTING)|  +-------------+
                   +-------------+    (terminal)
                        |     ^
                cancel()|     | (aur match ho sakta,
                        v     |  future submit()s se)
                  +-----------+
                  | Cancelled |
                  +-----------+
                  (terminal)

    Duplicate id / FOK precheck fail:
              submit() -----> Rejected (terminal, kabhi engine mein
                               ENTER hi nahi hua)
```

| State | Terminal? | Matlab |
|---|---|---|
| `New` | Nahi | Resting hai, zero fills abhi tak |
| `PartiallyFilled` | Nahi | Resting hai, kuch fill ho chuka, resting jaari |
| `Filled` | **Haan** | Poora fill, book se GONE |
| `Cancelled` | **Haan** | Remainder VOID (rest nahi hua) -- IOC/Market leftover, explicit cancel, ya STP |
| `Rejected` | **Haan** | Engine mein ENTER hi nahi hua -- duplicate id, ya FOK precheck fail |

**5 states, 3 terminal.** `New`/`PartiallyFilled` "resting" ki DO sub-states
hain — dono se `Filled` (future match), ya `Cancelled` (future cancel()
ya future match+STP-abort) tak pahunch sakte, koi seedha `Rejected` nahi
(Rejected sirf submit()-TIME pe decide hota, kabhi resting order ka
LATER outcome nahi ban sakta).

---

## `find_resting()` ka contract state-machine se derive hota

```cpp
const Order* find_resting(OrderId id) const;   // nullptr agar id resting NAHI
```

Ek id `find_resting()` se milega **sirf** jab uska order abhi `New` ya
`PartiallyFilled` state mein hai. Teeno terminal states (`Filled`,
`Cancelled`, `Rejected`) — `nullptr`. Yeh direct consequence hai: sirf
resting orders `index_` mein hote (add_resting() sirf Limit-leftover pe
call hota).

---

## `status` field vs `trades` list -- state-machine ke saath is bhram ko clear karna

06 mein hint diya tha: `submit()`'s return `status` sirf FINAL/terminal-
OR-resting state batata, `trades` EXACTLY kya fill hua batata. Ek
`Cancelled` status ke saath BHI non-empty `trades` ho sakte (IOC/Market
partial-fill-then-void) — status "kuch bhi fill nahi hua" ka guarantee
NAHI deta, sirf "remainder ka fate" batata. Yeh ek COMMON confusion point
hai jo state-machine diagram explicit karne se clear ho jaata.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — invalid transition allow karna
Jaise ek `Filled` (terminal) order pe `cancel()` call karna — engine ko
graceful `false` return karna chahiye (jaisa `cancel()` already karta,
`index_.find()` fail hota kyunki Filled order `index_` mein hai hi nahi).
Koi crash NAHI, sirf "no-op" — state machine ka discipline yeh guarantee
karta ki terminal states se koi transition possible hi nahi hai (structurally,
data-structure design se, na ki runtime check se).

### Trap 2 — `Rejected` ko `Cancelled` samajh lena
Dono "order fail ho gaya" jaise lagte, par semantically ALAG: `Rejected`
= engine mein kabhi ENTER hi nahi hua (koi matching attempt bhi nahi hua,
zero trades GUARANTEED). `Cancelled` = engine mein enter hua, MATCH bhi
ho sakta tha (kuch trades bhi ho sakte hain), sirf REMAINDER void hua.
Downstream systems (risk, accounting) inhe alag treat karte — `Rejected`
ka matlab "kuch bhi nahi hua," `Cancelled` ka matlab "shayad kuch hua ho."

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| `status` batata "kitna fill hua" | `trades` batata; `status` sirf remainder ka FATE batata |
| `Rejected` aur `Cancelled` same hain | `Rejected` = kabhi enter nahi hua; `Cancelled` = enter hua, remainder void hua |
| `New`/`PartiallyFilled` terminal states hain | Nahi -- dono se `Filled` ya `Cancelled` tak future events se pahunch sakte |

---

## Exercises

1. Ek order `PartiallyFilled` state mein resting hai. Do CHEEZEIN ho
   sakti agle "future" mein -- kya kya?
   <details><summary>Answer</summary>
   (1) Ek future incoming order isse poora match kar de -> `Filled`
   (terminal). (2) Koi explicit `cancel()` call ho jaaye, ya ek future
   STP-triggering order isse remove kar de -> `Cancelled` (terminal).
   Dono paths poora order ko book se HATA dete.
   </details>

---

## Interview questions

1. `OrderStatus` ke 5 states aur unke terminal/non-terminal classification
   batao.
2. `Rejected` aur `Cancelled` ka semantic fark kya hai?
3. `status` field aur `trades` list ka relationship explain karo -- kyun
   dono chahiye, ek se kaam kyun nahi chalta?

---

## Next
→ [`13-building-the-engine.md`](13-building-the-engine.md)

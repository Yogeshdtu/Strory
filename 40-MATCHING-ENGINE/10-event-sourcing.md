# 10 — Event log, replay, state reconstruction

## Prerequisites
- `09-determinism.md`

## Yeh topic abhi kyun

Determinism (09) ek PROPERTY hai. **Event sourcing** ek PATTERN hai jo
us property ko **use** karta — "state ko directly store mat karo, uski
jagah har CHANGE (event) ko ek append-only log mein store karo; state
kabhi bhi log ko replay karke reconstruct kar sakte ho."

---

## Pattern

```
   Traditional ("state-based"):        Event sourcing:
   +-----------------+                 +-----------------------------+
   | current state   |                 | Event 1: Submit(order A)    |
   | (book snapshot)  | <- OVERWRITE   | Event 2: Submit(order B)    |
   +-----------------+     each time   | Event 3: Cancel(order A)    |
                                        | Event 4: Submit(order C)    |
                                        | ...                          |
                                        +-----------------------------+
                                             |
                                             v  REPLAY (apply har event
                                                sequentially ek fresh
                                                engine pe)
                                        current state (RECONSTRUCTED,
                                        kabhi bhi, kisi bhi machine pe)
```

`engine_workload.hpp`'s `Cmd` (Submit/Cancel) hi is course ka "event"
type hai. `05_event_sourcing.cpp`'s `run_log()` function EXACTLY yeh
karta: `Cmd`s ki ek list leta, sequentially ek engine pe apply karta.

---

## Kyun powerful hai

| Fayda | Kaise |
|---|---|
| **Crash recovery** | Engine crash ho gaya? Log se REPLAY karo, exact SAME state wapas mil jaata (09's determinism ISI ko possible banata) |
| **Audit trail** | "Order X ka poora lifecycle dikhao" -- log mein saare events already hain, koi separate tracking nahi chahiye |
| **Time-travel debugging** | "State jab Y events ho chuke the (N events tak)" -- log ka PREFIX replay karo |
| **Multi-node replication** | Backup nodes SAME log consume karke primary ke SAME state pe pahunch jaate (09's guarantee) |
| **Testing** | Bugs ko REPRODUCE karna trivial -- jo log se bug hua, wahi log wapas replay karo |

---

## `05_event_sourcing.cpp` mein proof

```cpp
static std::vector<Trade> run_log(const std::vector<Cmd>& cmds, MatchingEngine& engine) {
    std::vector<Trade> all_trades;
    for (const auto& c : cmds) {
        if (c.kind == CmdKind::Submit) {
            auto r = engine.submit(c.order);
            for (const auto& t : r.trades) all_trades.push_back(t);
        } else {
            engine.cancel(c.cancel_id);
        }
    }
    return all_trades;
}

// SAME cmds, DO FRESH engines:
MatchingEngine engine_a; auto trades_a = run_log(cmds, engine_a);
MatchingEngine engine_b; auto trades_b = run_log(cmds, engine_b);
// -> trades_a == trades_b (field-by-field), engine_a's state == engine_b's state
```

20000 commands pe measured: **4342 trades**, dono engines mein
**byte-identical**, `resting_count=5245` dono mein, `best_bid`/`best_ask`
match. Yeh koi "claim" nahi hai — yeh CODE hai jo har run pe khud verify
karta.

---

## Snapshots -- replay ko FASTER banane ka optimization

Poori history se replay karna (din bhar ke lakhon events) SLOW ho sakta.
Real systems isliye periodically **snapshots** lete (poori state ek point
pe save karte) — recovery tab "latest snapshot + uske BAAD ke events"
replay karta, poore din ke SAB events nahi.

```
[Snapshot @ 10:00 AM] -> [events 10:00-10:05] -> [events 10:05-10:10] -> ...
Recovery @ 10:07: latest snapshot (10:00) load karo + 10:00-10:07 ke events replay karo
                  (poore din ke events NAHI, sirf 7 minute ke)
```

Yeh course is optimization ko IMPLEMENT nahi karta (scope se bahar), par
concept clear hona chahiye — 13-book-snapshots (39-ORDER-BOOK ka concept)
se related, connect karta.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — event log mein "derived" data store karna
Log mein sirf RAW commands (Submit/Cancel) store hone chahiye, koi
DERIVED info (jaisa "yeh trade hua") nahi — derived data replay se KHUD
generate ho jaata (deterministically). Agar tum derived data bhi store
karo aur woh kabhi INCONSISTENT ho jaaye raw log se, ambiguity create
hoti "sach kya hai."

### Trap 2 — event log ko MUTATE karna
Event sourcing ka core promise **append-only** hai. Agar tum purane
events edit/delete karo, poori replay-guarantee (determinism) tootg
jaati — purane replays ab NAYE result denge.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Event sourcing sirf "logging" hai | Log hi SOURCE OF TRUTH hai, state uska DERIVED cache hai |
| Snapshots event log ki jagah lete | Snapshots optimization hain (faster recovery), log FUNDAMENTAL truth rehta |
| Determinism na ho to bhi event sourcing kaam karta | Nahi -- 09's determinism EXACTLY yeh possible banata; iske bina replay HAR baar ALAG result de sakta |

---

## Hands-on

```bash
./build.ps1 fast 40-MATCHING-ENGINE/examples/05_event_sourcing.cpp
```

---

## Exercises

1. Kyun event log "append-only" hona ZAROORI hai, sirf "convention" nahi?
   <details><summary>Answer</summary>
   Agar log mutate ho sake, poori replay-based-recovery/audit guarantee
   khatam ho jaati -- kisi ne "kya hua" ka record change kar diya, ab
   replay ORIGINAL truth nahi de sakta. Yeh cryptographic audit logs
   (append-only ledgers) ke concept se bhi related hai.
   </details>

---

## Interview questions

1. Event sourcing pattern explain karo -- traditional state-based
   approach se contrast karo.
2. Snapshots ka purpose kya hai, aur woh event log ko REPLACE karte ya
   COMPLEMENT?
3. `05_event_sourcing.cpp` kya EXACTLY prove karta hai, aur kyun yeh
   "proof" hai "claim" nahi?

---

## Next
→ [`11-single-threaded-design.md`](11-single-threaded-design.md)

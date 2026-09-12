# 14 — Unit tests, scenario tests, property-based tests

## Prerequisites
- `13-building-the-engine.md`

## Yeh topic abhi kyun

`06_engine_tests.cpp` mein **43 tests** hain — yeh lesson unki
categories, philosophy, aur "kaunsa test kis BUG class ko pakadta" explain
karta.

---

## Teen categories

### 1. Scripted unit tests -- specific scenario, specific expected outcome

```cpp
static void test_basic_match_and_fifo() {
    e.submit(make_limit(1, 1, false, 100, 30));
    e.submit(make_limit(2, 1, false, 100, 20));
    auto r = e.submit(make_limit(3, 2, true, 100, 40));
    check(r.trades[0].resting_id == 1, "FIFO: id=1 fills FIRST");
    ...
}
```

Har function ek NAMED scenario hai — "yeh specific input diya, yeh
specific output expect karta." Fast, readable, failure PRECISELY batata
kya galat hua ("FIFO order galat hai," "STP mode ne resting order nahi
hataya," etc.).

`06_engine_tests.cpp` mein 8 aisi functions: basic-match+FIFO,
leftover-rests-own-side, market-sweep, IOC, FOK, **FOK+STP interaction**
(06 ka central bug-test), STP-modes, edge-ops (duplicate/cancel-nonexistent/
replace).

### 2. Invariant/property-based checks (07 mein, fuzzer ke andar)

Scripted tests "is EXACT input pe yeh EXACT output" check karte. Invariant
checks kuch DIFFERENT karte — "**kisi bhi** input pe, yeh property KABHI
nahi TOOTNI chahiye":

```cpp
// FOK invariant -- kisi bhi FOK order ke liye, HAMESHA sach:
(status == Filled && filled == orig_qty) || (status == Rejected && trades.empty())
// "partial FOK" jaisi teesri possibility KABHI nahi honi chahiye
```

```cpp
// STP invariant -- STP requested ho to KABHI self-trade nahi:
if (order.stp != None) {
    for (trade : trades) assert(trade.aggressor_participant != trade.resting_participant)
}
```

Yeh checks **thousands of RANDOM inputs** pe automatically chalte
(15-fuzzing.md), scripted tests se KAAFI zyaada coverage dete — kyunki
tumhe HAR specific scenario sochna nahi padta, sirf "yeh property kabhi
nahi tootni chahiye" define karna padta.

### 3. Cross-implementation equivalence (07 mein, `RefEngine` ke against)

Ek COMPLETELY independent, deliberately-simple implementation (`RefEngine`
— plain `vector` + linear scan, `07-engine_fuzz.cpp`) SAME commands pe
chalao, output COMPARE karo. Agar `MatchingEngine` (optimized, complex)
mein koi subtle bug ho, `RefEngine` (simple, "obviously correct" hone
ke kaafi close) shayad usse na kare — disagreement FLAG ho jaata.

---

## Kaunsa test kis bug-class ko pakadta

| Bug class | Kaunsa test pakadta |
|---|---|
| FIFO order galat | Scripted (`test_basic_match_and_fifo`) |
| Naive FOK precheck (STP ke saath) | Scripted (`test_fok_stp_interaction`) -- HAND-CRAFTED specific scenario, kyunki random fuzzing se yeh exact combination ATANA luck pe depend karta |
| Rare edge-case combinations jo koi socha hi nahi | Fuzzing (invariant checks, RANDOM inputs se) |
| Ek implementation ka subtle logic-bug jo dusri mein NAHI hai | Cross-implementation equivalence |
| Dangling reference / UB | Koi bhi test JAB sanitizers (35/15) ke saath chale, ya jab crash ho (assertion) |

**Sabse powerful combination: scripted test EXACT known-tricky-scenario
ko pakadta (jo tumne khud socha), fuzzing UNKNOWN-tricky-scenarios ko
pakadta (jo tumne NAHI socha).** Dono chahiye — ek dusre ka substitute
nahi hai.

---

## `check()` helper -- simple, NO framework

```cpp
static void check(bool cond, const std::string& name) {
    ++g_run;
    if (cond) { ++g_pass; std::printf("  PASS  %s\n", name.c_str()); }
    else      { std::printf("  FAIL  %s\n", name.c_str()); }
}
```

Koi external testing library (GTest, Catch2) NAHI use ki — 39's exact
pattern repeat kiya. Zero dependencies, exit code (`g_pass == g_run`)
CI-friendly hai, output human-readable hai.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — sirf happy-path scripted tests likhna
Agar tumne sirf "normal" scenarios test kiye hote (poora fill, koi STP
nahi, koi FOK nahi), `test_fok_stp_interaction`-jaisa bug KABHI pakda
nahi jaata — yeh test DELIBERATELY ek tricky combination target karta
hai, "typical" nahi.

### Trap 2 — invariant checks ko scripted tests ka REPLACEMENT samajhna
Fuzzing "kya galat ho sakta" nahi batata, sirf "kuch galat hua" batata
(aur exact failing input, jo phir SCRIPTED test ban sakta). Dono
complementary hain, ek dusre ka substitute nahi.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| 43 scripted tests "kaafi" hain | Fuzzing (15) ADDITIONAL coverage deta jo scripted tests miss kar sakte |
| Fuzzing scripted tests ki jagah le sakta | Fuzzing "kuch galat hai" batata, scripted tests "EXACTLY yeh galat hai" batate -- dono chahiye |
| Testing framework (GTest) zaroori hai | Simple `check()` helper kaafi hai chhote projects ke liye |

---

## Hands-on

```bash
./build.ps1 fast 40-MATCHING-ENGINE/examples/06_engine_tests.cpp
```

---

## Exercises

1. `test_fok_stp_interaction` ek FUZZING se pakde jaane wala test hai,
   ya sirf SCRIPTED se? Kyun?
   <details><summary>Answer</summary>
   Ismein DONO ka role hai -- humne isse pehle SCRIPTED test se HAND-
   CRAFT kiya (specific quantities jo exactly naive-vs-aware precheck ko
   diverge karayein), PAR fuzzer bhi apni RANDOM generation se isi tarah
   ke combinations kabhi-kabhi hit karta (07's "FOK invariant violated"
   check agar kabhi trigger hoti, woh fuzzing se pakda gaya scenario hota,
   jise phir ek NAYA scripted test banaya jaata).
   </details>

---

## Interview questions

1. Teen testing categories batao, har ek kis tarah ka bug pakadta.
2. `test_fok_stp_interaction` jaisa test scripted hona kyun BEHTAR hai
   sirf fuzzing pe depend karne se (is specific case mein)?
3. Cross-implementation equivalence testing kya hai, aur yeh kis tarah
   ka bug specifically pakadta jo scripted tests miss kar sakte?

---

## Next
→ [`15-fuzzing.md`](15-fuzzing.md)

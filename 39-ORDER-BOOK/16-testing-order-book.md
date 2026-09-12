# 16 — Testing the order book: unit tests, invariants, fuzzing

## Prerequisites
- Poora folder `39` (`01`–`15`)
- `examples/08_orderbook_tests.cpp`, `examples/09_orderbook_fuzz.cpp`

## Yeh topic abhi kyun
Teen independent implementations, complex state machines (arena,
intrusive lists, open-addressed hash) — **testing sirf "nice to have"
nahi, yeh is poore folder ki correctness-claim ka proof hai.**

---

## Teen layers of testing yahan use hue

```
1. Scripted scenarios     -- specific, hand-designed cases (08)
2. Cross-version equivalence -- V1/V2/V3 ko EK doosre ke against verify (07, 08)
3. Fuzzing + reference model -- random + edge-case ops, independent
                                 O(n)-but-obviously-correct model ke against (09)
```

Har layer **alag class of bug** pakadta.

---

## Layer 1: scripted scenarios

```cpp
template <class Book>
void test_price_time_priority(Book& book, const char* tag) {
    book.add(10, true, p, 100);
    book.add(11, true, p, 50);
    book.add(12, true, p, 75);
    const auto ids = book.ids_at_price(true, p);
    check(ids[0] == 10 && ids[1] == 11 && ids[2] == 12, ...);
}
```

Yeh **specific, human-designed** cases hain — "main jaanta hoon yeh
behavior CORRECT hona chahiye, verify karo." Har ek EK behavior test karta
(FIFO order, partial-cancel-keeps-position, full-execute-removes,
replace, duplicate-reject, nonexistent-id-reject). **58 tests, teeno
versions pe, sab PASS.**

**Yeh template function EK baar likha, teeno versions pe chalaya jaata**
(12's "consistent interface" ka direct payoff) — bina teen alag test-suites
likhe.

---

## Layer 2: cross-version equivalence (checked AFTER EVERY OP)

```cpp
for (op : ops) {
    v1.apply(op); v2.apply(op); v3.apply(op);
    // HAR op ke baad -- sirf end mein NAHI
    assert(v1.order_count() == v2.order_count() == v3.order_count());
    assert(v1.best_bid() == v2.best_bid() == v3.best_bid());
}
```

**"Har op ke baad" check karna zaroori hai** — agar sirf END mein check
karte, ek intermediate divergence (jo baad mein "accidentally" wapas
align ho jaaye) miss ho sakta. 4 alag seeds, 5000 ops har ek — **20000
checkpoints, sab match.**

**Yeh testing ka ek powerful pattern hai:** teen INDEPENDENT
implementations (alag data structures, alag code) same specification
follow karte — agar sab agree karte, yeh V1-specific ya V3-specific bug
hone ka chance bahut kam kar deta (agar EK implementation mein bug hota,
bahut chance hai woh doosron se DISAGREE karega).

---

## Layer 3: fuzzing — deliberately EDGE CASES inject karna

`generate_workload()` (poore folder mein use hua) **sirf valid ops**
banaata — real-world messiness (duplicate ids, nonexistent-id operations,
over-execute) kabhi test nahi hoti usse. **`09_orderbook_fuzz.cpp`
specifically yeh inject karta:**

```cpp
// ~10% ops deliberately "invalid":
//  - duplicate add (ek id jo already add ho chuki)
//  - reduce/remove ek RANDOM (aksar nonexistent) id pe
//  - over-execute (qty > order ki remaining qty -- clamp-behavior test)
//  - replace ek already-gone old_id pe
```

Har op **independent REFERENCE model** (simple, `O(n)`, obviously-correct
by inspection) ke against bhi check hota — na sirf V1/V2/V3 aapas mein.

```
=== Fuzz result: 30000 ops (2986 edge-case injected) ===
agree: 30000 / 30000  disagree: 0
final order_count: ref=6842 v1=6842 v2=6842 v3=6842
```

**Zero disagreements, ~10% edge-case-injection ke saath bhi.**

---

## Kyun reference model + cross-version dono zaroori hain

```
Cross-version equivalence: pakadta agar EK version doosron se DIFFER
                            karti (par agar SAB TEENO same galat cheez
                            karte -- jaise shared misunderstanding of a
                            requirement -- yeh MISS ho jaata).

Reference model:            ek INDEPENDENTLY-designed, simple model se
                            compare karta -- agar sab teeno same galat
                            assumption share karte (jaise "over-execute
                            error hona chahiye" vs "clamp hona chahiye"),
                            reference model (jo simplest/most-obvious
                            interpretation follow karta) yeh pakad sakta.
```

**Dono layers complementary hain** — na koi ek dusre ka replacement.

---

## Real bug jo testing ne pakda (11 se recap, poora context yahan)

```
Bug: BookV3 test mein best_bid() call hua bina has_bid() check kiye,
     aur test-prices V3's bounded-range se BAHAR the (add() silently
     failed) -- assertion crash (out-of-bounds array access).

Kaise pakda gaya: TESTING KE DAURAAN, na production mein. Test likhte
                  waqt hi crash mila (array-bounds assertion, strict
                  build flags ki wajah se).

Fix: (a) test prices ko V3's range ke andar redesign kiya (BASE-relative
     constants), (b) is poori class of bug (`best_bid()` bina precondition-
     check) ko explicitly document kiya (01, 11).
```

**Yeh testing ka poora point hai** — production mein pakadne se PEHLE,
development-time pe pakadna. Strict compile flags (`-Wall -Wextra` etc.)
+ runtime assertions (array bounds checks) + explicit test-writing sab
ne mil ke isse jaldi surface kiya.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — sirf "happy path" test karna
Agar sirf `generate_workload()`-jaisa VALID-ops-only testing hota, over-
execute/duplicate-add/nonexistent-id jaisi cases kabhi test nahi hoti —
production mein pehli baar hit hoti (worst possible jagah).

### Trap 2 — cross-version equivalence ko "end mein hi check karo" (perf
ke liye)
Har-op-check thoda slower hai, par **kis specific op pe divergence hui**
yeh turant batata — end-mein-check sirf "kahin divergence hui" batata,
kahan nahi.

### Trap 3 — reference model ko bhi "optimize" karne ki koshish
Reference model ki poori value uski **simplicity** mein hai (O(n) scan,
obviously correct by inspection) — use bhi fast banane ki koshish karna
uska purpose defeat karta (agar reference khud bug-prone ho jaaye, poora
comparison meaningless).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Cross-version agreement akela kaafi hai | Reference model bhi chahiye (shared-mistake ko pakadne ke liye) |
| Sirf valid-op workload testing kaafi hai | Fuzzing (edge cases) alag bugs pakadta |
| Testing "documentation ke baad" ka kaam hai | Yahan testing ne khud ek DOCUMENTATION-worthy bug (precondition) surface kiya |
| Reference model ko bhi fast/optimized hona chahiye | Simplicity hi uski value hai |

---

## Exercises

1. Agar V1, V2, V3 teeno "over-execute" (qty > remaining) ko GALAT tareeke
   se handle karte (jaise sab teeno negative qty allow kar dete, koi
   clamp nahi), kya cross-version equivalence check yeh pakadta?
   <details><summary>Answer</summary>
   Nahi — agar teeno SAME (galat) tareeke se behave karte, wo aapas mein
   AGREE karenge (sab negative qty dikhayenge), cross-version check PASS
   ho jaata. Sirf REFERENCE MODEL (jo correctly clamp karta) yeh
   divergence pakadta — yehi wajah hai reference model zaroori hai, sirf
   cross-version nahi.
   </details>

2. `best_bid()`/`has_bid()` precondition bug production mein (fuzz/test
   ke bina) kaise manifest hota?
   <details><summary>Answer</summary>
   Ek edge case mein (jaise book poori tarah khaali ho jaaye kisi busy
   period ke baad, ya koi price accidentally range se bahar chali jaaye)
   `best_bid()` call hoga bina precondition-check ke — release build mein
   (assertions off) yeh silently GALAT memory padhega (UB), crash NAHI
   karega turant, balki kahin aur galat data flow karega — bahut zyada
   debug karna mushkil hota production mein, is bug ko development mein
   hi pakadna kahin behtar tha.
   </details>

---

## Interview questions

1. Teen testing layers batao (scripted, cross-version, fuzz+reference),
   har ek kis class of bug pakadta.
2. Cross-version equivalence "har op ke baad" check karna kyun end-mein-
   check se better hai?
3. Reference model kyun zaroori hai jab cross-version equivalence already
   hai?
4. Is folder mein testing ne kaunsa real bug pakda, aur kaise?

---

## Next
→ [`17-exercises.md`](17-exercises.md)

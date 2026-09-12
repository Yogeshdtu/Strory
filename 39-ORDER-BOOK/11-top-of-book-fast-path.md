# 11 — Top-of-book fast path: best bid/ask, cached

## Prerequisites
- [`10-price-time-priority-impl.md`](10-price-time-priority-impl.md)

## Yeh topic abhi kyun
Best bid/ask **sabse zyada query hone wala** state hai — har strategy
decision (37/10), har spread/microprice calculation (37/05) isi se shuru
hoti. Teeno versions mein iska cost bahut alag hai.

---

## V1/V2: "best" hamesha container ke sabse pehle element mein hai

```cpp
// V1
Price best_bid() const { return bids_.begin()->first; }   // map's begin() -- O(log n) sa amortized? NAHI -- O(1)!

// V2
Price best_bid() const { return bids_.front().price; }     // vector's front() -- O(1)
```

**Yeh dono ACTUALLY O(1) hain** — `std::map::begin()` internally leftmost
node ka pointer CACHE karta (red-black tree implementation detail, koi
traversal nahi lagti har call pe), aur `vector::front()` seedha `[0]`
access hai. Container khud "best = extreme" invariant maintain karta.

---

## V3: EXPLICIT caching zaroori hai

Flat array mein "best" **koi natural extreme position nahi hai** — kaunsa
index best hai, yeh **track karna padta**:

```cpp
int best_bid_idx_ = NUM_LEVELS;   // sentinel: "koi live bid nahi"
int best_ask_idx_ = NUM_LEVELS;

Price best_bid() const { return center_ - 1 - best_bid_idx_; }
```

**Add pe update (O(1)):**
```cpp
if (was_empty && idx < best_bid_idx_) best_bid_idx_ = idx;
```
Naya level agar SABSE close-to-center (numerically smallest idx) hai,
best ban jaata — ek comparison, O(1).

**Level khaali hone pe update (usually O(1), worst-case O(k)):**
```cpp
if (level.count == 0 && idx == best) {
    int i = idx + 1;
    while (i < NUM_LEVELS && levels[i].count == 0) ++i;
    best = i;
}
```

**Honest nuance:** yeh scan **sirf tab** chalta jab exact BEST level
khaali ho jaaye (rare event compared to total ops — Add/reduce dono
zyaadatar EXISTING, non-best levels ko touch karte). Aur typical case
mein agla non-empty level **paas hi** hota (dense order books mein
consecutive empty levels rare). Isliye yeh **"amortized/typical O(1),
worst-case O(NUM_LEVELS)"** hai — na ki strictly-guaranteed O(1) har call
pe. Measured numbers (14) is claim ko support karte (V3's tail bhi tight
hai), par yeh **guarantee nahi**, empirical observation hai is workload
ke liye.

> Yeh 37/24's spirit hai: kabhi "O(1) guaranteed" mat bolo jab tak
> mathematically true na ho — "usually fast, occasionally scans" zyada
> honest hai.

---

## ⚠️ Precondition: `has_bid()`/`has_ask()` check pehle — REAL BUG STORY

Is folder banate waqt exactly yeh mila:

```cpp
// Test likha gaya tha:
BookV3 book(10000, 1024);
book.add(1, true, 100, 50);   // GALAT price -- center se bahut door (100 << 10000-256)
check(book.best_bid() == 100, "...");   // add() SILENTLY FAILED (range check),
                                          // best_bid_idx_ abhi bhi sentinel (256)
                                          // best_bid() ne bid_levels_[256] access kiya
                                          // -- OUT OF BOUNDS -- assertion crash
```

**Root cause:** test-writer (yahan, mujhe) ne test-price values (100, 99,
105, ...) choose kiye bina V3's bounded-range constraint (07) yaad rakhe.
`add()` ne correctly `false` return kiya (silent reject, range check ne
kaam kiya) — par test ne return value CHECK nahi kiya, aur seedha
`best_bid()` call kar diya **bina `has_bid()` verify kiye.**

**Fix:** test prices ko `BASE`-relative (center ke paas) redesign kiya
gaya (`08_orderbook_tests.cpp`), aur is poori class of bug ko yaad rakhne
ke liye — **`best_bid()`/`best_ask()` ka precondition hamesha document +
enforce karo:**

```cpp
if (book.has_bid()) {
    Price p = book.best_bid();   // SAFE
}
```

**Yeh exactly woh cheez hai jo testing (16) pakadti hai** — ek precondition
jo "zyaadatar case mein" (jab prices sahi range mein hon) silently sahi
lagta, par edge case mein crash deta. `01`'s trap section mein yeh already
note hua tha; yahan iska **mechanism** (kyun specifically V3 mein array-
bounds crash) samjha.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `best_bid_idx_` ko sirf Add pe update karna, remove pe nahi
Agar remove/reduce ke baad best-level-khaali-hone-pe scan-forward logic
miss ho jaaye, `best_bid_idx_` **stale** reh jaata (pehle se ja-chuke
level ko point karta) — subsequent `best_bid()` galat price dega.

### Trap 2 — sentinel value (`NUM_LEVELS`) ko valid index samajhna
`best_bid_idx_ == NUM_LEVELS` ka matlab "koi bid nahi" hai — is value ko
seedha array-index ki tarah use karna (jaisa upar ka bug) out-of-bounds
access hai.

### Trap 3 — V1/V2 ke `begin()`/`front()` ko "O(1) hone ka guarantee nahi"
samajhna
Confusion ulta bhi ho sakta — kuch log sochte "tree traversal lagegi
begin() ke liye" — nahi, `std::map` ye specifically O(1) rakhta (cached
leftmost-node pointer), standard-guaranteed complexity hai.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| V3 mein best-tracking "guaranteed O(1)" hai | Typical O(1), worst-case O(NUM_LEVELS) scan (honest) |
| `best_bid()` hamesha safe call hai | Precondition: `has_bid()` true (crash-story se seekha) |
| `map::begin()` slow hota (tree traversal) | O(1) hai — leftmost node cached |
| Sentinel value ek "valid" index hai | Nahi — `has_*()` se pehle guard karo |

---

## Exercises

1. `best_bid_idx_` sentinel `NUM_LEVELS` (256) hai. Agar galti se
   `bid_levels_[best_bid_idx_]` bina check kiye access ho, kya hota
   (concrete failure mode)?
   <details><summary>Answer</summary>
   `std::array<LevelV3, 256>::operator[](256)` — index 256, array size
   256 (valid indices 0-255) — out-of-bounds. Debug builds mein
   (assertions on, jaisa is folder ka strict-flags build) turant assertion-
   fail crash. Release build (`-DNDEBUG`) mein UB — silently kisi aur
   memory ko "level" maan ke padhega, corrupt/garbage data dega, shayad
   crash nahi (worse — silently galat).
   </details>

2. V1's `best_bid()` O(1) kyun hai jab ki tumne socha hoga tree ka
   traversal lagega?
   <details><summary>Answer</summary>
   `std::map` (aur `std::set`) apne implementation mein leftmost node ka
   pointer maintain karta rehta (ya root se O(height) navigate karta jo
   practically bhi fast hai, par standard `begin()` complexity O(1)
   GUARANTEE karta) — is se `begin()` call HAMESHA constant time hai,
   size ya height se independent.
   </details>

---

## Interview questions

1. V1/V2 mein best-bid O(1) kaise milta bina explicit tracking ke?
2. V3 mein best-tracking explicit kyun karni padi?
3. Level-khaali-hone-pe scan-forward "usually O(1)" hai — exact honest
   claim kya hai (guarantee nahi, kyun)?
4. `best_bid()`/`has_bid()` precondition bug ka poora mechanism batao
   (isi folder mein mila real example).

---

## Next
→ [`12-add-cancel-modify-execute.md`](12-add-cancel-modify-execute.md)

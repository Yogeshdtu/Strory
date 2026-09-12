# 13 — Book snapshots: generation, consistency

## Prerequisites
- [`12-add-cancel-modify-execute.md`](12-add-cancel-modify-execute.md)
- `38-MARKET-DATA/03-snapshots-vs-incremental.md`

## Yeh topic abhi kyun
38/03 mein snapshot **CONSUME** karna seekha (naya consumer join karta,
poori book state ek baar leta). Yeh lesson ulta hai: **agar tumhara khud
ka book kisi ko snapshot DENA** ho (internal downstream consumer, UI,
recovery/backup, ya recovery-request ka jawab), use kaise generate karo.

---

## Snapshot = poori book state, ek point-in-time pe

```
L1 snapshot: {best_bid_px, best_bid_qty, best_ask_px, best_ask_qty}
L2 snapshot: [{price, total_qty}, ...] har level ke liye, best se worst
L3 snapshot: [{price, [order_id, order_id, ...]}, ...] -- FIFO order bhi
```

Konsa level chahiye, use-case pe depend karta (38/02 se yaad karo).

---

## L2 snapshot generate karna (V3 example)

```cpp
struct L2Entry { Price price; Qty qty; };

std::vector<L2Entry> snapshot_bids_l2(const BookV3& book, int max_levels) {
    std::vector<L2Entry> out;
    // best_bid_idx_ se aage scan karo (worse price ki taraf), non-empty
    // levels collect karo. Yeh O(levels scanned) hai -- max_levels tak
    // rukta agar caller ko poori depth nahi chahiye.
    ...
}
```

**Yeh O(NUM_LEVELS) ya O(max_levels) hai** — `best_bid()` (O(1), 11) se
**fundamentally alag** operation. Snapshot generation **HOT PATH pe kabhi
nahi honi chahiye** — yeh ek control-plane operation hai (periodic timer
se trigger, ya explicit request pe — jaisa 38/13 ka snapshot channel).

---

## L3 snapshot — `ids_at_price()` (10) ka natural use

```cpp
struct L3Entry { Price price; std::vector<OrderId> order_ids; };  // FIFO order

std::vector<L3Entry> snapshot_bids_l3(const Book& book) {
    std::vector<L3Entry> out;
    for (each non-empty bid level, best to worst) {
        out.push_back({price, book.ids_at_price(true, price)});
    }
    return out;
}
```

L3 snapshot **sabse expensive** hai (har order individually copy hota) —
zyaadatar systems L2 snapshot use karte routine consumers ke liye, L3
sirf specific debugging/audit purposes ke liye.

---

## Consistency — single-threaded (abhi) vs multi-threaded (41 preview)

**Is folder ke books single-threaded hain** — snapshot generation ke
dauraan koi doosra thread book ko mutate nahi kar sakta (kyunki koi doosra
thread hai hi nahi abhi). Isliye consistency **trivially guaranteed** hai
— jo bhi state snapshot ke shuru mein thi, wahi poori tarah capture hoti.

**Multi-threaded context mein (41-HFT-CONCURRENCY preview):** agar EK
thread book ko mutate kar rahi ho (market data se) aur DOOSRI thread
simultaneously snapshot le rahi ho, **torn read** ka risk hota — snapshot
ka half hissa "purani" state se, half "nayi" state se (agar mutation
beech mein ho jaaye). Solutions (preview, poora 41/28 mein):
- **Lock** (mutex) — simple, par hot-path mutation ko block kar sakta.
- **Seqlock** (28-LOCK-FREE mein seekha) — writer odd/even sequence
  number, reader retry agar sequence beech mein badal jaaye. Snapshot-
  jaisi "occasionally read, frequently write" pattern ke liye ideal
  (28's measured ~80-100x faster reads than `shared_mutex`).
- **Copy-on-write / double-buffering** — writer naya buffer banata,
  atomically pointer swap karta; readers purane buffer se consistent
  snapshot lete bina lock ke.

---

## Snapshot ka use: recovery (38/13 se connection)

Agar tumhara book **downstream consumers** ko apna khud ka feed deta
(jaise ek internal normalized-book service), aur woh consumer gap detect
kare (38/04) ya naya join kare (38/03), unhe **tumhare snapshot channel**
se resync karna padta — bilkul wahi pattern jo 38/03 mein "upstream
exchange" ke liye tha, ab tumhara book "upstream" ban gaya hai kisi aur
ke liye.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — snapshot ko hot-path pe generate karna
O(levels)/O(orders) cost ek per-message operation ke saath compare karo
(jo O(1) hai, 07/09/11) — snapshot ko turant per-tick trigger karna poora
latency-budget (37/14) bigaad sakta.

### Trap 2 — multi-threaded context mein bina synchronization ke snapshot
lena
"Book to sirf padh rahe hain, write nahi kar rahe" — galat assumption
agar EK doosri thread simultaneously WRITE kar rahi ho. Torn reads silent
hote (koi crash nahi, bas galat data) — 27-ATOMICS ka "data race = UB"
principle yahan bhi.

### Trap 3 — L3 snapshot ko routine operation banana
L3 sabse expensive hai — routine/frequent consumers ke liye L2 kaafi hota
zyaadatar cases mein (38/02 ka trade-off).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Snapshot O(1) operation hai | O(levels) ya O(orders) — best_bid() se fundamentally alag |
| Single-threaded book mein consistency "extra kaam" chahiye | Trivially guaranteed (koi concurrent mutation possible nahi) |
| Multi-threaded snapshot bina lock/seqlock ke safe hai "sirf reading hai" | Concurrent write ke saath torn read risk — synchronization zaroori |
| L3 snapshot hamesha zaroori hai | L2 zyaadatar consumers ke liye kaafi, L3 sirf specific needs ke liye |

---

## Exercises

1. Ek snapshot generation function `O(NUM_LEVELS)` hai (poori array scan
   karta, chahe zyaadatar levels khaali hon). Isko `O(non-empty levels)`
   kaise banaoge?
   <details><summary>Answer</summary>
   `best_bid_idx_` se shuru karke, sirf `count > 0` wale levels collect
   karo aur agle non-empty level tak "jump" karo (bilkul 11's scan-forward
   logic jaisa) — bajaay poori array linearly scan karne ke, jo zyaadatar
   khaali slots pe waste hoti. Trade-off: thoda complex logic, par sparse
   books (thodi si active price levels) ke liye significant improvement.
   </details>

2. Multi-threaded context mein (41 preview) seqlock snapshot ke liye
   mutex se better kyun ho sakta?
   <details><summary>Answer</summary>
   Snapshot ek "occasionally read, frequently write" pattern hai (book
   constantly market data se update hoti, snapshot kabhi-kabhi request
   hoti). Seqlock (28-LOCK-FREE) writer ko KABHI block nahi karta (writer
   hamesha turant likh sakta, sirf apna sequence number badhaata) — reader
   consistency verify karta (retry agar beech mein write hua), par writer
   ka hot path bilkul unaffected rehta. Mutex writer ko bhi block kar
   sakta agar reader lock hold kar raha ho — hot-path ke liye undesirable.
   </details>

---

## Interview questions

1. L1, L2, L3 snapshot mein fark (38/02 se connect karke)?
2. Snapshot generation ka complexity kya hai, aur kyun hot-path pe nahi
   honi chahiye?
3. Single-threaded vs multi-threaded snapshot-consistency ka fark batao.
4. Seqlock snapshot-context mein mutex se kyun behtar fit hai?

---

## Next
→ [`14-measuring-v3.md`](14-measuring-v3.md)

# 10 — Price-time priority: FIFO order preserved, aur PROVEN

## Prerequisites
- [`09-order-id-lookup.md`](09-order-id-lookup.md)
- `37-HFT-FUNDAMENTALS/07-price-time-priority.md`

## Yeh topic abhi kyun
37/07 mein FIFO matching **concept** seekha. Ab dekhte hain teeno versions
mein yeh **guarantee kaise implement** hoti hai — aur ek accessor jo ise
directly, testably prove karta.

---

## FIFO guarantee kahan se aati hai

Teeno versions mein, **container ka natural append-order** hi FIFO order
hai:

| Version | Kaunsa mechanism preserve karta arrival-order |
|---|---|
| V1 | `std::list::push_back` — naya order hamesha end mein |
| V2 | `std::deque::push_back` — same |
| V3 | Intrusive list `tail` pointer — naya order hamesha `level.tail` ke baad |

**Koi explicit "sort by timestamp" step nahi hai** — arrival order khud-
ba-khud preserve hoti hai kyunki har structure "append at the end" pattern
follow karta. Yeh important hai: agar tum kabhi ek unordered container
(jaise plain `std::unordered_set<Order>`) use karte, FIFO guarantee
apne-aap TOOT jaati.

---

## `ids_at_price()` — FIFO ko directly dekho

Sab teeno headers mein ek accessor hai (test/demo ke liye):

```cpp
std::vector<OrderId> ids_at_price(bool is_buy, Price price) const;
```

Yeh us level ke sab live orders **FIFO order mein** (jo pehle aaya, pehle)
return karta.

```
08_orderbook_tests.cpp:
  book.add(10, true, p, 100);   // pehla
  book.add(11, true, p, 50);    // doosra
  book.add(12, true, p, 75);    // teesra

  ids_at_price(true, p) == [10, 11, 12]   -- exact arrival order
```

Yeh **teeno versions pe identical result** deta (08 mein verified) — FIFO
guarantee implementation-independent hai, jab tak container "append at
end, preserve order" property rakhta.

---

## Partial reduce FIFO position NAHI badalta

```cpp
book.add(20, true, p, 100);
book.reduce(20, 30);   // partial cancel/execute
```

Order 20 apni **QUEUE POSITION KEEP** karta (sirf qty ghatti). 37/07 se
yaad karo: partial fill/cancel order ko "line ke peeche" nahi bhejta —
sirf uski size chhoti hoti. Sirf **poora remove/delete** (`remove()`,
`reduce()` jab qty 0 tak pahunche) us order ko queue se hataata.

**Modify/Replace ALAG hai:** `replace()` **naya order_id** deta (37/07
ka "modify = naya time-priority" — cancel-replace) — is naye order ki
FIFO position **end mein** hoti (naya arrival), purani position se koi
relation nahi.

```cpp
book.add(40, true, p1, 60);
book.replace(40, 41, true, p2, 70);
// order 41 ab p2 ke level pe, LAST position (naya arrival)
// order 40 poori tarah gone -- uski purani p1-level position bhi khatam
```

---

## Yeh matching engine (40) ke liye kyun zaroori hai

Yeh book khud match nahi karta (01's scope-note), par jab **40-MATCHING-
ENGINE** ek incoming aggressive order match karega, use isi FIFO order
mein resting orders ko "line se pehle" fill karna hoga (37/07 ka
`match_fifo()` example yaad karo):

```cpp
// 40 (preview) is book ke ids_at_price() jaisa hi kuch use karega:
for (order_id in ids_at_price(side, best_price)) {
    fill order_id jitna ho sake, incoming_qty se;
    if (incoming_qty == 0) break;
}
```

Isi liye FIFO order **preserve** karna (sirf "orders ka collection"
rakhna kaafi nahi) is data structure ka ek core requirement hai.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — level ke andar order ko "kahin bhi" store kar dena
Agar koi bhavishya optimization order ko level ke BEECH mein insert kare
(jaise "price ke alawa kisi aur cheez se sort karo"), FIFO guarantee toot
jaati. Level ke andar sirf **arrival order** matter karta, kuch aur nahi.

### Trap 2 — Replace ko "position-preserving" samajhna
37/07 already flag kiya — Replace = naya order = naya (last) position.
Kisi ne agar assume kiya "modify apni jagah pe rehta," woh galat hai.

### Trap 3 — partial-reduce ko position-changing samajhna
Ulta trap — kabhi kabhi log yeh sochte "qty ghati, to shayad line mein
peeche chala gaya" — nahi, partial reduce sirf size ghataata, position
same rehti (37/07 explicitly yeh clarify karta hai).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| FIFO explicit sorting se aati hai | Container ka natural append-order se aati hai |
| Partial cancel/execute position badalta | Nahi — sirf qty ghatti, position same |
| Replace position preserve karta | Nahi — naya order_id, naya (last) position |
| FIFO sirf V1/V2/V3 mein alag-alag guarantee hoti | Sab teeno IDENTICAL FIFO guarantee dete (verified) |

---

## Exercises

1. Order 10, 11, 12 same price pe add hote (is order mein). Order 11
   partial-cancel hota (qty ghatti, poora nahi jaata). Ab `ids_at_price()`
   kya return karega?
   <details><summary>Answer</summary>
   `[10, 11, 12]` — same order, kyunki 11 poori tarah remove nahi hua
   (sirf qty ghati). Agar order 11 POORA remove hota (qty 0 tak, ya
   explicit delete), result `[10, 12]` hota.
   </details>

2. Order 10 ka price REPLACE hota (naya id 15, same price pe). Kya order
   15 apni "purani" position (jahan 10 tha) mein rahega?
   <details><summary>Answer</summary>
   Nahi. `replace()` purane order ko poora REMOVE karta (`remove(old_id)`)
   phir naya order ko `add()` karta — jo hamesha level ke END mein jaata
   (naya arrival). Agar level mein pehle se doosre orders hain, order 15
   ab unke PEECHE hoga, apni "purani" jagah mein nahi.
   </details>

---

## Interview questions

1. FIFO guarantee kahan se aati hai (mechanism), explicit sorting kyun
   nahi chahiye?
2. `ids_at_price()` teeno versions pe same result kyun deta?
3. Partial reduce aur Replace ka FIFO-position-impact ka fark batao.
4. Matching engine (40) ko yeh FIFO order kyun chahiye hogi?

---

## Next
→ [`11-top-of-book-fast-path.md`](11-top-of-book-fast-path.md)

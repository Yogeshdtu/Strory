# 06 — ITCH-style protocol: message types, layout, parsing

## Prerequisites
- [`05-binary-protocols.md`](05-binary-protocols.md)
- `examples/01_message_structs.cpp`, `examples/wire_protocol.hpp`

## Yeh topic abhi kyun
Nasdaq ka **ITCH** protocol industry mein sabse widely-referenced binary
market-data format hai — publicly documented (isliye teaching ke liye
ideal), aur "L3, message-per-event, big-endian, sequenced" design ka
canonical example. Is folder ka `wire_protocol.hpp` isi style mein
**ek chhota, teaching-purpose ITCH-jaisa protocol** hai — real ITCH nahi
(jiske apne exact message types/fields hain, exchange-specific spec ke
saath), par **wahi design principles**.

---

## ITCH-style design ke core principles

1. **Ek message = ek event.** Har add/execute/cancel/delete/replace apna
   khud ka message type (`AddOrderMsg`, `ExecuteMsg`, ...).
2. **Fixed layout per type** (05 ka poora point — no scanning).
3. **Big-endian (network byte order)** — historical convention (real
   ITCH bhi big-endian hai), 10-endianness-handling ka reason yehi hai.
4. **Sequenced** (04) — har message ek monotonic `seq_num`.
5. **Length-prefixed** (11) — har message apna `length` carry karta,
   framing ke liye.
6. **Order-level (L3)** — har event ek specific `order_id` ke against.

---

## Hamara protocol (`wire_protocol.hpp`) — poora layout

```cpp
struct MsgHeader {             // 16 bytes, HAR message ke shuru mein
    std::uint16_t length;      //  message ka total size (header included)
    std::uint8_t  msg_type;    //  'A'/'E'/'X'/'D'/'U'
    std::uint8_t  _reserved;   //  explicit pad
    std::uint32_t seq_num;     //  monotonic sequence
    std::uint64_t exch_ts_ns;  //  exchange timestamp (14)
};

struct AddOrderMsg  { MsgHeader hdr; uint64_t order_id; uint32_t symbol_id;
                       uint32_t qty; int64_t price_ticks; uint8_t side; };  // 41 bytes
struct ExecuteMsg   { MsgHeader hdr; uint64_t order_id; uint32_t exec_qty; };  // 28 bytes
struct CancelMsg    { MsgHeader hdr; uint64_t order_id; uint32_t cancel_qty; };  // 28 bytes
struct DeleteMsg    { MsgHeader hdr; uint64_t order_id; };  // 24 bytes
struct ReplaceMsg   { MsgHeader hdr; uint64_t old_order_id, new_order_id;
                       uint32_t qty; int64_t price_ticks; };  // 44 bytes
```

(`01_message_structs.cpp` mein exact sizes verified via `static_assert`.)

**Dhyaan do:** har message **apni requirement ke hisaab se chhota** hai —
`DeleteMsg` sirf `order_id` chahiye (24 bytes), `AddOrderMsg` poori detail
(41 bytes). Yeh L3 protocols ki ek common design choice hai: **type se
size predict** hoti hai, ek "generic" bade struct mein sab fields hamesha
bhejna wasteful hota.

---

## 5 message types — poori lifecycle

```
AddOrder   -> naya order book mein aata
Execute    -> order (partial/poora) fill hota
Cancel     -> order ki qty REDUCE hoti (partial cancel)
Delete     -> order POORA khatam hota
Replace    -> order cancel + naya order (naya id, shayad naya price/qty)
```

Yeh 5 events har order ki **poori possible lifecycle** cover karte —
39-ORDER-BOOK mein tum inhi 5 events se ek poora book maintain karoge.

---

## Message dispatch — `msg_type` se

```cpp
switch (hdr.msg_type) {
    case MSG_ADD_ORDER: /* AddOrderMsg */ break;
    case MSG_EXECUTE:   /* ExecuteMsg */  break;
    case MSG_CANCEL:    /* CancelMsg */   break;
    case MSG_DELETE:    /* DeleteMsg */   break;
    case MSG_REPLACE:   /* ReplaceMsg */  break;
}
```

`msg_type` **header mein** hai (fixed position, `offset 2`) — isliye
parser HAMESHA pehle 16 bytes (header) padh sakta, dispatch decide kar
sakta, **phir** type-specific body padhta. Yeh two-phase design (`peek_
header` phir body) `wire_protocol.hpp` mein already implement hai, aur
03-10 ke sab examples isi pattern ko follow karte.

---

## Real Nasdaq ITCH se fark (honest disclosure)

Yeh course ka protocol **teaching-purpose** hai — real ITCH se yeh alag
hai:
- Real ITCH mein bahut zyada message types hain (stock directory, trading
  status, market participant position, cross trades, ...) — hum sirf core
  5 (lifecycle) le rahe.
- Real ITCH ka exact byte layout, field names, aur values **Nasdaq ki
  published spec** mein hain (publicly available document) — agar kabhi
  real ITCH parse karna ho, **wahi spec authoritative source hai**, yeh
  course nahi.
- Design **principles** (fixed layout, big-endian, sequenced, length-
  prefixed, L3) same hain — yehi is lesson ka asli takeaway hai.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — hardcode karna ki har message ek fixed size ka hai
`sizeof(AddOrderMsg)` ≠ `sizeof(DeleteMsg)`. Parser ko **header ka
`length` field** use karna chahiye agla message dhoondne ke liye, na ki
"is type ka struct size" — jyada robust, forward-compatible bhi (agar
kabhi field add ho, purana parser bhi `length` follow kar sakta,
crash nahi hoga — 11 mein detail).

### Trap 2 — `msg_type` padhne se pehle poora message parse karne ki
koshish karna
Header (16 bytes, fixed) **hamesha** pehle valid hota — `msg_type` usi
mein hai. Type-specific body ka size TYPE PE depend karta — pehle type
jaano, phir decide karo kitna aur padhna hai.

### Trap 3 — `Replace` ko sirf "modify" samajhna, lifecycle-consequence
bhoolna
Replace = purana `order_id` **retire** hota, naya `order_id` uski jagah
leta — 37/07 ka "modify = naya time-priority" point yaad karo. Book state
mein purana id ko poora hatana zaroori hai, sirf uske fields update karna
nahi.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Har message same size ki hoti | Type-dependent — header ka `length` use karo |
| Poora message ek saath parse hota | Two-phase: header pehle (dispatch ke liye), phir body |
| Yeh course ka protocol = real ITCH | Design PRINCIPLES same, exact bytes/types alag |
| Replace = sirf field update | Purana order retire, naya order (naya time-priority) |

---

## Hands-on

`examples/01_message_structs.cpp` chalao — poore protocol ke sizes,
static_asserts, aur ek raw message ke bytes dekho:
```bash
./build.ps1 fast 38-MARKET-DATA/examples/01_message_structs.cpp
```

---

## Exercises

1. `hdr.length` field NA hota (sirf `msg_type` se size guess karna padta)
   — kya problem aata agar exchange kabhi ek naya OPTIONAL field kisi
   type mein add kare?
   <details><summary>Answer</summary>
   Purana parser (jo hardcoded `sizeof(OldType)` use kar raha) galat
   offset pe agla message dhoondega — ya to crash, ya silently galat data
   parse karega. `length` field hone se, parser hamesha exchange-bataye
   gaye size ko follow kar sakta — naya field agar bhi add ho jaaye, purana
   parser use bas "ignore" kar sakta (jo bytes samajhta nahi, skip kar
   deta `length` follow karke) bina crash kiye.
   </details>

2. Kyun `msg_type` header mein hai, kisi bhi type-specific struct ke andar
   nahi?
   <details><summary>Answer</summary>
   Taaki parser **dispatch decision** (kaunsa type hai) le sake bina yeh
   jaane ki type-specific body kaisi dikhti hai. Agar `msg_type` kisi
   specific struct ke andar hota, parser ko pehle hi janna padta "yeh
   kaunsa type hai" — chicken-and-egg problem. Header sab types mein
   COMMON hai isliye woh hi self-describing entry point ban sakta.
   </details>

---

## Interview questions

1. Hamara protocol ka poora message layout batao (header + 5 types).
2. Kyun `length` field pe rely karna chahiye, hardcoded struct sizes pe
   nahi?
3. Two-phase parsing (header-first, phir body) kyun zaroori hai?
4. Yeh teaching protocol aur real Nasdaq ITCH mein kya fark hai?

---

## Next
→ [`07-fix-and-fast.md`](07-fix-and-fast.md)

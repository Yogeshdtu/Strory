# 17 — Exercises: market data (practice + parsing challenges)

## Prerequisites
- Poora folder `38` (`01`–`16`) + `examples/`

## Kaise use karein
- **Part A** — parsing challenges: raw bytes diye hain, haath se decode karo.
- **Part B** — scenario/design questions (framing, gaps, recovery, timestamps).
- **Part C** — hands-on: `examples/` modify karo, predict karo, verify karo.
- Folder-wide interview questions end mein.

---

## Part A — Parsing challenges

### A1
Yeh raw bytes ek `MsgHeader` (16 bytes, big-endian) hain:
```
00 29 41 00 00 00 00 2A 00 00 00 00 07 5B CD 15
```
`length`, `msg_type`, `seq_num`, aur `exch_ts_ns` decode karo (haath se —
calculator use kar sakte ho).
<details><summary>Answer</summary>

`MsgHeader` layout: `length`(u16) `msg_type`(u8) `_reserved`(u8)
`seq_num`(u32) `exch_ts_ns`(u64) — sab big-endian, MSB byte pehle.

- bytes[0-1] = `00 29` → length = 0x0029 = **41**
- bytes[2]   = `41` → msg_type = 0x41 = ASCII `'A'` = **Add Order**
- bytes[3]   = `00` → reserved (ignore)
- bytes[4-7] = `00 00 00 2A` → seq_num = 0x0000002A = **42**
- bytes[8-15] = `00 00 00 00 07 5B CD 15` → exch_ts_ns = 0x0000000075BCD15
  = **123,456,789** ns (upper 4 bytes zero kyunki value 32 bits mein fit
  ho gaya — exactly `01_message_structs.cpp` ka test value)

Yeh EXACTLY `01_message_structs.cpp` ke output se match karta (length=41,
msg_type=Add, seq=42, exch_ts_ns=123456789) — apna kaam us file ke actual
byte-dump se cross-check kar sakte ho.
</details>

### A2
Ek message ka `length` field `0x001C` (28) hai, `msg_type` `0x45` ('E').
Kaunsa struct hai yeh, aur kitne bytes uske PAYLOAD (header ke baad) mein
hain?
<details><summary>Answer</summary>
`msg_type = 'E' = MSG_EXECUTE` → `ExecuteMsg`. Total length 28 bytes,
header 16 bytes, isliye payload = 28 - 16 = **12 bytes** (`order_id`
8 bytes + `exec_qty` 4 bytes = 12 — match karta `ExecuteMsg` ke layout se).
</details>

### A3
Ek parser buffer ke end pe pahunchta hai, aur sirf 10 bytes bache hain
(16-byte header ke liye kaafi nahi). Kya karna chahiye — error throw karo,
skip karo, ya kuch aur?
<details><summary>Answer</summary>
Na error, na skip — yeh ek **partial header** hai (11-message-framing).
Parser ko yahan RUKNA chahiye (`peek_header` false return karta), aur yeh
10 bytes ko **carry-over buffer** mein rakhna chahiye agli `recv()` call
ke naye bytes ke saath combine karne ke liye. Discard karna data-loss
create karega.
</details>

---

## Part B — Scenario / design questions

### B1
Tumhara feed handler ek gap detect karta hai (`seq` jump 500 se 505 tak,
matlab 501-504 missing). A/B arbitration try karta hai, dono feeds mein
yeh 4 messages missing hain. Kya karoge, step by step?
<details><summary>Answer</summary>
1. Book ko us range ke liye "uncertain"/"stale" mark karo (13, 37/13).
2. Retransmission request bhejo (TCP, off hot-path) seq 501-504 ke liye,
   timeout ke saath.
3. Response mile to apply karo, book LIVE wapas.
4. Timeout ho jaaye to snapshot resync try karo (03).
5. Woh bhi fail ho to symbol ko suspend karo, operator alert karo — kabhi
   "silently continue karo purani state pe" mat karo (fail-closed, 37/13).
</details>

### B2
Ek naya consumer feed ko 10:15:30 baje join karta hai (feed already
09:00:00 se chal rahi). Woh sirf incremental channel sunta hai, snapshot
channel ignore karta. Kya hoga uski book ki state ke saath?
<details><summary>Answer</summary>
Book **khaali/galat** shuru hogi — incremental updates pehle-se-existing
(09:00-10:15 ke) orders ke against aayenge (cancel/execute) jo consumer
ki (khaali) book mein hain hi nahi. Yeh 03's core lesson hai: incremental
self-sufficient nahi, snapshot se shuru karna zaroori hai.
</details>

### B3
Tumhara system dono feed A aur B ko sunta hai, par galti se dono ek hi
network interface/switch se route ho rahe hain (accidental misconfiguration).
A/B arbitration (12) ka statistical faayda kaisa affect hoga?
<details><summary>Answer</summary>
Bahut kam ho jaayega — agar dono paths physically same hain, unka packet
loss **correlated** hoga (same congestion/issue dono ko ek saath affect
karega). Arbitration ka poora faayda "independent loss" assumption pe
tha; woh assumption break ho gayi.
</details>

### B4
Tumhara measured wire-to-wire latency (`recv_ts - exch_ts`) kabhi 50µs,
kabhi -20µs (negative!) aata hai, same network path pe. Kya diagnose
karoge?
<details><summary>Answer</summary>
Negative latency physically impossible hai (message apne bhejne se pehle
nahi aa sakti) — yeh **clock skew** (14) ka signature hai, jiski magnitude
actual network delay ke comparable ya usse bada hai. PTP-based clock sync
check karo — agar PTP nahi hai ya drift kar raha hai, timestamps ka delta
meaningless hai jab tak fix na ho.
</details>

---

## Part C — Hands-on

### C1
`06_parser_comparison.cpp` mein `N` ko `20000` (10x chhota) kar do,
chalao. Predict karo: kya p99.9 ratio (naive/zero-copy) badlega, aur
kis direction mein?
<details><summary>Answer</summary>
Chhote N pe, vector kam baar grow karega (kam total push_backs), isliye
naive path ke liye tail-triggering events (realloc) bhi kam frequent
honge — p99.9 measurement khud NOISIER ho sakta (kam samples, percentile
ek "lucky"/"unlucky" hit pe zyada depend karta). Ratio ka exact number
run-to-run zyada vary karega, par same DIRECTION (naive > zero-copy at
tail) expect karo — mechanism (allocation-driven tail) N se independent
hai.
</details>

### C2
`08_gap_detection.cpp` mein `keep_prob` ko `1.0` (koi loss nahi) kar do.
Expected output kya hoga?
<details><summary>Answer</summary>
`gaps detected: 0`, `total missing sequence numbers: 0`,
`sent == received`. Koi message drop nahi hui, isliye `expected_seq`
hamesha exactly agli `seq_num` se match karega.
</details>

### C3
`wire_protocol.hpp` mein `generate_feed()` ka `base_ts_ns` parameter
badal ke dekho ki `exch_ts_ns` values kaise shift hote (07 ka output
re-run karke). Kya `07`'s "inter-message gap" stats badalte?
<details><summary>Answer</summary>
`exch_ts_ns` VALUES shift honge (sab ek constant offset se aage/peeche),
par **gaps (deltas)** SAME rahenge — kyunki gap = `ts[i] - ts[i-1]`, aur
dono values same offset se shift hue, delta unaffected. Yeh ek achha
sanity-check hai (offset-invariance) apne khud ke timestamp-processing
code ke liye.
</details>

---

## Folder-wide interview questions

1. Market data feed kya hai, aur poore HFT pipeline (37/12) mein kahan
   fit hoti?
2. L1, L2, L3 ka fark, aur har ek ka use-case.
3. Snapshot vs incremental — trade-off aur kyun dono zaroori hain.
4. Sequence numbers se gap/duplicate/out-of-order kaise detect karte?
5. Binary protocol ka real faayda kya hai (byteswap-cost se related
   claim ko sahi karo — measured evidence ke saath).
6. Hamare protocol ka layout batao (header + 5 message types).
7. FIX, FAST, aur custom binary — kab kya use hota?
8. SBE native-endian kyun choose karta (sahi reasoning)?
9. Zero-copy parsing ka measured p99.9 improvement kitna tha is folder
   ke examples mein, aur mechanism kya hai?
10. Byteswap kaise implement karte (`if constexpr` ka role), aur iski
    measured cost?
11. Message framing — length-prefix vs delimiter, TCP vs UDP context.
12. A/B feed arbitration measured improvement kitna tha, aur kis
    assumption pe depend karta?
13. Recovery ke 3 layers batao, increasing-cost order mein.
14. Exchange timestamp vs receive timestamp, aur PTP ka role.
15. Conflation kab safe hai, kab nahi?
16. Poora "build simple → measure → optimize → re-measure → explain"
    process is folder ke concrete numbers ke saath batao.

---

## Next
→ [`../39-ORDER-BOOK/00-README.md`](../39-ORDER-BOOK/00-README.md)

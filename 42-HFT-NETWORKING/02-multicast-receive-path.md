# 02 — Market data multicast: group management, filtering

## Prerequisites
- `01-hft-network-path.md`
- `30-NETWORKING/06-multicast.md` (`IP_ADD_MEMBERSHIP` ka basic mechanism)

## Yeh topic abhi kyun

Market data ALMOST HAMESHA multicast pe distribute hoti (37/02, 30/06)
— ek exchange EK BAAR bhejta, N subscribers apni copy paate. Yeh lesson
`01_multicast_receiver.linux.cpp` ko deep-dive karta — 30/06 ka SAME
core mechanism, do naye layers ke saath.

---

## Layer 1: backend abstraction

```cpp
KernelSocketReceiver rx(group, port, ifname);
...
while (...) {
    if (!rx.try_receive(pkt)) continue;
    // process pkt.data, pkt.len
}
```

`08_bypass_abstraction.hpp`'s `INetworkReceiver` interface — is folder
ki HAR receiver-based example (01, 03, 06) isi shape ke against likhi
hai. `try_receive(RxPacket&)` HAMESHA same signature, chahe andar
kernel-socket ho ya (03-06 mein discuss hone wali) bypass library.
**Yeh 03's poora point hai** — abhi detail nahi, bas notice karo ki
receive-LOOP ka code kabhi backend-specific nahi dikhta.

---

## Layer 2: sequence-gap detection

```cpp
if (m.seq != expected) {
    const std::uint32_t missing = m.seq - expected;
    ++gaps;
}
expected = m.seq + 1;
```

**UDP koi delivery guarantee nahi deta** (30's foundational fact) —
multicast toh aur bhi zyaada (fan-out ke through packet-loss ka chance
badhta, especially IGMP-snooping-misconfigured switches pe). Isliye
market-data protocols (38-MARKET-DATA jaisa) HAMESHA ek `seq_num` carry
karte — receiver isi se **transport-level loss** detect karta, jo
KHUD MARKET DATA ka hissa nahi hai (koi trade "miss" nahi hua, sirf
uska NOTIFICATION tumhare paas nahi pahuncha — recovery, 38/07 ka kaam,
snapshot/retransmit se fill hota).

`01`'s demo isse LITERALLY prove karta: deliberately dropped seqs
(`seq % 777 == 0`) HAMESHA `gaps_detected` mein EXACT count ke saath
pakde jaate — deterministic (skip pattern predictable hai), reproducible.

---

## `SO_RCVBUF` — buffer size ka trade-off

```cpp
int rcvbuf = 4 * 1024 * 1024;
::setsockopt(rx.fd(), SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof rcvbuf);
```

Kernel har socket ke liye ek DEFAULT receive buffer rakhta (typically
kaafi CHHOTA, kilobytes range mein). Agar market data BURSTY hai (ek
saath bahut saare updates, jaisa market open) aur app thodi der consume
karne mein delay kare, kernel buffer FULL ho sakta — us point ke baad
naye packets **SILENTLY DROP** hote, kernel-level (application ko koi
error bhi nahi milta, bas packet kabhi aata hi nahi). Bada buffer isse
absorb karta — trade-off: zyaada memory, aur agar buffer bahut bada ho
to "purani" data zyaada der tak queue mein reh sakti (queueing delay,
35/05's principle).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — sequence-gap ko "market data corrupt hai" samajh lena
Gap ka matlab hai TRANSPORT ne kuch khoya — market-data KHUD kabhi
"corrupt" nahi hoti (exchange se sahi bheji gayi thi). Yeh transport-vs-
content distinction 38/09 mein pehle bhi mila.

### Trap 2 — `SO_RCVBUF` ko "jitna bada utna behtar" samajhna
Bahut bada buffer STALE data ko zyaada der queue mein rakhta — agar
app genuinely peeche hai, bada buffer sirf "kab drop hoga" delay karta,
asli problem (app slow kyun hai) nahi fix karta.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Multicast reliable delivery guarantee deta | UDP unicast jaisa hi unreliable, bas 1-to-many |
| `SO_RCVBUF` bada karna "hamesha safe" hai | Trade-off hai -- queueing delay vs drop-avoidance |
| Ek receive-loop backend-specific hona CHAHIYE | `INetworkReceiver` abstraction se backend-agnostic ho sakta |

---

## Hands-on

```bash
# Linux/WSL pe (is repo ka dev box Windows hai, yahan verify nahi hota):
g++ -std=c++20 -O2 -Wall -Wextra -pthread 01_multicast_receiver.linux.cpp -o mcastrx
./mcastrx 239.1.2.4 9400 lo
```

---

## Exercises

1. Ek receiver `gaps_detected=3` dikhata, `total_missing_seqs=3`. Kya
   yeh possible hai ki actual PACKETS lost hue ho `4` ho (na ki 3)?
   <details><summary>Answer</summary>
   Haan -- agar 2 CONSECUTIVE packets khoye (jaisa seq 100 aur 101 dono),
   yeh EK gap (ek "jump" expected se actual tak) hoga, par
   `total_missing_seqs` us gap ke andar 2 count karega. `gaps_detected`
   "kitni baar jump hua" batata, `total_missing_seqs` "kitne total seq
   khoye" -- alag metrics, jaan-boojh kar dono track kiye jaate.
   </details>

---

## Interview questions

1. `INetworkReceiver` abstraction ka fayda kya hai?
2. Sequence-gap detection kya batata, aur kya NAHI batata?
3. `SO_RCVBUF` ka trade-off explain karo.

---

## Next
→ [`03-kernel-bypass-overview.md`](03-kernel-bypass-overview.md)

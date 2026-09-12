# 11 — Message framing: length prefixes, partial reads

## Prerequisites
- [`10-endianness-handling.md`](10-endianness-handling.md)
- `30-NETWORKING` (TCP stream vs UDP datagram — is lesson ka context)

## Yeh topic abhi kyun
Ab tak humne maana ki poora message buffer mein available hai. Real
network I/O mein yeh guarantee **nahi** hoti — "framing" hi decide karta
hai ki parser ko pata kaise chale "ek message kahan khatam hoti, agli kahan
shuru hoti."

---

## Framing problem — ek line mein

> **Bytes ek stream/sequence mein aate hain. Parser ko pata hona chahiye
> "yeh 41 bytes ek poora message hain" — bina kisi explicit boundary
> marker dekhe bhi.**

Do common solutions:

| Approach | Kaise | Trade-off |
|---|---|---|
| **Length-prefix** (hum use kar rahe) | Har message shuru mein apni length carry karti | Parser ko length field padhni padti pehle; koi ambiguity nahi |
| **Delimiter** | Ek special byte/sequence (jaise `\n`) message ke end pe | Simple, par agar delimiter khud DATA mein aa jaaye, escaping chahiye |

Binary protocols (yeh folder) **length-prefix** use karte — 05 mein
dekha, fixed-offset access ka poora faayda delimiter-scanning avoid karna
hai; delimiter approach wapas scanning maang leti.

---

## Hamara framing (`wire_protocol.hpp`)

```cpp
struct MsgHeader {
    std::uint16_t length;   // <- YEH poore message ka size (header included)
    ...
};
```

Parser ka loop (`03`-`10` sab isi pattern ko follow karte):
```cpp
std::size_t offset = 0;
while (offset < buf.size()) {
    MsgHeader hdr{};
    if (!peek_header(buf.data() + offset, buf.size() - offset, hdr))
        break;   // <- partial HEADER (< 16 bytes bache) -- wait for more
    if (buf.size() - offset < hdr.length)
        break;   // <- partial BODY (header mila, poora message nahi) -- wait for more
    process(buf.data() + offset, hdr);
    offset += hdr.length;   // agle message tak jump -- struct-size guess NAHI, hdr.length
}
```

**Do alag "partial" cases hain**, dono handle karne zaroori:
1. Buffer mein 16 bytes se kam bache — **header khud incomplete** hai.
2. Header mil gaya (`hdr.length` pata chal gaya), par baaki buffer mein
   poore `hdr.length` bytes nahi hain — **body incomplete** hai.

---

## TCP vs UDP — framing ka context bilkul alag

### TCP — **stream**, koi message boundary hi nahi
```
TCP se recv() jo bhi bytes de de -- ek call mein POORA ek message ho
sakta, ADHA message ho sakta, DO-AUR-ADHA message ho sakta. Kernel/
network TCP mein "message boundary" ka concept hi preserve NAHI karta.
```
**Length-prefix framing TCP ke liye MANDATORY hai** — bina isse, parser
ko pata hi nahi chal sakta ek message kahan khatam hoti (30-NETWORKING
mein Nagle/buffering se yeh aur bhi unpredictable ho sakta).

### UDP — **datagram**, per-packet boundary preserved (partially)
```
UDP recv() EK poora datagram deta (ya kuch nahi) -- kabhi "aadha datagram"
nahi milta. Par ek datagram mein MULTIPLE wire messages ho sakte
(batching, throughput ke liye -- folder 36/16).
```
Isliye UDP mein bhi **length-prefix zaroori hai agar ek datagram multiple
messages carry karta** (jo throughput ke liye common practice hai) — sirf
"single message per datagram" design mein framing trivial ho jaati (poora
datagram = poora message), par batching ka faayda chhod dena padta.

---

## Partial reads — kyun "wait for more" zaroori hai, crash nahi

Agar tum `peek_header`/`hdr.length` check **nahi** karte, aur seedha
buffer se struct overlay-cast kar lete:
```cpp
// ❌ DANGEROUS agar buffer mein poora message nahi hai
const auto* m = reinterpret_cast<const AddOrderMsg*>(buf.data() + offset);
// buffer se aage ke bytes padh sakta -- OUT-OF-BOUNDS read (heap-buffer-overflow)
```
Yeh **buffer over-read** hai — sanitizers (35/15 ka ASan) isse pakadte,
production mein yeh crash ya (worse) silently galat memory padh sakta.
**`remaining < needed` check hamesha PEHLE** — koi shortcut nahi.

---

## "Carry-over" — jo bytes is round mein process nahi hue

Real streaming feed handler (TCP context) mein, jab `on_bytes()` "partial"
return karta (jaisa `10_feed_handler.cpp` ka `on_bytes` 0 return karta),
un **bache hue bytes ko discard nahi karna** — agli `recv()` call ke naye
bytes ke saath **prepend** karna padta (ek chhota carry-over buffer),
taaki jab poora message assemble ho jaaye, wahi se process ho sake.

```
recv() call 1: [poora msg A][poora msg B][aadha msg C]
    -> A, B process, "aadha C" ko carry-over buffer mein rakho

recv() call 2: [baaki C][poora msg D]
    -> carry-over + naye bytes = poora C -- process karo, phir D
```

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `sizeof(SpecificMsgType)` se agla offset compute karna
06 ka Trap 1 yaad karo — `hdr.length` se offset badhao, hardcoded struct
size se nahi. Forward-compatible bhi, aur galat agar type-size mismatch
ho.

### Trap 2 — partial message ko "corrupt data" samajh ke discard karna
Partial ≠ corrupt. Partial ka matlab hai "abhi tak poora nahi aaya" —
sahi response **wait karna** hai (carry-over), discard karna data-loss
create karta.

### Trap 3 — TCP pe UDP-jaisi "ek recv = ek message" assumption
Yeh sabse common real-world bug hai. TCP mein **kabhi** guarantee nahi hoti
ki ek `recv()` call ek poori message degi — chhoti bhi de sakti, kai
messages bhi de sakti.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| `sizeof(Type)` se agla message dhoondo | `hdr.length` follow karo (06) |
| Partial message = corrupt data | Partial = "abhi poora nahi aaya," wait karo |
| TCP mein ek recv() = ek message | TCP stream hai, koi boundary guarantee nahi |
| UDP mein ek datagram = hamesha ek message | Multiple messages/datagram common hai (batching) |

---

## Exercises

1. TCP se `recv()` karke tumhe 100 bytes mile, par pehla message ka
   `length` field 60 hai aur doosra message ka header abhi poora nahi
   aaya (sirf 20 bytes mile uska, 16 chahiye header ke liye khud — theek
   hai — par uska `length` batayega ki poore message ke liye kitna aur
   chahiye). Kya karoge?
   <details><summary>Answer</summary>
   Pehla message (60 bytes) process karo (poora available hai). Baaki 40
   bytes se doosre message ka header (agar 16 bytes se zyada available
   hai) peek karo — agar header khud complete hai to uska `length` pata
   chal jaayega, phir dekho poora message available hai ki nahi. Agar
   NAHI (partial body), bache hue bytes ko carry-over buffer mein rakho,
   agli `recv()` ka wait karo — discard mat karo.
   </details>

2. Kyun UDP mein bhi length-prefix framing zaroori ho sakti hai, jabki
   UDP "already" datagram boundaries preserve karta hai?
   <details><summary>Answer</summary>
   Agar ek datagram mein SIRF EK message hoti, datagram boundary hi
   framing hoti (length-prefix redundant). Par throughput ke liye (36/16
   "batching") ek datagram mein MULTIPLE messages bhejna common practice
   hai — tab datagram ke ANDAR bhi framing chahiye (kaunsa message kahan
   khatam hota), aur wahi length-prefix karta hai.
   </details>

---

## Interview questions

1. Length-prefix aur delimiter-based framing ka trade-off batao.
2. `hdr.length` follow karna kyun `sizeof(SpecificType)` se better hai?
3. TCP aur UDP mein framing ki zaroorat kaise alag hoti?
4. "Partial message" ka sahi response kya hai, aur "corrupt message" se
   yeh kaise alag hai?

---

## Next
→ [`12-ab-feed-arbitration.md`](12-ab-feed-arbitration.md)

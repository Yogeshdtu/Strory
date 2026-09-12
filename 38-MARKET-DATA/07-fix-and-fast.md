# 07 — FIX aur FAST: text protocol, aur uska compressed cousin

## Prerequisites
- [`06-itch-protocol.md`](06-itch-protocol.md)
- [`05-binary-protocols.md`](05-binary-protocols.md)

## Yeh topic abhi kyun
ITCH-style binary (06) is folder ka focus hai — par industry mein **FIX**
utna hi common naam hai, khaaskar order-entry side pe. Yeh samajhna zaroori
hai ki FIX **kahan** dikhta hai, aur kyun market-data-specific binary
protocols (ITCH/SBE) usse alag jagah use hote.

---

## FIX (Financial Information eXchange) — text-based

37/02 ka refresher: order entry (write path) aur market data (read path)
alag hain. **FIX historically dono ke liye use hua**, par aaj:
- **Order entry** — FIX **abhi bhi widely used hai**, khaaskar broker-to-
  exchange, buy-side-to-broker connectivity mein. Bahut systems iske around
  ban chuke hain, integration cost high hai isse hataane ka.
- **Market data** — high-throughput venues ne largely **binary** (ITCH-
  style, SBE) apna liya hai bandwidth/latency ke liye (05 mein reason).

```
FIX message (tag=value, SOH `\x01` delimiter se separated):
8=FIX.4.4|9=112|35=D|49=SENDER|56=TARGET|34=1|52=20240115-10:30:00|
55=AAPL|54=1|38=100|44=150.25|40=2|...|10=128|
```

Har `tag=value`: `35=D` (MsgType=NewOrderSingle), `55=AAPL` (Symbol),
`54=1` (Side=Buy), `38=100` (Qty), `44=150.25` (Price). **Human-readable,
self-describing** (agar tumhe FIX tag dictionary pata hai).

---

## FIX kyun abhi bhi zinda hai (order entry mein)

| Faayda | Kyun matter karta order entry mein |
|---|---|
| **Human-debuggable** | Ek order reject/dispute ho, log seedha padh sakte ho |
| **Extensible** | Naya optional tag add karna backward-compatible hai |
| **Industry standard** | Dashak se sab broker/exchange isse support karte — replace karna costly |
| **Latency-tolerant use case** | Order entry ka volume market-data se **bahut kam** (ek trader minute mein kuch orders bhejta, feed millisecond mein hazaaron updates deta) |

**Market data mein yeh trade-offs ulat jaate:** volume bahut zyada hota,
latency-sensitivity zyada hoti, aur "human debug karna" itna zaroori nahi
(automated systems consume karte, insaan nahi).

---

## FAST (FIX Adapted for STreaming) — FIX ka compressed cousin

FAST ek **binary encoding** hai FIX messages ka — same logical fields
(tags), par:
- **Field-level compression** — repeated/predictable values (jaise same
  symbol baar-baar) ko encode karne ka efficient tareeka.
- **Templates** — dono taraf (sender/receiver) ek known message-shape
  template share karte, sirf **changes** encode hoti (delta encoding
  jaisa concept).
- Result: FIX se bahut chhota, par pure custom-binary (ITCH-style) se
  zyada complex/generic.

**Kab dikhta hai:** FAST historically un venues mein use hua jo apna
market data **FIX-compatible rehte hue** compress karna chahte the (already
FIX-based infrastructure thi, poora rewrite nahi karna tha). Naye-design
high-frequency venues zyaadatar **pure custom binary** (ITCH-style, SBE)
seedha choose karte — kam complexity, zyada control.

---

## Decision map: kab kya

```
Order entry, low volume, latency non-critical, debugging priority
   -> FIX (text)

Order entry/market data, FIX infrastructure already hai, compress karna hai
   -> FAST

Market data, high volume, latency-critical, greenfield design
   -> Custom binary (ITCH-style, 06) ya SBE (08)
```

> **HFT connection:** hot-path (market data ingestion, strategy decision)
> **hamesha** binary. Control-plane (order entry confirmations, admin,
> compliance reporting) FIX ho sakta bina koi latency risk ke — 37/12 ka
> "market-data-path vs order-path, do alag concerns" yahan protocols ke
> level pe bhi apply hota.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — "FIX purana/deprecated hai" samajhna
Widely used hai, khaaskar order-entry mein. "Purana" ≠ "unused" — bahut
production systems iske around design hain.

### Trap 2 — market data ke liye FIX use karne ki koshish karna
Text-parsing cost (05) high-frequency, high-volume market data ke liye
prohibitive hai. Iske liye FIX **kabhi** design hi nahi hua tha.

### Trap 3 — FAST ko "FIX jitna simple" samajhna
FAST templates/state-tracking involve karta (compression state dono
taraf sync rehni chahiye) — parsing complexity binary-fixed-layout se
zyada hai, chahe FIX-text se kam.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| FIX deprecated hai | Order entry mein widely used, industry standard |
| Market data FIX use karti | Zyaadatar binary (ITCH/SBE) — bandwidth/latency ke liye |
| FAST = FIX ka binary version, utna hi simple | Templates/compression state — zyada complex parsing |
| Sab protocols same use-case ke liye design hote | Order-entry vs market-data ka latency/volume profile bahut alag |

---

## Exercises

1. Ek naya venue design ho raha, greenfield (koi legacy FIX infra nahi).
   Market data ke liye FIX, FAST, ya custom binary — kaunsa choose
   karoge, aur kyun?
   <details><summary>Answer</summary>
   Custom binary (ITCH-style ya SBE) — FIX ka text-parsing overhead
   (05) high-volume market data ke liye nahi chalega. FAST FIX-legacy
   infrastructure ke saath backward-compatibility ke liye useful hai —
   greenfield mein woh constraint hi nahi hai, to seedha simplest-possible
   binary design behtar (kam complexity, zyada control, easier to
   optimize).
   </details>

2. Ek broker apne clients ko order-entry API deta. Kyun FIX abhi bhi ek
   reasonable choice ho sakta, ITCH-style binary ke bajaye?
   <details><summary>Answer</summary>
   Order-entry volume market-data se bahut kam hota (ek client minute mein
   kuch orders bhejta, na ki hazaaron/second). Latency-sensitivity bhi
   kam hoti (order confirmation ek human/algo ke liye 10ms vs 100µs mein
   aana bahut clients ke liye barabar hai). Debuggability aur industry-
   wide interoperability (sab clients FIX jaante) yahan bigger wins hain.
   </details>

---

## Interview questions

1. FIX kya hai, aur yeh kahan (order entry vs market data) zyada use
   hota, kyun?
2. FAST kya hai, aur FIX se kaise alag hai?
3. Greenfield high-frequency market-data design mein FIX/FAST kyun
   typically NAHI choose kiya jaata?
4. Order-entry aur market-data ke protocol-choice trade-offs alag kyun
   hote (volume, latency-sensitivity)?

---

## Next
→ [`08-sbe.md`](08-sbe.md)

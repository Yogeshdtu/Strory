# 05 — Binary vs text protocols: why binary wins (measured, honest)

## Prerequisites
- [`04-sequence-numbers.md`](04-sequence-numbers.md)
- `examples/02_endian_handling.cpp`

## Yeh topic abhi kyun
Ab tak humne binary wire format (`wire_protocol.hpp`) use kiya bina justify
kiye. Yeh lesson batata hai **kyun** — aur, CLAUDE.md Rule 2 ki spirit mein,
ek common **oversimplified** justification ko measured data se correct
karta hai.

---

## Text protocol — kaisa dikhta hai

```
35=D|55=AAPL|54=1|38=100|44=150.25|...
```

(Yeh FIX jaisa hai — 07 mein detail.) Har field `TAG=VALUE`, delimiter se
separated. **Human-readable** — tum ise seedha padh sakte ho, `grep` kar
sakte ho, debug karna aasan.

## Binary protocol — kaisa dikhta hai

```
[16-byte header][8-byte order_id][4-byte symbol_id][4-byte qty][8-byte price][1-byte side]
```

Fixed offsets, koi delimiter nahi, koi text-to-number conversion nahi.
**Machine ke liye design kiya**, insaan ke liye nahi.

---

## Common (oversimplified) justification: "binary fast hai kyunki byte-
swap text-parsing se sasta hai"

Yeh **half-sach** hai. Chalo measure karte hain:

```
02_endian_handling.cpp (-O2):
  bswap64() measured cost = 0.753 ns/swap
```

**Sub-nanosecond.** Ek single `BSWAP` CPU instruction, ~1-2 cycles. Yeh
**bilkul bhi expensive nahi hai** — agar binary ka poora faayda "swap text
parsing se sasta hai" hota, woh faayda itna chhota hota ki kisi ko fark na
padta.

**To fir binary kyun jeetta hai?** Kyunki text protocol ki asli cost
**delimiter scanning + string-to-number conversion** hai, byteswap nahi:

```
Text: "150.25" ko double/int64 mein convert karna:
  - delimiter dhoondo (scan character-by-character)
  - digit-by-digit parse karo (multiply-accumulate loop)
  - decimal point handle karo
  - locale-dependent ho sakta (std::stod)
  - allocate ho sakta agar tum pehle substring nikaalte ho (std::string)

Binary: field already ek int64_t hai, memory mein.
  - memcpy (ya direct overlay-read)
  - EK bswap instruction (0.753 ns)
  - DONE
```

**Nichod:** binary ka real faayda hai **"parsing" step ka poora KHATAM ho
jaana** — number already number hai, string se number banana hi nahi
padta. Byteswap iska ek chhota, sasta, leftover step hai — asli cost text
mein STRING-SCANNING aur STRING-TO-NUMBER conversion thi.

> Yeh Rule 2 ka classic case hai: "obvious" reasoning ("swap costly hai")
> measure karne pe galat nikla, par **conclusion** (binary jeetta hai)
> sahi hai — sirf **wajah** galat samjhi gayi thi.

---

## Binary ke aur faayde (measure-independent)

| Faayda | Kyun |
|---|---|
| **Fixed size** | Compiler/parser ko pata offset X pe field Y hai — koi scan nahi (11-message-framing) |
| **No ambiguity** | Text mein delimiter khud data mein aa sakta (escaping chahiye); binary mein length-prefixed, koi confusion nahi |
| **Compact** | `"150.25"` = 6 bytes text; `int64_t` = 8 bytes par UNLIMITED precision range mein, aur typically kam bytes (integer ticks — 37/08) |
| **Zero-copy possible** | Fixed layout => struct overlay directly on buffer (09-zero-copy-parsing) — text protocol mein yeh possible hi nahi (har field variable-length) |
| **Deterministic parse time** | Fixed offsets => O(1) field access; text => O(length) scan |

**Yeh "zero-copy possible" wala point sabse bada hai** — text protocol
kabhi bhi struct-overlay se parse nahi ho sakta (variable-length fields,
delimiters) — hamesha kuch scanning/copying chahiye hoti. Binary
fixed-layout iss poori class of cost ko structurally khatam kar deta.

---

## Text protocol kab phir bhi use hoti hai

Text (FIX) **abhi bhi widely used hai** (07 mein detail) kyunki:
- **Human-debuggable** — production issue ho to log padh sakte ho seedha.
- **Extensible** — naya optional field add karna text mein trivial hai.
- **Non-latency-critical paths** — order entry confirmation, admin
  messages, jahan microseconds matter nahi karte.

Market data (yeh folder) **almost hamesha binary** hai kyunki volume +
latency dono high-stakes hain. Order entry (37) **mix** hota — bahut
FIX-based systems still exist, HFT-specific gateways binary use karte.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — "byteswap costly hai" bina measure kiye maan lena
Upar dikha — 0.753 ns/swap. Agar tumhara reasoning "binary fast hai kyunki
swap sasta" hai, tum galat reason de rahe ho (chahe conclusion sahi ho).

### Trap 2 — text protocol ko "hamesha slow" samajhna
Text protocol slow **parsing** ki wajah se hai (scanning, string-to-number),
network bandwidth ki wajah se bhi (verbose). Par kisi non-hot-path use case
mein yeh problem hi nahi hai.

### Trap 3 — binary = automatically zero-copy samajhna
Binary sirf zero-copy **possible** banata — agar tum phir bhi struct ko
memcpy karke owned copy banate ho (03/04 ka naive parser), tum binary ka
zero-copy faayda use hi nahi kar rahe (05/06 mein isi ka measured fark).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Binary fast hai kyunki byteswap sasta hai | Byteswap already sasta tha (0.75ns); real faayda parsing elimination hai |
| Text protocol hamesha bekaar hai | Non-hot-path (admin, debugging) mein still valid |
| Binary = automatic zero-copy | Sirf POSSIBLE banata, tumhe zero-copy design karna padta (09) |
| Byteswap = performance bottleneck | Measured negligible — 0.75 ns/op |

---

## Exercises

1. `"150.25"` ko `double` mein convert karne ka poora cost list karo (kya
   kya ho raha), aur binary `int64_t price_ticks` field se compare karo.
   <details><summary>Answer</summary>
   Text: delimiter-scan (kaunse characters number ke hain), digit-by-digit
   accumulate, decimal-point handle, floating-point rounding (double —
   03-VARIABLES ka trap), locale-dependence, possibly `std::string`
   allocation for the substring. Binary: koi scan nahi (offset fixed),
   `memcpy` + `bswap64` (0.75ns), koi rounding (integer), koi allocation.
   Text ka cost text-SCANNING + STRING-PARSING mein hai, binary mein woh
   step hi exist nahi karta.
   </details>

2. Agar byteswap sirf 0.75 ns lagta hai, to phir SBE (08) native-endian
   kyun choose karta, agar swap-cost-saving itna chhota hai?
   <details><summary>Answer</summary>
   Preview (08 mein poora): SBE ka main faayda swap ka RAW CPU-cost bachana
   nahi hai (jo already negligible hai) — balki **swap CODE bilkul likhna
   hi na pade** (correctness/maintenance win: har field ke liye ek "kya
   yeh swap ki gayi?" tracking bug-class khatam), aur schema-driven codegen
   se manual parser likhna avoid hona.
   </details>

---

## Interview questions

1. Byteswap ka actual measured cost kitna hai, aur binary protocols ka
   real faayda kya hai (agar swap sasta hai)?
2. Text protocol kab bhi appropriate hota hai?
3. "Binary = zero-copy" statement mein kya galat/incomplete hai?
4. Fixed-offset layout parsing ko O(1) kaise banata hai, text ko O(length)?

---

## Next
→ [`06-itch-protocol.md`](06-itch-protocol.md)

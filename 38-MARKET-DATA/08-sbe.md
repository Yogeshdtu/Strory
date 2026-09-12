# 08 — Simple Binary Encoding (SBE): schema, codegen, native-endian

## Prerequisites
- [`07-fix-and-fast.md`](07-fix-and-fast.md)
- [`05-binary-protocols.md`](05-binary-protocols.md) (byteswap ka
  measured cost — is lesson ka context)

## Yeh topic abhi kyun
Hamara `wire_protocol.hpp` (06) ek **hand-written** ITCH-style protocol
hai — real production systems aksar isse **generate** karte hain, ek
schema se. SBE isi approach ka ek well-known standard hai (FIX Trading
Community dwara maintained), aur uska ek design choice (native-endian) 05
ke byteswap-cost discussion ko directly continue karta hai.

---

## SBE kya hai — schema-first binary encoding

> **SBE = ek XML schema se, message structs + encode/decode code
> AUTO-GENERATE karna** — hand-written parser likhne ke bajaye.

```xml
<!-- illustrative -- SBE schema jaisa dikhta -->
<sbe:message name="AddOrder" id="1">
    <field name="orderId"   id="1" type="uint64"/>
    <field name="symbolId"  id="2" type="uint32"/>
    <field name="qty"       id="3" type="uint32"/>
    <field name="price"     id="4" type="int64"/>
    <field name="side"      id="5" type="char"/>
</sbe:message>
```

Ek codegen tool is schema se **struct definitions + accessor functions**
generate karta (multiple languages ke liye — C++, Java, ...). Result
conceptually hamare `AddOrderMsg` jaisa hi hai — par **hand-likha nahi**,
**generated**.

---

## Kyun schema-first (hand-written parser ka alternative)

| Hand-written (jaisa hum kar rahe) | Schema-generated (SBE) |
|---|---|
| Fast to write for a small protocol | Bade protocols (100+ message types) ke liye scale karta |
| Field-offset bugs manual review pe depend | Codegen tool guarantee karta offsets sahi hain |
| Naya field add karna = sab jagah manually update | Schema update + regenerate = automatically consistent |
| Versioning manual hai | SBE **built-in versioning** support karta (naye optional fields, backward-compat) |
| Har language ke liye alag hand-written code | EK schema, sab languages ke liye codegen |

Chhote teaching protocol (yeh folder) ke liye hand-written theek hai. Real
exchange jiske 50+ message types hain, multiple language bindings chahiye,
aur versioning zaroori hai — schema-first **operationally zaroori** ban
jaata.

---

## SBE ka design choice: NATIVE-endian, ITCH se alag

Yeh is lesson ka **honest, measured** hissa hai. 05 mein dekha:
byteswap sasta hai (**0.753 ns/swap measured**). To phir SBE **native-
endian** (host order — x86 pe little-endian, koi swap hi nahi) kyun
choose karta, agar swap-cost-saving itni chhoti hai?

**Sahi wajah performance nahi, yeh hain:**

1. **Codegen simplicity** — agar wire format = host format, generated
   accessor code mein **koi conditional swap-logic hi nahi likhni padti**.
   Ek poori class of "kya field X swap hua ki nahi" bugs structurally
   khatam ho jaati.
2. **Direct struct overlay, literally zero conversion code** — `m->price`
   seedha use ho sakta, `net_to_host_i64(m->price)` jaisa koi wrapper
   nahi chahiye. Kam code = kam bugs, aur codegen tool ke liye simpler
   output.
3. **Dominant trading platform x86/x64 hai** (little-endian) — agar
   almost sab consumers little-endian hain, native-endian choose karna
   **most consumers ke liye** swap-code hi eliminate kar deta (jinke liye
   yeh matter karta unke liye hi optimize kiya).

**Trade-off:** agar koi consumer big-endian machine pe hai (rare aajkal —
kuch legacy/specialized hardware), unhe swap karna padta — SBE yeh
"problem" ko majority ke liye shift kar deta minority pe, performance ke
liye nahi, **code-simplicity** ke liye.

> Yeh 05 ke Rule-2 lesson ka natural extension hai: byteswap sasta hai
> (measured), isliye SBE ka native-endian choice **speed** ke baare mein
> nahi hai — **correctness aur codegen simplicity** ke baare mein hai.
> Dono valid engineering reasons hain, bas alag reason se.

---

## SBE mein aur bhi kya milta (high level)

- **Optional fields + versioning** — schema mein field "since version N"
  mark ho sakta; purana parser naya field ignore kar sakta (`length`-driven
  skip, 06 ka Trap 1 wala pattern, formally schema mein baked).
- **Repeating groups** — variable-count nested structures (jaise ek order
  ke multiple fills) schema mein express ho sakte, generated code unhe
  safely iterate karta.
- **Direct buffer access API** — generated code typically `wrap(buffer,
  offset)` pattern deta, jo bilkul hamara `reinterpret_cast` overlay
  pattern hai (09), bas generated aur type-safe wrapper ke saath.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — sochna SBE "hamesha better" hai chhote protocols ke liye
Ek 5-message-type teaching protocol (yeh folder) ke liye codegen tooling
setup karna overkill hai. SBE ka faayda **scale** pe milta (bahut message
types, multiple languages, versioning needs).

### Trap 2 — native-endian ko "performance optimization" bolna
Jaisa upar dikha, byteswap already sasta hai. Native-endian ka asli
faayda **code-simplicity aur correctness**, marginal-sa raw-cycle-saving
nahi.

### Trap 3 — apna consuming system big-endian assume karna
Agar tumhara consumer x86/ARM (dono little-endian) nahi hai, native-endian
wire format tumhare liye **ulta** swap-burden create karega. Verify karo.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| SBE hamesha ITCH-style se better hai | Scale/versioning-dependent trade-off |
| Native-endian = speed optimization | Codegen-simplicity/correctness optimization (swap already sasta) |
| SBE = ek specific protocol jaisa ITCH | SBE ek ENCODING STANDARD hai, protocol-agnostic |
| Codegen sirf convenience hai | Bade protocols mein correctness-critical (manual offset bugs avoid) |

---

## Exercises

1. Ek naya venue 80 message types define karta, 3 languages (C++, Java,
   Python) mein clients support karna hai. Hand-written parser vs SBE —
   kaunsa choose karoge?
   <details><summary>Answer</summary>
   SBE (ya similar schema-first approach). 80 message types x 3 languages
   hand-likhna error-prone aur maintenance-heavy hai — ek schema change
   3 jagah manually sync karni padegi. Codegen se ek schema, teeno
   languages ke liye consistent, correct-by-construction code.
   </details>

2. Kyun native-endian choice ka "real" faayda measure karna mushkil hai
   agar tum sirf raw CPU cycles dekho?
   <details><summary>Answer</summary>
   Kyunki faayda cycles mein nahi hai (byteswap 0.753 ns/op measured, kisi
   bhi hot path ke liye negligible) — faayda **code ki simplicity aur
   correctness** mein hai (kam lines, koi swap-forget bug class, cleaner
   generated code). Yeh cheezein "ns/op" se measure nahi hoti, bug-count/
   maintenance-cost se hoti hain — ek alag axis hai jo micro-benchmark
   nahi pakadta.
   </details>

---

## Interview questions

1. SBE kya hai, aur hand-written parsers se yeh kaise alag hai?
2. SBE native-endian kyun choose karta — sahi wajah (measured cost se
   connect karke) batao.
3. Schema-first approach kis scale pe zaroori ban jaata?
4. SBE mein versioning/optional-fields kaise handle hote (high level)?

---

## Next
→ [`09-zero-copy-parsing.md`](09-zero-copy-parsing.md)

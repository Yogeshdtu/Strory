# 09 — Zero-copy parsing (measured): overlay-read vs owned-copy

## Prerequisites
- `36-LOW-LATENCY-CPP/22-zero-copy-patterns.md` (concept + 4 caveats —
  yeh lesson usi ko market-data context mein, MEASURED, dobara dekhta)
- `examples/03_simple_parser.cpp`, `05_zero_copy_parser.cpp`,
  `06_parser_comparison.cpp`

## Yeh topic abhi kyun
Ab tak (01-08) humne protocol DESIGN dekha. Yeh lesson wahi protocol
**parse karne ke DO tareekon** ko head-to-head measure karta hai — aur
yehi is poore folder ka sabse important, sabse measured result hai.

---

## Do tareeke -- recap (36/22 se)

```
NAIVE (03/04): har message -> memcpy full struct -> byteswap -> OWNED
               ParsedEvent -> push_back into a (growing) std::vector

ZERO-COPY (05): reinterpret_cast overlay SEEDHA buffer pe -> sirf
                zaroori fields byteswap -> seedha PRE-ALLOCATED fixed
                sink mein likho -- koi owned copy nahi
```

```cpp
// NAIVE -- 03_simple_parser.cpp
AddOrderMsg m{};
std::memcpy(&m, buf, sizeof m);              // poora struct copy
ev.price_ticks = net_to_host_i64(m.price_ticks);
events.push_back(ev);                         // vector mein push (grow ho sakta)

// ZERO-COPY -- 05_zero_copy_parser.cpp
const auto* m = reinterpret_cast<const AddOrderMsg*>(buf);  // overlay, NO copy
book[sym].last_price = net_to_host_i64(m->price_ticks);      // seedha fixed sink mein
```

---

## Measured (`06_parser_comparison.cpp` — SAME feed, SAME run)

```
same feed (200000 messages), same box, same run:

  naive (03/04)                p50   30.1   p99    110.2   p99.9     891.7   max  1269363.2  ns
  zero-copy (05)               p50   30.1   p99     30.1   p99.9      40.1   max    11511.9  ns

p99.9 ratio (naive / zero-copy) = 22.2x
```

- **p50 barabar hai** (30.1 vs 30.1) — typical case mein dono roughly
  equal memory-touching work karte (naive header bhi utna hi bada, memcpy
  bhi cheap hai typical case mein).
- **p99.9 mein 22.2x fark** — yehi allocation tail hai (36/04 se yaad
  karo: `std::vector::push_back` bina reserve ke, periodically **grow**
  karta — realloc + copy). Zero-copy path mein **koi allocation hi nahi
  hai** (fixed `std::array<SymbolState, 32>` pre-allocated), isliye koi
  tail nahi.
- **`04_parser_benchmark.cpp` (naive standalone) ka max ~1.7 ms tha** —
  yeh (35/06 se yaad karo) unpinned Windows box pe OS-interrupt noise
  ho sakta, **par p99.9 (771-892 ns range, dono runs mein consistent)
  akela hi kaafi hai** — yeh allocator ka real signal hai, max nahi.

**Nichod:** binary format hona (05) sirf zero-copy **possible** banata —
yeh measurement dikhata ki agar tum phir bhi owned-copy-into-container
style likhte ho (03), tum us possibility ka faayda hi nahi utha rahe.

---

## Yeh kaam karta hai kyun — 4 caveats (36/22 se, market-data context mein)

### 1. Alignment
Hamare wire structs `#pragma pack(1)` hain -> `alignof == 1` (verified
`01_message_structs.cpp` mein `static_assert`). Isliye `reinterpret_cast<
const std::byte*>` se `const AddOrderMsg*` mein cast **`-Wcast-align` bhi
trigger nahi karta** (target alignment source se zyada nahi hai).

### 2. Endianness
Overlay-read karne ke baad bhi **har multi-byte field ko `net_to_host*()`
se swap karna zaroori hai** (05, 10) — overlay sirf COPY avoid karta,
byte-order conversion abhi bhi manual hai (aur zaroori hai).

### 3. Lifetime
`const auto* m = reinterpret_cast<...>(feed.data() + offset)` — `m` sirf
tab tak valid hai jab tak `feed` (ya jo bhi buffer hai) alive hai aur
REUSE nahi hua. Ek network-receive loop mein, agar tum buffer ko next
`recv()` se pehle reuse kar dete ho aur `m` ko kahin store kar liya tha
(bina copy kiye), woh dangling ho jaayega.

### 4. Aliasing
`std::byte*`/`char*` se struct-pointer cast **read** ke liye accepted
idiom hai (char types "anything ko alias" kar sakte, 36/22 mein already
established). Hamare examples isi established pattern ko follow karte.

---

## Kab zero-copy WORTH NAHI hai (honest trade-off)

- Agar message ka data **outlive karna hai** buffer se (jaise ek "last 10
  trades" history rakhni hai) — tab **copy zaroori hai**, ek baar,
  minimal (36/22 ka "jab copy karna PADE, kam karo" principle).
- Agar parsing itni rare hai ki performance matter hi nahi karta (jaise
  ek admin/config message, ek din mein kuch baar) — naive approach
  simpler hai, aur simplicity ki apni value hai (37/24 ka "measure first,
  measurably faster na ho to mat karo" — yahan lagta agar tumhara path
  truly cold hai).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — "binary protocol" aur "zero-copy parsing" ko same samajhna
05 se yaad karo: binary format zero-copy **enable** karta, guarantee
nahi. 03 ka naive parser bhi binary hi parse kar raha tha — bas woh
possibility use nahi kar raha tha.

### Trap 2 — overlay pointer ko buffer se lambe time tak zinda rakhna
Buffer agli message ke liye reuse hoga (ring buffer, 30-NETWORKING).
Overlay pointer se jo bhi chahiye, USE-AND-DISCARD karo turant, ya
explicitly copy karo (caveat 3).

### Trap 3 — sochna zero-copy "free" hai, koi trade-off nahi
Zero-copy code naive-copy code se **kam readable** ho sakta (raw pointer
arithmetic, manual byteswap har jagah). Yeh maintenance cost hai jo
measure nahi hoti par real hai — hot path pe worth hai, cold path pe
shayad nahi.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Binary format = automatic zero-copy | Binary sirf POSSIBLE banata; naive parsing (03) phir bhi copy kar sakta |
| p50 mein bhi bada fark hoga | p50 barabar tha yahan — fark p99.9 (tail) mein hai |
| Zero-copy hamesha karna chahiye | Cold path/rarely-called code mein simplicity better ho sakti |
| Overlay pointer kabhi bhi store kar sakte | Lifetime caveat — buffer jitni der zinda hai, utni hi der valid |

---

## Exercises

1. `06_parser_comparison.cpp` mein p50 dono approaches ka barabar (30.1
   ns) hai, par p99.9 mein 22.2x fark hai. Yeh kya batata hai ALLOCATION
   ke behavior ke baare mein (36/01 se connect karo)?
   <details><summary>Answer</summary>
   Allocation (yahan `vector::push_back` ka occasional grow) **typical
   case mein cheap hai** (fast-path free-list/amortized) par **tail mein
   bahut mehnga** (realloc + copy sab existing elements ka) — bilkul
   36/01 ka pattern (`new`/`delete` p50 chhota, p99.9 bahut zyada). Yeh
   iss course ka repeated theme hai: allocation ka danger typical case
   mein nahi, TAIL mein hai.
   </details>

2. Ek developer kehta "hum zero-copy use kar rahe, isliye humara code fast
   hai" — bina measure kiye. Kya missing hai is claim mein?
   <details><summary>Answer</summary>
   "Zero-copy" ek design PATTERN hai, guarantee nahi. Agar overlay ke
   baad bhi kahin ek unnecessary owned copy ban rahi hai (jaise result ko
   `std::string`/`std::vector` mein daal diya), "zero-copy parsing" ka
   naam hone se real allocation nahi rukti. Rule 2: measure karo (35),
   claim mat karo.
   </details>

---

## Interview questions

1. Overlay-cast parsing aur owned-copy parsing ka measured p99.9 fark
   kitna tha (is folder ke examples mein), aur KYUN (kaunsa mechanism)?
2. Zero-copy ke 4 caveats batao (alignment, endianness, lifetime,
   aliasing) market-data context mein.
3. Kab zero-copy worth NAHI hai?
4. "Binary protocol" aur "zero-copy parsing" mein fark batao.

---

## Next
→ [`10-endianness-handling.md`](10-endianness-handling.md)

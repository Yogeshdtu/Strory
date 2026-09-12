# 03 — Project 2: Binary feed parser (simple → zero-copy)

## Prerequisites
- `02-project-market-data-simulator.md`
- `38-MARKET-DATA/05-binary-protocols.md`, `06-itch-protocol.md`,
  `09-zero-copy-parsing.md`, `10-endianness-handling.md`
- `43-HFT-OPTIMIZATION/13-case-study-feed-handler.md`

## Yeh topic abhi kyun

Wire pe bytes aate. Unhe `MdMessage` mein badalna parse hai. Yeh pipeline
ka pehla CPU-bound stage. 38/06 ne ITCH-style parsing sikhaya; yahan
capstone context mein v1 (simple) → v3 (fast), measure ke saath.

## v1 — portable, safe (`parse_v1`)

```cpp
inline std::size_t parse_v1(const std::uint8_t* buf, std::size_t avail, MdMessage& m) {
    if (avail < kWireSize) return 0;              // bounds check every frame
    m.type = static_cast<MdType>(buf[0]);
    m.side = static_cast<Side>(buf[1]);
    m.qty      = rd_u32_be_v1(buf + 2);          // shift-and-or, byte by byte
    m.seq      = rd_u64_be_v1(buf + 6);
    ...
}
```

`rd_u64_be_v1` = `for (i in 0..8) v = (v << 8) | p[i]`. Har host pe kaam
karta (alignment, endianness — kuch nahi maanta). Safe. Simple.

## v3 — memcpy + bswap, frame-validated once (`parse_v3`)

```cpp
inline std::uint64_t rd_u64_be_v3(const std::uint8_t* p) {
    std::uint64_t v;
    std::memcpy(&v, p, 8);            // one unaligned 8-byte load
    return __builtin_bswap64(v);      // one bswap instruction
}
inline void parse_v3(const std::uint8_t* buf, MdMessage& m) {   // no bounds check here
    ...
}
```

Do changes:
1. **Per-field shift loop → `memcpy` + `bswap`.** `memcpy` of a fixed
   small size compiles to a single (possibly unaligned) load; `bswap` is
   one instruction. GCC/Clang often fuse them into `movbe`. Fewer uops
   than 8 shift-or steps.
2. **Bounds check hoisted.** Caller ek baar frame-availability check karta
   (`avail >= kWireSize`), phir `parse_v3` bina check ke chalta. Ek branch
   per frame gaya.

**True zero-copy** (38/09) — ek step aur: `parse_v3` bilkul mat karo, ek
`struct MdWire` (exact wire layout) ke upar `std::memcpy`/`std::bit_cast`
se ek view banao aur accessor pe bswap karo. Yahan hum stack-struct-copy
version rakhte (safe, strict-aliasing-clean); true view `bit_cast` se
milta par `MdMessage` ko exact wire layout hona padega.

## Measure (`02_feed_parser.cpp`)

### Correctness gate first (43/01)
```
agreement : 200000 frames, 0 mismatches -> IDENTICAL
```
v1 aur v3 har frame pe **exact same** `MdMessage` dete. Yeh check speedup
se pehle. Fail → v3 reject.

### Throughput (this box, `-O2`)
```
parse_v1 (shift-and-or, bounds-checked) : ~1.76 ns/frame
parse_v3 (memcpy + bswap, frame-checked): ~1.76 ns/frame   (~1.0x)
```

**Honest Rule-2 null result.** `-O2` pe farak lagbhag zero. Kyun:
- Frame chhota (38 bytes), poora L1/regs mein.
- GCC v1 ke 8-iteration shift loop ko bhi unroll + combine kar deta —
  effectively wahi `movbe`/`bswap` sequence jo v3 explicitly likhta.
- Bounds check (`avail < kWireSize`) ek predicted-not-taken branch —
  ~free.

Yeh 43/08 (hot/cold split ~1%) jaisa — concept sahi hai, par **is scale
pe compiler pehle hi optimal code de raha**. Number chhupaya nahi.

**Kab v3 sach mein jeetta:**
- Text feed (FIX) — `atoi`/`atof`/`strtok` bahut mehnga; hand int-parse
  5–15× (43/13).
- Bahut bade messages / variable-length fields (SBE repeating groups).
- Compiler jo v1's loop ko unroll nahi karta (older, `-O1`, unusual
  target).

Isliye v3 pattern **default rakho** (yeh dher jagah jeetta hai, kabhi
haarta nahi), par is micro-bench pe number honest raho.

## HFT relevance

Real HFT feed handlers ka parse stage: ITCH/SBE binary → fields already
fixed-offset integers → "parse" ≈ `memcpy` + `ntohl` + a few branches.
Text feeds (some FIX venues) parse-heavy hote — wahan v3-style hand
parsing dominant. Aur **validation**: real handler har field range-check
karta (`parse_v1`'s `avail` check bas shuruaat hai), malformed frame ko
cold-path reject karta (38/11 framing).

## ⚠️ Traps

### Trap 1 — `reinterpret_cast<const MdMessage*>(buf)` (true zero-copy) galat kar dena
Sirf tab valid jab `MdMessage` = exact wire layout (packed, right field
order, right endianness) AND `buf` correctly aligned. Warna strict-aliasing
UB + wrong values. `std::bit_cast` / `memcpy` into a stack struct safe hai.
25/strict-aliasing.

### Trap 2 — endianness bhoolna
Wire BE, host LE. `memcpy` alone galat value deta — `bswap` zaroori. v1
`(v<<8)|p[i]` implicitly BE-decodes (correct). Test on the actual host.

### Trap 3 — bounds check poora hata dena
`parse_v3` bina check ke chalta — par caller ne frame availability ensure
ki honi chahiye. Real feed pe partial frame at buffer end common (38/11) —
wahan check karo, warna OOB read.

### Trap 4 — micro-bench mein parse ko 10× dikhana, pipeline mein 2%
Parse pipeline ka chhota hissa ho to Amdahl (43/03). `11_mini_hft_engine`
mein parse ~45 ns/msg dikhta par usme ~40 ns rdtsc-probe hai — real parse
work < 5 ns. Book stage (~150 ns) hi asli target tha.

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| `memcpy` copy karta = slow | Fixed small size → single load; compiler ka intent hint |
| v3 hamesha v1 se tez | `-O2` + small frame pe compiler pehle hi optimal (Rule-2) |
| Zero-copy = `reinterpret_cast` | Layout-exact + aligned zaroori; warna `bit_cast`/`memcpy` |
| Parse tez → pipeline tez | Amdahl — parse ka % dekho pehle |

## Exercises

1. `02_feed_parser.cpp` ko `-O1` pe compile karo. v1 vs v3 ab?
   <details><summary>Answer</summary>
   `-O1` pe GCC v1's shift loop ko itna aggressively unroll/combine nahi
   karta → v3 (explicit memcpy+bswap) thoda tez dikh sakta. `-O2` ne gap
   khaya. Isliye benchmarks hamesha `-O2` (35/03, 43/02).
   </details>

2. Wire ko native-endian (LE, no bswap) bana do. `parse_v3` ab kya banega?
   <details><summary>Answer</summary>
   `parse_v3` = ek `std::memcpy(&m_fields, buf, ...)` — bilkul zero
   transformation (LE host pe). Fastest possible. Par ab wire non-portable
   (BE host galat parse karega). Real protocols BE (network order) rakhte
   isliye — portability > 1 bswap.
   </details>

3. `MdMessage` ka field order wire layout se match kar do aur `#pragma
   pack`. Ab true zero-copy view kaise?
   <details><summary>Answer</summary>
   `const MdWire* w = std::bit_cast<...>` nahi (bit_cast needs same size,
   trivially copyable) — better: `MdWire w; std::memcpy(&w, buf,
   sizeof w);` phir `w.seq_be` pe `bswap` accessor. Ya agar wire LE +
   aligned: `const MdWire* w = reinterpret_cast<const MdWire*>(buf)` (packed
   struct, `has_unique_object_representations`). Alignment + aliasing
   caveats — 25.
   </details>

## Interview questions

1. Big-endian wire ko LE host pe decode — do tareeke, trade-offs?
2. "Zero-copy parsing" ka exact matlab? `reinterpret_cast` kab safe, kab UB?
3. v1 (shift loop) aur v3 (memcpy+bswap) `-O2` pe same speed kyun ho sakte?
4. Feed parser mein bounds/validation kahan (hot path vs cold path)?

## Next
→ [`04-project-order-book.md`](04-project-order-book.md)

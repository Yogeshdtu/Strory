# 07 — Array-of-Structs vs Struct-of-Arrays

## Prerequisites
- [`05-padding-and-alignment.md`](05-padding-and-alignment.md), [`06-packed-structs.md`](06-packed-structs.md)
- `09-ARRAYS/10-array-performance.md` (`07_aos_vs_soa.cpp` — naapa hua 5.4–5.8×)
- `07-LOOPS/09-loop-performance.md` (cache locality)

## Yeh topic abhi kyun
Ek "entity" ke kai fields hain, aur aapke paas laakhon entities hain. Store karne ke do
tareeqe — **AoS** (`vector<Entity>`) aur **SoA** (har field ka apna array). Yeh
**data-oriented design** ka core hai, aur HFT mein order books / analytics ka default layout.

---

## Do layouts

```cpp
struct Quote { int64_t ts; int32_t bid, ask, bidSz, askSz; int16_t venue, flags; };

// AoS -- Array of Structs
std::vector<Quote> quotes;
// memory: [ts0 bid0 ask0 ...][ts1 bid1 ask1 ...]...   -- ek struct ke saare fields saath

// SoA -- Struct of Arrays
struct QuotesSoA {
    std::vector<int64_t> ts;
    std::vector<int32_t> bid, ask, bidSz, askSz;
    std::vector<int16_t> venue, flags;
};
// memory: [ts0 ts1 ts2 ...] [bid0 bid1 bid2 ...] ...   -- ek field ki saari values saath
```

Analogy: AoS = har customer ki alag file (naam, phone, balance sab ek jagah). SoA = ek register sirf
naamon ka, ek sirf phone numbers ka, ek sirf balances ka. "Sabka balance jodo" → SoA mein ek hi register
palatna hai; "customer #42 ki poori detail" → AoS mein ek file kholni hai.

---

## Performance ka sawaal: kaunse fields chhoote ho?

### Kuch fields (SUBSET) → SoA jeet-ta hai

"Saare quotes ka max bid" — sirf `bid` chahiye.

- **AoS**: `for (const Quote& q : quotes) m = max(m, q.bid);` — stride `sizeof(Quote)` (32 B). Har 64-byte
  cache line mein 2 quotes → 4 B use karne ke liye 32 B load. **~8x bandwidth bekaar.**
- **SoA**: `for (int32_t b : soa.bid) m = max(m, b);` — contiguous. Har cache line ka har byte ek `bid` hai.
  **100% kaam ka**, aur vectorize karna aasaan.

### Naapa hua (GCC 16.2, Zen 2, N = 4M, 30 reps)

`examples/04_aos_vs_soa.cpp` (is folder ka — `max(bid)`):

| Flags | AoS (stride 32 B) | SoA (contiguous) | SoA kitna tez |
|---|---|---|---|
| `-O2` | 213–219 ms | 92–94 ms | **~2.3×** |
| `-O2 -fvect-cost-model=cheap` | 213–216 ms | 30–31 ms | **~7×** |
| `-O3 -march=native` | 209–252 ms | 25–26 ms | **8–10×** |

`09-ARRAYS/examples/07_aos_vs_soa.cpp` (ek field ka read-modify-write, `-O2`): **5.4–5.8×**.

**Farq kyun badalta hai?** `-O2` pe GCC ka default vectorizer cost model ("very cheap") `max` wala SoA loop
vectorize **nahi** karta (trip count pehle se pata nahi → bache elements ke liye extra loop chahiye, jo
"very cheap" model mein allowed nahi) — dono loops scalar `cmov` chain rehte hain, aur SoA sirf kam memory
traffic se jeet-ta hai. Folder 09 ka RMW loop `-O2` pe hi vectorize ho jaata hai, isliye wahan gap bada. Cost
model dheela karo (`cheap`) ya `-O3 -march=native` do → SoA vectorize hota hai → gap 7–10×. AoS har flag pe
~210 ms pe atka rehta hai: `-O3 -march=native` pe woh bhi vectorize hua (assembly mein gather nahi, har element
ka alag `vmovd` load + `vpmaxsd`), par 4M × 32 B = 128 MB har pass — seema **memory bandwidth** hai.

**Kitna farq aayega, yeh operation + flags pe depend karta hai** — par field-subset access pe SoA lagataar
tez hai. Apna asli kernel naapo.

⚠️ **Benchmark ki galti jo yahan pakdi:** is example ka purana version loop seedha `main()` mein chalata tha
aur sirf aakhri rep ka result use karta tha. GCC 16.2 ne pehle 29 reps dead code maan ke hata diye — "30 reps"
~8 ms mein (128 MB × 30 = 3.8 GB, 8 ms mein padhna physically namumkin). Kernels ko `[[gnu::noipa]]` functions
mein daalne se har rep sach mein chala. **Number physically possible hai ya nahi — hamesha check karo.**

### Poori ENTITY ka access → AoS jeet-ta hai

"Entity `i` ke saare fields ke saath kuch karo" (jaise "order `i` pe fill lagao", "particle `i` render karo").

- **AoS**: entity `i` ke fields saath hain — ek ya do cache lines, ek load mein sab aa gaye.
- **SoA**: entity `i` ke liye 7 arrays se ek-ek element — **7 alag cache lines**, 7 sambhavit misses.

---

## Faisla

| Access pattern | Layout |
|---|---|
| Bahut entities scan, **kuch fields** chhoone (analytics, filtering, SIMD math) | **SoA** |
| **Ek entity ek baar mein, uske saare fields** | **AoS** |
| Mila-jula / pakka nahi | AoS (simple); profile karo; **AoSoA** socho (hybrid: chhote SoA blocks ka array) |
| Chhota data (L1/L2 mein aa jaaye) | Zyada fark nahi |

Aur: struct ka **size** (padding, file 05) dono ko badhata hai — phoola hua AoS struct har line mein zyada
bekaar karta hai; pehle members reorder karo.

---

## SoA ki suvidha (ergonomics)

SoA mein "ek object" wali suvidha chali jaati hai. Helpers:

```cpp
struct QuotesSoA {
    std::vector<int64_t> ts;
    std::vector<int32_t> bid, ask;
    std::size_t size() const { return ts.size(); }
    void push(int64_t t, int32_t b, int32_t a) { ts.push_back(t); bid.push_back(b); ask.push_back(a); }
    // entity i ke liye "reference proxy":
    struct Ref { int64_t& ts; int32_t& bid; int32_t& ask; };
    Ref operator[](std::size_t i) { return { ts[i], bid[i], ask[i] }; }
};
```

C++ mein built-in SoA container nahi hai. Options: `boost::pfr` + custom code, EnTT (game ECS), ya haath se
likha. Math ke liye `std::experimental::simd` (libstdc++ mein hai, GCC 16.2 pe compile karke dekha).

---

## AoSoA — hybrid

```cpp
struct QuoteBlock {                 // 8 quotes, block ke andar SoA
    std::array<int64_t, 8> ts;
    std::array<int32_t, 8> bid, ask, bidSz, askSz;
};
std::vector<QuoteBlock> blocks;     // SoA blocks ka array
```

Har block kuch cache lines mein aa jaata hai aur 8-wide vectorize hota hai, jabki ek poori entity bhi lagbhag
paas-paas hai (ek hi block mein). High-end SIMD code aur kuch HFT feed handlers mein use hota hai.

---

## Andar kya hota hai

- **AoS single-field scan**: stride wale loads, `ek_line_mein_records = 64 / sizeof(struct)`. Effective bandwidth
  = asli bandwidth × `field_size / stride`. Vectorizer ko har stride se alag-alag load karke vector banana padta
  hai — bandwidth ki seema nahi hatti.
- **SoA scan**: seedhe sequential loads, poori bandwidth, prefetcher stream pakadta hai, vectorizer packed loads
  + SIMD reduction banata hai (flags/cost model allow kare to).
- **SoA whole-entity**: har array se ek load — addresses door-door → spatial locality nahi, zyada fields ho to
  TLB pressure.
- AoS mein struct padding stride badhata hai → aur bura. (File 05.)

> **HFT relevance:** Order books aur market-data analytics **SoA / column layout** hote hain:
> `std::array<int64_t, Depth> prices; std::array<int64_t, Depth> qtys; ...` — "saare prices" ya "saari qtys"
> scan karna contiguous memory chhoota hai aur vectorize hota hai. Per-order processing (match, fill) AoS-style
> order records use karti hai. Feed handlers kabhi SIMD decode ke liye AoSoA. Layout ka faisla har hot kernel ke
> liye alag, naap ke, review karke hota hai — aur flags bhi: wahi SoA loop `-O2` pe 2.3× aur `-O3 -march=native`
> pe 8–10× nikla. Folders 32, 36, 39, 43.

---

## Hands-on

```bash
./build.ps1 fast 11-STRUCTS/examples/04_aos_vs_soa.cpp                    # max(bid): -O2 pe ~2.3x
./build.ps1 fast 09-ARRAYS/examples/07_aos_vs_soa.cpp                     # RMW: ~5.5x
# dono -O3 -march=native pe bhi -- gap badalta hai:
g++ -std=c++20 -O3 -march=native 11-STRUCTS/examples/04_aos_vs_soa.cpp -o aos3 && ./aos3
# kaunsa loop vectorize hua:
g++ -std=c++20 -O2 -fopt-info-vec-optimized 11-STRUCTS/examples/04_aos_vs_soa.cpp -o aos
```

---

## ⚠️ Traps

### Trap 1 — field-subset hot scan ke liye AoS
```cpp
for (auto& q : quotes) sum += q.bid;   // ⚠️ har line ka 7/8 bekaar. SoA
```

### Trap 2 — whole-entity processing ke liye SoA
```cpp
for (i) match(soa.px[i], soa.qty[i], soa.id[i], soa.side[i], ...);   // ⚠️ har entity pe N cache misses
```

### Trap 3 — SoA arrays ka sync bigadna
```cpp
bid.push_back(b);   // ask.push_back(a) bhool gaye -> arrays alag lambai -> bug
```
(Ek `push()` helper mein lapeto jo saare arrays update kare.)

### Trap 4 — chhote data ke liye layout micro-optimize karna
L1 mein aa jaata hai → AoS vs SoA se lagbhag fark nahi. Pehle profile.

### Trap 5 — AoS mein struct padding ko nazarandaz karna
Jo struct 24 bytes ka hona chahiye woh 40 ka hai → AoS scan 1.67x zyada bytes chhoota hai. SoA se pehle
reorder karo (file 05).

### Trap 6 — aisa benchmark jiske reps compiler hata de
```cpp
for (int r = 0; r < REPS; ++r) { int m = 0; for (auto& q : v) m = std::max(m, q.bid); result = m; }
// ⚠️ sirf aakhri rep ka result use -> GCC 16.2 -O2 ne 29 reps hata diye
```
Kernel ko `[[gnu::noipa]]` function mein rakho (ya har rep ka result jodo) aur number ko sanity-check karo.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "SoA hamesha tez" | Sirf field-subset access pe; whole-entity pe AoS jeet-ta hai |
| "AoS vs SoA hamesha 10x ki baat hai" | Kernel + flags pe: isi machine pe 2.3× se 10× tak — naapo |
| "SoA bas code style hai" | Alag memory layout → alag cache behaviour |
| "AoS ke liye struct size matter nahi karta" | Padding stride badhata hai (file 05) |
| "C++ mein SoA container hai" | Nahi — haath se ya library |
| "Loop vectorize ho gaya to tez" | AoS `-O3 -march=native` pe vectorize hua, phir bhi bandwidth pe atka |
| "`-O2` har simple loop vectorize karta hai" | Default "very cheap" cost model unknown trip count wala `max` loop chhod deta hai |

---

## Exercises

1. **Dono benchmarks:** is folder ka `04_aos_vs_soa.cpp` (max reduction) aur folder 09 ka `07_aos_vs_soa.cpp`
   (RMW) `-O2` pe chalao. RMW ka gap bada kyun hai?
   <details><summary>Answer</summary>

   `-fopt-info-vec-optimized` se dekho: `-O2` pe folder 09 ka SoA RMW loop vectorize hota hai, par yahan ka SoA
   `max` loop nahi (GCC ka default "very cheap" cost model unknown trip count wala loop chhod deta hai). Scalar
   `cmov` chain compute-bound hai, isliye yahan sirf ~2.3×. `-fvect-cost-model=cheap` do → SoA 30 ms, gap ~7×.
   </details>

2. **Whole-entity task:** `04_aos_vs_soa.cpp` mein ek task jodo — "har quote ke liye `mid = (bid + ask) / 2`,
   `sum(mid)` jodo". Ab kaunsa layout jeet-ta hai? (Kernel `[[gnu::noipa]]` mein rakho.)

3. **Padding × AoS:** `Quote` ko phoola do (bura member order, +20 bytes padding). AoS scan dobara chalao. Slow
   hua? 32-byte version se kitna?

4. **SoA helper:** `QuotesSoA` ko `push()`, `size()` aur `Ref` proxy `operator[]` ke saath banao. Whole-entity
   loop mein use karo.

5. **AoSoA:** `QuoteBlock` (8-wide) + `std::vector<QuoteBlock>` banao. Saare blocks mein `bid` scan karo. Pure AoS
   aur pure SoA se time milao.

6. **`-O3 -march=native`:** saare layouts `-O2` aur `-O3 -march=native` pe chalao. Kya compiler ka AoS
   vectorization gap band karta hai?
   <details><summary>Answer (is machine pe, `04_aos_vs_soa.cpp`)</summary>

   Nahi — ulta gap badha. AoS vectorize hua (gather nahi, alag-alag `vmovd` loads) par ~210 ms pe hi raha
   (bandwidth-bound); SoA 93 → 25 ms. Gap 2.3× → 8–10×.
   </details>

---

## Interview questions

1. AoS vs SoA — memory layout ka fark?
2. Field-subset access pe SoA kyun jeet-ta hai (cache line ka hisaab)?
3. Whole-entity access pe AoS kyun behtar?
4. AoS/SoA ka speedup itna kyun badalta hai (operation, vectorization, flags)?
5. Struct padding AoS performance ko kaise affect karta hai?
6. AoSoA kya hai, kab use hota hai?
7. Benchmark ka result "bahut hi tez" aaye to kya check karoge?

---

## Next
→ [`08-unions.md`](08-unions.md)

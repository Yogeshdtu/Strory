# 08 — Exception cost: "zero-cost" ka matlab, measured

## Prerequisites
- `04-stack-unwinding.md`, `03-exceptions-basics.md`
- `35-PROFILING-BENCHMARKING` ka basic idea (`-O2`, steady_clock) — ya bas
  [`examples/04_exception_cost.cpp`](examples/04_exception_cost.cpp)

## Yeh topic abhi kyun
"Exceptions slow hain" — half-truth. "Exceptions zero-cost hain" — bhi half-truth.
Sach: **happy path (koi throw nahi) pe overhead ~0; `throw` ki path bahut mehngi.**
Is file mein hum ise **measure** karte hain, aur phir samajhte hain ki latency-
sensitive code ke liye iska kya matlab hai.

---

## "Zero-cost" / "zero-overhead" EH model

Modern C++ (Itanium ABI, GCC/Clang/MSVC) **table-based** exception handling use
karta hai:

- `try` block enter/exit pe **koi instruction nahi**. Koi register save nahi, koi
  "setjmp". Compiler sirf side tables likhta hai (`.gcc_except_table` /
  `.eh_frame`): "PC range X mein ho aur unwind karna pade → yeh dtors chalao / yeh
  handler."
- Iska ulta purana **"sjlj" (setjmp/longjmp) EH** tha — har `try` pe runtime cost,
  ab sirf kuch embedded targets pe.

Toh "zero-cost" = **jab exception nahi hoti, tab.** `throw` hone pe unwinder woh
tables padhta hai (cold code, cold data, aksar page/cache miss), dtors chalata
hai, RTTI se handler match karta hai. Yeh mehnga hai.

Trade-off tha: normal path fast rakho, exceptional path slow chalne do. Zyada
tar programs ke liye achha trade. Latency-jitter-sensitive ke liye nahi.

---

## Measured — `examples/04_exception_cost.cpp`

GCC 15.1.0, `-O2`, x86-64 (MinGW ucrt), is machine pe. Teen scenarios, har ek
exception-style vs return-code-style.

```
benchmark                                   total       per-iter
-----------------------------------------------------------------
1. happy: try/catch (0 throws)           ~31.5 ms     ~1.58 ns
1. happy: return-code                    ~31.5 ms     ~1.57 ns
   -> ratio ~1.00x

2. throw: every iteration              ~1240 ms     ~6200 ns    <- per throw+catch
2. return-code: every iteration           ~0.30 ms     ~1.50 ns
   -> ratio ~4000x

3. 0.1% errors: try/catch                ~155 ms      ~7.7 ns
3. 0.1% errors: return-code               ~31 ms      ~1.55 ns
   -> ratio ~5x
```

### Padhne ka tareeka

1. **Happy path: ratio ~1.0.** `try`/`catch` wrap karne se hot loop **utna hi
   fast** raha jitna plain return-code check. Yeh "zero-cost" ka proof — jab throw
   nahi hota, `try` region literally muft hai.

2. **Ek `throw`+`catch` ≈ 6,000+ ns.** Ek return-code check ≈ 1.5 ns. Yaani `throw`
   **~4,000x** mehnga. Yeh sirf ek data point (exception object chhota, handler
   paas), par order-of-magnitude sahi hai: **microseconds, nanoseconds nahi.**
   Pehli baar aur bhi zyada (cold `.eh_frame` pages, unwinder code I-cache miss).

3. **0.1% error rate pe ratio ~5x.** Jab errors *sach mein* durlabh hain, throw ki
   average cost amortize ho jaati — ~8 ns/iter, abhi bhi theek. Fail rate jitna
   badhega, throw utna zeher. 10% pe yeh loop return-code se ~50x slow ho jaayega.

---

## `throw` mehnga kyun — breakdown

Ek `throw std::runtime_error("x")` roughly:

| Step | Kya | Approx |
|---|---|---|
| `__cxa_allocate_exception` | exception object ke liye memory (usually a special heap/emergency buffer) | ~malloc |
| exception ctor | `runtime_error("x")` — string refcount/copy | small |
| `__cxa_throw` → `_Unwind_RaiseException` | **search phase**: har frame ka personality routine, `.gcc_except_table` lookup | walks stack |
| **cleanup phase** | har frame ke dtors invoke, SP unwind | ∝ frame count |
| handler match | RTTI: `type_info` compare / base-chain walk | small |
| transfer | registers/SP restore, jump to `catch` | small |
| `__cxa_end_catch` | exception object free | ~free |

Sab **cold code + cold data**. Branch predictor iske liye kuch nahi kar sakta.
I-cache/D-cache/TLB — sab miss, kam se kam pehli baar. Isliye 1000s of ns.

---

## Binary size / build

- Exceptions on → har function ke liye unwind tables (`.eh_frame`,
  `.gcc_except_table`), landing pads. Typical **5–15% bada `.text`+metadata**.
- `-fno-exceptions` → yeh sab hatata hai, aur compiler ko zyada inline/optimize
  karne deta hai (cleanup paths ke bina simpler CFG). File `09`.

---

## Kab exceptions "theek" hain (cost ke bawajood)

- **Startup / config / CLI parsing** — ek baar chalta, latency irrelevant, "throw
  a clear message and exit" hi chahiye.
- **Genuinely rare + genuinely exceptional** — out of memory, disk full, corrupt
  state. Error rate ~0 → amortized cost ~0.
- **Deep call stacks jahan har level pe manual forwarding = bada boilerplate** aur
  woh path hot nahi hai.
- **Constructors** jinke paas return value nahi.

## Kab nahi

- **Hot loop / event loop** — market data decode, order matching, risk checks.
- **Expected failures** — "parse miss", "order rejected", "key not found",
  "would block". Yeh throw nahi, **value** hain.
- **Jab latency *variance* (P99/P99.9) matter karti** — ek 6 µs throw ek visible
  spike hai.

---

## Andar kya hota hai (aur measure kaise karein sahi se)

- **`-O2` zaroori.** `-O0` pe sab kuch 5–10x slow, aur "zero-cost" model ka
  point hi chala jaata (compiler tables optimize nahi karta, sab kuch materialize
  hota).
- **Loop se error condition ko unpredictable rakho** — warna compiler `throw` ko
  hoist/eliminate kar sakta. Example runtime data array use karta hai.
- **Sink ko print karo** — warna dead-code elimination poora loop uda deta.
- **Alag N per scenario** — 20M throws = 2 min. All-error case chhota (200k) rakho.

---

## > **HFT relevance**
> Yeh file ka number — **~6 µs per throw** — woh reason hai jispe HFT hot path
> exception-free hota hai. Ek market-data burst mein agar har 1000th message pe ek
> `throw` ho (sequence gap, unknown symbol), toh 6 µs × (burst rate / 1000) =
> steady latency tax **plus** ek visible P99.9 spike har baar. Return-code / `enum`
> path: ~1.5 ns, branch-predicted, jitter-free.
>
> Aur "happy path zero-cost" ka bhi HFT interpretation: agar aap `-fexceptions`
> ke saath build karte ho par hot path mein kabhi throw nahi karte, **runtime
> cost technically ~0** — par aap `-fno-exceptions` phir bhi choose karte ho, do
> aur reasons se: (1) code size / inlining (I-cache), (2) discipline — compiler
> enforce karta hai ki koi galti se `throw` na kar de (e.g. `std::vector::at`,
> `std::stoi`, `std::make_shared` ka `bad_alloc`).

---

## Hands-on

```bash
./build.ps1 fast 23-ERROR-HANDLING/examples/04_exception_cost.cpp
```

`fast` = `-O2` (benchmark ke liye zaroori). Do–teen baar chalao, numbers stable
hone chahiye. Phir:
- Bench 2 ka `N` badhao (say 2M) — throw path minutes lega; return-code ms.
- `throw BadValue(x)` ko `throw std::runtime_error("bad")` se badal ke dekho —
  frequ­ently similar, kyunki cost unwinding mein hai object size mein nahi.
- `-O0` pe chala ke dekho — happy-path ratio 1.0 se hat jaayega (noise), yeh
  demonstrate karta hai ki benchmark `-O2` pe hi meaningful hai.

---

## ⚠️ Traps

### Trap 1 — `-O0` pe exception benchmark
Numbers meaningless. "Zero-cost" model `-O2`+ ka hai.

### Trap 2 — compiler ne `throw` ko optimize kar diya
```cpp
for (int i=0;i<N;++i) { try { if (i<0) throw 1; } catch(...) {} }
// i<0 kabhi nahi -> compiler poora try/catch uda deta -> "throw is free!" (galat conclusion)
```
Error condition ko runtime data se banao.

### Trap 3 — "happy path ~0" se "exceptions free hain" nikalna
Happy path ~0. `throw` ~6 µs. Dono sach.

### Trap 4 — micro-benchmark se production latency assume karna
Warm caches, hot handler, chhoti stack. Production mein pehla throw thande pages pe
— 2–5x zyada. Aur deeper stack = zyada dtors.

### Trap 5 — throughput dekhna, jitter nahi
Average throughput mein 0.1% throws chhup jaate. HFT mein P99.9 dekho — wahan har
throw dikhta hai.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "exceptions har jagah slow" | Happy path pe ~0 overhead (`-O2`, table-based) |
| "exceptions zero-cost, freely use karo" | `throw` ~µs. Sirf happy path free |
| "`try` block likhne ki cost hai" | Nahi — cost `throw` execute hone pe |
| "chhota exception object = sasta throw" | Cost unwinding/tables/RTTI mein, object size mein nahi |
| "`-fno-exceptions` sirf `throw` hatata" | Code size ghatta, inlining badhta, `at()`/`stoi` compile-error ban jaate |
| "0.1% errors pe throw fine hai, toh 5% bhi" | 5% pe woh loop return-code se ~50x slow |

---

## Exercises

1. **Predict:** `-O2` pe, ek loop jo 1,000,000 baar chalta, har iteration ek
   `try { work(); } catch(...) {}` jisme `work()` kabhi throw nahi karta. Overhead
   vs bina try/catch?

   <details><summary>Answer</summary>

   ~0 (ratio ~1.0). Table-based EH — `try` region enter/exit pe koi instruction
   nahi. Example bench 1 yahi dikhata hai.
   </details>

2. **Estimate:** ek strategy jo 500,000 market updates/sec process karti, aur har
   2000th update pe ek `throw` (sequence gap). Throw cost 6 µs. Steady-state
   latency tax per update?

   <details><summary>Answer</summary>

   250 throws/sec × 6 µs = 1500 µs/sec = 0.15% CPU on average — chhota lagta hai,
   par har throw ek 6 µs **spike** hai us particular update pe → P99.9 latency 6 µs
   se kharab. Return-code path: har update ~1.5 ns, koi spike nahi.
   </details>

3. **Benchmark bug:** dost ka code "throw is basically free" claim karta hai:
   ```cpp
   auto t0 = now();
   for (int i=0;i<1'000'000;++i) try { maybe_throw(i); } catch(...) {}
   print(now()-t0);
   ```
   `maybe_throw` mein `if (i == -1) throw 1;`. Bug?

   <details><summary>Answer</summary>

   `i` kabhi `-1` nahi → compiler `throw` ko dead code samajh ke poora
   `try/catch` remove kar deta hai. Woh "no throw ever" measure kar raha hai, jo
   sach mein ~0 hai — par "throw is free" galat nikla. Real throw ke liye
   condition runtime-unpredictable honi chahiye.
   </details>

4. **When ok:** in me se kaunse exceptions ke liye theek: (a) config file missing
   at startup, (b) `map::find` miss in hot loop, (c) `malloc` returns null,
   (d) UDP packet CRC fail at 1M pps.

   <details><summary>Answer</summary>

   (a) theek — startup, rare, "die with message". (c) theek-ish — truly
   exceptional, rare, recovery hard (or `abort`). (b) nahi — expected, hot,
   `find`/`contains`. (d) nahi — expected at high rate, counter + drop.
   </details>

5. **Size:** ek project `-fexceptions` se `-fno-exceptions` pe switch karta hai
   aur `.text` 8% chhota ho jaata, kuch functions ab inline hote hain jo pehle
   nahi. Kya hua?

   <details><summary>Answer</summary>

   Unwind tables / landing pads / cleanup code gaya. Cleanup paths ke bina CFG
   simpler → inliner ka cost model ab un functions ko inline karne deta hai →
   aur optimization. Plus `.eh_frame`/`.gcc_except_table` sections shrink.
   </details>

---

## Interview questions

1. "Zero-cost exceptions" — exactly kya zero hai, kya nahi?
2. Table-based vs setjmp/longjmp EH — farq, kyun table-based jeeta?
3. Ek `throw` ki approx cost (order of magnitude)? Kahan jaati hai woh time?
4. `-O0` pe exception benchmark kyun bekaar?
5. 0.1% error rate pe exceptions theek, 10% pe nahi — kyun (amortization)?
6. `-fno-exceptions` `throw` hatane ke alawa aur kya badalta hai?
7. Throughput theek dikhe par latency P99.9 kharab — exceptions ka kya role ho sakta?

---

## Next
→ [`09-no-exceptions-hft.md`](09-no-exceptions-hft.md)

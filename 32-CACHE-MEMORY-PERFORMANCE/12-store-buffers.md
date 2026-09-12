# 12 — Store buffers, write combining, non-temporal stores

## Prerequisites
- `27-ATOMICS-MEMORY-MODEL/` (store buffer, memory ordering)
- `31-CPU-ARCHITECTURE/06-out-of-order-execution.md`
- `06-prefetching.md` (NTA hint)

## Yeh topic abhi kyun
Ab tak reads pe focus tha. **Writes** ka apna machinery hai: har store L1 mein
seedha nahi jaata — pehle ek **store buffer** mein baithta hai aur
asynchronously drain hota. Isse store latency mostly hide ho jaati. Par store
buffer bhar sakta hai (stall), aur write-only bulk data cache ko pollute kar
sakta hai — jiske liye **non-temporal stores** hain. Yeh mechanism `memcpy`,
packet TX, aur bade buffer fills ko samajhne ke liye zaroori hai.

---

## Store buffer (aka store queue)

Jab CPU ek `mov [addr], val` retire karta hai, `val` seedha cache mein nahi
jaata — woh ek **store buffer entry** mein jaata (address + data + status).
Store buffer se L1 mein drain **background mein** hota hai jab:
- Line already L1 mein M/E state mein hai → quick.
- Line nahi hai → RFO (read-for-ownership): line ko exclusive laao, phir
  merge karo.

Faayde:
- **Store latency hidden.** CPU ko store "complete" hone ka wait nahi —
  entry buffer mein daali, aage badh gaya. Retirement store buffer full na
  ho tab tak nahi rukta.
- **Store-to-load forwarding.** Agar koi baad ka load usi address ko padhta
  hai jo abhi buffer mein pending store hai → CPU **buffer se hi** value de
  deta, L1 round-trip bina. (Partial overlap / size mismatch pe forwarding
  fail → stall — "store forwarding stall", a real perf counter.)

### Store buffer full → stall
Buffer chhota hai (~50-70 entries, uarch pe depend). Agar aap **bahut tezi
se** store karte ho aur lines RFO mein atak jaati hain (har store ek naya
line, sab miss) → buffer bhar jaata → retirement rukta → pipeline stall.

Yeh tab hota hai jab aap ek bade array ko **randomly** ya **write-miss-heavy**
pattern se likhte ho. Sequential writes theek — prefetcher / RFO pipeline
kar lete.

---

## RFO — writes cost a read too

Ek "cold" line pe likhne ke liye CPU ko woh line **pehle padhni** padti hai
(exclusive ownership + existing bytes, taaki partial write merge ho sake).
Yeh **Read For Ownership**. Matlab: ek write to a cold line = ek cache miss
(the RFO) + the write.

Isliye ek `memset` / array-fill ki cost bhi ~read bandwidth jaisi hoti hai —
aap "sirf likh rahe" ho par har line pehle aati hai.

**Exception: full-line writes.** Agar aap ek poori 64-B line write karte ho
bina uske purane bytes padhe (e.g. AVX-512 full-line store, ya rep stosb
optimized) → CPU RFO skip kar sakta ("write allocate" avoid) — kuch uarch pe.
NT stores yeh explicitly karte (neeche).

---

## Write-combining (WC) buffers

Ek alag chhota set of buffers (~4-10, line-sized) jo **partial writes** ko ek
line ke liye combine karte before pushing out — taaki 16 alag 4-byte stores
ek hi 64-B burst ban jaayen.

Primarily **WC memory type** ke liye (MMIO, GPU framebuffers, `PCIe` BARs) —
jahan har store device pe jaana hota aur individual stores mehnge honge. Bhi
NT stores WC buffers use karte.

WC buffer partial rehta to eventually flush hota (buffer pressure, `SFENCE`,
serializing instruction, or a read to the same line). Yeh ordering-weak hai —
`SFENCE` chahiye guarantee ke liye.

---

## Non-temporal (streaming) stores

```cpp
#include <immintrin.h>
_mm256_stream_ps(dst, vec);          // MOVNTPS  -- 32 B, bypass cache
_mm256_stream_si256((__m256i*)dst, v);
_mm_stream_si64(dst, val);           // scalar NT store
// AFTER a batch of NT stores, before anyone reads them:
_mm_sfence();                        // make them globally visible in order
```

NT store: data ko cache mein daale bina seedha memory ki taraf (WC buffer ke
through) bhejta. Faayde jab aap **write-once, won't-read-soon** bulk data
likhte ho:
- **Cache pollution avoid** — aapka hot L2/L3 data evict nahi hota us bulk
  write se.
- **RFO avoid** — full-line NT stores line ko pehle nahi padhte ("no write
  allocate") → ~half the memory traffic of a normal fill.

Use cases:
- `memcpy` of large buffers (glibc `memcpy` NT stores above a ~size
  threshold, e.g. > L3/2).
- Packet assembly for TX — you write it once, the NIC DMAs it, you never
  read it back.
- Zeroing / filling a huge buffer you'll populate later.
- Streaming compute output larger than LLC (`c[i] = a[i] op b[i]` over GBs).

**Jab NT ULTA:**
- Data turant dobara padhi jaayegi → ab woh cache mein nahi → full miss.
- Partial-line NT stores → WC buffer inefficiency, can be slower.
- Small copies → the `sfence` + WC setup overhead dominates.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `memset`/fill ko "free" maan lena
Har cold line = RFO (a read miss) + write. 100 MB fill ≈ 100 MB of read
bandwidth. Use NT stores for large write-only fills, or fill lazily.

### Trap 2 — NT stores bina `sfence`
NT stores weakly ordered. Doosra thread (ya same thread ka baad ka
normal-ordered code, ya the NIC) unhe out-of-order / late dekh sakta.
`_mm_sfence()` after the batch, before publishing.

### Trap 3 — NT stores for data you'll re-read
It's not in cache now → your next read is a full DRAM miss. NT sirf genuinely
use-once bulk output.

### Trap 4 — store buffer stall ko "compute slow" samajhna
Random / write-miss-heavy store pattern → store buffer full → retirement
stall. `perf`: `resource_stalls.sb` (store buffer). Fix: sequential write
order, or NT stores, or fewer distinct lines written.

### Trap 5 — store-to-load forwarding stall
Ek chhota store (4 B) phir ek bada overlapping load (8 B) usi jagah se → CPU
buffer se forward nahi kar paata (size/alignment mismatch) → load waits for
the store to hit L1. Common with type-punning / unions / bitfield read-after-
write. Keep read and write sizes/alignments matched.

### Trap 6 — mixing NT and normal stores to the same region
Ordering between them is not guaranteed without fences; and you lose the
"no pollution" benefit if the normal stores pull the lines in anyway.

---

## > **HFT relevance**

> - **Packet TX buffers → NT stores + `sfence`.** You build the wire message
>   once, the NIC reads it via DMA, you never touch it again → NT stores keep
>   your hot book/order data in L2/L3 undisturbed.
> - **Large replay / snapshot writes → NT.** Writing a multi-MB book snapshot
>   or a log segment → NT so it doesn't flush the hot working set.
> - **Hot ring buffer → normal stores.** The SPSC ring the consumer reads
>   microseconds later must stay in cache → normal stores, and mind store-to-
>   load forwarding on the sequence numbers (match sizes).
> - **Watch `resource_stalls.sb` and `ld_blocks.store_forward`** in the
>   bench harness — store-side stalls are easy to miss when you only look at
>   load misses.
> - **`memcpy` size awareness** — know your libc's NT threshold; for hot-path
>   small copies it's plain stores, for cold bulk it's NT.

---

## Hands-on

```bash
# NT vs normal store for a big write-only fill (Linux/any x86):
#   normal: for(i) buf[i] = v;         -> RFO per line, pollutes cache
#   NT:     for(i) _mm256_stream_si256(&buf[i], v); _mm_sfence();
#   time both for buf >> L3; NT should be faster AND leave your other data hot.

./build.ps1 asm 32-CACHE-MEMORY-PERFORMANCE/examples/02_stride_access.cpp
# (dekho compiler kaunse stores emit karta)

# Linux perf:
perf stat -e resource_stalls.sb,ld_blocks.store_forward,offcore_requests.all_data_rd ./prog
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "store turant cache mein jaata" | store buffer mein, async drain |
| "write-only fill sasta hai" | RFO per cold line = ~read bandwidth |
| "NT store hamesha tez" | re-read soon → full miss; small → sfence overhead |
| "NT store bhi ordered hai" | weakly ordered — `sfence` chahiye |
| "store latency matters" | mostly hidden by buffer; buffer-full stall matters |
| "store-forwarding always works" | size/align mismatch → forwarding stall |

---

## Exercises

1. `for (i = 0; i < N; ++i) big[i] = 0;` where `big` is 200 MiB. Estimate the
   memory traffic with normal stores vs `_mm256_stream_si256`.

   <details><summary>Answer</summary>

   **Normal stores**: each 64-B line is cold → RFO reads it (64 B from DRAM)
   then you overwrite it → line later written back (64 B to DRAM). ≈ 200 MiB
   read + 200 MiB write = **~400 MiB traffic**, and all 200 MiB churns through
   L2/L3 evicting whatever was hot. **NT stores**: no RFO (full-line,
   no-write-allocate), data goes DRAM-ward via WC buffers, nothing cached.
   ≈ **~200 MiB traffic** (write only), zero cache pollution. ~2× less
   bandwidth and your hot data survives. Add one `_mm_sfence()` at the end.
   </details>

2. Aapke code mein `union { uint64_t u; struct { uint32_t lo, hi; } p; };` —
   `x.p.lo = a; x.p.hi = b; use(x.u);` — profiler `ld_blocks.store_forward`
   high dikhata hai. Kya ho raha, fix?

   <details><summary>Answer</summary>

   Two 4-byte stores (`lo`, `hi`) followed by an 8-byte load (`x.u`) of the
   same location. The store buffer holds two 4-B pending stores; the 8-B load
   needs **both** — the CPU generally can't forward from *multiple* stores to
   one wider load → **store-forwarding stall**: the load waits for both
   stores to drain to L1, then re-loads. ~10-15 cyc each time. Fix: do a
   single 8-B store — `x.u = (uint64_t(b) << 32) | a;` — then the 8-B load
   forwards cleanly from one matching store. Match store and load widths.
   </details>

3. NT stores se ek packet buffer banaya aur `sfence` laga diya, phir bhi
   kabhi-kabhi NIC ko purana data dikhta hai. Ek aur cheez check karo.

   <details><summary>Answer</summary>

   `sfence` NT stores ko **globally visible** aur mutually ordered banata,
   par: (a) the "packet ready" flag / doorbell write to the NIC must come
   **after** the `sfence` (program order + the flag being a normal store that
   the NIC polls) — if you write the doorbell before/around the sfence, the
   NIC sees "ready" before the data lands. (b) If the doorbell is MMIO (WC),
   it may also need ordering. Sequence: `NT stores → _mm_sfence() → write
   doorbell/flag`. Also ensure the buffer isn't being reused before the NIC
   signals TX-complete (DMA still reading old contents).
   </details>

---

## Interview questions

1. Store buffer — kya karta, aur store latency ko kaise hide karta.
2. Store-to-load forwarding — kya, aur woh kab fail (stall) hota.
3. RFO — cold line pe write ki cost kya, aur full-line write ka exception.
4. Write-combining buffers — kis memory type / use case ke liye.
5. Non-temporal store — 2 benefits (pollution, RFO), 2 failure modes.
6. NT stores ke baad `sfence` kyun zaroori.
7. `memcpy` bade sizes pe NT kyun use karta, chhote pe kyun nahi.

---

## Next
→ [`13-memory-bandwidth.md`](13-memory-bandwidth.md)

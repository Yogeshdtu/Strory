# 14 — Exercises: HFT concurrency (practice + pipeline building)

## Prerequisites
- Poora folder `41` (`01`–`13`) + `examples/`

## Kaise use karein
- **Part A** — concept recall.
- **Part B** — design/scenario questions.
- **Part C** — hands-on: `examples/` modify karo, predict karo, verify.
- **Part D** — extension challenges (harder, code likhna padega).
- Folder-wide interview questions end mein.

---

## Part A — Concept recall

### A1
Single-writer principle ki exact scope kya hai — poora program single-
threaded hona chahiye, ya kuch aur?
<details><summary>Answer</summary>
Har MUTABLE STATE ka ek fixed owner thread hona chahiye (per-variable
scope), poora program single-threaded hone ki zaroorat nahi (02).
</details>

### A2
SPSC queue aur Disruptor ka fundamental difference batao.
<details><summary>Answer</summary>
SPSC = work DISTRIBUTION (ek item, ek consumer, consume hote hi gone).
Disruptor = fan-OUT (ek item, MULTIPLE independent consumers, sab poori
history dekhte apni speed pe) (05).
</details>

### A3
Lock-free aur wait-free mein kya fark hai?
<details><summary>Answer</summary>
Lock-free: system-wide progress guaranteed, PAR ek specific thread
theoretically unbounded retry kar sakta (seqlock reader). Wait-free:
HAR thread bounded steps mein complete karta, retry-loop hi nahi hota
(atomic snapshot-swap read) (10).
</details>

---

## Part B — Design / scenario questions

### B1
Ek engineer sochta hai "seqlock hamesha shared_mutex se behtar hai,
hamesha use karna chahiye." Kab yeh galat ho sakta?
<details><summary>Answer</summary>
Agar reads bahut RARE hain aur writes bhi rare hain (koi high-contention
scenario nahi), `shared_mutex` ka simplicity/safety fayda seqlock ke
manual-retry-logic se zyaada matter kar sakta. Seqlock ka DRAMATIC fayda
SPECIFICALLY heavy-contention (frequent writes, frequent reads)
scenarios mein dikhta (06) — har jagah "hamesha behtar" nahi hai.
</details>

### B2
Ek naya matching-engine thread (HIGH priority) aur ek logging thread
(LOW priority) ke beech ek `std::mutex`-protected shared counter hai
(dono increment karte). Priority inversion ka risk kaise MITIGATE
karoge, is course ke principles se (mutex hataye bina bhi ek option
batao)?
<details><summary>Answer</summary>
Behtar fix: shared counter ko HATA do — har thread apna khud ka counter
rakhe (atomic, ya plain local variable), phir periodically (ya via
message-passing/queue, 03) combine karo. Mutex-based fix (priority
inheritance, 11) bhi kaam karta par HFT ka structural approach shared
resource ko poori tarah avoid karna hai.
</details>

### B3
Ek server mein 2 NUMA nodes hain. Tumhara pipeline (feed -> match ->
risk -> send) 4 stages ka hai. In stages ko kaise place karoge?
<details><summary>Answer</summary>
Saare 4 stages EK HI NUMA node pe (12's rule — ek pipeline ke stages
frequently SAME queues touch karte, cross-node cost avoid karna
zaroori). Agar 2 independent symbols/pipelines hain, ek Node 0 pe, ek
Node 1 pe (symbol-sharding NUMA ke saath naturally fit hota).
</details>

---

## Part C — Hands-on

### C1
`02_spsc_benchmark.cpp` mein `kCap` ko `1u << 6` (64) kar do (chhota).
Predict karo throughput pe kya asar hoga, phir verify karo.
<details><summary>Answer</summary>
Chhoti capacity ka matlab producer JALDI backpressure hit karega
(zyaada `try_push()` failures agar consumer thoda bhi peeche ho) —
throughput pass mein consumer ko HAMESHA turant pop karna padega, warna
producer spin karta rehta. Latency pass (paced, queue anyway shallow
rehti) pe zyaada asar nahi hona chahiye, throughput pass pe girawat aa
sakti agar consumer occasionally slow ho (OS scheduling).
</details>

### C2
`06_core_pinning.cpp` mein `kIters` ko 10x badha do. Kya `hiccups` count
(proportionally) same rehta ya badh/ghat jaata?
<details><summary>Answer</summary>
Proportionally roughly SAME rehna chahiye (agar OS jitter ek RANDOM,
roughly-constant-rate process hai) — par is unpinned desktop pe run-to-
run variance kaafi hai (05/06/13 ka established finding), isliye exact
number predict karna mushkil hai; TREND (pinned vs unpinned ka
comparison) consistent rehna chahiye.
</details>

### C3
`04_seqlock_snapshot.cpp` mein writer ko PACE karo (har write ke baad
`std::this_thread::sleep_for(1us)` add karo). Retry% pe kya asar
padega?
<details><summary>Answer</summary>
Retry% DRAMATICALLY kam ho jaayega — writer ab itna aggressive nahi
hai, readers ko beech mein pakadne ka chance bahut kam ho jaata (06's
exercise answer se predicted).
</details>

---

## Part D — Extension challenges

### D1 — Overwrite-oldest policy
`spsc_queue.hpp` ka `try_push()` currently "reject if full" karta.
Ek variant banao (`push_overwrite()`) jo full hone pe OLDEST item ko
OVERWRITE kar de (naya data hamesha jeetta, purana discard) — kis
use-case mein yeh behtar hoga "reject" se (hint: 06's BBO snapshot jaisa
"sirf LATEST matter karta" data)?

### D2 — Real wait-free snapshot
10's `std::atomic<std::shared_ptr<const Snapshot>>` pattern actually
IMPLEMENT karo — ek writer thread jo har 1ms ek naya `Snapshot` publish
karta, 4 reader threads jo continuously read karte. Compare karo
`Seqlock<T>` ke against — latency AUR memory-allocation-rate dono
measure karo.

### D3 — Multi-producer Disruptor
`03_disruptor.cpp`'s `DisruptorRing` currently single-producer hai
(`next_to_claim_` plain variable, no atomicity). Ek MULTI-producer
version banao (`claim()` ab `std::atomic<int64_t>` pe CAS use karega) —
kya naye correctness issues aate hain (hint: do producers ek SAME `seq`
claim kar sakte agar CAS sahi se na ho)?

### D4 — Full pipeline benchmark suite
`08_pipeline_demo.cpp` ko extend karo: ek 4th stage add karo ("risk
check," jo har trade ko ek simple check se guzarta — jaisa `qty <
max_qty`), aur poori pipeline ka per-stage latency BREAKDOWN measure
karo (na sirf end-to-end, balki "Stage1->Stage2 hop cost," "Stage2
internal cost," etc. alag-alag).

### D5 (challenging) — NUMA-aware pipeline (conceptual, code Linux pe)
Agar tumhare paas ek 2-socket Linux machine ho, `numactl`/`libnuma` use
karke `08_pipeline_demo.cpp` ke saare stages ko EK node pe pin karo
(`numactl --cpunodebind=0 --membind=0 ./pipeline`), aur SAME test do
NODES ke across split karke (stage2 node1 pe) chalao. Latency difference
measure karo — kya 12's predicted cross-node penalty CONFIRM hoti?

---

## Folder-wide interview questions

1. HFT pipeline model general multithreading se kaise alag hai.
2. Single-writer principle explain karo, ek example ke saath.
3. Shared-nothing design aur message-passing ka relationship.
4. `SpscQueue`'s cached-index optimization (28 se recall).
5. Disruptor pattern — gating sequences, batching, fan-out vs distribution.
6. Seqlock mechanism, aur is folder ka measured 2500-3500x finding
   (28's original se itna alag kyun).
7. Busy-spin vs blocking — dono axes (latency + CPU) explain karo.
8. Affinity aur isolation ka fark, aur `06`'s measured "pinned max worse"
   result ka mechanism.
9. Async logging ka architecture, aur drop-on-full policy kyun sahi hai.
10. Lock-free vs wait-free — exact difference, example ke saath.
11. Priority inversion ka classic scenario, aur HFT ka structural fix.
12. NUMA-aware placement symbol-sharding ke saath kaise combine hota.
13. Cross-thread event ordering kaise achieve karte ho (single machine
    vs multi-machine).
14. `08`'s pacing-experiment ka counter-intuitive result aur root-cause
    explain karo.

---

## Next
→ [`../42-HFT-NETWORKING/00-README.md`](../42-HFT-NETWORKING/00-README.md)

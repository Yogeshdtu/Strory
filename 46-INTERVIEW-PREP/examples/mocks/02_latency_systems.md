# Mock transcript — Latency / systems round (~40 min)

---

**Interviewer:** Give me the rough latency of: an L1 hit, a DRAM access,
a branch mispredict, a syscall, and a same-datacenter network round trip.

**Candidate:** Order of magnitude — L1 about 1 nanosecond, so a few
cycles. L2 ~4 ns, L3 ~10–20 ns. A DRAM miss ~60–100 ns. A branch
mispredict is a pipeline flush, ~15–20 cycles, so roughly 5 ns. A syscall
~100–300 ns even for a trivial one. A context switch ~1–3 microseconds
plus the cold-cache aftermath. SSD read tens of microseconds. Network
RTT in a datacenter ~10–100 microseconds; kernel-bypass wire-to-app can
get to ~1 microsecond.

`[note: correct orders of magnitude, uses "cycles" and "ns" fluently.]`

**Interviewer:** A pointer-chasing loop — walking a linked list — is
slow. The CPU is out-of-order, why can't it hide the misses?

**Candidate:** Because each iteration's load depends on the previous
one's result — `node = node->next`. It's a serial dependency chain. The
CPU can't issue load N+1 until load N returns, and each of those is
likely a cache miss to DRAM. Out-of-order execution hides latency by
finding *independent* work to do around a stall, but here there is none.
That's why hot code uses flat arrays or index-based structures — the
addresses are known ahead, the loads run in parallel, and the hardware
prefetcher engages.

**Interviewer:** Your feed handler's p99.9 latency spikes every ~100 ms.
You run `perf record` and the profile looks completely flat — no hot
function. Where do you look?

**Candidate:** A flat on-CPU profile with a periodic tail spike tells me
the cost is **off-CPU** — `perf record` only samples code that's actually
running, so time spent blocked is invisible. I'd use `offcputime` from
bcc or a bpftrace script to see where threads block and for how long, and
`perf sched timehist` and `perf sched latency` for scheduler delays and
involuntary context switches. I'd also run `perf stat -e
page-faults,context-switches,cpu-migrations` for per-run counts. The
~100 ms period smells like a timer or a batch boundary. Candidates:
a synchronous log flush on the hot path, a minor page fault from an
allocation that isn't pre-faulted, a futex on a shared lock, or the
thread being migrated or preempted because the core isn't isolated.

**Interviewer:** Say it's minor page faults. Fix?

**Candidate:** Allocate everything the hot path needs at startup, touch
every page so it's mapped — a prefault pass — and `mlockall` with
`MCL_CURRENT | MCL_FUTURE` so nothing gets paged out. Then verify with
`perf stat -e minor-faults,major-faults` that steady state is ~0. If it's
a big `mmap`'d file, `MAP_POPULATE` or a warm-up read.

**Interviewer:** You have a counter incremented by four threads and it's
giving wrong totals sometimes. Walk me through it.

**Candidate:** That's a data race — `counter += 1` is a load, modify,
store, and interleaved threads lose updates. It's undefined behaviour,
which is why it's non-deterministic and sometimes appears to work; at
`-O2` the compiler can even register-promote the increment across the
loop, hiding the race entirely. First I'd confirm with ThreadSanitizer —
it points at the exact two racing lines and the variable, and it catches
the race even on a run where the timing happened not to interleave
badly. To fix: make it `std::atomic<long>` and use `fetch_add(1,
std::memory_order_relaxed)` — relaxed is fine because it's just a
counter, nothing else is published alongside it. Or a mutex if several
fields have to stay mutually consistent. Best of all, per-thread local
counters summed at the end — no shared write, no contention. Then verify:
a hundred-plus runs clean, or TSan clean.

`[note: names it as UB / data-race, mentions the -O2 register-promotion
subtlety, gives three fixes with the memory-order justification, and
insists on verification.]`

**Interviewer:** Why `relaxed` and not `seq_cst` there?

**Candidate:** `seq_cst` maintains a single total order across all
sequentially-consistent operations, which on x86 can mean a full barrier
— extra cycles. For a standalone statistics counter there's no other
data whose visibility we're ordering against it, so we only need the
atomicity, which is `relaxed`. If the counter's value gated a read of
some other data, I'd need acquire/release to establish happens-before,
and I'd write a comment explaining that argument. Default to `seq_cst`,
weaken deliberately.

---

## Rubric

| Dimension | Score 1–5 | Notes |
|---|---|---|
| Latency numbers | | Right orders of magnitude, used to justify design |
| Systems reasoning | | off-CPU vs on-CPU, page faults, scheduling, isolation |
| Concurrency | | data race = UB, TSan to confirm, memory-order justification |
| Verification instinct | | "then I'd measure / run it 100 times / TSan clean" |
| Communication | | hypotheses enumerated, next step concrete |

**Bar for a strong hire:** knows the numbers cold, recognizes an off-CPU
problem, and treats "add a lock" as one option among several with stated
trade-offs — plus always closes with how they'd verify.

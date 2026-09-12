# 01 — Why lock-free

## Prerequisites
- `27-ATOMICS-MEMORY-MODEL` (poora — atomics, memory ordering, `is_lock_free`, ABA)
- `26-CONCURRENCY` file 06 (mutex), file 11 (deadlock), file 16 (false sharing)

## Yeh topic abhi kyun
Folder 27 ne lock-free ki **definition** di (progress guarantee). Ab **motivation**:
ek `std::mutex` uncontended bahut sasta hai — toh phir lock-free kyun? Jawab
**contention** aur **latency ka tail** mein hai, average mein nahi.

---

## `std::mutex` ka asli cost

Uncontended `std::mutex::lock()` = ek atomic CAS (~13 ns is box pe, folder 27
file 06). Wahi tak toh theek hai. Problem tab shuru hoti jab do+ threads usse
**ek saath** chahte hain:

| Kya hota | Cost |
|---|---|
| Lock word CAS fail → thread ko **sona** padta | futex/`WaitOnAddress` **syscall** (~1–2 µs) |
| Sleeping thread ko wake | doosra syscall + **context switch** (~1–5 µs) |
| Lock word ka cache line cores ke beech ping-pong | ~tens–100+ ns per bounce |
| Lock-holder ko OS ne **preempt** kar diya | **saare** waiters us holder ke wapas schedule hone tak ruke (ms-scale, non-deterministic) |

Yeh sab **P99 / P99.9 latency** ko tod deta hai — average shayad theek dikhe, par
tail spike ho jata. HFT mein tail hi sab kuch hai.

---

## Teen classic mutex pathologies

### 1. Priority inversion
Low-priority thread ne lock liya → preempt hua → high-priority thread us lock pe
block → *medium*-priority thread (jo lock nahi chahta) CPU kha raha → high-priority
thread effectively medium ke peeche. (Mars Pathfinder ne isi se reboot kiya tha.)
Lock-free code mein koi "held lock" nahi → yeh ho hi nahi sakta.

### 2. Convoying
Ek thread lock hold karke ek slow operation (page fault, cache miss, syscall)
karta → baaki sab uske peeche line mein lag jate → throughput girta, aur convoy
tootne mein der lagti.

### 3. Deadlock / livelock
Do lock ulta order mein → hang (folder 26 file 11). Lock-free structures mein koi
lock nahi → deadlock structurally impossible.

---

## Lock-free kya **guarantee** karta (recap, folder 27 file 14)

- **Koi thread doosre ko block nahi kar sakta** OS-level pe. Ek thread kisi bhi
  point pe pause/preempt/crash ho jaye → baaki **progress karte rehte hain**.
- **System-wide progress** bounded steps mein (lock-free). SPSC jaise cases
  effectively **wait-free** — har thread bounded steps.
- **No deadlock, no priority inversion, no convoying.**

---

## Lock-free ≠ fast (yeh yaad rakho)

Folder ke examples yeh khud dikhate hain — **measure kiya, chhupaya nahi**:

| Example | Result is box pe |
|---|---|
| `06` SPSC hand-off | lock-free ring **~4–6× faster** throughput, p50 ~0.4 µs vs mutex ~1.2 µs / cv ~6 µs — **lock-free jeeta** |
| `05` seqlock vs `shared_mutex` | seqlock **~80–100× faster** reads — **lock-free jeeta bada** |
| `03` MPMC vs mutex+deque | lock-free **~1.5–1.75×** — thoda jeeta |
| `04` Treiber stack vs mutex+vector | lock-free **~0.2× — yaani 5× SLOWER** — **mutex jeeta** |

`04` ka sabak: ek single hot head pe CAS-loop + high contention = **retry storm**.
Lock-free hai (koi na koi progress karta) par throughput girta. Mutex + cache-
friendly `std::vector` behtar nikla.

**Lock-free chuno predictability ke liye** (no deadlock, no priority inversion,
bounded tail, preemption-tolerance) — headline throughput number ke liye nahi.
Aur **shape maayne rakhta**: SPSC / sharded → lock-free clearly better; ek hot
shared word pe sab threads CAS-loop → shayad mutex.

---

## > **HFT relevance**
> - **Hot path pe koi lock nahi** — market data lock-free SPSC/MPSC queues pe
>   aati hai, config/limits ek immutable snapshot hai jo atomic pointer swap se
>   publish hota (`12-seqlock.md`, folder 26 file 10). Ek contended mutex ka
>   futex syscall + context switch = P99 spike jab volume peak pe ho.
> - **Predictable tail > low mean.** Ek strategy jo average 800 ns pe react karti
>   par P99.9 pe 2 ms — woo 2 ms wale trades pe paisa haarti. Lock-free structures
>   preemption ke bawajood bounded rehti hain.
> - **Lock-free ka matlab careful design** — reclamation (`09`–`11`), ABA
>   (folder 27 file 15), testing (`13`). Galat lock-free code x86 pe pass hota,
>   ARM pe / load pe corrupt. Budget rakho.
> - **Sabse pehla lock-free tool: SPSC ring** (`04`) — simplest, wait-free in
>   practice, aur 90% hot-path hand-offs isse ban jate hain.

---

## Hands-on

```bash
./build.ps1 fast 28-LOCK-FREE/examples/06_mutex_vs_lockfree.cpp   # SPSC: lock-free jeeta
./build.ps1 fast 28-LOCK-FREE/examples/04_lock_free_stack.cpp     # contended stack: mutex jeeta
```

Dono chalao. `06` mein lock-free SPSC ka p50 hand-off aur throughput mutex/cv se
kaafi behtar. `04` mein contended Treiber stack mutex+vector se **slow** — wahi
lesson (lock-free ≠ fast).

---

## ⚠️ Traps

### Trap 1 — "lock-free hamesha fast"
Contended CAS-loop retry storm mutex se slow ho sakta (`04`). Lock-free = progress
guarantee, speed nahi.

### Trap 2 — "mutex hamesha slow"
Uncontended mutex ~13 ns. Cache-friendly critical section + low hold time =
`std::mutex` ka jeetna aam hai.

### Trap 3 — average latency dekhna
Lock-free ka faayda P99/P99.9 tail mein hai (no futex syscall, no preemption
stall). Average kabhi-kabhi mutex ke barabar.

### Trap 4 — "lock-free = koi atomic ordering nahi sochna"
Ulta — har atomic ka memory order haath se prove karna padta (folder 27).

### Trap 5 — reclamation ignore karna
Lock-free container se node **kab free karein** ek alag hard problem hai (`09`).
Galat → use-after-free.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "lock-free → faster" | Progress guarantee; speed shape pe depend (SPSC jeeta, contended stack haara) |
| "mutex → slow" | Uncontended ~13 ns; often the right choice |
| "lock-free → simpler" | Harder — ordering proofs, ABA, reclamation, testing |
| "lock-free → no starvation" | Individual thread starve ho sakta (wait-free nahi to); system progress guaranteed |
| "priority inversion mutex ka config issue hai" | Structural — koi held lock hi nahi to inversion possible nahi |

---

## Exercises

1. **Cost break-down:** ek contended `std::mutex::lock()` par 4 alag costs likho.

   <details><summary>Answer</summary>

   (1) CAS on the lock word (fails). (2) futex/`WaitOnAddress` syscall to sleep.
   (3) context switch out, and later back in (2 switches). (4) lock-word cache
   line bouncing between the contending cores. Plus the tail risk: the holder
   getting preempted stalls every waiter for a scheduler quantum.
   </details>

2. **Which wins?** (a) 1 producer → 1 consumer hand-off; (b) 16 threads pushing a
   shared LIFO; (c) 1 writer, 32 readers of a 64-byte snapshot.

   <details><summary>Answer</summary>

   (a) lock-free SPSC ring — clean win (`06`). (b) probably a mutex or sharded
   design — a single lock-free head is a retry storm (`04`). (c) seqlock —
   massive win over `shared_mutex` (`05`).
   </details>

3. **Priority inversion:** why can't it happen in a lock-free stack?

   <details><summary>Answer</summary>

   Priority inversion needs a low-priority thread to *hold* a resource a
   high-priority thread waits on. A lock-free stack has no held lock — a
   descheduled thread's half-done CAS just makes other threads' CAS fail and
   retry; nobody is blocked *on it*.
   </details>

4. **`04` result:** the lock-free Treiber stack lost to a mutex here. Give two
   reasons, and one thing you'd change to make lock-free win.

   <details><summary>Answer</summary>

   (1) Every op is a CAS-loop on one hot head (plus a free-list head) shared by 4
   threads → most CASes fail and retry (retry storm). (2) The mutex + `std::vector`
   baseline has a tiny critical section, a single-CAS lock, and all threads hit
   one hot cache line — very cache-friendly, low hold time, barely contends.
   To make lock-free win: change the shape — per-thread (sharded) stacks, or an
   SPSC/MPSC ring instead of a LIFO, so there isn't one contended point.
   </details>

5. **Tail vs mean:** why does an HFT shop care about P99.9 more than the average?

   <details><summary>Answer</summary>

   The slow tail events are exactly the ones that lose money — a 2 ms stall while
   the market moves means the fill you wanted is gone or adverse. A good average
   with a fat tail is worse than a slightly higher average with a tight tail.
   Lock-free structures keep the tail bounded under preemption/contention.
   </details>

---

## Interview questions

1. Uncontended vs contended `std::mutex` — cost ka farq, kaunse syscalls?
2. Priority inversion — kya hai, lock-free mein kyun impossible?
3. Convoying — kaise hota, kya effect?
4. "Lock-free = fast" galat kyun — ek concrete counter-example (contended stack)?
5. Lock-free kis metric ko sabse zyada improve karta (P99 tail)?
6. Kaunsi contention shape lock-free ke liye acchi (SPSC / sharded), kaunsi buri?
7. Lock-free adopt karne ki hidden costs (reclamation, ABA, testing)?

---

## Next
→ [`02-lock-free-rules.md`](02-lock-free-rules.md)

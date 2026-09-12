# 01 — Wire → NIC → kernel → app → NIC → wire: har step ki cost

## Prerequisites
- `41-HFT-CONCURRENCY` (poora)
- `30-NETWORKING` (poora — TCP/UDP/epoll/socket-options ki foundation)

## Yeh topic abhi kyun

41 ne dikhaya ek pipeline ke ANDAR (threads ke beech) latency kahan
jaati hai. Yeh folder us pipeline ke **PEHLE stage se PEHLE** shuru hota
hai — packet abhi wire pe hai, tumhare process tak pahunchne mein kitni
"stops" hain, aur har stop ki cost kya hai.

---

## Poora path — ek packet ka safar

```
[Exchange]  --wire-->  [NIC PHY]  --DMA-->  [Kernel network stack]
                                                    |
                                            [Socket buffer]
                                                    |
                                    recv() syscall  |  (context switch: user->kernel->user)
                                                    v
                                            [Your application]
                                                    |
                                          (decision, matching, etc.)
                                                    |
                                    send() syscall  |
                                                    v
                                    [Kernel network stack]  --DMA-->  [NIC PHY]  --wire-->  [Exchange]
```

Har arrow ek **cost** hai. Kaunsi jagah kitna time leti (typical, real
NIC/kernel pe — is course ke context mein already measured/discussed):

| Stage | Typical cost | Kahan cover hua |
|---|---|---|
| Wire → NIC PHY | Speed-of-light + cabling (12) | 12 |
| NIC → kernel (interrupt-driven) | ~1-5 µs (interrupt + DMA + driver) | 29 (syscall cost), yahan 03 |
| Kernel network stack (routing, socket demux) | Kernel-version/config dependent, kai µs ho sakta | 29 |
| Socket buffer → app (`recv()` syscall) | ~50-200 ns (syscall overhead alone, 29's measured syscall cost) + context switch | 29 |
| App processing | **Tumhare control mein** — 36/40/41 ka poora focus | 36, 40, 41 |
| App → kernel (`send()` syscall) | Same as recv() | 29 |
| Kernel → NIC → wire | TX queueing + DMA + PHY | 09 |

**Kernel bypass (03-06) poore "kernel stack" hisse ko HATA deta** — NIC
seedha user-space memory mein DMA karta, koi interrupt/syscall/context-
switch nahi. Yehi 1-5µs ka block, jo largest single chunk hai, GAYAB ho
jaata.

---

## "Kernel aapka dushman hai" — matlab kya hai

Kernel network stack **generality** ke liye design hui hai — routing,
firewalling, multiple protocols, fairness sabhi processes ke beech.
HFT ko in mein se KUCH NAHI chahiye (ek fixed multicast group, ek fixed
peer, koi routing decision) — par kernel HAR packet pe woh saari
machinery chalata hai chahe zaroorat ho ya na ho. Bypass libraries (03)
issi generality-tax ko skip karti hain.

> **HFT relevance:** 37/14's latency budget mein "network" ek line-item
> hai — is folder ka poora content us EK line-item ko microseconds se
> nanoseconds tak le jaane ke baare mein hai.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — "network latency" ko ek single number samajhna
Jaisa table dikhata hai, "network" khud KAI sub-stages hai (NIC, kernel,
syscall, app) — optimize karne se pehle pata hona chahiye KAUNSA stage
sabse zyaada le raha (measure, phir optimize — 35/36 ka established
principle, yahan bhi laagu).

### Trap 2 — bypass ko "sab kuch fix kar deta" samajh lena
Bypass sirf KERNEL stage ko hataata — NIC-to-PHY cost, switch latency
(12), aur app processing time (jo tumhari khud ki responsibility hai)
abhi bhi wahin hain.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Network latency = "internet slow hai" jaisi cheez | Kai discrete stages, har ek measurable aur optimizable |
| Kernel bypass "magic speed" deta | Sirf ek SPECIFIC stage (kernel stack) hataata, baaki stages wahin |
| App processing time "free" hai (network se alag) | Yeh bhi latency budget ka hissa hai, aur MOST controllable |

---

## Exercises

1. Ek system mein latency breakdown: NIC 2µs, kernel stack 3µs, syscalls
   0.3µs, app processing 0.5µs. Kernel bypass laga do (kernel stack ~0).
   Naya total kya, aur ab sabse BADA remaining contributor kaunsa hai?
   <details><summary>Answer</summary>
   Naya total ~2.8µs (2+0.3+0.5, kernel stack ka 3µs gaya). Ab sabse
   bada contributor NIC (2µs) hai -- agla optimization step waha jaana
   chahiye (jaisa ef_vi/DPDK jo NIC interaction bhi optimize karte, 05/06),
   na ki app processing pe (jo already sabse chhota hai).
   </details>

---

## Interview questions

1. Poora wire-to-wire path, stage-by-stage batao.
2. "Kernel dushman hai" ka exact matlab kya hai?
3. Kernel bypass EXACTLY kaunsa stage hataata, aur kya NAHI hataata?

---

## Next
→ [`02-multicast-receive-path.md`](02-multicast-receive-path.md)

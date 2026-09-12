# 17 — Exercises: HFT networking (practice + network optimization)

## Prerequisites
- Poora folder `42` (`01`–`16`) + `examples/`

## Kaise use karein
- **Part A** — concept recall.
- **Part B** — design/scenario questions.
- **Part C** — hands-on (Linux/WSL zaroori — is folder ke examples
  `.linux.cpp` hain, is repo ke Windows dev box pe verify nahi hote).
- **Part D** — extension challenges.
- Folder-wide interview questions end mein.

---

## Part A — Concept recall

### A1
Wire-to-wire path ke 3 major stages batao jahan latency add hoti (01 se).
<details><summary>Answer</summary>
NIC (interrupt + DMA), kernel network stack (routing/demux), syscall +
context-switch (recv()/send()). App processing alag, tumhare control
mein (01).
</details>

### A2
`SO_BUSY_POLL` aur app-level `MSG_DONTWAIT` spin ka fundamental fark.
<details><summary>Answer</summary>
`SO_BUSY_POLL`: KERNEL busy-polls (bounded budget), driver-dependent,
socket API unchanged. App-level spin: APPLICATION khud spin karta,
hardware-independent, LAGATAAR CPU burn karta (07).
</details>

### A3
Kyun TX timestamp `MSG_ERRQUEUE` se milta, seedha `sendto()` ke return
se nahi?
<details><summary>Answer</summary>
TX completion ek ASYNC kernel/driver event hai (packet actually NIC
tak jaane ke baad aata) -- `sendto()` khud sirf yeh confirm karta ki
data kernel ko SAUP diya gaya, uska actual TRANSMIT timestamp baad mein
error queue pe deliver hota (08).
</details>

---

## Part B — Design / scenario questions

### B1
Ek team apna latency requirement 50 microseconds se 500 nanoseconds
tak improve karna chahti. Unka current setup: kernel sockets, default
NIC settings. Kis ORDER mein optimizations try karoge?
<details><summary>Answer</summary>
1. Measure current baseline (35/05's principle) — kahan time ja raha
   hai, breakdown karo (14).
2. NIC tuning (09) + IRQ affinity (10) — SASTA, quick wins.
3. Thread pinning + isolated cores (41/08).
4. `SO_BUSY_POLL` (07) — agar NIC driver support kare.
5. Tab hi kernel bypass (03-06) consider karo, agar abhi bhi target
   se door ho — sabse mehnga/complex step LAST mein, jab pehle ke steps
   "kaafi" na ho.
</details>

### B2
Ek production system Onload use karta. Ek naya engineer `tcpdump` se
debug karne ki koshish karta, "koi packet nahi dikh raha" confuse ho
jaata. Kya explain karoge?
<details><summary>Answer</summary>
Onload traffic ko kernel BYPASS karta, isliye kernel-level capture
(`tcpdump`) usse invisible hai (04, 15). Onload-specific tools
(`solar_capture`) ya SPAN-port mirroring (15) chahiye.
</details>

### B3
Ek order-gateway team `TCP_NODELAY` sirf CLIENT-side set karti hai.
Kya problem ho sakti?
<details><summary>Answer</summary>
SERVER-side ka Nagle abhi bhi ON hai -- server se client ko jaane wale
(jaisa ack) packets Nagle-delay ka shikaar ho sakte. Dono taraf set
karna zaroori hai (11).
</details>

---

## Part C — Hands-on (Linux/WSL)

### C1
`01_multicast_receiver.linux.cpp` mein `seq % 777` ki jagah `seq % 100`
kar do (zyaada drops). Predict karo `gaps_detected` pe kya asar, phir
verify karo.
<details><summary>Answer</summary>
`gaps_detected` badh jaayega (5000/100 = 50 drops, vs pehle 6). Har
individual gap abhi bhi 1 seq missing hoga (pattern SAME, sirf zyaada
FREQUENT).
</details>

### C2
`03_receiver_benchmark.linux.cpp` ko real (loopback nahi) NIC pe chalao
(agar do machines available hon). Kya strategy C (SO_BUSY_POLL) ab
strategy B ke KAREEB aata hai?
<details><summary>Answer</summary>
Agar NIC driver `ndo_busy_poll` support karta hai (`ethtool -k` se
check karo), haan -- C, B ke kareeb aana CHAHIYE (07's prediction).
Agar driver support nahi karta, C abhi bhi A ke kareeb rahega, chahe
real hardware ho.
</details>

### C3
`05_order_gateway.linux.cpp` mein server-side `TCP_NODELAY` line COMMENT
OUT kar do. Kya latency pe koi measurable asar aata (loopback pe)?
<details><summary>Answer</summary>
Shayad chhota/measurable NAHI hoga LOOPBACK pe (bahut fast RTT, delayed-
ACK timer trigger hone se pehle hi response aa jaata) -- REAL network
pe yeh EXACT scenario 30/03 ka 40ms stall trigger kar sakta. Yeh khud
ek lesson hai: loopback SAB interactions reveal nahi karta.
</details>

---

## Part D — Extension challenges

### D1 — Recovery mechanism
`01_multicast_receiver.linux.cpp` mein ek "gap recovery" simulate karo
— jab gap detect ho, ek SEPARATE TCP connection pe "resend request"
bhejo (38/07's 3-layer recovery ladder ka simplified version), aur
missing message ko RECEIVE karke sequence complete karo.

### D2 — Multi-interface A/B feed
Do ALAG multicast groups (A/B redundant feeds, 38's concept) se receive
karo, `INetworkReceiver` ke DO instances use karke, aur jo pehle aaye
(seq-based arbitration) usse use karo, doosre ko discard karo.

### D3 — Real hardware timestamp verification
Agar tumhare paas ek Linux box + supported NIC hai: `ethtool -T eth0`
chalao, check karo `hardware-receive` capability hai ya nahi. Agar hai,
`04_hw_timestamps.linux.cpp` chalao aur verify karo `ts[2]` (hardware)
non-zero aata hai (is course ke loopback demo mein hamesha 0 aata).

### D4 (challenging) — DPDK minimal example
Agar tumhare paas DPDK-supported NIC + hugepage-configured Linux box
hai: DPDK ka "hello world" (`rte_eal_init` + `rte_eth_rx_burst` basic
loop) actually implement + measure karo. Kitna latency-improvement
milta kernel-socket baseline (01) ke against?

### D5 (challenging) — Full SPAN-port debugging setup
Agar tumhare paas managed switch access hai: ek SPAN/mirror port
configure karo, ek Onload-simulated (ya sirf normal) traffic setup pe,
aur verify karo `tcpdump` SPAN port pe traffic dekh sakta hai jabki
production port pe (bypass simulate karte hue) nahi.

---

## Folder-wide interview questions

1. Poora wire-to-wire path, stage-by-stage, aur har stage ki typical cost.
2. Kernel bypass ladder (kernel-socket → SO_BUSY_POLL → Onload → ef_vi →
   DPDK) — trade-offs har step pe.
3. `INetworkReceiver` abstraction ka business-value.
4. Software vs hardware timestamp ka fark, aur TX timestamp kaise milta.
5. PTP kya solve karta?
6. NIC tuning ke 4-5 major knobs (ring buffer, coalescing, GRO/LRO,
   pause frames) aur unke trade-offs.
7. IRQ affinity aur thread affinity ka relationship (kyun dono zaroori).
8. `TCP_NODELAY`, `writev`, keepalive, `TCP_USER_TIMEOUT` — order gateway
   context mein har ek ka role.
9. Cut-through vs store-and-forward switching.
10. FPGA kis specific use-case ke liye sensible hai, aur kyun "sabse fast
    isliye hamesha use karo" galat soch hai.
11. Wire-to-wire measurement design (timestamp-har-hop-pe) explain karo.
12. Kernel-bypass systems debug karne ka challenge, aur SPAN-port solution.
13. "Building the receiver" (16) ke 6 layers batao, aur kab poori design
    OVER-ENGINEERED hoti.

---

## Next
→ [`../43-HFT-OPTIMIZATION/00-README.md`](../43-HFT-OPTIMIZATION/00-README.md)

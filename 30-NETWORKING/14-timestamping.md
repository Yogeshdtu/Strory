# 14 — Timestamping: SO_TIMESTAMPING, hardware timestamps, PTP

## Prerequisites
- `29-LINUX-SYSTEMS` file 16 (clocks: MONOTONIC, TSC, vDSO)
- `07-sockets-api.md` (`recvmsg` / cmsg), `05-udp.md`

## Yeh topic abhi kyun
HFT latency ka har analysis "kab kya hua" pe khada hai: packet NIC pe kab aaya,
kernel ne kab dekha, tumne kab `recv` kiya, order kab wire pe gaya, exchange ne
kab match kiya. In sab ke liye **precise, comparable timestamps** chahiye —
software timestamps mein already kernel/scheduler jitter mila hota, isliye
**hardware timestamps** (NIC PHY pe) chahiye, aur unhe exchange ki clock ke
saath **PTP** se sync karna hota (sub-µs). Aur MiFID II jaise regulations bhi
timestamp accuracy demand karte. Example `09` `SO_TIMESTAMPING` demo hai.

---

## The timestamp points

```
   wire  ->  NIC PHY  ->  NIC/driver  ->  kernel stack  ->  socket buffer  ->  recv()  ->  your code
             ^HW RX TS    ^SW "hwtstamp"  ^SW RX TS                            ^app TS (you take)
```

- **Hardware RX timestamp:** the NIC stamps the packet at the PHY (or MAC) the
  moment it arrives, using the **NIC's own clock (PHC — PTP Hardware Clock)**.
  Most accurate — no kernel/scheduler noise. Needs NIC + driver support
  (`ethtool -T eth0`).
- **Software RX timestamp:** the kernel stamps it when the driver/softirq
  processes it — already includes IRQ + softirq latency, but cheap and always
  available.
- **App timestamp:** you call `clock_gettime` after `recv` — includes everything
  up to and including the scheduler waking you.
- **TX timestamps** (`SOF_TIMESTAMPING_TX_HARDWARE`/`_SOFTWARE`): when the packet
  left — delivered later via the **socket error queue** (`MSG_ERRQUEUE`).

**The gap between HW RX TS and app TS = the kernel + wakeup latency you're trying
to shrink** (`08`, `29/10`, `29/15`). Measuring it tells you where the µs go.

---

## Enabling `SO_TIMESTAMPING`

```cpp
int flags = SOF_TIMESTAMPING_RX_HARDWARE      // NIC stamps RX
          | SOF_TIMESTAMPING_RAW_HARDWARE     // deliver the raw PHC value
          | SOF_TIMESTAMPING_RX_SOFTWARE      // kernel RX stamp (fallback / comparison)
          | SOF_TIMESTAMPING_SOFTWARE
          | SOF_TIMESTAMPING_TX_HARDWARE      // (optional) TX stamps
          | SOF_TIMESTAMPING_TX_SOFTWARE
          | SOF_TIMESTAMPING_OPT_TSONLY;      // TX: don't loop the payload back, just the stamp
setsockopt(fd, SOL_SOCKET, SO_TIMESTAMPING, &flags, sizeof flags);
```

You also need the NIC's hardware timestamping turned on
(`SIOCSHWTSTAMP` ioctl, or `hwstamp_ctl -i eth0 -r 1`), and for HW RX on some
NICs you must enable it per-flow / all-packets.

## Reading the timestamps — `recvmsg` + cmsg

```cpp
char ctrl[256];
msghdr msg{}; iovec iov{buf, sizeof buf};
msg.msg_iov = &iov; msg.msg_iovlen = 1;
msg.msg_control = ctrl; msg.msg_controllen = sizeof ctrl;

recvmsg(fd, &msg, 0);

for (cmsghdr* c = CMSG_FIRSTHDR(&msg); c; c = CMSG_NXTHDR(&msg, c)) {
    if (c->cmsg_level == SOL_SOCKET && c->cmsg_type == SO_TIMESTAMPING) {
        auto* ts = reinterpret_cast<scm_timestamping*>(CMSG_DATA(c));
        // ts->ts[0] = software timestamp (timespec)
        // ts->ts[1] = deprecated (HW in a legacy transformed clock)
        // ts->ts[2] = RAW hardware timestamp (the NIC PHC's timespec)
    }
}
```

TX timestamps come back on the **error queue**:
```cpp
recvmsg(fd, &msg, MSG_ERRQUEUE);   // read completion + its scm_timestamping
```
(The example `09` reads RX timestamps; TX follows the same shape via
`MSG_ERRQUEUE`.)

---

## Clock domains — the thing that trips everyone

You now have timestamps from **different clocks**:
- NIC PHC (hardware TS) — its own oscillator.
- `CLOCK_REALTIME` (software TS base).
- Your app's `CLOCK_MONOTONIC` / `rdtsc`.

Subtracting a HW TS from an app `CLOCK_MONOTONIC` TS gives a number whose
**absolute value is meaningless** (different epochs, different rates) — only its
**variation (jitter)** is meaningful, unless you discipline the clocks together.

**To get comparable absolute times:**
1. **PTP (PTP4L)** disciplines the NIC PHC to a grandmaster clock (often the
   exchange's, or a GPS-backed source) — sub-µs, sometimes ~ns.
2. **`phc2sys`** disciplines the system clock (`CLOCK_REALTIME`) to the NIC PHC.
3. Now HW TS, kernel TS, and your `CLOCK_REALTIME` reads share a time base you
   can subtract.

---

## PTP (IEEE 1588) in one paragraph

A **grandmaster** broadcasts time; **boundary/transparent switches** correct for
their own queuing delay; **slaves** (your NIC PHC) run a servo loop:
`Sync` + `Follow_Up` (master→slave), `Delay_Req` + `Delay_Resp` (slave→master)
→ estimate offset and path delay → steer the PHC. With hardware timestamping at
each hop, accuracy is sub-µs (often tens of ns). NTP, by contrast, is ~ms —
useless for HFT ordering. Exchanges publish PTP; colocated boxes sync to it.

```bash
ethtool -T eth0                 # "PTP Hardware Clock: 0", HW RX/TX filters
ptp4l -i eth0 -m                # run the PTP slave (disciplines the PHC)
phc2sys -s eth0 -w -m           # sync system clock to the PHC
pmc -u -b 0 'GET CURRENT_DATA_SET'   # offset from master, path delay
chronyc tracking                # if using chrony+PTP refclock
```

---

## Uses in HFT

| Measurement | How |
|---|---|
| **Exchange → us latency** | exchange packet carries its send time (or you compare to a known reference); your **HW RX TS** minus that = wire + their egress. PTP-synced clocks required. |
| **Kernel + wakeup latency** | HW RX TS vs your app TS after `recv` — the gap you shrink with busy-poll / isolation / bypass. |
| **Internal pipeline stages** | one monotonic clock (or TSC), timestamp at each stage (feed decode, book update, signal, order encode) — find the slow stage. |
| **Order → wire latency** | TX HW TS (via `MSG_ERRQUEUE`) vs when your code called `send`. |
| **Regulatory (MiFID II RTS 25)** | timestamp trading events to ~µs (HFT) / ~ms, traceable to UTC — PTP-disciplined clocks + logged timestamps. |
| **Clock health monitoring** | `pmc` offset-from-master, `phc2sys` residuals — alert if PTP sync degrades. |

---

## ⚠️ Traps / Common mistakes

### Trap 1 — comparing timestamps from unsynced clocks
HW TS (NIC PHC) − app `CLOCK_MONOTONIC` = a number with a meaningless absolute
value. Discipline the clocks (PTP + `phc2sys`) or only look at jitter.

### Trap 2 — using software timestamps and calling them "hardware accurate"
Software RX TS already includes IRQ + softirq latency (and its jitter). For
"when did it hit the wire/NIC" you need `SOF_TIMESTAMPING_RX_HARDWARE` +
`RAW_HARDWARE`, and NIC support.

### Trap 3 — NIC HW timestamping not actually enabled
`SO_TIMESTAMPING` with HW flags but no `SIOCSHWTSTAMP` / `hwstamp_ctl` → you get
software timestamps only, silently. Check `ethtool -T` and the ioctl return.

### Trap 4 — reading TX timestamps from the normal recv path
TX completions/timestamps come on the **error queue** (`recvmsg(MSG_ERRQUEUE)`),
not the regular data path. Poll the error queue (it's `EPOLLERR` in epoll).

### Trap 5 — PTP without hardware timestamping
`ptp4l` in software-timestamp mode is ~µs-to-ms and jittery — barely better than
NTP. The whole point is **HW timestamps at the NIC**; without them PTP doesn't
deliver.

### Trap 6 — ignoring switch PTP behaviour
A plain (non-PTP-aware) switch adds variable queuing delay that PTP can't
correct → offset wanders. Need boundary/transparent-clock switches on the PTP
path (the exchange's colocation network provides them).

### Trap 7 — `NTP` and `PTP` both disciplining the system clock
`ntpd`/`chrony` and `phc2sys` fighting over `CLOCK_REALTIME` → oscillation. Pick
one authority (usually `phc2sys` from the PTP-disciplined PHC); disable the
other or make it a fallback only.

---

## > **HFT relevance**

> - **Hardware RX timestamps on the market-data NIC** — the reference point for
>   every latency measurement. Enable via `SO_TIMESTAMPING` (`RX_HARDWARE |
>   RAW_HARDWARE`) + `SIOCSHWTSTAMP`.
> - **PTP-discipline the NIC PHC** (`ptp4l`) to the exchange grandmaster, and
>   `phc2sys` the system clock to the PHC — sub-µs, traceable to UTC (also the
>   regulatory requirement).
> - **Measure HW RX TS → app TS** to quantify the kernel + wakeup latency you're
>   attacking with busy-poll / isolation / bypass (`08`, `13`, `29`).
> - **Timestamp every internal pipeline stage** with one monotonic clock / TSC
>   (`29/16`) to find the slow stage; timestamp `send` and read the **TX HW TS**
>   from the error queue for order → wire latency.
> - **Monitor PTP health** (`pmc` offset, `phc2sys` residual) and alert on
>   degradation — a drifting clock silently corrupts every latency number and
>   can breach regulatory accuracy.
> - **One clock authority** — `phc2sys` from the PHC; don't also run `ntpd`.

---

## Hands-on

```bash
# Linux pe -- example 09: SO_TIMESTAMPING, kernel-RX -> app gap
g++ -std=c++20 -O2 -pthread 30-NETWORKING/examples/09_timestamping.linux.cpp -o /tmp/ts && /tmp/ts

# NIC capability
ethtool -T eth0                 # PTP Hardware Clock id, HW rx/tx filters, SW/HW support

# is HW timestamping enabled on the NIC?
hwstamp_ctl -i eth0            # or: cat /sys/class/net/eth0/... (driver-dependent)

# PTP (needs a grandmaster on the network)
sudo ptp4l -i eth0 -m -S       # -S: software TS (test only). Real: hardware TS, no -S
sudo phc2sys -s eth0 -w -m
pmc -u -b 0 'GET TIME_STATUS_NP'   # offset from master
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "software timestamp accurate hai" | includes IRQ+softirq jitter; use HW TS for wire time |
| HW TS − app monotonic TS = latency | different clock domains; discipline or use jitter only |
| "PTP set kiya = sub-us" | only with **hardware** timestamping at the NIC |
| "TX timestamp recv se milega" | error queue (`MSG_ERRQUEUE` / `EPOLLERR`) |
| "NTP kaafi hai ordering ke liye" | NTP ~ms; HFT/regulatory needs PTP ~µs |
| "ntpd + phc2sys dono chalao" | two authorities fight; pick one (phc2sys) |

---

## Exercises

1. You compute `app_recv_ns - hw_rx_ts` and get values like `-4_000_000` (a
   negative 4 ms). What's wrong?

   <details><summary>Answer</summary>

   The two timestamps are from **different clocks**: `hw_rx_ts` is the NIC PHC
   (its own epoch/rate), `app_recv_ns` is your `CLOCK_MONOTONIC` (boot-relative)
   or `CLOCK_REALTIME`. Their absolute difference is meaningless and can be
   negative. To get a real number, discipline the PHC to a grandmaster (`ptp4l`)
   and the system clock to the PHC (`phc2sys`), then take both timestamps in the
   same disciplined domain (e.g. both `CLOCK_REALTIME`, or convert the PHC value
   with a known offset). Until then, only the *variation* of the difference
   (jitter) is usable.
   </details>

2. `ethtool -T eth0` shows hardware timestamping support, but your program only
   ever gets software timestamps. Why?

   <details><summary>Answer</summary>

   Capability ≠ enabled. You must turn on hardware timestamping on the NIC via
   the `SIOCSHWTSTAMP` ioctl (or `hwstamp_ctl -i eth0 -r 1` / your PTP daemon
   does it) — set the RX filter to "all packets" (or the right per-flow filter)
   and TX type. Without that, `SO_TIMESTAMPING` with HW flags silently falls
   back to software stamps. Check the ioctl's returned config and `hwstamp_ctl`
   output.
   </details>

3. What's the difference between what a *software* RX timestamp and a *hardware*
   RX timestamp tell you, for latency analysis?

   <details><summary>Answer</summary>

   Hardware RX TS: the instant the frame arrived at the NIC PHY/MAC — no kernel
   involvement, so it's your ground truth for "when the packet reached the box".
   Software RX TS: when the kernel's driver/softirq got to it — that already
   includes interrupt latency, softirq scheduling, and any coalescing, plus
   their jitter. `HW_RX → SW_RX` measures the kernel's early receive path;
   `HW_RX → app` measures the whole kernel + wakeup path (what busy-poll/bypass
   removes).
   </details>

4. Why does PTP need PTP-aware (boundary/transparent-clock) switches on the path?

   <details><summary>Answer</summary>

   A normal store-and-forward switch adds a **variable** queuing delay to each
   packet depending on load. PTP estimates path delay from `Sync`/`Delay_Req`
   exchanges assuming a stable, symmetric path; variable switch delay makes that
   estimate wander, so the slave's offset drifts. A **transparent clock** switch
   measures each PTP packet's residence time and writes a correction into the
   packet; a **boundary clock** switch terminates PTP and re-serves it. Either
   removes the switch's delay from the equation, keeping accuracy sub-µs. Plain
   switches → tens of µs of wander.
   </details>

5. Regulatory: you must timestamp trade events to microsecond accuracy,
   traceable to UTC. Outline the setup.

   <details><summary>Answer</summary>

   (1) A UTC-traceable time source: a PTP grandmaster with a GPS/GNSS reference
   (or the exchange's grandmaster which is itself traceable). (2) NIC with a PTP
   Hardware Clock and hardware timestamping (`ethtool -T`). (3) `ptp4l`
   disciplining the PHC to the grandmaster using **hardware** timestamps; PTP-
   aware switches on the path. (4) `phc2sys` disciplining the system clock to
   the PHC. (5) The application timestamps each reportable event with
   `CLOCK_REALTIME` (now disciplined) or the PHC directly, and logs it. (6)
   Continuous monitoring: `pmc` offset-from-master, `phc2sys` residual, alarms if
   accuracy budget is exceeded; keep the logs for the audit trail.
   </details>

---

## Interview questions

1. The timestamp points from wire to your code; which one is "ground truth".
2. `SO_TIMESTAMPING` — enabling HW RX stamps, reading them via `recvmsg` cmsg.
3. TX timestamps — why they come on the error queue.
4. Clock domains — why HW TS minus app TS is meaningless without discipline.
5. PTP vs NTP — accuracy, and why HFT needs PTP + hardware timestamps.
6. `ptp4l` + `phc2sys` — what each disciplines.
7. Why PTP needs boundary/transparent-clock switches.

---

## Next
→ [`15-network-tuning.md`](15-network-tuning.md)

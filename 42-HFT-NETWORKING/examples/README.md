# Examples — Folder 42 (HFT networking)

> **6 of 8 examples are `.linux.cpp`** — genuine Linux-only syscalls
> (multicast sockets, `SO_BUSY_POLL`, `SO_TIMESTAMPING`, `MSG_ERRQUEUE`,
> `TCP_NODELAY`/keepalive) that CANNOT compile on this repo's Windows/
> MinGW dev box, and this box has no WSL installed to verify them
> locally either. `./build.ps1 folder`/`checkall` **skip** `.linux.cpp`
> files entirely (the established pattern from `29-LINUX-SYSTEMS` and
> `30-NETWORKING` — see their `examples/` dirs). They were written
> carefully, reusing already-verified structural patterns from 30's
> files, and reviewed by hand for the usual pitfalls (sign-compare,
> unbounded blocking, empty-vector UB) — but **run them on a real
> Linux box (or WSL) to actually verify and measure**, per each file's
> `EXPECTED (... NAHI napa gaya)` comment block.

## What's genuinely new vs 30-NETWORKING (not a re-hash)

| This file | Builds on | What's actually new |
|---|---|---|
| `01_multicast_receiver.linux.cpp` | 30/06 (multicast) | `INetworkReceiver` abstraction (08) + realistic MdMessage + sequence-gap detection (38/09's mechanism, on a real socket) |
| `02_busy_poll_receiver.linux.cpp` | 30/10 (manual spin) | The REAL kernel `SO_BUSY_POLL` sockopt (different mechanism from app-level spin) |
| `03_receiver_benchmark.linux.cpp` | 30/10 (ping-pong) | One-way multicast (not round-trip) + a THIRD strategy (blocking+SO_BUSY_POLL) 30/10 didn't have |
| `04_hw_timestamps.linux.cpp` | 30/09 (RX-only) | TX+RX timestamps, BOTH kernel-clock — fixes 30/09's "different epochs, relative-only" limitation |
| `05_order_gateway.linux.cpp` | 30/03 (Nagle) | `writev` single-packet technique + keepalive/`TCP_USER_TIMEOUT` tuning, framed as a "gateway" |
| `06_wire_to_wire.linux.cpp` | 01+04 combined | Full 2-hop breakdown (MD receive → processing → order send), all kernel-timestamped |

## Portable (compile-checked on this box)

| File | Kya |
|---|---|
| `08_bypass_abstraction.hpp` | `INetworkReceiver` interface + working `KernelSocketReceiver` (Linux-specific code, but only ever `#include`d from `.linux.cpp` files — never touches the Windows-verified side) |
| `07_nic_tuning.sh` | ethtool/sysctl tuning checklist — `bash -n` syntax-checked, not compiled (it's a shell script, not C++) |

## Compile / run (Linux / WSL)

```bash
g++ -std=c++20 -O2 -Wall -Wextra -pthread 01_multicast_receiver.linux.cpp -o mcastrx
g++ -std=c++20 -O2 -Wall -Wextra -pthread 02_busy_poll_receiver.linux.cpp -o busypoll
g++ -std=c++20 -O2 -Wall -Wextra -pthread 03_receiver_benchmark.linux.cpp -o rxbench
g++ -std=c++20 -O2 -Wall -Wextra -pthread 04_hw_timestamps.linux.cpp -o hwts
g++ -std=c++20 -O2 -Wall -Wextra -pthread 05_order_gateway.linux.cpp -o gateway
g++ -std=c++20 -O2 -Wall -Wextra -pthread 06_wire_to_wire.linux.cpp -o w2w
```

## The recurring honest caveat across this folder

`SO_BUSY_POLL` (02, 03) and hardware timestamps (04, 06) both depend on
**real NIC driver support** that loopback/virtual interfaces don't have.
Every example that touches them says so explicitly and predicts the
loopback-specific (weaker) result rather than overselling it — consistent
with CLAUDE.md Rule 4 (claims must be verifiable) and Rule 2 (teach the
surprising/limited result, don't hide it).

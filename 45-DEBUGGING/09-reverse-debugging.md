# 09 — Reverse debugging: `rr` record & replay

## Prerequisites
- `02-gdb-basics.md`, `03-gdb-advanced.md` (watchpoints)
- `08-debugging-multithreaded.md` (heisenbugs)

## Yeh topic abhi kyun

Normal debugging aage chalti hai. Tum breakpoint se aage badhte ho, aur
agar "oops, woh galat state pehle ban gaya tha" — to **dobara run karo**
aur ummeed karo ki wahi timing aaye. Non-deterministic bug pe yeh painful,
kabhi impossible.

**Reverse debugging** = execution ko **peeche** chala sakte ho.
`reverse-next`, `reverse-continue`, aur — killer feature — `watch x` +
`reverse-continue` = "us line pe le chalo jisne `x` ko yeh (galat) value
di", ek command mein.

`rr` (Mozilla) sabse practical implementation: ek run **record** karo
(2× overhead), phir us **exact** run ko jitni baar chahe **replay** karo —
deterministically, forwards *aur* backwards.

> **Linux x86-64 only.** `rr` Windows/macOS pe nahi (CPU perf counters +
> ptrace chahiye; VM mein bhi aksar nahi). gdb ka native `record full`
> bhi Linux-only ("current architecture doesn't support record" MinGW pe).
> Yeh lesson ka workflow Linux/bare-metal ya properly-configured VM pe.

---

## Do mechanisms

| | `gdb` native `record full` | `rr record` / `rr replay` |
|---|---|---|
| Setup | gdb built-in, `record` command | `rr` install (`apt install rr`), `rr record ./prog` |
| Overhead | **very high** (~1000×) — har insn log | **~2×** record; replay near-native |
| Multi-thread | ❌ (single-thread only, practically) | ✅ (serializes to 1 core — deterministic) |
| Duration | seconds of execution | full program runs, minutes+ |
| Use | "last 5000 instructions before crash" | "whole run, replay 50 times" |
| Requires | x86/x86-64 Linux | x86-64 Linux + perf counters (`perf_event_paranoid <= 1`) |

Practically: **`rr`**. Native `record` sirf jab `rr` na ho aur tumhe bas
crash ke aakhri kuch hazaar instructions chahiye.

---

## `rr` — basic workflow

```bash
# 1. record (once). Bug is run mein hona chahiye.
rr record ./prog --some args
#   -> "rr: Saving execution to trace directory ..."
#   (agar "Requires perf_event_paranoid <= 1": 
#      sudo sysctl kernel.perf_event_paranoid=1   )

# 2. replay -- gdb khulta, trace ke against
rr replay
#   (gdb) continue           -> program crash tak chalega, exactly jaisa record mein
#   (gdb) break process_order
#   (gdb) reverse-continue    -> PICHHLE hit pe jao

# non-deterministic bug? ek race-y run record karo, phir:
rr replay -- -ex "continue" -ex "bt"     # har baar SAME crash
```

`rr` intercepts syscalls, signal delivery, RDTSC, thread scheduling — sab
record karta. Replay mein woh sab **exactly** dobara feed hota. Isliye ek
"1-in-1000" race, jab ek baar record ho gaya, ab **har replay mein** hota.

---

## Reverse commands

| Command | Kya |
|---|---|
| `reverse-continue` (`rc`) | pichhle breakpoint / watchpoint / start tak PEECHHE |
| `reverse-next` (`rn`) | ek line peeche (calls skip) |
| `reverse-step` (`rs`) | ek line peeche (calls ke andar) |
| `reverse-stepi` / `reverse-nexti` | ek instruction peeche |
| `reverse-finish` | current function ke **call site** pe wapas |
| `set exec-direction reverse` | phir normal `next`/`continue` bhi ulta chalein |

### Killer pattern — "yeh value kisne set ki"

```
(gdb) continue
Program received signal SIGSEGV ...
0x... in use_config () at cfg.cpp:88
88    return cfg->timeout_ms / cfg->divisor;      # divisor == 0 -> SIGFPE/SEGV

(gdb) print cfg->divisor
$1 = 0

(gdb) watch cfg->divisor
Hardware watchpoint 2: cfg->divisor

(gdb) reverse-continue
Hardware watchpoint 2: cfg->divisor
Old value = 0
New value = 0                       # (initial)
... 

(gdb) reverse-continue
Thread 1 hit Hardware watchpoint 2: cfg->divisor
Old value = 4
New value = 0
0x... in reload_config () at cfg.cpp:142
142       c->divisor = parse_int(line);   # <- YAHAN 0 set hua

(gdb) print line
$2 = "divisor="                    # empty value -> parse_int returns 0
```

Ek baar mein root cause. Forward debugging mein: crash dekho → shak → `cfg`
kahan bharta hai dhoondho → breakpoint → dobara run → ummeed karo wahi
config aaye → `watch` → aage badho. Reverse: **crash se seedha peeche**.

---

## Multi-threaded / heisenbug — `rr` ka asli faayda

`rr` threads ko **ek core pe serialize** karta aur scheduling decisions
record karta. To:

- Ek race jo 200 runs mein 1 baar hua → us 1 run ko `rr record` se pakdo
  → ab **har `rr replay`** mein woh race hai.
- `reverse-continue` se race ke **dono** accesses pe jao, dekho beech mein
  koi sync tha ya nahi.
- Scheduling deterministic hai → koi "is baar timing alag" nahi.

`08` ka heisenbug problem `rr` se kaafi had tak khatam: observe karna ab
run ko nahi badalta (recording ho chuki).

> **HFT relevance:** ek order-book corruption jo sirf ek specific
> market-data sequence + GC-pause-like scheduling hiccup pe hota — 1 in
> millions. Prod pe `rr`-record karna mehnga (2×, aur multi-core → 1 core
> serialize latency ke liye no-go in the live path). Par: **capture the
> input stream**, replay it offline **under `rr`**, aur jab corruption
> reproduce ho — `reverse-continue` from the bad book state to the exact
> message + code path. Yeh "replay harness + rr" combo HFT debugging ka
> high-end tool hai. Live path mein nahi; post-incident analysis mein haan.

---

## `rr` extras

```bash
rr record -n ./prog          # -n: don't try to use hardware perf (some VMs)
rr record --chaos ./prog     # scheduling ko randomize -> races ko provoke karo
rr replay -p <pid>           # multi-process: ek process chuno
rr pack                      # trace ko portable banao (dependencies bundle)
rr ps                        # trace ke processes list
```

`--chaos` mode khaas: record ke waqt scheduler ko deliberately "adversarial"
banata (random thread priorities/timeslices) taaki race pehle hi record
run mein aa jaaye. `rr record --chaos` loop mein chalao jab tak trace mein
bug na aaye, phir infinite deterministic replays.

Replay mein state change nahi kar sakte (`set var` no-op-ish, `call` jo
side-effect kare risky) — recording immutable hai. Pure inspection +
time-travel.

---

## Jab `rr` na ho — poor man's reverse

- **`gdb checkpoint`** — `checkpoint` ek fork-based snapshot banata;
  `restart N` us point pe wapas. "Reverse" nahi par "ek achhe point pe
  wapas kood jao" (Linux; fork ke saath).
- **`record full`** (short window) — crash se pehle `record`, kuch
  instructions, phir `reverse-stepi`. High overhead, single-thread.
- **Core dumps at intervals** — `gcore` se periodic snapshots; crash pe
  pichhla core khol ke compare.
- **Deterministic replay harness** — apne system ko aisa design karo ki
  input log se poora run byte-identical reproduce ho (folder 44 ka
  `MiniHftEngine` yahi hai). Phir "reverse" = "replay up to message N-1".

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `rr` ko VM/CI pe expect karna
`rr` ko CPU hardware performance counters chahiye. Zyaadatar cloud VMs,
containers, aur nested virtualization mein nahi (`rr record` → "PMU ...
not available"). `rr record -n` kabhi chalta (software counting, slower).
Bare-metal Linux ya `rr`-friendly VM chahiye.

### Trap 2 — `perf_event_paranoid`
`rr record` → "Cannot access performance counter ... perf_event_paranoid".
`sudo sysctl kernel.perf_event_paranoid=1` (ya `-1`). Persist:
`/etc/sysctl.d/`.

### Trap 3 — Replay mein state badalne ki koshish
Recording immutable. `set var`, side-effecting `call`, "yeh input change
karke dekhta hoon" — nahi. Naya scenario = naya `rr record`.

### Trap 4 — Bug record run mein aaya hi nahi
`rr replay` sirf woh dikhayega jo record hua. Agar bug intermittent aur
record run "clean" tha → replay bhi clean. `rr record --chaos` loop, ya
stress input, jab tak trace mein bug na aaye.

### Trap 5 — Native `record` ko `rr` samajhna
`gdb` `record full` ≠ `rr`. Native = ~1000× overhead, single-thread,
seconds of execution. `rr` = ~2×, multi-thread, whole runs. Alag tools.

### Trap 6 — Massive trace size
Long-running program ka `rr` trace GBs ka ho sakta (`~/.local/share/rr/`).
`rr record` ko problem ke aas-paas hi rakho (chhota repro), ya periodically
`rr rm` purani traces.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Reverse debugging Windows pe" | `rr` Linux x86-64 only. Native `record` bhi Linux. |
| "`rr` overhead 1000× hai" | Native `record` hai. `rr` ~2× record, near-native replay. |
| "Replay mein experiment kar sakta hoon" | Recording immutable — pure time-travel inspection. |
| "`rr` ne bug reproduce nahi kiya" | Bug us record run mein nahi tha. `--chaos` + stress loop. |
| "watchpoint + reverse-continue = slow" | `rr` replay mein fast — yeh iska flagship use-case. |

---

## Hands-on

Linux/bare-metal:
```bash
sudo sysctl kernel.perf_event_paranoid=1

g++ -std=c++20 -g -O0 -pthread 45-DEBUGGING/examples/04_race_debug.cpp -o race
rr record ./race                 # race-y run capture (retry until total != 2000000)
rr replay
  (gdb) continue                 # program end
  (gdb) break worker
  (gdb) reverse-continue         # last worker() entry
  (gdb) watch g_counter_racy
  (gdb) reverse-continue         # -> exact interleaving jahan update lost hua

# config-null style bug (make one):
rr record ./prog ; rr replay -ex continue -ex "watch cfg->divisor" -ex reverse-continue
```

---

## Exercises

1. Ek NULL-deref crash. `bt` dikhata `p` null hai frame #0 mein. `rr`
   ke saath 2-command recipe likho jo tumhe `p` set/cleared hone wali
   line pe le jaaye.
   <details><summary>Answer</summary>
   ```
   (gdb) watch -location p          # p ke current address (jo abhi null hai -- iski jagah
                                    # jahan se p aata, us pointer variable pe watch)
   (gdb) reverse-continue           # p ko null likhne wali line pe (ya jahan woh
                                    # non-null se null hua)
   ```
   Agar `p` ek struct field hai (`obj->p`): `watch obj->p` phir
   `reverse-continue`. Yeh forward "kahan set hota hai dhoondho + re-run"
   ko ek step mein badal deta.
   </details>

2. `rr record ./server` chalaya, "PMU not available" aaya. Do options?
   <details><summary>Answer</summary>
   (1) `rr record -n ./server` — hardware perf counters ke bina (software
   instruction counting), slower record par chalta zyada environments
   mein. (2) Bare-metal Linux, ya `rr`-supported VM (KVM with PMU
   passthrough), ya `kernel.perf_event_paranoid` theek karo. Cloud CI pe
   aksar (1) ya "rr yahan nahi" accept karo.
   </details>

3. Heisenbug: race gdb mein reproduce nahi hota. `rr` kaise help karta jo
   plain gdb nahi kar sakta?
   <details><summary>Answer</summary>
   `rr` scheduling + timing ko **record** karta. Ek baar jab race-y run
   record ho gaya (stress / `--chaos` se), har `rr replay` mein woh **exact**
   interleaving dobara chalta — deterministically. gdb (live) har run pe
   naya timing deta, isliye observe karte hi bug chhup jaata. `rr` mein
   observe karna run ko nahi badalta (woh already recorded hai).
   </details>

4. `rr replay` mein tum `reverse-continue` maarte ho aur woh program ke
   **start** pe pahunch jaata bina kisi breakpoint ke. Kya hua?
   <details><summary>Answer</summary>
   Peeche jaate hue koi breakpoint/watchpoint hit nahi hua, to `rr`
   recording ki shuruaat tak chala gaya ("stopped at end of trace / start
   of program"). Matlab tumhara breakpoint us code path pe tha hi nahi
   (galat function naam? condition kabhi true nahi?), ya tum already us
   point se pehle the. Breakpoint/watchpoint set karke phir `reverse-
   continue`.
   </details>

---

## Interview questions

1. `rr record`/`replay` ka overhead? Native `gdb record` se kaise alag?
2. `watch x` + `reverse-continue` — yeh forward debugging ke kis multi-step
   process ko replace karta?
3. `rr` heisenbug ke liye kyun itna powerful (determinism kaise deta)?
4. `rr --chaos` kya karta aur kab use karoge?
5. `rr` kis platform pe chalta, aur kyun nahi chalta zyaadatar VMs pe?
6. HFT context: `rr` ko live trading path mein kyun nahi, par post-incident
   analysis mein kaise use karoge?

---

## Next
→ [`10-logging-strategy.md`](10-logging-strategy.md)

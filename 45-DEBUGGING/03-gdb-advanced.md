# 03 — GDB advanced: watchpoints, conditions, scripting

## Prerequisites
- `02-gdb-basics.md` (break / run / print / bt / step)
- `27-ATOMICS-MEMORY-MODEL` helpful (watchpoints se data-race hunt)

## Yeh topic abhi kyun

`02` ne "manually chalao aur dekho" sikhaya. Real bugs mein woh dheere hai —
loop 10,000 baar chalta, bug 7,432nd iteration pe. Tum `next` 7,432 baar
nahi maaroge. Advanced gdb = **gdb ko bolo kab rukna hai** (condition),
**kya karna hai** (commands), aur **automate** (scripts).

Sab kuch `examples/01_gdb_practice.cpp` ke against real transcripts.

---

## Conditional breakpoints — "sirf jab X"

```
(gdb) break 44 if accts[i].balance_cents < 100000
```

Ya baad mein condition lagao/hatao:
```
(gdb) break 44
(gdb) condition 2 accts[i].balance_cents < 100000     # bp #2 pe
(gdb) condition 2                                       # condition hatao
```

**Transcript H:**
```
(gdb) break sum_balances
(gdb) run
(gdb) delete                     # pehla bp hata do
(gdb) break 44
(gdb) condition 2 accts[i].balance_cents < 100000
(gdb) continue

Thread 1 hit Breakpoint 2, sum_balances (...) at 01_gdb_practice.cpp:44
(gdb) print i
$1 = 2
(gdb) print accts[i]
$2 = {name = "carol", balance_cents = 75000, trades = 0}
```

Loop 3 baar chala, gdb sirf `i==2` (carol, balance < 100000) pe ruka.
10,000-iteration loop mein: `break 44 if i == 7432`. Ya bug ki **condition
express karo**: `break process if order.price <= 0` — pehla invalid order
pe seedha ruk jao.

> **HFT relevance:** feed handler 40 million messages/sec process karta;
> ek malformed message pe crash. `break parse if msg.len > 1500` — 40M
> messages ke beech se woh ek pakad lo. Manual stepping impossible.

**Cost:** conditional breakpoint = gdb har hit pe ruk ke condition
evaluate karta phir resume. Hot loop mein 100× slow ho sakta. Alternative:
`03/09` (`10-logging`) ya hardware watchpoint (neeche).

---

## `ignore` — pehle N hits skip

Pata hai bug ~500th call pe hai:
```
(gdb) break process_order
(gdb) ignore 1 499        # bp #1 ke agle 499 hits ignore karo
Will ignore next 499 crossings of breakpoint 1.
(gdb) run                 # 500th pe rukega
```

---

## `tbreak` — ek baar wala breakpoint

```
(gdb) tbreak main         # main pe ruko, phir bp khud delete
```
`charge_all` pe sirf pehli entry chahiye, har loop iteration nahi jahan
woh recursively... nahi, non-recursive. Par `main` jaisi jagah, ya "bas
yahan tak pahunchna hai phir aage manual" — `tbreak`. **Transcript E:**
```
(gdb) tbreak charge_all
(gdb) run
Thread 1 hit Temporary breakpoint 1, charge_all (...) at ...:59
```
Ab `charge_all` dobaara call ho to nahi rukega.

---

## `commands` — breakpoint pe auto-actions

"Har baar `apply_fee` call ho, arguments print karo, phir chalte raho" —
`printf`-debugging bina recompile:

```
(gdb) break apply_fee
(gdb) commands
> silent
> printf "apply_fee called: bal=%ld fee=%ld\n", balance_cents, fee_cents
> continue
> end
(gdb) run
```

**Transcript F:**
```
apply_fee called: bal=500000 fee=2500
apply_fee called: bal=125000 fee=2500
apply_fee called: bal=75000  fee=2500
total_before = 700000 cents
...
```

- `silent` — "Breakpoint 1, apply_fee..." banner mat chhaapo, sirf mera
  `printf`.
- `continue` — ruko mat, log karke aage.
- Yeh = **tracepoint by hand**. Effectively tumne source mein `printf`
  add kiya bina file chhue, bina recompile. Loop condition ke saath
  combine: `break 44 if i>7000` + `commands` → sirf aakhri iterations log.

---

## `display` — har stop pe auto-print

```
(gdb) display collected
(gdb) next
1: collected = 0
(gdb) next
1: collected = 0
```
Ab har `next`/`step`/breakpoint pe `collected` khud dikhega. `info
display` list; `undisplay 1` hatao. Multiple: `display i`, `display
accts[i].balance_cents` — chalte hue sab track.

---

## `set var` — chalti process badlo

**Transcript E (cont.):**
```
(gdb) set var fee_cents = 0
(gdb) print fee_cents
$1 = 0
(gdb) continue
...
fees_collected = 2500 cents        # 7500 nahi -- humne beech mein 0 kar diya
total_after  = 697500 cents
```

Uses:
- **"Agar yeh value X hoti to?"** — hypothesis test bina recompile.
- **Bug ke aas-paas jao** — galat pointer ko sahi set karke aage
  chalao, dekho baaki logic theek hai ya nahi.
- **`return`** — `return 42` current function ko turant return kara deta
  (given value ke saath) — ek buggy function ko "skip" karne ke liye.

⚠️ `set var` real state badalta — output ab "asli" run ka nahi. Debugging
probe hai, fix nahi.

---

## Watchpoints — teen flavour

| Command | Trigger |
|---|---|
| `watch EXPR` | EXPR ki value **badle** (write) |
| `rwatch EXPR` | EXPR **padha** jaaye (read) |
| `awatch EXPR` | read ya write (access) |

`watch` = "yeh variable galat ho gaya, **kaun** karta hai" ka one-shot
jawab (`02` transcript C). `rwatch` = "yeh flag kaun padhta hai" — jab
tumhe lage ki koi stale read kar raha.

**Memory address pe watch** (variable scope se bahar bhi):
```
(gdb) watch *(int*)0x60c4a0
(gdb) watch -location ptr->field      # ptr abhi jo point karta, us address pe
```
`-location` important: plain `watch ptr->field` tab bhi fire hoga jab
`ptr` khud badle. `-location` ne address freeze kar diya.

**Data-race hunt:** `watch g_shared` + `continue`. Fire → `bt` → kaunsa
thread, kaunsi line. Phir `TSAN` (`06`, `08`) se confirm.

---

## `catch` — exceptions, syscalls, signals

**Transcript G** (`catch throw`):
```
(gdb) catch throw
(gdb) run
Thread 1 hit Catchpoint 1 (exception thrown), 0x... in __cxa_throw ()
(gdb) backtrace
#0  __cxa_throw ()
#1  parse (s=0x... "x") at thr.cpp:3
#2  main () at thr.cpp:4
```

`catch throw` = exception **throw hone ke waqt** ruko — stack abhi intact
hai, tum dekh sakte ho throw **kahan** se hua. `try/catch` ke baad `bt`
karoge to woh context ja chuka hoga.

| `catch ...` | Kab |
|---|---|
| `catch throw` / `catch rethrow` / `catch catch` | C++ exception lifecycle |
| `catch throw std::runtime_error` | sirf ek type |
| `catch syscall write` | koi syscall (Linux) — "yeh `write()` kaun karta" |
| `catch signal SIGPIPE` | signal delivery pe (`29-LINUX-SYSTEMS/04`) |

---

## Scripting — `define`, `-x`, Python

**User-defined command:**
```
define pv
  # print a std::vector's first $arg1 elements
  set $i = 0
  while $i < $arg1
    printf "[%d] ", $i
    print $arg0[$i]
    set $i = $i + 1
  end
end
# usage: pv accts 3
```

**Convenience variables:** `$pc` `$sp` `$rax` (registers), `$1 $2` (value
history), `$foo` (tumhara — `set $foo = accts[0]`).

**Python (gdb built-in):**
```python
(gdb) python
import gdb
v = gdb.parse_and_eval("accts")
print(int(v['_M_impl']['_M_finish'] - v['_M_impl']['_M_start']))
end
```
Pretty-printers, complex conditions, dumping structures to a file — sab
Python se. Bade projects ke `.gdbinit` mein aksar custom printers hote.

**Batch/CI:**
```bash
gdb -q -batch -x triage.gdb --args ./prog input.dat
```
`triage.gdb` mein: `run` → `bt full` → `info registers` → `info threads`
→ `quit`. Crash aaya to poora dump ek file mein — automated crash triage.

---

## Attach to a running process

```bash
gdb -p $(pgrep my_server)        # already-running process se judo
# ya gdb ke andar:
(gdb) attach 12345
(gdb) detach                     # chhod do (process chalta rehta)
```
Server hang ho gaya, kabhi crash nahi karta? `attach` → `thread apply all
bt` → dekho har thread kahan atka (`08`). Production debugging ka bread
and butter.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — Conditional breakpoint hot loop ko 100× slow karta
`break inner_loop if x == target` jab `inner_loop` 10M/sec chalta — gdb
har hit pe user↔debugger context switch. Behtar: `watch` (hardware, jab
possible), ya code mein ek `if (x == target) __builtin_trap();` daal ke
`-g` recompile (turant break, zero steady-state cost).

### Trap 2 — `watch` local, function return, watchpoint gone
Normal (`02` trap 4). Persistent chahiye → global/heap address pe
`watch -location`.

### Trap 3 — Software watchpoint by accident
`watch big_struct` → 4 hardware slots se bada → gdb silently "software"
mode → single-steps everything → 1000× slow, lagta gdb hang. `info
watchpoints` "hw"/"sw" batata. Fix: ek specific field/word pe watch.

### Trap 4 — `set var` ke baad results ko "real" maanna
Tumne state badla. Ab jo output aaya woh ek **kalpanik** run hai. Note
karo "maine yahan `fee=0` set kiya" — warna 10 min baad bh.oolke real bug
report kar doge.

### Trap 5 — `catch throw` har chhoti exception pe ruke
Kuch code normal flow mein exceptions use karta (`std::stoi` fail,
`filesystem`). `catch throw std::logic_error` se narrow karo, ya
`condition` add: `catch throw` phir `condition 1 $_exception...` (advanced).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| Loop mein `next` N baar | `break LINE if i == N`, ya `ignore bp N` |
| Recompile to add a print | `break` + `commands` + `silent` + `printf` + `continue` |
| "kaun badalta hai yeh var" — prints scatter | `watch var` — ek command, seedha us line pe |
| `catch` sirf signals ke liye | `catch throw` — exception ke origin pe, `try` ke baad nahi |
| Har debug session fresh setup | `.gdbinit` + `define` custom commands + `-x` scripts |

---

## Hands-on

```bash
./build.ps1 build 45-DEBUGGING/examples/01_gdb_practice.cpp
gdb -q .build/01_gdb_practice.exe
```
1. `break 44 if i == 2` → `run` → `print accts[i].name` ("carol"?)
2. `break apply_fee` → `commands` / `silent` / `printf "%ld\n",
   balance_cents` / `continue` / `end` → `run` — teen balances log honge.
3. `break sum_balances` → `run` → `watch total` → `continue` × 3 — Old/New
   500000 → 625000 → 700000.
4. `tbreak factorial` → `run` → `set var n = 3` → `finish` → return value
   6, 120 nahi. (Tumne recursion ki depth kaat di.)

---

## Exercises

1. 1,000,000-iteration loop, bug ~iteration 950,000 pe (state corrupt).
   `break body if i == 950000` slow hai. Do behtar approaches?
   <details><summary>Answer</summary>
   (a) `ignore 1 949999` — condition-check nahi, sirf counter decrement,
   sasta. (b) Us "corrupt" state ke variable/field pe `watch` — hardware
   watchpoint zero steady-state cost, seedha corrupting write pe rukta.
   (c) Code mein `if (i == 950000) __builtin_trap();`, `-g` recompile.
   </details>

2. `watch ptr->count` aur `watch -location ptr->count` — `ptr` khud
   reassign ho jaaye to fark?
   <details><summary>Answer</summary>
   Plain `watch ptr->count`: gdb `ptr` ko bhi track karta; `ptr` badla to
   watchpoint bhi fire/re-evaluate ho sakta, aur ab **naye** object ke
   `count` pe dekh raha. `-location`: expression **ek baar** evaluate,
   us address ko pin kiya — `ptr` badle to bhi purane object ke us word
   pe hi watch. UAF hunt ke liye `-location` behtar.
   </details>

3. `commands` block mein tum `silent` + `printf` + (no `continue`) rakhte
   ho. Kya hota?
   <details><summary>Answer</summary>
   Har hit pe log hota **aur gdb ruk jaata** (prompt deta) — kyunki
   `continue` nahi hai. "Log + inspect at each hit" chahiye to yeh sahi;
   "pure tracepoint, mat ruko" chahiye to `continue` last line mein
   zaroori.
   </details>

4. `catch throw` fire hua, `bt` ne `#1 parse(...)` dikhaya. Ab tum `parse`
   ke locals dekhna chahte ho throw ke **theek pehle** wale state mein.
   Command?
   <details><summary>Answer</summary>
   `frame 1` (ya `up`) — `__cxa_throw` frame #0 se `parse` frame #1 pe
   jao. Ab `info locals` / `print s` / `list` us context mein. Exception
   abhi propagate nahi hui, `parse` ka frame zinda hai.
   </details>

---

## Interview questions

1. Conditional breakpoint hot path mein kyun mehnga? Do alternatives.
2. `watch` vs `rwatch` vs `awatch` — ek use-case har ek ka.
3. `catch throw` `try/catch` ke baad breakpoint se behtar kyun (exception
   debugging ke liye)?
4. `set var` / `return` se ek buggy function ko "bypass" karke aage ka
   code test kaise karoge?
5. Production server hang ho gaya (crash nahi). GDB se kya karoge, step
   by step?

---

## Next
→ [`04-debugging-crashes.md`](04-debugging-crashes.md)

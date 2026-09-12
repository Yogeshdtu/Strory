# 02 — GDB basics: breakpoint, step, print, backtrace

## Prerequisites
- `01-debugging-mindset.md` (hypothesis → test loop)
- `08-FUNCTIONS/05-the-call-stack.md` (stack frames, return address — `backtrace` isi ko dikhata hai)
- `03-VARIABLES-DATA-TYPES` (types — `print` inhe dikhata hai)

## Yeh topic abhi kyun

`printf`-debugging kaam karta hai — par har hypothesis pe recompile,
re-run, aur print hatao. GDB tumhe **chalti hui process rok ke** andar
jhaankne deta hai: koi bhi variable print karo, stack dekho, ek line
chalao, ek variable pe watch lagao. Ek gdb session mein tum 10 hypotheses
test kar sakte ho — bina ek baar recompile kiye.

Yeh lesson `examples/01_gdb_practice.cpp` ke against **real transcripts**
dikhata hai. `./build.ps1 45-DEBUGGING/examples/01_gdb_practice.cpp` se
compile karo, phir `gdb -q .build/01_gdb_practice.exe` — aur khud yeh
commands chalao.

---

## Setup — `-g` zaroori hai

```bash
g++ -std=c++20 -g -O0 prog.cpp -o prog      # -g = debug info (line numbers, var names, types)
gdb -q ./prog                               # -q = no license banner
```

- **`-g` ke bina** gdb ke paas addresses hain, naam nahi — `bt` sirf hex
  pata dikhayega, `print x` "No symbol x" bolega.
- **`-O0`** — optimizer off. `-O2` pe variables `<optimized out>`, lines
  jump karti hain, functions inline ho jaate — woh `05-debugging-optimized.md`
  ka topic hai. Pehle `-O0` seekho.
- **`-O0 -g`** default debug build hai. Is repo mein: `./build.ps1 build file.cpp`.

---

## Core commands — ek table

| Command | Short | Kya karta |
|---|---|---|
| `run [args]` | `r` | program shuru karo (arguments ke saath) |
| `break LOC` | `b` | breakpoint: `b func`, `b file.cpp:44`, `b 44` |
| `continue` | `c` | agle breakpoint tak chalao |
| `next` | `n` | agli line — **function calls ke upar se guzro** |
| `step` | `s` | agli line — **function calls ke andar jao** |
| `finish` | `fin` | current function poora karo, return value dikhao |
| `print EXPR` | `p` | expression evaluate karke dikhao |
| `backtrace` | `bt` | call stack — kis raste se yahan aaye |
| `frame N` | `f N` | stack frame N pe jao (locals uske context mein) |
| `info locals` | `i lo` | is frame ke saare local variables |
| `info args` | `i ar` | is function ke arguments |
| `list` | `l` | aaspaas ka source dikhao |
| `watch EXPR` | — | jab `EXPR` ki value badle, ruk jao (`03`) |
| `set var X = V` | — | chalti process mein X ki value badlo |
| `quit` | `q` | niklo |

Enter dabao (khaali command) = **pichhla command dobara** — `next`
repeat karne ke liye bas Enter maarte raho.

---

## Transcript A — break, run, backtrace, print

```
$ gdb -q .build/01_gdb_practice.exe
(gdb) break sum_balances
Breakpoint 1 at 0x140001751: file 01_gdb_practice.cpp, line 42.

(gdb) run
Thread 1 hit Breakpoint 1, sum_balances (accts=std::vector of length 3, capacity 3 = {...})
    at 01_gdb_practice.cpp:42
42          long total = 0;

(gdb) backtrace
#0  sum_balances (accts=...) at 01_gdb_practice.cpp:42
#1  0x00007ff7ee051a0d in main () at 01_gdb_practice.cpp:76
```

`bt` padho **neeche se upar**: `main` (frame #1) ne line 76 pe
`sum_balances` call kiya, hum ab uske andar hain (frame #0). Yeh "hum
yahan kaise pahunche" ka poora jawab hai.

```
(gdb) info args
accts = std::vector of length 3, capacity 3 = {
  {name = "alice", balance_cents = 500000, trades = 0},
  {name = "bob",   balance_cents = 125000, trades = 0},
  {name = "carol", balance_cents = 75000,  trades = 0}}
```

GDB ke paas **STL pretty-printers** hain — `std::vector`, `std::string`,
`std::map` sab readable form mein. Raw `{_M_start = 0x..., ...}` nahi.

```
(gdb) next        # line 42 -> 43
43          for (std::size_t i = 0; i < accts.size(); ++i) {
(gdb) next        # -> 44
44              total += accts[i].balance_cents;
(gdb) next        # -> back to 43 (loop)
43          for (std::size_t i = 0; i < accts.size(); ++i) {

(gdb) print total
$1 = 500000
(gdb) print accts[1]
$2 = {name = "bob", balance_cents = 125000, trades = 0}
(gdb) print accts[1].name
$3 = "bob"
```

`print` **koi bhi C++ expression** le sakta hai — indexing, member access,
arithmetic (`p total + accts[2].balance_cents`), function calls
(`p accts.size()`), casts (`p (char)65`). Har result `$N` mein save hota —
`p $2.trades` bhi chalega.

> **Note (Windows/MinGW):** `[New Thread ...]` lines CRT startup threads
> hain, tumhare code ke nahi. Linux pe yeh dikhti bhi nahi jab tak tum
> `std::thread` na banao.

---

## Transcript B — recursion aur `backtrace`

```
(gdb) break factorial if n == 1        # conditional breakpoint (lesson 03)
(gdb) run

Thread 1 hit Breakpoint 1, factorial (n=1) at 01_gdb_practice.cpp:31
31          if (n <= 1) {

(gdb) backtrace
#0  factorial (n=1) at 01_gdb_practice.cpp:31
#1  0x...735 in factorial (n=2) at 01_gdb_practice.cpp:34
#2  0x...735 in factorial (n=3) at 01_gdb_practice.cpp:34
#3  0x...735 in factorial (n=4) at 01_gdb_practice.cpp:34
#4  0x...735 in factorial (n=5) at 01_gdb_practice.cpp:34
#5  0x...a3d in main () at 01_gdb_practice.cpp:79
```

Recursion ka poora unwind: `main` → `factorial(5)` → `factorial(4)` → ...
→ `factorial(1)`. Har frame ka apna `n`. Note frames #1–#4 ka return
address same hai (`...735`) — kyunki har `factorial` call **usi line 34**
se hui.

```
(gdb) frame 3                     # frame #3 pe jao (n=4 wala)
#3  factorial (n=4) at 01_gdb_practice.cpp:34
34          long rest = factorial(n - 1);
(gdb) print n
$2 = 4                            # ab `n` ka matlab frame #3 ka n hai
(gdb) info frame
Stack level 3, frame at 0x5ffcf0:
  Saved registers: rbp at 0x5ffce0, rip at 0x5ffce8
```

`frame N` = "us frame ke chashme se dekho". `print`, `info locals`, `info
args` sab us frame ke context mein. Deep stack mein bug dhoondhne ka main
tareeka: `bt` → shaqi frame pehchaano → `frame N` → uske locals inspect.

---

## Transcript C — `watch` (ek variable pe pehra)

```
(gdb) break sum_balances
(gdb) run
(gdb) watch total
Hardware watchpoint 2: total

(gdb) continue
Hardware watchpoint 2: total
Old value = 0
New value = 500000
sum_balances (...) at 01_gdb_practice.cpp:43

(gdb) continue
Old value = 500000
New value = 625000            # += 125000 (bob)

(gdb) continue
Old value = 625000
New value = 700000            # += 75000 (carol)
```

`watch total` = "jaise hi `total` badle, ruk jao aur batao purani/nayi
value". Yeh **debugging ka sabse powerful command** hai jab sawaal ho
"yeh variable galat kyun ho gaya" — tumhe seedha us line pe le jaata hai
jo use badalti hai. Recompile nahi, print scatter nahi.

**"Hardware watchpoint"** = CPU ke debug registers (DR0–DR3) use karta,
zero overhead. Sirf 4 available, aur max 8 bytes each. Bade struct pe
watch → "software watchpoint" → gdb har instruction ke baad check karta →
1000× slow. Tab specific field pe watch karo: `watch acct.balance_cents`.

---

## Transcript D — `step` vs `next` vs `finish`

```
(gdb) break charge_all
(gdb) run
59          long collected = 0;
(gdb) next          # 59 -> 60
60          for (auto& a : accts) {
(gdb) next          # -> 61
61              long before = a.balance_cents;
(gdb) next          # -> 62 (a line WITH a function call)
62              a.balance_cents = apply_fee(a.balance_cents, fee_cents);

(gdb) step          # NEXT hota to line 63 pe jaata; STEP andar ghusta:
apply_fee (balance_cents=500000, fee_cents=2500) at 01_gdb_practice.cpp:54
54          long after = balance_cents - fee_cents;

(gdb) bt
#0  apply_fee (...) at 01_gdb_practice.cpp:54
#1  charge_all (...) at 01_gdb_practice.cpp:62
#2  main () at 01_gdb_practice.cpp:77

(gdb) finish        # apply_fee poora chalao, return karke ruko
Run till exit from #0  apply_fee (...) at 01_gdb_practice.cpp:54
0x...820 in charge_all (...) at 01_gdb_practice.cpp:62
Value returned is $1 = 497500
```

- **`next`** — line as a unit; function call ho to **poora chalake** agli
  line pe.
- **`step`** — agar line mein tumhare code ka function call hai, uske
  **andar** jao. (Library functions ke andar nahi jaata agar unka `-g`
  nahi.)
- **`finish`** — "yeh function bas ho jaaye" — return value bhi dikhata
  (`Value returned is $1 = 497500`). Kisi function mein galti se `step`
  kar gaye? `finish` se bahar.

Rule of thumb: `next` maarte raho; jis line pe result galat aata hai
usmein `step` karke andar jao; galat function nahi hai to `finish`.

---

## `list` aur TUI

```
(gdb) list sum_balances       # function ke aaspaas source
(gdb) list 40,50              # lines 40-50
(gdb) list                    # agli 10 lines (repeat)
```

**TUI mode** — source + code side-by-side, live:
```
(gdb) tui enable        # ya launch: gdb -tui ./prog   (ya Ctrl-x Ctrl-a toggle)
```
Windows/MinGW pe TUI kabhi-kabhi flaky; `layout src` / `layout asm` /
`layout regs`. Agar tut jaaye: `tui disable`.

---

## `.gdbinit` — har session ke shortcuts

Home directory ya project mein `.gdbinit`:
```
set print pretty on          # struct/array multi-line
set pagination off           # "---Type <return>..." prompt band
set history save on          # command history persist
break main                   # har baar main pe ruko
```
Project-local `.gdbinit` load hone ke liye: `~/.config/gdb/gdbinit` mein
`add-auto-load-safe-path /path/to/project` (security feature).

---

## Batch mode — script se debugging

Recompile-free, par CI-friendly. Isi se yeh lesson ke transcripts bane:
```bash
gdb -q -batch \
  -ex "break sum_balances" -ex "run" \
  -ex "backtrace" -ex "info args" \
  -ex "continue" \
  ./prog
```
`-batch` = commands chalao, program exit pe gdb bhi exit. `-x script.gdb`
= commands ek file se. Crash triage automation ke liye standard.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `-g` bhool gaye
`bt` sirf `#0 0x00005555 in ?? ()` dikhata, `print x` = "No symbol". Fix:
`-g` ke saath recompile. Release binary crash? `-g` alag `.debug` file
mein ho sakta — `gdb prog` + `symbol-file prog.debug`.

### Trap 2 — `-O2` pe debug kar rahe ho
`<optimized out>`, ek line pe `next` kai baar, inlined functions `bt` mein
nahi. Pehle `-O0` pe reproduce karne ki koshish. Nahi hota (timing bug) →
`05-debugging-optimized.md`.

### Trap 3 — `step` library ke andar phas gaye
`step` ne `std::vector::operator[]` ke andar le liya, ab tum STL internals
mein ho. `finish` (baar-baar) ya `Ctrl-C` se nikaalo. Ya:
`skip -gfi /usr/include/c++/*` — un files ko step-over mark kar do.

### Trap 4 — watchpoint scope se bahar
`watch total` (local). Function return ho gaya → "Watchpoint 2 deleted
because the program has left the block". Yeh normal hai — local ka scope
khatam. Global/heap variable pe watch permanent rehta.

### Trap 5 — breakpoint template/inline function pe
Ek `b func` template ki har instantiation pe lag sakta (`func<int>`,
`func<double>`). `info breakpoints` dekho — "1.1, 1.2" multiple locations.
Header-only inline function har TU mein — `b file.cpp:LINE` zyada precise.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "gdb = crash ke baad hi" | gdb kisi bhi waqt attach/run; watchpoint se logic bug bhi |
| `next` sabke liye | Jis line pe result galat, uspe `step` karo — andar bug hai |
| "print sirf variables" | `p` koi bhi expression: `p v.size()`, `p (a>b)?a:b`, `p func(3)` |
| `bt` upar se padhna | Neeche se padho: `main` → ... → yahan. #0 = abhi. |
| "watchpoint slow hai" | Hardware watchpoint (≤8 bytes) zero-cost; bade struct pe field pe lagao |

---

## Hands-on

```bash
./build.ps1 build 45-DEBUGGING/examples/01_gdb_practice.cpp
gdb -q .build/01_gdb_practice.exe
```
Chalao, in order:
1. `break charge_all` → `run` → `info args` (fee_cents = 2500?)
2. `watch accts` ... nahi, bada hai. `break apply_fee` → `run` →
   `bt` → `finish` — return value kya? `print balance_cents - fee_cents`
   se milaao.
3. `break factorial` → `run` → `continue` 4 baar → har baar `print n`
   (5,4,3,2). 5th `continue` pe `bt` — 5 frames deep.
4. `frame 2` → `info locals` → `frame 0` → dekho `n` badal gaya.

---

## Exercises

1. `bt` yeh dikhata hai:
   ```
   #0  compute (x=0) at m.cpp:12
   #1  loop_body (i=5) at m.cpp:30
   #2  main () at m.cpp:44
   ```
   Crash `compute` mein `1/x` pe hai. `x` kahan se aaya — kaunsa frame,
   kaunsa command?
   <details><summary>Answer</summary>
   `x` `compute` ka argument hai, `loop_body` ne (frame #1, line 30) diya.
   `frame 1` → `info locals` / `list 30` → dekho `compute()` ko kya pass
   hua (shayad `i - 5` = 0 jab `i==5`). Root cause frame #1 mein, crash
   #0 mein.
   </details>

2. `next` aur `step` — ek line `int r = helper(compute(a), b);` pe `step`
   maaroge to kahan ruk sakte ho (2 possibilities)?
   <details><summary>Answer</summary>
   `compute(a)` mein YA `helper(...)` mein — evaluation order pe depend
   (unspecified — `05-OPERATORS/10`). GDB usually pehle jo call evaluate
   hota uske andar. Nested calls debug karne ke liye: alag lines mein
   todo, ya `b compute` `b helper` dono laga ke `continue`.
   </details>

3. `watch g_counter` lagaya (global). Program 10 threads chalata hai.
   Watchpoint kis thread pe fire karega?
   <details><summary>Answer</summary>
   Jo bhi thread `g_counter` likhe. Multi-threaded watchpoint kaam karta
   par slow ho sakta (har thread ka har write check). Fire hone pe `bt`
   sirf **us** thread ka; `thread apply all bt` sabka (`08`). Yeh data
   race dhoondhne ka ek tareeka hai.
   </details>

4. `finish` ne dikhaya `Value returned is $1 = 497500`, par caller mein
   variable `498000` ho gaya. Kya hua?
   <details><summary>Answer</summary>
   Return value aur assignment ke beech kuch hua: shayad caller
   `x = f() + 500`, ya ek dusra call, ya `f()` ne bhi ek out-param /
   global badla. `next` karke us line ke baad `print` karo, ya us
   variable pe `watch`.
   </details>

---

## Interview questions

1. `-g` kya add karta hai binary mein? `-g` ke bina gdb kya de sakta,
   kya nahi?
2. `step` aur `next` ka fark — ek line par yeh kab matter karta?
3. `backtrace` ki information kahan store hoti (stack)? `-fomit-frame-pointer`
   se `bt` ko kya hota? (Ans: frame pointer chain toot sakti; gdb ko
   `.eh_frame`/DWARF CFI se unwind karna padta — `-g` ho to theek.)
4. Hardware vs software watchpoint — fark aur kab kaunsa.
5. `-O2 -g` binary mein `<optimized out>` kyun dikhta hai kuch variables
   ke liye?

---

## Next
→ [`03-gdb-advanced.md`](03-gdb-advanced.md)

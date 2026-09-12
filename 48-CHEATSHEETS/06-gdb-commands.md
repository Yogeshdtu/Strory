# 06 — GDB cheatsheet

Deep: folder `45-DEBUGGING` (02, 03, 08 — with real transcripts on this box,
MinGW GCC 15.1.0 / GDB 16.3). Build with `-g` (`-O0` or `-Og` for sane stepping).

```
gdb ./prog                 gdb --args ./prog arg1 arg2
gdb -p <pid>               # attach to a running process
gdb ./prog core            # post-mortem on a core dump
gdb -q -batch -ex "run" -ex "bt" --args ./prog     # scripted, one-shot
gdb -x script.gdb ./prog   # run commands from a file  (also ~/.gdbinit)
```

---

## Run / step

| Cmd | Short | Does |
|---|---|---|
| `run` | `r` | start (args persist) |
| `start` | | `break main` + run |
| `continue` | `c` | resume |
| `next` | `n` | step, **over** calls |
| `step` | `s` | step, **into** calls |
| `finish` | `fin` | run until current frame returns (prints retval) |
| `until <line>` | `u` | run until a line ahead (skips loops) |
| `stepi` / `nexti` | `si`/`ni` | one machine instruction |
| `return <val>` | | force-return from this frame |
| `kill` | `k` | stop the inferior |

---

## Breakpoints & watchpoints

```
break file.cpp:42            b 42            b func            b Klass::method
break f.cpp:42 if x > 100                    # conditional
tbreak ...                                    # one-shot
break ... thread 3
rbreak ^Order::                               # regex — break on every match
watch expr                                    # stop when it CHANGES (write)
rwatch expr    awatch expr                    # read / read-write
catch throw    catch catch    catch syscall write
info breakpoints    delete 2    disable 3    enable 3
condition 2 i == 500                          # add/change a condition
ignore 2 999                                  # skip next 999 hits
commands 2 \n silent \n printf "x=%d\n", x \n continue \n end
```

---

## Inspect

```
bt              bt full          # backtrace (+ locals per frame)
frame 2   f 2   up   down        # move between frames
info args       info locals      info registers   info frame
print expr   p expr              p/x  p/d  p/t  p/c  p/a    # hex/dec/binary/char/addr
p *ptr    p arr@10    p arr[3]@5                # array slices
p $rsp    p $pc                                # registers as convenience vars
display expr                                    # auto-print on each stop
x/16xb &buf    x/4i $pc    x/s ptr    x/8xg &v  # examine memory (fmt/count/size)
ptype obj    whatis expr    info line f.cpp:42
list   l 42   l func                            # source
set var x = 5                                   # mutate a variable
set print pretty on    set pagination off       # nicer output (put in ~/.gdbinit)
```

STL pretty-printers: usually on with modern libstdc++ (`p myVector` shows
elements). If not: `python import sys; ...` or `-enable-pretty-printing`.

---

## Threads

```
info threads
thread 3                        # switch
thread apply all bt             # every thread's stack — deadlock hunting
set scheduler-locking on        # step only the current thread
break f.cpp:10 thread 2
```

Deadlock signature: ≥ 2 threads parked in `__lll_lock_wait` / `pthread_mutex_lock`,
each holding what the other wants. (folder 45/08)

---

## Signals & crashes

```
handle SIGPIPE nostop noprint pass
info signals
# on SIGSEGV: bt, then `frame` to the crash, `info locals`, `x` the bad pointer
```

Shell exit code `128 + signum`: 139 = SEGV (11), 134 = ABRT (6), 136 = FPE (8).

---

## -O2 debugging reality (folder 45/05)

- `<optimized out>` — value not live in a register/stack slot here. Use `-Og`,
  or move the breakpoint, or read the asm.
- Function-entry arg values can be garbage until the prologue settles.
- Inlined callees have no own frame; one `bt` line covers several source funcs.
- A `constexpr`-foldable call (`factorial(5)`) may not exist in the binary at all.

---

## TUI & layout

```
gdb -tui           Ctrl-x a  (toggle)     Ctrl-x 2 (2 windows)
layout src    layout asm    layout regs    layout split
focus cmd / src    refresh
```

---

## `~/.gdbinit` starter

```
set print pretty on
set pagination off
set history save on
set disassembly-flavor intel
python
import subprocess, sys
end
```

## Next
→ [`07-perf-commands.md`](07-perf-commands.md)

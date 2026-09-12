# 09 — Program vs Executable vs Process

## Prerequisites
`08-what-happens-when-you-press-run.md`

## Yeh topic abhi kyun
Yeh teen shabd log aksar mila dete hain. Inka fark samajhna zaroori hai — kyunki jab
aap folder 26 (concurrency) aur folder 29 (Linux) pe pahunchoge, yeh distinction
foundation hogi.

---

## Teenon ka fark — ek analogy

Socho ek **recipe** ke baare mein:

| Cheez | Recipe wala example | Computer wala |
|---|---|---|
| **Program** | Recipe ka **idea** — "chai kaise banti hai" | Aapka source code / algorithm |
| **Executable** | Recipe **kagaz pe likhi hui**, almari mein padi | `.exe` / `a.out` file, SSD pe |
| **Process** | **Aap abhi chai bana rahe ho** — gas jal rahi hai, pateela garam hai | Chalta hua program, RAM mein |

**Key insight:** Ek recipe se aap **kai baar** chai bana sakte ho, ek saath bhi
(do stoves pe). Waise hi ek executable se **kai processes** ban sakte hain.

---

## 1. PROGRAM

**Program = instructions ka set, ek abstract idea.**

Yeh source code ho sakta hai, ya bas aapke dimaag mein ek algorithm.

```cpp
// yeh ek program hai
int main() {
    std::cout << "Hi\n";
}
```

Program **kahin chal nahi raha**. Yeh bas ek description hai.

---

## 2. EXECUTABLE

**Executable = program ka compiled, chalne-layak version, ek file ke roop mein.**

- Storage (SSD) pe padi hai
- Binary format mein hai (Linux: **ELF**, Windows: **PE**, macOS: **Mach-O**)
- Isme machine code hai + metadata
- **Abhi bhi chal nahi rahi** — bas padi hui hai

```bash
ls -lh hello        # -rwxr-xr-x  1 user user 16K hello
file hello          # ELF 64-bit LSB pie executable, x86-64...
```

Note karo `x` in permissions (`rwx`) — yeh batata hai ki file executable hai.

### Executable ke andar kya hai?

```bash
readelf -h hello        # ELF header
readelf -S hello        # sections
size hello              # text/data/bss sizes
```

Sections:
| Section | Kya hai |
|---|---|
| `.text` | machine code (aapke functions) |
| `.rodata` | read-only data (string literals jaise `"Hello"`) |
| `.data` | initialized global variables |
| `.bss` | uninitialized globals (sirf size, content nahi) |
| `.symtab` | symbol table |

---

## 3. PROCESS

**Process = chalta hua program, RAM mein, OS ki management ke saath.**

Jab aap `./hello` chalate ho, OS ek process banata hai. Process ke paas hota hai:

| Cheez | Detail |
|---|---|
| **PID** | Process ID — unique number |
| **Virtual address space** | Apni private memory, doosre process ise nahi dekh sakte |
| **Registers** | CPU ki current state |
| **Program counter** | abhi kaunsi instruction chal rahi hai |
| **Stack** | function calls, local variables |
| **Heap** | dynamic memory |
| **File descriptors** | khuli hui files/sockets |
| **Threads** | kam se kam ek |
| **Credentials** | kis user ka hai, kya permissions hain |

```bash
./hello &            # background mein chalao
ps aux | grep hello  # dekho
cat /proc/<PID>/maps # iski memory map dekho (Linux)
```

---

## Ek executable, kai processes

Yeh important hai:

```bash
# terminal 1
./myprogram          # PID 1001

# terminal 2
./myprogram          # PID 1002 - alag process!

# terminal 3
./myprogram          # PID 1003
```

Teen processes, ek hi executable file se. Har ek ki **apni alag memory** hai.
Ek mein variable badalne se doosre pe koi asar nahi.

Yehi wajah hai ki Chrome mein har tab ek alag process hota hai — ek tab crash ho to
baaki bache rahein.

---

## Process vs Thread (chhota preview)

Yeh folder 26 mein detail mein aayega, par abhi basic fark:

```
   PROCESS A                        PROCESS B
   +------------------------+       +------------------------+
   |  Memory (private)      |       |  Memory (private)      |
   |  +------------------+  |       |  +------------------+  |
   |  | code, data, heap |  |       |  | code, data, heap |  |
   |  +------------------+  |       |  +------------------+  |
   |                        |       |                        |
   |  Thread 1  [stack]     |       |  Thread 1  [stack]     |
   |  Thread 2  [stack]     |       |                        |
   |  Thread 3  [stack]     |       |                        |
   +------------------------+       +------------------------+
   
   Threads MEMORY SHARE karte hain      Processes NAHI karte
```

| | Process | Thread |
|---|---|---|
| Memory | apni alag | share karte hain |
| Banane ki cost | mehngi (~100 µs) | sasti (~10 µs) |
| Crash ka asar | sirf khud pe | poore process pe |
| Communication | mushkil (IPC chahiye) | aasan (shared memory) |
| Data races | nahi ho sakte | **ho sakte hain** ⚠️ |

> **HFT relevance:** HFT systems mein aksar **ek process, kai threads** hote hain,
> aur har thread ek dedicated CPU core pe pinned hota hai. Kyun? Kyunki threads ke beech
> data share karna processes se bahut tez hai (shared memory), aur context switching
> ka kharcha bach jaata hai. Folder 41 mein detail.

---

## Process lifecycle

```
       fork()/exec()
            |
            v
       +----------+
       |   NEW    |
       +----------+
            |
            v
       +----------+  scheduler chunta hai  +-----------+
       |  READY   | ---------------------> |  RUNNING  |
       +----------+ <--------------------- +-----------+
            ^        time slice khatam        |    |
            |                                 |    | I/O ke liye rukna
            |                                 |    v
            |                            +----------+
            +----------------------------|  BLOCKED |
                  I/O poora hua          +----------+
                                              |
                                              v (exit)
                                        +-----------+
                                        | TERMINATED|
                                        +-----------+
```

> **HFT relevance:** Woh `RUNNING → BLOCKED → READY → RUNNING` cycle **latency ka
> dushman** hai. Har transition mein context switch hota hai (~1-5 µs). Isliye HFT
> mein hum **busy-spinning** karte hain — thread sota hi nahi, loop mein check karta
> rehta hai. CPU waste hoti hai, par latency predictable rehti hai. Folder 36/41.

---

## Exit codes

Jab process khatam hota hai, woh ek number OS ko deta hai.

```cpp
int main() {
    return 0;      // 0 = success
}
```

| Code | Convention |
|---|---|
| `0` | Sab theek |
| `1` | General error |
| `2` | Misuse of command |
| `130` | Ctrl+C se maara gaya |
| `139` | Segmentation fault (128 + 11) |

```bash
./hello
echo $?         # exit code dekho
```

Yeh important hai kyunki scripts aur build systems isi se pata karte hain ki kaam hua
ya nahi.

---

## Hands-on

```bash
cd ~/cpp-practice

cat > longrun.cpp << 'END'
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    // 30 second tak chalta rahega taaki hum ise inspect kar sakein
    for (int i = 0; i < 30; ++i) {
        std::cout << "Chal raha hoon... " << i << "\n";
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;
}
END

g++ -std=c++20 longrun.cpp -o longrun
./longrun &                    # background mein chalao
PID=$!
echo "PID hai: $PID"

ps -p $PID -o pid,ppid,stat,rss,comm    # process info
cat /proc/$PID/status | head -12        # Linux only
ls /proc/$PID/fd                        # file descriptors
cat /proc/$PID/maps | head              # memory map

kill $PID                               # band karo
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Program aur process same hain" | Program = file, Process = chalti hui instance |
| "Ek executable = ek process" | Ek executable se hazaaron processes ban sakte hain |
| "Process aur thread same hain" | Threads memory share karte hain, processes nahi |
| "Program band karne pe memory leak ho jaati hai" | Nahi, OS process ki saari memory wapas le leta hai |
| "Executable mein source code hota hai" | Nahi (unless debug symbols). Sirf machine code |

---

## Exercises

1. Upar wala hands-on chalao. `/proc/<PID>/maps` mein kya dikha? Kya aapko `.text`,
   `heap`, `stack` labels dikhe?

2. Ek program ko **teen** terminals mein ek saath chalao. `ps aux | grep` se dekho —
   kitne processes hain? PIDs alag hain?

3. Exit code test:
   ```bash
   cat > exitcode.cpp << 'END'
   int main() { return 42; }
   END
   g++ exitcode.cpp -o exitcode && ./exitcode; echo "Exit code: $?"
   ```

4. Crash karke exit code dekho:
   ```bash
   cat > crash.cpp << 'END'
   int main() { int* p = nullptr; *p = 5; return 0; }
   END
   g++ crash.cpp -o crash && ./crash; echo "Exit code: $?"
   ```
   <details><summary>Answer</summary>
   `Segmentation fault` aur exit code `139` (= 128 + 11, jahan 11 = SIGSEGV).
   Aapne null pointer ko dereference kiya — folder 12 mein detail mein padhenge.
   </details>

5. `size hello` chalao. `text`, `data`, `bss` ke numbers kya hain?

---

## Interview questions

1. Process aur thread mein kya fark hai?
2. Ek executable se kai processes ban sakte hain kya? Woh memory share karte hain?
3. Context switch kya hai aur woh mehnga kyun hai?
4. Process ka virtual address space kya hai?

---

## Next
→ [`10-bits-bytes-binary.md`](10-bits-bytes-binary.md)

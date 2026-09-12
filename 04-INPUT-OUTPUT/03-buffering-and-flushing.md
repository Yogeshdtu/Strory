# 03 — Buffering aur flushing

## Prerequisites
`01-cout-deep-dive.md`, folder 01 lesson 08 (syscalls)

## Yeh topic abhi kyun
Folder 02 mein aapne dekha tha ki `std::endl` slow hai. Ab **kyun** samjhenge.

Aur yeh **HFT logging** ka core concept hai — kyunki har flush ek syscall hai,
aur syscalls latency kill karte hain.

---

## Buffer kya hai?

Jab aap `std::cout << "Hello"` likhte ho, woh text **turant screen pe nahi jaata**.

Woh pehle ek **buffer** (memory ka chunk) mein jama hota hai. Jab buffer bhar jaata
hai, ya koi trigger aata hai, tab woh sab ek saath OS ko bheja jaata hai.

```
   Aapka program                    OS / Terminal
   
   cout << "A"  ──┐
   cout << "B"  ──┼──> [ BUFFER: "ABC..." ]  ──(flush)──> write() syscall ──> screen
   cout << "C"  ──┘         (memory mein)
```

---

## Buffer kyun hai?

**Kyunki syscalls mehnge hote hain.**

Har `write()` syscall mein:
1. User mode se kernel mode mein switch
2. Kernel ka kaam
3. Wapas user mode mein switch

Cost: **~500–2000 nanoseconds** per syscall.

### Bina buffer ke
```
   1000 characters print karna = 1000 syscalls = ~1,000,000 ns = 1 ms
```

### Buffer ke saath
```
   1000 characters buffer mein jama = 1 syscall = ~1,000 ns
```

**1000x faster.** Yehi buffering ka poora point hai.

---

## Flush kab hota hai?

Buffer khali hone ke 5 tareeke:

| Trigger | Kab |
|---|---|
| **Buffer bhar gaya** | Automatic (usually 4KB–64KB pe) |
| **`std::endl`** | Har baar, zabardasti |
| **`std::flush`** | Har baar, zabardasti |
| **Program khatam** | `main` se return / `exit()` pe |
| **`cin` se read** | Kyunki `cin` `cout` se tied hai |
| **`cerr` pe likhna** | `cerr` unbuffered hai (`unitbuf` flag) |

---

## `\n` vs `std::endl` — poora sach

```cpp
std::cout << "Hello\n";              // sirf newline daala. NO FLUSH.
std::cout << "Hello" << std::endl;   // newline daala + FLUSH kiya (syscall!)
```

`std::endl` ka source roughly:
```cpp
ostream& endl(ostream& os) {
    os.put('\n');
    os.flush();          // <-- yeh syscall hai
    return os;
}
```

### Benchmark

```cpp
#include <iostream>
#include <chrono>

int main() {
    const int N = 100000;

    auto t1 = std::chrono::steady_clock::now();
    for (int i = 0; i < N; ++i) std::cout << i << "\n";
    auto t2 = std::chrono::steady_clock::now();

    for (int i = 0; i < N; ++i) std::cout << i << std::endl;
    auto t3 = std::chrono::steady_clock::now();

    std::cerr << "\\n:   "
              << std::chrono::duration<double,std::milli>(t2-t1).count() << " ms\n";
    std::cerr << "endl: "
              << std::chrono::duration<double,std::milli>(t3-t2).count() << " ms\n";
}
```

```bash
g++ -std=c++20 -O2 bench.cpp -o bench
./bench > /dev/null          # output discard, sirf timing dekho
```

Typical result: **`endl` 5–50x slower.**

Aur `strace -c` se dekho:
```bash
strace -c -e trace=write ./bench > /dev/null
```
`endl` version mein **bahut zyada** `write` syscalls dikhenge.

---

## `endl` kab use karein?

**Bahut kam.** Sirf jab:

1. **Crash se pehle output chahiye** — agar program crash ho gaya, buffered output kho
   jaayega
2. **Interactive prompts** jahan turant dikhna zaroori ho (aur `cin` tie nahi hai)
3. **Long-running program ka progress** jo aap live dekh rahe ho
4. **Doosre process ko pipe kar rahe ho** aur woh turant chahiye

Baaki 95% cases mein: **`\n` use karo.**

```cpp
// ✅ Normal
std::cout << "Processing...\n";

// ✅ Jab pakka flush chahiye
std::cout << "Critical\n" << std::flush;

// ⚠️ Aadat mat banao
std::cout << "text" << std::endl;
```

---

## Buffering ke 3 modes

| Mode | Kab flush | Kaun use karta hai |
|---|---|---|
| **Fully buffered** | buffer bharne pe | files, pipes |
| **Line buffered** | har `\n` pe | terminal (usually) |
| **Unbuffered** | turant | `std::cerr`, `stderr` |

### 🔑 Yeh important hai

**Terminal pe** `cout` usually **line-buffered** hota hai — matlab `\n` pe waise bhi
flush ho jaata hai.

**File/pipe mein** `cout` **fully buffered** hota hai — `\n` pe flush nahi hota.

Isliye:
```bash
./program                 # terminal -- output turant dikhta hai
./program > out.txt       # file -- output chunks mein aata hai
./program | grep foo      # pipe -- output chunks mein aata hai
```

Yeh explain karta hai ek classic confusion:
```cpp
std::cout << "Start\n";
crashNow();                // program crash
```
Terminal pe "Start" dikh jaayega. File mein redirect karo to **shayad nahi** — buffer
mein hi reh gaya.

---

## Crash ke waqt output kho jaata hai

```cpp
#include <iostream>
int main() {
    std::cout << "Line 1\n";
    std::cout << "Line 2\n";
    int* p = nullptr;
    *p = 5;                    // CRASH
    std::cout << "Line 3\n";
}
```

```bash
./crash              # terminal: "Line 1", "Line 2" dikh jaayenge (line buffered)
./crash > out.txt    # file: out.txt shayad KHALI hoga! (buffer flush nahi hua)
cat out.txt
```

**Isliye debugging output ke liye `cerr` use karo** — woh unbuffered hai.

---

## `std::cerr` unbuffered kyun hai?

```cpp
std::cerr << "Error!\n";      // turant dikhega, chahe crash ho jaaye
```

`cerr` ka `unitbuf` flag set hota hai — har operation ke baad auto-flush.

```cpp
std::cerr.flags() & std::ios::unitbuf;      // true
```

**Trade-off:** safe hai, par **slow** hai. Isliye normal output ke liye `cout`.

---

## Manual buffer control

```cpp
// Flush karo
std::cout << std::flush;
std::cout.flush();

// unitbuf on karo (har operation ke baad flush -- SLOW)
std::cout << std::unitbuf;
std::cout << std::nounitbuf;     // wapas normal

// Buffer size badalna (advanced)
char buf[65536];
std::cout.rdbuf()->pubsetbuf(buf, sizeof(buf));
```

---

## 🔴 HFT: logging aur buffering

Yeh section poore folder ka sabse important hissa hai.

### Problem

```cpp
void onMarketData(const Message& msg) {
    LOG << "Received " << msg.type << " seq=" << msg.seq << std::endl;   // ⚠️ DISASTER
    processMessage(msg);
}
```

Har message pe:
- Formatting (string building — shayad allocation!)
- `endl` → `write()` syscall (~500-2000 ns)
- Disk I/O ka wait

Aapka 5 µs ka budget kha gaya.

### Solution: asynchronous logging

```
   HOT PATH THREAD                    LOGGER THREAD
   ───────────────                    ─────────────
   
   onMarketData()
      |
      +-> ring buffer mein            [ LOCK-FREE QUEUE ]
          raw data DAAL do  ────────>       |
          (~20 ns)                          |
      |                                     v
      +-> processMessage()            format karo
                                      file mein likho
                                      flush karo
                                      (hot path ko koi farak nahi)
```

**Hot path sirf yeh karta hai:**
1. Ek pre-allocated slot le lo
2. Raw binary data copy kar do (koi formatting nahi!)
3. Aage badh jao

**Logger thread:** dhire se format karke disk pe likhta rahe.

Yeh pattern folder 41 mein poora banayenge.

### Rules

| ❌ Hot path mein | ✅ Iski jagah |
|---|---|
| `std::endl` | `\n`, ya kuch bhi nahi |
| String formatting | Raw binary values queue mein |
| `std::cout` / `printf` | Lock-free ring buffer |
| Direct file write | Async logger thread |
| Log level check runtime pe | `if constexpr` / compile-time |

---

## Hands-on

`examples/03_endl_benchmark.cpp` chalao.

```bash
cd examples
g++ -std=c++20 -O2 03_endl_benchmark.cpp -o bench
./bench > /dev/null

# Syscalls count karo
strace -c -e trace=write ./bench > /dev/null 2>&1 | tail -5
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`endl` sirf newline hai" | Newline + **flush** (syscall) |
| "`\n` bhi flush karta hai" | Nahi. Terminal pe line-buffering karta hai, `\n` khud nahi |
| "Buffering se output kho jaata hai" | Normal exit pe flush ho jaata hai. Sirf crash pe kho sakta hai |
| "`cerr` `cout` jaisa hai" | `cerr` unbuffered hai — safe par slow |
| "Buffer size fixed hai" | Implementation-defined, aur badla ja sakta hai |

---

## Exercises

1. `endl` benchmark chalao. Kitna fark mila?

2. Syscalls count karo:
   ```bash
   strace -c -e trace=write ./bench > /dev/null
   ```
   `\n` aur `endl` versions ke liye alag-alag chalao.

3. Crash test:
   ```cpp
   #include <iostream>
   int main() {
       std::cout << "Buffered line\n";
       std::cerr << "Unbuffered line\n";
       int* p = nullptr; *p = 5;
   }
   ```
   ```bash
   g++ crash.cpp -o crash
   ./crash                    # dono dikhe?
   ./crash > out.txt 2>err.txt
   cat out.txt                # khali?
   cat err.txt                # bhara?
   ```
   <details><summary>Answer</summary>
   `out.txt` khali hoga (cout ka buffer flush nahi hua — file mein fully buffered).
   `err.txt` mein message hoga (cerr unbuffered hai).

   **Yehi wajah hai ki error messages `cerr` pe jaate hain.**
   </details>

4. Line-buffered vs fully-buffered ka fark dekho:
   ```cpp
   #include <iostream>
   #include <thread>
   int main() {
       for (int i = 0; i < 5; ++i) {
           std::cout << "Line " << i << "\n";
           std::this_thread::sleep_for(std::chrono::seconds(1));
       }
   }
   ```
   ```bash
   ./prog              # ek-ek line aati hai?
   ./prog | cat        # sab ek saath aayi?
   ```

5. `std::unitbuf` on karke benchmark dobara chalao. `endl` jitna slow hua?

6. Sochkar batao: HFT hot path mein logging ke liye 3 rules likho.

---

## Interview questions

1. Buffering kyun hoti hai?
2. `\n` aur `std::endl` mein exact fark?
3. `cerr` unbuffered kyun hai?
4. Crash pe output kyun kho jaata hai?
5. HFT mein logging kaise karte hain aur kyun?

---

## Next
→ [`04-cerr-and-clog.md`](04-cerr-and-clog.md)

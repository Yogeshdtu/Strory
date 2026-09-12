# 04 — `cerr`, `clog`, aur stream redirection

## Prerequisites
`03-buffering-and-flushing.md`

## Yeh topic abhi kyun
Errors ko normal output se **alag** rakhna professional practice hai. Aur yeh Unix
philosophy ka core hai — jo aage shell scripting aur production logging mein kaam aayega.

---

## Teen output streams

| Stream | File descriptor | Buffered? | Kis liye |
|---|---|---|---|
| `std::cout` | 1 (stdout) | ✅ haan | normal program output |
| `std::cerr` | 2 (stderr) | ❌ **nahi** (unitbuf) | errors, warnings — turant chahiye |
| `std::clog` | 2 (stderr) | ✅ haan | logging — buffered, par stderr pe |

Aur input:
| `std::cin` | 0 (stdin) | ✅ haan | input |

---

## Yeh alag kyun hain?

Taaki aap unhe **alag-alag redirect** kar sako.

```bash
./program                      # dono screen pe
./program > output.txt         # sirf cout file mein, cerr screen pe
./program 2> errors.txt        # sirf cerr file mein, cout screen pe
./program > out.txt 2> err.txt # dono alag files mein
./program > all.txt 2>&1       # dono ek file mein
./program 2>/dev/null          # errors discard karo
./program > /dev/null          # normal output discard karo
```

### Yeh practically kaam kaise aata hai

```bash
# Ek program jo data process karke result deta hai
./parser < data.txt > results.csv 2> errors.log

# Ab results.csv mein SIRF data hai (errors se ganda nahi hua)
# Aur errors.log mein sab problems hain
```

Agar aapne errors bhi `cout` pe bheje hote, to `results.csv` corrupt ho jaati.

---

## Practical use

```cpp
#include <iostream>

int main() {
    // Normal output -> cout
    std::cout << "Processing 1000 records...\n";
    std::cout << "Result: 42\n";

    // Errors/warnings -> cerr
    std::cerr << "Warning: 3 records skipped\n";
    std::cerr << "Error: file not found\n";

    // Diagnostic logging -> clog (buffered, stderr pe)
    std::clog << "[DEBUG] cache hit rate: 87%\n";
}
```

**Rule:**
- Program ka **actual output** → `cout`
- **Kuch galat hua** → `cerr`
- **Diagnostic/progress info** → `cerr` ya `clog`

---

## `cerr` unbuffered hone ka faayda

```cpp
#include <iostream>
int main() {
    std::cout << "Yeh cout pe hai\n";
    std::cerr << "Yeh cerr pe hai\n";
    int* p = nullptr;
    *p = 5;                              // CRASH
}
```

```bash
./crash > out.txt 2> err.txt
cat out.txt      # ⚠️ KHALI (buffer flush nahi hua)
cat err.txt      # ✅ "Yeh cerr pe hai" (unbuffered)
```

**Isliye debugging messages `cerr` pe daalo** — crash ke baad bhi dikhenge.

---

## `cerr` vs `clog`

Dono **stderr (fd 2)** pe jaate hain. Fark sirf buffering ka hai:

```cpp
std::cerr << "msg\n";     // turant flush -- SLOW, par guaranteed
std::clog << "msg\n";     // buffered -- FAST, par crash pe kho sakta hai
```

| | `cerr` | `clog` |
|---|---|---|
| Buffered | ❌ nahi | ✅ haan |
| Speed | slow | fast |
| Crash pe survive? | ✅ haan | ❌ shayad nahi |
| Kis liye | critical errors | high-volume logging |

**Practical:** `clog` kam use hota hai. Log volume zyada ho to log libraries
(spdlog, glog) use hoti hain — jo async hoti hain.

---

## Benchmark: `cout` vs `cerr`

```cpp
#include <iostream>
#include <chrono>

int main() {
    const int N = 50000;

    auto t1 = std::chrono::steady_clock::now();
    for (int i = 0; i < N; ++i) std::cout << i << "\n";
    std::cout.flush();
    auto t2 = std::chrono::steady_clock::now();

    for (int i = 0; i < N; ++i) std::cerr << i << "\n";
    auto t3 = std::chrono::steady_clock::now();

    for (int i = 0; i < N; ++i) std::clog << i << "\n";
    std::clog.flush();
    auto t4 = std::chrono::steady_clock::now();

    // Timing ko ek THIRD jagah bhejo taaki measurement affect na ho
    FILE* tty = fopen("/dev/tty", "w");
    if (tty) {
        fprintf(tty, "cout: %.2f ms\n",
                std::chrono::duration<double,std::milli>(t2-t1).count());
        fprintf(tty, "cerr: %.2f ms\n",
                std::chrono::duration<double,std::milli>(t3-t2).count());
        fprintf(tty, "clog: %.2f ms\n",
                std::chrono::duration<double,std::milli>(t4-t3).count());
        fclose(tty);
    }
}
```

```bash
g++ -std=c++20 -O2 bench.cpp -o bench
./bench > /dev/null 2>/dev/null
```

Typical: `cerr` `cout`/`clog` se **5-20x slow**.

---

## Redirection code se

```cpp
#include <fstream>
#include <iostream>

int main() {
    std::ofstream logFile("output.log");

    // cout ka buffer save karo
    std::streambuf* oldBuf = std::cout.rdbuf();

    // cout ko file pe redirect karo
    std::cout.rdbuf(logFile.rdbuf());

    std::cout << "Yeh file mein jaayega\n";

    // Wapas normal karo
    std::cout.rdbuf(oldBuf);
    std::cout << "Yeh screen pe aayega\n";
}
```

**Better: RAII wrapper banao** (folder 17 ka preview):
```cpp
class CoutRedirect {
    std::streambuf* old_;
public:
    explicit CoutRedirect(std::streambuf* newBuf)
        : old_(std::cout.rdbuf(newBuf)) {}
    ~CoutRedirect() { std::cout.rdbuf(old_); }      // ✅ automatic restore

    CoutRedirect(const CoutRedirect&) = delete;
    CoutRedirect& operator=(const CoutRedirect&) = delete;
};

{
    std::ofstream f("out.log");
    CoutRedirect redirect(f.rdbuf());
    std::cout << "file mein\n";
}   // <- yahan automatically restore ho gaya
std::cout << "screen pe\n";
```

---

## Exit codes ke saath combination

Professional programs dono use karte hain:

```cpp
#include <iostream>
#include <cstdlib>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <filename>\n";   // ✅ cerr
        return EXIT_FAILURE;                                     // ✅ non-zero exit
    }

    std::cout << "Processing " << argv[1] << "\n";               // ✅ cout
    return EXIT_SUCCESS;
}
```

```bash
./prog                    # usage message dikha, exit code 1
echo $?                   # 1
./prog file.txt           # kaam hua, exit code 0
echo $?                   # 0

# Script mein use karo
./prog data.txt && echo "Success!" || echo "Failed!"
```

---

## 🔴 HFT: logging streams

Production trading systems mein:

```
   TRADING PROCESS
   ├── stdout  ──> normal output (aksar khali, ya bas startup info)
   ├── stderr  ──> critical errors -> monitoring/alerting system
   └── async logger ──> ring buffer ──> logger thread ──> log files
                                                     ──> log aggregator
```

**Rules:**
1. Hot path mein `cout`/`cerr` **kabhi nahi** — dono syscalls karte hain
2. Critical errors `cerr` pe (guaranteed delivery), par **hot path ke bahar**
3. High-volume logging async (folder 41)
4. Exit codes se monitoring ko batao ki clean shutdown tha ya crash

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`cerr` alag jagah jaata hai" | Screen pe hi jaata hai — bas alag fd (2) pe |
| "`>` se sab kuch redirect ho jaata hai" | Sirf stdout. `2>` chahiye stderr ke liye |
| "`cerr` `cout` se tez hai" | Ulta — unbuffered hai, isliye slow |
| "`clog` ki zarurat nahi" | High-volume logging mein `cerr` se behtar hai |
| "Errors `cout` pe bhi chal jaate hain" | Redirect karne pe data corrupt hota hai |

---

## Exercises

1. Ek program likho jo `cout`, `cerr`, `clog` teenon pe likhe. Phir:
   ```bash
   ./prog > out.txt
   ./prog 2> err.txt
   ./prog > out.txt 2> err.txt
   ./prog > all.txt 2>&1
   ```
   Har case mein kya kahan gaya?

2. Crash test karo (upar wala example). `out.txt` khali aaya?

3. `cerr` vs `cout` benchmark chalao. Kitna fark?

4. `CoutRedirect` RAII class likho aur test karo.

5. Ek program likho jo argument leta hai. Agar argument na ho:
   - `cerr` pe usage message
   - `EXIT_FAILURE` return
   Test karo shell mein `&&` aur `||` ke saath.

6. Sochkar batao: yeh command kya karega?
   ```bash
   ./prog 2>&1 | grep ERROR
   ```
   <details><summary>Answer</summary>
   `2>&1` stderr ko stdout mein merge karta hai, phir dono `grep` mein jaate hain.
   Isse aap errors ko bhi filter kar sakte ho.

   ⚠️ Order matter karta hai: `./prog | grep ERROR 2>&1` **alag** cheez karta hai
   (grep ka stderr redirect karta hai, prog ka nahi).
   </details>

---

## Next
→ [`05-getline-and-strings.md`](05-getline-and-strings.md)

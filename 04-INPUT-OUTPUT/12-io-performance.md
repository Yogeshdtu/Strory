# 12 — I/O performance

## Prerequisites
`03-buffering-and-flushing.md`, `11-printf-family.md`

## Yeh topic abhi kyun
I/O aksar programs ka sabse bada bottleneck hota hai. Aur HFT mein logging **hot path
ka #1 dushman** hai.

Yeh folder ka last technical lesson hai — aur sabse practical.

---

## Baseline: kya slow hai?

Roughly (1 million lines, relative):

```
   printf                    1.0x   (baseline)
   std::print (C++23)        0.7x   ✅ fastest
   std::format + cout        0.9x
   cout << (sync off)        1.2x
   cout << (sync ON, default) 3-5x  ⚠️
   cout << ... << endl       15-50x ⚠️⚠️
   ostringstream per line    20x    ⚠️⚠️
```

---

## Fix 1: `sync_with_stdio(false)` 🔑

**Default se C++ streams C ke `stdio` ke saath synchronized hote hain.**

Iska matlab: aap `printf` aur `std::cout` mix kar sakte ho aur output sahi order mein
aayega. Iske liye `cout` apna buffer use nahi karta — har operation `stdio` ke through
jaata hai.

```cpp
#include <iostream>

int main() {
    std::ios::sync_with_stdio(false);     // ✅ 2-5x faster
    // ...
}
```

### ⚠️ Iske baad
- `printf` aur `std::cout` **mix mat karo** — output ka order galat ho sakta hai
- `std::cin` aur `scanf` bhi mix mat karo

**Competitive programming mein yeh standard hai.** Production mein bhi safe hai
agar aap C stdio use nahi kar rahe.

---

## Fix 2: `cin.tie(nullptr)`

Default mein `std::cin` `std::cout` se **tied** hai — har `cin` operation se pehle
`cout` flush hota hai.

```cpp
std::cin.tie(nullptr);        // ✅ flush hatao
```

### ⚠️ Iske baad
Prompts atak sakte hain:
```cpp
std::cin.tie(nullptr);
std::cout << "Naam? ";        // ⚠️ shayad na dikhe
std::cin >> name;             // user ko pata hi nahi kya poocha
```

Fix — manually flush karo jab prompt chahiye:
```cpp
std::cout << "Naam? " << std::flush;
std::cin >> name;
```

### Standard fast I/O setup
```cpp
int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    // ...
}
```

---

## Fix 3: `endl` mat use karo

Yeh sabse bada single improvement hai (file 03 se).

```cpp
std::cout << "line" << std::endl;      // ❌ har line pe syscall
std::cout << "line\n";                 // ✅
```

**5-50x fark.**

---

## Fix 4: Bada buffer

```cpp
#include <iostream>

int main() {
    static char buf[1 << 20];                       // 1 MB
    std::cout.rdbuf()->pubsetbuf(buf, sizeof(buf));
    // ...
}
```

Bade outputs ke liye kam syscalls.

⚠️ Yeh stream use karne se **pehle** karna padta hai.

---

## Fix 5: Ek baar mein zyada likho

```cpp
// ❌ 4 operator<< calls
std::cout << "a" << "b" << "c" << "\n";

// ✅ 1 call (adjacent literals compiler jodh deta hai)
std::cout << "abc\n";

// ✅ Loop mein: string build karo, ek baar likho
std::string output;
output.reserve(1 << 20);
for (int i = 0; i < N; ++i) {
    output += std::to_string(i);
    output += '\n';
}
std::cout << output;                    // ek hi write
```

---

## Fix 6: `std::format` / `std::print` use karo

```cpp
// Slowest
std::cout << std::fixed << std::setprecision(2) << std::setw(10) << x << "\n";

// Better
std::cout << std::format("{:>10.2f}\n", x);

// Best (C++23)
std::print("{:>10.2f}\n", x);
```

---

## Reading performance

### Slow
```cpp
std::string line;
while (std::getline(std::cin, line)) {      // ⚠️ har line pe string resize ho sakti hai
    process(line);
}
```

### Better — string reuse karo
```cpp
std::string line;
line.reserve(1024);                          // pehle se allocate
while (std::getline(std::cin, line)) {       // ✅ same buffer reuse hota hai
    process(line);
}
```

`getline` string ko **clear karke reuse** karta hai — capacity bani rehti hai.

### Fastest — poori file ek baar mein
```cpp
#include <fstream>
#include <sstream>

std::ifstream in("data.txt", std::ios::binary);
std::stringstream ss;
ss << in.rdbuf();                            // ek hi bada read
std::string content = ss.str();
// Ab memory mein parse karo (string_view se, no copies)
```

### Sabse fast — `mmap` (folder 29)
```cpp
// Bade files ke liye -- file ko memory mein map karo
// Koi copy nahi, koi read syscall nahi
```

---

## Benchmark example

```cpp
#include <iostream>
#include <chrono>
#include <string>
#include <format>

int main(int argc, char** argv) {
    const int N = 200000;
    const int mode = (argc > 1) ? std::atoi(argv[1]) : 0;

    auto t1 = std::chrono::steady_clock::now();

    switch (mode) {
        case 0:  // baseline: cout with sync
            for (int i = 0; i < N; ++i) std::cout << i << "\n";
            break;
        case 1:  // endl (worst)
            for (int i = 0; i < N; ++i) std::cout << i << std::endl;
            break;
        case 2:  // sync off
            std::ios::sync_with_stdio(false);
            for (int i = 0; i < N; ++i) std::cout << i << "\n";
            break;
        case 3:  // build string, one write
            {
                std::string out;
                out.reserve(N * 8);
                for (int i = 0; i < N; ++i) {
                    out += std::to_string(i);
                    out += '\n';
                }
                std::cout << out;
            }
            break;
    }
    std::cout.flush();
    auto t2 = std::chrono::steady_clock::now();

    std::cerr << "mode " << mode << ": "
              << std::chrono::duration<double,std::milli>(t2-t1).count() << " ms\n";
}
```

```bash
g++ -std=c++20 -O2 bench.cpp -o bench
for m in 0 1 2 3; do ./bench $m > /dev/null; done
```

---

## 🔴 HFT: I/O in the hot path

**Rule #1: hot path mein koi I/O nahi. Bilkul nahi.**

### Kyun

| Operation | Cost |
|---|---|
| `std::cout << x` (buffered) | ~50–200 ns (formatting) |
| `std::cout << x << endl` | ~500–2000 ns (syscall) |
| `printf` | ~100–300 ns |
| File write | ~1000–10000 ns |
| Disk flush | ~100 µs – 10 ms ⚠️ |

Aapka poora tick-to-trade budget **5 µs** hai. Ek `endl` uska 40% kha jaata hai.

### Solution: asynchronous logging

```
   ┌─────────────── HOT PATH THREAD ────────────────┐
   │  onMarketData(msg) {                           │
   │      logQueue.push(RawLogRecord{                │
   │          timestamp, msgType, seq, price        │  ← ~20 ns
   │      });                    // RAW VALUES,     │
   │      processMessage(msg);   // koi formatting  │
   │  }                          // nahi!           │
   └────────────────┬───────────────────────────────┘
                    │  lock-free SPSC ring buffer
                    ▼
   ┌─────────────── LOGGER THREAD ──────────────────┐
   │  while (running) {                             │
   │      auto rec = logQueue.pop();                │
   │      auto line = format(rec);   // ab format   │
   │      file.write(line);          // ab I/O      │
   │  }                                             │
   └────────────────────────────────────────────────┘
```

### Hot path mein kya karte hain
1. Ek pre-allocated slot le lo (ring buffer se)
2. **Raw binary values** copy kar do — koi string, koi format
3. Sequence number/index increment karo
4. Aage badh jao

**Total: ~20-50 ns.**

### Logger thread kya karta hai
- Records padhta hai
- Format karta hai (ab time hai)
- Batch karke file mein likhta hai
- Periodically flush karta hai

Yeh folder 41 mein poora banayenge.

### Aur rules

```cpp
// ❌ Runtime log level check
if (logLevel >= DEBUG) { log("..."); }        // branch har call pe

// ✅ Compile-time
if constexpr (kLogLevel >= DEBUG) { log("..."); }   // code hi nahi banta

// ❌ String building hot path mein
log("Order " + std::to_string(id) + " @ " + std::to_string(price));

// ✅ Raw values
logQueue.push({LogType::Order, id, price});
```

---

## Checklist

Normal programs ke liye:
```cpp
std::ios::sync_with_stdio(false);
std::cin.tie(nullptr);
// endl mat use karo
// std::format use karo
```

HFT hot path ke liye:
```
[ ] Hot path mein koi cout/printf/file write nahi
[ ] Async logging (lock-free queue + logger thread)
[ ] Raw values queue mein, formatting logger thread mein
[ ] Compile-time log levels
[ ] Pre-allocated buffers, koi runtime allocation nahi
[ ] Periodic batched flush, per-message flush nahi
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "iostream inherently slow hai" | Sync off + no endl ke saath fine hai |
| "`sync_with_stdio(false)` hamesha safe hai" | C stdio mix mat karo uske baad |
| "`cin.tie(nullptr)` ka koi side-effect nahi" | Prompts atak sakte hain |
| "Logging free hai" | Har log line ek measurable cost hai |
| "Buffered I/O hot path mein theek hai" | ❌ Formatting bhi mehngi hai |

---

## Exercises

1. Benchmark chalao (4 modes). Numbers note karo. Kaunsa kitna tez?

2. `sync_with_stdio(false)` ke saath aur bina, timing compare karo.

3. Mixing bug reproduce karo:
   ```cpp
   std::ios::sync_with_stdio(false);
   printf("From printf\n");
   std::cout << "From cout\n";
   printf("From printf again\n");
   ```
   Output ka order sahi aaya?

4. `cin.tie(nullptr)` ke saath prompt test karo — dikha?

5. Syscall count karo:
   ```bash
   strace -c -e trace=write ./bench 0 > /dev/null
   strace -c -e trace=write ./bench 1 > /dev/null
   strace -c -e trace=write ./bench 3 > /dev/null
   ```

6. Ek simple async logger banao (abhi `std::mutex` + `std::queue` se — lock-free
   version folder 41 mein):
   - Producer thread messages queue mein daale
   - Consumer thread file mein likhe
   - Producer ki latency measure karo

7. Sochkar batao: HFT hot path mein logging ke 6 rules likho.

---

## Interview questions

1. `sync_with_stdio(false)` kya karta hai? Side-effects?
2. `cin.tie(nullptr)` kyun aur kab?
3. `endl` kitna slow hai aur kyun?
4. HFT mein logging kaise karte hain?
5. Async logging mein hot path kya karta hai?

---

## Next
→ [`13-exercises.md`](13-exercises.md)

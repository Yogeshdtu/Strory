# Examples — Folder 04 (Input / Output)

| File | Kaunse lesson se | Kya dikhata hai |
|---|---|---|
| `01_cout_basics.cpp` | 01 | Chaining, types, **precedence trap**, pointer trap, custom `operator<<` |
| `02_cin_input.cpp` | 02, 06 | `>>`, **newline trap**, fail state recovery, robust loop |
| `03_endl_benchmark.cpp` | 03, 12 | ⭐ **`\n` vs `endl` — real measured numbers** |
| `04_manipulators.cpp` | 07 | `setw`/`setprecision`/`setfill`, **sticky traps**, table, hex dump, RAII state saver |
| `05_format_cpp20.cpp` | 08 | `std::format` poora tour + custom formatter |
| `06_file_io.cpp` | 09 | Text/binary I/O, truncate vs append, **RAII + exceptions** |
| `07_stringstream_parse.cpp` | 10 | Parsing, **`from_chars` vs `stoi` vs `istringstream` benchmark** |
| `08_robust_input.cpp` | 06 | ⭐ **Bulletproof input** — har edge case handled |

## Compile karne ka tarika

```bash
g++ -std=c++20 -Wall -Wextra -Wshadow -g 01_cout_basics.cpp -o cout && ./cout
```

Benchmarks ke liye **`-O2` zaroori hai**:
```bash
g++ -std=c++20 -O2 03_endl_benchmark.cpp -o bench && ./bench > /dev/null
g++ -std=c++20 -O2 07_stringstream_parse.cpp -o ssparse && ./ssparse
```

Ya Makefile se:
```bash
# repo root se
make fast FILE=04-INPUT-OUTPUT/examples/03_endl_benchmark.cpp
```

## Sab ek saath

```bash
for f in *.cpp; do
    echo "════════ $f ════════"
    g++ -std=c++20 -O2 -Wall -Wextra "$f" -o "/tmp/$(basename $f .cpp)"
done
```

## ⚠️ Interactive examples

`02_cin_input.cpp` aur `08_robust_input.cpp` **input maangte hain**.

Test karne ke liye yeh inputs try karo:
- `abc` — galat type
- `12abc` — trailing garbage
- `99999999999999999999` — overflow
- Khali line — bas Enter
- `Ctrl+D` (Linux/Mac) ya `Ctrl+Z` (Windows) — EOF

## Extra experiments

### 1. Syscalls count karo
```bash
g++ -std=c++20 -O2 03_endl_benchmark.cpp -o bench
strace -c -e trace=write ./bench > /dev/null
```
`endl` version mein **bahut zyada** `write` calls dikhenge.

### 2. Buffering ka fark dekho
```bash
g++ -std=c++20 06_file_io.cpp -o fileio
./fileio                    # terminal — line buffered
./fileio > out.txt          # file — fully buffered
```

### 3. Crash pe output kho jaata hai
```cpp
// crash_test.cpp
#include <iostream>
int main() {
    std::cout << "cout line\n";
    std::cerr << "cerr line\n";
    int* p = nullptr; *p = 5;
}
```
```bash
g++ crash_test.cpp -o crash
./crash > out.txt 2> err.txt
cat out.txt      # KHALI (buffer flush nahi hua)
cat err.txt      # BHARA (cerr unbuffered hai)
```

### 4. `printf` warnings
```bash
g++ -Wall -Wformat -Wformat-security yourfile.cpp
```

# Examples — Folder 02 (C++ First Steps)

| File | Kaunse lesson se | Kya dikhata hai |
|---|---|---|
| `01_hello_world.cpp` | 01, 02 | Pehla program, har token comment ke saath |
| `02_multiple_outputs.cpp` | 02 | Chaining, alag types, cout vs cerr, precedence trap |
| `03_comments_demo.cpp` | 07 | Comment styles, Doxygen, achhe vs bure comments |
| `04_escape_sequences.cpp` | 08 | `\n` `\t` `\\` `\"`, raw strings, sizeof, char-as-number |
| `05_scope_demo.cpp` | 06 | Blocks, scope, lifetime, **destruction order** |
| `06_broken_on_purpose.cpp` | 10 | ⚠️ 8 jaan-boojh kar dali gayi galtiyan — **aapko fix karni hain** |
| `07_return_codes.cpp` | 04 | Exit codes, `return` vs `std::exit()` |

## Compile karne ka tarika

```bash
g++ -std=c++20 -Wall -Wextra -Wshadow -g 01_hello_world.cpp -o hello
./hello
```

Ya root ke Makefile se:
```bash
# repo root se
make FILE=02-CPP-FIRST-STEPS/examples/01_hello_world.cpp
```

## Sab ek saath compile karo

```bash
for f in 0[1-5]*.cpp 07*.cpp; do
    echo "=== $f ==="
    g++ -std=c++20 -Wall -Wextra -Wshadow "$f" -o "/tmp/$(basename $f .cpp)" && \
    "/tmp/$(basename $f .cpp)"
done
```

## ⚠️ `06_broken_on_purpose.cpp` ke baare mein

Yeh file **jaan-boojh kar toot hui hai**. Woh compile NAHI hogi — yahi iska point hai.

Aapka kaam:
1. Compile karo
2. **Sirf pehla** error padho
3. Fix karo
4. Repeat

Answers file ke neeche hain, par pehle khud try karo. 8 galtiyan hain, 5 alag stages se.

## Try karne wali cheezein

Har example ko chalane ke baad, use **todo**:
- semicolon hatao
- `std::` hatao
- brace hatao
- `\n` ko `endl` se badlo aur timing dekho
- `-Wall -Wextra` hata ke compile karo, fark dekho
- `-fsanitize=address,undefined` ke saath chalao

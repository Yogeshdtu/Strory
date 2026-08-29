# Examples — Folder 01

Is folder ke examples lessons ke andar inline diye gaye hain (bash heredoc ke roop mein),
kyunki Phase 0 mein abhi C++ syntax padhaya nahi gaya hai.

Yahan un examples ki standalone copies hain, taaki aap seedha compile kar sako:

| File | Kaunse lesson se | Kya dikhata hai |
|---|---|---|
| `pipeline_demo.cpp` | 08 | compilation pipeline explore karne ke liye |
| `bits_demo.cpp` | 10 | binary, hex, sizes, overflow |
| `memory_layout.cpp` | 11 | stack/heap/data/text ke addresses |
| `cache_locality.cpp` | 11 | cache-friendly vs unfriendly access ka timing |
| `endianness.cpp` | 10 | apna system little ya big endian |

## Compile karne ka tarika

```bash
g++ -std=c++20 -Wall -Wextra -g pipeline_demo.cpp -o pipeline_demo
./pipeline_demo
```

`cache_locality.cpp` ke liye optimization ON rakhna zaroori hai:
```bash
g++ -std=c++20 -O2 cache_locality.cpp -o cache_locality
./cache_locality
```

# 09 — Compilation pipeline, ab code ke saath

## Prerequisites
Folder 01 lesson 08 (pipeline overview), aur is folder ke lessons 01–08

## Yeh topic abhi kyun
Folder 01 mein humne pipeline **theory** mein dekhi thi. Ab aapke paas asli code hai.
Ab hum wahi pipeline **apne Hello World pe** chalayenge aur har stage ka output
apni aankhon se dekhenge.

Yeh HFT interviews ka favourite topic hai. Aur jab build toote, aapko pata hona
chahiye kahan dekhna hai.

---

## Poora flow, ek nazar mein

```
   hello.cpp                                    [aapka text]
       |
       |  cpp / g++ -E
       v
   hello.i                                      [expanded source, ~30k lines]
       |
       |  cc1plus / g++ -S
       v
   hello.s                                      [assembly, human-readable]
       |
       |  as / g++ -c
       v
   hello.o                                      [object file, binary, adhoora]
       |
       |  ld / g++
       v
   hello                                        [executable, complete]
       |
       |  ./hello -> execve() syscall
       v
   OS loader (+ ld.so dynamic linker)
       |
       v
   Process in RAM
       |
       v
   CPU: fetch -> decode -> execute
       |
       v
   write() syscall -> "Hello World" screen pe
```

---

## Hamara program

```cpp
#include <iostream>

int main() {
    std::cout << "Hello World\n";
    return 0;
}
```

---

## STAGE 1: PREPROCESSOR

```bash
g++ -E hello.cpp -o hello.i
wc -l hello.i
```

**Result:** ~30,000–50,000 lines (compiler version pe depend karta hai)

### Kya hua?

1. `#include <iostream>` ki jagah poora `iostream` header paste ho gaya
2. `iostream` ke andar ke includes bhi paste ho gaye (recursively)
3. Comments hat gaye
4. Macros expand ho gaye
5. Line markers add ho gaye (`# 42 "file.h"`)

### Dekho

```bash
head -20 hello.i         # line markers dikhenge
tail -8 hello.i          # aapka actual code, sabse aakhir mein
grep -c "namespace std" hello.i
```

Aapka 6-line program us 30,000 lines ke **sabse aakhir mein** hai.

### 🔑 Seekhne wali baat
`#include` ka cost real hai. Har `#include <iostream>` compile time badhata hai.
Bade projects mein isliye:
- Sirf zaroori headers include karo
- Forward declarations use karo
- Precompiled headers
- **C++20 modules** (folder 22)

---

## STAGE 2: COMPILER (front-end)

```bash
g++ -S hello.cpp -o hello.s
```

**Ab hum assembly dekh sakte hain.**

```bash
cat hello.s
```

Aapko kuch aisa dikhega (x86-64 AT&T syntax):

```asm
main:
        push    rbp                    ; stack frame setup
        mov     rbp, rsp
        mov     esi, OFFSET FLAT:.LC0  ; string literal ka address
        mov     edi, OFFSET FLAT:_ZSt4cout
        call    _ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc
        mov     eax, 0                 ; return 0
        pop     rbp
        ret
```

### `_ZStlsISt11char_traitsIcEE...` yeh kya hai?

Yeh **name mangling** hai!

C++ mein function overloading hai — `print(int)` aur `print(double)` alag functions
hain. Par linker sirf naam dekhta hai. To compiler har function ke naam mein uske
parameter types encode kar deta hai.

**Demangle karo:**
```bash
c++filt _ZSt4cout
# std::cout

c++filt _ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc
# std::basic_ostream<char, std::char_traits<char> >& 
#     std::operator<<<std::char_traits<char> >(
#         std::basic_ostream<char, std::char_traits<char> >&, char const*)
```

Ab clear hai — yeh `operator<<` hai jo `ostream&` aur `const char*` leta hai.

**Ya poori file demangle karo:**
```bash
g++ -S hello.cpp -o - | c++filt | head -40
```

### Optimization ka asar dekho

```bash
g++ -S -O0 hello.cpp -o hello_O0.s
g++ -S -O2 hello.cpp -o hello_O2.s
wc -l hello_O0.s hello_O2.s
diff hello_O0.s hello_O2.s | head -30
```

Ek behtar demo — ek function ke saath:
```bash
cat > add.cpp << 'END'
int add(int a, int b) {
    int result = a + b;
    return result;
}
END

echo "=== -O0 ==="
g++ -S -O0 add.cpp -o - | grep -A12 "^_Z3addii:"

echo "=== -O2 ==="
g++ -S -O2 add.cpp -o - | grep -A6 "^_Z3addii:"
```

**`-O0`** mein: values stack pe save hoti hain, phir load hoti hain, phir add.
**`-O2`** mein: bas `lea eax, [rdi+rsi]` aur `ret`. Ek instruction!

> **HFT relevance:** Yeh workflow — code likhna, assembly dekhna, verify karna ki
> compiler ne woh kiya jo aap chahte the — HFT engineer ka **roz ka kaam** hai.
> [godbolt.org](https://godbolt.org) isi ke liye bana hai. Folder 33/34 mein poora.

---

## STAGE 3: ASSEMBLER

```bash
g++ -c hello.cpp -o hello.o
file hello.o
```

**Output:** `ELF 64-bit LSB relocatable, x86-64, ...`

**"relocatable"** ka matlab: yeh abhi tak final nahi hai. Addresses fix nahi hue.

### Symbols dekho

```bash
nm -C hello.o
```

Output kuch aisa:
```
0000000000000000 T main                              <- DEFINED (Text section)
                 U std::cout                          <- UNDEFINED
                 U std::ostream::operator<<(...)      <- UNDEFINED
```

| Letter | Matlab |
|---|---|
| `T` | Text (code) section mein defined hai |
| `D` | Data section mein defined |
| `B` | BSS section mein |
| `U` | **Undefined** — linker ko bharna hai |
| `W` | Weak symbol |

**`U` wale symbols hi linker ka kaam hain.**

### Sections dekho
```bash
readelf -S hello.o | grep -E "text|data|rodata|bss"
size hello.o
```

---

## STAGE 4: LINKER

```bash
g++ hello.o -o hello
```

Linker ne kya kiya:
1. `std::cout` ka definition `libstdc++` mein dhoondha
2. `operator<<` ka code jodh diya
3. Startup code (`crt1.o`, `crti.o`) jodh diya
4. Sab addresses fix kiye
5. Executable likha

### Verify karo

```bash
file hello
# ELF 64-bit LSB pie executable, dynamically linked, ...

ldd hello
# linux-vdso.so.1
# libstdc++.so.6 => /lib/x86_64-linux-gnu/libstdc++.so.6
# libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6
# libm.so.6, libgcc_s.so.1

nm -C hello | grep -i " t main"
# 0000000000001189 T main         <- ab address hai!
```

Dhyaan do: `hello.o` mein `main` ka address `0` tha. Ab `0x1189` hai. **Linker ne
address assign kiya.**

### Static vs Dynamic linking

```bash
# Dynamic (default) - libraries runtime pe load hongi
g++ hello.cpp -o hello_dynamic
ls -lh hello_dynamic       # ~16 KB

# Static - sab kuch executable ke andar
g++ -static hello.cpp -o hello_static
ls -lh hello_static        # ~2-3 MB !
ldd hello_static           # "not a dynamic executable"
```

| | Dynamic | Static |
|---|---|---|
| Size | chhota | bada |
| Startup | thoda slow (`.so` load hoti hain) | **fast** |
| Library update | apne aap mil jaata hai | recompile chahiye |
| Deployment | libraries chahiye target pe | ek hi file, bas |
| Symbol resolution | runtime pe (PLT/GOT indirection) | compile time pe |

> **HFT relevance:** HFT systems aksar **statically linked** hote hain. Kyun?
> (1) Startup deterministic hota hai, (2) runtime symbol lookup (PLT indirection)
> ki cost bachti hai, (3) deployment simple hota hai, (4) library version surprises
> nahi hote. Trade-off: bade binaries aur security patches ke liye rebuild.

---

## STAGE 5-6: LOADER aur EXECUTION

```bash
strace -f ./hello 2>&1 | head -30
```

Aapko dikhega:
```
execve("./hello", ["./hello"], 0x...) = 0     <- process shuru
mmap(...)                                     <- memory setup
openat(..., "/lib/.../libstdc++.so.6", ...)   <- dynamic library load
mmap(...)                                     <- library ko memory mein map
...
write(1, "Hello World\n", 12) = 12            <- YAHAN OUTPUT AAYA!
exit_group(0)                                 <- program khatam
```

**`write(1, "Hello World\n", 12)`** — yahi asli syscall hai jo output karta hai.
`1` = stdout ka file descriptor.

`std::cout << "Hello World\n"` aakhir mein bas yahi ek syscall banta hai.

### Syscall count dekho
```bash
strace -c ./hello
```

Yeh dikhata hai kitne syscalls hue aur kitna time laga.

> **HFT relevance:** Har syscall mein user mode se kernel mode mein switch hota hai
> (~100-1000 ns). HFT hot path mein hum syscalls **count karte hain** aur unhe
> minimize karte hain. `strace -c` yeh dekhne ka pehla tool hai. Folder 29 mein.

---

## Complete hands-on script

```bash
cd ~/cpp-practice
mkdir -p pipeline && cd pipeline

cat > hello.cpp << 'END'
#include <iostream>
int main() {
    std::cout << "Hello World\n";
    return 0;
}
END

echo "########## STAGE 1: PREPROCESSOR ##########"
g++ -E hello.cpp -o hello.i
echo "Original: $(wc -l < hello.cpp) lines"
echo "Preprocessed: $(wc -l < hello.i) lines"
echo "Aapka code sabse aakhir mein hai:"
tail -6 hello.i

echo
echo "########## STAGE 2: COMPILER ##########"
g++ -S hello.cpp -o hello.s
echo "Assembly (mangled):"
grep -A8 "^main:" hello.s
echo
echo "Assembly (demangled):"
c++filt < hello.s | grep -A8 "^main:"

echo
echo "########## STAGE 3: ASSEMBLER ##########"
g++ -c hello.cpp -o hello.o
file hello.o
echo "Symbols:"
nm -C hello.o

echo
echo "########## STAGE 4: LINKER ##########"
g++ hello.o -o hello
file hello
echo "Dependencies:"
ldd hello
echo "main ka address ab:"
nm -C hello | grep " T main"

echo
echo "########## SIZES ##########"
ls -lh hello.cpp hello.i hello.s hello.o hello

echo
echo "########## STAGE 5-6: EXECUTION ##########"
./hello
echo "Exit code: $?"
echo
echo "Syscalls:"
strace -c ./hello 2>&1 | tail -15 || echo "(strace installed nahi hai)"
```

**Yeh poora script chalao.** Yeh 5 minute ka hai aur aapko poori pipeline dikha dega.

---

## Error → Stage mapping (yaad kar lo)

| Error message | Stage | Kya karo |
|---|---|---|
| `fatal error: xyz.h: No such file` | 1 Preprocessor | file naam check karo, `-I` path do |
| `error: expected ';' before ...` | 2 Parser | syntax dekho, **upar wali line** dekho |
| `error: 'x' was not declared in this scope` | 2 Semantic | typo? `std::` bhool gaye? header missing? |
| `error: no matching function for call` | 2 Semantic | arguments ke types dekho |
| `error: invalid conversion from X to Y` | 2 Semantic | type mismatch |
| `undefined reference to 'foo()'` | 4 Linker | definition missing, ya `.cpp` link nahi kiya |
| `multiple definition of 'x'` | 4 Linker | ODR violation, header mein definition |
| `cannot find -lxyz` | 4 Linker | library missing, `-L` path do |
| `error while loading shared libraries` | 5 Loader | `.so` nahi mili, `LD_LIBRARY_PATH` dekho |
| `Segmentation fault` | 6 Runtime | memory bug — gdb/ASan use karo |
| Chala par galat answer | 6 Runtime | logic bug — debug karo |

---

## Exercises

1. Poora hands-on script chalao. Har stage ka output samjho.

2. `hello.i` mein apna code dhoondho. Kitni line pe hai?

3. `nm -C hello.o` chalao. Kitne `U` symbols hain? Woh kahan se aayenge?

4. `-static` se compile karo. Size kitna badha? `ldd` kya bolta hai?

5. Ek linker error banao:
   ```cpp
   #include <iostream>
   int mystery();          // declare kiya
   int main() { std::cout << mystery(); }
   ```
   Compile karo `-c` se (kaam karega!) aur phir link karo (fail hoga).
   Yeh proof hai ki error linker se aa raha hai, compiler se nahi.

6. godbolt.org kholo, yeh code daalo, aur `-O0` vs `-O3` compare karo:
   ```cpp
   int sumTo(int n) {
       int total = 0;
       for (int i = 1; i <= n; ++i) total += i;
       return total;
   }
   ```
   <details><summary>Kya hoga</summary>
   `-O0` pe poora loop dikhega.
   `-O3` pe compiler shayad loop hata dega aur formula `n*(n+1)/2` use kar lega!
   Yeh dekh kar aapko andaza lagega ki compiler kitna smart hai.
   </details>

7. `strace -c ./hello` chalao. Sabse zyada kaunsa syscall hua?

---

## Interview questions

1. Compilation ke stages naam se batao aur har ek ka kaam.
2. Name mangling kya hai aur kyun zaroori hai?
3. `undefined reference` kis stage ka error hai? Kaise fix karte hain?
4. Static aur dynamic linking mein trade-offs?
5. Object file mein `U` symbol ka kya matlab hai?
6. `std::cout << "x"` aakhir mein kaunsa syscall banta hai?

---

## Next
→ [`10-your-first-errors.md`](10-your-first-errors.md)

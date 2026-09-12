# 08 — "Run" dabane pe kya hota hai?

## Prerequisites
`05-compiler-vs-interpreter.md`, `07-editor-ide-terminal.md`

## Yeh topic abhi kyun
Yeh folder ka **sabse important lesson** hai. Ek beginner sochta hai "Run dabaya, output
aa gaya, jaadu." Lekin beech mein 8 alag steps hote hain. Har HFT interview mein yeh
poocha jaata hai. Aur jab kuch todega, aapko pata hona chahiye ki **kis step pe toota**.

---

## Poori picture (yeh diagram yaad kar lo)

```
   [ 1 ]  AAP CODE LIKHTE HO
          hello.cpp  (plain text, storage/SSD pe)
                 |
                 |  g++ hello.cpp -o hello
                 v
   [ 2 ]  PREPROCESSOR
          #include expand, #define replace, comments hatao
          Output: hello.i (bada .cpp file)
                 |
                 v
   [ 3 ]  COMPILER (front-end + optimizer)
          C++ parse karo -> check karo -> optimize karo -> assembly banao
          Output: hello.s (assembly text)
                 |
                 v
   [ 4 ]  ASSEMBLER
          assembly -> machine code
          Output: hello.o (object file, binary, adhoora)
                 |
                 v
   [ 5 ]  LINKER
          hello.o + libstdc++ + startup code jodo
          Output: hello (executable file, storage pe)
                 |
                 |  ./hello     <- ab AAP chalate ho
                 v
   [ 6 ]  OS LOADER
          executable ko RAM mein load karo, memory setup karo
                 |
                 v
   [ 7 ]  PROCESS ban gaya
          OS ne memory di, CPU time diya
                 |
                 v
   [ 8 ]  CPU INSTRUCTIONS CHALATI HAI
          fetch -> decode -> execute -> repeat
                 |
                 v
          OUTPUT SCREEN PE
```

Ab har step ko detail mein dekhte hain.

---

## Step 1: Aap code likhte ho

`hello.cpp` — plain text file, SSD pe padi hui. Abhi tak yeh **bas text hai**.
Computer ke liye iska koi matlab nahi.

---

## Step 2: PREPROCESSOR

Preprocessor ek **text-replacement machine** hai. Yeh C++ ko samajhta hi nahi —
yeh sirf text kaat-jod karta hai.

Yeh 3 kaam karta hai:

### a) `#include` ko expand karta hai
```cpp
#include <iostream>
```
Yeh line **hat jaati hai** aur uski jagah poora `iostream` file ka content paste ho
jaata hai. Aur `iostream` ke andar bhi includes hain — wo bhi paste hote hain.
Recursively.

Result: aapki 6-line file ~30,000 lines ki ho jaati hai. **Sach mein.**

### b) `#define` macros replace karta hai
```cpp
#define MAX 100
int arr[MAX];      // -> int arr[100];  (literally text replace)
```

### c) Comments hata deta hai
```cpp
int x = 5;  // yeh comment gayab ho jaata hai
```

Compiler ko comments kabhi dikhte hi nahi.

**Khud dekho:**
```bash
g++ -E hello.cpp -o hello.i
wc -l hello.i          # kitni lines?
```

---

## Step 3: COMPILER

Ab asli kaam. Compiler ke andar bhi kai stages hain:

```
   hello.i
      |
      v
  [3a] LEXER / TOKENIZER
       Text ko tokens mein todo: int | main | ( | ) | { | return | 0 | ; | }
      |
      v
  [3b] PARSER
       Tokens se ek tree banao (AST - Abstract Syntax Tree)
       Grammar check: kya yeh valid C++ hai?
       -> SYNTAX ERRORS yahan aate hain
      |
      v
  [3c] SEMANTIC ANALYSIS
       Types check karo, naam resolve karo, overloads choose karo
       -> TYPE ERRORS yahan aate hain
      |
      v
  [3d] IR GENERATION
       Ek intermediate representation banao (GCC: GIMPLE, Clang: LLVM IR)
      |
      v
  [3e] OPTIMIZER  <- yahan jaadu hota hai
       Dead code hatao, loops unroll karo, functions inline karo,
       constants fold karo, vectorize karo...
      |
      v
  [3f] CODE GENERATION
       Target CPU ke liye assembly banao
      |
      v
   hello.s
```

> **HFT relevance:** Step **3e (optimizer)** hi wo jagah hai jahan aapki performance
> banti ya bigadti hai. `-O2` aur `-O0` mein 10-50x ka fark aa sakta hai.
> Folder 33 poora isi pe hai. Aur folder 34 mein hum step 3f ka output padhna seekhenge.

**Khud dekho:**
```bash
g++ -S hello.cpp -o hello.s
cat hello.s
```

---

## Step 4: ASSEMBLER

Assembly text ko machine code (binary) mein badalta hai.

```asm
mov  eax, 5      ->     10111000 00000101 00000000 00000000
```

Output: **object file** (`hello.o`).

Yeh binary hai, lekin **adhoora**. Kyun? Kyunki isme `std::cout` ka code nahi hai.
Bas ek "khali jagah" hai jisme likha hai: *"yahan `std::cout` wala function call hona
hai — linker, tum bhar dena."*

Inhe **unresolved symbols** kehte hain.

**Khud dekho:**
```bash
g++ -c hello.cpp -o hello.o
file hello.o                    # "ELF 64-bit LSB relocatable"
nm -C hello.o | head -20        # symbols dekho ('U' = undefined)
```

---

## Step 5: LINKER

Linker sabse under-appreciated hero hai. Yeh:

1. Saari `.o` files leta hai
2. Libraries leta hai (`libstdc++` — yahan `std::cout` ka asli code hai)
3. **Startup code** jodta hai (`crt0.o` — jo `main()` ko call karta hai!)
4. Har unresolved symbol ko dhoondhta hai aur jodta hai
5. Final memory addresses assign karta hai
6. Executable file likhta hai

```
   hello.o        libstdc++.so      crt0.o (startup)
      |                |                |
      +----------------+----------------+
                       |
                  [ LINKER ]
                       |
                       v
                    hello  (executable)
```

**Agar koi symbol nahi mila?** → `undefined reference to ...` — classic linker error.

**Khud dekho:**
```bash
g++ hello.o -o hello
ldd hello              # kaunsi shared libraries chahiye?
```

---

### 🔑 Ek chhupa hua sach: `main()` pehla function nahi hota!

Aapko laga `main()` se program shuru hota hai? **Galat.**

Asli sequence:
```
OS -> _start (crt0 se)  ->  __libc_start_main  ->  global objects ke constructors
                                                ->  main()          <- ab aap
                                                ->  global objects ke destructors
                                                ->  exit()
```

`_start` asli entry point hai. Woh:
- stack setup karta hai
- command-line arguments taiyaar karta hai
- global/static variables initialize karta hai
- **phir** `main()` call karta hai
- `main()` ka return value leke `exit()` call karta hai

Yeh baat folder 25 (object model) mein bahut important banegi — "static initialization
order fiasco" isi se aata hai.

---

## Step 6: OS LOADER

Aapne `./hello` type kiya. Ab OS:

1. File padhta hai, check karta hai ki valid executable hai
2. Ek **naya process** banata hai
3. Virtual memory space allocate karta hai
4. Program ka code aur data RAM mein load karta hai
5. Shared libraries load karta hai (dynamic linker `ld.so`)
6. Memory ko sections mein set karta hai:

```
   HIGH ADDRESS
   +---------------------------+
   |  STACK                    |  local variables, function calls
   |  (neeche ki taraf badhta) |
   +---------------------------+
   |         |                 |
   |         v                 |
   |                           |  <- khali jagah
   |         ^                 |
   |         |                 |
   +---------------------------+
   |  HEAP                     |  new/delete se milne wali memory
   |  (upar ki taraf badhta)   |
   +---------------------------+
   |  BSS                      |  global variables (bina initialization ke)
   +---------------------------+
   |  DATA                     |  global variables (initialized)
   +---------------------------+
   |  TEXT (CODE)              |  aapke program ke instructions (read-only)
   +---------------------------+
   LOW ADDRESS
```

**Yeh diagram yaad kar lo.** Folder 14 (Memory) poora isi pe hai. Har C++ interview
mein yeh poocha jaata hai.

7. Instruction pointer ko `_start` pe set karta hai
8. CPU ko de deta hai

---

## Step 7: Process chal raha hai

Ab aapka program ek **process** hai — OS ki nazar mein ek zinda entity.

```bash
./hello &      # background mein chalao
ps aux | grep hello
```

Process ke paas hota hai:
- Ek unique **PID** (Process ID)
- Apni **virtual memory** (doosre processes ise nahi dekh sakte)
- Kam se kam ek **thread**
- File descriptors (0=stdin, 1=stdout, 2=stderr)

---

## Step 8: CPU chalati hai

CPU ek simple loop chalati hai, arabon baar per second:

```
   +----------------------------------------------+
   |                                              |
   v                                              |
[FETCH]  -> memory se agla instruction lao        |
   |                                              |
   v                                              |
[DECODE] -> yeh instruction kya keh raha hai?     |
   |                                              |
   v                                              |
[EXECUTE]-> kaam karo (jodo, compare, jump...)    |
   |                                              |
   v                                              |
[WRITE]  -> result register/memory mein rakho     |
   |                                              |
   +----------------------------------------------+
```

Modern CPUs ismein **bahut** chalaki karti hain — pipelining, out-of-order execution,
speculative execution, branch prediction. Yeh sab folder 31 mein.

Jab `std::cout << "Hello"` chalti hai, aakhir mein ek **syscall** (`write`) hoti hai
jo OS se bolti hai "yeh text screen pe daal do".

---

## Poora flow — ek line mein

> Text file → preprocess (paste) → compile (samajh + optimize) → assemble (binary) →
> link (jodna) → executable file → OS load karta hai → process banta hai → CPU chalati hai →
> syscall se output

---

## Kahan kya toot sakta hai

| Error kaisa dikhta hai | Kis step pe toota | Example |
|---|---|---|
| `No such file or directory: 'iostrem.h'` | **2** Preprocessor | include ka naam galat |
| `expected ';' before ...` | **3b** Parser | syntax galat |
| `'cout' was not declared in this scope` | **3c** Semantic | naam nahi mila |
| `invalid conversion from 'const char*' to 'int'` | **3c** Semantic | type mismatch |
| `undefined reference to 'foo()'` | **5** Linker | function declare kiya, define nahi |
| `multiple definition of 'x'` | **5** Linker | ODR violation |
| `error while loading shared libraries` | **6** Loader | `.so` file nahi mili |
| `Segmentation fault` | **8** Runtime | galat memory access |
| Program chala par galat answer | **8** Runtime | logic bug |

**Yeh table bookmark kar lo.** Error dekhte hi aapko pata chal jayega kahan dekhna hai.

---

## Hands-on: poora pipeline khud chalao

```bash
cd ~/cpp-practice

cat > pipeline.cpp << 'END'
#include <iostream>

#define GREETING "Namaste"

int main() {
    // yeh comment preprocessor hata dega
    std::cout << GREETING << " Duniya\n";
    return 0;
}
END

echo "=== STEP 2: Preprocessor ==="
g++ -E pipeline.cpp -o pipeline.i
echo "Original lines: $(wc -l < pipeline.cpp)"
echo "After preprocessing: $(wc -l < pipeline.i)"
tail -12 pipeline.i          # dekho: comment gayab, GREETING replace ho gaya

echo "=== STEP 3: Compiler ==="
g++ -S pipeline.cpp -o pipeline.s
grep -A5 "main:" pipeline.s | head -10

echo "=== STEP 4: Assembler ==="
g++ -c pipeline.cpp -o pipeline.o
file pipeline.o
nm -C pipeline.o | grep " U " | head -5     # undefined symbols

echo "=== STEP 5: Linker ==="
g++ pipeline.o -o pipeline
ldd pipeline

echo "=== STEP 6-8: Chalao ==="
./pipeline
```

**Yeh zaroor chalao.** 5 minute lagega, aur aapko poori pipeline dikh jayegi.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Compile aur run ek hi cheez hai" | Do bilkul alag steps. Compile ek baar, run kai baar |
| "Program `main()` se shuru hota hai" | `_start` se. `main()` ko woh call karta hai |
| "Linker chhota kaam karta hai" | Linker sabse zyada errors ka source hai bade projects mein |
| "Executable mein poori library hoti hai" | Dynamic linking mein nahi — runtime pe load hoti hai |
| "Preprocessor C++ samajhta hai" | Bilkul nahi. Woh sirf text replace karta hai |

---

## Exercises

1. Upar wala poora hands-on script chalao. Har step ka output dekho.

2. `pipeline.i` file mein `GREETING` search karo. Mila? Kyun nahi mila?
   <details><summary>Answer</summary>
   Nahi milega (define ke alawa) — kyunki preprocessor ne `GREETING` ko `"Namaste"` se
   replace kar diya. Woh ab exist hi nahi karta.
   </details>

3. Yeh code compile karo — kaunsa error aayega aur kis step pe?
   ```cpp
   #include <iostream>
   int foo();               // declare kiya
   int main() {
       std::cout << foo();  // use kiya
       return 0;
   }
   // foo define nahi kiya!
   ```
   <details><summary>Answer</summary>
   `undefined reference to 'foo()'` — **Linker error (Step 5)**.
   Compiler khush tha kyunki declaration mil gayi thi. Linker ko definition nahi mili.
   Yeh sabse common linker error hai.
   </details>

4. `nm -C pipeline.o` chalao. `U` wale symbols kaunse hain? `T` wale?
   <details><summary>Answer</summary>
   `U` = Undefined (linker ko bharna hai) — `std::cout`, `operator<<`, etc.
   `T` = Text section mein defined hai — `main`.
   </details>

5. `g++ -O0 -S` aur `g++ -O2 -S` dono chalao ek loop wale program pe. Assembly kitni
   alag hai?

---

## Interview questions

1. Preprocessor, compiler, assembler, linker — har ek ka kaam batao.
2. `undefined reference` aur `was not declared in this scope` mein kya fark hai?
3. Static linking vs dynamic linking ke trade-offs?
4. `main()` se pehle kya chalta hai?
5. Process ki memory kaise organized hoti hai?

Abhi in sab ka jawab dena mushkil hoga. Course ke end tak aap in sab pe 10 minute
bol sakoge.

---

## Next
→ [`09-program-vs-process-vs-executable.md`](09-program-vs-process-vs-executable.md)

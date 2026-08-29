# 05 — Compiler vs Interpreter

## Prerequisites
`03-what-is-a-programming-language.md`, `04-source-code-and-files.md`

## Yeh topic abhi kyun
Aapne likha: "compiler source code ko machine code mein badalta hai." Ab yeh samajhte
hain ki **exactly kaise**, aur doosra tarika (interpreter) kya hai. Yeh samajh liya to
aapko pata chal jayega ki C++ tez kyun hai, aur Python slow kyun.

---

## Do tareeke: Compile ya Interpret

Duniya mein source code ko chalane ke do main tareeke hain.

### Tareeka 1: COMPILER (C++, C, Rust, Go)

**"Pehle poori kitaab translate karo, phir padho."**

```
   hello.cpp
      |
      | [COMPILER chalao]  <- ek baar, pehle
      v
   hello (executable)
      |
      | [CHALAO]           <- jitni baar chaho
      v
   Output
```

Compiler poori file padhta hai, machine code banata hai, aur ek **executable file** deta hai.
Uske baad aap us executable ko jitni baar chaho chala sakte ho — compiler ki zarurat nahi.

### Tareeka 2: INTERPRETER (Python, JavaScript, Ruby)

**"Ek-ek line padho aur turant translate karke bolo."**

```
   hello.py
      |
      | [INTERPRETER]  <- har baar jab bhi chalao
      v
   Output (line by line)
```

Interpreter file ko line-by-line padhta hai aur turant execute karta hai.
Koi separate executable nahi banti.

---

## Comparison table

| | **Compiled (C++)** | **Interpreted (Python)** |
|---|---|---|
| Translation kab | pehle, ek baar | har run pe, line by line |
| Output | executable file | kuch nahi |
| Speed | **bahut tez** | slow (10–100x) |
| Errors kab pata chalte hain | compile time pe (chalane se pehle) | run time pe (jab us line pe pahunche) |
| Portability | recompile karna padta hai har platform ke liye | same file har jagah chal jaati hai |
| Startup time | instant (already compiled) | interpreter load hota hai |
| Development speed | slow (har change pe compile) | fast (bas chalao) |
| Type errors | compile pe pakde jaate hain | shayad kabhi nahi pakde jaayein |

---

## Ek concrete example

Yeh Python code:
```python
print("Hello")
x = "text" + 5      # <- yeh error hai
```

Output:
```
Hello
TypeError: can only concatenate str (not "int") to str
```

Dekha? Python ne **pehle "Hello" print kar diya**, phir line 2 pe error diya.
Error tab mila jab woh line chali.

Wahi cheez C++ mein:
```cpp
#include <iostream>
int main() {
    std::cout << "Hello\n";
    int x = "text" + 5;    // <- yeh error hai
}
```

Output:
```
error: invalid conversion from 'const char*' to 'int'
```

**Program chala hi nahi.** "Hello" bhi print nahi hua. Compiler ne pehle hi rok diya.

> **Yeh C++ ki sabse badi taakat hai.** Bahut saare bugs **chalane se pehle** pakde
> jaate hain. HFT mein jahan ek bug crores ka nuksaan kar sakta hai, yeh property
> invaluable hai.

---

## C++ actually kya hai?

C++ **compiled** hai. Aur sirf compiled nahi — **AOT compiled** (Ahead-Of-Time).
Matlab machine code pehle hi ban jaata hai, chalane se bahut pehle.

Yeh aapko yeh cheezein deta hai:
- **Predictable performance** — koi surprise nahi ki "aaj slow kyun chal raha hai"
- **No runtime overhead** — koi interpreter background mein nahi chal raha
- **Deterministic** — same input, same time (roughly)

> **HFT relevance:** HFT mein **predictability** speed se bhi zyada important hai.
> Ek system jo hamesha 5 µs leta hai, us system se behtar hai jo kabhi 2 µs kabhi 50 µs
> leta hai. Interpreted languages aur garbage-collected languages (Java, C#) mein
> unpredictable pauses aate hain. Isliye HFT mein C++ hi hai.

---

## Beech ka raasta: JIT (Just-In-Time)

Java aur C# ek hybrid model use karte hain:

```
   Source  ->  Bytecode  ->  [VM chalati hai]  ->  hot code JIT-compile hoti hai
```

Pehle bytecode banta hai (portable), phir chalte waqt VM dekhti hai ki kaunsa code
baar-baar chal raha hai, aur usko **runtime pe** machine code mein compile kar deti hai.

**Fayda:** portable + eventually tez
**Nuksaan:** startup slow, warm-up chahiye, aur **garbage collection pauses**

> **HFT note:** Kuch firms Java use karti hain (LMAX famously), lekin unhe GC ko
> practically disable karna padta hai — zero allocation ke saath likhna padta hai.
> Jab aap Java se GC nikaal dete ho, to aap basically C++ hi likh rahe ho, bas mushkil
> tareeke se. Isliye zyadatar C++ hi chuni jaati hai.

---

## C++ ka toolchain — ek jhalak

Jab aap "compiler" bolte ho, actually 4 alag programs kaam kar rahe hote hain:

```
   hello.cpp
       |
       v
  [1] PREPROCESSOR    -> #include ko expand karta hai, #define replace karta hai
       |                  Output: ek bada .cpp jisme sab kuch paste ho gaya
       v
  [2] COMPILER        -> C++ ko assembly mein badalta hai
       |                  Output: hello.s
       v
  [3] ASSEMBLER       -> assembly ko machine code mein badalta hai
       |                  Output: hello.o (object file)
       v
  [4] LINKER          -> saare .o files + libraries jodta hai
       |                  Output: hello (executable)
       v
   CHALNE KE LIYE TAIYAAR
```

`g++` actually ek **driver** hai jo yeh chaaron chalata hai.

Yeh pipeline agle folder (`02-CPP-FIRST-STEPS`) mein detail mein padhenge, aur folder 24
mein bahut hi detail mein.

---

## Aap yeh khud dekh sakte ho — abhi

```bash
cd ~/cpp-practice
cat > demo.cpp << 'END'
#include <iostream>
int main() {
    std::cout << "Hi\n";
    return 0;
}
END

# Step 1: sirf preprocess karo
g++ -E demo.cpp -o demo.i
wc -l demo.i          # kitni lines? (hazaaron! iostream expand ho gaya)

# Step 2: assembly tak compile karo
g++ -S demo.cpp -o demo.s
head -30 demo.s       # assembly dekho

# Step 3: object file banao
g++ -c demo.cpp -o demo.o
file demo.o           # "ELF 64-bit relocatable" dikhega

# Step 4: link karke executable banao
g++ demo.o -o demo
./demo
```

**Yeh zaroor karo.** Yeh aapka pehla "andar kya ho raha hai" experience hai.

`demo.i` mein hazaaron lines dekhkar hairan mat hona — `#include <iostream>` ne poora
iostream header (aur uske andar ke sab headers) aapki file mein paste kar diya.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Compiler code chalata hai" | Nahi. Compiler executable **banata** hai. Chalana OS karta hai |
| "C++ compile hote hi chal jaata hai" | Nahi, `./program` likhna padta hai |
| "Compiled = hamesha tez" | Galat likha C++ code Python se slow ho sakta hai |
| "g++ ek single program hai" | g++ ek driver hai jo preprocessor, compiler, assembler, linker chalata hai |
| "Python compile nahi hoti" | Actually `.pyc` bytecode banti hai. Par machine code nahi |

---

## Exercises

1. Upar wale 4 steps (`-E`, `-S`, `-c`, link) khud chalao. Har output file ka size dekho:
   ```bash
   ls -lh demo.cpp demo.i demo.s demo.o demo
   ```
   Kya order hai sizes ka? Kya surprise hai?
   <details><summary>Answer</summary>
   `demo.cpp` chhoti (~80 bytes), `demo.i` **bahut badi** (~1 MB!) kyunki iostream expand
   ho gaya, `demo.s` medium, `demo.o` chhoti, `demo` (executable) ~16 KB.
   Yeh dikhata hai ki `#include` kitna mehnga hai — folder 24 mein compile-time
   optimization padhenge.
   </details>

2. `demo.s` file kholo aur dhoondho: kya aapko `main` naam kahin dikha?

3. C++ ko interpreted banaya ja sakta hai kya? (Hint: `cling` search karo)
   <details><summary>Answer</summary>
   Haan! `cling` (CERN se) ek C++ interpreter hai. Aur C++ compile bhi ho sakta hai aur
   interpret bhi. "Compiled language" actually ek *implementation* choice hai,
   language ki property nahi. Bas C++ ko practically hamesha compile hi kiya jaata hai.
   </details>

4. HFT mein Python kyun nahi use hoti trading ke hot path mein? 3 wajah likho.
   <details><summary>Answer</summary>
   (1) 10-100x slow (interpreted), (2) GIL ke kaaran true parallelism nahi,
   (3) Garbage collection se unpredictable pauses, (4) Memory layout pe control nahi
   (cache-unfriendly), (5) Type errors runtime pe pakde jaate hain.
   *Note:* Python HFT mein research/backtesting/analytics ke liye khoob use hoti hai —
   bas hot path mein nahi.
   </details>

---

## Next
→ [`06-what-is-cpp.md`](06-what-is-cpp.md)

# 04 — `main()` function

## Prerequisites
`02-anatomy-line-by-line.md`

## Yeh topic abhi kyun
`main()` har program ka dil hai. Par isme kuch special rules hain jo baaki functions
pe apply nahi hote. Yeh clear kar lo, taaki aage confusion na ho.

---

## `main` special kyun hai?

`main` **akela function** hai jo aapke program mein **hona hi chahiye**.

Aur yeh **akela function** hai jise **OS (indirectly) call karta hai**, aap nahi.

```
   OS  ->  _start  ->  __libc_start_main  ->  main()  <- yahan aapka code shuru
```

---

## Do valid signatures

Standard sirf yeh do forms guarantee karta hai:

```cpp
int main() { ... }
int main(int argc, char* argv[]) { ... }
```

Doosra form command-line arguments ke liye hai (folder 08 mein detail).

### Kuch platforms pe teesra form bhi chalta hai
```cpp
int main(int argc, char* argv[], char* envp[]) { ... }    // environment variables
```
Yeh standard mein nahi hai, par Linux/Unix pe kaam karta hai.

---

## ❌ Jo GALAT hai

```cpp
void main() { }          // ❌ standard ke khilaaf
                         // (MSVC allow karta hai, GCC/Clang warning dete hain)

main() { }               // ❌ C89 mein chalta tha (implicit int), C++ mein nahi

int Main() { }           // ❌ 'M' capital hai - yeh alag function hai
                         // Linker error: undefined reference to `main'
```

**Test karo:**
```bash
cat > wrong.cpp << 'END'
#include <iostream>
void main() { std::cout << "hi\n"; }
END
g++ -std=c++20 -Wall wrong.cpp -o wrong
```
Error: `'::main' must return 'int'`

---

## `main` ke 4 special rules

### Rule 1: `return` optional hai

```cpp
int main() {
    std::cout << "Hi\n";
    // koi return nahi!
}       // <- compiler yahan apne aap `return 0;` daal deta hai
```

**Sirf `main` mein.** Baaki non-void functions mein `return` chhodna UB hai:

```cpp
int getValue() {
    // koi return nahi
}       // ⚠️ UNDEFINED BEHAVIOUR!
```

Test karo:
```cpp
#include <iostream>
int getValue() { }              // warning: no return statement
int main() {
    std::cout << getValue();    // garbage value ya crash
}
```

### Rule 2: Aap `main()` ko call nahi kar sakte

```cpp
int main() {
    main();     // ⚠️ UNDEFINED BEHAVIOUR (C++ mein)
}
```

C mein yeh legal hai, C++ mein nahi. Practically yeh infinite recursion se stack
overflow karega.

### Rule 3: `main` ka address nahi le sakte

```cpp
auto p = &main;     // ⚠️ technically UB
```
(Practically compilers allow karte hain — humne folder 01 ke example mein kiya bhi tha.)

### Rule 4: `main` `inline`, `static`, `constexpr` nahi ho sakta

```cpp
static int main() { }        // ❌
constexpr int main() { }     // ❌
```

---

## Return value ka matlab

Yeh number **OS ko** jaata hai. Isko **exit code** ya **exit status** kehte hain.

```cpp
int main() {
    return 0;      // success
}
```

### Convention

| Value | Matlab |
|---|---|
| `0` | Success |
| `1` | General error |
| `2` | Misuse of shell command |
| `126` | Command found but not executable |
| `127` | Command not found |
| `128 + N` | Signal N se maara gaya |
| `130` | Ctrl+C (SIGINT = 2, so 128+2) |
| `139` | Segfault (SIGSEGV = 11, so 128+11) |

### Portable constants
```cpp
#include <cstdlib>

int main() {
    if (somethingFailed) {
        return EXIT_FAILURE;    // portable "fail"
    }
    return EXIT_SUCCESS;        // portable "success" (hamesha 0)
}
```

### Check karo
```bash
./program
echo $?          # Linux/Mac
echo %ERRORLEVEL%  # Windows cmd
```

### Yeh matter kyun karta hai?

Scripts aur build systems exit code se decide karte hain ki aage badhein ya nahi:

```bash
./run_tests && ./deploy          # deploy sirf tab jab tests pass hon (exit 0)
./build || echo "Build fail!"    # error message sirf tab jab build fail ho
```

> **HFT relevance:** Trading systems mein exit codes se monitoring systems ko pata
> chalta hai ki process crash hua ya cleanly band hua. Ek galat exit code se alert
> nahi bajega, aur aapka system down rahega. Yeh chhoti cheez production mein badi hai.

---

## Program band karne ke tareeke

```cpp
#include <cstdlib>

int main() {
    return 0;              // ✅ normal — destructors chalte hain, cleanup hota hai
}
```

| Tareeka | Destructors chalte hain? | Buffers flush hote hain? | Kab use karein |
|---|---|---|---|
| `return 0;` | ✅ haan | ✅ haan | normal exit |
| `std::exit(0)` | ⚠️ locals ke nahi, globals ke haan | ✅ haan | deep function se exit |
| `std::quick_exit(0)` | ❌ nahi | ❌ nahi | fast shutdown |
| `std::abort()` | ❌ nahi | ❌ nahi | crash, core dump |
| `std::terminate()` | ❌ nahi | ❌ nahi | unhandled exception |

**⚠️ Warning:** `std::exit()` **local objects ke destructors nahi chalata**. Agar
aapke paas RAII objects hain (file handles, locks, network connections), woh clean
nahi honge. Isliye `return` best hai.

```cpp
void deepFunction() {
    std::ofstream file("data.txt");
    file << "important data";
    std::exit(0);      // ⚠️ file ka destructor nahi chalega!
                       // Data buffer mein reh sakta hai, file corrupt ho sakti hai
}
```

---

## Global objects aur `main`

```cpp
#include <iostream>

struct Logger {
    Logger()  { std::cout << "Logger start (main se PEHLE)\n"; }
    ~Logger() { std::cout << "Logger end (main ke BAAD)\n"; }
};

Logger globalLogger;      // global object

int main() {
    std::cout << "main() chal raha hai\n";
    return 0;
}
```

**Output:**
```
Logger start (main se PEHLE)
main() chal raha hai
Logger end (main ke BAAD)
```

Dekha? Global objects ke constructors `main()` se **pehle** chalte hain, aur
destructors `main()` ke **baad**.

> Yeh baat folder 25 mein bahut important banegi — "static initialization order fiasco"
> isi se aata hai. Do alag files ke global objects kis order mein banenge, yeh
> **undefined** hai. Yeh real bugs ka source hai.

---

## Hands-on

```bash
cd ~/cpp-practice
cat > mainDemo.cpp << 'END'
#include <iostream>
#include <cstdlib>

struct Tracer {
    const char* name;
    Tracer(const char* n) : name(n) { std::cout << "[+] " << name << " bana\n"; }
    ~Tracer()                        { std::cout << "[-] " << name << " gaya\n"; }
};

Tracer globalTracer("GLOBAL");

int main() {
    std::cout << ">>> main() shuru\n";
    Tracer localTracer("LOCAL");
    std::cout << ">>> main() khatam\n";
    return 42;
}
END
g++ -std=c++20 -Wall mainDemo.cpp -o mainDemo
./mainDemo
echo "Exit code: $?"
```

Output ka order dhyaan se dekho.

**Ab `return 42;` ko `std::exit(42);` se badlo aur dobara chalao.** Kya fark aaya?
<details><summary>Answer</summary>
`LOCAL gaya` **print nahi hoga**. `std::exit()` local objects ke destructors skip
kar deta hai. Global ka chal jayega. Yeh ek real bug source hai.
</details>

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`void main()` chalta hai" | Standard ke khilaaf. Kuch compilers allow karte hain |
| "`return 0;` zaroori hai" | `main` mein optional. Baaki functions mein zaroori |
| "`main` sabse pehle chalta hai" | Global constructors pehle chalte hain |
| "`exit()` aur `return` same hain" | `exit()` local destructors skip karta hai |
| "Exit code se koi farak nahi padta" | Scripts, CI/CD, monitoring — sab isse depend karte hain |

---

## Exercises

1. `void main()` likh ke compile karo. Kya error aaya?

2. Ek program likho jo `return 7;` kare. `echo $?` se verify karo.

3. `EXIT_SUCCESS` aur `EXIT_FAILURE` ki values print karo:
   ```cpp
   #include <cstdlib>
   #include <iostream>
   int main() {
       std::cout << "SUCCESS = " << EXIT_SUCCESS << "\n";
       std::cout << "FAILURE = " << EXIT_FAILURE << "\n";
   }
   ```

4. Upar wala `mainDemo.cpp` chalao. Order note karo. Phir `exit()` version chalao.

5. Ek non-`main` function banao jisme `return` na ho, aur `-Wall` ke saath compile karo:
   ```cpp
   int broken() { }
   int main() { return broken(); }
   ```
   Kya warning aayi? Chalane pe kya hua?

6. Ek shell script likho jo aapke program ka exit code check kare:
   ```bash
   ./myprogram
   if [ $? -eq 0 ]; then echo "Success!"; else echo "Failed!"; fi
   ```

---

## Interview questions

1. `int main()` aur `void main()` mein kya fark? Kaunsa sahi hai?
2. `main()` ka return value kahan jaata hai? Kaun use karta hai?
3. `main()` se pehle koi code chal sakta hai?
4. `exit()` aur `return` mein kya fark hai `main` ke andar?
5. `main()` ko recursively call kar sakte ho?

---

## Next
→ [`05-statements-and-semicolons.md`](05-statements-and-semicolons.md)

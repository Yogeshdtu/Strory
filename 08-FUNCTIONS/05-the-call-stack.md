# 05 — The call stack — deep dive

## Prerequisites
- [`03-parameters-and-arguments.md`](03-parameters-and-arguments.md), [`04-return-values.md`](04-return-values.md)
- `01-PROGRAMMING-BASICS/11-memory-basics.md` (RAM, addresses)
- `03-VARIABLES-DATA-TYPES/05-int-deep-dive.md` (bytes, layout)
- `05-OPERATORS/09-precedence-associativity.md` mein `-S` assembly dekha tha

## Yeh topic abhi kyun
Yeh folder ka **sabse important lesson** hai. Har function call ek **stack frame**
banati hai — ek chhota memory block jismein us call ke parameters, local
variables, aur "wapas kahan jaana hai" (return address) hote hain. Yeh samajhna:

- **Pointers** (folder 12) — locals ke addresses, dangling pointers
- **Recursion** (lesson 09) — har call apna frame
- **Stack overflow** — frames khatam
- **Debugging** — `gdb` ka `backtrace` yahi stack hai
- **Move semantics, RAII** (folders 17–18) — destructors kab chalte hain
- **Calling conventions, ABI** (folder 25), **exceptions** (folder 23)

...sab isi pe khade hain.

---

## Process memory — stack kahan hai

```
   high address
   ┌──────────────────┐
   │   command-line   │
   │   args, env      │
   ├──────────────────┤
   │      STACK        │  ← function calls yahan;  NEECHE ki taraf badhta hai
   │        │          │
   │        ▼          │
   │                   │
   │        ▲          │
   │        │          │
   │      HEAP          │  ← new / malloc;  UPAR ki taraf badhta hai
   ├──────────────────┤
   │   BSS  (uninit globals)   │
   ├──────────────────┤
   │   DATA (init globals)     │
   ├──────────────────┤
   │   TEXT (code, read-only)  │
   └──────────────────┘
   low address
```

**Stack** ek contiguous memory region hai (Linux default ~8 MB, Windows ~1 MB).
Ek **stack pointer** register (`rsp` x86-64) hamesha stack ke "current top" ko
point karta hai. Stack **neeche** badhta hai — har naya frame **kam** address pe.

(Poora process memory layout folder 14 mein.)

---

## Stack frame mein kya hota hai

Jab `caller()` `callee(a, b)` ko call karta hai, `callee` ka frame roughly:

```
   higher addr
   ┌─────────────────────────┐
   │  caller ke arguments     │  (jo registers mein fit na huye)
   ├─────────────────────────┤
   │  RETURN ADDRESS          │  ← `call` ne push kiya: "callee khatam ho to yahan wapas"
   ├─────────────────────────┤
   │  saved caller's rbp      │  ← callee ke prologue ne push kiya
   ├─────────────────────────┤ ← rbp (frame base) yahan point karta hai
   │  callee ke LOCAL vars    │
   │  saved registers         │
   │  temporaries             │
   ├─────────────────────────┤ ← rsp (stack top) yahan
   lower addr
```

Har frame:
- **Return address** — `callee` khatam hone pe CPU kahan jump kare
- **Saved frame pointer** (`rbp`) — pichle frame ka base (backtrace isi chain se banti hai)
- **Local variables** — is call ke, is call ke liye
- **Saved registers, spill slots, temporaries**

Frame ka size compile-time pe fix hota hai (locals kitni jagah leti hain).
`examples/02_call_stack_trace.cpp` mein measured: chhoti recursion ka frame
**~96 bytes**, 4 KB buffer wala frame **~4 KB bada**.

---

## Prologue aur epilogue — real assembly

```cpp
int helper(int x, int y) {
    int local = x * y;
    return local + 7;
}
int caller(int n) {
    int arr[4] = { n, n+1, n+2, n+3 };
    return helper(arr[0], arr[3]);
}
```

`g++ -O0 -S -masm=intel` (GCC 15, **Windows x64**) — `caller`:

```asm
caller(int):
        push    rbp                 ; ── PROLOGUE ──
        mov     rbp, rsp            ;   naya frame base = current stack top
        and     rsp, -16            ;   16-byte align (ABI requirement)
        sub     rsp, 48             ;   48 bytes locals ke liye reserve (arr[4] + spill)
        mov     DWORD PTR 16[rbp], ecx   ; n (arg, ecx mein aaya) ko frame mein save

        mov     eax, DWORD PTR 16[rbp]   ; arr[0] = n
        mov     DWORD PTR 32[rsp], eax
        ... (arr[1..3] set) ...
        mov     edx, DWORD PTR 44[rsp]   ; helper ka 2nd arg  (arr[3])
        mov     eax, DWORD PTR 32[rsp]   ; helper ka 1st arg  (arr[0])
        mov     ecx, eax
        call    helper(int, int)         ; ── return address push + jump ──

        leave                       ; ── EPILOGUE ── (mov rsp, rbp ; pop rbp)
        ret                         ;   return address pe wapas jump
```

- **Prologue** (`push rbp; mov rbp, rsp; sub rsp, N`): naya frame set karo,
  locals ke liye `N` bytes reserve.
- **`call helper`**: return address (agli instruction ka pata) stack pe push,
  phir `helper` ke code pe jump.
- **Epilogue** (`leave; ret`): frame gira do (`rsp` wapas), return address pop
  karke wahan jump.

### ⚠️ Calling convention — arg pehle register mein

Dekha `n` `ecx` mein aaya? **Windows x64**: pehle 4 integer args `rcx, rdx, r8,
r9`. **System V (Linux/Mac)**: `rdi, rsi, rdx, rcx, r8, r9`. Baaki args stack pe.
Return value `rax` mein. Yeh **ABI** hai (folder 25) — isi liye alag OS ke
compiled binaries directly compatible nahi.

### `-O2` — sab gayab

Wahi code, `g++ -O2 -S`:

```asm
caller(int):
        lea     eax, 3[rcx]
        imul    eax, ecx
        add     eax, 7
        ret
```

`helper` **inline** ho gaya, `arr` compile-time constant fold ho gaya, **koi
frame nahi, koi `call` nahi**. `caller(n)` = `(n+3)*n + 7`. Abstraction ka
runtime cost **zero** (lesson 10).

---

## Call → return, step by step

```cpp
int square(int x) { return x * x; }         // (A)
int main() {
    int r = square(5);                       // (B)
    std::cout << r;                          // (C)
}
```

1. `main` running. `rsp` `main` ke frame ke top pe.
2. `(B)`: `5` ko `ecx`/`edi` mein rakho. `call square` → **return address** (`(C)`
   ka pata) stack pe push, `square` pe jump.
3. `square` prologue: `x` (=5) frame mein.
4. `x * x` = 25 → `eax` mein.
5. `square` epilogue: frame gira, `ret` → stack se return address pop → `(C)` pe jump.
6. `main` mein `r = eax` = 25.

`square` ka frame **ab exist nahi karta** — us memory ko agli call reuse karegi.
Isi liye `square` ke local ka address return karna UB hai (lesson 04, 06).

---

## Recursion — har call ka apna frame

```cpp
std::uint64_t fact(int n) {
    if (n <= 1) return 1;
    return n * fact(n - 1);
}
fact(4);
```

```
   fact(4)  frame:  n=4   →  needs fact(3)
     fact(3)  frame:  n=3   →  needs fact(2)
       fact(2)  frame:  n=2   →  needs fact(1)
         fact(1)  frame:  n=1   →  return 1        ← base case, unwinding shuru
       fact(2):  return 2 * 1 = 2                   ← frame pop
     fact(3):  return 3 * 2 = 6                     ← frame pop
   fact(4):  return 4 * 6 = 24                      ← frame pop
```

**4 frames ek saath** stack pe (peak). Har `return` pe ek frame pop. `fact(4)` ke
`n=4` aur `fact(3)` ke `n=3` alag-alag memory locations hain — isi liye recursion
kaam karta hai.

`examples/02_call_stack_trace.cpp` — recursion ke har level pe local ka address
print karta hai; consecutive frames ka address-diff ≈ frame size.

---

## Stack OVERFLOW — frames khatam

Stack ka size limited hai. In se overflow:

- **Bina base case ke recursion** — infinite frames
- **Bahut deep recursion** — `fact(1'000'000)` (1M frames)
- **Bahut bada local** — `int huge[10'000'000];` (40 MB > 8 MB stack) ek hi frame mein
- **Deep recursion + bada local** — dono

```cpp
void goDeeper(int d) {
    char buf[1024];
    buf[0] = 0;
    goDeeper(d + 1);       // no base case
}
```

`examples/05_stack_overflow.cpp` — yeh **jaan-boojh kar crash** hoti hai. Result:
- **Linux/Mac**: `Segmentation fault` (SIGSEGV), exit code 139
- **Windows**: exit code `0xC00000FD` (STATUS_STACK_OVERFLOW)
- **Koi exception nahi, koi clean message nahi** — OS process ko maar deta hai
- `gdb` mein `backtrace` = hazaaron identical frames

### Bachne ke tareeke
- Recursion ko **iteration** mein badlo (loop + apna `std::stack<T>` heap pe)
- Base case theek karo, depth bound karo
- Bade buffers **heap** pe (`std::vector`, `new`) — stack pe nahi
- Thread ko bada stack do (`pthread_attr_setstacksize`, linker `/STACK`)

### Stack size
```bash
ulimit -s              # Linux: 8192 (KB)
```
Windows: linker `/STACK:reserve` (default 1 MB).

---

## Stack vs heap — quick contrast (folder 14 mein poora)

| | Stack | Heap |
|---|---|---|
| Allocate | `rsp` move (1 instruction) — **super fast** | `new`/`malloc` — bookkeeping, slow |
| Free | frame pop (automatic, function end) | `delete`/`free` (manual) ya smart pointer |
| Size | fixed, chhota (~1–8 MB) | bada (GBs) |
| Lifetime | function scope | aap decide karo |
| Fragmentation | nahi | ho sakti hai |
| Locality | excellent (contiguous, cache-hot) | scattered |

**Stack allocation lagbhag free hai** — isi liye chhote locals ko `new` mat karo.

---

## Andar kya hota hai — `gdb` se dekho

```bash
g++ -std=c++20 -O0 -g prog.cpp -o prog
gdb ./prog
(gdb) break square
(gdb) run
(gdb) backtrace          # frame chain: square -> main
(gdb) info frame         # is frame ka rbp, rsp, return addr
(gdb) info locals        # is frame ke local variables
(gdb) frame 1            # caller ke frame pe jao
(gdb) print $rsp         # stack pointer
```

Folder 45 (debugging) mein poora.

> **HFT relevance:**
> - **Stack allocation free hai** → hot path mein `new` avoid, fixed-size buffers
>   stack pe (par overflow ka dhyaan — deep call trees + bade buffers).
> - **Inlining** call overhead + frame setup poora hata deta hai (~2 ns/call
>   bachta hai per call — lesson 10 measured). Hot functions inline honi chahiye.
> - **Recursion** hot path mein avoid — unpredictable depth = unpredictable stack
>   use + latency. Iteration + preallocated buffers.
> - **Frame layout aur ABI** samajhna — assembly padhne (folder 34), profiler
>   ke output samajhne (folder 35), aur crash dumps (folder 45) ke liye zaroori.
> - **`alloca` / VLAs** (runtime-size stack alloc) — HFT mein kabhi-kabhi, par
>   overflow risk; aksar arena allocator behtar (folder 36).

---

## Hands-on

```bash
# frames ko dekho
./build.ps1 08-FUNCTIONS/examples/02_call_stack_trace.cpp

# prologue/epilogue assembly
./build.ps1 asm 08-FUNCTIONS/examples/06_inline_asm_check.cpp

# stack overflow -- ⚠️ yeh CRASH karega (expected)
g++ -std=c++20 -O0 -g 08-FUNCTIONS/examples/05_stack_overflow.cpp -o so && ./so
echo $?          # non-zero (139 Linux / big negative Windows)
```

---

## ⚠️ Traps

### Trap 1 — local ka address return / store karna
```cpp
int* f() { int x = 42; return &x; }   // ⚠️ frame gayab -> dangling pointer -> UB
```

### Trap 2 — bada array stack pe
```cpp
void f() { double m[2000][2000]; }    // ⚠️ 32 MB > stack -> overflow. Heap: std::vector
```

### Trap 3 — deep recursion "elegant" samajhna
```cpp
long long sum(const int* a, int n) {
    return n == 0 ? 0 : a[0] + sum(a + 1, n - 1);   // ⚠️ n = 10^6 -> overflow. Loop use karo
}
```

### Trap 4 — stack overflow ko catch karne ki koshish
```cpp
try { deepRecursion(); } catch (...) { }   // ⚠️ stack overflow exception nahi hai -- SIGSEGV, catch nahi hoga
```

### Trap 5 — `-O0` assembly ko "aisa hi chalta hai" samajhna
`-O0` har local ko stack pe rakhta hai (debugging ke liye). `-O2` aadhe registers
mein, frame chhota / gayab. Production `-O2` hai.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Stack upar badhta hai" | x86 pe **neeche** — naye frame kam address pe |
| "Recursion ke sab calls ek variable share karte hain" | Har call ka apna frame, apne locals |
| "Stack overflow ek exception hai" | OS signal (SIGSEGV / STATUS_STACK_OVERFLOW) — catch nahi hota |
| "Local return by pointer se copy bachti hai" | Frame gayab → dangling → UB |
| "Function call free hai" | Prologue/epilogue + `call`/`ret` + ABI setup; inline se gayab |
| "Bada array stack pe theek hai" | > kuchh KB → heap. Stack chhota (1–8 MB) |

---

## Exercises

1. **Frames dekho:** `examples/02_call_stack_trace.cpp` chalao. Nested calls ke
   addresses ghat rahe hain? Recursion ka frame size kitna?

2. **Prologue/epilogue:** yeh do functions ek file mein, `g++ -O0 -S -masm=intel`
   se assembly —
   ```cpp
   int leaf(int a) { return a + 1; }
   int node(int a, int b) { int t[3] = {a, b, a+b}; return leaf(t[2]); }
   ```
   `node` ka `push rbp` / `sub rsp` / `call` / `leave` `ret` identify karo. Phir
   `-O2` pe — kya bacha?

3. **Crash it:** `examples/05_stack_overflow.cpp` chalao. Kitni depth tak pahuncha
   (aapke OS pe)? Exit code kya? Ab base case add karke fix karo.

4. **Big local overflow:** `void f() { int big[3'000'000]; big[0] = 1; std::cout
   << big[0]; }` — chalao. Crash? Ab `std::vector<int> big(3'000'000);` se —
   chalta hai?

5. **Recursion → iteration:** `sumRec(const int* a, int n)` (recursive) ko loop
   version `sumIter` mein badlo. `n = 5'000'000` pe dono — kaunsa crash, kaunsa
   theek?

6. **gdb backtrace:** koi bhi 3-level call chain `-g` ke saath compile, `gdb`
   mein deepest function pe breakpoint, `backtrace` / `info locals` / `frame N`
   try karo.

7. **Frame size measure:** ek recursive function jismein alag-alag size ke locals
   hon (`char buf[N]`), `&local` ka address print karke frame-diff se `N` ka asar
   dekho.

---

## Interview questions

1. Stack frame mein kya-kya hota hai (4 cheezein)?
2. Prologue aur epilogue kya karte hain? `call`/`ret` kya karte hain?
3. Stack kis direction mein badhta hai? Return address kahan?
4. Calling convention kya hai? Windows x64 vs System V — pehla arg kahan?
5. Recursion ke `n` levels pe stack pe kitne frames? Unwinding kab?
6. Stack overflow kis-kis se hota hai (3)? Crash kaisa dikhta hai — exception?
7. Local variable ka address return karna kyun UB?
8. Stack vs heap allocation — speed, lifetime, size?
9. `-O0` aur `-O2` ka assembly itna alag kyun (frame ke context mein)?

---

## Next
→ [`06-scope-and-lifetime.md`](06-scope-and-lifetime.md)

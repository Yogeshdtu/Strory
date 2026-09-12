# 03 — References as parameters (pass by reference)

## Prerequisites
- [`01-what-is-a-reference.md`](01-what-is-a-reference.md), [`02-reference-vs-pointer.md`](02-reference-vs-pointer.md)
- Folder 08 (functions, parameters, call stack)

## Yeh topic abhi kyun
Reference ka **#1 use** yahi hai: function parameters. Do maqsad —
(1) function ko caller ka object **badalne** dena, (2) bade objects ko **bina
copy** ke bhejna. Yeh har C++ codebase mein har jagah hai.

---

## 3 tareeke — recap

```cpp
void byValue(int n)  { n += 1; }   // n = caller ke arg ki COPY. Caller safe (unchanged)
void byPtr  (int* n) { *n += 1; }  // caller ka object badla. Call: byPtr(&x). null possible
void byRef  (int& n) { n += 1; }   // caller ka object badla. Call: byRef(x).  null nahi
```

```cpp
int x = 5;
byValue(x);   // x still 5
byPtr(&x);    // x = 6
byRef(x);     // x = 7
```

Reference = "pointer jitna powerful (mutate kar sakta hai), copy jitna clean
(syntax normal)".

---

## Use 1 — output / in-out parameters

Function ek se **zyada cheezein "return"** karni ho:

```cpp
void divmod(int a, int b, int& quot, int& rem) {
    quot = a / b;
    rem  = a % b;
}

int q, r;
divmod(17, 5, q, r);      // q = 3, r = 2
```

Ya ek object ko **jagah pe update** karna:

```cpp
void normalize(std::vector<double>& v) {
    double s = 0;
    for (double e : v) s += e * e;
    double n = std::sqrt(s);
    for (double& e : v) e /= n;      // in-place -- koi naya vector nahi
}
```

> Modern style: agar function "ek nayi cheez banata hai", to **return by value**
> karo (RVO se free — folder 08). Output-param sirf tab jab (a) multiple outputs,
> (b) existing buffer reuse karna hai, (c) return type awkward ho.

---

## Use 2 — bade objects bina copy (read-only) → `const T&`

```cpp
long long sum(const std::vector<int>& v) {   // vector COPY nahi hua -- sirf address gaya
    long long s = 0;
    for (int e : v) s += e;
    return s;
}
```

`const` ka matlab: function `v` ko **padh sakta hai, badal nahi sakta**. Caller ko
guarantee. Yeh **default parameter type** hai har us type ke liye jo copy karna
mehnga ho (`std::string`, `std::vector`, bade `struct`). Poori detail file 04.

Chhote types (`int`, `double`, `char`, ek pointer) → **by value** hi rakho — woh
ek register mein aa jaate hain, `const T&` ulta ek indirection add karta hai.

---

## `T&` vs `const T&` vs `T` — chai-patti rule

| Param | Kab | Caller ka object |
|---|---|---|
| `T` (by value) | chhota (≤ 2 words), ya function ko apni copy chahiye hi | safe (copy) |
| `const T&` | bada, sirf padhna | safe (read-only alias) |
| `T&` | function ko caller ka object **badalna** hai | modify hota hai |
| `T*` | upar wala, par "kuch nahi" bhi valid ho | modify / null |

```cpp
void f(int v);                 // chhota, read -> by value
void g(const std::string& s);  // bada, read -> const&
void h(std::string& s);        // bada, modify -> &
void k(BigThing* p = nullptr); // optional -> pointer
```

---

## Overload resolution + references

```cpp
void p(int&);          // #1
void p(const int&);    // #2

int a = 5;
const int b = 5;
p(a);      // #1 -- non-const lvalue non-const ref ko prefer karta hai
p(b);      // #2 -- const object sirf const& se bind
p(10);     // #2 -- literal (rvalue) non-const T& se bind nahi hota
```

Aur agar `void p(int&&)` (file 08) bhi ho, `p(10)` usse jaayega.

---

## Andar kya hota hai

- **`byRef(x)`** → caller `lea rdi, [x]` (x ka address), `call byRef`. Callee
  `add dword [rdi], 1`. Yani **`byPtr(&x)` jaisa hi machine code** — sirf source
  mein `*`/`&` nahi dikhta.
- **`sum(vec)`** → sirf ek pointer (8 bytes, ek register) pass. Agar by-value
  hota (`sum(std::vector<int> v)`), har call pe `operator new` + `memcpy(size)` +
  `operator delete` — [`examples/03_const_ref_performance.cpp`](examples/03_const_ref_performance.cpp)
  mein yeh **~190x** measure hua (524 ns/call vs 2.7 ns/call, 4096-int vector).
- **Inlining ka effect:** agar function inline ho jaaye aur compiler poora dekh
  raha ho, to woh by-value copy bhi **elide** kar sakta hai. Isliye us benchmark
  mein `__attribute__((noinline))` lagaya — real-world "function doosri TU mein"
  case model karne ke liye.

> **HFT relevance:** message handlers, book updaters, strategy callbacks — sab
> `const Msg&` / `Book&` lete hain. Ek `Quote` struct ko by-value pass karna
> (agar woh 64+ bytes ho) matlab har event pe ek copy — nanoseconds jud'te hain,
> aur cache mein extra traffic. `const T&` se woh copy zero ho jaati hai. Chhote
> POD (`Price`, `Qty` = ek `int64`) by value hi — register mein, indirection se
> sasta.

---

## Hands-on

```bash
./build.ps1 13-REFERENCES/examples/02_pass_by_reference.cpp
./build.ps1 fast 13-REFERENCES/examples/03_const_ref_performance.cpp   # -O2 benchmark
```

`02` — by value / ptr / ref / output-param / swap. `03` — copy vs `const&` ka
measured farq.

---

## ⚠️ Traps

### Trap 1 — bada object by value (chup-chaap copy)
```cpp
void process(std::vector<int> data);   // ⚠️ har call pe poora copy. const& karo
```

### Trap 2 — `const&` ko temporary ke saath, phir usse cheez nikaal ke rakhna
```cpp
const std::string& r = getName();   // getName() by value -> temporary, r lifetime-extend (ok)
const char* c = getName().c_str();  // ⚠️ temporary is line ke baad marta -> c dangling (file 05, 09)
```

### Trap 3 — non-const `T&` param → caller ko lvalue dena padta hai
```cpp
void f(std::string& s);
f("hello");        // ❌ ERROR -- "hello" temporary; non-const ref bind nahi
f(std::string{});  // ❌ same
```

### Trap 4 — chhote type ko `const T&`
```cpp
void f(const int& x);   // ⚠️ int ke liye by-value behtar -- yeh ek indirection add karta hai
```

### Trap 5 — output param bina document kiye
```cpp
compute(input, result);   // ⚠️ pata nahi 'result' modify hoga -- call site pe & nahi dikhta.
                          //    Naam se saaf karo (outResult), ya return value use karo
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "By value safe hai, isliye default rakho" | Bade types ke liye mehnga — `const T&` default |
| "`const T&` har type ke liye best" | Chhote/POD by value tez (register, no indirection) |
| "Output param modern C++ style hai" | Return-by-value (RVO) prefer karo; output-param exception |
| "`f(x)` kabhi x ko nahi badalta" | Agar signature `T&` hai to badal sakta hai — signature dekho |
| "`f("literal")` `std::string&` param mein chalega" | Nahi — non-const ref lvalue maangta |

---

## Exercises

1. **Teen versions:** `void triple(...)` — by value, by pointer, by reference.
   Teenon call karke dikhao caller ka `int x` kis-kis mein badla.

   <details><summary>Answer</summary>

   by value → nahi badla. by pointer (`triple(&x)`, body `*n *= 3`) → badla.
   by reference (`triple(x)`, body `n *= 3`) → badla.
   </details>

2. **min & max ek call mein:** `void minmax(const std::vector<int>& v, int& lo,
   int& hi)` likho. `const&` input, `int&` outputs — dono kyun?

   <details><summary>Answer</summary>

   Input sirf padhna hai + bada ho sakta hai → `const std::vector<int>&`.
   `lo`/`hi` function ke "return values" hain jinhe caller ke variables mein
   likhna hai → `int&` output params.
   </details>

3. **Copy pakdo:** `void f(std::string s)` vs `void f(const std::string& s)` —
   dono ke andar `s.size()` print. Ek loop mein 1e6 baar ek 50-char string se
   call. `-O2` pe time. Farq?

   <details><summary>Answer</summary>

   By-value version har call pe string copy (alloc + memcpy) karta hai (jab tak
   inline hoke elide na ho) — measurably slower. `const&` version sirf ek
   pointer pass karta hai. Example `03` isi ka bada version hai (~190x vector pe).
   </details>

4. **Overload:** `void g(int&)` aur `void g(const int&)` dono. `int a; const int
   b = 1;` — `g(a)`, `g(b)`, `g(5)` kaunsa call karega?

   <details><summary>Answer</summary>

   `g(a)` → `g(int&)`. `g(b)` → `g(const int&)` (const object). `g(5)` →
   `g(const int&)` (rvalue non-const `T&` se bind nahi hota).
   </details>

5. **Kya galat:** `std::vector<int> makeBig(); void use(std::vector<int>& v);
   use(makeBig());` — compile hoga? Kyun / kyun nahi? Fix?

   <details><summary>Answer</summary>

   Nahi — `makeBig()` ek temporary (rvalue), non-const `std::vector<int>&` usse
   bind nahi hota. Fix: `use(const std::vector<int>&)` agar read-only; ya pehle
   `auto v = makeBig();` phir `use(v)`; ya `use(std::vector<int>&&)` agar consume
   karni hai.
   </details>

---

## Interview questions

1. Pass-by-reference kyun? Do alag maqsad bata.
2. `T` / `const T&` / `T&` / `T*` — kis param ke liye kaunsa, kyun?
3. `const T&` chhote types (`int`) ke liye kyun avoid karte hain?
4. Output parameter vs return-by-value — modern guidance kya?
5. `void f(std::string&)` mein `f("hi")` kyun fail hota hai?
6. By-value bada object pass karne ki asli cost (measure ki?) — kya banega har call pe?

---

## Next
→ [`04-const-references.md`](04-const-references.md)

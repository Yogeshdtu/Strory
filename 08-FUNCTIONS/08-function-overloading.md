# 08 — Function overloading aur overload resolution

## Prerequisites
- [`03-parameters-and-arguments.md`](03-parameters-and-arguments.md), [`07-default-arguments.md`](07-default-arguments.md)
- `03-VARIABLES-DATA-TYPES/13-type-conversions.md` (promotions, conversions)
- `05-OPERATORS/03-comparison-operators.md` (signed/unsigned)

## Yeh topic abhi kyun
**Overloading** = ek hi naam ke kai functions, alag parameters. `print(int)`,
`print(const std::string&)`, `print(const std::vector<int>&)` — sab `print`.
Compiler har call ke liye "best match" chunta hai — yeh process **overload
resolution** hai, aur iske rules interviews mein poore pooche jaate hain. Ambiguity
errors samajhne ke liye bhi zaroori.

---

## Overloading — kya allowed

Do functions ek naam ke, agar unke **parameters alag** hon:

```cpp
void print(int x);
void print(double x);              // ✅ alag type
void print(int x, int y);          // ✅ alag count
void print(const char* s);         // ✅ alag type
```

### Kya overload NAHI kar sakte

```cpp
int  f(int x);
double f(int x);                   // ❌ sirf return type alag -- NOT allowed

void g(int x);
void g(const int x);              // ❌ top-level const value param -- SAME signature

void h(int x);
void h(int& x);                   // ✅ yeh alag hai (value vs reference)
```

**Return type overloading ka hissa nahi hai.** Aur value parameter ka top-level
`const` ignore hota hai.

---

## Overload resolution — 3 phases

Ek call `f(args)` ke liye compiler:

### Phase 1 — Candidate set
Saare `f` naam ke functions jo scope mein hain (+ ADL — argument-dependent
lookup, file 14).

### Phase 2 — Viable functions
Un candidates mein se jo **is call ko accept kar sakte hain**:
- Parameter count match (defaults count karke)
- Har argument ka har parameter mein **koi conversion** possible

### Phase 3 — Best match
Viable functions mein se **best**. Har argument ke liye conversion ka "rank"
nikalta hai:

```
   1. EXACT MATCH        -- koi conversion nahi, ya trivial (T -> const T,
                            array -> pointer, function -> function pointer)
   2. PROMOTION          -- char/short -> int ; float -> double ; bool -> int
   3. STANDARD CONVERSION-- int -> double ; int -> bool ; D* -> B* ; 0 -> pointer ;
                            int -> unsigned ; enum -> int
   4. USER-DEFINED CONV. -- constructor (implicit) ya  operator T()
   5. ELLIPSIS (...)     -- last resort
```

**Best = jo har argument pe kam se kam utna hi accha ho, aur kam se kam ek argument
pe strictly better.** Do functions equally good → **ambiguous** (error).

---

## Examples (`examples/03_overloading.cpp`)

```cpp
void show(int);
void show(double);
void show(const char*);
void show(const std::string&);

show(42);              // EXACT -> show(int)
show(3.14);            // EXACT -> show(double)
show("hi");            // EXACT -> show(const char*)
show(std::string{});   // EXACT -> show(const std::string&)

char c = 'A';
show(c);               // PROMOTION char->int -> show(int)
float f = 2.5f;
show(f);               // PROMOTION float->double -> show(double)
show(true);            // bool->int promotion -> show(int)
```

### Ambiguity

```cpp
void g(int);
void g(double);

g(5L);      // ❌ AMBIGUOUS -- long->int aur long->double, dono "standard conversion",
            //    koi behtar nahi
g(5);       // ✅ EXACT -> g(int)
g('x');     // ✅ PROMOTION -> g(int)
```

### Classic: `0` vs `nullptr`

```cpp
void h(int);
void h(char* p);

h(0);         // h(int) -- `0` ek int literal hai
h(NULL);      // ⚠️ platform-dependent (NULL 0 ho sakta ya 0L) -> confusing
h(nullptr);   // h(char*) -- nullptr ka type std::nullptr_t, pointer ko convert hota hai
```

**Isli ye `NULL` nahi, `nullptr` use karo** (folder 12).

### `const` overload (pointers/references pe — value pe nahi)

```cpp
void access(int* p);          // modifiable
void access(const int* p);    // read-only

int x; const int y = 0;
access(&x);   // access(int*)
access(&y);   // access(const int*) -- const int* -> int* allowed nahi
```

---

## Overloading vs templates vs default args

| | Overloading | Template | Default arg |
|---|---|---|---|
| Alag types, **alag body** | ✅ | ❌ (ek body sab types) | ❌ |
| Alag types, **same body** | ⚠️ (repeat) | ✅ `template<class T> void f(T)` | ❌ |
| Alag **count**, same body, param optional | ⚠️ | ❌ | ✅ |
| Compile-time dispatch | ✅ | ✅ | n/a |

Modern C++: same-logic-different-types → **template** ya `auto` param
(`void f(auto x)`, C++20). Genuinely different handling → overload.

---

## Name mangling — linker ke liye

Linker ke paas "naam" hota hai, signature nahi. Compiler har overload ko ek
**unique mangled name** deta hai:

```cpp
void f(int);              // -> _Z1fi           (Itanium ABI)
void f(double);           // -> _Z1fd
void f(int, const char*); // -> _Z1fiPKc
```

```bash
nm -C a.out | grep ' f'       # -C = demangle
echo "_Z1fi" | c++filt        # -> f(int)
```

Isi liye C++ functions ko C se call karne ke liye `extern "C"` chahiye (mangling
off — folder 24). Aur return type mangled name mein **nahi** hota → return-type-
only overload impossible.

---

## Andar kya hota hai

Overload resolution **poori tarah compile-time** hai — koi runtime cost nahi. Jo
overload chuna gaya, uska ek seedha `call` (ya inline) hota hai. Compare karo
**virtual** dispatch se (folder 16) jo runtime pe vtable dekhta hai.

`examples/06_inline_asm_check.cpp` — resolved function chhota ho to inline,
warna `call <mangled name>`.

> **HFT relevance:** Overloading zero-cost compile-time dispatch hai — hot path
> ke liye ideal (virtual calls ke ulat). Market-data decoders aksar overloaded
> `decode(const QuoteMsg&)`, `decode(const TradeMsg&)` use karte hain ya
> `if constexpr` + templates (folder 21). Ambiguity aur implicit-conversion
> surprises (`0`/`nullptr`, `int`/`size_t`, narrowing) ko `explicit`, strong
> types, aur `-Wconversion` se rokte hain. Interview: overload resolution steps
> aana zaroori hai.

---

## Hands-on

```bash
./build.ps1 08-FUNCTIONS/examples/03_overloading.cpp

# mangled names dekho
g++ -std=c++20 -c 08-FUNCTIONS/examples/03_overloading.cpp -o ov.o
nm ov.o | grep -i show          # mangled
nm -C ov.o | grep -i show       # demangled
```

Section 6 ke commented ambiguous calls ko ek-ek karke uncomment karo — exact
error text note karo.

---

## ⚠️ Traps

### Trap 1 — return type se overload
```cpp
int  parse(std::string_view);
bool parse(std::string_view);     // ❌ ambiguous / redeclaration
```

### Trap 2 — `f(0)` pointer vs int
```cpp
void f(int); void f(char*);
f(0);           // f(int) -- shayad aap f(char*) chahte the -> f(nullptr)
```

### Trap 3 — implicit conversion se galat overload
```cpp
void draw(bool visible);
void draw(int layer);
draw("hello");   // ⚠️ const char* -> bool (non-null = true) -> draw(bool)!
```
`explicit` constructors, strong types se bacho.

### Trap 4 — overload + default arg ambiguity
```cpp
void g(int);
void g(int, int = 0);
g(5);           // ambiguous
```

### Trap 5 — `size_t` vs `int` overload
```cpp
void at(int index);
void at(std::size_t index);
at(0);           // ⚠️ `0` int -> at(int); at(0u) -> at(size_t). Container APIs mein confusing
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Return type se overload kar sakte ho" | Nahi — parameters se hi |
| "`const int` value param naya overload" | Top-level const ignore — same signature |
| "`f(0)` `f(char*)` ko call karega" | `0` int hai → `f(int)`. `nullptr` chahiye |
| "Overload resolution runtime pe" | Poora compile-time — zero cost |
| "Ambiguous call = koi match nahi" | Do (ya zyada) **equally good** match |
| "Overloading aur template same" | Overload: alag body OK. Template: ek body, sab types |

---

## Exercises

1. **Resolve karo:** har call kaunsa overload —
   ```cpp
   void p(int); void p(double); void p(const char*);
   p(5);  p(5.0);  p('a');  p("x");  p(5u);  p(true);  p(5L);
   ```
   <details><summary>Answer</summary>
   `p(int)` (exact), `p(double)` (exact), `p(int)` (char→int promo), `p(const char*)`,
   `p(int)` (unsigned→int is a conversion but both int/double are conversions... actually
   `p(5u)`: unsigned→int and unsigned→double both standard conv → **ambiguous**), 
   `p(int)` (bool→int promo), `p(5L)`: long→int and long→double both conv → **ambiguous**.
   </details>

2. **`nullptr`:** `void reg(int); void reg(void*);` — `reg(0)`, `reg(nullptr)`,
   `reg(NULL)` — kaunsa kya call karta hai? `NULL` wala platform pe check.

3. **Ambiguity fix:** `void area(int, int); void area(double, double);` —
   `area(3, 4.0)` compile hua? Kyun / kyun nahi? Fix (`static_cast` ya teesra overload).

4. **`const` overload:** `void touch(std::string& s)` aur `void touch(const
   std::string& s)` — ek mutable string aur ek `const` string pass karo. Kaunsa
   chuna gaya? Kyun useful (proxy/lazy patterns)?

5. **Mangling:** 3 overloads `void f(int)`, `void f(int, int)`, `void f(double)`
   — `nm` se mangled names, `c++filt` se wapas demangle.

6. **Overload → template:** yeh 3 overloads ko ek template mein badlo —
   ```cpp
   int maxOf(int a, int b) { return a > b ? a : b; }
   double maxOf(double a, double b) { return a > b ? a : b; }
   std::string maxOf(std::string a, std::string b) { return a > b ? a : b; }
   ```

7. **Implicit-conversion trap:** `void handle(bool); void handle(const std::string&);`
   — `handle("literal")` kaunsa? Kyun surprising? Fix.

---

## Interview questions

1. Overloading kya hai? Kis basis pe functions distinguish hote hain?
2. Return type se overload kyun nahi?
3. Overload resolution ke 3 phases? Conversion ranks (exact/promotion/conversion/user-defined)?
4. Ambiguous call kab hota hai? `g(int); g(double); g(5L)` — kya?
5. `f(0)` vs `f(nullptr)` overload resolution mein — fark?
6. Name mangling kya hai? `extern "C"` se connection?
7. Overloading vs templates vs default arguments — decision?

---

## Next
→ [`09-recursion.md`](09-recursion.md)

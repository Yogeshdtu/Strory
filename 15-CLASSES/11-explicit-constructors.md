# 11 — `explicit` constructors

## Prerequisites
- [`04-constructors.md`](04-constructors.md)
- Folder 03 (implicit conversions, narrowing)

## Yeh topic abhi kyun
Ek single-argument constructor **implicit conversion** enable kar deta hai —
compiler chup-chaap `T` ki jagah uska argument type accept karke `T` bana leta
hai. Kabhi yeh convenient, aksar yeh silent bugs aur surprise overload
resolution. **`explicit`** isse rokta hai. Modern C++ mein "`explicit` by
default" ka rule.

---

## Implicit conversion — kya hota hai

```cpp
class Meters {
    double value_;
public:
    Meters(double v) : value_(v) {}         // single-arg ctor -- IMPLICIT (no `explicit`)
    double value() const { return value_; }
};

void jump(Meters distance);

jump(5.0);                  // ⚠️ compiles! 5.0 -> Meters{5.0}  implicitly. "5 kya? meters? feet?"
Meters d = 3.0;             // ⚠️ compiles -- copy-init se implicit conversion
Meters e = {3.0};           // ⚠️ compiles
```

Compiler ne `5.0` (a `double`) ko `Meters` mein **chup-chaap** convert kar diya
kyunki `Meters(double)` implicit hai. Kabhi theek, par aksar:
- Unit confusion (`jump(5.0)` — 5 kya units?).
- Wrong-overload selection.
- Accidental temporaries in hot code.

---

## `explicit` — implicit conversion band

```cpp
class Meters {
    double value_;
public:
    explicit Meters(double v) : value_(v) {}    // ✅ explicit
};

void jump(Meters distance);

// jump(5.0);              // ❌ ERROR -- 5.0 se Meters implicitly nahi banta
jump(Meters{5.0});         // ✅ explicit -- reader ko pata "yeh Meters hai"

// Meters d = 3.0;         // ❌ ERROR (copy-init)
Meters d{3.0};             // ✅ direct-init
Meters e(3.0);             // ✅ direct-init
```

`explicit` ke saath:
- **Direct initialization** (`Meters d{3.0}`, `Meters d(3.0)`) — kaam karta.
- **Copy initialization** (`Meters d = 3.0`, function arg passing, `return 3.0;`
  jab return type `Meters` ho) — **band**. Aapko `Meters{...}` likhna padta.

---

## `explicit` kab lagao — "by default" rule

**Har single-argument constructor ko `explicit` banao**, sivaay jab implicit
conversion genuinely wanted ho.

```cpp
class Timestamp {
public:
    explicit Timestamp(std::int64_t ns);         // ✅ int64 se Timestamp accidental nahi
};

class Seconds {
public:
    explicit Seconds(double s);                   // ✅
};

// Exception -- implicit intentionally:
class String {
public:
    String(const char* s);                        // OK implicit -- "hello" -> String natural hai
};
```

Multi-arg ctors bhi `explicit` ho sakte hain (C++11+) — brace-init se implicit
conversion rokne ke liye:

```cpp
class Rect {
public:
    explicit Rect(int w, int h);
};
Rect r = {3, 4};        // ❌ agar explicit -- copy-list-init band
Rect r{3, 4};           // ✅
```

---

## `explicit` conversion operators

Ulta bhi — class se doosre type mein conversion:

```cpp
class Handle {
    int fd_ = -1;
public:
    explicit operator bool() const { return fd_ >= 0; }   // ✅ explicit
    // bina explicit: `if (h)` chalta, par `int x = h + 1;` bhi (bool->int)! surprising
};

Handle h;
if (h) { }              // ✅ explicit operator bool -- "contextual conversion to bool" allowed
// int x = h;           // ❌ explicit -- direct conversion band
bool b = static_cast<bool>(h);   // ✅ explicit cast
```

`explicit operator bool()` — `if`, `while`, `&&`, `||`, `!` mein kaam karta
(contextual bool), par `int`/arithmetic contexts mein nahi. Yeh standard idiom
hai (`std::optional`, `std::unique_ptr`, streams sab yeh karte hain).

---

## `explicit(bool)` — C++20 conditional

```cpp
template <class T>
class Wrapper {
public:
    explicit(!std::is_convertible_v<T, int>) Wrapper(T value);
    // explicit tab jab T int mein convert nahi hota; warna implicit
};
```

Advanced — generic code mein jahan `explicit`-ness type par depend kare. Abhi
bas pehchan lo.

---

## Andar kya hota hai

- `explicit` **zero runtime cost** — pure compile-time overload-resolution rule.
  Ek `explicit` ctor ko direct-init mein use karo → bilkul wahi code jo implicit
  hone par hota.
- Bina `explicit`, compiler har jagah jahan `Meters` expected hai aur `double`
  mila, ek **implicit conversion sequence** consider karta — jo (a) galat
  overload chun sakta, (b) ek chhupa temporary construct kar sakta (ctor call jo
  source mein nahi dikhta).
- `explicit` se woh conversion sequences overload resolution se **hat** jaati
  hain → kam candidates, no hidden temporaries, aur ambiguity errors turant
  (chup-chaap galat conversion ke bajaye).

> **HFT relevance:** strong types (`Price`, `Qty`, `Side`, `SymbolId`) ke ctors
> `explicit` — taaki `sendOrder(150.25, 100)` compile hi na ho (units/order
> ambiguous), `sendOrder(Price{150.25}, Qty{100})` likhna pade. Yeh whole-class
> of bugs (arg swap, wrong units, wrong scale) compile-time pakadta. Hot code
> mein accidental implicit conversions = chhupe ctor calls (aur unki cost) —
> `explicit` unhe visible/banned banata. `explicit operator bool()` resource
> handles (`unique_ptr`-jaise pool handles) pe standard. Rule: **single-arg ctor
> = `explicit` jab tak strong reason na ho.**

---

## Hands-on

```bash
./build.ps1 15-CLASSES/examples/06_operator_overload.cpp
```

`Money` ka `explicit Money(std::int64_t)` — `Money m = 500;` (comment) compile
nahi hoga, `Money m{500}` / `Money::fromRupees(5)` hi. Try: `explicit` hatao,
phir `Money x = 5;` aur `void f(Money); f(5);` — dono compile ho jayenge.

---

## ⚠️ Traps

### Trap 1 — single-arg ctor bina `explicit`
```cpp
class Id { Id(int); };  void log(Id);  log(42);   // ⚠️ 42 -> Id{42} chup-chaap. explicit lagao
```

### Trap 2 — `explicit` ke saath copy-init
```cpp
explicit Meters(double);
Meters d = 3.0;        // ❌ (copy-init). Meters d{3.0};  ✅ (direct-init)
```

### Trap 3 — `return` mein implicit conversion
```cpp
Meters makeM() { return 3.0; }   // ❌ agar explicit -- return Meters{3.0};
```

### Trap 4 — non-explicit `operator bool` ka spread
```cpp
struct S { operator bool() const; };  S a, b;  int x = a + b;   // ⚠️ bool->int, "a+b" = 0/1/2. explicit operator bool
```

### Trap 5 — `explicit` on the wrong ctor (multi-arg jab implicit chahiye)
```cpp
explicit Point(int x, int y);
Point p = {1, 2};      // ❌ -- agar aap `{1,2}` se Point banane dena chahte ho, explicit mat lagao
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`explicit` runtime cost / slower" | Zero — compile-time overload rule |
| "`explicit` matlab ctor use hi nahi hota" | Direct-init (`T{x}`, `T(x)`) kaam karta; sirf implicit conversion band |
| "Sirf single-arg ctors `explicit` ho sakte" | Multi-arg bhi (C++11+) — brace copy-init rokne ko |
| "`operator bool` bina `explicit` fine" | Implicit → arithmetic contexts mein leak. `explicit operator bool` |
| "`explicit` by default over-cautious" | Modern guideline — implicit sirf jab genuinely a "same thing" |

---

## Exercises

1. **Add explicit:** `class Celsius { double t_; public: Celsius(double t); };  void
   setTemp(Celsius);` — `setTemp(20.0);` abhi compile hota hai. `explicit` lagao,
   ab kya likhna padega?

   <details><summary>Answer</summary>

   `explicit Celsius(double);` → `setTemp(20.0)` ERROR. `setTemp(Celsius{20.0})`
   likhna padega — intent clear.
   </details>

2. **Which compile:** `explicit Meters(double v);` ke saath —
   ```cpp
   Meters a{5.0};
   Meters b(5.0);
   Meters c = 5.0;
   Meters d = {5.0};
   Meters e = Meters{5.0};
   void f(Meters); f(5.0);
   ```

   <details><summary>Answer</summary>

   `a` ✅ (direct-list), `b` ✅ (direct), `c` ❌ (copy-init), `d` ❌
   (copy-list-init), `e` ✅ (explicit temp then copy — actually `Meters{5.0}` is
   fine, copy elision), `f(5.0)` ❌ (arg passing = copy-init).
   </details>

3. **operator bool:** `class SafeInt { int v_; bool ok_; public: ... };` —
   `explicit operator bool()` add karo jo `ok_` return kare. `if (si)`,
   `bool b = si`, `int x = si + 1` — kaunse compile?

   <details><summary>Answer</summary>

   `if (si)` ✅ (contextual bool). `bool b = si` ❌ (copy-init — explicit).
   `int x = si + 1` ❌ (no implicit bool→arith). `bool b = static_cast<bool>(si)`
   ✅.
   </details>

4. **Intentional implicit:** kis type ke liye implicit single-arg ctor genuinely
   OK hai? `class Kilometers(double)`, `class Json(std::string)`, `class
   UserId(int)`, `class BigInt(long long)`.

   <details><summary>Answer</summary>

   Debatable, par: `BigInt(long long)` — a bigger integer, "same kind of thing",
   implicit reasonable. Baaki (`Kilometers`, `Json`, `UserId`) — `explicit`
   (units / parsing / id-vs-int confusion). `Json(std::string)` implicit khaas
   khatarnaak (koi bhi string Json ban jaaye).
   </details>

5. **Arg-swap bug prevented:** `void trade(Price p, Qty q);` — dono `explicit`
   strong types. `trade(Qty{100}, Price{50})` — compile hota hai? `trade(50,
   100)`?

   <details><summary>Answer</summary>

   `trade(Qty{100}, Price{50})` → ERROR (types swapped, `Qty` ≠ `Price`). `trade(50,
   100)` → ERROR (explicit — `int`/`double` se `Price`/`Qty` implicitly nahi).
   Dono common bugs compile-time caught.
   </details>

---

## Interview questions

1. Single-arg ctor bina `explicit` — kya enable hota, kya risk?
2. `explicit` ke saath kaunse initialization forms kaam karte, kaunse nahi?
3. `explicit operator bool()` — kya allow karta, kya rokta?
4. `explicit` ka runtime cost?
5. Multi-arg ctor `explicit` — kya rokta?
6. "`explicit` by default" — kaunse cases exception?

---

## Next
→ [`12-nested-and-local-classes.md`](12-nested-and-local-classes.md)

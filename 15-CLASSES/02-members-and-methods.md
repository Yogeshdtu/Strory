# 02 — Members, methods, aur `this`

## Prerequisites
- [`01-what-is-a-class.md`](01-what-is-a-class.md)
- Folder 12 (pointers — `this` ek pointer hai)

## Yeh topic abhi kyun
Class ke andar do kism ke "members" hote hain: **data members** (state) aur
**member functions / methods** (behaviour). Aur ek hidden cheez har method ko
milti hai — **`this` pointer** — jo batata hai "kis object pe kaam ho raha hai".
Yeh samajhna zaroori hai — `this` overloading, chaining, aur bahut saari
compile-error messages ki jad hai.

---

## Data members

```cpp
class Trade {
    std::uint64_t id_;              // data member
    double        price_;           // data member
    std::uint32_t qty_ = 0;         // data member with default member initializer (DMI)
    static inline int s_count = 0;  // STATIC data member -- class ka, object ka nahi (file 08)
};
```

- Har **non-static** data member har object ki apni copy hai — object ke andar
  storage leta hai.
- **DMI** (`= 0`) — agar constructor us member ko explicitly init na kare to yeh
  default use hota hai.
- Naming convention (is repo mein): trailing underscore `id_` — member ko local
  variable / parameter se distinguish karne ke liye.

---

## Member functions (methods)

```cpp
class Trade {
    double price_;
    std::uint32_t qty_;

public:
    double notional() const { return price_ * qty_; }    // method -- object ke data pe kaam

    void applyFill(std::uint32_t f) {                     // mutating method
        qty_ -= f;
    }
};
```

Method ke andar aap data members ko **seedha naam se** access karte ho (`price_`,
`qty_`) — koi object prefix nahi. Compiler khud samajhta hai "current object ka".

### Declaration vs definition (bade classes ke liye)

```cpp
// trade.hpp
class Trade {
    double price_;
public:
    double notional() const;        // sirf declaration
    void   reprice(double p);
};

// trade.cpp
double Trade::notional() const { return price_ * qty_; }   // definition -- Trade:: prefix
void   Trade::reprice(double p) { price_ = p; }
```

Chhoti methods class ke andar hi define karo (implicitly `inline`); badi / jinke
liye extra headers chahiye — `.cpp` mein `ClassName::` ke saath.

---

## `this` — "kis object pe?"

Har **non-static** method ko ek hidden pehla parameter milta hai: **`this`**, jo
current object ka pointer hai.

```cpp
class Trade {
    double price_;
public:
    void reprice(double price) {
        price_ = price;              // = this->price_ = price;
        // yahan 'price' (param) aur 'price_' (member) alag hain -- underscore ki wajah
    }

    double getPrice() const {
        return this->price_;        // 'this->' optional -- yahan bekaar, par valid
    }
};
```

- `obj.reprice(10)` ≈ compiler internally `Trade::reprice(&obj, 10)` — `this == &obj`.
- `this` ka type: non-const method mein `Trade*`, **const method** mein `const
  Trade*` (file 07).
- `this` ko rebind nahi kar sakte (`this = ...` ❌); modern C++ mein woh ek
  prvalue hai.

### Kab `this->` sach mein chahiye

```cpp
class Widget {
    int value;
public:
    void set(int value) {           // param aur member ka naam same
        this->value = value;        // ⚠️ 'this->' zaroori -- warna `value = value;` (no-op, -Wself-assign)
    }
};
```

Isiliye members pe `_` suffix (ya `m_` prefix) — naming se yeh confusion hi nahi
hota.

### `return *this` — method chaining

```cpp
class Query {
    std::string sql_;
public:
    Query& select(const std::string& cols) { sql_ += "SELECT " + cols + " "; return *this; }
    Query& from  (const std::string& tbl)  { sql_ += "FROM "   + tbl  + " "; return *this; }
    Query& where (const std::string& cond) { sql_ += "WHERE "  + cond;       return *this; }
    const std::string& str() const { return sql_; }
};

Query q;
q.select("*").from("orders").where("qty > 0");     // chaining -- har method ne *this ka ref diya
```

`return *this;` — object ka reference return karo (folder 13 file 05) taaki agla
`.method()` usi object pe chale. `T&` return karo, `T` (by value) nahi — warna
har call ek copy pe chalega.

---

## Method resolution — kaunsa `x`?

Method body mein ek naam `x` dhoondhne ka order:
1. Local variables / parameters
2. Class ke members (`this->x`)
3. Base class ke members (folder 16)
4. Enclosing namespace / globals

```cpp
int count = 100;                    // global

class C {
    int count = 5;                  // member
public:
    void f(int count) {             // parameter
        // 'count' yahan = parameter (10)
        // this->count = member (5)
        // ::count = global (100)
    }
};
```

---

## Andar kya hota hai

- **`obj.method(args)`** compile hota hai as `method(&obj, args)` — `this` ek
  normal pointer argument (Win x64: `rcx`; System V: `rdi`).
- Member access `price_` inside a method = `[this + offsetof(Trade, price_)]` —
  ek load/store relative to `this`. Bilkul `p->price_` jaisa (folder 12 file 08).
- **`-O2` pe** chhoti methods poori inline ho jaati hain — `this` indirection
  gaayab, member seedha register/stack se. `notional()` ek `mul` ban sakta hai.
- Method ka koi per-object storage nahi — 1000 `Trade` objects, `notional` code
  ek hi jagah.
- `return *this` = `this` ka value return (ek address) — caller usse reference
  ki tarah use karta.

> **HFT relevance:** methods free hain — `this` ek pointer arg, member access ek
> offset load, aur hot methods `-O2` pe fully inline. To ek achhe encapsulated
> `Book`/`Order` class ka accessor (`book.bestBid()`) generated code mein ek raw
> `struct` field read jitna hi hai. Chaining (`builder.a().b().c()`) bhi zero-cost
> jab inline ho. Watch-out: badi non-inlinable methods hot path pe (ek `call` +
> `this` setup + no cross-call optimization) — unhe chhota rakho ya `inline`
> header mein.

---

## Hands-on

```bash
./build.ps1 15-CLASSES/examples/01_first_class.cpp     # this->owner_, member access
```

`01_first_class.cpp` ka `print()` `this->owner_` use karta hai. Aur khud: ek
`Vec3` class banao with `Vec3& operator+=(const Vec3&)` returning `*this`, phir
`a += b += c;` chalao.

---

## ⚠️ Traps

### Trap 1 — parameter aur member ka naam same, bina `this->`
```cpp
void set(int value) { value = value; }   // ⚠️ self-assign (-Wself-assign). this->value = value;
```

### Trap 2 — chaining method by value return
```cpp
Query select(...) { ...; return *this; }   // ⚠️ copy! har chained call alag copy pe. Query& return karo
```

### Trap 3 — `this` ko store karke object ke marne ke baad use
```cpp
struct S { S* self() { return this; } };
S* p; { S s; p = s.self(); }  *p;   // ⚠️ dangling -- s gaya (folder 12/13)
```

### Trap 4 — `static` method mein `this`
```cpp
static int count() { return this->n_; }   // ❌ static method mein `this` nahi (file 08)
```

### Trap 5 — member init order pe `this` ke through depend
```cpp
C() : b_(compute()), a_(b_) {}   // ⚠️ a_ pehle declare hua? to a_ garbage b_ se (file 05)
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`this` har method mein hota hai" | Sirf **non-static** methods mein |
| "`this` ek reference hai" | Pointer (`T*` / `const T*`). `*this` deref |
| "Method call = normal function call + kuch overhead" | `this` ek hidden arg; `-O2` pe aksar fully inline |
| "`return *this` copy karta" | `T&` return → koi copy. `T` return → copy |
| "Member aur param same naam → member wins" | Param wins (inner scope). `this->` se member |

---

## Exercises

1. **`this` desugar:** `class C { int n_; public: void inc() { ++n_; } };  C c;
   c.inc();` — `inc()` ko free function `inc(C* self)` ke roop mein likho jo
   same kaam kare.

   <details><summary>Answer</summary>

   `void inc(C* self) { ++self->n_; }` (n_ ko public maan ke). `c.inc()` ≈
   `inc(&c)`. Compiler yahi karta hai.
   </details>

2. **Shadowing:** `int x = 1; class C { int x = 2; public: int f(int x) {
   return x + this->x + ::x; } };  C{}.f(10);` — result?

   <details><summary>Answer</summary>

   `10 (param) + 2 (member via this->) + 1 (global via ::) = 13`.
   </details>

3. **Chaining:** `class Str { std::string s_; public: Str& add(char c); const
   std::string& get() const; };` — `add` likho jo `*this` return kare. `Str
   x; x.add('a').add('b').add('c');` — `x.get()`?

   <details><summary>Answer</summary>

   `Str& add(char c) { s_ += c; return *this; }` → `x.get() == "abc"`. Agar `Str
   add(...)` (by value) hota to `x` unchanged rehta (temporaries pe chalta).
   </details>

4. **Broken chain:** upar wale `add` ko `Str` (by value) return karwao. `x.add('a').add('b');`
   ke baad `x.get()` kya? Kyun?

   <details><summary>Answer</summary>

   `x.get() == "a"` — pehla `add` `x` ko modify karta hai aur ek **copy**
   lautaata hai; `.add('b')` us copy pe chalta hai, `x` pe nahi.
   </details>

5. **Method vs storage:** `class Big { double a[100]; public: double sum() const;
   void scale(double); double at(int i) const; };` — `sizeof(Big)`? 3 methods
   add karne se kitne bytes bade?

   <details><summary>Answer</summary>

   `sizeof(Big) == 800` (100 doubles). Methods → 0 bytes. Method code `.text`
   mein, per-object storage nahi.
   </details>

---

## Interview questions

1. Data member vs member function — object storage pe kya asar?
2. `this` pointer kya hai, kaunsa type, kab available (static?)?
3. `obj.method(x)` internally kaise call hota (`this` kahan)?
4. `return *this` — kyun `T&`, `T` nahi?
5. Method body mein naam resolution ka order?
6. Chhoti method class-body mein vs `.cpp` mein — `inline` implication?

---

## Next
→ [`03-access-specifiers.md`](03-access-specifiers.md)

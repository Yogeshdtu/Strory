# 12 — Nested & local classes

## Prerequisites
- [`03-access-specifiers.md`](03-access-specifiers.md), [`10-friend-functions.md`](10-friend-functions.md)

## Yeh topic abhi kyun
Ek class doosri class ke **andar** define ho sakti hai (nested), aur ek function
ke andar bhi (local). Nested classes real code mein aksar dikhte hain —
container ka `iterator`, `Node`, ek `Config` ka `Section`. Local classes rare,
par kuch patterns (custom deleters, ad-hoc comparators pre-lambda) mein aate the.
Scoping, access, aur kab kaunsa.

---

## Nested class

```cpp
class LinkedList {
    struct Node {                          // nested class (default private here)
        int   value;
        Node* next = nullptr;
    };

    Node* head_ = nullptr;

public:
    class Iterator {                       // nested, public -- API ka hissa
        Node* cur_;
    public:
        explicit Iterator(Node* n) : cur_(n) {}
        int&      operator*()  { return cur_->value; }
        Iterator& operator++() { cur_ = cur_->next; return *this; }
        bool operator!=(const Iterator& o) const { return cur_ != o.cur_; }
    };

    Iterator begin() { return Iterator{head_}; }
    Iterator end()   { return Iterator{nullptr}; }
};

// bahar se: LinkedList::Iterator  (scoped naam)
```

Key points:
- Nested class ka **naam enclosing class ke scope mein** hai: `LinkedList::Node`,
  `LinkedList::Iterator`.
- Nested class ek **normal independent class** hai — enclosing object ka koi
  automatic pointer / access nahi. `Node` ke andar `head_` seedha nahi dikhta.
- **Access:** nested class enclosing class ke `private` members access **kar
  sakti hai** (C++11+ — nested class implicitly friend jaisa treatment). Ulta:
  enclosing class ko nested ke private access ke liye `friend` chahiye (ya nested
  public rakhho).

```cpp
class Outer {
    int secret_ = 42;
    class Inner {
    public:
        int peek(const Outer& o) { return o.secret_; }   // ✅ C++11+ -- Inner can see Outer::secret_
    };
};
```

---

## Nested class kab use karein

- **Implementation detail tightly bound to the enclosing class** — `Node` of a
  list/tree, `Bucket` of a hash map, `Impl` of a pImpl. `private` nested.
- **A helper type that's part of the class's interface** — `iterator`,
  `const_iterator`, `Section`, `Handle`. `public` nested, scoped naam
  (`Map::iterator`).
- **Namespacing** — `Widget::Config`, `Parser::Error` — reader ko clear hai yeh
  type kis cheez se related hai.

Agar type standalone useful hai (kai classes use karenge) → nested mat karo,
namespace mein rakho.

---

## Nested class — forward declaration & out-of-line definition

```cpp
class Server {
    class Session;                    // forward-declare nested
    std::vector<Session*> sessions_;
public:
    void tick();
};

class Server::Session {              // out-of-line definition -- Server:: prefix
    int id_;
public:
    explicit Session(int id) : id_(id) {}
};
```

Useful jab nested class bada ho ya circular dependency ho.

---

## Local class (function ke andar)

```cpp
std::sort(v.begin(), v.end(), [](int a, int b) { return a > b; });   // aaj: lambda

// pre-C++11 / special cases: local class
void process() {
    struct DescCompare {                          // local class -- sirf is function mein
        bool operator()(int a, int b) const { return a > b; }
    };
    std::sort(v.begin(), v.end(), DescCompare{});
}
```

Rules:
- Local class ka naam **sirf us function mein** visible.
- **Enclosing function ke local variables access NAHI kar sakti** (sivaay
  `static` locals aur `constexpr`) — kyunki koi capture mechanism nahi (lambda
  ke ulat).
- Member functions **function ke andar hi define** hone chahiye (out-of-line
  nahi).
- Templates member nahi ban sakte.

**Aaj local classes lagbhag hamesha lambdas se replace ho gaye hain.** Aap inhe
purane codebases mein dekhoge; naya code likhte waqt lambda.

---

## Nested vs pImpl (folder 25 preview)

```cpp
// widget.hpp -- header mein sirf pointer, Impl chhupa
class Widget {
    class Impl;                       // forward-declared nested
    std::unique_ptr<Impl> pimpl_;
public:
    Widget();
    ~Widget();                        // Impl complete hone ke liye .cpp mein define
    void doThing();
};

// widget.cpp
class Widget::Impl {
    // saara asli implementation + members yahan -- header clean rehta
};
```

"pImpl" (pointer to implementation) — nested `Impl` class ko `.cpp` mein rakho,
header mein sirf `unique_ptr<Impl>`. Fayda: compile-time isolation (Impl badlo,
users recompile nahi), ABI stability. Cost: ek extra allocation + indirection.
Detail folder 25.

---

## Andar kya hota hai

- Nested class = **completely separate type**, bas naam scoped. `sizeof(Outer)`
  mein `Inner` ka koi contribution nahi (jab tak `Outer` ka koi `Inner` **member**
  na ho). Layout, ABI — independent.
- Nested class ke methods normal functions `.text` mein, mangled naam mein
  enclosing scope (`Outer::Inner::method`).
- `LinkedList::Iterator` jaise nested iterators `-O2` pe fully inline ho jaate —
  `for (int x : list)` ek raw pointer walk ban jaata, koi Iterator object
  overhead nahi.
- Local class ke methods bhi normal functions; compiler unhe unique internal
  linkage naam deta. `-O2` pe inline.
- pImpl: `pimpl_` ek pointer (8 bytes) + har method call ek indirection
  (`pimpl_->realMethod()`) — jo inline nahi ho sakta cross-TU.

> **HFT relevance:** nested classes hot data structures mein common —
> `OrderBook::Level`, `RingBuffer::Slot`, custom `iterator`s — jo enclosing
> type ke saath tightly designed hain aur `-O2` pe inline hoke zero-cost. pImpl
> **hot path pe avoid** (extra allocation + non-inlinable indirection) — woh
> library boundaries / rarely-called APIs ke liye. Local classes practically
> gaye — lambdas (folder 22) unhe replace karte hain zero-cost. Nested `Node`/
> `Slot` types ko trivially-copyable + fixed-size rakha jaata (pool/`memcpy` —
> folder 14).

---

## Hands-on

```cpp
// nested.cpp
#include <iostream>
class Matrix {
    int data_[4] = {1, 2, 3, 4};
public:
    class Row {                              // nested public
        int* p_;
    public:
        explicit Row(int* p) : p_(p) {}
        int& operator[](int c) { return p_[c]; }
    };
    Row operator[](int r) { return Row{data_ + r * 2}; }
};
int main() {
    Matrix m;
    m[0][1] = 99;                            // Matrix::Row proxy
    std::cout << m[0][1] << " " << m[1][0] << "\n";   // 99 3
}
```

```bash
g++ -std=c++20 -Wall -Wextra nested.cpp -o nested && ./nested
```

---

## ⚠️ Traps

### Trap 1 — nested class enclosing object ka access assume karna
```cpp
class Outer { int n_; class Inner { int f() { return n_; } }; };   // ❌ n_ kis Outer ka? Inner ke paas Outer* nahi
```

### Trap 2 — nested class ka naam bahar bina scope
```cpp
Iterator it = list.begin();        // ❌ -- LinkedList::Iterator it = ...
```

### Trap 3 — local class se local variable capture
```cpp
void f() { int x = 5; struct S { int g() { return x; } }; }   // ❌ local class captures nahi karti. Lambda use karo
```

### Trap 4 — local class ko template arg mein (pre-C++11)
Pre-C++11 local classes template arguments nahi ho sakte the. C++11+ OK, par
lambda better.

### Trap 5 — pImpl ka destructor header mein `= default`
```cpp
// widget.hpp: ~Widget() = default;   -- ⚠️ Impl incomplete -> unique_ptr<Impl> delete nahi kar sakta.
// .cpp mein ~Widget() = default;  (jahan Impl complete hai)
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Nested class ke paas enclosing object ka pointer hota" | Nahi — bilkul independent type, bas scoped naam |
| "Nested class `sizeof(Outer)` badhati" | Sirf tab jab `Outer` ka koi nested-type **member** ho |
| "Local class local variables use kar sakti" | Nahi (sirf `static`/`constexpr`). Lambda captures |
| "Local classes modern C++ mein useful" | Lambdas ne replace kiya — legacy code mein dikhenge |
| "Nested class enclosing ke private nahi dekh sakti" | Dekh sakti (C++11+) |

---

## Exercises

1. **Iterator nesting:** `class IntStack { int data_[16]; int top_ = 0; };` —
   ek nested `public` `Iterator` add karo jo `begin()`/`end()` se mile aur
   `for (int x : stack)` chale.

   <details><summary>Answer</summary>

   `class Iterator { int* p_; public: explicit Iterator(int* p):p_(p){} int&
   operator*(){return *p_;} Iterator& operator++(){++p_;return *this;} bool
   operator!=(const Iterator& o)const{return p_!=o.p_;} };` +
   `Iterator begin(){return Iterator{data_};} Iterator end(){return
   Iterator{data_+top_};}`.
   </details>

2. **Access direction:** `class Bank { double reserve_ = 0; class Auditor {
   public: double check(const Bank& b); }; };` — `check` `b.reserve_` access
   kar sakta? Ulta, `Bank` `Auditor` ke private access ke liye kya chahiye?

   <details><summary>Answer</summary>

   `check` → yes, nested class enclosing ke private dekh sakti (C++11+). `Bank`
   ko `Auditor` ke private access ke liye `Auditor` mein `friend class Bank;` ya
   `Auditor`'s members public.
   </details>

3. **Local → lambda:** yeh local class comparator ko lambda mein convert karo:
   `struct ByLen { bool operator()(const std::string& a, const std::string& b)
   const { return a.size() < b.size(); } };  std::sort(v.begin(), v.end(),
   ByLen{});`

   <details><summary>Answer</summary>

   `std::sort(v.begin(), v.end(), [](const std::string& a, const std::string& b)
   { return a.size() < b.size(); });` — same codegen, less boilerplate, can
   capture if needed.
   </details>

4. **Scoped name:** `class Parser { public: enum class Error { None, Syntax, EOF_
   }; struct Result { Error err; int value; }; };` — bahar se `Result` aur
   `Error::Syntax` kaise likhoge?

   <details><summary>Answer</summary>

   `Parser::Result r = ...;` and `Parser::Error::Syntax`. Nested types are
   accessed with `Parser::`.
   </details>

5. **pImpl sketch:** `class Db` — header mein sirf `std::unique_ptr<Impl>
   pimpl_;` + declared (not defaulted) `~Db()`. `.cpp` mein `class Db::Impl {
   ... }` + `Db::~Db() = default;`. Header mein `= default` kyun nahi chalta?

   <details><summary>Answer</summary>

   Header mein `Impl` incomplete → `unique_ptr<Impl>`'s destructor needs complete
   `Impl` to `delete` it → `= default` in header instantiates it against
   incomplete type → error. `.cpp` mein `Impl` complete hai → `= default` there
   works.
   </details>

---

## Interview questions

1. Nested class ka naam kis scope mein? Enclosing object se relation?
2. Nested class enclosing ke `private` access kar sakti? Ulta?
3. Local class local variables use kar sakti? Lambda se fark?
4. Nested class `sizeof(Outer)` pe kab asar daalti?
5. pImpl idiom — kya, kyun (2 fayde), kya cost?
6. Local classes modern C++ mein kyun rare?

---

## Next
→ [`13-class-layout.md`](13-class-layout.md)

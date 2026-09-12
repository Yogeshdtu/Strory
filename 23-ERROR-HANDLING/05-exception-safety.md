# 05 — Exception safety: basic / strong / nothrow

## Prerequisites
- `04-stack-unwinding.md`, `18-COPY-MOVE` (copy/move, `noexcept` move)
- [`examples/02_exception_safety.cpp`](examples/02_exception_safety.cpp)

## Yeh topic abhi kyun
Agar aapke code mein kahin `throw` ho sakta hai (aapke ya library ke), to har
function ke baare mein ek sawaal hai: **agar yeh beech mein fail ho jaaye, object
kis haal mein chhod jaayega?** Iska jawab function ka "exception safety level" hai.
Yeh sirf theory nahi — `std::vector::push_back` ki poori design isi ke around hai,
aur "copy-and-swap" idiom yahin se aata hai.

---

## Chaar levels (Abrahams guarantees)

| Level | Guarantee agar exception nikle | Matlab |
|---|---|---|
| **No-throw** (`noexcept`) | Exception nikalta hi nahi | Operation hamesha succeed |
| **Strong** | Operation *poora hua ya kuch nahi badla* (commit-or-rollback) | Transaction-like |
| **Basic** | Object **valid** state mein, koi leak nahi — par *kaunsa* valid state pata nahi | Invariants hold, resources safe |
| **None** | Kuch bhi — corrupt state, leak, double-free | ❌ acceptable nahi |

Har function ka **kam se kam "basic"** hona chahiye. "Strong" jahan sasta ho.
"No-throw" un operations ke liye jinpe strong/basic *dependencies* build hoti hain
(swap, move, dtor, cleanup).

---

## Basic guarantee — "valid, no leak"

```cpp
void Account::apply(const Txn& t) {
    balance_ += t.amount;          // (1)
    ledger_.push_back(t);          // (2) yeh throw kar sakta (alloc)
}
```

Agar (2) throw kare: `balance_` badal chuka, `ledger_` nahi. Object **valid** hai
(koi invariant nahi toota, koi leak nahi) — par state "aadha applied". **Basic**
guarantee. Aksar yeh kaafi hai (caller retry ya abort kar sakta).

**Basic guarantee todta kya hai:** raw resource jo leak ho jaaye, ya invariant
jo half-update se toot jaaye.

```cpp
void bad_grow() {
    T* nd = new T[ncap];                        // (1) alloc
    for (size_t i=0;i<n;++i) nd[i] = data_[i];  // (2) copy — THROW yahan?
    delete[] data_;                             // (3)
    data_ = nd;
}
// (2) throw -> `nd` LEAK (kabhi delete nahi hua). Basic guarantee bhi TOOT gaya.
```

Fix: `nd` ko RAII holder (`unique_ptr<T[]>`, ya ek chhota `RawBuf`) mein rakho —
throw pe uska dtor `nd` free kar deta hai. (Example `02` ka `BadVec` vs `GoodVec`
bilkul yeh dikhata hai, live-object counter ke saath.)

---

## Strong guarantee — "commit or rollback"

Operation ya to poora hota hai, ya object bilkul waisa hi rehta hai jaisa pehle
tha. Client ke liye "as if it never ran".

### Pattern: work-on-the-side, then no-throw commit

```cpp
void GoodVec::push_back(const T& x) {
    if (size_ < cap_) { construct(data_+size_, x); ++size_; return; }

    size_t ncap = cap_ ? cap_*2 : 4;
    RawBuf nb(ncap);                                  // side buffer (RAII)
    for (size_t i=0;i<size_;++i) { construct(nb.p+i, data_[i]); ++nb.built; }
    construct(nb.p+size_, x); ++nb.built;             // <-- koi bhi throw yahan tak?
                                                     //     nb ka dtor rollback karega,
                                                     //     *this UNTOUCHED
    destroy(data_, size_);                            // ---- COMMIT (sab noexcept) ----
    data_ = nb.release();
    cap_  = ncap;
    ++size_;
}
```

Jab tak "commit" line nahi aayi, `*this` ke members chhue hi nahi. Commit ke steps
(dtor calls, pointer assignment, int increment) **sab `noexcept`** — isliye commit
kabhi aadha nahi rehta.

### Pattern: copy-and-swap (assignment ke liye)

```cpp
class Buffer {
    friend void swap(Buffer& a, Buffer& b) noexcept { /* pointers/ints exchange */ }

    Buffer& operator=(Buffer rhs) noexcept {   // <-- rhs BY VALUE (copy pehle)
        swap(*this, rhs);                      // <-- noexcept swap
        return *this;
        // purana *this data ab `rhs` ke saath, scope-end pe destruct
    }
};
```

- Parameter **by value** → copy call site pe banti hai. Copy throw kare → `*this`
  ko haath tak nahi lagaya → strong guarantee, muft.
- Copy ban gayi → `swap` (noexcept) → ab fail ho hi nahi sakta.
- **Self-assignment** automatically safe (`x = x` → copy pehle banti hai).
- Ek chhota cost: kabhi-kabhi ek extra move. 99% code mein worth it.

---

## No-throw (`noexcept`) — foundation

Strong/basic guarantees **no-throw building blocks** pe khade hain:

- **`swap`** — pointers/ints exchange, kabhi throw nahi. Har resource-owning type
  ke liye ek `noexcept` `swap` do.
- **Move constructor / move assignment** — `noexcept` hona chahiye. Warna
  `std::vector` reallocation pe **move ke bajaye copy** karega (neeche).
- **Destructors** — implicitly `noexcept`. Kabhi throw mat karo (file `04`).
- **Cleanup / rollback code** — `noexcept`.

### `std::vector` aur `noexcept` move — the famous one

`vector::push_back` **strong guarantee** deta hai. Reallocate karte waqt purane
elements ko naye buffer mein le jaana hai:

- Move constructor **`noexcept`** hai → `vector` **move** karta hai (fast). Agar
  koi move beech mein throw kare to strong guarantee toot jaati — par `noexcept`
  ne promise kiya woh nahi hoga.
- Move constructor `noexcept` **nahi** hai → `vector` majboori mein **copy** karta
  hai (slow, par copy fail hone pe purana buffer intact = strong guarantee bacha).

```cpp
struct S {
    S(S&&) noexcept;              // ✅ vector move karega — teardown fast
    S(S&&);                       // ❌ noexcept nahi -> vector COPY karega
};
```
`std::move_if_noexcept` yeh choice karta hai. **Apne move ops pe `noexcept` lagao.**

---

## Andar kya hota hai

- **Strong guarantee ki cost**: aksar ek extra allocation + copy/move (side
  buffer). `vector` growth already realloc karta hai, toh "strong" almost free.
  Ek `Account::apply` ke liye poora object copy karna mehnga — wahan "basic" chuno.
- **`noexcept` ka optimizer effect**: `noexcept` function ke around compiler
  cleanup/unwind code emit nahi karta → chhota code, better inlining. `noexcept`
  violate hua → `std::terminate` (koi unwinding nahi).
- **`swap` noexcept**: teen `mov` (pointers) + teen `mov` (sizes). Compiler aksar
  register shuffle mein badal deta.

---

## > **HFT relevance**
> `-fno-exceptions` codebase mein "exception safety" ka roop badalta hai: ab sawaal
> hai **"early `return Err::x` pe object valid rehta hai?"** — wahi discipline,
> alag trigger. Side-buffer + commit pattern, aur "fail hone se pehle shared state
> mat chhuo" — dono waise hi lagoo.
>
> **`noexcept` move constructors** har hot type pe — warna `std::vector<Order>`
> ya `std::vector<PriceLevel>` grow hone pe elements ko *copy* karega, jo ek
> avoidable latency spike hai. `static_assert(std::is_nothrow_move_constructible_v
> <Order>);` daal do.
>
> **`noexcept` swap** har resource type pe — pool handles, buffers, ring cursors —
> taaki "reset to known-good" ek atomic-feeling, fail-free operation ho.

---

## Hands-on

```bash
./build.ps1 23-ERROR-HANDLING/examples/02_exception_safety.cpp
```

`BadVec` (leak on throw, live counter se DIKHTA hai), `GoodVec` (RawBuf rollback,
counter 0 pe), copy-and-swap `Buffer`. Phir `GoodVec` ke `RawBuf` ka `if (!p)
return;` guard hata ke dekho kya hota hai (release ke baad stale `built` pe dtor).

---

## ⚠️ Traps

### Trap 1 — raw resource bina RAII holder ke, throw ke beech
```cpp
T* tmp = new T[n];
risky_fill(tmp);          // ⚠️ throw -> tmp leak
data_ = tmp;
```
`auto tmp = std::make_unique<T[]>(n);` … `data_ = tmp.release();`.

### Trap 2 — "strong" claim karna, par commit throw kar sakta
```cpp
void set(X v) {
    validate(v);          // ok
    a_ = compute_a(v);    // ⚠️ agar compute_b throw kare, a_ already badal chuka
    b_ = compute_b(v);
}
```
Dono compute pehle (locals mein), phir dono assign (noexcept). Ya copy-and-swap.

### Trap 3 — move constructor `noexcept` bhoolna
```cpp
struct Order { std::string sym; std::vector<Leg> legs; Order(Order&&); };
// noexcept nahi -> std::vector<Order> realloc pe COPY karega
```
`Order(Order&&) noexcept = default;` (agar members noexcept-movable hain).

### Trap 4 — `swap` jo throw kar sakta
```cpp
friend void swap(Big& a, Big& b) { Big t = a; a = b; b = t; }   // ⚠️ copies -> throw
```
Member-wise `std::swap` of pointers/handles, `noexcept`.

### Trap 5 — self-assignment
```cpp
Buffer& operator=(const Buffer& o) {
    delete[] p_;                 // ⚠️ x = x -> apna hi buffer uda diya
    p_ = new char[o.n_]; ...
}
```
Copy-and-swap se yeh problem hi nahi hota.

### Trap 6 — over-engineering "strong" jahan "basic" kaafi
Har setter ke liye poora object clone karna = latency + memory. Decide per
operation.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "exception-safe = `try/catch` laga do" | Guarantee levels ke baare mein socho; aksar RAII + ordering, `catch` nahi |
| "har function strong guarantee de" | Kam se kam **basic**. Strong jahan sasta |
| "basic guarantee = state unchanged" | Nahi — state *valid* aur leak-free; kaunsa valid state undefined |
| "`noexcept` bas ek hint hai" | Contract — violate = `terminate`; aur optimizer/`vector` ispe depend karte |
| "copy-and-swap slow hai" | Ek extra move aksar; strong guarantee + self-assign safety muft |

---

## Exercises

1. **Level batao:**
   ```cpp
   void Stack::push(const T& x) {
       T* p = alloc_.allocate(size_+1);
       std::uninitialized_copy(data_, data_+size_, p);
       p[size_] = x;
       alloc_.deallocate(data_, size_);
       data_ = p; ++size_;
   }
   ```

   <details><summary>Answer</summary>

   Buggy. `uninitialized_copy` ya `p[size_] = x` throw kare to `p` leak (aur jo
   elements `p` mein bane unke dtors nahi) → **basic guarantee bhi nahi**. RAII
   holder + `uninitialized_copy`'s own rollback + noexcept commit chahiye → phir
   **strong**.
   </details>

2. **Make it strong:** `void Config::merge(const Config& other)` jo `other` ke
   keys is object mein daalta hai. Abhi basic hai (aadha merge ho sakta). Strong
   kaise?

   <details><summary>Answer</summary>

   `Config tmp = *this; tmp.merge_basic(other); swap(*this, tmp);` — copy pe kaam,
   phir noexcept swap se commit. Copy/merge throw kare → `*this` untouched.
   </details>

3. **`noexcept` impact:** `std::vector<Widget>` jisme `Widget(Widget&&)` `noexcept`
   *nahi* hai — 1M elements, `push_back` ne realloc trigger kiya. Kya hota hai,
   kitna slow?

   <details><summary>Answer</summary>

   `vector` `std::move_if_noexcept` se decide karta — noexcept nahi → 1M **copies**
   (har `Widget` ka deep copy: string dup, vector dup) instead of 1M cheap moves.
   10–100x slower realloc, plus 2x peak memory. Fix: `noexcept` move.
   </details>

4. **Copy-and-swap self-assign:** `x = x` ko step-by-step trace karo copy-and-swap
   assignment ke saath. Kyun safe?

   <details><summary>Answer</summary>

   `operator=(Buffer rhs)` — `rhs` `x` ka copy hai (alag buffer). `swap(*this=x,
   rhs)` — `x` ab `rhs` ka data (jo `x` jaisa hi tha), `rhs` ab purana `x` data,
   scope-end pe `rhs` destruct. `x` intact, koi self-destruction nahi.
   </details>

5. **Design call:** `MatchingEngine::process(Order)` — har order pe poora book
   clone karna strong guarantee ke liye = unacceptable latency. Kya karo?

   <details><summary>Answer</summary>

   Basic guarantee: order apply karne se pehle **saare validations** (symbol,
   price band, risk, self-trade) — jo fail ho sakta hai woh mutation se pehle.
   Mutation steps khud noexcept (pre-allocated levels, intrusive list splice).
   Agar mutation ke beech kuch throw *kar sakta* hai, woh design bug hai — hot
   path allocation-free hona chahiye.
   </details>

---

## Interview questions

1. Chaar exception safety levels — define karo, ek-ek example.
2. Strong guarantee dene ke do idioms.
3. Copy-and-swap: parameter by value kyun? Self-assignment kaise handle hota?
4. Move constructor `noexcept` na ho to `std::vector` realloc pe kya?
5. Kaunse operations `noexcept` hone hi chahiye aur kyun?
6. "Basic guarantee" exactly kya promise karta — aur kya nahi?
7. Ek function ko basic se strong banane ki typical cost?

---

## Next
→ [`06-noexcept.md`](06-noexcept.md)

# 05 — Temporaries & lifetime extension

## Prerequisites
- `02-object-lifetime.md`, `18-COPY-MOVE` (value categories, prvalue)
- [`examples/03_temporaries.cpp`](examples/03_temporaries.cpp)

## Yeh topic abhi kyun
Temporary objects har jagah hote hain — `f() + g()`, `std::string("x")`, function
ka by-value return. Default rule: **temporary marta hai full-expression (`;`) ke
end pe.** `const T&` / `T&&` se ek local binding usse extend kar sakta — par sirf
kuch cases mein. Baaki cases mein aapko ek **dangling reference** milta hai, jo
sabse ganda "works in dev" bug hai.

---

## Temporary kya hai, kab banta

**Temporary** = ek unnamed object jo ek expression evaluate karte waqt materialize
hota. Sources:
- Literal-ish: `std::string("hello")`, `Widget{}`, `Point{1, 2}`.
- Function ka **by-value return**: `make_widget()`.
- Implicit conversions: `foo(42)` jab `foo(const std::string&)` — `42`... nahi,
  `foo("42")` → `const char*` → `std::string` temporary.
- Arithmetic sub-results: `a + b + c` mein `(a + b)`.
- Cast to a prvalue: `static_cast<Widget>(x)`.

C++17 se: **temporary tabhi materialize hota jab uski storage genuinely chahiye**
(guaranteed copy elision — folder 22). `Widget w = Widget{};` mein koi temporary
nahi — `w` seedha usi jagah banta.

---

## Default rule: end of the full-expression

```cpp
std::printf("%d\n", make_loud().value());   // Loud temp banta, .value() call hota,
                                            // phir ~Loud — sab is ; se pehle
```

**Full-expression** = statement ka poora expression (`;` tak), ya ek `for`/`if`
init/condition, etc. Us point pe saare temporaries destruct hote — **reverse order
of construction**.

```cpp
process(build_a(), build_b());   // dono temps ; pe destruct (b pehle, a baad)
```

---

## Lifetime extension — kab hoti hai

Jab ek **`const T&` ya `T&&`** ek temporary ko **directly bind** kare (ek local
variable, ya ek function parameter ke through nahi), to temporary us reference ke
**scope tak** jeeta hai:

```cpp
const Loud& r = make_loud();       // temp ~Loud yahan NAHI — r ke scope tak jeeta
r.value();
// ... scope end pe ~Loud
```

**Transitive:** ek temporary ke **subobject** ka reference bhi poore temporary ko
extend karta:

```cpp
const int& v = make_loud().id;     // poora Loud temp extend hota (id iske andar hai)
```

**`auto&&`** bhi extend karta (forwarding ref binding a prvalue):

```cpp
auto&& x = make_loud();            // extends
```

---

## Lifetime extension — kab NAHI hoti (dangling)

### 1. Function return se pass hone pe
Extension **sirf** direct local binding pe. Ek `const T&` parameter jo function se
`return` ho — temporary caller ke full-expression tak hi jeeta:

```cpp
const Loud& pick(const Loud& x) { return x; }
const Loud& r = pick(make_loud());   // ⚠️ temp ; pe marta; r DANGLES
```

### 2. Constructor member-init se pass hone pe
Reference **member** ko ek ctor argument temporary se bind karo — extension NAHI:

```cpp
struct Holder {
    const Loud& ref;
    Holder(const Loud& r) : ref(r) {}   // extension apply NAHI hoti
};
Holder h(make_loud());   // ⚠️ temp ; pe marta; h.ref DANGLES
```

### 3. `std::string_view` / pointer INTO a temporary

```cpp
std::string_view sv = get_string();   // ⚠️ temp std::string ; pe marta; sv points at freed memory
const char* p = get_string().c_str(); // ⚠️ same
```

`string_view` ek reference-jaisa hai (pointer + length) par ek **object**, isliye
extension rules apply nahi hote — temporary bas mar jaata.

### 4. Range-for over a temporary's subobject (C++20 mein bug, C++23 mein fixed)

```cpp
for (int x : make_bag().items)   // ⚠️ C++20: Bag temp destroyed BEFORE the loop body
    use(x);                       //    C++23: fixed (temp extended for the loop)
```

Safe har standard mein: `auto bag = make_bag(); for (int x : bag.items) ...`.

### 5. Ternary / comma producing a reference to a temp

```cpp
const std::string& s = cond ? std::string("a") : some_lvalue;   // ⚠️ subtle — one branch is a temp
```

---

## Andar kya hota hai

- Compiler har full-expression ke end pe us expression mein banaye temporaries ke
  dtors emit karta (reverse order). Yeh code literally `;` ke baad.
- Lifetime extension → temporary ko ek hidden local slot mein rakha jata (naye
  scope ka part), aur uska dtor us scope ke end pe emit hota — jaise woh ek named
  local ho.
- Non-extension cases → temporary ka slot full-expression ke end pe reclaim, uska
  dtor wahin. Aapka `const T&` ab freed slot ko point karta → dangling.
- `-Wdangling-reference` (GCC 13+), `-Wreturn-stack-address`, `-Wdangling-gsl`
  (Clang, for `string_view`/`span`) — kuch cases pakadte, sab nahi. **ASan / life
  extension in `-fsanitize=address` — best runtime catch.**

---

## > **HFT relevance**
> - **`std::string_view` / `std::span` discipline** — inhe kabhi ek temporary se
>   mat banao. Ek function jo `std::string_view` return kare, uska backing storage
>   caller-owned aur alive hona chahiye. Ye types HFT mein zero-copy parsing ke
>   liye ubiquitous hain — dangling `string_view` ek common, hard-to-spot bug.
> - **Popped queue entries / recycled slots** — ek reference jo ek ring-buffer
>   slot ko point karta jo overwrite ho gaya = same class of bug (file 02).
> - **`-Wdangling-reference` on, ASan in CI** — inhe compile/test time pe pakdo,
>   production mein nahi.
> - **Prefer values on the stack for small hot types** — ek `Price` ya `Qty` by
>   value return/pass, no reference-lifetime puzzle.

---

## Hands-on

```bash
./build.ps1 25-OBJECT-MODEL/examples/03_temporaries.cpp
```

Example: full-expression rule, `const T&` extension, transitive extension, aur 4
non-extension cases — dekho `~Loud` **kab** print hota (dangling cases mein "still
using r" se pehle). Phir:
- `-fsanitize=address` (Linux) se rebuild — dangling accesses ab loud errors.
- `std::string_view sv = std::string("x") + "y";` likho aur `sv` print karo —
  `-O0` "kaam kar sakta", `-O2` ya ASan pe garbage/error.

---

## ⚠️ Traps

### Trap 1 — `string_view` from a temporary
```cpp
std::string_view name = build_name();   // ⚠️ dangles
std::string name = build_name(); std::string_view v(name);   // ✅
```

### Trap 2 — returning a `const T&` parameter
```cpp
const T& id(const T& x) { return x; }
const T& r = id(make_t());   // ⚠️ dangles (no extension through the return)
```

### Trap 3 — reference member from a ctor arg temporary
Reference members don't extend ctor-argument temporaries. Store by value, or ensure
the referent outlives the object (and document it).

### Trap 4 — `auto` vs `auto&&` for a temporary
```cpp
auto x   = make_big();   // moves/copies — safe, but a copy
auto&& x = make_big();   // extends the temporary — safe, no copy  ✅
const auto& x = make_big();   // also extends  ✅
```

### Trap 5 — C++20 range-for over `getObj().member`
Materialize first: `auto o = getObj(); for (auto& e : o.member) ...`.

### Trap 6 — chained temporaries in one expression
`a().b().c()` — each returns by value? Each temp lives to the `;`. If `b()` returns
a reference into `a()`'s temp, and you keep it past the `;`, dangling.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "temporary lives until it's 'done being used'" | Until the end of the full-expression (`;`), unless extended |
| "`const T&` always extends a temporary" | Only a *direct local* binding; not through a return or a member-init |
| "`string_view` extends like `const string&`" | It's an object, not a reference — no extension; it just dangles |
| "range-for over `f().c` is fine" | C++20: the temp dies before the body. C++23 fixed. Materialize first |
| "`-Wdangling-reference` catches all of these" | Catches some; ASan is the reliable runtime catch |
| "elision means no temporaries anywhere" | Guaranteed elision removes *some*; sub-expressions still make temps |

---

## Exercises

1. **When does `~X` run?**
   ```cpp
   struct X { ~X(){ std::puts("~X"); } int v(){ return 1; } };
   int a = X{}.v();
   std::puts("after");
   ```

   <details><summary>Answer</summary>

   `~X` then `after`. The `X{}` temporary is destroyed at the end of the
   full-expression (the `;` after `.v()`), before `"after"`.
   </details>

2. **Dangle or not:** (a) `const std::string& r = std::string("x");`
   (b) `const char* p = std::string("x").c_str();`
   (c) `std::string_view sv = std::string("x");`
   (d) `auto&& v = make_vector();`

   <details><summary>Answer</summary>

   (a) OK — direct local `const&` binding extends the temporary to `r`'s scope.
   (b) Dangles — the temporary dies at the `;`, `p` points at freed memory.
   (c) Dangles — `string_view` is an object, no extension.
   (d) OK — `auto&&` binding a prvalue extends it.
   </details>

3. **Fix the API:** `std::string_view trim(std::string_view s);` called as
   `auto t = trim(get_config_line());`. What's wrong, two fixes.

   <details><summary>Answer</summary>

   `get_config_line()` returns a temporary `std::string` (say); `trim` returns a
   `string_view` into it; the temporary dies at the `;` → `t` dangles. Fixes:
   (1) `std::string line = get_config_line(); auto t = trim(line);` — keep the
   backing string alive. (2) Have `trim` (or the pipeline) return an owning
   `std::string`.
   </details>

4. **Member reference:** `struct Cache { const Table& t; Cache(const Table& tbl) :
   t(tbl) {} };` — when is `Cache c(build_table());` a bug, and when is it fine?

   <details><summary>Answer</summary>

   Bug when `build_table()` returns a temporary (dies at the `;`, `c.t` dangles).
   Fine when you pass a `Table` that genuinely outlives `c` (a long-lived member
   or global) — and you should document that precondition. Safer: store `Table t;`
   by value.
   </details>

5. **Range-for:** rewrite `for (auto& kv : load_map()) use(kv);` to be safe in
   C++20.

   <details><summary>Answer</summary>

   `auto m = load_map(); for (auto& kv : m) use(kv);` — materialize the map into a
   named local so it outlives the loop. (In C++23 the original is also fine, but
   the explicit form is safe everywhere.)
   </details>

---

## Interview questions

1. Temporary object kab banta, kab marta (default rule)?
2. Lifetime extension — kis reference se, kis scope tak?
3. Extension NAHI hoti — 3 cases (return, member-init, `string_view`).
4. `std::string_view` from a temporary — kyun dangles?
5. `auto` vs `auto&&` vs `const auto&` for `auto x = make_big()` — semantics.
6. C++20 range-for over `f().member` — kya bug, C++23 mein kya badla?
7. Transitive extension — subobject reference se poora temporary kaise extend hota?

---

## Next
→ [`06-trivial-standard-layout-pod.md`](06-trivial-standard-layout-pod.md)

# 10 — `std::expected<T, E>` (C++23)

## Prerequisites
- `01-error-handling-strategies.md`, `09-no-exceptions-hft.md`
- `22-MODERN-CPP` file 03 (`std::optional`/`variant`)
- [`examples/05_expected.cpp`](examples/05_expected.cpp)

## Yeh topic abhi kyun
`std::expected<T, E>` woh missing piece hai: error ko **value** ki tarah lautao,
`std::optional` ki tarah lightweight, par **"kyun fail hua"** bhi carry kare. No
`throw`, no hidden control flow, no out-param gymnastics. Monadic operations se
deep pipelines mein error forwarding automatic ho jaata. Rust ka `Result<T, E>`,
Haskell ka `Either` — same idea, ab standard C++ mein.

> Toolchain note: `<expected>` ko **`-std=c++23`** chahiye. Repo default `-std=c++20`
> hai, isliye `examples/05_expected.cpp` ek chhota `Expected<T,E>` khud rakhta hai
> (same API) jab `__cpp_lib_expected` undefined ho. C++20 codebases practically
> `tl::expected` (single header) ya `Boost.Outcome` use karte hain.

---

## Shakl

```cpp
#include <expected>

std::expected<int, ParseErr> parse_int(std::string_view s);

auto r = parse_int(tok);
if (r) {                    // has value?
    use(*r);                // *r / r.value()  -> T
} else {
    log(r.error());         // r.error()       -> E
}
```

- `std::expected<T, E>` — andar **ya `T` ya `E`** (union-like), plus ek bool
  "which". `optional<T>` + reason.
- `r.has_value()` / `explicit operator bool` — success?
- `*r`, `r.value()` — `T` (`value()` khali pe `std::bad_expected_access<E>` throw
  karta hai; `*r` UB agar khali).
- `r.error()` — `E` (valid sirf jab `!r`).
- `r.value_or(fallback)` — `T` ya `fallback`.

Error banana:

```cpp
#include <expected>
std::expected<int, ParseErr> parse_int(std::string_view s) {
    if (s.empty()) return std::unexpected(ParseErr::empty);   // E branch
    ...
    return n;                                                 // T branch (implicit)
}
```

`std::unexpected<E>` ek chhota wrapper hai jo `expected` ko batata "yeh `E` hai,
`T` nahi" (kyunki `T` aur `E` same type bhi ho sakte hain).

---

## Monadic operations — plumbing khatam

Deep chain mein har step `expected` lautata hai. Bina monadic ops:

```cpp
auto a = parse_int(s);
if (!a) return std::unexpected(a.error());
auto b = validate(*a);
if (!b) return std::unexpected(b.error());
auto c = scale(*b);
...
```

Monadic ops se:

```cpp
return parse_int(s)
     . and_then(validate)                 // E -> E passthrough; T -> validate(T)
     . transform([](long v){ return v*5; })   // T -> U (wrap back)
     . or_else(recover);                   // E -> expected<T,E> (recover/translate)
```

| Op | Signature of `f` | `expected` hai → | `unexpected` hai → |
|---|---|---|---|
| `and_then(f)` | `T -> expected<U, E>` | `f(value)` | error passthrough |
| `transform(f)` | `T -> U` | `expected<U,E>{ f(value) }` | error passthrough |
| `or_else(f)` | `E -> expected<T, G>` | value passthrough | `f(error)` |
| `transform_error(f)` | `E -> G` | value passthrough | `expected<T,G>{ unexpected{f(error)} }` |

- **`and_then`** — chain a step that can *also* fail.
- **`transform`** — chain a step that *can't* fail (pure function).
- **`or_else`** — handle/recover/translate the error.
- **`transform_error`** — map error type (e.g. low-level `E` → domain `DomErr`).

Yeh Rust ke `?` operator + `.map()` / `.and_then()` / `.map_err()` ka barabar.

---

## `E` kya hona chahiye

| `E` choice | Kab |
|---|---|
| `enum class` | Chhota, fixed set of reasons. **Sabse common, sabse cheap** (ek int) |
| `std::error_code` | Portable, rich, interop with STL / OS errors (file `11`) |
| `struct { Code c; SourceLoc where; small_string detail; }` | Context chahiye (kaunsa field, kahan) |
| `std::string` | ❌ aksar galat — allocation on error path, no programmatic handling |

**Chhota, cheap-to-copy, trivially-destructible `E` chuno** — error path ko bhi
allocation-free / branch-friendly rakhne ke liye. `enum class` default.

---

## `expected<void, E>` — "fail ho sakta hai, lautata kuch nahi"

```cpp
std::expected<void, WriteErr> flush();       // success = void

auto r = flush();
if (!r) handle(r.error());
// ya: return flush().and_then([]{ return next(); });
```

`bool`/`Status`-returning function ka type-safe roop — ignore nahi kar sakte,
compose ho jaata hai.

---

## `expected` vs alternatives

| | ignore-proof | reason carry | alloc-free | compose | pre-C++23 |
|---|---|---|---|---|---|
| `bool` / `int` code | `[[nodiscard]]` | ❌ (bool) / weak | ✅ | ❌ | ✅ |
| `enum class` code | `[[nodiscard]]` | ✅ | ✅ | ❌ | ✅ |
| `std::optional<T>` | `[[nodiscard]]` | ❌ | ✅ | partial (`and_then`) | ✅ (C++17) |
| **`std::expected<T,E>`** | `[[nodiscard]]` | ✅ | ✅ (if `E` cheap) | ✅ (monadic) | ❌ (use `tl::expected`) |
| exceptions | can't ignore | ✅ | ❌ (`__cxa_allocate`) | ✅ (auto-propagate) | ✅ |

`expected` = "return code" ki predictability + exception ki compose-ability, bina
throw cost.

---

## Andar kya hota hai

- `std::expected<T,E>` ka layout: roughly `union { T; E; } + bool` — `sizeof` ≈
  `max(sizeof(T), sizeof(E))` + 1 (+ padding). `T=int, E=enum` → 8 bytes typical.
- Koi heap nahi (jab tak `T`/`E` khud allocate na karein). Return value register(s)
  ya caller ke slot mein — bilkul ek struct return jaisa.
- `and_then` / `transform` inline ho jaate `-O2` pe — final code ~ waisa jaisa
  aap haath se `if (!x) return x.error();` likhte. Zero abstraction cost.
- `value()` ka throw path (`bad_expected_access`) sirf tab compile hota jab aap
  `value()` call karo — `*r` / `and_then` use karo to koi exception machinery nahi.

---

## > **HFT relevance**
> `std::expected` (ya `tl::expected` pre-C++23) hot-path error handling ka **modern
> default** hai `-fno-exceptions` codebases mein. `E = enum class RejectReason :
> uint8_t` → 2-byte error, ek register, branch-predictable, koi alloc.
>
> ```cpp
> std::expected<OrderId, RejectReason> Gateway::submit(const NewOrder& o) noexcept {
>     if (!risk_.check(o))          return std::unexpected(RejectReason::risk);
>     if (!book_.price_in_band(o))  return std::unexpected(RejectReason::band);
>     return oms_.enqueue(o);       // expected<OrderId, RejectReason>
> }
> ```
>
> `and_then` chains se validation pipelines flat rehte hain, har stage apna reject
> reason deta hai, aur `-O2` pe yeh sab ek `if`-ladder mein compile hota — koi
> overhead vs manual. `transform_error` se internal error codes ko client-facing
> FIX reject codes mein map karo boundary pe.
>
> Ek dhyaan: `value()` mat use karo hot path pe (throw path pull karta hai);
> `*r` / `has_value()` / monadic use karo.

---

## Hands-on

```bash
./build.ps1 23-ERROR-HANDLING/examples/05_expected.cpp                  # C++20: mini Expected
g++ -std=c++23 -O2 -Wall -Wextra 23-ERROR-HANDLING/examples/05_expected.cpp -o exp && ./exp   # real std::expected
```

Example: `parse_int` → `validate_price` → `transform` pipeline, `value_or`,
`or_else`. Dono modes same output. Phir:
- Ek `transform_error` step add karo jo `ParseErr` ko ek `int` HTTP-jaise code
  mein badle.
- `expected<void, ParseErr>` wala ek `check_all(span<string_view>)` likho jo pehli
  bad token pe `unexpected` de.

---

## ⚠️ Traps

### Trap 1 — `value()` bina check
```cpp
long v = parse_int(s).value();   // ⚠️ error pe std::bad_expected_access throw
                                 //    (aur -fno-exceptions mein terminate)
if (auto r = parse_int(s)) v = *r; else ...   // ✅
```

### Trap 2 — `E = std::string`
```cpp
std::expected<int, std::string> f();   // ⚠️ har error pe allocation
enum class FErr { ... };                // ✅
```

### Trap 3 — `return {};` ambiguity intent
```cpp
std::expected<int,Err> f() { return {}; }   // yeh T{} (value 0) hai, error NAHI
return std::unexpected(Err::x);              // error ke liye explicit
```

### Trap 4 — `T` aur `E` same type, bina `unexpected`
```cpp
std::expected<int, int> f() { return 5; }             // value 5
std::expected<int, int> g() { return std::unexpected(5); }   // error 5
```
`std::unexpected` isi ambiguity ke liye hai.

### Trap 5 — `and_then` vs `transform` mix
```cpp
.transform(validate)   // ⚠️ validate returns expected<T,E> -> transform se expected<expected<T,E>,E>
.and_then(validate)    // ✅ validate can fail -> and_then
```
Fail kar sakta → `and_then`. Pure/infallible → `transform`.

### Trap 6 — `[[nodiscard]]` bhoolna
`std::expected` khud `[[nodiscard]]`-friendly hai par apne function pe bhi
`[[nodiscard]]` lagao taaki `f();` (result ignore) warn kare.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`expected` = `optional` with extra step" | `optional` "kyun nahi" nahi batata; `expected` batata (`E`) |
| "`expected` slow, it's a wrapper" | `-O2` pe manual `if`-ladder jitna; zero overhead |
| "`E` kuch bhi ho sakta, `std::string` daal do" | Chhota, cheap, trivially-destructible `E` (enum) — error path bhi fast |
| "`and_then` aur `transform` same" | `and_then`: f can fail. `transform`: f pure |
| "`value()` safe hai" | Khali pe throws (`bad_expected_access`) — `*r` / check first |
| "`return {};` returns an error" | Woh `T{}` value hai; error = `std::unexpected(...)` |

---

## Exercises

1. **Signature:** `read_config(path) ->` — fail modes: file missing, permission,
   bad TOML syntax (with line no.), missing required key. `T` = `Config`. `E` = ?

   <details><summary>Answer</summary>

   `E` = a small struct: `struct ConfigErr { Kind kind; int line = -1; std::string_view
   key; };` (or `enum Kind` + separate detail). Not `std::string` — you want the
   line number and key programmatically. `std::expected<Config, ConfigErr>`.
   </details>

2. **Rewrite monadic:**
   ```cpp
   auto a = fetch(url); if (!a) return std::unexpected(a.error());
   auto b = parse(*a);  if (!b) return std::unexpected(b.error());
   auto c = index(*b);  if (!c) return std::unexpected(c.error());
   return summarize(*c);   // returns plain Summary, cannot fail
   ```

   <details><summary>Answer</summary>

   `return fetch(url).and_then(parse).and_then(index).transform(summarize);`
   (`summarize` can't fail → `transform`; the rest → `and_then`.)
   </details>

3. **`void` version:** `apply_update(Update) -> expected<void, ApplyErr>`. Caller
   ek loop mein updates apply karta hai, pehli error pe ruk ke usse return karta
   hai. Sketch.

   <details><summary>Answer</summary>

   ```cpp
   std::expected<void, ApplyErr> apply_all(std::span<const Update> ups) {
       for (const auto& u : ups)
           if (auto r = apply_update(u); !r) return r;   // propagate
       return {};   // success (void)
   }
   ```
   </details>

4. **Layout:** `sizeof(std::expected<std::uint32_t, MyEnum>)` jahan `MyEnum : uint8_t`.
   Approx?

   <details><summary>Answer</summary>

   `union { uint32_t; uint8_t; }` = 4 bytes + 1 discriminant bool + padding to
   align → **8 bytes**. No heap. Returned in a register pair / small struct.
   </details>

5. **`transform_error`:** internal `enum class DbErr` ko client-facing `int`
   error code mein map karna hai boundary pe. Ek line.

   <details><summary>Answer</summary>

   `return do_query(q).transform_error([](DbErr e){ return to_client_code(e); });`
   → result type `std::expected<Rows, int>`.
   </details>

---

## Interview questions

1. `std::expected<T,E>` vs `std::optional<T>` — kab kaunsa?
2. `std::unexpected` kis liye zaroori (hint: `T == E`)?
3. `and_then` vs `transform` vs `or_else` vs `transform_error`.
4. `E` ke liye achhi choice kya, `std::string` kyun aksar galat?
5. `std::expected` ki runtime cost `-O2` pe (vs manual error forwarding)?
6. `.value()` ka danger, safe alternatives.
7. `expected<void, E>` ka use case.
8. C++20 codebase mein `std::expected` na ho to kya (`tl::expected` / Outcome)?

---

## Next
→ [`11-error-codes.md`](11-error-codes.md)

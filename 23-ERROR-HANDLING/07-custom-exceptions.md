# 07 — Apni exception hierarchy

## Prerequisites
- `03-exceptions-basics.md`, `16-OOP` (inheritance, virtual)
- `18-COPY-MOVE` (copy ctor `noexcept`)

## Yeh topic abhi kyun
Jab aapko exceptions use karni hain (startup, config, library boundaries), to
`throw std::runtime_error("...")` har jagah kaafi nahi — caller ko **type** se
distinguish karna hota hai ("config error alag se handle karo, network error alag
se"), aur structured data chahiye (kaunsa field, kaunsa error code). Ek chhoti,
soch-samajh ke banayi hui hierarchy yeh deti hai — bina over-engineering.

---

## Base: hamesha `std::exception` se derive

```cpp
#include <exception>
#include <stdexcept>
#include <string>

// Poore project ka ek root — caller ek hi catch mein "meri koi bhi error" pakad sake
class AppError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;   // string/const char* ctors inherit
};
```

- `std::exception` — root. Sirf `virtual const char* what() const noexcept;`.
- `std::runtime_error` / `std::logic_error` — inhone `what()` implement kar diya
  hai (message ko `std::string` mein store karte, ref-counted copy). Inse derive
  karo to `what()` **muft**.
- **`using Base::Base;`** — base ke constructors inherit. Ab
  `throw AppError("msg");` chalta hai bina ctor likhe.

⚠️ Seedha `std::exception` se derive karke `what()` khud implement karoge to
message ko kahan store karo yeh aapka sirdard — `std::string` member rakha aur
`what()` mein `.c_str()` diya, par phir **copy ctor throw kar sakta** (string
alloc). `runtime_error` yeh already solve kar chuka (COW/ref-counted). Isliye
99% cases: `runtime_error` se derive.

---

## Ek chhoti hierarchy

```cpp
class AppError : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

class ConfigError : public AppError {
public:
    ConfigError(std::string file, std::string key, std::string why)
        : AppError("config " + file + " [" + key + "]: " + why)
        , file_(std::move(file)), key_(std::move(key)) {}

    const std::string& file() const noexcept { return file_; }
    const std::string& key()  const noexcept { return key_; }
private:
    std::string file_, key_;
};

class NetworkError : public AppError {
public:
    NetworkError(std::string endpoint, std::error_code ec)
        : AppError("network " + endpoint + ": " + ec.message())
        , endpoint_(std::move(endpoint)), code_(ec) {}

    std::error_code code() const noexcept { return code_; }
private:
    std::string endpoint_;
    std::error_code code_;
};
```

Caller:

```cpp
try {
    boot();
} catch (const ConfigError& e) {
    std::fprintf(stderr, "bad config: %s (%s / %s)\n",
                 e.what(), e.file().c_str(), e.key().c_str());
    return 2;
} catch (const NetworkError& e) {
    if (e.code() == std::errc::connection_refused) return retry_later();
    return 3;
} catch (const AppError& e) {                 // meri koi bhi baaki error
    std::fprintf(stderr, "fatal: %s\n", e.what());
    return 1;
} catch (const std::exception& e) {           // library / STL
    std::fprintf(stderr, "unexpected: %s\n", e.what());
    return 1;
}
```

---

## Design rules

1. **Ek project root** (`AppError`) — taaki "meri sab errors" ek catch se pakdein,
   aur third-party/STL errors se alag rahein.
2. **Utni hi types jitni caller sach mein alag-alag handle karega.** `ConfigError`
   aur `NetworkError` alag kyunki recovery alag. `MissingKeyError` vs
   `BadSyntaxError` — sirf tab jab caller inpe alag react karega, warna ek
   `ConfigError` + enum field.
3. **Data ko members mein rakho** (`file_`, `key_`, `code_`) — sirf `what()` string
   mein sab kuch concat karke mat chhodo; programmatic access chahiye hota.
4. **Message base ko de do** (`runtime_error(std::string)`), khud store mat karo.
5. **`final`** laga do agar aage derive nahi karna (`class ConfigError final : ...`)
   — devirtualization + intent.
6. **Copy ctor throw na kare** — `runtime_error` se derive + `std::string`/
   `std::error_code` members = ok (string copy COW/small; ec trivial).

---

## `throw` karte waqt: object banao, catch by `const&`

```cpp
throw ConfigError(path, "max_rate", "expected integer");   // temporary — move/copy into EH storage
```

- Thrown object EH ki storage mein copy/move hota hai. Isliye thrown type
  **copyable** (ya at least movable) hona chahiye, aur woh copy ideally `noexcept`.
- Catch `const NetworkError&` — polymorphic, no slice.

---

## `what()` ko override karna (jab zaroori ho)

```cpp
class ParseError : public AppError {
public:
    ParseError(std::size_t line, std::size_t col, std::string msg)
        : AppError(build_(line, col, msg)), line_(line), col_(col) {}
    std::size_t line() const noexcept { return line_; }
    std::size_t col()  const noexcept { return col_; }
private:
    static std::string build_(std::size_t l, std::size_t c, const std::string& m) {
        return "parse error at " + std::to_string(l) + ":" + std::to_string(c) + ": " + m;
    }
    std::size_t line_, col_;
};
```

Message ko **ctor mein** ban a ke `AppError` ko de do (static helper se) — `what()`
ko override karke on-the-fly string banana risky hai (`what()` `noexcept` hai, aur
string alloc throw kar sakta).

---

## Nested exceptions — `std::throw_with_nested`

Low-level error ko high-level context ke saath wrap karo, dono preserve:

```cpp
try {
    read_file(path);                       // throws std::system_error
} catch (...) {
    std::throw_with_nested(ConfigError(path, "*", "load failed"));
}

// caller side:
void print_all(const std::exception& e, int depth = 0) {
    std::fprintf(stderr, "%*s%s\n", depth*2, "", e.what());
    try { std::rethrow_if_nested(e); }
    catch (const std::exception& inner) { print_all(inner, depth + 1); }
}
```

Deep pipelines mein "kya fail hua + kis context mein" dono milta hai.

---

## Andar kya hota hai

- `std::runtime_error` ka message: libstdc++ mein ek **reference-counted immutable
  string** (`_Rope`-jaisa nahi, par COW-ish) — isliye `runtime_error` ka copy ctor
  `noexcept` (bas refcount ++). Aapka derived type isse inherit karta hai jab tak
  aap koi throwing member add na karo.
- Har polymorphic exception type ka apna **vtable + `type_info`**. `catch` matching
  is `type_info` ko compare/walk karta hai (base chain). Zyada types = thoda zyada
  RTTI data, negligible.
- `throw ConfigError(...)` — temporary construct → `__cxa_allocate_exception` →
  move ctor us buffer mein → unwind. Members (`file_` etc.) usi object ke andar.

---

## > **HFT relevance**
> Custom exception hierarchy **startup / control-plane** code ke liye: config
> loading, gateway handshake, symbology download, strategy DLL load. Yahan aapko
> "kaunsa cheez fail hui, kis file/line/endpoint pe" chahiye taaki operator turant
> fix kar sake — aur latency matter nahi karti.
>
> **Hot path (data plane) mein yeh sab nahi.** Wahan error = `enum`/`expected`/
> `error_code`. Ek achha split: `namespace boot` exceptions use karta hai;
> `namespace engine` `noexcept` + error values. Jab `boot` `engine` ko call kare
> aur `engine` error value de, `boot` usse `throw ConfigError(...)` mein badal sakta
> hai (boundary pe translation).
>
> Agar poora binary `-fno-exceptions` hai to yeh file "jaan ne ke liye" hai —
> hierarchy ki jagah ek `struct Error { ErrCode code; SourceLoc where; small_string
> detail; };` value type banao jo waise hi structured info deta hai, bina `throw`.

---

## Hands-on

```bash
./build.ps1 23-ERROR-HANDLING/examples/01_exceptions_basics.cpp   # PriceError : std::runtime_error
```

Example ka `PriceError` dekho (`runtime_error` + extra `bad_price` field, catch
order mein sabse pehle). Banao: `AppError` → `{ConfigError, NetworkError}` teen
types, ek `boot()` jo teenon me se ek throw kare (arg se choose), aur `main` jo
alag-alag exit codes de. `std::throw_with_nested` add karke inner error bhi print
karo.

---

## ⚠️ Traps

### Trap 1 — `std::exception` se seedha derive + string member
```cpp
class E : public std::exception {
    std::string msg_;
public:
    E(std::string m) : msg_(std::move(m)) {}
    const char* what() const noexcept override { return msg_.c_str(); }
    // ⚠️ copy ctor implicit -> msg_ copy -> alloc -> throw during exception copy
};
```
`std::runtime_error` se derive karo — woh message storage solve kar chuka.

### Trap 2 — catch by value (hierarchy toh aur bhi bura)
```cpp
catch (AppError e)          // ⚠️ ConfigError -> AppError slice: file()/key() gaye
catch (const AppError& e)   // ✅
```

### Trap 3 — bahut zyada exception types
50 exception classes, har ek 2-line, koi caller distinguish nahi karta. Ek type +
`enum`/`error_code` field kaafi tha.

### Trap 4 — base handler pehle
```cpp
catch (const AppError& e) { ... }
catch (const ConfigError& e) { ... }   // ⚠️ dead
```
Derived → base order.

### Trap 5 — `what()` mein string build karna
```cpp
const char* what() const noexcept override {
    static thread_local std::string s = "err " + std::to_string(code_);  // ⚠️ alloc in noexcept
    return s.c_str();
}
```
Message ctor mein banao, base ko do.

### Trap 6 — exception ko across DLL/`.so` boundary throw karna (different runtime)
Do alag C++ runtimes (ya `/MT` vs `/MD`) ke beech exception throw = UB. Boundary
pe `extern "C"` + error code.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "har error ke liye ek naya exception class" | Sirf jab caller alag handle karega; warna 1 type + field |
| "`std::exception` se derive kar ke `what()` likh do" | `runtime_error`/`logic_error` se derive — `what()` + safe copy free |
| "saara info `what()` string mein daal do" | Structured data → members (`file()`, `code()`); string sirf human ke liye |
| "exception hierarchy = OOP, deep banao" | Flat + shallow. 2 levels kaafi (root + specific) |
| "exceptions sab jagah, hot path mein bhi" | Startup/boundary. Hot path = error values |

---

## Exercises

1. **Design:** ek REST client ke liye exception hierarchy. Errors: DNS fail,
   connect timeout, TLS handshake fail, HTTP 4xx, HTTP 5xx, malformed JSON
   response. Kaunse alag types, kaunse ek type + field?

   <details><summary>Answer</summary>

   Root `HttpError`. Shayad `TransportError` (DNS/connect/TLS — retry/backoff
   same) with an `enum Stage`. `HttpStatusError` (4xx vs 5xx — `int status()`;
   4xx no-retry, 5xx retry) as one type. `ResponseParseError` alag (bug/contract
   mismatch, no retry). Yaani ~3 types, fields carry the detail.
   </details>

2. **Fix:** `class DbError : public std::exception { std::string m_; public:
   DbError(std::string m):m_(move(m)){} const char* what() const noexcept
   override { return m_.c_str(); } };` — do improvements.

   <details><summary>Answer</summary>

   (1) `: public std::runtime_error` and `using std::runtime_error::runtime_error;`
   — drops the risky string member + gives safe copy. (2) If you need a query
   field, add e.g. `int sqlstate_` + `int sqlstate() const noexcept` rather than
   burying it in the message.
   </details>

3. **Catch order:** given `AppError <- IoError <- FileNotFound`, write the catch
   sequence to handle `FileNotFound` specially, other `IoError` generically, other
   `AppError` as fatal.

   <details><summary>Answer</summary>

   ```cpp
   catch (const FileNotFound& e) { /* specific */ }
   catch (const IoError& e)      { /* other io */ }
   catch (const AppError& e)     { /* fatal */ }
   ```
   Most-derived first.
   </details>

4. **Nested:** `parse_config` calls `read_file` (throws `std::system_error`).
   Wrap so the caller sees "config load failed" *and* can still get the errno.
   Sketch.

   <details><summary>Answer</summary>

   ```cpp
   try { auto txt = read_file(p); ... }
   catch (...) { std::throw_with_nested(ConfigError(p, "*", "load failed")); }
   ```
   Caller: `catch (const ConfigError& e) { log(e.what()); try {
   std::rethrow_if_nested(e); } catch (const std::system_error& se) {
   log(se.code()); } }`.
   </details>

5. **Boundary:** `engine::match()` is `noexcept` and returns `MatchResult` (an
   error enum inside). `boot` layer wants exceptions. Where/how translate?

   <details><summary>Answer</summary>

   In the `boot`-side wrapper: `auto r = engine::match(o); if (r.error !=
   MatchErr::ok) throw EngineError(to_string(r.error));`. The translation lives at
   the boundary; `engine` stays exception-free.
   </details>

---

## Interview questions

1. `std::exception` vs `std::runtime_error` se derive — farq, kaunsa default?
2. `using Base::Base;` exception classes mein kyun handy?
3. Exception ke andar structured data (fields) kyun, sirf `what()` kyun nahi?
4. `what()` ko override karte waqt kya khatra? Safe pattern?
5. Kitni exception types banani chahiye — decision rule?
6. `std::throw_with_nested` / `std::rethrow_if_nested` — use case.
7. Exception ko DLL/`.so` boundary ke paar throw karna — problem?
8. Thrown exception object ki copy ctor `noexcept` kyun honi chahiye?

---

## Next
→ [`08-exception-cost.md`](08-exception-cost.md)

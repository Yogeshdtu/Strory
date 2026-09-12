# 01 — Error handling ka poora landscape

## Prerequisites
- `17-RAII` (RAII, destructors), `18-COPY-MOVE` (move, `noexcept` ka zikr)
- `22-MODERN-CPP` file 05 (`std::expected` ka naam)
- Function return values, `enum class`, `<optional>`

## Yeh topic abhi kyun
Ab tak aapne "error aaya to `return -1`" ya "`throw` kar do" — dono dekhe hain,
bina yeh soche ki **kaunsa kab**. C++ mein error report karne ke **paanch alag
mechanisms** hain, har ek ka apna trade-off. HFT ka pehla sabak yahin hai:
**exceptions aksar disabled hote hain** — aur uske peeche solid engineering reason
hai, dogma nahi. Yeh folder poora map deta hai; yeh file us map ka index hai.

---

## Do sawaal jo har error ke liye poochho

1. **Kya yeh error *expected* hai ya *exceptional*?**
   - *Expected*: input parse fail, order reject ho gaya, key map mein nahi hai,
     socket ne `EWOULDBLOCK` diya. Yeh roz hota hai, hot path ka hissa hai.
   - *Exceptional*: config file corrupt, out of memory, invariant toot gaya,
     hardware fail. Yeh "kabhi-kabhi", aur aksar recover nahi kar sakte.

2. **Caller error handle *kar sakta* hai, ya sirf *report/abort* kar sakta hai?**
   - Handle kar sakta hai → error ko value ki tarah lautao (`expected` / code).
   - Nahi kar sakta → `throw` (kahin upar catch hoga) ya `abort` (game over).

 Inn do axes pe har mechanism ki jagah fix ho jaati hai.

---

## Paanch mechanisms — ek nazar mein

| # | Mechanism | Error kaise dikhta | Sabse achha jab | Lesson |
|---|---|---|---|---|
| 1 | **Return code** (`int`, `bool`, `enum class`) | function ka return value | C API, simple pass/fail, `-fno-exceptions` | `02` |
| 2 | **`errno`** (thread-local global) | call ke baad `errno` padho | sirf legacy C library calls (`fopen`, `read`) | `02` |
| 3 | **Exceptions** (`throw` / `try` / `catch`) | alag control-flow path | constructor fail, deep call stack, *exceptional* | `03`–`08` |
| 4 | **`std::expected<T,E>`** (C++23) | `T` ya `E` — ek hi value | expected failure jahan caller decide karega | `10` |
| 5 | **`std::error_code`** | out-param ya return, `{int, category}` | expected failure + rich error info, throw-free | `11` |
| + | **Assertions / contracts** | `assert`, `static_assert`, UB | *bug* pakadne ke liye (error nahi — galti) | `12`, `13` |

`std::optional<T>` bhi hai — par woh "value hai ya nahi", *kyun nahi* nahi batata.
"Not found" ke liye theek; "kyun fail hua" chahiye to `expected`/`error_code`.

---

## 1. Return code

```cpp
enum class ReadErr { ok, eof, io_error, bad_format };

[[nodiscard]] ReadErr read_record(int fd, Record& out);   // out-param se value
```

- **Plus:** zero overhead, koi hidden control flow, `-fno-exceptions` mein bhi
  chalta hai, har platform/ABI pe same.
- **Minus:** caller check karna *bhool sakta* hai (`[[nodiscard]]` se thoda bacha
  lo). Deep call stack mein har level pe manually forward karna padta hai
  (`if (auto e = f(); e != ok) return e;` — boilerplate). Return slot value ke
  liye chahiye tha, ab error ke liye "chura" liya (isliye out-param).

## 2. `errno`

```cpp
#include <cerrno>
#include <cstring>
errno = 0;
FILE* f = std::fopen(path, "rb");
if (!f) std::printf("open fail: %s\n", std::strerror(errno));
```

- `errno` ek **thread-local `int`** hai (macro). POSIX/C library isi se fail
  batati hai.
- **Aaj aapka apna code isse use na kare.** Sirf tab dikhega jab neeche koi C
  library call kar rahe ho. `errno` ko turant `std::error_code(errno,
  std::generic_category())` mein badal lo (file `11`).

## 3. Exceptions

```cpp
struct ConfigError : std::runtime_error { using std::runtime_error::runtime_error; };

Config load_config(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw ConfigError("config khula hi nahi: " + path);
    // ... parse ...
    return cfg;                     // sirf SUCCESS path return karta hai
}
```

- **Plus:** happy path bilkul saaf — error handling code beech mein nahi ghusa.
  Constructor fail kar sakta hai (return value nahi hota). Error apne aap deep
  stack se upar `catch` tak jaata hai, har intermediate function ko chhue bina.
  Ignore karna *namumkin* — uncaught → `std::terminate`.
- **Minus:** `throw` **mehnga** hai (file `08`: ~6000+ ns is machine pe vs
  ~1.5 ns ek return). Control flow "invisible" — koi bhi call throw kar sakta
  hai, exception-safe code likhna discipline maangta hai (file `05`). Binary
  size badhta (unwind tables). `-fno-exceptions` build mein available nahi.

## 4. `std::expected<T,E>` (C++23)

```cpp
std::expected<int, ParseErr> parse_int(std::string_view s);

auto r = parse_int(tok);
if (r) use(*r);
else   log(r.error());
```

- **Plus:** error *value* ki tarah — type system mein dikhta hai, ignore nahi kar
  sakte (`[[nodiscard]]`), koi hidden control flow, `throw` jaisa mehnga nahi.
  Monadic `and_then` / `transform` / `or_else` se plumbing khatam.
- **Minus:** C++23 (purane toolchain pe khud likho / `tl::expected` / Boost.Outcome
  — file `10`). Har call site pe check likhna padta (compiler yaad dilata hai).
  Deep stack mein manually forward (monadic ops isse kam karte).

## 5. `std::error_code`

```cpp
std::error_code ec;
std::size_t n = sock.read(buf, ec);
if (ec == std::errc::resource_unavailable_try_again) { /* retry */ }
else if (ec) { /* real error */ }
```

- **Plus:** `{ int value, const error_category* }` — do words, copy sasta, koi
  heap/throw. Custom categories se domain errors. Standard library isi ko use
  karti (`<filesystem>`, `<thread>`). Portable checks (`== std::errc::...`).
- **Minus:** thoda ceremony (category class banao). `T` aur `ec` alag — `expected`
  jitna clean nahi.

## + Assertions

```cpp
assert(index < size_);              // -DNDEBUG pe gayab
static_assert(sizeof(Tick) == 16);  // compile time
```

- Yeh **error handling nahi** hai — yeh **bug detection** hai. `assert` woh cheez
  check karta hai jo *kabhi false nahi honi chahiye*. Agar false hai → aapke code
  mein galti hai, user ke input mein nahi. Release mein `assert` hata diya jaata
  hai (`NDEBUG`). File `12`.

---

## Andar kya hota hai — bahut chhota

- **Return code / `errno` / `error_code` / `expected`**: normal function return.
  Value register(s) ya caller ke stack slot mein. CPU ke liye yeh ek `mov` +
  `ret`. Branch predictor `if (err)` ko easily seekh leta (hamesha not-taken).
- **Exception**: `throw X` → runtime `__cxa_allocate_exception` (heap!) → object
  copy → `__cxa_throw` → unwinder `.eh_frame` / `.gcc_except_table` padhta hai,
  har frame ke liye "yahan koi cleanup/handler hai?" — dtors chalata hai —
  matching `catch` mile to control transfer. Yeh sab **cold code**, aur pehli baar
  I-cache/page miss. Isliye 1000s of nanoseconds.

Poora measured breakdown file `08` mein.

---

## > **HFT relevance**
> Yeh folder ka kendra-bindu. Ek trading engine ka hot path — market data parse,
> book update, signal, order out — **microseconds** mein hota hai, aur uski
> **latency variance (jitter)** utni hi important hai jitni average. Ek `throw`
> jo 6 µs le le woh ek "latency spike" hai jo P99 kharab karta hai. Isliye:
>
> - **Hot path mein exceptions nahi.** Expected failures (order reject, risk
>   check fail, book crossed, sequence gap) → `expected` / `error_code` /
>   status enum. Yeh branch predictable hai, inline hota hai, jitter nahi.
> - **Poora binary aksar `-fno-exceptions -fno-rtti`** — chhota code, koi unwind
>   table, compiler aur aggressive inline/optimize karta hai (file `09`).
> - **Exceptions (agar allowed) sirf startup/config/shutdown** mein — jahan latency
>   matter nahi karti aur "abort with a clear message" hi chahiye.
> - **Unrecoverable = fail fast.** Corrupt book state pe `throw` karke "chalte
>   raho" se bura kuch nahi. `abort()` — process maro, hot standby le lega.

---

## Hands-on

```bash
./build.ps1 23-ERROR-HANDLING/examples/01_exceptions_basics.cpp     # mechanism 3
./build.ps1 23-ERROR-HANDLING/examples/05_expected.cpp              # mechanism 4
./build.ps1 23-ERROR-HANDLING/examples/06_error_code.cpp            # mechanism 5
./build.ps1 23-ERROR-HANDLING/examples/07_no_exceptions.cpp        # -fno-exceptions toolkit
```

Ek `parse_price(std::string_view) -> ?` function ko teen tarah likho: (a) `bool` +
out-param, (b) `std::optional<long>`, (c) `expected<long, PriceErr>`. Teenon ke
call sites compare karo — kaun sabse kam boilerplate, kaun sabse zyada info deta.

---

## ⚠️ Traps

### Trap 1 — `std::optional` jab error ki wajah chahiye
```cpp
std::optional<Config> load(const std::string&);   // fail hua... par KYUN?
```
"Not found" ke liye `optional` theek. "Fail" ke liye `expected`/`error_code` —
warna caller ko kuch bata nahi paoge.

### Trap 2 — return code ko ignore karna
```cpp
write_all(fd, buf, n);            // ⚠️ return ignore -> partial write pakda hi nahi
```
Fallible functions pe `[[nodiscard]]` lagao. Compiler warn karega.

### Trap 3 — exceptions ko normal control flow ki tarah use karna
```cpp
try { x = map.at(key); } catch (...) { x = default; }   // ⚠️ "not found" exceptional nahi hai
```
`map.find` / `map.contains` use karo. `throw` ko sirf *exceptional* ke liye rakho.

### Trap 4 — mix karke consistency khona
Ek codebase mein kuch functions throw karte, kuch `-1` lautate, kuch `errno` set
karte — caller ko har API ke liye alag mental model. **Ek project, ek convention.**

### Trap 5 — `errno` ko check karne se pehle set na karna 0
```cpp
long v = std::strtol(s, &end, 10);
if (errno == ERANGE) ...          // ⚠️ errno purane call ka bacha hua ho sakta
```
C library `errno` ko success pe clear *nahi* karti. Call se pehle `errno = 0`.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Exceptions slow hain, kabhi use mat karo" | Happy path zero-cost. `throw` mehnga. *Exceptional* ke liye theek |
| "`-fno-exceptions` = error handling nahi" | Return codes / `expected` / `abort` — poora toolkit hai (file `09`) |
| "`throw` ka matlab program crash" | Nahi — matching `catch` tak jaata hai. Uncaught tabhi crash |
| "`error_code` aur `error_condition` same" | `code` = kya exactly hua; `condition` = portable bucket (file `11`) |
| "`assert` se user errors handle karo" | `assert` = bug detection. User error = normal handling |
| "har error ke liye ek exception type" | Zyada tar errors expected hote — `enum`/`expected` kaafi |

---

## Exercises

1. **Classify:** in errors ko "expected" ya "exceptional" batao aur mechanism
   chuno: (a) UDP packet ka checksum fail, (b) `malloc` ne `nullptr` diya,
   (c) user ne `--rate=abc` diya, (d) order book crossed ho gaya (bid > ask),
   (e) config file mein required field missing.

   <details><summary>Answer</summary>

   (a) expected — hot path, `error_code`/counter, drop & continue.
   (b) exceptional — `abort` ya `bad_alloc` (agar exceptions on); recover mushkil.
   (c) expected (startup) — return code / `expected`, clear message, exit.
   (d) exceptional *and* a bug-signal — log loudly, maybe `abort` (invariant toota).
   (e) expected (startup) — `expected<Config, ConfigErr>`, batao kaunsa field.
   </details>

2. **Boilerplate count:** ek 4-level deep call chain hai (`a`→`b`→`c`→`d`), `d`
   fail kar sakta hai. Return-code style mein kitni lines har level pe error
   forward karne ko? Exception style mein? `expected` + `and_then` style mein?

   <details><summary>Answer</summary>

   Return code: har level pe `if (auto e = next(); e != ok) return e;` → 3
   intermediate levels × 1 line = 3 (plus har signature `[[nodiscard]] Err`).
   Exceptions: 0 — `b`/`c` ko chhua bhi nahi, `throw` seedha upar. `expected` +
   `and_then`: intermediate levels bas `return next().and_then(...)` — ~0 extra
   branching, error apne aap forward.
   </details>

3. **API design:** `class RingBuffer { bool try_push(T); std::optional<T>
   try_pop(); };` — yeh do function kaunsa mechanism use kar rahe aur kyun sahi
   hai? Agar `push` fail ka *reason* chahiye (full vs closed) to?

   <details><summary>Answer</summary>

   `bool` return + `optional` — SPSC queue ka hot path, sirf "hua/nahi hua"
   chahiye, zero overhead. Reason chahiye to `enum class PushResult { ok, full,
   closed };` — abhi bhi throw-free, ek register.
   </details>

4. **`errno` bug:** yeh code kab galat answer dega?
   ```cpp
   double d = std::strtod(s, nullptr);
   if (errno == ERANGE) return Err::range;
   ```

   <details><summary>Answer</summary>

   Agar is line se *pehle* kisi call ne `errno` ko `ERANGE` set kiya tha aur
   `strtod` khud range-ok tha — `errno` clear nahi hua, jhoota `Err::range`.
   Fix: `errno = 0;` `strtod` se theek pehle.
   </details>

5. **Consistency:** ek team decide karti hai "sab kuch `expected`". `main` ki
   `int` return, ek third-party callback jo `noexcept` hai aur throw kar sakta,
   aur `operator[]` — teenon `expected` return nahi kar sakte. Har ek ka rasta?

   <details><summary>Answer</summary>

   `main`: `expected` ko `if (!r) { log(r.error()); return 1; } return 0;` mein
   convert. Callback: uske andar `try { ... } catch` (agar exceptions on) ya usse
   `noexcept`-safe rakho aur error ko member/out-param se nikaalo. `operator[]`:
   unchecked by design (jaise `std::vector`) — ek alag `at()`/`get()` jo
   `expected`/`optional` de.
   </details>

---

## Interview questions

1. C++ mein error report karne ke kitne tareeke? Har ek ka ek trade-off.
2. `std::optional` vs `std::expected` — kab kaunsa?
3. Exceptions ki "zero-cost" ka matlab exactly kya? Kya sach mein free?
4. HFT / low-latency code mein exceptions kyun aksar band hote hain?
5. Return code ka sabse bada problem kya, aur `[[nodiscard]]` usse kaise kam karta?
6. `errno` aaj bhi kahan dikhta hai, aur aapko usse kya karna chahiye?
7. "Expected error" vs "exceptional error" — ek-ek example aur handling.

---

## Next
→ [`02-return-codes-and-errno.md`](02-return-codes-and-errno.md)

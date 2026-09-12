# 02 — Return codes aur `errno`

## Prerequisites
- `01-error-handling-strategies.md`
- Functions, out-parameters (pointer/reference), `enum class`

## Yeh topic abhi kyun
Sabse purana, sabse portable error mechanism. C ki poori duniya isi pe chalti hai,
aur aapka C++ code jab bhi OS ya C library ko chhuega — `open`, `read`, `mmap`,
`strtol` — yahi milega. `-fno-exceptions` build (file `09`) mein yeh aapka
default ban jaata hai. Isliye ise achhe se karna aana chahiye: sahi type,
`[[nodiscard]]`, aur `errno` ke sharp edges.

---

## Return code — basic form

```cpp
// C style: magic int. 0 = ok, non-zero = kuch gadbad.
int parse_config(const char* path);        // 0 ok, -1 fail... par KYA fail?
```

Problem: `-1` kuch nahi batata. Better — ek **`enum class`**:

```cpp
enum class CfgErr {
    ok = 0,
    not_found,
    permission_denied,
    bad_syntax,
    missing_field,
};

[[nodiscard]] CfgErr parse_config(const char* path, Config& out);
```

- `enum class` → type-safe (`CfgErr` aur `NetErr` galti se mix nahi honge),
  scoped (`CfgErr::not_found`), underlying `int` (register mein fit).
- `ok = 0` → `if (err)` idiom chalta hai (0 = falsy = success).
- **`[[nodiscard]]`** → caller ne return ignore kiya to compiler warning.
- Actual result **out-parameter** (`Config& out`) se — kyunki return slot ab error
  ke liye hai.

---

## Out-parameter kaise pass karein

| Form | Kab |
|---|---|
| `Config& out` (reference) | hamesha valid object, caller pehle bana ke deta |
| `Config* out` (pointer) | optional ho sakta (`nullptr` = "result nahi chahiye") |
| `std::span<char> buf, size_t& written` | buffer fill karna, caller ka storage |

```cpp
[[nodiscard]] CfgErr parse_config(const char* path, Config* out) {
    std::ifstream in(path);
    if (!in)              return CfgErr::not_found;
    Config tmp;
    if (!read_kv(in, tmp)) return CfgErr::bad_syntax;
    if (tmp.name.empty())  return CfgErr::missing_field;
    if (out) *out = std::move(tmp);      // sirf success pe out likho
    return CfgErr::ok;
}
```

⚠️ **Rule: error return karte waqt `out` ko mat chhoo.** Caller assume karega ki
fail hone pe `out` untouched hai (kam se kam predictable). Half-written `out` sabse
bura — "basic guarantee" bhi nahi.

---

## Deep call stack — forwarding ka dard

```cpp
[[nodiscard]] Err step_c(State& s);

[[nodiscard]] Err step_b(State& s) {
    if (Err e = step_c(s); e != Err::ok) return e;   // manually forward
    // ... aur kaam ...
    return Err::ok;
}
[[nodiscard]] Err step_a(State& s) {
    if (Err e = step_b(s); e != Err::ok) return e;   // phir se
    return Err::ok;
}
```

Har level pe wahi 1 line. 10 level deep → 10 jagah. Yeh return-code style ka sabse
bada minus hai. Kuch codebases ek macro rakhte hain:

```cpp
#define TRY(expr) do { if (Err e_ = (expr); e_ != Err::ok) return e_; } while (0)

Err step_b(State& s) { TRY(step_c(s)); /* ... */ return Err::ok; }
```

`std::expected` + `and_then` (file `10`) yahi problem monadically solve karta hai.
Rust ka `?` operator bhi bilkul yeh hai.

---

## `errno` — thread-local global

C library (aur POSIX syscalls) fail hone pe **`errno`** set karti hain — ek
`<cerrno>` ka macro jo effectively ek **thread-local `int`** hai.

```cpp
#include <cerrno>
#include <cstring>    // std::strerror
#include <cstdio>

errno = 0;                                   // (1) pehle clear karo
FILE* f = std::fopen("data.bin", "rb");
if (!f) {                                    // (2) pehle RETURN VALUE check
    int e = errno;                           // (3) turant copy (agla call badal dega)
    std::printf("open fail: %s (errno=%d)\n", std::strerror(e), e);
}
```

Teen rules:

1. **Call se pehle `errno = 0`** — C library ise success pe clear *nahi* karti.
   Purane call ka bacha hua value aapko dhoka dega.
2. **Pehle function ka apna failure signal check karo** (`NULL`, `-1`, `SIZE_MAX`).
   `errno` sirf *tab* meaningful hai jab function ne fail bataya ho. Kuch functions
   success pe bhi `errno` set kar dete hain.
3. **`errno` ko turant copy karo.** `std::printf`, `std::strerror`, koi bhi call
   `errno` overwrite kar sakta hai.

`strtol` / `strtod` family khaas: return value se error nahi pata chalta (0 ya
`LONG_MAX`), sirf `errno == ERANGE` se.

```cpp
errno = 0;
char* end = nullptr;
long v = std::strtol(s, &end, 10);
if (end == s)          return NumErr::not_a_number;   // koi digit nahi
if (errno == ERANGE)   return NumErr::out_of_range;   // overflow
if (*end != '\0')      return NumErr::trailing_junk;
```

### `errno` ko `std::error_code` bana lo

Apne API mein `errno` mat leak karo. Boundary pe convert:

```cpp
if (::read(fd, buf, n) < 0)
    return std::error_code(errno, std::generic_category());   // file 11
```

---

## Andar kya hota hai

- **`enum class` return**: `int`-sized value, `eax`/`w0` register mein wapas. Caller
  ka `if (err)` ek `test`/`cbz`. Branch predictor "hamesha ok" seekh leta → hot
  path pe ~0 cost. Yahi return codes ka bada plus.
- **out-param**: caller ke stack pe (ya register) object ka address pass hota; callee
  us address pe likhta. Ek pointer indirection. RVO/`expected` value-return se
  thoda kam optimizer-friendly, par predictable.
- **`errno`**: `errno` actually `*__errno_location()` (glibc) ya `*_errno()`
  (UCRT) — ek function call jo thread-local slot ka address deta. Isliye `errno`
  ko baar-baar padhna bhi bilkul free nahi; ek baar copy karke rakho.

---

## > **HFT relevance**
> `-fno-exceptions` build mein return codes / status enums **default** ban jaate
> hain. Hot path ke expected failures — `try_pop` empty, order rejected, risk
> check fail, sequence gap — sab `enum class Result : uint8_t { ok, ... }` se,
> ek register, branch-predictable, zero jitter.
>
> Syscall boundary pe (`recvmmsg`, `io_uring`, `sendto`) `errno`/`-1` milta hai —
> use turant apne domain error ya counter mein badlo, `errno` ko upar mat le
> jao. Aur syscall ki return value **hamesha check** — silently-dropped partial
> `send` ek ganda production bug hai.
>
> `[[nodiscard]]` har fallible function pe — HFT code review mein "unchecked
> return" ek blocker hai.

---

## Hands-on

```bash
./build.ps1 23-ERROR-HANDLING/examples/07_no_exceptions.cpp    # status-enum + out-param style
./build.ps1 23-ERROR-HANDLING/examples/06_error_code.cpp       # errno -> error_code (section A)
```

Likho: `read_line(std::FILE*, std::string& out) -> enum class LineErr { ok, eof,
io_error }`. `std::fgets` use karo, `std::feof` / `std::ferror` se distinguish
karo. Caller ek loop mein `while (read_line(f, s) == LineErr::ok)`.

---

## ⚠️ Traps

### Trap 1 — return value check kiye bina `errno` padhna
```cpp
FILE* f = std::fopen(p, "r");
if (errno) { ... }               // ⚠️ f == NULL check karo; errno stale ho sakta
```

### Trap 2 — `errno` ko `0` pe reset na karna
```cpp
long v = std::strtol(s, &e, 10);
if (errno == ERANGE) ...          // ⚠️ pichhle call ka ERANGE bacha ho sakta
```

### Trap 3 — `errno` ko intervening call ke baad padhna
```cpp
if (::connect(fd, ...) < 0) {
    log("connect failed");        // ⚠️ log() -> printf() -> errno badal sakta
    return std::error_code(errno, std::system_category());   // ab galat errno
}
```
Fix: `int e = errno;` sabse pehle, phir log.

### Trap 4 — `enum` (unscoped) ka return, sab jagah collide
```cpp
enum Err { ok, timeout };
enum NetErr { ok, refused };      // ⚠️ 'ok' redefinition. `enum class` use karo
```

### Trap 5 — error pe out-param likhna
```cpp
Err parse(std::string_view s, int& out) {
    out = 0;                      // ⚠️ caller ka data pehle hi tabaah
    if (bad(s)) return Err::bad;
    ...
}
```
Pehle validate, success pe hi `out` likho.

### Trap 6 — `bool` return jab 2 se zyada outcomes hain
```cpp
bool send(...);   // false = ? disconnected? would-block? too-big?
```
`enum class SendResult` — reason chahiye caller ko.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`int` return code kaafi hai" | `enum class` — type-safe, self-documenting, `[[nodiscard]]` |
| "`errno` mera error variable ho sakta" | `errno` sirf C library ke liye; apna → `error_code`/enum |
| "`errno` fail pe hi set hota" | Kuch functions success pe bhi set karti; pehle return value dekho |
| "return code ignore karna theek, warning bas hai" | Unchecked I/O return = data loss bug. `[[nodiscard]]` + treat as error |
| "out-param se return-value fast" | Value return + RVO aksar barabar ya behtar; out-param ek indirection |

---

## Exercises

1. **Type choose:** `parse_ipv4(std::string_view) -> ?`. 4 octets nikaalne hain,
   fail modes: empty, galat format, octet > 255. Kaunsa return type aur kyun?

   <details><summary>Answer</summary>

   `enum class IpErr { ok, empty, bad_format, octet_range };` + `uint32_t& out`,
   ya `std::expected<uint32_t, IpErr>`. `bool` kam pad jaayega (3 fail reasons).
   `optional` bhi — reason chahiye to nahi.
   </details>

2. **`errno` trace:** yeh kya print karega aur kyun galat ho sakta hai?
   ```cpp
   errno = 0;
   int fd = ::open("/no/such", O_RDONLY);
   printf("opened\n");
   if (fd < 0) printf("err = %s\n", strerror(errno));
   ```

   <details><summary>Answer</summary>

   `open` fail → `errno = ENOENT`. Phir `printf("opened\n")` — output stream pe
   likhta hai; agar woh fail na kare to `errno` waise hi. Practically yahan
   `ENOENT` print hoga, par `printf` ke beech aana **risky pattern** hai —
   `errno` ko `open` ke turant baad copy karna chahiye.
   </details>

3. **`TRY` macro:** `#define TRY(x) do { if (auto e_ = (x); e_ != Err::ok) return
   e_; } while (0)` — is macro ka ek problem batao (hint: `Err` type hard-coded).

   <details><summary>Answer</summary>

   Sirf un functions mein kaam karta jo `Err` return karte hain aur jinka
   caller bhi `Err` return karta hai. Alag error type ya `void` function mein
   nahi. `std::expected` + `and_then` / `or_else`, ya C++23 pattern, zyada
   general — aur `?`-jaisa proposal isi liye aata raha hai.
   </details>

4. **Out-param safety:** `Err decode(std::span<const std::byte> in, Frame& out)` —
   `out` ke baare mein caller ko kya guarantee do, aur code mein kaise ensure karo?

   <details><summary>Answer</summary>

   Guarantee: "`ok` return karne pe `out` fully valid; kisi bhi error pe `out`
   untouched." Ensure: local `Frame tmp;` mein decode karo, sab checks pass hone
   ke baad `out = std::move(tmp);` bilkul aakhri line pe.
   </details>

5. **Portability:** `std::error_code(errno, std::system_category())` vs
   `std::generic_category()` — POSIX pe farq? Windows pe?

   <details><summary>Answer</summary>

   POSIX pe `errno` values `std::generic_category()` ke saath match karte
   (`std::errc` inhi se bana). `system_category()` platform ke native error space
   ka — POSIX pe practically same numbers, Windows pe `GetLastError()` ka space
   (errno se alag). `errno` ke liye `generic_category()` sahi/portable choice.
   </details>

---

## Interview questions

1. `enum class` return code vs plain `int` — kya jeeta?
2. `[[nodiscard]]` kya karta, kahan lagana chahiye?
3. `errno` ko safely padhne ke teen rules.
4. `strtol` overflow kaise detect karte ho? (hint: return value nahi)
5. Deep call stack mein return-code forwarding ka boilerplate — kaise kam karein?
6. Error return karte waqt out-parameter ke saath kya *nahi* karna?
7. `errno` ko apne public API se kyun nahi leak karna chahiye?

---

## Next
→ [`03-exceptions-basics.md`](03-exceptions-basics.md)

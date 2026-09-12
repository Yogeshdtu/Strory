# 11 — `std::error_code`, `error_condition`, custom categories

## Prerequisites
- `02-return-codes-and-errno.md`, `10-std-expected.md`
- `07-custom-exceptions.md` (`std::system_error`)
- [`examples/06_error_code.cpp`](examples/06_error_code.cpp)

## Yeh topic abhi kyun
`std::error_code` woh mechanism hai jo standard library **khud** use karti hai —
`<filesystem>`, `<thread>`, networking. Yeh `errno` ka type-safe, extensible,
portable successor hai: ek **`{ int value, const error_category* }`** pair,
throw-free, allocation-free, do-word copy. Custom categories se aap apne domain
errors ko usi framework mein daal sakte ho, aur portable checks (`ec ==
std::errc::...`) likh sakte ho.

---

## `error_code` ka anatomy

```cpp
#include <system_error>

std::error_code ec;               // default: value 0, system_category, "no error"
ec = std::make_error_code(std::errc::timed_out);

ec.value();            // int  — 110 (POSIX ETIMEDOUT)  — category-specific number
ec.category();         // const std::error_category&  — kaunsa error space
ec.category().name();  // "generic"
ec.message();          // std::string — "Connection timed out"  (human)
static_cast<bool>(ec); // true agar value != 0  (i.e. koi error hai)
```

- **Sirf 2 words:** ek `int` + ek pointer to a category singleton. Copy = trivial.
  Koi heap, koi throw.
- `value()` **category ke context mein** meaningful hai — `5` ka matlab
  `generic_category` mein alag, `my_category` mein alag.
- `if (ec)` = "koi error hai?". `if (!ec)` = "ok".

---

## `error_code` vs `error_condition`

Yeh distinction hi log ko confuse karta hai. Simple version:

| | `std::error_code` | `std::error_condition` |
|---|---|---|
| Kya hai | **Exactly kya hua** — platform/library-specific | **Portable bucket** — "yeh kis kism ka error hai" |
| Kaun banata | jo operation fail hui (OS, library) | aap, comparison ke liye |
| Example | `{54, windows_category}` (`ERROR_CONNECTION_RESET`) | `std::errc::connection_reset` |
| Use | store, log, return | `if (ec == std::errc::connection_reset)` |

```cpp
std::error_code ec = do_network_thing();     // {platform-specific value + category}

// portable check — exact number/category yaad rakhne ki zaroorat nahi:
if (ec == std::errc::connection_reset) { reconnect(); }
```

Yeh `==` kaam kaise karta hai: `std::errc` ek enum hai jiske liye
`is_error_condition_enum` true hai → `std::errc::connection_reset` implicitly ek
`error_condition` ban jaata hai → `error_code == error_condition` comparison
`ec.category().default_error_condition(ec.value()) == that_condition` check karta
hai. Yaani **har category batati hai ki uske codes kaunse portable bucket mein
girte hain.**

`<system_error>` ka `std::errc` = POSIX `errno` values, portable form mein.

---

## `std::system_error` — exception jo `error_code` carry karta hai

```cpp
try {
    std::filesystem::rename(a, b);            // throwing overload
} catch (const std::system_error& e) {
    std::error_code ec = e.code();            // structured error mila
    if (ec == std::errc::cross_device_link) { copy_then_delete(a, b); }
    else throw;
}
```

`std::system_error : std::runtime_error` + ek `error_code`. Yeh bridge hai jab
koi cheez throw karti hai par aapko programmatic handling chahiye. Non-throwing
overloads (`fs::rename(a, b, ec)`) directly `error_code` set karti hain — HFT /
`-fno-exceptions` mein woh use karo.

---

## Apni custom category

Do cheezein chahiye: ek **`enum`** aur ek **`error_category`** subclass.

```cpp
// 1. domain error enum
enum class OrderErr {
    ok = 0, unknown_symbol, price_out_of_band, size_too_large, market_closed,
};

// 2. category: enum ke int -> message + portable condition
class OrderErrCategory final : public std::error_category {
public:
    const char* name() const noexcept override { return "order"; }

    std::string message(int c) const override {
        switch (static_cast<OrderErr>(c)) {
            case OrderErr::ok:                 return "ok";
            case OrderErr::unknown_symbol:     return "unknown symbol";
            case OrderErr::price_out_of_band:  return "price outside band";
            case OrderErr::size_too_large:     return "size exceeds limit";
            case OrderErr::market_closed:      return "market closed";
        }
        return "unknown order error";
    }

    // optional: portable bucket mapping
    std::error_condition default_error_condition(int c) const noexcept override {
        switch (static_cast<OrderErr>(c)) {
            case OrderErr::unknown_symbol:
            case OrderErr::price_out_of_band:
            case OrderErr::size_too_large:
                return std::errc::invalid_argument;
            case OrderErr::market_closed:
                return std::errc::operation_not_permitted;
            default:
                return std::error_condition(c, *this);
        }
    }
};

// 3. single global instance — address hi identity hai
inline const std::error_category& order_category() {
    static const OrderErrCategory cat;
    return cat;
}

// 4. enum -> error_code hook (ADL se milta)
inline std::error_code make_error_code(OrderErr e) {
    return { static_cast<int>(e), order_category() };
}

// 5. STL ko batao: OrderErr ek error-code enum hai (implicit conversion enable)
template <> struct std::is_error_code_enum<OrderErr> : std::true_type {};
```

Ab:

```cpp
std::error_code ec = OrderErr::price_out_of_band;   // implicit (step 5)
std::printf("%s:%d %s\n", ec.category().name(), ec.value(), ec.message().c_str());
// order:2 price outside band

if (ec == std::errc::invalid_argument) { /* portable — step 4 mapping */ }
```

### API jo isse use kare

```cpp
long submit(const Order& o, std::error_code& ec) {
    ec.clear();
    if (!known(o.sym))          { ec = OrderErr::unknown_symbol;    return 0; }
    if (!in_band(o.price))      { ec = OrderErr::price_out_of_band; return 0; }
    return next_id();
}
```

Ya `std::expected<long, std::error_code>` (file `10`) — best of both.

---

## `error_code` vs `expected<T, E>`

| | `error_code` out-param | `expected<T, error_code>` |
|---|---|---|
| Result + error | alag (`T` return, `ec` param) | ek value |
| Ignore-proof | `ec` check bhoolna easy | `[[nodiscard]]`, must handle |
| Compose | manual | monadic `and_then` |
| STL interop | direct (`fs`, `thread`) | wrap karna padta |
| Legacy / C++17 | ✅ | needs `tl::expected` |

Modern code: **`expected<T, error_code>`** jab dono chahiye. **Raw `error_code`
out-param** jab STL-style API match karni ho ya C++17.

---

## Andar kya hota hai

- `error_category` singletons — har category ka exactly **ek** instance (function-
  local `static`). `error_code` sirf uska **address** rakhta hai. Do `error_code`
  same tab jab value + category-pointer dono same.
- `error_code` = `{ int, const error_category* }` = 16 bytes (x64). Trivially
  copyable, trivially destructible. Register pair mein pass hota.
- `.message()` — yahan `std::string` banta hai (allocation!) — sirf tab jab aap
  call karo (logging). Error ko *carry* karne mein koi alloc nahi.
- `ec == std::errc::x` → `category().equivalent(...)` / `default_error_condition`
  — virtual call, chhota. Aksar not on hot path.
- `is_error_code_enum<E>` specialization → `error_code`/`unexpected` ke templated
  ctors us enum ke liye enable ho jaate (SFINAE).

---

## > **HFT relevance**
> `std::error_code` do jagah shine karta hai:
>
> 1. **Syscall / OS boundary** — `errno`/`GetLastError` ko turant
>    `std::error_code(errno, std::system_category())` mein wrap karo, phir aur upar
>    apne domain code mein translate. `errno` ko API se leak mat karo.
>
> 2. **Non-throwing STL overloads** — `std::filesystem::*(..., ec)`,
>    `std::from_chars` (returns `std::errc`), future networking. `-fno-exceptions`
>    build mein yeh **must**.
>
> Hot path pe ek 16-byte `error_code` copy karna ek `enum class` (1–2 bytes) se
> thoda mehnga hai — pure hot inner loops mein `enum class` prefer karo.
> Control-plane / setup / boundary code mein `error_code` (ya `expected<T,
> error_code>`) — richness + STL interop worth it. `.message()` ko hot path pe
> mat call karo (alloc); logging thread pe karo.

---

## Hands-on

```bash
./build.ps1 23-ERROR-HANDLING/examples/06_error_code.cpp
```

Example: `errno` → `error_code` (section A, portable `== std::errc::...` check),
custom `order` category (B), `error_code` vs `error_condition` (C). Phir:
- `OrderErr::market_closed` add karke uska `default_error_condition` map karo
  `std::errc::operation_not_permitted` pe, aur portable check likho.
- Ek `submit` variant `std::expected<long, std::error_code>` return kare — call
  site compare karo.

---

## ⚠️ Traps

### Trap 1 — `error_code` ko `error_condition` samajhna (ya ulta)
```cpp
if (ec.value() == ETIMEDOUT) ...      // ⚠️ platform-specific number, category ignore
if (ec == std::errc::timed_out) ...    // ✅ portable
```

### Trap 2 — category ka multiple instances
```cpp
std::error_code f() { OrderErrCategory c; return {1, c}; }   // ⚠️ har call naya instance
```
Ek `static` singleton (`order_category()`), hamesha wahi.

### Trap 3 — `is_error_code_enum` specialization bhoolna
```cpp
std::error_code ec = OrderErr::x;   // ⚠️ implicit conversion nahi -> compile error
```
`template <> struct std::is_error_code_enum<OrderErr> : std::true_type {};`.

### Trap 4 — `.message()` ko hot path pe
```cpp
if (ec) hot_log(ec.message());   // ⚠️ std::string alloc har error pe
```
Error ko `error_code` ki tarah carry karo; string sirf jab actually print karna ho.

### Trap 5 — `error_code` vs `errc` enum ki `make_` functions confuse
`make_error_code(std::errc::x)` → `error_code`. `make_error_condition(std::errc::x)`
→ `error_condition`. Category mapping ka comparison inhi ke through.

### Trap 6 — 0 ko error samajhna
`error_code{}` / value 0 = **no error**. `if (ec)` false. Apne enum mein bhi
`ok = 0` rakho.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`error_code` = `errno`" | `errno` int; `error_code` = `{int, category}`, extensible, portable checks |
| "`error_code` throw kar sakta / heap use karta" | Nahi — 16 bytes, trivially copyable, throw-free. `.message()` alloc karta, carry nahi |
| "`error_code` aur `error_condition` interchangeable" | `code` = exact/platform; `condition` = portable bucket for comparison |
| "custom error ke liye exception hierarchy hi" | `enum` + `error_category` → same framework as STL, throw-free |
| "`ec.value() == 5` check karna fine" | Category-specific number — `== std::errc::...` portable check karo |
| "category class ka instance kahin bhi bana lo" | Ek `static` singleton — identity = address |

---

## Exercises

1. **`errno` wrap:** `::recv` ne `-1` diya. Ise `std::error_code` mein wrap karo
   aur "would block" ka portable check likho.

   <details><summary>Answer</summary>

   ```cpp
   ssize_t n = ::recv(fd, buf, len, 0);
   if (n < 0) {
       std::error_code ec(errno, std::system_category());
       if (ec == std::errc::operation_would_block ||
           ec == std::errc::resource_unavailable_try_again) return WouldBlock{};
       return ec;   // real error
   }
   ```
   </details>

2. **Minimal category:** `enum class ProtoErr { ok, truncated, bad_magic, version
   };` ke liye 5 steps (name, message, singleton, `make_error_code`,
   `is_error_code_enum`) — sabse chhota form likho.

   <details><summary>Answer</summary>

   A `final : std::error_category` with `name()` returning `"proto"` and a
   `switch` `message()`. `inline const std::error_category& proto_cat() { static
   const C c; return c; }`. `inline std::error_code make_error_code(ProtoErr e) {
   return {int(e), proto_cat()}; }`. `template<> struct
   std::is_error_code_enum<ProtoErr> : std::true_type {};`. (No
   `default_error_condition` needed if you don't want portable buckets.)
   </details>

3. **Which type:** for each pick `error_code` or `enum class`: (a) SPSC ring
   `try_push` result, (b) `Config::load` result, (c) `Socket::connect` result,
   (d) per-tick `Book::apply` result at 2M/s.

   <details><summary>Answer</summary>

   (a) `enum class` (1 byte, hottest). (b) `error_code` or `expected<Config,
   ConfigErr>` — rich, not hot. (c) `error_code` — OS interop. (d) `enum class` —
   hot inner loop, avoid 16-byte copies.
   </details>

4. **`condition` mapping:** aapki `DbErr` mein `conn_lost`, `deadlock`,
   `constraint`. Kaunse ko `std::errc` bucket de sakte ho, kaunse ko nahi?

   <details><summary>Answer</summary>

   `conn_lost` → `std::errc::connection_aborted` (or `network_down`). `deadlock` →
   `std::errc::resource_deadlock_would_occur`. `constraint` → koi natural POSIX
   bucket nahi → `default_error_condition` mein khud ki category return karo (no
   portable mapping). Mapping optional hai — sirf jahan sensible.
   </details>

5. **Interop:** ek library `std::expected<T, std::error_code>` return karti,
   doosri `T` + `error_code&` out-param. Ek adapter likho jo second ko first ke
   shape mein laaye.

   <details><summary>Answer</summary>

   ```cpp
   template <class F, class... A>
   auto to_expected(F&& f, A&&... a) -> std::expected<std::invoke_result_t<F, A..., std::error_code&>, std::error_code> {
       std::error_code ec;
       auto v = std::forward<F>(f)(std::forward<A>(a)..., ec);
       if (ec) return std::unexpected(ec);
       return v;
   }
   ```
   </details>

---

## Interview questions

1. `std::error_code` ka layout aur cost (copy, throw, heap)?
2. `error_code` vs `error_condition` — kaun kya, comparison kaise kaam karta?
3. Custom category banane ke steps.
4. `is_error_code_enum` specialization ka kaam?
5. `std::system_error` kab useful — throwing library + structured handling.
6. `.message()` ki cost — hot path pe kyun na call karo?
7. `expected<T, error_code>` vs raw `error_code` out-param — kab kaunsa?
8. `errno` ko `error_code` mein wrap karte waqt `system_category` vs `generic_category`?

---

## Next
→ [`12-assertions.md`](12-assertions.md)

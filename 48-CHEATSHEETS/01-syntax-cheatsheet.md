# 01 — C++ syntax cheatsheet

Ek-page syntax reference. Depth chahiye → course folder link diya hai.
Compile: `g++ -std=c++20 -Wall -Wextra -Wshadow -g file.cpp -o t && ./t`

---

## Declarations & initialization  (folder 03)

```cpp
int a = 20;              // copy-init
int b{20};               // list-init (narrowing = ERROR — prefer this)
int c(20);               // direct-init
int d{};                 // zero-init  -> 0
auto e = 20;             // int   (auto never a reference/const by itself)
const auto& f = e;       // const int&
constexpr int g = 20;    // compile-time constant
int arr[3] = {1,2,3};
std::array<int,3> arr2{1,2,3};
int x, y, z;             // 3 ints (uninitialized — reading = UB)

int&  ref = a;           // reference — must bind now, never rebinds  (folder 13)
int*  p  = &a;           // pointer                                   (folder 12)
const int* pc;           // ptr to const int  (pointee locked)
int* const cp = &a;      // const ptr        (pointer locked)
```

Integer types: `std::int32_t`, `std::uint64_t`, `std::size_t`, `std::ptrdiff_t`
(`<cstdint>`). `int` "practically 4 bytes" — standard sirf `>= 16 bits` guarantee
karta; jab pakka chahiye `int32_t` use karo. (folder 03/05)

---

## Control flow  (folders 06, 07)

```cpp
if (init; cond) {} else if (cond) {} else {}      // if with initializer (C++17)
if (auto it = m.find(k); it != m.end()) use(it);

switch (x) {                                       // integral / enum only
    case 1: [[fallthrough]];                       // explicit fallthrough
    case 2: doit(); break;
    default: break;
}

while (cond) {}
do {} while (cond);
for (int i = 0; i < n; ++i) {}
for (const auto& v : container) {}                 // range-for
for (auto& [k, v] : map) {}                        // structured bindings (C++17)

break; continue;
[[likely]] / [[unlikely]]                          // layout hint (C++20)
```

---

## Functions  (folder 08)

```cpp
ReturnT name(ParamT p, ParamT2 q = default_);      // default args: only in decl
auto f(int x) -> int;                              // trailing return type
[[nodiscard]] int must_use();
void g() noexcept;                                 // promises not to throw
constexpr int sq(int x) { return x*x; }            // usable at compile time
consteval int ce(int x) { return x*x; }            // MUST run at compile time (C++20)

// pass-by:  value (copy) | const T& (read, no copy) | T& (mutate) | T&& (move-from)
void h(const std::string& s);                      // big object, read-only
void h(std::string s);                             // want your own copy / will move

int add(int, int);                                 // overload set — by param types
```

Call stack: har call ka ek **stack frame** (return address, saved regs, locals).
Recursion depth data pe depend kare → stack overflow risk. (folder 08/05)

---

## Lambdas  (folder 22)

```cpp
auto add = [](int a, int b) { return a + b; };
auto f = [x, &y, this](int a) mutable -> int { return x + y + a; };
//        ^cap-by-val  ^cap-by-ref        ^ret   ^can modify captured-by-value
auto g = [](auto x) { return x * 2; };             // generic (templated operator())
auto h = [&](){ ... };                             // capture-all-by-ref (careful: dangling)
auto k = [=](){ ... };                             // capture-all-by-copy
```

`[&]` + stored/async lambda → **dangling capture**. Synchronous use only, ya
capture explicitly.

---

## Classes  (folders 15, 16, 17, 18)

```cpp
class Widget {
public:
    Widget() = default;
    explicit Widget(int v) : val_(v) {}            // member-init list (order = decl order)
    Widget(const Widget&)            = default;    // copy ctor
    Widget(Widget&&) noexcept        = default;    // move ctor  (noexcept! -> vector relocs)
    Widget& operator=(const Widget&) = default;    // copy assign
    Widget& operator=(Widget&&) noexcept = default;// move assign
    ~Widget()                        = default;    // dtor  (virtual if polymorphic base!)

    int  value() const { return val_; }           // const member fn
    void set(int v) { val_ = v; }
    static int count();                            // no `this`

private:
    int val_ = 0;                                 // default member initializer
};

struct Point { int x, y; };                        // struct = class with public default
```

**Rule of 0**: koi special member mat likho, compiler ke defaults + RAII members
use karo. **Rule of 3/5**: agar ek (dtor/copy/move) likha, sab ke baare mein
socho. (folder 18)

Inheritance / virtual:
```cpp
struct Base { virtual void f(); virtual ~Base() = default; };
struct Derived : Base { void f() override; };      // `override` — always use it
Base* b = new Derived;  b->f();  delete b;         // virtual dtor -> ~Derived runs
```

---

## Templates  (folder 21)

```cpp
template <class T>              T   max_of(T a, T b) { return a < b ? b : a; }
template <class T, int N>       struct Array { T data[N]; };
template <class... Ts>          auto sum(Ts... xs) { return (xs + ...); }  // fold
template <std::integral T>      T   half(T x) { return x / 2; }           // concept (C++20)

template <class T> requires std::is_trivially_copyable_v<T>
void blit(T&);

if constexpr (std::is_pointer_v<T>) { ... }        // compile-time branch
```

---

## Enums, namespaces, aliases

```cpp
enum class Color : std::uint8_t { Red, Green, Blue };   // scoped, fixed underlying type
Color c = Color::Red;  auto n = std::to_underlying(c);  // C++23

namespace app { namespace net { ... } }
namespace app::net { ... }                               // C++17 nested
namespace fs = std::filesystem;                          // alias

using Bytes = std::vector<std::byte>;                    // type alias (prefer over typedef)
template <class T> using Vec = std::vector<T>;           // alias template
```

---

## Error handling  (folder 23)

```cpp
throw std::runtime_error("msg");
try { ... } catch (const std::exception& e) { log(e.what()); } catch (...) { }

std::optional<int> parse(std::string_view);             // "maybe a value"
std::expected<int, Err> parse2(std::string_view);       // value OR error (C++23)
noexcept                                                // hot path: prefer error codes
```

---

## Common one-liners

```cpp
std::swap(a, b);
auto v = std::exchange(x, newval);                 // set x, return old
[[maybe_unused]] int dbg = compute();
static_assert(sizeof(int) >= 4, "need 32-bit int");
#include <cassert>   assert(inv());                // NDEBUG disables
std::size(arr);  std::ssize(arr);                  // size as size_t / ptrdiff_t
```

## Next
→ [`02-stl-containers.md`](02-stl-containers.md)

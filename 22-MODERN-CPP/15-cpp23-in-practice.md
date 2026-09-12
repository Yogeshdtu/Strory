# 15 — C++23 in practice: chala ke, naap ke

## Prerequisites
- [`05-cpp23-features.md`](05-cpp23-features.md) — C++23 ka naksha (yeh file usi ko gehra karti hai)
- [`06-lambdas-deep.md`](06-lambdas-deep.md), [`08-ranges-deep.md`](08-ranges-deep.md), [`09-coroutines.md`](09-coroutines.md), [`13-format-and-print.md`](13-format-and-print.md)
- `18-COPY-MOVE/12-perfect-forwarding.md` (`std::forward`), `23-ERROR-HANDLING/10-std-expected.md`
- `20-ALGORITHMS-DSA/17-cache-aware-dsa.md` (flat containers kyun cache-friendly hote hain)

## Yeh topic abhi kyun
File 05 ne C++23 ki list dikhayi thi — par wahan likha tha "discussed hai, compile-verified
nahi". Ab aapke paas coroutines, ranges, lambdas aur `std::format` sab aa chuke hain, to
C++23 ke features **asli code mein chala ke** dekh sakte ho.

Is file mein teen kaam honge:
1. Har kaam ka feature ek chalte hue example se (`examples/09_cpp23_in_practice.cpp23.cpp`).
2. Saaf batana ki **is toolchain pe kya nahi chalta** — chupana nahi.
3. `std::flat_map` vs `std::map` ka **asli benchmark** — aur uska result textbook wale
   jawab se poora match nahi karta. Wahi is file ka sabse achha lesson hai.

---

## Is box pe kya chalta hai — pehle yeh check karo

C++23 "aa gaya" bolna aasaan hai. Asli sawaal: **aapke compiler + standard library** mein kya
hai? Maine MinGW GCC 16.2 pe 26 features ek-ek chhote program se test kiye
(`-std=c++23 -lstdc++exp`, compile + run + result check):

| Status | Features |
|---|---|
| ✅ Seedha chalte hain (24) | deducing `this`, recursive lambda, `static operator()`, `m[i, j]`, `auto(x)`, `if consteval`, `[[assume]]`, `std::unreachable`, `std::to_underlying`, `std::byteswap`, `string::contains`, `std::expected` (monadic), `std::generator`, `std::flat_map`, `std::flat_set`, `std::move_only_function`, `std::mdspan`, `views::enumerate`, `views::zip`, `views::chunk`/`slide`, `views::pairwise`, `std::ranges::to`, `std::ranges::fold_left`, `std::print` |
| 🔧 Setup ke saath chalta hai (1) | `import std;` — pehle standard library ka module ek baar build karna padta hai (section 10) |
| ❌ Is build mein nahi hai (1) | `std::stacktrace` — header file maujood hai, par `__cpp_lib_stacktrace` define nahi hai (library backtrace support ke bina build hui hai) |

**Ek asli kahani:** yahi probe pehle GCC 15.1 pe chalaya tha — tab `<mdspan>` ka header hi
nahi tha. Compiler 16.2 pe upgrade hua, aur `mdspan` chal gaya. Isliye "C++23 support hai
ya nahi" ka jawab **compiler version ke saath badalta hai** — har upgrade pe dobara check karo.

Do baatein yaad rakho:
- **Language aur library alag cheezein hain.** Compiler naya syntax samajh leta hai (deducing
  `this`), par library feature shayad us build mein na ho (`std::stacktrace`). Header file
  maujood hona bhi kaafi nahi — `<stacktrace>` file yahan hai, par andar ka code band hai.
- **Apne code mein check karo, maan ke mat chalo** — feature-test macros se:
  ```cpp
  #include <version>
  #if __cpp_lib_stacktrace >= 202011L
      // std::stacktrace use karo
  #else
      // fallback: bina stack trace ke log karo (ya platform tool)
  #endif
  ```

> Build note: is repo mein C++23 examples ka naam `NN_name.cpp23.cpp` hota hai.
> `build.ps1` aur `Makefile` aisi files ko apne aap `-std=c++23 -lstdc++exp` se compile karte
> hain. Baaki repo `-std=c++20` pe hi rehta hai.

---

## 1. Deducing `this` — ek function, teen overloads ki jagah

### Problem kya thi
Maan lo ek class hai jo andar ek `std::string` rakhti hai, aur aapko `value()` getter chahiye.
Sahi C++ (C++23 se pehle) mein yeh likhna padta tha:

```cpp
struct Holder {
    std::string data;
    std::string&        value() &       { return data; }            // normal object
    const std::string&  value() const&  { return data; }            // const object
    std::string&&       value() &&      { return std::move(data); } // temporary -- move karne do
};
```

Teen baar wahi logic. Agar logic lamba ho, teen jagah bug.

### C++23 ka tareeka
`this` ko ek **normal parameter** ki tarah likh do. Compiler khud deduce karega ki object
lvalue hai, const hai, ya temporary:

```cpp
struct Holder {
    std::string data = "order-42";

    template <class Self>
    auto&& value(this Self&& self) {                  // `this Self&& self` = explicit object parameter
        return std::forward<Self>(self).data;         // category aage pass ho gayi
    }
};
```

`Self&&` yahan wahi forwarding reference hai jo aapne folder 18 file 12 mein dekha tha.
Is example ne yeh print kiya:

```
  h.kind()        -> lvalue
  ch.kind()       -> const lvalue
  Holder{}.kind() -> rvalue (temporary)
  value() from temporary moved out: "order-42"
```

Matlab ek hi function ne teeno case sahi pakde, aur temporary se string **move** hui — copy nahi.

### Recursive lambda — bonus
C++23 se pehle lambda khud ko call nahi kar sakta tha (uska apna naam nahi hota). Log
`std::function` mein daalte the — jo type erasure aur indirect call ka kharcha jodta hai.
Ab lambda khud ko parameter ki tarah le sakta hai:

```cpp
auto fib = [](this auto self, int n) -> long long {
    return n < 2 ? n : self(n - 1) + self(n - 2);
};
fib(30);   // 832040
```

### Andar kya hota hai
- `value(this Self&& self)` ek **member function template** hai. Har alag `Self`
  (`Holder&`, `const Holder&`, `Holder`) ke liye alag instantiation banta hai — bilkul
  jaise aapne teeno overloads khud likhe hote.
- Function ke andar **koi chhupa hua `this` pointer nahi** hota. Member ko `self.data`
  likh ke hi use kar sakte ho — sirf `data` likhoge to error.
- Kyunki yeh template hai, yeh **`virtual` nahi ho sakta**.
- Recursive lambda mein `self` lambda object khud hai (by value, `this auto`). Stateless
  lambda ka size 1 byte hota hai, to copy practically free hai.

---

## 2. `static operator()` aur `m[i, j]`

```cpp
struct TickToPaise {
    static std::int64_t operator()(std::int64_t ticks) { return ticks * 5; }  // 1 tick = 5 paise
};
TickToPaise{}(7);   // 35
```

Stateless function object (jaise comparator, hash, converter) ke `operator()` ko ab
`static` bana sakte ho. Fark: call karte waqt **`this` pointer pass hi nahi hota** — ek
chhupa hua argument kam.

```cpp
struct Grid {
    int cols;
    std::vector<int> cells;                              // flat, row-major
    int& operator[](int r, int c) { return cells[r * cols + c]; }
};
g[1, 2] = 99;    // C++23: asli do-argument subscript
```

⚠️ C++20 mein `g[1, 2]` ka matlab tha `g[(1, 2)]` — comma operator, yaani sirf `g[2]`!
C++20 ne us use ko deprecate kiya, C++23 ne comma ko asli multi-arg subscript bana diya.

Upar ka `Grid` khud banaya hua **flat vector + stride** hai — wahi pattern jo folder 09
file 06 aur folder 39 ki flat book mein hai. `std::mdspan` yahi cheez standard library mein
deta hai:

```cpp
std::mdspan view(g.cells.data(), 2, 3);   // 2 rows, 3 cols -- memory g.cells ki hai
view[0, 1] = 7;                           // view se likha -> g.cells[1] badal gaya
view.extent(0);                           // 2
```

Output: `mdspan view[1, 2] = 99   g.cells[1] = 7   extents 2x3   sizeof(view) = 24 bytes`

`mdspan` **non-owning view** hai — `std::span` ka multi-dimensional bhai. Woh memory ka
maalik nahi; sirf pointer + dimensions rakhta hai. Isliye jis buffer pe bana hai, woh
view se zyada jeena chahiye (warna dangling — folder 12 file 12). `sizeof(view)` wala number
upar ke run se hai; runtime extents (`2, 3`) bhi view ke andar store hote hain.

---

## 3. `std::generator` — lazy sequence bina plumbing ke

Folder 22 file 09 mein aapne `Generator<T>` khud banaya tha — `promise_type`,
`coroutine_handle`, iterator, sab. C++23 mein yeh standard library mein hai:

```cpp
struct Tick { std::uint64_t seq; std::int64_t px; };

std::generator<Tick> replay(std::int64_t start_px) {
    std::int64_t px = start_px;
    for (std::uint64_t seq = 1;; ++seq) {        // infinite loop -- darne ki baat nahi
        px += (seq % 3 == 0) ? -1 : 1;
        co_yield Tick{seq, px};                  // ek tick do, phir yahin ruk jao
    }
}

for (const Tick& t : replay(10'000) | std::views::take(5))
    std::println("seq={} px={}", t.seq, t.px);
```

Output:
```
  seq=1 px=10001
  seq=2 px=10002
  seq=3 px=10001
  seq=4 px=10002
  seq=5 px=10003
```

Loop infinite hai, par program atakta nahi — kyunki generator **lazy** hai. Jitne ticks
maange (`take(5)`), utne hi bane. Poora vector memory mein kabhi nahi bana.

### Andar kya hota hai
- `replay(...)` call karne pe body **chalti hi nahi**. Coroutine frame banta hai aur turant
  ruk jaata hai.
- `begin()` pe body pehle `co_yield` tak chalti hai. Har `++it` pe agle `co_yield` tak.
- Coroutine frame (locals: `px`, `seq`) aam taur pe **heap pe allocate** hota hai — ek
  generator = ek allocation. `std::generator` ne boilerplate hataya, **cost nahi** badli
  (file 09 wala analysis waisa hi hai).

---

## 4. Ranges ke naye adaptors

```cpp
std::vector<std::int64_t> bid_px{100, 99, 97};
std::vector<std::int64_t> bid_qty{5, 12, 40};

for (auto [level, px] : std::views::enumerate(bid_px))      // index + value
    ...                                                      // level 0 px 100, ...

auto notional = std::views::zip(bid_px, bid_qty)             // do ranges ek saath
              | std::views::transform([](auto pq) { auto [p, q] = pq; return p * q; })
              | std::ranges::to<std::vector>();              // seedha vector bana do
                                                             // [500, 1188, 3880]

for (auto [a, b] : bid_px | std::views::pairwise)            // lagataar jodiyaan
    ...                                                      // gap 100 -> 99 = 1, 99 -> 97 = 2

for (auto batch : std::views::iota(1, 8) | std::views::chunk(3))
    ...                                                      // sizes 3, 3, 1

std::ranges::fold_left(bid_qty, std::int64_t{0}, std::plus{});   // 57
```

| Adaptor | Ek line mein | Kahan kaam aata |
|---|---|---|
| `views::enumerate` | index bhi do, value bhi | manual `i` counter ki galtiyan khatam |
| `views::zip` | kai ranges ek saath, sabse chhoti tak | SoA data (alag price aur qty arrays) |
| `views::pairwise` | `(a0,a1), (a1,a2), ...` | gaps, deltas, returns |
| `views::chunk(n)` | n-n ke tukde, aakhri chhota | batching (folder 36 file 16) |
| `ranges::to<C>` | pipeline ko container bana do | `std::vector` materialize karna |
| `ranges::fold_left` | `accumulate` ka ranges version | typed, constrained reduce |

⚠️ `views::zip` sabse **chhote** range pe ruk jaata hai. Agar `bid_px` mein 3 aur `bid_qty`
mein 2 elements hon, to teesra level chupchaap chhoot jaayega — koi error nahi.

---

## 5. `std::expected` — monadic chain

Poori detail folder 23 file 10 mein hai. Yahan sirf chain ka shape:

```cpp
auto lots = parse_qty(input)                                   // expected<int, Err>
    .and_then([](int q) -> std::expected<int, Err> {           // success pe agla step
        if (q == 0) return std::unexpected(Err::bad_qty);
        return q;
    })
    .transform([](int q) { return q / 50; })                   // value ko badlo
    .or_else([](Err e) -> std::expected<int, Err> {            // error pe (log / map)
        return std::unexpected(e);
    });
```

Output:
```
  "250" -> 5 lots
  "" -> error 0
  "2x0" -> error 1
```

Pehli galti jahan hui, wahi error aakhir tak chali — beech ke steps chale hi nahi.

---

## 6. `std::move_only_function`

```cpp
auto order = std::make_unique<std::int64_t>(123);

// std::function<std::int64_t()> bad = [o = std::move(order)] { return *o; };
//   ^ COMPILE ERROR -- std::function ko callable copy karna padta hai, unique_ptr copy nahi hota

std::move_only_function<std::int64_t()> task = [o = std::move(order)] { return *o; };
task();   // 123
```

`std::function` andar callable ki **copy** bana sakta hona chahiye. Isliye `unique_ptr`
pakadne wala lambda usme nahi jaata, aur log `shared_ptr` ka jugaad karte the (extra
allocation + atomic refcount). `std::move_only_function` sirf move maangta hai — task queues,
callbacks jo ownership rakhte hain, unke liye sahi type.

---

## 7. Chhote par kaam ke utilities

```cpp
enum class Side : std::uint8_t { Buy = 'B', Sell = 'S' };
std::to_underlying(Side::Sell);             // 'S' -- static_cast<std::underlying_type_t<...>> ki jagah

int side_sign(Side s) {
    switch (s) {
        case Side::Buy:  return +1;
        case Side::Sell: return -1;
    }
    std::unreachable();                     // "yahan kabhi nahi aayenge" -- galat nikla to UB
}
```

**Endianness — wire se aaye bytes:**
```cpp
const unsigned char wire_bytes[4] = {0x00, 0x00, 0x27, 0x10};   // 10000, big-endian
std::uint32_t raw = 0;
std::memcpy(&raw, wire_bytes, sizeof raw);   // x86 little-endian hai -> galat padha
std::byteswap(raw);                          // 10000
```
Output: `wire bytes read as-is = 270991360   byteswap -> 10000`. Wahi 4 bytes, ulta padhne se
bilkul alag number. Market data protocols (ITCH jaise) big-endian hote hain — folder 38.

```cpp
std::string sym = "NSE:RELIANCE";
sym.contains("NSE:");                     // true -- find() != npos se saaf

auto copy = auto(original);               // auto(x): "iski decay-copy banao", intent saaf

constexpr int ct = where_am_i();          // if consteval -> compile-time branch: 1
int rt = where_am_i();                    // runtime branch: 2
```

`[[assume(expr)]]` bhi C++23 mein hai — optimizer ko batata hai "yeh sach hai". Par agar
runtime pe galat nikla, to **UB**. Sirf wahan likho jahan pehle se validate kiya ho.

---

## 8. `std::print` / `std::println` — aur ek link surprise

```cpp
#include <print>
std::println("{} + {} = {}", 2, 3, 2 + 3);   // newline khud lagata hai
```

Type-safe hai (`{}` aur argument mismatch = compile error) aur iostream `<<` chain se saaf.
File 13 mein format spec detail mein hai.

**Is box pe ek asli jhatka:** `g++ -std=c++23 file.cpp` pe **link error** aaya:

```
undefined reference to `std::__open_terminal(_iobuf*)'
undefined reference to `std::__write_to_terminal(void*, std::span<char, ...>)'
```

MinGW GCC pe (15.1 aur 16.2 dono pe check kiya) `std::print` ka Windows-terminal wala hissa
`libstdc++exp` library mein hai. Fix: link line ke **aakhir** mein `-lstdc++exp` jodo. Sabak: "header include ho gaya"
ka matlab "feature poora mil gaya" nahi hota — link bhi karke dekho.

> **HFT relevance:** `std::print` logging ke liye achha hai, par **tick path pe nahi**.
> Koi bhi formatted I/O (formatting + syscall) hot path ke microsecond budget ko kha jaata
> hai. Hot path pe raw data ek SPSC queue mein daalo, formatting doosre thread pe karo
> (folder 45 file 10).

---

## 9. `std::flat_map` vs `std::map` — theory vs naap

### Theory kya kehti hai
- `std::map` = red-black tree. Har element ek **alag heap node**.
- `std::flat_map` = do sorted **contiguous vectors** (keys aur values alag). Lookup =
  binary search.
- Isliye "flat_map lookup cache-friendly hai, tez hoga." Aur insert O(n) hai kyunki beech
  mein jagah banane ke liye elements khiskane padte hain.

### Memory model — maine khud check kiya
Ek counting allocator se: `std::map<int64_t, int64_t>` ka **har node 48 bytes** ka ek alag
allocation hai (libstdc++, x86-64). Aur `flat_map` sach mein do vectors rakhta hai —
`keys()` aur `values()`.

```
std::map  (231,845 keys)                      std::flat_map (231,845 keys)
                                              keys()   [k0|k1|k2|k3|...........]  ~1.85 MB, ek block
  [node 48B]      [node 48B]                  values() [v0|v1|v2|v3|...........]  ~1.85 MB, ek block
      \  heap mein bikhre hue  /
   [node]  [node]   [node]  [node]            find(k): keys() pe binary search -> index i
   ~11 MB nodes, har level pe naya             phir values()[i]
   cache line; ~18 levels gehra
```

### Naap (MinGW GCC 16.2, `-O2`, best of 5, 3 alag runs ka range)

Lookup — 2,000,000 random `find()`, keys pehle se bani hui (sirf lookup time naapa):

| Keys | `std::map` | `std::flat_map` | flat kitna tez |
|---|---|---|---|
| **55** | 19.0–19.2 ns | 20.7–20.9 ns | **0.91–0.93×** — map TEZ hai |
| **3,660** | 55.8–56.2 ns | 48.7–48.8 ns | 1.14–1.15× |
| **231,845** | 512–518 ns | 101–104 ns | **4.98–5.09×** |

Insert — random order mein `emplace` (naya container har run):

| Keys | `std::map` | `std::flat_map` | flat kitna slow |
|---|---|---|---|
| 1,000 | 118–122 ns | 127–130 ns | 1.05–1.09× |
| **50,000** | 203–211 ns | 4,542–4,610 ns | **~22×** slow (21.7–22.3×) |

### Isse kya seekha
1. **Chhote size pe flat_map ka faayda gayab — map thoda tez nikla.** 55 keys ke map
   nodes (~2.6 KB) aur flat_map ke vectors dono L1 cache mein aaram se aa jaate hain, to
   "contiguous memory" ka koi faayda bacha hi nahi. Jo ~2 ns ka fark hai woh
   implementation ka hai — ek sambhavit wajah: flat_map keys aur values **alag vectors**
   mein rakhta hai, to key milne ke baad value ke liye doosri jagah jaana padta hai.
   (Yeh andaaza hai, naapa nahi — exercise 3 mein aap khud test karoge.)
2. **Bade size pe cache ka asar dikhta hai — 5× tak.** 231k nodes ≈ 11 MB bikhri memory
   hai; tree ke har level pe cache miss ka khatra. flat_map ke keys ~1.85 MB mein ek jagah
   hain.
3. **Random insert bade size pe bahut mehnga hai — ~22×.** Har insert beech mein jagah
   banane ke liye average aadhe elements dono vectors mein khiskata hai.
4. **"flat_map tez hai" poora sach nahi.** Sahi baat: *bade, read-mostly* data pe lookup
   tez hai; chhote data pe koi faayda nahi; random inserts pe bahut slow. Pehle naapo.

> **HFT relevance:** Order book ke price levels ke liye folder 39 ne `flat_map` nahi,
> **tick-indexed flat array** chuna tha — kyunki updates top-of-book ke paas lagataar hote
> hain, aur flat_map ka beech mein insert O(n) hai. `flat_map` wahan achha hai jahan data
> **ek baar bane aur baad mein sirf padha jaaye** — jaise startup pe load hone wali symbol
> table ya instrument config. Bulk data ho to pehle ek `vector` mein bharo, sort karo, phir
> `flat_map` banao (sorted-unique constructor) — beech-beech mein insert se bacho.

---

## 10. `import std;` — setup, aur asli faayda kitna hai

`import std;` poori standard library ko ek **module** ki tarah laata hai. Har `.cpp` file mein
`<vector>`, `<string>`, `<map>`... ke hazaaron lines dobara parse nahi hoti — compiler ek
pehle se bani binary module file (`.gcm`) padh leta hai. Modules ki basics file 10 mein hain.

**GCC 16.2 pe setup (is box pe chala ke verify kiya):**
```bash
# 1. EK BAAR: standard library ka module banao -> gcm.cache/std.gcm
g++ -std=c++23 -fmodules -fsearch-include-path -c bits/std.cc

# 2. Har file: -fmodules ke saath compile karo
g++ -std=c++23 -fmodules main.cpp -o main -lstdc++exp
```
```cpp
import std;                                   // koi #include nahi
int main() {
    std::vector<int> v{1, 2, 3};
    std::println("import std ok: {}", v.size());
}
```

**Kitna tez? Naapa** (ek file jo 9 headers use karti hai — `algorithm`, `chrono`, `format`,
`map`, `print`, `ranges`, `string`, `unordered_map`, `vector`; sirf compile, `-O0`, best of 5):

| Tareeka | Compile time |
|---|---|
| 9 `#include` | 3,936 ms |
| `import std;` | 2,279 ms (~1.7× tez) |
| Module ek baar banana (`bits/std.cc`) | 7,844 ms |

Matlab: **ek akeli file ke liye faayda nahi** — module banane mein hi 7.8 s lag gaye. Har file
pe ~1.66 s bachte hain, to ~5 files ke baad hisaab barabar, uske baad har file pe bachat.
Hazaaron files wale project mein yahi bada fark banta hai. "Huge win" project size pe depend
karta hai — ek file pe nahi.

⚠️ Do baatein jo chala ke pakdi gayin (GCC 16.2):

1. **Module macros nahi deta.** `import std;` ke baad `assert(...)` aur `INT_MAX` pe
   `was not declared` error aaya. Macros ke liye `#include <cassert>` / `<climits>` chahiye.
2. **Order matter karta hai.** Woh `#include` `import std;` se **pehle** likho:
   ```cpp
   #include <cassert>    // ✅ pehle
   #include <climits>
   import std;
   ```
   `import std;` ke **baad** koi bhi `#include` (`<assert.h>` bhi) likha to libstdc++ ke
   andar `redefinition of 'void std::__terminate()'` jaise errors aaye.

### Jo is build mein nahi hai: `std::stacktrace`

`std::stacktrace::current()` call stack ko ek value ki tarah pakad leta hai — error log mein
"yeh kahan se call hua tha" dikhane ke liye. Is MinGW build mein `<stacktrace>` header hai,
par `__cpp_lib_stacktrace` define nahi — library backtrace support ke bina bani hai. Tab tak
stack traces ke liye folder 45 ke tools (gdb `bt`, sanitizers) use karo.

---

## Andar kya hota hai — ek jagah

- **Deducing `this`**: member function template; har object category ka alag
  instantiation; function ke andar implicit `this` nahi; `virtual` nahi ho sakta.
- **`static operator()`**: `this` pointer pass nahi hota — stateless functors ke liye ek
  argument kam.
- **`std::generator`**: coroutine frame aam taur pe heap pe; body pehle `begin()` tak nahi
  chalti; har `++it` agle `co_yield` tak resume.
- **`std::move_only_function`**: type-erased callable jo copy nahi, sirf move maangta hai;
  `std::function` jaisa hi indirect call.
- **`std::flat_map`**: do sorted vectors; lookup binary search; insert/erase O(n) shift;
  insert ke baad purane iterators invalid (vector jaisa).
- **`std::byteswap`**: x86 pe ek `bswap` instruction.
- **`std::print`**: `std::format` wala engine + direct write; MinGW pe `-lstdc++exp`.

---

## Hands-on

```bash
# repo helper -- .cpp23.cpp naam dekh ke khud -std=c++23 -lstdc++exp laga deta hai
./build.ps1 fast 22-MODERN-CPP/examples/09_cpp23_in_practice.cpp23.cpp

# haath se (benchmark ke liye -O2 ZAROORI; -O0 pe timing ka koi matlab nahi)
g++ -std=c++23 -Wall -Wextra -O2 22-MODERN-CPP/examples/09_cpp23_in_practice.cpp23.cpp -o cpp23 -lstdc++exp && ./cpp23

# "What happens next?" ke jawab -- pehle khud socho, phir chalao
./build.ps1 22-MODERN-CPP/examples/10_cpp23_what_happens_next.cpp23.cpp
```

Chalao, apne numbers file 9 wali tables se milao. Alag machine pe absolute ns alag honge —
**ratios aur shape** (chhote pe map ≈ flat, bade pe flat bahut tez, random insert bahut
slow) wahi rehne chahiye. Na rahein to wahi aapka naya lesson hai.

---

## ⚠️ Traps

### Trap 1 — "C++23 flag laga diya, sab mil gaya"
```cpp
#include <stacktrace>
auto t = std::stacktrace::current();   // ❌ is box pe: 'std::stacktrace' has not been declared
```
Header file mil gayi, phir bhi feature nahi mila. Language support aur library support alag
hain — `__cpp_lib_*` macros se check karo, aur compiler upgrade ke baad dobara check karo
(GCC 15.1 pe `<mdspan>` nahi tha, 16.2 pe hai).

### Trap 2 — `std::print` compile hua par link fail
```bash
g++ -std=c++23 f.cpp -o f               # ❌ undefined reference to std::__open_terminal (MinGW)
g++ -std=c++23 f.cpp -o f -lstdc++exp   # ✅ library source ke BAAD
```

### Trap 3 — `import std;` ke baad `#include`
```cpp
import std;
#include <cassert>        // ❌ GCC 16.2: redefinition of 'void std::__terminate()' ...
```
`#include` wali lines `import std;` se pehle rakho (section 10).

### Trap 4 — deducing `this` mein member ko seedha naam se use karna
```cpp
template <class Self>
auto&& value(this Self&& self) { return data; }   // ❌ ERROR: yahan implicit this nahi hai -- self.data likho
```

### Trap 5 — `std::forward<Self>` bhool jaana
```cpp
auto&& value(this Self&& self) { return self.data; }   // ⚠️ `self` naam wala variable hamesha lvalue hai
                                                        //    -> temporary se bhi copy/lvalue-ref milega, move nahi
```

### Trap 6 — `views::zip` chupchaap chhota ho jaata hai
```cpp
std::vector<int> px{100, 99, 97}, qty{5, 12};
for (auto [p, q] : std::views::zip(px, qty)) { }   // ⚠️ sirf 2 baar chalega -- 97 chhoot gaya, koi error nahi
```

### Trap 7 — flat_map mein loop ke andar random inserts
```cpp
std::flat_map<std::int64_t, Level> book;
for (auto& msg : feed) book.emplace(msg.px, ...);   // ⚠️ bade size pe har insert O(n) shift (~22x slow naapa)
```
Bulk data: pehle vector mein bharo, sort karo, ek baar mein `flat_map` banao.

### Trap 8 — flat_map insert ke baad purana iterator
```cpp
auto it = m.find(10);
m.emplace(5, 0);          // ⚠️ vectors mein shift/realloc -> `it` ab invalid (vector jaise rules)
it->second = 1;           // UB
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`-std=c++23` lagao, saare features mil jaayenge" | Library support alag hai — is build mein `stacktrace` nahi; `import std` ko module setup chahiye |
| "`import std;` har file ko bahut tez compile karta hai" | Naapa: ~1.7× per file, par module banane ki one-time cost 7.8 s — ~5 files ke baad faayda |
| "Deducing `this` bas `this` ka naya naam hai" | Object ek explicit parameter ban jaata hai → ek template saare const/ref cases sambhalta hai |
| "`std::generator` coroutine ki cost khatam karta hai" | Sirf boilerplate hataata hai; frame allocation waisa hi |
| "`std::flat_map` hamesha `std::map` se tez hai" | Naapa: 55 keys pe map tez, 3.6k pe ~1.15×, 231k pe ~5×; random insert ~22× slow |
| "`std::move_only_function` = `std::function` bina copy ke, isliye free" | Wahi type-erased indirect call hai; sirf copy ki shart hati |
| "`std::print` hot path ke liye theek hai" | Formatting + I/O — tick path se bahar rakho |
| "`g[1, 2]` C++20 mein bhi do-argument tha" | C++20 mein woh comma operator tha → `g[2]` |

---

## Exercises

1. **Deducing `this` refactor:** neeche wali class ke teen `value()` overloads ko ek
   deducing-`this` function mein badlo. Phir `Holder{}.value()` se ek string move karke
   dikhao.
   ```cpp
   struct Holder {
       std::string data;
       std::string&       value() &      { return data; }
       const std::string& value() const& { return data; }
       std::string&&      value() &&     { return std::move(data); }
   };
   ```
   <details><summary>Answer</summary>

   ```cpp
   template <class Self>
   auto&& value(this Self&& self) { return std::forward<Self>(self).data; }
   ```
   `std::string s = Holder{"x"}.value();` — `Self` = `Holder`, `std::forward` rvalue
   deta hai, isliye `data` move hota hai. Bina `std::forward` ke `self.data` lvalue rehta
   aur copy hoti (Trap 5).
   </details>

2. **Recursive lambda:** ek lambda likho jo `std::vector<int>` ke nested index path pe
   binary search kare — ya simple rakho: factorial. Bina `std::function` ke.
   <details><summary>Answer</summary>

   `auto fact = [](this auto self, int n) -> long long { return n <= 1 ? 1 : n * self(n - 1); };`
   `fact(20)` = `2432902008176640000`. `self` lambda object khud hai; koi type erasure nahi,
   isliye compiler inline bhi kar sakta hai.
   </details>

3. **flat_map ka chhota-size result test karo:** hypothesis hai ki 55 keys pe flat_map
   isliye thoda slow hai kyunki keys aur values alag vectors mein hain. Test design karo:
   ek sorted `std::vector<std::pair<int64_t,int64_t>>` pe `std::ranges::lower_bound`
   (projection `&pair::first`) ka lookup time naapo aur 55 keys pe flat_map se compare
   karo. Kya fark gayab hua?
   <details><summary>Answer</summary>

   Yeh ek khula experiment hai — jawab aapka measurement hai, main pehle se nahi bataunga.
   Sahi design ke checklist: dono containers mein **same keys**, probes pehle se bane, `-O2`,
   best-of-N, aur checksum match. Agar pair-vector bhi ~21 ns aaye, to hypothesis galat thi
   (fark kahin aur hai — jaise binary search ka code); agar ~19 ns aaye, to alag vectors
   wali wajah sahi thi. Dono result valid lessons hain — jo aaye wahi likho (Rule 2).
   </details>

4. **Bulk build:** 50,000 random keys ko flat_map mein daalne ka tez tareeka likho, aur
   random `emplace` loop se time compare karo.
   <details><summary>Answer</summary>

   Keys ek `std::vector` mein bharo, `std::ranges::sort` + `std::ranges::unique` (duplicates
   hatao), values bhi usi order mein banao, phir `std::flat_map<K,V> m(std::sorted_unique,
   std::move(keys), std::move(values));`. Total kaam O(n log n) sort + O(n) — har insert pe
   O(n) shift nahi. Asli speedup apni machine pe naapo; random-emplace wala 50k case is box
   pe ~4.5–4.6 µs per insert tha (GCC 16.2).
   </details>

5. **Endianness:** bytes `{0x00, 0x01, 0x86, 0xA0}` ek big-endian `uint32_t` hain. Value kya
   hai, aur x86 pe `memcpy` ke baad bina swap ke kya milega?
   <details><summary>Answer</summary>

   Big-endian value `0x000186A0` = **100000**. x86 (little-endian) pe `memcpy` ke baad raw
   value `0xA0860100` = **2693136640**. `std::byteswap(raw)` → 100000. (Chala ke check kiya.)
   </details>

6. **Feature detection:** ek header likho jo `std::mdspan` ho to use kare, warna ek chhota
   `View2D` (pointer + rows + cols + `operator[](r, c)`) de, aur dono ka interface same rakhe.
   Kisi purane compiler (jaise GCC 15.1, jahan `<mdspan>` nahi tha) pe bhi compile hona chahiye.
   <details><summary>Answer</summary>

   `#include <version>`, phir `#if __cpp_lib_mdspan >= 202207L` → `#include <mdspan>` aur
   `template<class T> using View2D = std::mdspan<T, std::dextents<std::size_t, 2>>;` `#else` →
   apna `View2D<T>` jisme `T* data; std::size_t rows, cols;` aur `T& operator[](std::size_t r,
   std::size_t c) const { return data[r * cols + c]; }`. Dono `View2D<int> v{ptr, 2, 3};
   v[1, 2]` se chalte hain. GCC 16.2 pe `#if` branch compile hoti hai (chala ke check kiya);
   15.1 pe `#else`. Dhyaan: `#include <mdspan>` ko bhi `#if` ke **andar** rakho — bahar hoga
   to purane compiler pe "file not found" aayega.
   </details>

---

## What happens next?

Har snippet mein batao **agla step kya karega** — pehle apna jawab likho, phir
[`examples/10_cpp23_what_happens_next.cpp23.cpp`](examples/10_cpp23_what_happens_next.cpp23.cpp)
chala ke check karo. (Neeche ke jawab usi program ke asli output se hain.)

1. ```cpp
   Holder h;                                   // h.data = "order-42"
   std::string s = std::move(h).value();
   std::println("{} / {}", s, h.data);         // <- yeh line kya print karegi?
   ```
   <details><summary>Answer</summary>

   `order-42 / ` — `std::move(h)` ne `Self` = `Holder` banaya, `std::forward` ne rvalue diya,
   aur string **move** ho gayi. Is box (libstdc++) pe `h.data` khaali string (size 0) print
   hui. Standard sirf "valid but unspecified" guarantee karta hai — moved-from string pe
   khaali hone ka bharosa mat karo, bas use dobara assign karke use karo.
   </details>

2. ```cpp
   std::generator<int> ticks() {
       std::println("[gen] body started");
       for (int i = 1; i <= 3; ++i) { std::println("[gen] yield {}", i); co_yield i; }
   }
   auto g = ticks();
   std::println("after ticks()");
   auto it = g.begin();                        // <- ab kya print hoga, kis order mein?
   ```
   <details><summary>Answer</summary>

   ```
   after ticks()
   [gen] body started
   [gen] yield 1
   ```
   `ticks()` call pe body **nahi** chali — pehle "after ticks()" aaya. `begin()` ne body ko
   pehle `co_yield` tak chalaya. Agla `++it` "resumed" wala hissa aur `yield 2` chalayega.
   </details>

3. ```cpp
   std::move_only_function<int()> task = [p = std::make_unique<int>(7)] { return *p; };
   auto t2 = std::move(task);
   if (task) task(); else std::println("task empty");   // <- kaunsi branch?
   ```
   <details><summary>Answer</summary>

   Is box pe `task empty` — moved-from `move_only_function` `false` test hua, aur `t2()`
   `7` deta hai. Dhyaan: khaali `move_only_function` ko **call karna UB hai** (`std::function`
   jaise exception nahi aata). Isliye call se pehle `if (task)` check karo.
   </details>

4. ```cpp
   std::flat_map<int, int> m{{10, 1}, {30, 3}};
   auto [pos, ok] = m.emplace(20, 2);
   auto [pos2, ok2] = m.emplace(20, 99);       // <- ok2 aur pos2->second kya?
   ```
   <details><summary>Answer</summary>

   Pehla `emplace`: `ok = true`, key 20 index **1** pe (10 aur 30 ke beech — sorted order).
   Doosra: key pehle se hai, to `ok2 = false` aur value **2** hi rahi — 99 insert nahi hua
   (`emplace` overwrite nahi karta; overwrite chahiye to `insert_or_assign` ya `m[20] = 99`).
   </details>

---

## Challenge

### Challenge 1 — "Replay → book → notional", sirf C++23 tools se
`std::generator<Tick>` se 1,000,000 ticks banao (lazy). `views::chunk(1024)` se batches mein
lo. Har batch ke ticks ko ek `Grid`-style flat price ladder mein apply karo. Har batch ke
baad `views::zip` + `ranges::fold_left` se top 5 levels ka notional nikalo. Koi raw index
loop nahi. Phir **wahi pipeline** plain `for` loops se likho, `-O2` pe dono ka ns/tick naapo,
aur `-S` se dekho ki compiler ne ranges pipeline ko kitna inline kiya. Jo naapa wahi likho —
ranges slow nikle to woh bhi.

### Challenge 2 — `flat_map` crossover point dhoondo
`bench_lookup` ko 32 se 1,048,576 keys tak powers-of-2 pe chalao. Graph (ya table) banao:
kis size pe flat_map pehli baar map se aage nikalta hai, aur kis size pe ratio 2× cross
karta hai? Apne CPU ke L1/L2/L3 sizes dekho (folder 32) aur batao kya crossover points cache
sizes ke aas-paas aate hain — ya nahi. Andaaza aur naap alag nikle to dono likho.

---

## Interview questions

1. Deducing `this` kya solve karta hai? Iske baad function `virtual` kyun nahi ho sakta?
2. Explicit object parameter wale function mein `std::forward<Self>(self)` kyun zaroori hai?
3. `std::generator` ne coroutine ka kya badla, kya nahi (frame allocation)?
4. `std::flat_map` kab `std::map` se tez hai, kab nahi? Aapke paas numbers hon to kaise explain karoge?
5. `std::move_only_function` kyun aaya? `std::function` mein `unique_ptr` capture kyun nahi chalta?
6. `std::byteswap` HFT mein kahan lagta hai? x86 pe yeh kya instruction banta hai?
7. "Humare paas C++23 hai" — production mein yeh claim verify kaise karoge (language vs library, feature-test macros)?

---

## Next
→ [`16-exercises.md`](16-exercises.md)

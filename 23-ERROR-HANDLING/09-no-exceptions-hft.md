# 09 — `-fno-exceptions`: HFT mein kyun, kya kho jaata hai

## Prerequisites
- `08-exception-cost.md`, `06-noexcept.md`
- `02-return-codes-and-errno.md`
- [`examples/07_no_exceptions.cpp`](examples/07_no_exceptions.cpp)

## Yeh topic abhi kyun
Bahut sare low-latency / embedded / game-engine / kernel-adjacent C++ codebases
`-fno-exceptions -fno-rtti` se build hote hain. Yeh dogma nahi — ismein concrete
engineering benefits hain, aur concrete costs. Aapko dono pata hone chahiye, aur
"exceptions ke bina kaise likhein" ka poora toolkit.

---

## `-fno-exceptions` kya karta hai

| Cheez | `-fexceptions` (default) | `-fno-exceptions` |
|---|---|---|
| `throw` / `try` / `catch` | works | **compile error** |
| `noexcept` | contract | mostly no-op (sab kuch effectively noexcept) |
| unwind tables (`.eh_frame`, `.gcc_except_table`) | emitted | mostly gone |
| `.text` size | baseline | typically **5–15% chhota** |
| Inlining | cleanup-path CFG rok sakta | simpler CFG → **zyada inline** |
| `std::vector::at()` out-of-range | throws `out_of_range` | calls `std::__throw_out_of_range` → **`std::terminate`** |
| `new` (fail) | `std::bad_alloc` | `std::terminate` (use `new(nothrow)`) |
| `std::stoi` / `std::stod` bad input | throws | `terminate` |
| `dynamic_cast<T&>` fail | `std::bad_cast` | `terminate` (usually `-fno-rtti` bhi) |

`-fno-rtti` aksar saath aata: `typeid` / `dynamic_cast` (pointer form → `nullptr`,
reference form → problem), aur exception matching ka RTTI bhi. HFT builds typically
dono off.

---

## Benefits (kyun HFT karta hai)

1. **Latency determinism.** Koi `throw` = koi 6 µs unwinding spike (file `08`).
   Poora binary structurally exception-free — ek pura category of tail-latency
   gone.

2. **Chhota code → better I-cache behaviour.** Unwind tables + landing pads +
   cleanup code hatane se hot `.text` denser. Trading engine ka inner loop I-cache
   mein rehna chahiye; 10% code bloat woh margin kha sakta.

3. **Zyada aggressive optimization.** Har call jo throw kar sakta hai woh compiler
   ke liye ek implicit branch + cleanup edge banata hai. Woh gaya → simpler CFG →
   inliner zyada kaam karta hai, register allocator behtar, vectorizer kam roka
   jaata.

4. **Discipline / auditability.** Compiler enforce karta hai ki koi library call
   chupke se `throw` na kare. `v.at(i)`, `std::stoi`, `std::regex`, `std::filesystem`
   — inpe compile error milega (ya link error), forcing you to use the non-throwing
   path. Code review mein "yeh throw kar sakta hai?" sawaal hi khatam.

5. **ABI / FFI simplicity.** Exceptions ko C boundary / different-runtime `.so` ke
   paar throw karna UB. Exception-free binary is class of bug se free.

---

## Costs (kya kho jaata hai)

1. **Constructors error report nahi kar sakte** cleanly. `throw` nahi → ya
   two-phase init (`X x; if (auto e = x.init(...))`), ya factory (`static
   expected<X, Err> make(...)`), ya "ctor kabhi fail nahi hota" design (pre-validate).

2. **Deep call stacks mein manual error forwarding.** Har level pe `if (auto e =
   f(); e) return e;`. `std::expected` + `and_then` (file `10`) isse kam karta hai
   par khatam nahi.

3. **Third-party libraries jo throw karti hain** unusable ya risky — `-fno-exceptions`
   TU mein unki throwing path `terminate` karegi. Kai libs `-fno-exceptions` mode
   support karti hain (Abseil, {fmt}, etc.), kai nahi.

4. **Kuch STL cheezein off-limits ya careful:** `std::stoi` family, `std::any`,
   throwing `std::variant` access, `std::regex` (throws on bad pattern),
   `std::filesystem` (throwing overloads — non-throwing `error_code` overloads use
   karo).

5. **`new` ko handle karna padta hai** — `new(std::nothrow)` + null check, ya (HFT
   mein anyway) custom allocators / pre-allocated pools jahan "allocation fail"
   design se possible hi nahi hot.

---

## Exception-free toolkit

```cpp
// 1. Status enum + [[nodiscard]] — 2+ outcomes
enum class [[nodiscard]] Status : std::uint8_t { ok, overflow, bad_input, oom };

// 2. expected<T,E> — result ya reason (file 10)
std::expected<Order, RejectReason> build_order(const Msg&);   // C++23 / tl::expected

// 3. error_code — rich, portable, throw-free (file 11)
std::size_t Socket::read(std::span<std::byte>, std::error_code& ec);

// 4. optional<T> — sirf "hai ya nahi"
std::optional<Level> Book::best_bid() const;

// 5. FATAL / abort — truly unrecoverable (corrupt invariant)
#define FATAL(msg) (std::fprintf(stderr,"FATAL %s:%d %s\n",__FILE__,__LINE__,msg), std::abort())

// 6. Fixed-capacity containers jo overflow ko RETURN karte, throw nahi
template <class T, std::size_t N> class FixedVec { Status push_back(const T&); ... };

// 7. new(nothrow) jab heap chahiye
char* p = new (std::nothrow) char[n];
if (!p) return Status::oom;
```

**Precondition violations** (`assert`) — `-DNDEBUG` pe gayab, par debug builds
mein bug pakadte hain (file `12`). `-fno-exceptions` se orthogonal.

---

## Mixed mode — realistic setup

Bahut projects **poore binary** ko `-fno-exceptions` nahi karte; woh:

- **Hot path libraries / engine core**: `-fno-exceptions -fno-rtti`.
- **Bootstrap / config / admin / tooling**: normal, exceptions allowed.
- Ya ek hi binary, sab `-fno-exceptions`, aur startup errors `int main()` se
  `return 1` + stderr message.

Compiler flag **per-TU** hai — aap ek `.cpp` ko `-fno-exceptions` se aur doosre ko
bina, ek hi binary mein link kar sakte ho (bas exception us pehli TU se guzar na
jaaye — woh `terminate` karegi).

---

## Andar kya hota hai

- `-fno-exceptions` ke saath, libstdc++ ke throwing helpers (`__throw_length_error`
  etc.) `__cxa_throw` ki jagah **`std::abort`** (via `__gnu_cxx::__verbose_terminate_handler`
  se pehle `std::terminate`) call karte hain. Yaani woh code path abhi bhi hai, bas
  "recover" ki jagah "die".
- `throw`/`try`/`catch` tokens ko frontend reject karta hai (`-fno-exceptions`
  diagnostic).
- `.eh_frame` — kuch abhi bhi reh sakta (unwind info debuggers/profilers ke liye,
  `-fasynchronous-unwind-tables` default). `.gcc_except_table` (LSDA) mostly gone.
  Sach mein sab hatane ke liye `-fno-asynchronous-unwind-tables` bhi (par phir
  profiler stack unwind nahi kar paayega — trade-off).

---

## > **HFT relevance**
> Yeh **hai** HFT ka default error-handling posture. Ek typical low-latency shop:
>
> - Engine, feed handlers, order gateway, book, matching: `-fno-exceptions
>   -fno-rtti -fno-asynchronous-unwind-tables` (last one debatable — profiling
>   cost).
> - Har fallible hot function: `enum class Result` ya `expected` ya `error_code`,
>   `[[nodiscard]]`.
> - Allocation: hot path pe **zero** — arenas/pools pre-allocated at startup, so
>   "alloc failed" ek startup problem hai, runtime nahi.
> - Truly-broken invariant (crossed book jo cross nahi ho sakta, negative
>   inventory jahan namumkin): **`abort`** — process maro, hot standby seedha le
>   leta hai. "Chalte raho with corrupt state" = worst outcome (wrong orders).
> - Startup: exceptions theek (agar enabled) ya `main` se `return N` + clear log.
>
> Interview mein "aap exceptions use karte ho?" ka expected answer: *"Hot path pe
> nahi — `-fno-exceptions`, error values, aur unrecoverable pe fail-fast. Startup/
> control-plane pe theek hai."* Aur **kyun** bata pao (latency determinism, code
> size/I-cache, optimization, auditability).

---

## Hands-on

```bash
# normal build:
./build.ps1 23-ERROR-HANDLING/examples/07_no_exceptions.cpp

# no-exceptions build (Git Bash / direct g++):
g++ -std=c++20 -fno-exceptions -fno-rtti -O2 -Wall -Wextra \
    23-ERROR-HANDLING/examples/07_no_exceptions.cpp -o ne && ./ne
```

Example dono tarah compile+run hota hai (koi `throw` nahi). Phir usme ek line
`throw std::runtime_error("x");` add karo aur `-fno-exceptions` se compile karo —
error dekho. Phir `std::vector<int> v; v.at(99);` add karke `-fno-exceptions`
build ko **run** karo — `terminate` (abort) dekho.

Bonus: dono binaries ka `size` compare karo (`size ne_exc ne_noexc`), aur
`objdump -h` se `.eh_frame` / `.gcc_except_table` sections dekho.

---

## ⚠️ Traps

### Trap 1 — `-fno-exceptions` mein `v.at(i)` "safe" samajhna
```cpp
return v.at(idx);   // -fno-exceptions: out-of-range -> std::terminate, catch nahi
```
`if (idx < v.size())` khud check karo, ya `v[idx]` + apni bounds logic.

### Trap 2 — `new` ka `bad_alloc` handle na karna
```cpp
T* p = new T[n];    // -fno-exceptions: fail -> terminate
T* p = new (std::nothrow) T[n]; if (!p) return Status::oom;   // ✅
```

### Trap 3 — throwing library ko `-fno-exceptions` TU mein use karna
Library ka `throw` aapke TU se guzar ke `terminate`. Library ke `-fno-exceptions`
support / non-throwing API check karo.

### Trap 4 — `std::stoi` / `std::stod` / `std::filesystem` (throwing overloads)
```cpp
int n = std::stoi(s);                      // ⚠️ bad input -> terminate
auto [p, ec] = std::from_chars(b, e, n);   // ✅ non-throwing
std::filesystem::file_size(p);             // ⚠️ throws; use overload with error_code&
```

### Trap 5 — `noexcept` ko "ab bekaar hai" samajhna
`-fno-exceptions` mein `noexcept` mostly no-op **runtime pe**, par
`std::is_nothrow_move_constructible` jaise traits abhi bhi function ke declared
`noexcept` dekhte — aur agar aap kabhi mixed-mode ya library-facing code likho,
woh matter karta hai. `= default` moves rakho.

### Trap 6 — poora binary `-fno-exceptions`, phir startup error pe kuch nahi
`main` mein `if (!load_config(path)) { std::fprintf(stderr, "config: %s\n",
err.c_str()); return 2; }` — exceptions ke bina bhi startup errors ko clean report
karo.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`-fno-exceptions` = error handling chhod do" | Return codes / `expected` / `error_code` / `abort` — poora toolkit |
| "`-fno-exceptions` sirf `throw` disable karta" | Code chhota, inlining behtar, `at()`/`stoi`/`new` behaviour badalta |
| "`-fno-exceptions` mein `v.at()` safe" | Out-of-range → `terminate`. Khud bounds-check |
| "exceptions ka koi runtime cost nahi agar throw na karo" | Runtime ~0, par code size / inlining / audit cost hai — HFT inhi ke liye off karta |
| "sab kuch ek flag se, poora binary" | Per-TU flag; hot core off, bootstrap on — common |
| "`-fno-rtti` se `dynamic_cast` bas kaam nahi karega, harmless" | Design ko virtual-dispatch / visitor / `std::variant` pe shift karna padta |

---

## Exercises

1. **Toolkit choice:** `-fno-exceptions` build. In ke liye mechanism batao:
   (a) `Book::apply(Update)` jo sequence-gap detect kar sakta, (b) `Config`
   loading at startup, (c) `Pool::allocate()` jab pool full, (d) internal
   assertion "level count negative".

   <details><summary>Answer</summary>

   (a) `enum class ApplyResult { ok, gap, stale }` — hot, expected. (b)
   `expected<Config, ConfigErr>` — rich reason, startup. (c) `T* allocate()`
   returning `nullptr` (or `expected<Slot*, PoolErr>`) — expected under load.
   (d) `assert` in debug + `FATAL`/`abort` in release — that's a bug, not an
   error.
   </details>

2. **Spot the terminate:** `-fno-exceptions` build.
   ```cpp
   int rate = std::stoi(argv[1]);
   auto cfg = std::make_shared<Config>(load(argv[2]));
   double px = cfg->levels.at(0).price;
   ```
   Kitne terminate risks?

   <details><summary>Answer</summary>

   Three: `std::stoi` (bad/empty arg → terminate), `make_shared` (alloc fail →
   terminate; also `Config` ctor if it throws), `.at(0)` (empty → terminate).
   Non-throwing: `std::from_chars`, `new(nothrow)` / pool, `if
   (!cfg->levels.empty())`.
   </details>

3. **Size experiment:** predict — ek 50k-line engine ko `-fexceptions` se
   `-fno-exceptions` pe le jaao. `.text` +/-? Inlining? Ek risk?

   <details><summary>Answer</summary>

   `.text` ~5–15% chhota, `.gcc_except_table` gone, more inlining (simpler CFG).
   Risk: koi dependency ya STL path jo silently `throw` karta tha ab `terminate`
   karega — thorough testing + audit of `at()`, `stoi`, `variant` access, throwing
   `filesystem`/`regex` chahiye.
   </details>

4. **Constructor design:** `-fno-exceptions` mein `class TcpConn` jiska "connect"
   fail ho sakta. Teen valid designs?

   <details><summary>Answer</summary>

   (1) Two-phase: `TcpConn c; std::error_code ec = c.connect(addr);`. (2) Factory:
   `static std::expected<TcpConn, std::error_code> make(addr);` (move out on
   success). (3) Ctor never fails — takes an already-connected fd from a separate
   `connect()` free function.
   </details>

5. **Interview framing:** "Why does your shop disable exceptions?" — 4 concrete
   reasons in one breath.

   <details><summary>Answer</summary>

   Latency determinism (no 6 µs unwind spikes), smaller hot `.text` (I-cache),
   more aggressive inlining/optimization (no cleanup edges), and auditability
   (compiler forbids silent `throw` from library calls). Cost: manual error
   forwarding + factories for fallible constructors.
   </details>

---

## Interview questions

1. `-fno-exceptions` ke 3–4 concrete benefits (latency, size, opt, audit).
2. Uske costs — constructors, forwarding, libraries.
3. `-fno-exceptions` mein `std::vector::at()` out-of-range pe kya?
4. `new` fail hone pe `-fno-exceptions` mein kya, aur fix?
5. Kya ek hi binary mein `-fno-exceptions` aur `-fexceptions` TUs mix ho sakte hain?
6. `-fno-rtti` saath kyun aata, aur design pe kya asar?
7. Fallible constructor `-fno-exceptions` mein — patterns.
8. "Unrecoverable" error pe HFT engine kya karta hai aur kyun (`abort` + failover)?

---

## Next
→ [`10-std-expected.md`](10-std-expected.md)

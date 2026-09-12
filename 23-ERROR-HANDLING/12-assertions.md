# 12 — Assertions, `static_assert`, contracts

## Prerequisites
- `01-error-handling-strategies.md`
- `13-undefined-behaviour.md` bhi isi ke saath padho (assertions UB ko pakadne ka tool hai)
- Preprocessor basics (`NDEBUG`), `constexpr`

## Yeh topic abhi kyun
Assertion **error handling nahi** hai — yeh **bug detection** hai. Frank
distinction: error = "duniya ne kuch aisa diya jo handle karna hai" (bad input,
file missing). Bug = "mera code apni hi assumption tod raha hai". Assertions
doosre wale ke liye hain: sasta, loud, aur release build mein (mostly) invisible.
`static_assert` yeh compile time pe karta hai. Contracts (C++26) isko formalize
karte hain.

---

## `assert` — runtime, debug-only

```cpp
#include <cassert>

Level& Book::level_at(std::size_t i) {
    assert(i < levels_.size());        // "yeh kabhi false nahi hona chahiye"
    return levels_[i];
}
```

- Condition **true** → kuch nahi.
- Condition **false** → message (`file:line`, expression) stderr pe → `std::abort()`
  → `SIGABRT` → core dump.
- **`-DNDEBUG`** (release builds standard) → `assert(x)` poori tarah gayab —
  expression **evaluate bhi nahi hota**.

Yeh last point critical: `assert` ka argument **side-effect-free** hona chahiye.

```cpp
assert(pop_and_check());   // ⚠️ release mein pop_and_check() call HI NAHI hoga
```

### `assert` kis ke liye

- **Preconditions** — function ki assumptions on its args / object state.
  `assert(ptr != nullptr); assert(begin <= end); assert(is_sorted(v));`
- **Postconditions** — return karne se pehle result sane hai.
  `assert(result.size() == input.size());`
- **Invariants** — class ka internal consistency. `assert(size_ <= capacity_);`
- **"Can't happen" branches** — `default:` in a switch over a closed enum.
  `default: assert(false && "unhandled OrderType"); std::unreachable();`

### `assert` kis ke liye NAHI

- **User input validation** — `assert(argc == 2)` ❌. Woh error hai, bug nahi.
  Release mein assert gayab → koi check nahi → crash/UB. `if (argc != 2) { usage();
  return 2; }`.
- **Recoverable runtime conditions** — file missing, network timeout, alloc fail.
- **Kuch jispe aap actually react karna chahte ho.**

Rule of thumb: **"agar yeh trigger hua to maine (programmer ne) galti ki"** → assert.
**"agar yeh trigger hua to kisi aur ne / kisi cheez ne"** → error handling.

---

## Better assert messages

```cpp
assert(i < n && "index out of range in Book::level_at");   // string literal -> truthy, printed
```

`&& "message"` trick: string literal non-null → truthy → condition ka matlab nahi
badalta, par assert fail hone pe message mein dikhta hai.

---

## `static_assert` — compile time

```cpp
static_assert(sizeof(Tick) == 16, "Tick must stay cache-friendly (16B)");
static_assert(std::is_trivially_copyable_v<MarketUpdate>);
static_assert(alignof(RingSlot) >= 64, "false sharing risk");
static_assert(kQueueCapacity && (kQueueCapacity & (kQueueCapacity - 1)) == 0,
              "capacity must be a power of two");
```

- Condition ek **constant expression** honi chahiye.
- False → **compile error** with the message. Koi runtime, koi `NDEBUG` — hamesha.
- C++17: message optional (`static_assert(cond);`).
- Templates ke andar `static_assert(sizeof(T) <= 64, "...")` — instantiation pe
  fire hota, so ek clean error uss type ke liye.

Yeh sabse sasta assertion hai — user ke paas kabhi galat build pahunchta hi nahi.

---

## `assert` ke dost

| Tool | Kya | Release mein |
|---|---|---|
| `assert(x)` | debug precondition/invariant check | gayab (`NDEBUG`) |
| `static_assert(x)` | compile-time invariant | always (compile error) |
| `std::unreachable()` (C++23) | "control yahan pahunch hi nahi sakta" — optimizer ko batao | **UB agar reach ho** — assert ke baad lagao |
| `[[assume(expr)]]` (C++23) | "`expr` true maano" — optimizer hint | unchecked; false → UB |
| `-D_GLIBCXX_ASSERTIONS` | libstdc++ ke internal precondition checks (`vector::operator[]` bounds, etc.) | opt-in, small cost |
| Sanitizers (`-fsanitize=address,undefined`) | runtime UB/OOB detection | dev/CI builds |
| Custom `CHECK(x)` / `VERIFY(x)` macro | assert jo release mein bhi rehta | present (aapki choice) |

### `assert` vs `std::unreachable` vs `[[assume]]`

```cpp
switch (t) {
    case A: return handle_a();
    case B: return handle_b();
}
assert(false && "enum has only A,B");   // debug: catch the bug
std::unreachable();                     // release: tell optimizer -> no dead code/return
```

- `assert(false)` — debug mein pakad.
- `std::unreachable()` — release mein optimizer ko batao "yeh line dead hai" → woh
  bounds check / default return / phi-node hata deta. **Agar galti se reach ho
  gaya → UB** (kuch bhi). Isliye pehle `assert`.
- `[[assume(x)]]` — "`x` true hai, ispe optimize karo" — checked nahi. `[[assume(n
  > 0)]]` se compiler `n == 0` wali branch hata sakta. Galat assume = UB.

---

## Custom `CHECK` — release mein bhi rehne wala

Kabhi aap chahte ho ek invariant release mein bhi verify ho (cost acceptable),
crash-with-context ho, telemetry jaaye:

```cpp
#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) [[unlikely]] {                                          \
            std::fprintf(stderr, "CHECK failed: %s\n  at %s:%d\n",           \
                         #cond, __FILE__, __LINE__);                         \
            std::abort();                                                    \
        }                                                                    \
    } while (0)

CHECK(book_.bid() < book_.ask());   // crossed book -> die with context, always
```

Abseil ka `CHECK`, folly ka `CHECK`, GTest ka `ASSERT_*` — sab yeh pattern.
`assert` se farq: `NDEBUG` ise nahi hata; cost hamesha hai (ek predicted-not-taken
branch — usually negligible).

---

## Contracts (C++26 — aa raha hai)

```cpp
// C++26 syntax (proposed / in-progress)
Level& level_at(std::size_t i)
    pre(i < levels_.size())                 // precondition
    post(r: &r == &levels_[i])              // postcondition (r = return value)
{
    return levels_[i];
}
```

- `pre` / `post` / `contract_assert` — assertions ko function **signature** ka
  part banate hain (documentation + tooling).
- Build modes: *ignore* / *observe* (log, continue) / *enforce* (terminate).
- Abhi tak experimental. GSL ka `Expects()` / `Ensures()` aaj iska library-level
  substitute hai.

---

## Andar kya hota hai

- `assert` ek macro: roughly `((cond) ? (void)0 : __assert_fail(#cond, __FILE__,
  __LINE__, __func__))`. `NDEBUG` defined → `#define assert(x) ((void)0)` — argument
  discarded, no code.
- `static_assert` — pure compile-time; koi symbol, koi code. Sirf diagnostic.
- `std::unreachable()` → `__builtin_unreachable()` — optimizer us path ki
  assumptions propagate karta hai (dead code elim, range narrowing).
- `[[assume(e)]]` → GCC `__attribute__((assume(e)))` / `if (!(e))
  __builtin_unreachable();` — `e` ke side effects **evaluate nahi** hote (unlike
  `assert` in debug).
- `-D_GLIBCXX_ASSERTIONS` — libstdc++ apne hot paths mein `__glibcxx_assert(cond)`
  daalta hai; yeh flag unhe live karta hai (chhota branch cost).

---

## > **HFT relevance**
> - **`static_assert` liberally** — `sizeof`/`alignof`/`is_trivially_copyable`/
>   power-of-two capacities/`offsetof` layout checks. Yeh muft hai aur ek poori
>   class of "kisi ne struct mein field add kiya aur wire format toot gaya" bugs
>   compile pe pakadta hai.
> - **`assert` for hot-path invariants** — `assert(idx < cap_)`,
>   `assert(seq == expected_seq)`, `assert(px > 0)`. Debug/CI builds mein bug
>   turant, release mein **zero cost**.
> - **`std::unreachable()` after exhaustive switches** — optimizer ko default
>   branch / bounds check hatane deta hai. Measured wins in dispatch loops.
>   Par pehle `assert(false)` — warna ek din woh "unreachable" branch UB dega.
> - **`CHECK`-style for a few sacred invariants** — crossed book, negative
>   inventory, position limit breach. Yeh galat trades se bachata hai; ek
>   always-on predicted branch ki cost acceptable hai, aur fail hone pe
>   `abort` + failover >> "chalte raho".
> - **Sanitizers in CI** — `-fsanitize=address,undefined` pe poora test suite.
>   Production build pe nahi (cost), par har UB CI mein pakda jaaye.

---

## Hands-on

```bash
# assert on:
g++ -std=c++20 -g -O0 file.cpp -o t && ./t          # assert live
# assert off:
g++ -std=c++20 -DNDEBUG -O2 file.cpp -o t && ./t    # assert gayab
```

Ek function `int mid(int lo, int hi) { assert(lo <= hi); return lo + (hi-lo)/2; }`.
Debug mein `mid(5, 2)` call karo — abort + message. `-DNDEBUG` se rebuild — ab woh
chup-chaap galat answer deta hai (bug chhup gaya — isliye assert *bhi* chahiye
*aur* input validation alag).

`static_assert(sizeof(void*) == 8, "64-bit only");` file ke top pe daalo, 32-bit
target pe compile try karo.

---

## ⚠️ Traps

### Trap 1 — `assert` mein side effect
```cpp
assert(queue.pop() == expected);   // ⚠️ -DNDEBUG: pop() kabhi nahi chalega
auto v = queue.pop(); assert(v == expected);   // ✅
```

### Trap 2 — `assert` se user input validate karna
```cpp
assert(argc == 3);                 // ⚠️ release: no check -> argv[2] OOB read
if (argc != 3) { print_usage(); return 2; }   // ✅
```

### Trap 3 — `assert(a = b)` (single `=`)
```cpp
assert(status = OK);   // ⚠️ assignment, hamesha truthy. `==` chahiye tha
```
Compiler `-Wparentheses` warn karta hai — dhyan do.

### Trap 4 — `std::unreachable()` bina `assert`
```cpp
switch (x) { case 1: ...; case 2: ...; }
std::unreachable();    // ⚠️ x==3 kabhi aaya to UB, silent. Pehle assert(false)
```

### Trap 5 — `[[assume]]` with side-effecting expression
```cpp
[[assume(next() > 0)]];   // ⚠️ next() call NAHI hota; aur agar assumption galat -> UB
```

### Trap 6 — comma in `assert` / `static_assert` (macro sees 2 args)
```cpp
static_assert(std::is_same_v<A, B>, "x");   // ok
assert((std::pair<int,int>{1,2}.first == 1));   // extra parens if comma inside
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`assert` = error handling" | `assert` = bug detection; release mein gayab. Errors alag |
| "`assert` ka expression release mein bhi chalta" | `NDEBUG` pe fully removed — side effects mat daalo |
| "`static_assert` runtime pe check hota" | Pure compile-time; false = compile error |
| "`std::unreachable()` safe fallback hai" | Reach ho gaya to UB. `assert(false)` + phir yeh |
| "`[[assume(x)]]` == `assert(x)`" | `assume`: unchecked, no eval, false→UB. `assert`: checked in debug |
| "release build mein koi assertion nahi ho sakti" | `CHECK`-style macro, `_GLIBCXX_ASSERTIONS`, contracts (enforce) |

---

## Exercises

1. **Assert or handle:** in ke liye batao — assert, input-validation, ya recoverable
   error: (a) `parse_port(s)` ko `s` non-empty milna chahiye, (b) internal
   `bucket_index < num_buckets`, (c) config se aaya `thread_count` 1..256 mein,
   (d) `memcpy` src/dst overlap nahi karte.

   <details><summary>Answer</summary>

   (a) input validation — `s` bahar se. (b) `assert` — internal invariant.
   (c) input validation — config bhi "duniya" hai; range check + error message.
   (d) `assert(!overlap(src,dst,n))` — caller (programmer) ka contract.
   </details>

2. **Side-effect bug:** `assert(logfile.write(line));` — debug mein theek chalta,
   production mein logs missing. Kyun, fix?

   <details><summary>Answer</summary>

   `-DNDEBUG` → `assert(...)` removed → `logfile.write(line)` **call hi nahi
   hota**. Fix: `bool ok = logfile.write(line); assert(ok);` (or handle the
   `false` as a real error — a failed write usually is).
   </details>

3. **`static_assert` design:** ek `struct WireMsg` hai jo exactly network layout
   match karna chahiye. Kaunse 3 `static_assert` daaloge?

   <details><summary>Answer</summary>

   `static_assert(std::is_standard_layout_v<WireMsg>);`
   `static_assert(sizeof(WireMsg) == 48);` (exact wire size, no padding surprises)
   `static_assert(offsetof(WireMsg, price) == 16);` (each critical field's offset).
   Optionally `static_assert(std::endian::native == std::endian::little);` if you
   rely on it.
   </details>

4. **`unreachable` win:** ek dispatch `switch` over `enum class Op { add, sub,
   mul, div }` (all 4 handled, each `return`s). `std::unreachable()` add karne se
   codegen mein kya farak?

   <details><summary>Answer</summary>

   Bina it, compiler ek implicit "fell off the switch" path rakhta hai (undefined
   return / a jump). `std::unreachable()` se woh path delete → no spurious return,
   jump table tighter, aur caller-side the return value is known-defined on all
   real paths. Precede with `assert(false && "bad Op")`.
   </details>

5. **`CHECK` cost:** `CHECK(bid < ask)` har order pe, 1M orders/sec. Predicted-
   not-taken branch. Approx cost, aur kya yeh worth hai?

   <details><summary>Answer</summary>

   ~1 correctly-predicted branch ≈ sub-nanosecond, effectively free in the noise.
   Worth it: a crossed book means downstream wrong prices / wrong fills — `abort`
   + failover is far cheaper than trading on corrupt state. This is exactly where
   an always-on check earns its keep.
   </details>

---

## Interview questions

1. `assert` vs error handling — line kahan kheenchte ho?
2. `assert` ka argument side-effect-free kyun hona chahiye?
3. `static_assert` — kab fire hota, message optional kab se?
4. `std::unreachable()` — kya karta, kis ke saath pair karna chahiye aur kyun?
5. `[[assume(expr)]]` vs `assert(expr)` — teen farq.
6. `-DNDEBUG` exactly kya karta hai `assert` ke saath?
7. Release build mein invariant check chahiye — kaunse options (`CHECK`,
   `_GLIBCXX_ASSERTIONS`, contracts)?
8. C++26 contracts ka basic idea — `pre`/`post`, build modes.

---

## Next
→ [`13-undefined-behaviour.md`](13-undefined-behaviour.md)

# 03 — Exceptions: `throw` / `try` / `catch`

## Prerequisites
- `01-error-handling-strategies.md`, `02-return-codes-and-errno.md`
- Classes, inheritance (`16-OOP`), `std::string`
- [`examples/01_exceptions_basics.cpp`](examples/01_exceptions_basics.cpp)

## Yeh topic abhi kyun
Exception ek **alag control-flow channel** hai: error ko normal return se nahi,
ek parallel raste se upar bhejta hai — beech ke functions ko chhue bina — jab tak
matching `catch` na mil jaaye. Constructor fail kar sakta hai (return value nahi
hota), happy path saaf rehta hai. Mechanics simple hain, par kuch rules pakke hain:
catch by reference, catch order, aur "kya throw karein".

---

## `throw` — exception uthana

```cpp
throw std::runtime_error("socket band ho gaya");
```

`throw X` do cheezein karta hai:
1. `X` ki ek copy ek special jagah banata hai (implementation: heap se
   `__cxa_allocate_exception`).
2. Stack unwinding shuru — current function se bahar, har frame ke locals ke
   destructors chalte hue, upar ki taraf, jab tak koi `catch` match na kare.

Aap **kuch bhi** throw kar sakte ho (`throw 42;`, `throw "oops";`) — par **mat
karo**. Hamesha `std::exception` se derive ki hui type (ya woh khud) throw karo:
`what()` milta hai, generic handlers pakad lete hain.

---

## `try` / `catch`

```cpp
try {
    Config c = load_config(path);        // yeh throw kar sakta
    run(c);
} catch (const std::runtime_error& e) {  // specific
    std::fprintf(stderr, "runtime error: %s\n", e.what());
} catch (const std::exception& e) {      // general fallback
    std::fprintf(stderr, "error: %s\n", e.what());
} catch (...) {                          // sab kuch (type ka access nahi)
    std::fprintf(stderr, "unknown exception\n");
}
```

- `try` block ke andar (ya usme se call hui kisi cheez mein) throw hua to control
  seedha matching `catch` pe.
- Ek `catch` chala → baaki skip → `try/catch` ke baad wala code chalta hai.
- Koi `catch` match nahi kiya → exception aage ki taraf propagate (agla enclosing
  `try`, ya `main` se bahar → `std::terminate`).

### Catch **by `const` reference** — hamesha

```cpp
catch (const std::exception& e)     // ✅ no copy, no slicing, polymorphic e.what()
catch (std::exception e)            // ❌ SLICING: derived part kat gaya; extra copy
catch (std::exception* e)           // ❌ galat — pointer throw nahi kiya tha
```

By value catch karoge to:
- **Slicing** — `throw NetworkError(...)` kiya, `catch (std::exception e)` sirf base
  part copy karta, `what()` base ka.
- Ek **extra copy**, jo khud throw kar sakti (kam alag).

### Catch **order** — derived pehle, base baad mein

`catch` handlers **upar-se-neeche** try hote hain, first match jeetta — return-type
overload resolution *nahi*, bas order. Isliye:

```cpp
try { ... }
catch (const FileNotFound& e)   { ... }   // most derived
catch (const std::ios_base::failure& e) { ... }
catch (const std::exception& e) { ... }   // most base — LAST
```

Agar `std::exception` upar likh diya, to `FileNotFound` handler **kabhi nahi**
chalega (base pehle match kar lega). GCC/Clang `-Wexceptions` se warn karte hain.

---

## Rethrow — `throw;`

Current exception ko **wahi object** aage bhejo (nayi copy nahi):

```cpp
try {
    do_work();
} catch (const std::exception& e) {
    metrics.error_count++;               // log / count
    throw;                               // <-- same exception, upar bhejo
}
```

- `throw;` (bare) — sirf `catch` block ke andar valid. Original exception,
  original type.
- `throw e;` — **naya** exception, aur agar `e` `const std::exception&` hai to
  **slicing** (base copy). Rethrow ke liye hamesha bare `throw;`.

---

## Standard exception hierarchy

```
std::exception
├── std::logic_error           // "bug" — program ki galti, pehle roka ja sakta tha
│   ├── std::invalid_argument
│   ├── std::domain_error
│   ├── std::length_error
│   └── std::out_of_range       // vector::at, string::at, map::at
├── std::runtime_error          // "runtime condition" — pehle se pata nahi tha
│   ├── std::range_error
│   ├── std::overflow_error
│   ├── std::underflow_error
│   └── std::system_error       // carries a std::error_code (file 11)
├── std::bad_alloc              // new fail
├── std::bad_cast               // dynamic_cast<ref&> fail
├── std::bad_optional_access    // optional::value() jab khali
└── std::bad_variant_access
```

- **`logic_error`** — precondition violate hua, yeh caller ka bug hai. Argument:
  isse `assert`/contract se pehle hi pakadna behtar tha.
- **`runtime_error`** — bahari duniya ne fail kiya (file gayab, network down).
- `what()` — `const char*`, human-readable. `runtime_error`/`logic_error` ko
  string constructor mein message dete ho, woh store ho jaata hai.

---

## `std::exception_ptr` — exception ko pakad ke rakho

Exception ko ek variable mein store karo, baad mein / doosre thread pe rethrow:

```cpp
std::exception_ptr slot;
try { risky(); }
catch (...) { slot = std::current_exception(); }   // type-erased pakad

// ... kahin aur, baad mein ...
if (slot) std::rethrow_exception(slot);
```

`std::promise`/`std::future`, thread pools, coroutines — sab isse use karke worker
ki exception main thread tak pahunchate hain.

---

## Andar kya hota hai

- **`try` block enter/exit** — koi runtime instruction nahi (table-based EH). Bas
  compiler `.gcc_except_table` mein likhta hai: "PC is range mein hai to yeh
  handler / yeh cleanup." Isliye **happy path zero-cost** (file `08`).
- **`throw`** — `__cxa_allocate_exception(size)` (heap), exception object ka
  copy/move ctor us memory mein, phir `__cxa_throw`. Unwinder (`libgcc`/`libunwind`)
  har frame ke lookup tables padhta hai, cleanup routines (dtors) invoke karta,
  handler match hone pe stack pointer adjust karke `catch` pe jump.
- **`catch` match** — RTTI (`std::type_info`) se: thrown type is handler ke type ka
  same ya derived hai? Isliye `-fno-rtti` exceptions ke saath thoda tension mein
  (GCC handle kar leta, par HFT aksar dono off karta).
- **`std::terminate`** — default `std::abort()` → `SIGABRT`. `std::set_terminate`
  se hook kar sakte ho (last-ditch log).

---

## > **HFT relevance**
> Yeh **mechanism** samajhna zaroori hai kyunki aap ise **hataoge** (file `09`) —
> aur jaanna chahiye kya de rahe ho. Exceptions ka asli fayda: **constructor
> failure** (RAII types) aur **startup/config** code jahan "throw with a message
> and die" hi chahiye. Trading engine ka `main()` / bootstrap layer exceptions
> use kar sakta hai; jaise hi hot event loop shuru hua, `noexcept` aur error
> values.
>
> `std::system_error` (ek `error_code` carry karta hai) woh bridge hai jab koi
> library throw karti hai par aapko structured error chahiye: `catch (const
> std::system_error& e) { return e.code(); }`.

---

## Hands-on

```bash
./build.ps1 23-ERROR-HANDLING/examples/01_exceptions_basics.cpp
```

Example padho: catch order (B), unwinding (C), rethrow (D), `catch(...)` (E),
`exception_ptr` (F). Phir `catch` handlers ka order ulta karke (base upar) compile
karo — warning dekho, aur run karke dekho kaunsa handler chala.

---

## ⚠️ Traps

### Trap 1 — catch by value → slicing
```cpp
try { throw FileNotFound("x.cfg"); }
catch (std::exception e) { puts(e.what()); }   // ⚠️ "std::exception" — derived what() gaya
```
`catch (const std::exception& e)`.

### Trap 2 — base handler pehle
```cpp
catch (const std::exception& e) { ... }
catch (const std::out_of_range& e) { ... }     // ⚠️ DEAD — kabhi nahi chalega
```

### Trap 3 — `throw e;` rethrow ke liye
```cpp
catch (const std::exception& e) { log(e); throw e; }   // ⚠️ slice + naya object
catch (const std::exception& e) { log(e); throw;   }   // ✅
```

### Trap 4 — non-`std::exception` throw
```cpp
throw "config missing";        // ⚠️ const char* — catch(const std::exception&) miss karega
throw std::runtime_error("config missing");   // ✅
```

### Trap 5 — exception ko flow control ki tarah
```cpp
try { v = m.at(k); } catch (...) { v = 0; }    // ⚠️ "not found" exceptional nahi
if (auto it = m.find(k); it != m.end()) v = it->second; else v = 0;   // ✅
```

### Trap 6 — destructor se throw
```cpp
~Session() { flush(); }        // ⚠️ agar flush() throw kare aur hum unwinding mein hain -> terminate
```
Destructors implicitly `noexcept` (file `06`). Dtor mein `try/catch` + swallow, ya
error ko pehle explicit `close()` se handle karo.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "catch by value theek hai" | `const&` — warna slicing + extra copy |
| "`catch` best-match dhoondta hai" | Nahi — top-to-bottom first match. Order matters |
| "`throw e;` = rethrow" | `throw;` = rethrow. `throw e;` = naya (aksar sliced) object |
| "`throw 42;` bhi to legal hai" | Legal, par `std::exception`-derived hi throw karo |
| "exception = crash" | Uncaught exception = crash. Caught = normal recovery |
| "`try/catch` ki cost hoti hai har baar" | Happy path pe ~0 (table-based). `throw` ki cost hai |

---

## Exercises

1. **Output predict:**
   ```cpp
   struct G { ~G() { std::puts("~G"); } };
   try { G g; throw std::runtime_error("x"); std::puts("after throw"); }
   catch (const std::exception& e) { std::printf("caught %s\n", e.what()); }
   ```

   <details><summary>Answer</summary>

   ```
   ~G
   caught x
   ```
   `"after throw"` kabhi nahi — `throw` ke baad ka code skip. `g` ka dtor unwinding
   mein chala (catch se pehle).
   </details>

2. **Which handler:**
   ```cpp
   try { throw std::out_of_range("i"); }
   catch (const std::logic_error& e) { std::puts("logic"); }
   catch (const std::out_of_range& e) { std::puts("oor"); }
   ```

   <details><summary>Answer</summary>

   `logic` — `std::out_of_range` `std::logic_error` se derive hai, aur `logic_error`
   handler pehle likha hai → first match. `oor` handler dead hai (compiler warn
   kar sakta).
   </details>

3. **Slicing spot:** `catch (std::runtime_error e) { throw e; }` mein do bugs.

   <details><summary>Answer</summary>

   (1) `catch` by value — agar `std::system_error` (runtime_error se derived) thrown
   tha to slice ho gaya. (2) `throw e;` — rethrow nahi, `e` (jo already sliced ho
   sakta) ka naya copy. `catch (const std::runtime_error& e) { ...; throw; }`.
   </details>

4. **`exception_ptr`:** worker thread mein exception aata hai, main thread ko
   chahiye. Sketch karo (`std::current_exception`, `std::rethrow_exception`).

   <details><summary>Answer</summary>

   Worker: `try { work(); } catch (...) { p = std::current_exception(); }` jahan
   `p` shared/promise mein. Main: `if (p) std::rethrow_exception(p);` ek `try`
   ke andar → ab main thread mein normal `catch`. (`std::async`/`future::get`
   yahi karta hai internally.)
   </details>

5. **Design:** `Socket` ka constructor `connect()` karta hai jo fail kar sakta.
   Constructor error kaise report kare — `throw` ya kuch aur? Trade-off.

   <details><summary>Answer</summary>

   Constructor ke paas return value nahi → ya `throw`, ya "two-phase init"
   (`Socket s; if (auto e = s.open(addr))...`), ya factory `static
   expected<Socket, Err> make(addr)`. `throw` sabse clean par `-fno-exceptions`
   mein nahi; HFT codebases factory + `expected` ya two-phase choose karte hain.
   </details>

---

## Interview questions

1. `throw X` do kaam karta hai — kaunse?
2. Catch by value ke do problems.
3. `catch` handlers kis order mein try hote hain? Design implication?
4. `throw;` vs `throw e;` — farq.
5. Standard exception hierarchy: `logic_error` vs `runtime_error` — semantic farq.
6. `std::exception_ptr` / `std::current_exception` kis ke liye?
7. Uncaught exception ka kya hota hai? `std::terminate` ko customize kaise?
8. Kyun `std::exception`-derived hi throw karna chahiye, `int`/`const char*` nahi?

---

## Next
→ [`04-stack-unwinding.md`](04-stack-unwinding.md)

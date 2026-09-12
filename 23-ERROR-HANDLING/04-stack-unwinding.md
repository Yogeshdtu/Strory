# 04 — Stack unwinding

## Prerequisites
- `03-exceptions-basics.md`
- `17-RAII` (RAII, destructors), `08-FUNCTIONS` (call stack, stack frames)
- [`examples/03_raii_unwinding.cpp`](examples/03_raii_unwinding.cpp)

## Yeh topic abhi kyun
Exception ka asli jaadu unwinding hai: `throw` se `catch` tak, C++ **har frame ke
har fully-constructed local object ka destructor** deterministic order mein chalata
hai. Isi wajah se RAII + exceptions saath mein resource-safe hain. Agar aap yeh
mechanism nahi samajhte, to "exception-safe code" likhna guesswork ban jaata hai.

---

## Unwinding kya hai — step by step

```cpp
void c() {
    Lock lk(mtx);                 // (3) constructed
    Buffer b(4096);               // (4) constructed
    parse(b);                     // (5) parse() THROWS
    // (never reached)
}
void b_() { File f("in.dat");     // (2) constructed
            c(); }
void a() {
    try {
        Timer t;                  // (1) constructed
        b_();
    } catch (const ParseError& e) {   // (7) handler
        log(e);
    }
}
```

`parse()` throw karta hai. Unwinder:

1. `c()` ke frame mein: `b` ka `~Buffer()`, phir `lk` ka `~Lock()` — **construction
   ka ulta order**. `c()` ka frame pop.
2. `b_()` ke frame mein: `f` ka `~File()`. Frame pop.
3. `a()` ke frame mein: yahan `try` block hai jiska `catch (const ParseError&)`
   match karta hai. `t` ka `~Timer()` chalta hai (`try` block se bahar nikalte hue),
   phir control `catch` body pe.

Net: `~Buffer`, `~Lock`, `~File`, `~Timer` — sab chale, is exact order mein. Koi
leak nahi. `b_()` aur `c()` ke andar koi `try` nahi tha — unhe "pata bhi nahi
chala" ki error tha, bas unke locals clean ho gaye.

---

## Do rules jo sab decide karte hain

### Rule 1 — sirf **fully-constructed** objects ka dtor chalta hai

Agar constructor ke beech throw hua, to:
- Jo **members / base classes ban chuke** the — unke dtors chalte hain.
- Jo abhi nahi bane — nahi.
- **Object khud ka dtor NAHI chalta** (woh "bana hi nahi").

```cpp
struct Widget {
    Resource a;     // constructed
    Resource b;     // ctor THROWS here
    Resource c;     // never constructed
    Widget() : a("a"), b(fail()), c("c") {}
};
// throw pe: ~Resource("a") chalega. c banaya nahi. ~Widget() nahi chalega.
```

Isi liye **RAII members** itne powerful hain: partially-constructed object bhi
apne jo-bhi-bane members clean kar deta hai, bina aapke kuch likhe.

### Rule 2 — `new[]` / container: jitne bane, utne destruct

```cpp
new Foo[10];    // agar Foo #7 ka ctor throw kare -> Foo[0..6] ke dtors chalte hain,
                // allocated memory free hoti hai, phir exception aage
```
`std::vector`, `std::uninitialized_copy` etc. yahi guarantee dete hain (file `05`).

---

## `noexcept` aur unwinding — `std::terminate` ki lakeer

Agar unwinding ke dauraan control kisi `noexcept` function ki boundary paar karke
"exception nikalne" ki koshish kare → seedha **`std::terminate`**. Koi doosra
catch try nahi hota.

```cpp
void cleanup() noexcept {
    flush();            // agar flush() throw kare -> std::terminate (abhi, yahin)
}
```

Sabse common shakl: **destructor se throw** (dtors default `noexcept`):

```cpp
struct Session {
    ~Session() {                 // implicitly noexcept
        conn.close();            // agar close() throw kare...
    }                            // ...aur hum already unwinding mein hain -> terminate
};
```

Kyun yeh rule? Do exceptions ek saath propagate nahi ho sakte — "unwinding ke
dauraan naya exception" ambiguous hai, isliye standard ne kaha: terminate.

**`std::uncaught_exceptions()`** (C++17, plural) batata hai kitne exceptions abhi
"in flight" hain — dtor mein `if (std::uncaught_exceptions() > 0)` se aap decide
kar sakte ho "abhi throw karna mana hai".

---

## `-fno-exceptions` mein unwinding

`throw` compile-time error ban jaata hai. Toh unwinding "exception se" hota hi
nahi. Par **normal return / scope exit pe destructors waise hi chalte hain** —
RAII bilkul intact. Sirf "error ko upar phenkne" ka rasta gaya; cleanup mechanism
zinda hai.

Binary se `.eh_frame` unwinding tables bhi (mostly) hat jaati hain — chhota
binary, zyada inlining (file `09`).

---

## Andar kya hota hai

- Compiler har function ke liye **LSDA** (Language-Specific Data Area,
  `.gcc_except_table`) aur **CFI** (`.eh_frame`) emit karta hai: "is PC range mein
  yeh locals live hain → yeh cleanup code; yeh `try` region → yeh handler."
- `throw` → `_Unwind_RaiseException` (libgcc/libunwind). Do phases:
  1. **Search phase** — stack ko upar scan, har frame ka personality routine
     (`__gxx_personality_v0`) poochhta "tera handler hai?" Handler mila → phase 2.
  2. **Cleanup phase** — dobara upar chalo, har frame ke cleanup (dtors) invoke
     karte hue, handler frame pe ruko, SP/registers restore, `catch` pe jump.
- Handler nahi mila poore search phase mein → `std::terminate` (stack abhi
  unwound bhi nahi — isliye core dump mein original stack dikhta, useful).
- Yeh sab **table-driven**: normal execution mein zero instructions. Cost sirf
  tab jab `throw` actually chale.

---

## > **HFT relevance**
> Unwinding **correct** hai par **slow aur non-deterministic**: table lookups,
> personality routine calls, dtor chains — sab cold code + pehli baar page/I-cache
> miss. Ek `throw` jo hot path mein 6 µs le le = P99 latency spike. Isliye hot
> path exception-free.
>
> RAII ka fayda `-fno-exceptions` mein bhi poora milta hai — `LockGuard`,
> `FdHolder`, `PoolAllocation` scope-exit pe release hote hain, normal aur early
> `return` dono pe. Bas "throw se upar phenkna" nahi milta; uski jagah error
> values.
>
> **Destructors ko `noexcept` rakho aur unmein kabhi throw mat karo** — yeh HFT
> mein aur bhi kadai se, kyunki ek terminate = process gaya = hot standby
> failover, jo microseconds nahi, milliseconds ka event hai.

---

## Hands-on

```bash
./build.ps1 23-ERROR-HANDLING/examples/03_raii_unwinding.cpp
```

Example ke sections: manual cleanup leak vs RAII (nested scopes, reverse release
order), ctor ke beech throw (`HalfBuilt`), aur `DangerousDtor` (dtor-throws demo,
default OFF). `DangerousDtor` wale commented block ko uncomment karke run karo —
`std::terminate` dekho, aur stack trace mein original throw site.

---

## ⚠️ Traps

### Trap 1 — dtor se throw jab unwinding chal rahi ho → terminate
```cpp
~Writer() { file.flush(); }    // ⚠️ flush() throw + unwinding => std::terminate
```
Dtor mein: swallow (`try { flush(); } catch (...) {}`) ya pehle explicit
`writer.close()` jo error return kare.

### Trap 2 — ctor ke beech throw pe "main object ka dtor chalega" maan lena
```cpp
struct T { Buf big; T() : big(HUGE) { throw std::runtime_error("x"); } };
// ~T() NAHI chalega. big ka ~Buf() chalega. Agar ctor ne raw `new` kiya tha
// aur member mein store nahi kiya -> LEAK.
```
Ctor mein acquire ki har cheez ek RAII member ho.

### Trap 3 — unwinding ke beech naya kaam jo throw kar sakta
```cpp
catch (...) { retry_everything(); throw; }   // ⚠️ retry_everything() throw kare to?
```
`catch` block ke andar (jab tak `throw;` nahi hua) aap "unwinding mein" nahi ho —
yeh theek hai. Par dtor mein aisa nahi.

### Trap 4 — `longjmp` se RAII scope ke bahar kudna
```cpp
setjmp(buf); ... longjmp(buf, 1);    // ⚠️ beech ke C++ objects ke dtors NAHI chalenge
```
C++ mein `longjmp` sirf tab safe jab beech mein koi non-trivial dtor na ho. UB
warna. Exceptions use karo.

### Trap 5 — exception jo `noexcept` function se guzarna chahe
```cpp
void f() noexcept { g(); }      // g() throw kare -> terminate at f's boundary
```
`noexcept` ka matlab hai "yeh throw nahi karega" — nibhao.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "unwinding sirf `catch` wale function mein dtors chalata" | Har intermediate frame ke locals bhi — poore raste |
| "ctor throw kare to object ka dtor chalega" | Nahi — sirf constructed members/bases ke dtors |
| "dtor se throw karna bas bura style hai" | Unwinding ke beech ho to `std::terminate` — hard rule |
| "`-fno-exceptions` = RAII bekaar" | RAII scope-exit pe waise hi chalta; sirf `throw` gaya |
| "unwinding ki cost try block enter karne pe" | Nahi — table-based, cost sirf actual `throw` pe |

---

## Exercises

1. **Order predict:**
   ```cpp
   struct N { std::string s; ~N(){ std::printf("~%s\n", s.c_str()); } };
   void f() { N a{"a"}; { N b{"b"}; N c{"c"}; throw 1; } }
   int main() { try { N z{"z"}; f(); } catch (int) { std::puts("caught"); } }
   ```

   <details><summary>Answer</summary>

   ```
   ~c
   ~b
   ~a
   ~z
   caught
   ```
   Inner scope (`c`, `b`), phir `f`'s `a`, phir `main`'s `z` (try block se
   bahar), phir handler.
   </details>

2. **Ctor throw:** `struct P { A a; B b; C c; P(); };` jisme `b`'s ctor throws.
   Kaunse dtors chalte hain?

   <details><summary>Answer</summary>

   Sirf `~A()` (a ban chuka tha). `c` banaya hi nahi. `~P()` nahi chala. Agar `P`
   ke ctor body mein koi raw resource acquire hua tha b se pehle jo member nahi
   hai — woh leak.
   </details>

3. **Find the terminate:**
   ```cpp
   struct Guard { ~Guard() { if (rollback_needed) do_rollback(); } };
   // do_rollback() database ko hit karta hai, throw kar sakta hai
   ```
   Kab crash?

   <details><summary>Answer</summary>

   Jab `Guard` ek exception ke unwinding ke dauraan destroy ho raha ho aur
   `do_rollback()` throw kare — dtor `noexcept`, unwinding chal rahi → `terminate`.
   Fix: `~Guard() noexcept { try { if (rollback_needed) do_rollback(); } catch
   (...) { log_and_swallow(); } }`.
   </details>

4. **`uncaught_exceptions`:** ek `Transaction` type jo scope-exit pe commit karta
   hai *agar* koi exception nahi tha, warna rollback. Sketch.

   <details><summary>Answer</summary>

   Ctor mein `int entry_ = std::uncaught_exceptions();`. Dtor mein `if
   (std::uncaught_exceptions() == entry_) commit(); else rollback();` — dono ko
   `noexcept`/swallow rakho. (Yeh Andrei Alexandrescu ka `ScopeGuard`/`scope_fail`
   pattern hai.)
   </details>

5. **`longjmp` danger:** ek C library `setjmp`/`longjmp` se errors handle karti
   hai. Aapke C++ callback mein `std::string`, `std::lock_guard` hain. Kya risk?

   <details><summary>Answer</summary>

   `longjmp` stack ko rewind karta hai bina C++ dtors chalaye → `std::string`
   leak, `lock_guard` ka mutex hamesha locked, koi bhi RAII cleanup skip → UB /
   deadlock. Callback ko dtor-free rakho, ya library ke error hook ko C++
   exception mein wrap karo.
   </details>

---

## Interview questions

1. Stack unwinding step-by-step — `throw` se `catch` tak kya hota hai?
2. Ctor ke beech throw: kaunse dtors chalte, kaunse nahi?
3. Dtor se throw + unwinding = ? Kyun yeh rule?
4. `std::uncaught_exceptions()` (plural) kis ke liye? Singular se farq?
5. Table-based ("zero-cost") EH: try block enter karne ki runtime cost?
6. `-fno-exceptions` mein RAII ka kya hota hai?
7. Search phase vs cleanup phase — do-phase unwinding kyun?

---

## Next
→ [`05-exception-safety.md`](05-exception-safety.md)

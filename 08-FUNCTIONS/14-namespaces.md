# 14 — Namespaces: apne naam ke "surname" banana

## Prerequisites
- `02-CPP-FIRST-STEPS/02-anatomy-line-by-line.md` — "Namespace kya hai?", `::`, aur `using namespace std;` se kyun bachna
- `04-INPUT-OUTPUT/05-getline-and-strings.md` — `std::getline`, `std::string`
- [`06-scope-and-lifetime.md`](06-scope-and-lifetime.md) — scope, shadowing
- [`08-function-overloading.md`](08-function-overloading.md) — ek naam, kai functions; compiler candidate kaise dhoondhta hai

## Yeh topic abhi kyun
Folder 02 mein aapne namespace **use** kiya: `std::cout`, `std::string`. "Surname" wali analogy
bhi dekhi. Par ab tak aapne apna koi namespace **banaya** nahi.

Ab aapke programs mein kai functions hain. Jaise-jaise program bada hota hai, naam takrane
lagte hain — do log `lot_size()` naam ka function bana dete hain, ya aapka `count` variable
`std::count` se takra jaata hai. Namespaces isi ka ilaaj hain.

Aur ek aur cheez jo pichhle lesson (overloading) mein adhoori reh gayi thi: compiler function
ko **kahan-kahan** dhoondhta hai. File 08 ne "ADL" ka naam liya tha — yahan woh poori tarah
samjhenge.

---

## Apna namespace banana

Maan lo aapka program do exchanges ke saath kaam karta hai. Dono ka lot size alag hai, aur
dono jagah function ka sabse natural naam `lot_size` hi hai:

```cpp
namespace nse {
    int lot_size() { return 50; }
}

namespace bse {
    int lot_size() { return 25; }
}

int main() {
    nse::lot_size();   // 50
    bse::lot_size();   // 25
}
```

`namespace nse { ... }` ek **naam wala dabba** hai. Uske andar jo bhi likha, uska poora naam
ban jaata hai `nse::...`. Isliye `nse::lot_size` aur `bse::lot_size` compiler ke liye do
**bilkul alag** functions hain — takkar nahi.

Analogy wahi surname wali: school mein do "Rahul" the — "Rahul Sharma" aur "Rahul Verma"
se confusion khatam. `nse::` aur `bse::` yahi surnames hain.

### Andar kya hota hai — namespace machine code mein kahan jaata hai?
Namespace **sirf compile-time** ki cheez hai. Runtime pe koi "namespace object" nahi banta,
koi extra memory ya time nahi lagta. Namespace function ke **symbol naam** (mangled name)
ka hissa ban jaata hai. Example ko compile karke `nm` se dekha:

```
0000000000000000 T _ZN3nse8lot_sizeEv      ->  nse::lot_size()
000000000000000b T _ZN3bse8lot_sizeEv      ->  bse::lot_size()
```

`_ZN3nse8lot_sizeEv` ko todo: `3nse` = 3 letters ka naam "nse", `8lot_size` = 8 letters ka
"lot_size". Linker ke liye yeh do alag naam hain — isliye clash nahi. (Name mangling ki
poori kahani: folder 24 file 07.)

---

## Ek namespace kai jagah khul sakta hai

```cpp
namespace nse {
    int lot_size() { return 50; }
}

// ... bahut saara doosra code ...

namespace nse {                 // wahi namespace DOBARA khola
    int tick_paise() { return 5; }
}

nse::tick_paise();              // 5 -- dono hisse ek hi `nse` ke hain
```

Namespace ko band karke baad mein phir khol sakte ho — naye members jud jaate hain. Isi wajah
se `std` namespace sainkdo header files mein faila hua hai: `<vector>` bhi `namespace std`
kholta hai, `<string>` bhi.

---

## Nested namespaces

```cpp
// Purana tareeka (C++17 se pehle):
namespace exch { namespace nse { namespace fo {
    int expiry_day() { return 4; }
} } }

// C++17 ka chhota tareeka -- bilkul same matlab:
namespace exch::nse::fo {
    int expiry_day() { return 4; }
}

exch::nse::fo::expiry_day();    // 4
```

Symbol mein poori chain aati hai: `_ZN4exch3nse2fo10expiry_dayEv`.

---

## Namespace alias — lamba naam chhota karo

```cpp
namespace fo = exch::nse::fo;   // "fo" ab exch::nse::fo ka doosra naam hai
fo::expiry_day();               // 4
```

Alias koi copy nahi banata — sirf ek chhota naam. Function ke andar likhoge to sirf us
function mein chalega.

---

## `::` akele — global namespace

Jo cheez kisi namespace ke andar nahi, woh **global namespace** mein hai. Usko pakka pakadna
ho to aage sirf `::` lagao:

```cpp
int limit = 1000;               // global

int main() {
    int limit = 10;             // local ne global ko chhupa diya (shadowing, file 06)
    limit;                      // 10   -- local
    ::limit;                    // 1000 -- global
}
```

(Example mein yahan `-Wshadow` warning aati hai — jaan-boojh kar, kyunki yahi dikhana hai.)

---

## 🔑 `using` ke do roop — declaration vs directive

Har baar `nse::lot_size()` likhna bhaari lag sakta hai. `using` ke do bilkul alag roop hain:

```cpp
{
    using nse::lot_size;        // USING-DECLARATION: sirf EK naam laaya
    lot_size();                 // 50
}

{
    using namespace bse;        // USING-DIRECTIVE: bse ke SAARE naam dikhne lage
    lot_size();                 // 25
}
```

| | `using nse::lot_size;` | `using namespace bse;` |
|---|---|---|
| Kitne naam laata hai | sirf ek | poora namespace |
| Takkar ka khatra | kam — aapne khud chuna | zyada — pata nahi kya-kya aa gaya |
| Kahan theek hai | function ya chhote block mein | sirf chhote scope mein, `.cpp` file mein |
| Header file mein | bachna | **kabhi nahi** |

### Directive se takkar — asli error
```cpp
using namespace nse;
using namespace bse;
lot_size();     // ❌ error: call of overloaded 'lot_size()' is ambiguous
```
Yeh error GCC 16.2 pe chala ke liya hai. Dono namespace ke `lot_size` dikh rahe hain, compiler
kaise chune?

### Header mein `using namespace` kyun kabhi nahi?
Header file `#include` hote hi **copy-paste** ho jaati hai (folder 02 file 03). Agar
`trading.h` mein `using namespace std;` likha, to jo bhi `trading.h` include karega uske
code mein bhi `std` ke saare naam ghus jaayenge — bina uski marzi ke. Uska `count`, `size`,
`distance` achanak `std::count`, `std::size`, `std::distance` se takrane lagega.

---

## Anonymous (unnamed) namespace — "sirf is file ke liye"

```cpp
namespace {
    int file_private_counter = 0;
    void bump() { ++file_private_counter; }
}

bump();                         // is file mein normal naam ki tarah use karo
```

Bina naam ka namespace kehta hai: **yeh cheezein is `.cpp` file ke bahar nahi dikhengi**.
`nm` se proof:

```
000000000000002c t _ZN12_GLOBAL__N_1L4bumpEv                 (anonymous namespace)::bump()
0000000000000000 b _ZN12_GLOBAL__N_1L20file_private_counterE (anonymous namespace)::file_private_counter
0000000000000000 T _ZN3nse8lot_sizeEv                        nse::lot_size()
```

Letter dekho: `nse::lot_size` ke aage **bada `T`** (poore program ko dikhta hai — external
linkage), `bump` ke aage **chhota `t`** (sirf is file mein — internal linkage). Isliye doosri
`.cpp` file mein bhi `bump()` naam ka helper ho, to linker takkar nahi maarega.
(Linkage ki poori detail: folder 24 file 05.)

---

## Inline namespace — versioning

```cpp
namespace api {
    namespace v1 {
        int version() { return 1; }
    }
    inline namespace v2 {           // `inline` = yeh default version hai
        int version() { return 2; }
    }
}

api::version();        // 2  -- v2 ke members seedha api:: se mil jaate hain
api::v1::version();    // 1  -- purana version abhi bhi naam se mil sakta hai
api::v2::version();    // 2
```

Magic yeh hai ki symbol mein `v2` **abhi bhi likha hai**: `_ZN3api2v27versionEv`. Library
kal `v3` ko inline bana de, to naya code `api::version()` likh ke v3 lega, aur purana compiled
code jo v2 symbol pe link hua tha, woh tootega nahi. Library ABI versioning isi pe chalti hai
(folder 25 file 14). Aapko abhi yeh likhna kam padega — par library headers mein dikhega.

---

## 🔑 ADL — argument-dependent lookup

Yeh dekho:

```cpp
std::istringstream in("BUY 100 RELIANCE");
std::string side;
getline(in, side, ' ');          // std:: NAHI likha -- phir bhi compile hota hai!
// side = "BUY"
```

`getline` ke aage `std::` nahi hai, aur humne `using namespace std;` bhi nahi likha. Phir
compiler ko `std::getline` kaise mila?

**Rule:** jab aap koi function **bina qualify kiye** call karte ho (`getline(...)`, na ki
`std::getline(...)`), to compiler normal jagahon ke saath-saath **har argument ke type ke
namespace mein bhi** dhoondhta hai. `in` ka type `std::istringstream` hai → woh `std` mein
hai → compiler `std` mein bhi `getline` dhoondhta hai → mil gaya. Isi ko **ADL**
(argument-dependent lookup) kehte hain.

### Aap roz ADL use karte ho — bina jaane
```cpp
std::string sym = "RELIANCE";
std::cout << sym;
```
String ke liye `operator<<` ek **free function** hai jo `std` mein rehta hai. `std::cout <<
sym` asal mein yeh call hai:
```cpp
std::operator<<(std::cout, sym);    // example mein yahi likh ke chalaya
```
Aap `std::operator<<` nahi likhte — ADL arguments (`std::cout`, `sym`) dekh ke `std` mein
dhoondh leta hai. ADL na hota to har `<<` ke saath yeh poora naam likhna padta.

### ADL har argument dekhta hai
```cpp
int some_int = 0;
getline(some_int, side);    // ❌ error: no matching function for call to 'getline(int&, std::string&)'
```
Error message dhyaan se padho: "**no matching function**" — "not declared" nahi. `side` ka
type `std::string` hai, to ADL ne `std::getline` **dhoondh liya**; bas woh `int` accept nahi
karta. Error ka shabd batata hai ki lookup chala ya nahi (GCC 16.2 pe chala ke dekha).

### Overloading (file 08) se rishta
File 08 ka **Phase 1 — candidate set** ab poora hua: compiler ke candidates =
(normal scope mein dikhne wale functions) + (ADL se argument types ke namespaces mein mile
functions). Phir phase 2–3 mein best match chuna jaata hai.

### Aage ADL kahan dikhega
- Apne struct/class ke liye `operator<<` banaoge (folder 15 file 09) — ADL hi use dhoondhega.
- "Hidden friend" functions sirf ADL se milte hain (folder 15 file 10).
- `using std::swap; swap(a, b);` — do-step idiom (folder 18 file 02).
- Templates mein ADL ka second phase (folder 21 file 14).

---

## ⚠️ `std` mein apni cheez mat daalo

```cpp
namespace std {
    int my_helper() { return 1; }   // ❌ Undefined Behaviour
}
```

`std` namespace sirf standard library ka hai. Usme apne naye declarations jodna UB hai
(kuch khaas template specializations allowed hain — woh folder 21 mein). Compile ho jaaye to
bhi mat karo.

---

> **HFT relevance:** Bade trading codebases mein hazaaron files hoti hain — feed handler,
> order book, risk, gateway alag teams likhti hain. Namespaces (`md::`, `book::`, `risk::`)
> naam ki takkar rokte hain aur code padhte hi batate hain ki cheez kis component ki hai.
> Runtime cost **zero** hai — sirf symbol naam lamba hota hai. Do practical baatein:
> (1) `.cpp` file ke private helpers **anonymous namespace** mein rakho — woh internal linkage
> paate hain (upar `nm` mein chhota `t`), jisse compiler jaanta hai ki bahar se koi call nahi
> karega; yeh inlining aur unused code hataane mein madad karta hai (folder 24 file 05,
> folder 33). (2) Headers mein `using namespace` kabhi nahi — ek header ki galti poore
> codebase ke naam bigaad deti hai. Wire-protocol structs ke versions (`v1`, `v2`) ke liye
> inline namespace ek saaf pattern hai.

---

## Hands-on

`examples/08_namespaces.cpp` — do exchanges ke `lot_size`, reopen, nested + alias, `::`,
using-declaration vs directive, anonymous namespace, inline namespace, ADL:

```bash
./build.ps1 08-FUNCTIONS/examples/08_namespaces.cpp

# symbol naam khud dekho (namespace mangled name mein):
g++ -std=c++20 -c 08-FUNCTIONS/examples/08_namespaces.cpp -o ns.o
nm ns.o | grep -E "lot_size|bump|version"
nm -C ns.o | grep -E "lot_size|bump|version"      # -C = demangle, padhne layak naam
```

---

## ⚠️ Traps

### Trap 1 — ek scope mein do `using namespace` aur same naam
```cpp
using namespace nse;
using namespace bse;
lot_size();          // ❌ error: call of overloaded 'lot_size()' is ambiguous
```
Fix: `nse::lot_size()` likho, ya sirf `using nse::lot_size;`.

### Trap 2 — header mein `using namespace std;`
```cpp
// trading.h
using namespace std;    // ⚠️ har woh file jo trading.h include kare, ab std ke saare naam dekhti hai
```

### Trap 3 — local naam global ko chhupa de
```cpp
int limit = 1000;
void f() {
    int limit = 10;     // ⚠️ -Wshadow
    limit;              // 10, 1000 nahi. Global chahiye to ::limit
}
```

### Trap 4 — anonymous namespace header file mein
```cpp
// helpers.h
namespace { int counter = 0; }   // ⚠️ har .cpp jo include kare, usko APNA alag `counter` milega
```
Har file ki alag copy — ek file ne badla, doosri ko nahi dikhega. Folder 24 file 05 mein poora trap.

### Trap 5 — `std` namespace mein apni cheez jodna
```cpp
namespace std { void log(const char*); }   // ❌ UB
```

### Trap 6 — ADL se "chupke se" doosra function mil jaana
```cpp
namespace mylib { struct Price { long v; }; void print(Price); }
void print(mylib::Price);            // aapka global print
print(mylib::Price{100});            // ⚠️ dono candidate hain -> ambiguous
```
Struct folder 11 mein aayenge — tab yeh trap asli code mein dikhega. Abhi bas yaad rakho ki
unqualified call pe compiler argument ka namespace bhi dekhta hai.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Namespace runtime pe kuch karta hai" | Sirf compile-time; symbol naam ka hissa banta hai, cost zero |
| "Namespace ek hi jagah likha ja sakta hai" | Kitni bhi baar dobara khol sakte ho — `std` bhi sainkdo headers mein faila hai |
| "`using nse::lot_size;` aur `using namespace nse;` same hain" | Pehla ek naam laata hai, doosra poora namespace |
| "`using namespace std;` header mein theek hai" | Har include karne wali file mein ghus jaata hai — kabhi nahi |
| "Anonymous namespace = global" | Sirf is `.cpp` file tak (internal linkage — `nm` mein chhota `t`) |
| "`getline(in, s)` isliye chala kyunki `<string>` include tha" | ADL ki wajah se — `in` ka type `std` mein hai |
| "Inline namespace ka matlab inline function" | Alag cheez: members bahar wale namespace se bhi dikhte hain (versioning) |

---

## Exercises

1. **Takkar hatao:** yeh code compile nahi hota. Namespaces se theek karo, bina function ke
   naam badle.
   ```cpp
   double fee() { return 20.0; }   // broker A
   double fee() { return 15.5; }   // broker B
   ```
   <details><summary>Answer</summary>

   ```cpp
   namespace broker_a { double fee() { return 20.0; } }
   namespace broker_b { double fee() { return 15.5; } }
   // broker_a::fee() -> 20.0, broker_b::fee() -> 15.5
   ```
   Pehle wala code isliye fail tha kyunki same signature ke do functions ek hi scope mein
   the (redefinition). Namespace ne unke poore naam alag kar diye.
   </details>

2. **Output batao:**
   ```cpp
   int x = 1;
   namespace a { int x = 2; }
   int main() {
       int x = 3;
       std::cout << x << ::x << a::x;
   }
   ```
   <details><summary>Answer</summary>

   `312` — `x` local (3), `::x` global (1), `a::x` namespace wala (2).
   </details>

3. **Declaration vs directive:** neeche kaunsi line error degi, aur kyun?
   ```cpp
   namespace p { int id() { return 1; } }
   namespace q { int id() { return 2; } }
   int main() {
       using p::id;
       int a = id();             // (A)
       using namespace q;
       int b = id();             // (B)
   }
   ```
   <details><summary>Answer</summary>

   Koi error nahi — `a = 1`, `b = 1`. Using-**declaration** `using p::id;` ne `id` naam ko
   `main` ke scope mein **declare** kar diya. Using-**directive** `using namespace q;` naam ko
   utna paas nahi laata — lookup ke waqt `q` ke naam global scope jaisi door wali jagah se aate
   hain, aur `main` ke andar wala `p::id` pehle mil jaata hai aur baaki ko chhupa deta hai.
   Agar dono `using namespace p; using namespace q;` hote, tab (B) ambiguous hota (Trap 1).
   (Is jawab ko chala ke check kiya — neeche "What happens next?" #2 dekho.)
   </details>

4. **Alias:** `namespace exch::nse::fo::weekly { int lot() { return 25; } }` — ek alias banao
   jisse call `w::lot()` ho jaaye.
   <details><summary>Answer</summary>

   `namespace w = exch::nse::fo::weekly;` phir `w::lot()` → 25.
   </details>

5. **`nm` experiment:** `08_namespaces.cpp` ko `-c` se compile karke `nm` chalao. `bump` aur
   `nse::lot_size` ke aage kaunsa letter hai? Ab `bump` ko anonymous namespace se bahar nikaal
   ke dobara dekho. Kya badla, aur iska matlab kya hai?
   <details><summary>Answer</summary>

   Andar hone pe `bump` ke aage chhota `t` (local/internal), `lot_size` ke aage bada `T`
   (global/external). Bahar nikaalne pe `bump` bhi `T` ho jaata hai — ab doosri `.cpp` files
   bhi use dekh sakti hain, aur agar kisi aur file mein `bump()` hua to linker "multiple
   definition" error dega.
   </details>

6. **ADL pehchano:** in teeno mein se kaunsi calls ADL ki wajah se compile hoti hain?
   ```cpp
   std::string s = "hi";
   std::vector<int> v{3, 1, 2};
   std::cout << s;                  // (1)
   getline(std::cin, s);            // (2)
   std::size(v);                    // (3)
   ```
   <details><summary>Answer</summary>

   (1) aur (2) ADL se — `operator<<` aur `getline` unqualified hain, argument types `std` mein
   hain. (3) mein `std::` khud likha hai (qualified call) — wahan ADL ki zaroorat hi nahi.
   </details>

---

## What happens next?

1. ```cpp
   namespace api {
       namespace v1 { int version() { return 1; } }
       inline namespace v2 { int version() { return 2; } }
   }
   // Ab koi v2 se `inline` hata ke v1 pe laga de, aur program dobara compile ho:
   std::cout << api::version();     // <- ab kya print hoga?
   ```
   <details><summary>Answer</summary>

   `1`. `api::version()` hamesha us namespace ko pakadta hai jo **inline** hai. Default
   version badal gaya, call karne wala code nahi badla — versioning ka yahi point hai.
   </details>

2. ```cpp
   namespace p { int id() { return 1; } }
   namespace q { int id() { return 2; } }
   int main() {
       using p::id;
       using namespace q;
       std::cout << id();           // <- compile hoga? hoga to kya print?
   }
   ```
   <details><summary>Answer</summary>

   Compile hota hai aur `1` print hota hai (GCC 16.2 pe chala ke dekha). `using p::id;` ne
   `id` ko `main` ke scope mein declare kiya; `using namespace q;` ke naam lookup mein door se
   aate hain, isliye `p::id` jeet jaata hai. Agar `using p::id;` ki jagah `using namespace p;`
   hota → ambiguous error.
   </details>

3. ```cpp
   // a.cpp
   namespace { int counter = 0; }
   void inc_a() { ++counter; }
   // b.cpp
   namespace { int counter = 0; }
   int read_b() { return counter; }
   // main: inc_a(); inc_a(); std::cout << read_b();   <- link hoga? kya print?
   ```
   <details><summary>Answer</summary>

   Link ho jaata hai (koi "multiple definition" nahi) aur `0` print hota hai. Dono files ke
   `counter` **alag-alag** variables hain — har anonymous namespace sirf apni file ka hai.
   `inc_a()` ne a.cpp wala badla, `read_b()` b.cpp wala padhta hai. (Do files ke saath chala ke
   verify kiya.)
   </details>

---

## Interview questions

1. Namespace kya problem solve karta hai? Runtime cost kitni hai?
2. Using-declaration aur using-directive mein fark? Header mein `using namespace std;` kyun galat hai?
3. Anonymous namespace kya karta hai? `static` free function se kya rishta hai?
4. Inline namespace kis kaam aata hai? Symbol naam pe kya asar padta hai?
5. ADL kya hai? `std::cout << s` mein ADL kahan hai?
6. `std` namespace mein apna function jod sakte ho? Kyun nahi?
7. `getline(x, s)` pe "no matching function" aaya, "not declared" nahi — isse kya pata chalta hai?

---

## Next
→ [`15-exercises.md`](15-exercises.md)

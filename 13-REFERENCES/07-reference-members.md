# 07 — Reference members (class ke andar reference)

## Prerequisites
- [`05-returning-references.md`](05-returning-references.md)
- Folder 11 (structs), Folder 12 file 08 (pointers to structs)

## Yeh topic abhi kyun
Class/struct ke member ke roop mein reference rakhna **allowed** hai, par yeh
bahut saari cheezein aap se chheen leta hai: default construction, assignment,
easy copy. Zyada tar aise "kisi bahar wale object ka handle" ke liye **pointer**
(ya `std::reference_wrapper`) behtar hai. Kab kya — yahi lesson.

---

## Reference member — syntax aur pehla constraint

```cpp
struct Logger {
    std::ostream& out;              // reference member

    explicit Logger(std::ostream& o) : out(o) {}   // MEMBER INIT LIST mein bind ZAROORI
    void line(const std::string& s) { out << s << '\n'; }
};

std::ofstream file("log.txt");
Logger lg(file);                    // lg.out ab 'file' ka alias
```

- Reference member ko **constructor ki member initializer list** mein bind karna
  **padta hai** — body mein assign nahi kar sakte (`out = o;` woh referent ko
  likhega, bind nahi karega).
- Har constructor ko use bind karna hoga (warna compile error).

---

## Reference member kya cheenta hai

| Aap kho dete ho | Kyun |
|---|---|
| **Default constructor** | Reference bina bind ke exist nahi kar sakti |
| **Copy assignment** (`operator=`) | Reference rebind nahi hoti → compiler `operator=` delete kar deta hai |
| **Move assignment** | Same wajah |
| **`std::vector<Logger>` / resize** | Container ko assignable/movable elements chahiye — reference member se nahi |
| Aasani se relocate/`std::swap` | Assignment ke bina swap bhi nahi |

```cpp
Logger a(file1), b(file2);
a = b;               // ❌ ERROR -- copy assignment implicitly deleted (reference member)
std::vector<Logger> v;
v.push_back(a);      // copy-construct to chalega, par v.resize()/erase() aage jaake toot sakta
```

Copy **construction** chalti hai (naya `Logger` bana ke uski `out` ko `a.out`
wale object se bind kar dena — valid). Sirf **assignment** delete hota hai.

---

## ⚠️ Lifetime: reference member ka koi extension nahi

```cpp
struct Holder {
    const std::string& s;
    explicit Holder(const std::string& x) : s(x) {}
};

Holder h{ std::string("temp") };    // ⚠️ temporary "temp" is statement ke baad marta.
                                    //    h.s ab dangling. (const-ref member lifetime EXTEND NAHI karta)
std::cout << h.s;                   // UB
```

File 04 wala "`const T&` temporary ko zinda rakhta hai" **sirf local reference
variable** ke liye tha. **Member** ke liye nahi — GCC `-Wdangling-reference` /
`-Winit-list-lifetime` kuch cases pakadta hai, par sab nahi.

Rule: reference member sirf tab jab bind kiya gaya object **caller ke paas,
`Holder` se zyada der zinda** ho (e.g. ek long-lived service, ek parent object).

---

## Behtar options

### Option A — pointer member (rebindable, nullable, assignable)

```cpp
struct Logger {
    std::ostream* out = nullptr;    // default ctor OK, assignable, movable
    void line(const std::string& s) { if (out) *out << s << '\n'; }
};
```

Trade-off: har use pe null-check / deref. Par class normal (copyable, movable,
container-friendly) rehti hai. **Yeh default choice hai** "external handle" ke
liye.

### Option B — `std::reference_wrapper<T>` (assignable "reference")

```cpp
#include <functional>
struct Logger {
    std::reference_wrapper<std::ostream> out;   // rebindable "reference"
    explicit Logger(std::ostream& o) : out(o) {}
    void line(const std::string& s) { out.get() << s << '\n'; }
};

Logger a(file1), b(file2);
a = b;                 // ✅ ab chalta hai -- reference_wrapper assignable hai
```

`std::reference_wrapper` internally ek pointer hai jo **kabhi null nahi** (bina
`std::ref`/valid object ke banti hi nahi) aur **rebindable + assignable** hai.
`std::vector<std::reference_wrapper<T>>` bhi banta hai (file 02 ka "references ka
container" iske through). `.get()` se asli `T&` milta hai.

### Option C — value member (ownership le lo)

Agar object chhota hai ya class ko usko **own** karna chahiye, to reference/pointer
ki jagah bas value rakho — koi lifetime headache nahi.

---

## Kab reference member sach mein theek hai

- Class **short-lived** hai aur ek clearly longer-lived collaborator se bandhi
  (e.g. ek per-request handler jo ek engine ka `Book&` rakhta hai, jahan handler
  engine ke andar hi jeeta hai).
- Class ko **copy/move/default-construct karne ki zaroorat hi nahi** (aap
  explicitly aise design kar rahe ho).
- Non-null intent ko **type mein baandhna** chahte ho (reader ko `*` / null-check
  nahi dikhana).

In sab pe bhi, bahut saare senior codebases "reference member mat rakho, pointer
rakho" convention follow karte hain — kyunki lifetime bugs mehnge hote hain aur
pointer version future-proof hai.

---

## Andar kya hota hai

- Reference member **storage leta hai** — ~8 bytes (ek pointer), taaki object us
  alias ko yaad rakh sake. `sizeof(Logger with std::ostream&)` >= 8.
- Member init list mein `: out(o)` → object ke andar wale hidden pointer slot
  mein `&o` store hota hai. Baaki har `out` use → `[hidden_ptr]` deref.
- `std::reference_wrapper<T>` bhi exactly ek `T*` hold karta hai — same size,
  same codegen — bas assignable (pointer ko overwrite kar deta hai), aur API
  `T&` jaisi.
- Compiler-generated `operator=` **delete** hota hai kyunki member-wise assign
  reference ko rebind nahi kar sakta; isliye class non-assignable.

> **HFT relevance:** hot-path objects (order handlers, book views) aksar ek
> engine-owned structure ka reference/pointer rakhte hain — zero copy, direct
> access. Reference member tab jab woh object engine ke andar hi jeeta hai aur
> copy/move ki zaroorat nahi (ek fixed `std::array<Handler, N>`). Jahan handlers
> ko relocate / pool / reset karna ho — pointer ya `reference_wrapper`, taaki
> assignment zinda rahe. Lifetime hamesha review hota hai: dangling reference
> member = silent corruption.

---

## Hands-on

```cpp
#include <functional>
#include <iostream>

struct WithRef  { int& r;                              explicit WithRef(int& x): r(x) {} };
struct WithPtr  { int* p = nullptr;                                                     };
struct WithWrap { std::reference_wrapper<int> w;       explicit WithWrap(int& x): w(x) {} };

int main() {
    int a = 1, b = 2;
    WithRef  r1{a};   // r1 = r2;  -> ERROR (assignment deleted)
    WithPtr  p1{&a};  p1 = WithPtr{&b};      // ✅
    WithWrap w1{a};   w1 = WithWrap{b};      // ✅
    std::cout << sizeof(WithRef) << " " << sizeof(WithPtr) << " " << sizeof(WithWrap) << "\n";
}
```

```bash
g++ -std=c++20 -Wall -Wextra hands.cpp -o h && ./h    # prints: 8 8 8
```

---

## ⚠️ Traps

### Trap 1 — reference member ko body mein "assign"
```cpp
Logger(std::ostream& o) { out = o; }   // ⚠️ out already bound hona chahiye; yeh referent ko likhta hai
Logger(std::ostream& o) : out(o) {}    // ✅
```

### Trap 2 — temporary se reference member bind
```cpp
Holder h{ makeString() };   // ⚠️ h.s dangling (member lifetime extension nahi)
```

### Trap 3 — reference member wali class ko `std::vector` mein resize
```cpp
std::vector<Logger> v;  v.push_back(x);  v.push_back(y);   // ⚠️ realloc ko move/copy-assign chahiye -> compile error / breakage
```

### Trap 4 — sochna `operator=` bas "kaam karega"
```cpp
a = b;   // ❌ implicitly deleted -- error message padho: "use of deleted function"
```

### Trap 5 — `reference_wrapper` ko raw use karna
```cpp
std::reference_wrapper<std::ostream> w = std::cout;
w << "x";           // ❌ -- pehle w.get(), ya implicit conversion context
w.get() << "x";     // ✅
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Reference member 0 bytes (bas alias)" | ~8 bytes — hidden pointer store hota hai |
| "`const T&` member temporary ko zinda rakhta" | Nahi — member ke liye lifetime extension nahi |
| "Reference member wali class normal class hai" | No default-ctor, no copy/move assignment |
| "External handle = reference member" | Aksar pointer / `reference_wrapper` behtar (assignable, nullable) |
| "`reference_wrapper` slow wrapper hai" | Ek `T*` jitna — same codegen, bas assignable |

---

## Exercises

1. **Kya delete hota hai:** `struct S { int& r; S(int& x):r(x){} };` — `S`
   default-constructible? copy-constructible? copy-assignable? teenon test karo.

   <details><summary>Answer</summary>

   Default-ctor: nahi (reference bind chahiye). Copy-ctor: haan (naya `S`, uska
   `r` source ke referent se bind). Copy-assign: **nahi** (deleted — reference
   rebind nahi hoti).
   </details>

2. **Dangling member:** `struct Ref { const std::string& s; Ref(const
   std::string& x):s(x){} };` — `Ref r{ std::string("hi") };` kyun bug? `std::string
   name = "hi"; Ref r{name};` — ab safe? kab tak?

   <details><summary>Answer</summary>

   Temporary `std::string("hi")` full-expression ke baad marta → `r.s` dangling.
   `Ref r{name}` safe jab tak `name` `r` se zyada der zinda rahe (same/outer
   scope). Agar `name` pehle scope se nikal jaaye → dangling.
   </details>

3. **Fix with wrapper:** upar wali `struct S` ko `std::reference_wrapper<int>`
   se rewrite karo taaki `a = b;` compile ho. `sizeof` badla?

   <details><summary>Answer</summary>

   `struct S { std::reference_wrapper<int> r; S(int& x):r(x){} };` — `a = b` ab
   OK. `sizeof` same (8) — wrapper ek pointer hi hai.
   </details>

4. **Container:** `std::vector<T>` mein rakhne ke liye — `T` ko copy/move
   **assignable** kyun chahiye (sirf constructible nahi)? Reference member kyun
   fail karta hai?

   <details><summary>Answer</summary>

   `vector` grow/`erase`/`sort` elements ko **move-assign** karke shift karta
   hai. Reference member → move/copy-assign deleted → yeh operations compile
   nahi hote. Pointer / value / `reference_wrapper` members theek.
   </details>

5. **Design call:** ek `RequestHandler` jo ek `Engine` (poore program tak zinda)
   ko access karta hai. Reference member, pointer member, ya `reference_wrapper`
   — kya choose karoge aur kyun?

   <details><summary>Answer</summary>

   Agar `RequestHandler` copy/move/reset nahi hota aur `Engine` sach mein hamesha
   zinda → `Engine&` member theek (non-null intent type mein). Agar handlers
   pool/reset/relocate hote hain → `Engine*` (ya `reference_wrapper`) taaki
   assignable rahe. Bahut se codebases blanket "pointer member" rule rakhte hain.
   </details>

---

## Interview questions

1. Reference member kaise initialize hota hai? Body mein kyun nahi?
2. Reference member wali class ke kaunse special members delete/unavailable ho jaate hain?
3. `const T&` member — lifetime extension milti hai? (file 04 se contrast)
4. Reference member ka `sizeof` contribution — kitna, kyun?
5. `std::reference_wrapper<T>` reference member se kaise better (aur kaise same)?
6. External collaborator ke liye reference vs pointer member — trade-offs.

---

## Next
→ [`08-rvalue-references-intro.md`](08-rvalue-references-intro.md)

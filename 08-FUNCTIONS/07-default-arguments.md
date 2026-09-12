# 07 — Default arguments

## Prerequisites
- [`03-parameters-and-arguments.md`](03-parameters-and-arguments.md)
- [`02-declaration-vs-definition.md`](02-declaration-vs-definition.md)

## Yeh topic abhi kyun
Kabhi ek parameter ki value **99% baar same** hoti hai. Default argument se aap
usse optional bana dete ho — caller de to de, na de to default. Config functions,
logging, retries — sab mein useful. Par iske rules (kahan likhna hai, kab
evaluate hota hai) mein 3 common galtiyan hain.

---

## Basic

```cpp
void connect(const std::string& host, int port = 8080, int timeoutMs = 5000);

connect("example.com");                  // port=8080, timeout=5000
connect("example.com", 9090);            // timeout=5000
connect("example.com", 9090, 1000);      // sab explicit
```

Default = "agar caller ne yeh argument nahi diya, to yeh use karo."

---

## Rules

### 1. Sirf TRAILING parameters — right se left

```cpp
void f(int a, int b = 2, int c = 3);      // ✅ b, c ke baad koi non-default nahi
void f(int a = 1, int b, int c);          // ❌ ERROR -- a ke baad b, c non-default
void f(int a, int b = 2, int c);          // ❌ ERROR -- b ke baad c non-default
```

Kyun: `f(10, 20)` mein `20` `b` ko jaata hai, `c` default. Beech ka argument
"skip" nahi kar sakte. Isli ye jinke default hain woh **end mein** hone chahiye.

### 2. Default sirf ek jagah — aksar DECLARATION mein

```cpp
// header.hpp
void log(std::string_view msg, int level = 1);     // default yahan

// source.cpp
void log(std::string_view msg, int level) {         // yahan DOBARA mat likho
    // ...
}                                                    // `int level = 1` yahan -> ERROR: redefinition of default
```

Rule: **jo declaration callers dekhte hain, wahan default lagao** (header). Ek hi
TU mein agar declaration + definition dono ho, to kisi ek mein — convention:
declaration mein.

### 3. Har CALL pe evaluate hota hai (baad wale defaults new value dete hain)

```cpp
int counter = 0;
int next() { return ++counter; }

void f(int x = next());          // default = next() ka result

f();   // x = 1
f();   // x = 2   -- har call pe next() dobara chalta hai
f(99); // x = 99  -- next() chala hi nahi
```

Default expression **call site pe** evaluate hoti hai, definition pe nahi.

### 4. Default ki visibility caller pe depend karti hai

```cpp
void f(int a, int b = 10);       // yeh dekhne wale ke liye f(5) valid

// agar kisi TU mein sirf `void f(int, int);` dikhta hai (no default),
// to wahan f(5) ERROR -- us TU ko default nahi pata
```

### 5. Default `this` ke members / non-static ko use nahi kar sakta

```cpp
struct S {
    int scale = 2;
    int apply(int x, int factor = scale);   // ❌ ERROR -- `scale` non-static member
};
```

Static members / globals / constants theek hain.

---

## Default argument vs overloading

Dono "ek naam, kai tarah se call" dete hain. Kab kaunsa:

| | Default argument | Overloading |
|---|---|---|
| Ek function, kam likhna | ✅ | ❌ (kai definitions) |
| Alag **behaviour** per case | ❌ (body ek hi) | ✅ |
| Alag parameter **types** | ❌ | ✅ |
| Beech ka param skip | ❌ | ✅ (alag signatures) |

```cpp
// Default arg -- same logic, ek param optional
void draw(const Shape& s, Color c = Color::Black);

// Overload -- alag behaviour / alag types
void print(int);
void print(const std::string&);
void print(const std::vector<int>&);
```

Ek gotcha: **default arg + overload mix** karna ambiguity bana sakta hai:
```cpp
void g(int a, int b = 0);
void g(int a);                 // ⚠️ g(5) -- dono match -> ambiguous
```

---

## Common patterns

```cpp
// optional config
Logger makeLogger(std::string_view name,
                  Level level = Level::Info,
                  bool toFile = false);

// "sensible default, override jab chahiye"
std::string trim(std::string_view s, std::string_view chars = " \t\n\r");

// retry with default budget
bool fetch(const Url& u, int retries = 3, std::chrono::milliseconds backoff = 100ms);
```

⚠️ **Default argument as "output flag" mat use karo** — confusing:
```cpp
int parse(std::string_view s, bool& error = throwaway);   // ⚠️ don't
```

---

## Andar kya hota hai

Default argument **caller side** pe fill hota hai — compiler `f("host")` ko
`f("host", 8080, 5000)` mein badal deta hai **call site pe**. Function ke andar
koi "kya default use hua" jaisa flag nahi. Isi liye:
- Default expression caller ke context mein evaluate hoti hai
- Har TU ko default value pata honi chahiye (warna call banana nahi aata)
- Runtime cost: default value banane ki cost (`next()` call, `100ms` literal — usually free)

> **HFT relevance:** Config/setup functions mein defaults theek hain (cold path).
> Hot path functions mein defaults se bacho jo **non-trivial evaluation** karein
> (allocation, function call) — woh har call pe chalta hai. Aur cross-TU:
> default sirf header mein rakho taaki har caller ko consistent value mile
> (mismatched defaults across headers = subtle bug). Interfaces mein aksar
> overloading ya explicit "options struct" (`struct FetchOptions { int retries
> = 3; ... };`) zyada maintainable hota hai defaults ki lambi list se.

---

## Hands-on

```cpp
#include <iostream>
int calls = 0;
int nextVal() { std::cout << "[nextVal] "; return ++calls; }

void demo(int x = nextVal()) { std::cout << "x = " << x << "\n"; }

int main() {
    demo();      // nextVal chalega
    demo();      // dobara
    demo(100);   // nahi chalega
}
```

```bash
g++ -std=c++20 -Wall -Wextra defarg.cpp -o d && ./d
```

---

## ⚠️ Traps

### Trap 1 — non-trailing default
```cpp
void f(int a = 1, int b);          // ❌ ERROR
```

### Trap 2 — default declaration + definition dono mein
```cpp
void f(int x = 5);
void f(int x = 5) { }              // ❌ redefinition of default argument
```

### Trap 3 — "default ek baar evaluate hota hai" maan lena
```cpp
void log(std::string_view m, std::string t = currentTime());   // ⚠️ har call pe currentTime()
```
(Yeh actually aksar wahi hai jo chahiye — par pata hona chahiye.)

### Trap 4 — default value ka ek "static local vector" jaisa mutable object
```cpp
void add(int x, std::vector<int>& sink = defaultSink);   // ⚠️ Python-style mutable-default bug ki yaad
```
(C++ mein har call pe `defaultSink` wahi object — usme accumulate hota rahega.)

### Trap 5 — default + overload = ambiguity
```cpp
void g(int);
void g(int, int = 0);              // g(5) ambiguous
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Kisi bhi param ko default de sakte ho" | Sirf trailing (right-side) |
| "Default header aur source dono mein" | Ek jagah — convention: declaration/header |
| "Default ek baar evaluate hota hai" | Har call pe (jahan caller ne skip kiya) |
| "Default arg = overloading" | Overload alag behaviour/types; default = same body, optional param |
| "Function ko pata hota hai default use hua" | Nahi — caller side pe fill hota hai |

---

## Exercises

1. **Trailing rule:** in mein se kaunse valid —
   ```cpp
   void a(int x, int y = 1, int z = 2);
   void b(int x = 1, int y = 2, int z);
   void c(int x, int y = 1, int z);
   void d(int x = 0);
   ```
   <details><summary>Answer</summary>`a` aur `d` valid. `b`, `c` — non-trailing default → error.</details>

2. **Per-call eval:** upar wala `demo(int x = nextVal())` chalao. `nextVal` kitni
   baar chala 3 calls mein (`demo(); demo(); demo(5);`)?
   <details><summary>Answer</summary>2 baar (teesri call ne value di).</details>

3. **Header/source:** `math.hpp` mein `int scale(int v, int factor = 2);`,
   `math.cpp` mein definition — `factor = 2` source mein bhi likho. Error? Hatao.

4. **Options struct:** yeh 5-default-arg function ko ek `struct FetchOptions`
   mein badlo —
   ```cpp
   Response fetch(Url u, int retries = 3, int timeoutMs = 5000,
                  bool followRedirects = true, bool cache = false,
                  std::string_view userAgent = "app/1.0");
   ```

5. **Ambiguity:** `void p(int);` aur `void p(int, int = 0);` — `p(5)` compile
   hua? Error text?

6. **Default vs overload:** ek `area()` jo `area(5)` (square) aur `area(3, 4)`
   (rectangle) dono handle kare — pehle default arg se try karo (`int area(int w,
   int h = ?)` — kya `h` ke liye "square" signal doge?), phir 2 overloads se.
   Kaunsa saaf?

---

## Interview questions

1. Default argument kin parameters pe laga sakte ho? Kyun sirf trailing?
2. Default kahan likhna chahiye — declaration ya definition? Dono mein likhne pe?
3. Default expression kab evaluate hoti hai — ek baar ya har call pe?
4. Default argument aur function overloading — kab kaunsa?
5. `void f(int); void f(int, int = 0);` — `f(5)` ka kya?
6. Non-static member ko default value mein use kyun nahi kar sakte?

---

## Next
→ [`08-function-overloading.md`](08-function-overloading.md)

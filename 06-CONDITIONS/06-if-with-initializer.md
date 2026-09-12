# 06 — `if` with initializer (C++17)

## Prerequisites
- [`01-if-statement.md`](01-if-statement.md), [`04-switch-statement.md`](04-switch-statement.md)
- `03-VARIABLES-DATA-TYPES/12-auto-and-type-deduction.md`
- `03-VARIABLES-DATA-TYPES/02-what-is-a-variable.md` (scope basics)

## Yeh topic abhi kyun
C++17 ne `if` aur `switch` mein ek chhota par bahut useful feature joda: condition
se **pehle** ek initializer statement. Ismein aap ek variable bana sakte ho jo
**sirf us `if`/`else` block ke andar** zinda rehta hai.

Yeh scope-hygiene ka tool hai — aur functions ke error handling (folder 08, 23),
iterators (folder 19), `std::optional` (19), lock guards (17, 26) ke saath roz
kaam aata hai.

---

## Syntax

```cpp
if (init-statement; condition) {
    // ...
}
```

Do parts, `;` se alag:
1. **init-statement** — ek declaration ya expression statement (jaise `for` ka
   pehla part)
2. **condition** — normal `if` condition (init ki hui cheez use kar sakti hai)

```cpp
if (int n = countItems(); n > 0) {
    std::cout << n << " items\n";      // n visible
} else {
    std::cout << "empty (n = " << n << ")\n";   // n yahan bhi visible
}
// n yahan visible NAHI
```

---

## Pehle vs ab

### Purana tareeka 1 — extra braces

```cpp
{
    auto it = cache.find(key);
    if (it != cache.end()) {
        use(it->second);
    }
}   // it ka scope yahan khatam
```

Kaam karta hai, par nesting aur `{ }` ka shor.

### Purana tareeka 2 — variable leak

```cpp
auto it = cache.find(key);
if (it != cache.end()) {
    use(it->second);
}
// ⚠️ `it` yahan bhi zinda hai -- galti se dobara use ho sakta hai
// aur agla `auto it = otherMap.find(...)` -Wshadow / naming clash
```

### C++17 — `if` with initializer

```cpp
if (auto it = cache.find(key); it != cache.end()) {
    use(it->second);
}
// `it` ka scope sirf yeh if/else -- clean
```

Ek line, no extra braces, `it` bahar leak nahi hota.

---

## Kahan bahut fit baithta hai

### 1. Lookup + check (maps, sets)

```cpp
if (auto it = orders.find(id); it != orders.end()) {
    cancel(it->second);
} else {
    log("order not found: ", id);
}
```

### 2. `std::optional` unwrap

```cpp
if (auto price = tryGetPrice(symbol); price.has_value()) {
    submit(*price);
}
// ya seedha:
if (auto price = tryGetPrice(symbol); price) {
    submit(*price);
}
```

### 3. Error code check (C-style APIs, syscalls)

```cpp
if (int rc = ::connect(fd, addr, len); rc != 0) {
    perror("connect");
    return rc;
}
// rc bahar nahi jaata -- agla syscall apna rc bana sakta hai
```

### 4. Lock + guarded work (folder 17, 26 preview)

```cpp
if (std::lock_guard lk(mtx); queue.empty()) {
    return;          // lk yahin destruct -> unlock
}
// ⚠️ note: `lk` poore if/else tak lock rakhega -- chhota block rakho
```

### 5. `switch` ke saath bhi

```cpp
switch (auto ev = poll(); ev.type) {
    case Event::Data:  handleData(ev);  break;
    case Event::Error: handleError(ev); break;
    default:           break;
}
// ev sirf is switch ke andar
```

---

## Scope ki exact tasveer

```cpp
if (T x = init; cond(x)) {
    // x visible
} else if (cond2(x)) {      // x visible
    // x visible
} else {
    // x visible
}
// x DESTROYED yahan (block ke baad) -- iska destructor yahan chalta hai
```

`x` ka lifetime = poora `if`-`else if`-`else` structure. Uske end pe destructor
(RAII cleanup) chalta hai — deterministic.

⚠️ **`else if` chain mein init sirf pehle `if` pe lagta hai:**

```cpp
if (auto a = f(); a > 0) {
    ...
} else if (auto b = g(); b > 0) {    // b ka apna init -- yeh alag if hai
    ...                              // yahan a AUR b dono visible
}
```

---

## Yeh syntactic sugar hai — koi runtime cost nahi

```cpp
if (auto v = compute(); v.ok()) { use(v); }
```

barabar hai:

```cpp
{
    auto v = compute();
    if (v.ok()) { use(v); }
}
```

Compiler ke liye **bilkul same** — same assembly. Faayda sirf: kam scope, kam
naming bugs, saaf code. `-O2` pe temporary bhi optimize ho jaata hai jahan possible.

---

## Kab use na karo

- **Variable ko baad mein chahiye** → normal declaration
  ```cpp
  auto result = doWork();
  if (!result.ok()) return result;
  logSuccess(result);           // result yahan chahiye -> init-if galat
  return result;
  ```
- **Init trivial hai aur condition alag cheez pe** → forced lagega
  ```cpp
  if (int unused = 0; globalFlag) { }   // ⚠️ pointless
  ```
- **Readability girti ho** — line bahut lambi ho jaaye to alag declaration behtar

---

## Hands-on

Chhota program likho aur chalao:

```cpp
#include <iostream>
#include <map>
#include <string>

int main() {
    std::map<std::string, int> stock = {{"AAPL", 100}, {"MSFT", 50}};

    for (const std::string sym : {"AAPL", "TSLA"}) {
        if (auto it = stock.find(sym); it != stock.end()) {
            std::cout << sym << ": " << it->second << " units\n";
        } else {
            std::cout << sym << ": not held\n";
        }
        // `it` yahan compile error dega agar use karo -- try it
    }
}
```

```bash
g++ -std=c++20 -Wall -Wextra -Wshadow -g initif.cpp -o initif && ./initif
```

Phir block ke baad `std::cout << it->second;` add karke dekho — error kya aaya?

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `;` ke bajaye `,`
```cpp
if (auto x = f(), x > 0) { }    // ❌ yeh comma operator hai, galat parse
if (auto x = f(); x > 0) { }    // ✅ semicolon
```

### Trap 2 — init variable ko block ke baad use karna
```cpp
if (auto n = parse(s); n > 0) { }
return n;    // ❌ ERROR: n scope se bahar
```

### Trap 3 — `lock_guard` ko init mein daal ke bada block rakhna
```cpp
if (std::lock_guard lk(mtx); ready) {
    doLongWork();    // ⚠️ poore time lock held -- shayad aap yeh nahi chahte
}
```
Lock ka scope soch-samajh ke rakho.

### Trap 4 — `-Wshadow` se bachne ke liye zabardasti use karna
Tool sahi kaam ke liye hai, har jagah nahi. Judgement.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Init-`if` runtime pe tez/slow hai" | Bilkul same assembly as extra-braces version |
| "Init variable `else` mein visible nahi" | Poore `if`/`else if`/`else` mein visible |
| "`if (init, cond)`" | `,` nahi — `;` chahiye |
| "`else if` ka apna init pehle wale se juda hai" | Har `if` ka apna init; scopes nest hote hain |
| "Yeh sirf maps ke liye hai" | optional, error codes, locks, any lookup+check |

---

## Exercises

1. **Convert karo** init-`if` mein:
   ```cpp
   auto pos = line.find('=');
   if (pos != std::string::npos) {
       key = line.substr(0, pos);
   }
   ```
   <details><summary>Answer</summary>

   ```cpp
   if (auto pos = line.find('='); pos != std::string::npos) {
       key = line.substr(0, pos);
   }
   ```
   </details>

2. **Scope test:** yeh compile hoga?
   ```cpp
   if (int x = getVal(); x > 10) {
       std::cout << x;
   }
   std::cout << x;
   ```
   <details><summary>Answer</summary>
   Nahi — doosra `std::cout << x;` error (`x` scope ke bahar). Pehla theek hai.
   </details>

3. **`else` mein access:**
   ```cpp
   if (int rc = tryConnect(); rc == 0) {
       std::cout << "connected\n";
   } else {
       std::cout << "failed, code " << rc << "\n";   // rc yahan valid?
   }
   ```
   `rc` `else` mein use ho sakta hai? Chalao aur verify.
   <details><summary>Answer</summary>
   Haan — init variable poore `if`/`else` structure mein visible hai.
   </details>

4. **`switch` with init:** `poll()` se ek `Event` lo aur uske `.type` pe `switch`
   karo, ek hi line mein. `Event` sirf switch ke andar zinda rahe.

5. **`optional` unwrap:**
   ```cpp
   std::optional<int> parsePort(const std::string& s);
   ```
   Init-`if` se: agar valid port mile to `listen(port)`, warna `"bad port"` print.

6. **Kab NA use karein:** ek example likho jahan init-`if` galat choice hai kyunki
   variable baad mein chahiye.

---

## Interview questions

1. `if` with initializer ka syntax? Init variable ka scope kya hai?
2. C++17 se pehle same effect kaise paate the? Us tareeke ke 2 nuksaan?
3. Init variable `else` block mein accessible hai?
4. Init-`if` ka koi runtime cost hai extra-braces version ke mukable?
5. `if (auto lk = std::lock_guard(mtx); cond)` mein lock kab release hota hai?
6. `else if` chain mein har `if` ka apna initializer ho sakta hai? Scope kaise nest karta hai?

---

## Next
→ [`07-conditional-bugs.md`](07-conditional-bugs.md)

# 04 — `switch` statement

## Prerequisites
- [`02-else-and-else-if.md`](02-else-and-else-if.md)
- `03-VARIABLES-DATA-TYPES/07-char-and-ascii.md` (char integral hai)
- `05-OPERATORS/05-bitwise-operators.md` mein `enum class` ka intro dekha tha

## Yeh topic abhi kyun
Jab ek hi value ko **kai constant values** se compare karna ho — day number,
message type, opcode, state — `if/else if` chain lambi aur repetitive ho jaati hai.
`switch` iske liye bana hai: saaf syntax, aur (integer/enum pe) compiler aksar
**jump table** bana deta hai — O(1) dispatch (file 05 mein poora).

Par `switch` mein do C-legacy traps hain — **fallthrough** aur **case scope** — jo
har naye programmer ko kaatte hain.

---

## Basic syntax

```cpp
switch (expression) {
    case constant1:
        // ...
        break;
    case constant2:
        // ...
        break;
    default:
        // koi case match nahi hua
        break;
}
```

```cpp
switch (day) {
    case 1: std::cout << "Monday\n";    break;
    case 2: std::cout << "Tuesday\n";   break;
    case 3: std::cout << "Wednesday\n"; break;
    default: std::cout << "Other\n";    break;
}
```

### Rules

| Rule | Detail |
|---|---|
| `expression` **integral ya enum** | `int`, `char`, `short`, `long`, `enum`, `enum class`, `bool`. **`float`/`double`/`std::string` NAHI.** |
| har `case` label **compile-time constant** | literal, `constexpr`, enumerator. Variable nahi. |
| `case` labels **unique** | do same values → compile error |
| `case` sirf ek **entry point** hai | wahan se aage sab chalta hai jab tak `break`/`return`/`}` |
| `default` optional | kahin bhi likh sakte ho, par convention: aakhri mein |

---

## 🔑 `break` — aur FALLTHROUGH

Yeh `switch` ki sabse badi trap hai.

`case constant:` sirf yeh kehta hai: "yahan se andar aao". Ek baar andar aa gaye,
to **agle `case` labels ko ignore karke** sab kuch chalta hai — jab tak `break`,
`return`, ya block ka end (`}`) na aaye.

```cpp
int level = 1;
switch (level) {
    case 1: std::cout << "[basic] ";          // break nahi...
    case 2: std::cout << "[intermediate] ";   // ...to yahan bhi gir gaya
    case 3: std::cout << "[advanced] ";       // ...aur yahan bhi
            break;
    default: std::cout << "(none)";
}
// Output: [basic] [intermediate] [advanced]
```

`level == 1` hone pe bhi teenon chhap gaye. Yeh **fallthrough** hai. Agar aap sirf
`[basic]` chahte the — **bug**. `break` bhoolna sabse common `switch` bug hai.

### Compiler madad karta hai

`g++ -Wall -Wextra` (specifically `-Wimplicit-fallthrough`) warn karta hai:

```
warning: this statement may fall through [-Wimplicit-fallthrough=]
```

**Isli-Wall -Wextra hamesha on.** (`examples/02_switch_demo.cpp` mein yeh demo hai —
usme warning locally silence ki gayi hai sirf asar dikhane ke liye.)

---

## Jaan-boojh kar fallthrough

Kabhi-kabhi fallthrough **chahiye** hota hai — kai cases ka same treatment:

### Clean grouping (koi warning nahi)

Jab `case` labels ke **beech koi code nahi** ho, compiler ise intentional maanta hai:

```cpp
switch (c) {
    case 'a': case 'e': case 'i': case 'o': case 'u':
    case 'A': case 'E': case 'I': case 'O': case 'U':
        std::cout << "vowel\n";
        break;
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        std::cout << "digit\n";
        break;
    default:
        std::cout << "other\n";
        break;
}
```

Yeh idiomatic hai — koi warning nahi.

### `[[fallthrough]]` — jab labels ke beech code ho

C++17 attribute. "Yeh gira hui hai, jaan-boojh kar" — reader aur compiler dono ko
batata hai:

```cpp
switch (n) {
    case 0:
        std::cout << "zero ";
        [[fallthrough]];       // ; ke saath. warning silence.
    case 1:
        std::cout << "<=1 ";
        break;
    default:
        std::cout << "big ";
        break;
}
// n == 0  ->  "zero <=1 "
// n == 1  ->  "<=1 "
```

`[[fallthrough]]` ke bina `case 0` mein code hone se `-Wimplicit-fallthrough`
warn karega.

---

## `default`

"In mein se koi nahi" case:

```cpp
switch (opcode) {
    case OP_ADD: ...; break;
    case OP_SUB: ...; break;
    default:
        std::cerr << "unknown opcode: " << opcode << "\n";
        break;
}
```

- Optional hai. Na ho aur koi case match na kare → poora `switch` skip.
- `enum class` pe **jaan-boojh kar `default` chhodna** ek technique hai — neeche.

---

## `switch` on `enum class` — compile-time completeness

```cpp
enum class Side { Buy, Sell };

std::string_view name(Side s) {
    switch (s) {
        case Side::Buy:  return "BUY";
        case Side::Sell: return "SELL";
    }
    return "?";   // compiler ko lagta hai koi raasta chhoot sakta hai
}
```

Agar aap `Side` mein `Cancel` add karo par `name()` mein `case` na daalo:

```
warning: enumeration value 'Cancel' not handled in switch [-Wswitch]
```

**Yeh `switch` ka bada faayda hai `if/else if` ke mukable.** `if` chain mein aisi
koi warning nahi milti — naya enum value silently `else` mein chala jaata.

Rule of thumb:
- `enum class` pe `switch` → **`default` mat lagao**, sab cases explicit likho →
  compiler naye enum values pe yaad dila dega
- External / untrusted `int` pe `switch` → `default` **zaroor** (invalid input handle)

---

## ⚠️ `case` ke andar variable — `{ }` block chahiye

```cpp
switch (opcode) {
    case 1:
        int bonus = 10;          // ❌ ERROR: jump to case label crosses initialization
        use(bonus);
        break;
    case 2:
        use(bonus);              // bonus yahan "visible" hai par initialize nahi hua!
        break;
}
```

Saare `case` ek hi scope share karte hain. `case 2` pe jump `bonus` ki
initialization ko **bypass** kar deta hai — C++ ise error banata hai.

**Fix — har case ko apna block do:**

```cpp
switch (opcode) {
    case 1: {
        int bonus = 10;
        use(bonus);
        break;
    }
    case 2: {
        int bonus = 25;          // alag scope, alag variable
        use(bonus);
        break;
    }
}
```

Aadat daal lo: **jis `case` mein local variable ho, usko `{ }` mein wrap karo.**

---

## `switch` vs `if/else if`

| | `switch` | `if / else if` |
|---|---|---|
| Kis pe | integral / enum only | koi bhi bool expression |
| Ranges (`x > 5 && x < 10`) | ❌ | ✅ |
| `float` / string | ❌ | ✅ |
| Compiler jump table (O(1)) | ✅ (aksar) | ❌ (sequential compares) |
| Missing enum case warning | ✅ (`-Wswitch`) | ❌ |
| Fallthrough trap | ✅ (dhyaan) | nahi |

Detail — assembly ke saath — **file 05** mein.

---

## Andar kya hota hai (preview)

`switch` ke liye compiler 3 mein se kuch banata hai:
1. **Jump table** — cases dense hon (0,1,2,3,…) → array of addresses, direct index.
   O(1), branch predictor ke liye ek indirect branch.
2. **Binary search tree** of compares — cases bikhre hon (1, 100, 5000) → `if` tree,
   O(log n).
3. **Sequential compares** — bahut kam cases → `if/else` jaisa.

Kaunsa — depends on case values ki density. File 05 mein `-S` se dekhenge.

> **HFT relevance:** Message/event dispatch loops mein `switch` on `enum` /
> message-type-byte preferred hai: jump table O(1) hai (chain ke O(n) compares se
> behtar), aur `-Wswitch` naye message types add karne pe compile-time safety deta
> hai. Order book / matching engine ke event handlers isi tarah likhe jaate hain
> (folders 39–40).

---

## Hands-on

`examples/02_switch_demo.cpp` — basic switch, fallthrough bug, `[[fallthrough]]`,
case+variable, enum class switch:

```bash
./build.ps1 06-CONDITIONS/examples/02_switch_demo.cpp
```

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `break` bhoolna
Sabse common. `-Wimplicit-fallthrough` on rakho.

### Trap 2 — `default` upar likhna aur `break` bhoolna
```cpp
switch (x) {
    default: handleUnknown();   // break nahi...
    case 1: handleOne();        // ...to yeh bhi chal gaya
}
```
`default` kahin bhi ho sakta hai — uske baad bhi `break` chahiye.

### Trap 3 — `case` mein non-constant
```cpp
int threshold = getThreshold();
switch (x) {
    case threshold: ...   // ❌ ERROR -- constant nahi
}
```

### Trap 4 — `float` / `string` pe switch
```cpp
switch (price) { ... }        // ❌ price double hai -> ERROR
switch (name) { ... }         // ❌ std::string -> ERROR
```
`if/else` ya `std::unordered_map` use karo.

### Trap 5 — case + variable bina `{ }`
```cpp
case 1: std::string s = "x";  // ❌ crosses initialization
```

### Trap 6 — do cases same value
```cpp
case 1: ...
case 1: ...     // ❌ ERROR: duplicate case value
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`case` match hua to bas wahi block chalega" | `break` na ho to **aage sab** chalta hai |
| "`switch` string pe chalta hai" | Sirf integral / enum |
| "`case (x > 5):`" | `case` constant hota hai, condition nahi |
| "`default` sirf aakhri mein" | Kahin bhi — par uske baad bhi `break` chahiye |
| "`enum` pe `default` accha practice hai" | `enum class` pe **chhodo** — `-Wswitch` help karega |
| "`switch` `if` se hamesha tez" | Aksar (jump table), par depends — file 05 |

---

## Exercises

1. **Fallthrough predict karo:**
   ```cpp
   int x = 2;
   switch (x) {
       case 1: std::cout << "one ";
       case 2: std::cout << "two ";
       case 3: std::cout << "three "; break;
       case 4: std::cout << "four ";
   }
   ```
   <details><summary>Answer</summary>
   `two three ` — `x==2` se entry, `case 2` aur `case 3` chale, `break` pe ruka.
   `case 1` aur `case 4` skip.
   </details>

2. **Bug fix:** yeh function har din ke liye "weekday" ya "weekend" batana chahiye —
   ```cpp
   std::string_view kind(int day) {   // 1=Mon .. 7=Sun
       switch (day) {
           case 1: case 2: case 3: case 4: case 5:
               return "weekday";
           case 6: case 7:
               return "weekend";
       }
       return "invalid";
   }
   ```
   Yeh sahi hai? `-Wall -Wextra` se compile karo — koi warning?
   <details><summary>Answer</summary>
   Sahi hai. Grouped cases (labels ke beech code nahi) → koi fallthrough warning
   nahi. `return` `break` ki zaroorat khatam kar deta hai.
   </details>

3. **`[[fallthrough]]` lagao:** is code mein jahan intentional fallthrough hai, wahan
   `[[fallthrough]];` daalo taaki warning na aaye —
   ```cpp
   switch (logLevel) {
       case DEBUG: writeDebugInfo();
       case INFO:  writeTimestamp();
       case WARN:  writeMessage(); break;
   }
   ```

4. **case + variable:** yeh compile nahi hota. Fix karo —
   ```cpp
   switch (op) {
       case ADD: int r = a + b; print(r); break;
       case MUL: int r = a * b; print(r); break;
   }
   ```
   <details><summary>Answer</summary>
   Har case ko `{ }` do: `case ADD: { int r = a + b; print(r); break; }` — ab `r`
   har case mein alag scope. (Bina block ke: "duplicate declaration" + "crosses
   initialization" dono errors.)
   </details>

5. **enum completeness:** yeh enum aur switch likho —
   ```cpp
   enum class OrderType { Market, Limit, Stop, StopLimit };
   ```
   `describe(OrderType)` likho jo har type ka string de, **`default` ke bina**. Phir
   enum mein `Iceberg` add karo aur `-Wall` se compile karo. Warning aayi? Wahi
   `-Wswitch` ka faayda hai.

6. **`switch` → `if`:** kis case mein `switch` use **nahi** kar sakte, aur kyun —
   - (a) `int status` 0/1/2/3 pe
   - (b) `double temperature` ranges pe
   - (c) `char grade` 'A'/'B'/'C' pe
   - (d) `std::string command` pe
   <details><summary>Answer</summary>
   (b) `double` — switch integral/enum only, aur ranges bhi nahi. (d) `std::string`
   — non-integral. (a) aur (c) `switch` ke liye theek hain (`char` integral hai).
   </details>

---

## Interview questions

1. `switch` kis type ke expressions pe chal sakta hai? Kis pe nahi?
2. Fallthrough kya hai? Intentional fallthrough kaise document karte ho?
3. `case` label pe kya restrictions hain?
4. `case` ke andar variable declare karne pe kya dikkat, kya fix?
5. `enum class` pe `switch` mein `default` chhodne ka kya faayda?
6. Compiler `switch` ko kis-kis tareeke se implement kar sakta hai?
7. `switch` vs `if/else if` chain — decision kaise loge?

---

## Next
→ [`05-switch-vs-if.md`](05-switch-vs-if.md)

# 15 — Naming aur style

## Prerequisites
`02-what-is-a-variable.md`

## Yeh topic abhi kyun
Aap ab variables bana rahe ho. Aadat abhi se sahi daal lo — kyunki 6 mahine baad aapko
apna hi code padhna padega, aur 1 saal baad kisi team mein kaam karna padega.

**Code padha zyada jaata hai, likha kam.** Isliye readability sab kuch hai.

---

## Naming rules (compiler ke)

```cpp
int myVar;           // ✅ letters
int _private;        // ✅ underscore se shuru (par dhyaan se, neeche dekho)
int var123;          // ✅ digits (par shuru mein nahi)
int MAX_SIZE;        // ✅ caps

int 2fast;           // ❌ digit se shuru
int my-var;          // ❌ hyphen
int my var;          // ❌ space
int class;           // ❌ keyword
int __reserved;      // ⚠️ do underscores -- IMPLEMENTATION KE LIYE RESERVED
int _Global;         // ⚠️ underscore + capital -- RESERVED
```

### ⚠️ Reserved names (standard ke hisaab se)

Yeh naam **aap use nahi kar sakte** (technically UB hai):
- Kahin bhi `__` (do underscores) wale naam
- `_` + capital letter se shuru hone wale naam
- Global scope mein `_` se shuru hone wale naam

```cpp
int __count;         // ❌ reserved
int _Value;          // ❌ reserved
int _localVar;       // ⚠️ global scope mein reserved, function ke andar theek hai
int value_;          // ✅ trailing underscore SAFE hai (aur common convention hai)
```

---

## Naming conventions (teams ke)

Compiler ko farak nahi padta. Par teams ko padta hai.

### Popular styles

| Style | Example | Kahan use hota hai |
|---|---|---|
| `camelCase` | `orderCount` | Java, JavaScript, kuch C++ |
| `PascalCase` | `OrderCount` | Types/classes (zyadatar) |
| `snake_case` | `order_count` | **Standard library**, Google style |
| `SCREAMING_SNAKE` | `MAX_ORDERS` | Constants, macros |
| `m_prefix` | `m_orderCount` | Members (purana MFC style) |
| `trailing_` | `order_count_` | Members (Google style) |

### Common C++ convention (is course ka style)

```cpp
// Variables aur functions: camelCase ya snake_case
int orderCount = 0;
void processOrder();

// Types (class, struct, enum): PascalCase
class OrderBook { };
struct MarketData { };
enum class OrderSide { Buy, Sell };

// Constants: kPascalCase ya SCREAMING_SNAKE
constexpr int kMaxOrders = 1000;
constexpr int MAX_ORDERS = 1000;

// Class members: trailing underscore
class Order {
    std::int64_t price_;
    std::uint32_t quantity_;
};

// Macros: SCREAMING_SNAKE (aur macros se bacho)
#define DEBUG_MODE 1

// Namespaces: lowercase
namespace trading { namespace orderbook { } }
```

**Sabse important: CONSISTENT raho.** Team ka style follow karo. Naya project ho to
ek chuno aur usi pe rehna.

---

## Achhe naam kaise chunein

### Rule 1: Naam se pata chale ki kya hai

```cpp
// ❌ Bure naam
int d;                    // d kya hai?
int temp;                 // temporary kya?
int data;                 // kaunsa data?
int x1, x2, x3;           // kya hai yeh?
bool flag;                // kaunsa flag?

// ✅ Achhe naam
int daysElapsed;
int temporaryOrderId;
std::vector<Order> pendingOrders;
int bidPrice, askPrice, lastTradePrice;
bool isMarketOpen;
```

### Rule 2: Length scope ke hisaab se

```cpp
// Chhota scope -> chhota naam theek hai
for (int i = 0; i < n; ++i) { }              // ✅ i loop counter ke liye fine

// Bada scope -> descriptive naam
class OrderBook {
    std::map<std::int64_t, PriceLevel> bidLevels_;     // ✅ clear
    std::map<std::int64_t, PriceLevel> b_;             // ❌ 500 lines baad kya hai?
};
```

### Rule 3: Booleans ko question ki tarah naam do

```cpp
bool isValid;
bool hasError;
bool canExecute;
bool shouldRetry;
bool wasProcessed;

// ❌ Confusing
bool valid;         // "valid hai" ya "validate karo"?
bool status;        // kya status?
bool flag;          // kaunsa flag?
```

### Rule 4: Units naam mein daalo

```cpp
// ❌ Ambiguous -- disasters ka source
int timeout;               // seconds? milliseconds? nanoseconds?
double price;              // rupees? paise? ticks?
int size;                  // bytes? elements?

// ✅ Clear
int timeoutMs;
int timeoutNanos;
std::int64_t priceInTicks;
std::int64_t priceInPaise;
std::size_t sizeInBytes;
std::size_t elementCount;
```

> **Real disaster:** NASA ka Mars Climate Orbiter 1999 mein crash ho gaya kyunki
> ek team ne pounds-force use kiya aur doosri ne newtons. **$327 million** ka nuksaan.
> Units naam mein likh dete to bach jaata.

### Rule 5: Negatives se bacho

```cpp
// ❌ Double negative confusing hai
bool isNotReady;
if (!isNotReady) { }        // "not not ready" -- dimaag ghoom jaata hai

// ✅ Positive
bool isReady;
if (!isReady) { }           // clear
```

### Rule 6: Abbreviations se bacho (jab tak universal na hon)

```cpp
// ❌
int nOrdCnt;
int qty;              // ⚠️ borderline -- finance mein qty universal hai
int mkt;

// ✅
int orderCount;
int quantity;         // ya `qty` agar team convention ho
int market;

// ✅ Universal abbreviations theek hain
int id;               // identifier
std::string url;
int max, min;
int i, j, k;          // loop counters
```

---

## Magic numbers hatao

```cpp
// ❌ Magic numbers
if (orderCount > 1000) { }
double fee = price * 0.0025;
char buffer[256];
if (status == 3) { }

// ✅ Named constants
constexpr int kMaxPendingOrders = 1000;
constexpr double kExchangeFeeRate = 0.0025;
constexpr std::size_t kBufferSize = 256;

if (orderCount > kMaxPendingOrders) { }
double fee = price * kExchangeFeeRate;
char buffer[kBufferSize];
if (status == OrderStatus::Filled) { }      // enum, aur bhi better
```

**Fayde:**
- Padhne mein clear
- Ek jagah badlo, sab jagah badal jaata hai
- Search karna aasan
- Typo se bache (`1000` vs `10000` — silent bug)

**Exceptions (yeh magic numbers theek hain):**
```cpp
if (x == 0) { }
for (int i = 0; i < n; ++i) { }
size / 2
```

---

## Formatting

### Consistent indentation
```cpp
// ✅
int main() {
    if (condition) {
        doSomething();
    }
}
```

**4 spaces** ya **2 spaces** — dono theek. Tabs bhi. **Bas consistent raho.**

### Spacing
```cpp
// ✅ Readable
int result = (a + b) * c;
for (int i = 0; i < n; ++i) {
    if (arr[i] > threshold) {
        process(arr[i], i);
    }
}

// ❌ Bakwaas
int result=(a+b)*c;
for(int i=0;i<n;++i){if(arr[i]>threshold){process(arr[i],i);}}
```

### Line length
80 ya 100 ya 120 columns — team decide karti hai. 120 se zyada mat jao.

```cpp
// ❌ Bahut lambi
processOrder(orderId, price, quantity, side, timestamp, exchangeId, accountId, strategy);

// ✅ Todi hui
processOrder(orderId, price, quantity, side,
             timestamp, exchangeId, accountId, strategy);
```

### Ek line, ek declaration
```cpp
// ❌
int a = 1, b = 2, c = 3;
int* p1, p2;              // ⚠️ p2 pointer NAHI hai!

// ✅
int a = 1;
int b = 2;
int c = 3;
int* p1;
int* p2;
```

---

## Automated formatting: `clang-format`

Manually format karna waste of time hai. Tool use karo.

```bash
# Install
sudo apt install clang-format        # Ubuntu/Debian
brew install clang-format            # macOS

# Ek file format karo
clang-format -i myfile.cpp

# Poore project ko
find . -name "*.cpp" -o -name "*.h" | xargs clang-format -i
```

### `.clang-format` config file

Project ke root mein:
```yaml
---
BasedOnStyle: Google
IndentWidth: 4
ColumnLimit: 100
PointerAlignment: Left
AccessModifierOffset: -4
AllowShortFunctionsOnASingleLine: Inline
```

Ya ready-made styles: `LLVM`, `Google`, `Chromium`, `Mozilla`, `WebKit`, `Microsoft`

**VS Code mein:** `Ctrl+Shift+I` se format ho jaata hai (C++ extension ke saath).

---

## Declare karo jahan use karo

```cpp
// ❌ C89 style -- sab upar declare
int main() {
    int i, j, sum, count;
    double average;
    // ... 50 lines ...
    sum = 0;                  // sum ab kya tha? scroll karo upar
}

// ✅ Modern style -- jahan zarurat wahan declare
int main() {
    // ... 50 lines ...
    int sum = 0;              // ✅ yahin declare aur initialize
    for (int i = 0; i < n; ++i) {     // ✅ i ka scope sirf loop mein
        sum += arr[i];
    }
}
```

**Fayde:**
- Scope chhota → bugs kam
- Declare karte hi initialize kar sakte ho
- Padhne mein clear

---

## HFT codebases ka style (note)

HFT codebases mein aksar:
- **Aggressive naming** for units: `priceTicks`, `qtyLots`, `latencyNs`
- **Hungarian-ish prefixes** kabhi kabhi type ke liye
- **`const` correctness** strictly enforced
- **`-Werror`** hamesha
- **Comments** performance assumptions batate hain:
  ```cpp
  // HOT PATH: no allocation, no syscalls, no locks
  // Measured p99: 180ns
  void onMarketData(const Message& msg) noexcept;
  ```

---

## Hands-on

```bash
cd ~/cpp-practice
cat > badstyle.cpp << 'END'
#include <iostream>
int main(){int a=100;int b=5;int c=a*b;if(c>400){std::cout<<"big"<<"\n";}else{std::cout<<"small"<<"\n";}double f=c*0.0025;std::cout<<f<<"\n";return 0;}
END

# Format karo
clang-format -i --style="{BasedOnStyle: Google, IndentWidth: 4}" badstyle.cpp 2>/dev/null \
  && cat badstyle.cpp \
  || echo "clang-format installed nahi hai -- install karo"
```

Ab manually improve karo — naam, constants, spacing.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Naming style compiler ko matter karta hai" | Nahi — sirf insaanon ko |
| "Chhote naam tez code banate hain" | Zero difference. Compiler naam use nahi karta |
| "`__name` use kar sakte hain" | Reserved hai — technically UB |
| "Style ki fikr baad mein karenge" | Aadat abhi banti hai. Baad mein badalna mushkil |

---

## Exercises

1. In naamon ko improve karo:
   ```cpp
   int d;
   double p;
   bool f;
   int t;
   std::vector<int> v;
   int n1, n2;
   ```
   <details><summary>Sample answers</summary>

   ```cpp
   int daysUntilExpiry;
   std::int64_t priceInTicks;
   bool isMarketOpen;
   std::int64_t timestampNanos;
   std::vector<std::int64_t> executedPrices;
   int bidCount, askCount;
   ```
   </details>

2. Magic numbers hatao:
   ```cpp
   if (orders.size() > 500) { }
   double commission = value * 0.001;
   char buffer[1024];
   if (state == 2) { }
   ```

3. Yeh code format karo (haath se, phir `clang-format` se):
   ```cpp
   int main(){int x=5;if(x>0){std::cout<<"positive";}return 0;}
   ```

4. Units bug dhoondho:
   ```cpp
   void setTimeout(int timeout);
   setTimeout(5);          // 5 kya? seconds? ms?
   ```
   Fix karo.
   <details><summary>Answer</summary>

   ```cpp
   void setTimeoutMs(int timeoutMs);
   setTimeoutMs(5000);

   // Ya aur bhi better -- chrono use karo (folder 19/35)
   void setTimeout(std::chrono::milliseconds timeout);
   setTimeout(std::chrono::seconds{5});      // type se hi clear
   ```
   </details>

5. `.clang-format` file banao apne practice folder mein aur use karo.

6. Apna ek purana program uthao aur naming/style improve karo. Fark note karo.

---

## Interview questions

1. Reserved identifiers kaunse hote hain C++ mein?
2. Magic numbers kyun bure hain?
3. Variable ko kahan declare karna chahiye?
4. `int* p1, p2;` mein kya problem hai?

---

## Next
→ [`16-exercises.md`](16-exercises.md)

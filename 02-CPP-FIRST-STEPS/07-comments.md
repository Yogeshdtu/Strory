# 07 — Comments

## Prerequisites
`02-anatomy-line-by-line.md`

## Yeh topic abhi kyun
Comments ke bina aapka code 6 mahine baad aapko hi samajh nahi aayega. Aur is course
ke saare examples Hinglish comments se bhare hain — to unka syntax jaan lo.

---

## Do types

### 1. Single-line comment: `//`

```cpp
// yeh poori line comment hai
int x = 5;    // yeh line ke baaki hisse ko comment karta hai
```

`//` se line ke **end tak** sab kuch ignore ho jaata hai.

### 2. Multi-line comment: `/* */`

```cpp
/* yeh comment
   kai lines mein
   fail sakta hai */

int x = /* beech mein bhi */ 5;
```

---

## Compiler ko comments dikhte hi nahi

**Preprocessor comments ko hata deta hai**, compiler tak pahunchne se pehle.

```bash
cat > c.cpp << 'END'
// yeh comment gayab ho jayega
int main() {
    /* yeh bhi */
    return 0;
}
END
g++ -E c.cpp | tail -6
```

Output mein koi comment nahi milega. Isliye:
- Comments se **zero performance cost** hai
- Comments final executable mein nahi jaate

---

## ⚠️ `/* */` nest nahi hota

```cpp
/* bahar
   /* andar */
   yeh line ab code hai!  <- ERROR
*/
```

Pehla `*/` poore comment ko band kar deta hai. Baaki text code ban jaata hai.

**Solution:** Bade blocks ko comment karne ke liye `//` use karo (editor mein
`Ctrl+/` se ek saath ho jaata hai), ya `#if 0`:

```cpp
#if 0
    yeh sab kuch
    /* including comments */
    ignore ho jayega
#endif
```

---

## Achhe comments vs bure comments

### ❌ BURE comments — jo code hi dohraate hain

```cpp
int x = 5;              // x ko 5 set karo          <- bekaar
i++;                    // i ko badhao              <- bekaar
count = count + 1;      // count mein 1 jodo        <- bekaar
if (age > 18) {         // agar age 18 se badi hai  <- bekaar
```

Yeh comments **kuch nahi bata rahe**. Code khud yeh bata raha hai.

### ✅ ACHHE comments — jo "KYUN" batate hain

```cpp
// Price ko integer paise mein rakhte hain, double mein nahi,
// kyunki floating point mein 0.1 + 0.2 != 0.3 hota hai.
// Financial calculations mein yeh disaster hai.
int64_t priceInPaise = 10050;

// Exchange ka spec kehta hai ki sequence number 0 se shuru hota hai,
// lekin practice mein woh 1 se bhejta hai. Isliye adjustment.
uint32_t adjustedSeq = rawSeq - 1;

// Yeh loop reverse mein chalta hai kyunki hum elements delete kar rahe hain --
// forward mein chalne se indices shift ho jaate.
for (int i = size - 1; i >= 0; --i) {
    // ...
}

// HOT PATH: yahan koi allocation nahi honi chahiye.
// Har allocation ~100ns joddegi aur p99 latency kharab karegi.
void processMarketData(const Message& msg) noexcept {
    // ...
}
```

**Golden rule:**
> **Code batata hai KYA ho raha hai. Comment batata hai KYUN.**

---

## Comment ki zarurat kab nahi hoti?

Aksar comment ki zarurat isliye padti hai kyunki **code hi confusing hai**.

```cpp
// ❌ Comment se problem chhupa rahe hain
int d = 86400;    // seconds in a day

// ✅ Code ko hi clear kar do
constexpr int SECONDS_PER_DAY = 86400;
```

```cpp
// ❌
if (o.s == 2 && o.q > 0) { ... }    // agar order active hai aur quantity bachi hai

// ✅
if (order.status == OrderStatus::Active && order.remainingQty > 0) { ... }
```

**Achha naming > achhe comments.**

---

## Documentation comments (Doxygen style)

Bade projects mein functions ke upar structured comments likhte hain:

```cpp
/**
 * @brief Order book mein naya order daalta hai.
 *
 * Yeh function price-time priority maintain karta hai. Same price pe
 * naya order queue ke end mein jaata hai.
 *
 * @param orderId   Unique order identifier
 * @param price     Price in ticks (integer, paise nahi)
 * @param quantity  Order quantity (positive hona chahiye)
 * @return true agar successfully add hua, false agar validation fail
 *
 * @note Yeh function hot path mein hai -- koi allocation nahi karta
 * @warning Thread-safe NAHI hai. Single-writer se hi call karna
 */
bool addOrder(uint64_t orderId, int64_t price, uint32_t quantity);
```

Doxygen jaise tools isse automatic HTML documentation bana dete hain.

---

## Comments se code disable karna

Debugging ke waqt bahut kaam aata hai:

```cpp
int main() {
    doThis();
    // doThat();        <- temporarily band
    doSomethingElse();
}
```

**VS Code shortcut:** `Ctrl+/` — selected lines ko comment/uncomment kar deta hai.

⚠️ **Lekin:** Commented-out code ko permanently mat chhodo. Git use karo — purana
code history mein safe hai. Dead commented code sirf confusion banata hai.

---

## Special comment markers (conventions)

```cpp
// TODO: yahan error handling add karni hai
// FIXME: yeh edge case galat handle ho raha hai
// HACK: temporary fix, proper solution baad mein
// NOTE: dhyaan dena -- yeh assumption pe based hai
// XXX: yeh dangerous hai
// PERF: yeh optimize ho sakta hai
```

Zyada tar IDEs in markers ko highlight karte hain aur ek list bana dete hain.

```bash
grep -rn "TODO\|FIXME" .        # saare TODOs dhoondho
```

---

## Is course ka comment style

Is course mein har example aise comment kiya gaya hai:

```cpp
int age = 20;
// `age` naam ka integer variable banaya.
// `int` batata hai ki hum integer value rakh rahe hain.
// `=` yahan assignment ke liye use ho raha hai.
// Ab `age` ke andar 20 stored hai.
```

Aur advanced code mein:

```cpp
// Kya: order ko book mein daala
// Kyun: price-time priority maintain karne ke liye same price pe end mein daalte hain
// Memory: koi allocation nahi -- pre-allocated pool se node liya
// Performance: O(1) amortized, ~15ns typical
// HFT: yeh hot path hai, isme kabhi allocate mat karna
```

---

## Hands-on

```bash
cd ~/cpp-practice
cat > comments.cpp << 'END'
#include <iostream>

/*
 * ============================================================
 *  File header comment -- yeh batata hai file ka kya kaam hai
 *  Author: Aapka naam
 *  Purpose: comment styles demo
 * ============================================================
 */

// Single line comment

/* Multi-line
   comment */

int main() {
    int x = 5;    // inline comment

    /* Kai statements ko comment karne ke liye */
    // std::cout << "yeh chalega nahi\n";

    std::cout << "x = " << x << "\n";

    // TODO: yahan input validation add karo
    // FIXME: negative values handle nahi ho rahe

    /* Nested comment ki koshish (yeh TOOT jayega):
       /* andar wala */
       yahan tak aate aate error aa jayega
    */

    return 0;
}
END
g++ -std=c++20 comments.cpp -o comments
```

Error aayega nested comment ki wajah se. Us block ko hata do, phir chalega.

Ab preprocessor output dekho:
```bash
# nested comment wala hissa hata do pehle
g++ -E comments.cpp | tail -15
```
Koi comment nahi milega — sab gayab.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Zyada comments = achha code" | Zyada *zaroori* comments. Bekaar comments noise hain |
| "Comments se program slow hota hai" | Zero cost. Compiler ko dikhte hi nahi |
| "`/* */` nest ho sakta hai" | ❌ Nahi hota |
| "Har line pe comment likhna chahiye" | Nahi. Sirf jahan "kyun" batana ho |
| "Purana code comment karke rakh lo" | Git use karo. Dead code delete karo |

---

## Exercises

1. `comments.cpp` chalao. Nested comment ka error dekho. Fix karo.

2. Preprocessor output mein comments dhoondho. Mile?

3. In bure comments ko achha banao:
   ```cpp
   int t = 3600;                    // t ko 3600 set karo
   v.push_back(x);                  // v mein x daalo
   if (s == 1) { }                  // agar s 1 hai
   for (int i = 0; i < n; i++) { }  // 0 se n tak loop
   ```
   <details><summary>Sample answers</summary>

   ```cpp
   constexpr int SECONDS_PER_HOUR = 3600;   // comment ki zarurat hi nahi
   
   activeOrders.push_back(newOrder);        // naming se clear
   
   if (status == OrderStatus::Filled) { }   // enum se clear
   
   // Reverse iteration nahi kar sakte kyunki callback order matter karta hai
   for (int i = 0; i < orderCount; ++i) { }
   ```
   </details>

4. Ek function likho aur uske upar Doxygen-style documentation comment likho.

5. Apne ek purane code mein `TODO:` aur `FIXME:` daalo, phir `grep` se dhoondho.

---

## Next
→ [`08-string-literals-and-escapes.md`](08-string-literals-and-escapes.md)

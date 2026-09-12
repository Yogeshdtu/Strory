# 03 — Nested conditions, guard clauses, early return

## Prerequisites
- [`01-if-statement.md`](01-if-statement.md), [`02-else-and-else-if.md`](02-else-and-else-if.md)
- `05-OPERATORS/04-logical-operators.md` (`&&`, `||`, De Morgan)

## Yeh topic abhi kyun
Real code mein conditions ek doosre ke andar ghus jaate hain — "agar logged in
hai, **aur** token valid hai, **aur** admin hai, tab...". Naive tareeke se likho
to code daahine ki taraf bhaagne lagta hai ("arrow / pyramid of doom") aur padhna
mushkil ho jaata hai.

Yeh lesson do cheezein sikhata hai jo isse theek karti hain: **flatten karna**
(`&&`, early `return`) aur **guard clauses**. Yeh functions (folder 08) aur error
handling (23) ka core style hai.

---

## Nesting kya hai

Ek `if` ke body mein doosra `if`:

```cpp
if (loggedIn) {
    if (hasToken) {
        if (isAdmin) {
            grantFullAccess();
        }
    }
}
```

Har level ek indentation add karta hai. 3–4 level ke baad:
- Har line padhne se pehle upar ki saari conditions yaad rakhni padti hain
- `else` kahan lagega — confuse
- Modify karna risky

Isko "**arrow code**" ya "**pyramid of doom**" kehte hain.

```
if (...) {
    if (...) {
        if (...) {
            if (...) {
                do the thing        <-- asli kaam yahan, 4 level andar
            }
        }
    }
}
```

---

## Fix 1 — conditions ko `&&` se merge karo

Agar beech mein koi `else` / extra code nahi hai:

```cpp
// ❌ 3 level nesting
if (loggedIn) {
    if (hasToken) {
        if (isAdmin) {
            grantFullAccess();
        }
    }
}

// ✅ ek condition
if (loggedIn && hasToken && isAdmin) {
    grantFullAccess();
}
```

`&&` short-circuit karta hai (folder 05 file 04) — `loggedIn` false hua to baaki
check hote hi nahi. Behaviour same, nesting gaya.

⚠️ Yeh tabhi kaam karta hai jab har level ka **sirf** yehi kaam ho. Agar kisi level
pe `else` ya alag logic hai, to merge nahi kar sakte — Fix 2 dekho.

---

## Fix 2 — GUARD CLAUSE (early return)

**Idea:** "galat / edge cases pehle nikaal do (`return`/`break`/`continue`), phir
main logic bina nesting ke likho."

```cpp
// ❌ Nested -- "happy path" sabse andar
std::string_view access(bool loggedIn, bool hasToken, bool isAdmin) {
    if (loggedIn) {
        if (hasToken) {
            if (isAdmin) {
                return "full";
            } else {
                return "user";
            }
        } else {
            return "no-token";
        }
    } else {
        return "guest";
    }
}

// ✅ Guard clauses -- edge cases pehle, happy path flat aur aakhri mein
std::string_view access(bool loggedIn, bool hasToken, bool isAdmin) {
    if (!loggedIn)  return "guest";      // guard
    if (!hasToken)  return "no-token";   // guard
    if (isAdmin)    return "full";
    return "user";                       // happy path -- 0 nesting
}
```

Dono **bilkul same** result dete hain (`examples/01_if_else.cpp` mein
`classifyNested` vs `classifyGuard` — verify karo). Par guard version:
- Har line ka matlab akele saaf hai
- Naya edge case add karna = ek line upar jodo
- Koi deep `else` nahi

### Guard clause ka shape

```
FUNCTION:
    agar (input galat)      -> return / throw error
    agar (edge case 1)      -> return early
    agar (edge case 2)      -> return early
    ... ab hum jaante hain input achha hai ...
    <asli kaam, flat>
    return result
```

Loops mein `continue` isi ka version hai:

```cpp
for (const auto& order : orders) {
    if (order.cancelled)    continue;   // guard
    if (order.qty == 0)     continue;   // guard
    process(order);                     // flat
}
```

---

## Kab nesting theek hai

Guard clause zabardasti mat karo. Nesting theek hai jab:

1. **Do genuinely alag branches** hon, dono mein kaam ho:
   ```cpp
   if (isBuy) {
       book.addBid(order);
       updateBestBid();
   } else {
       book.addAsk(order);
       updateBestAsk();
   }
   ```
2. **1–2 level** ho aur padhne mein saaf ho
3. Function chhota ho

**Red flag:** 3+ level, ya `if (x) { ...50 lines... } else { ...50 lines... }`.
Tab: guard clauses, ya alag function nikaalo (folder 08).

---

## `&&` / `||` vs nesting — kaunsa kab

| Situation | Tareeka |
|---|---|
| Sab conditions ek hi outcome ke liye | `if (a && b && c)` |
| Koi ek bhi true → same outcome | `if (a \|\| b \|\| c)` |
| Har level pe alag `else` / alag kaam | nested, ya guard clauses |
| Bahut saari edge cases, ek happy path | guard clauses (early return) |
| Complex boolean, samajhna mushkil | naam do: `bool canTrade = a && b && !c; if (canTrade)` |

### Named conditions — readability

```cpp
// ⚠️ Padhne mein mushkil
if (acc.balance >= order.notional && !acc.frozen && mkt.isOpen() && order.qty > 0) {
    ...
}

// ✅ Intent explicit
const bool hasFunds   = acc.balance >= order.notional;
const bool accountOk   = !acc.frozen;
const bool marketOpen  = mkt.isOpen();
const bool validQty    = order.qty > 0;

if (hasFunds && accountOk && marketOpen && validQty) {
    ...
}
```

Compiler `-O2` pe yeh temporaries free kar deta hai (short-circuit bhi rehta hai) —
readability free milti hai.

---

## De Morgan — nested negatives simplify

```cpp
// ⚠️ Double negative, samajhna mushkil
if (!(!isReady || !hasData)) {
    process();
}

// ✅ De Morgan: !(!a || !b) == (a && b)
if (isReady && hasData) {
    process();
}
```

Guard clause banate waqt aksar condition ulti karni padti hai — De Morgan yaad rakho
(folder 05 file 04):

```
!(a && b)  ==  !a || !b
!(a || b)  ==  !a && !b
```

```cpp
// "process tabhi jab a && b" -> guard: "return jab NOT (a && b)"
if (!(a && b)) return;      // == if (!a || !b) return;
```

---

## Andar kya hota hai

Nesting compiler ke liye bas aur zyada compare + jump hai — koi extra runtime cost
"nesting hone ki wajah se" nahi. `if (a && b && c)` aur teen nested `if` **same
assembly** dete hain `-O2` pe (short-circuit dono mein).

Farq **insaan ke liye** hai: bug likhne aur padhne ka. Aur guard-clause style
branch predictor ke liye thoda behtar bhi hota hai — "common case = fall through,
edge case = jump" (file 08).

> **HFT relevance:** Hot-path functions (packet decode, order validate, book
> update) guard-clause style mein likhe jaate hain: sasti + selective checks pehle,
> `return` jaldi. Isse (1) zyada messages jaldi reject, (2) common path straight-
> line rehta hai (I-cache aur branch predictor dono khush), (3) code review mein
> har guard alag se dikhta hai. Deep dive folders 33, 36.

---

## Hands-on

`examples/01_if_else.cpp` mein `classifyNested()` aur `classifyGuard()` — dono ka
output same hai. Code padho, chalao, aur khud teesra edge case (`age > 120` →
`"invalid"`) dono versions mein add karo. Kaunsa add karna aasan tha?

```bash
./build.ps1 06-CONDITIONS/examples/01_if_else.cpp
```

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `&&` merge jahan `else` chhupa hai
```cpp
// Original -- token na hone pe kuch aur karta hai
if (loggedIn) {
    if (hasToken) grant();
    else logMissingToken();
}
// ❌ Ise `if (loggedIn && hasToken)` mein merge nahi kar sakte -- else gayab ho jaayega
```

### Trap 2 — Guard clause mein condition ulti karna bhool gaye
```cpp
// "process jab valid" -> guard likhte waqt:
if (isValid) return;    // ❌ ULTA -- valid hone pe return kar diya!
if (!isValid) return;   // ✅
```

### Trap 3 — Multiple returns ko "bura" samajhna
Purana C-era rule "single return per function" modern C++ mein zaroori nahi. Guard
clauses ke multiple early returns **zyada** readable hote hain. (RAII/destructors
cleanup khud kar dete hain — folder 17.)

### Trap 4 — Har chhoti nesting ko guard-clause mein todna
2-line `if/else` ko 4 guards mein todna bhi over-engineering hai. Judgement.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Nested `if` slow hota hai" | Same assembly as `&&` — farq readability ka |
| "`if (a && b)` aur nested `if` alag behave karte hain" | Same (dono short-circuit) |
| "Ek function mein ek hi `return` hona chahiye" | Guard clauses = multiple early returns, better |
| "Guard clause = condition jaisi hai waisi likho" | Aksar **ulti** karni padti hai (`!`) |
| "Deep nesting normal hai" | 3+ level = refactor signal |

---

## Exercises

1. **Flatten with `&&`:**
   ```cpp
   if (order != nullptr) {
       if (order->qty > 0) {
           if (order->price > 0) {
               submit(order);
           }
       }
   }
   ```
   <details><summary>Answer</summary>

   ```cpp
   if (order != nullptr && order->qty > 0 && order->price > 0) {
       submit(order);
   }
   ```
   Order matter karta hai — `order != nullptr` pehle (short-circuit se null-deref
   se bachta hai).
   </details>

2. **Guard clauses banao:**
   ```cpp
   HttpResponse handle(const Request& req) {
       if (req.isValid()) {
           if (req.isAuthenticated()) {
               if (req.hasPermission()) {
                   return process(req);
               } else {
                   return forbidden();
               }
           } else {
               return unauthorized();
           }
       } else {
           return badRequest();
       }
   }
   ```
   <details><summary>Answer</summary>

   ```cpp
   HttpResponse handle(const Request& req) {
       if (!req.isValid())          return badRequest();
       if (!req.isAuthenticated())  return unauthorized();
       if (!req.hasPermission())    return forbidden();
       return process(req);
   }
   ```
   </details>

3. **De Morgan:** in conditions ko `!` andar leke simplify karo —
   - `!(a && b && c)`
   - `!(x > 0 || y > 0)`
   - `!(isReady && !isPaused)`
   <details><summary>Answer</summary>
   `!a || !b || !c` · `x <= 0 && y <= 0` · `!isReady || isPaused`
   </details>

4. **`continue` guard:** yeh loop guard-clause style mein likho —
   ```cpp
   for (const auto& t : trades) {
       if (!t.cancelled) {
           if (t.qty > 0) {
               total += t.qty * t.price;
           }
       }
   }
   ```

5. **Named conditions:** is condition ko 4 named `bool`s mein todo —
   ```cpp
   if (sym.tradable && !halted[sym.id] && now < sessionEnd && risk.ok(sym)) { ... }
   ```

6. **Judgement:** kaunsa refactor karna chahiye, kaunsa chhodo?
   - (a) 4-level nested validation, har level ek `return`
   - (b) `if (isBuy) { addBid(); } else { addAsk(); }`
   - (c) 3-level nesting jisme sabse andar 40 lines ka calculation
   <details><summary>Answer</summary>
   (a) → guard clauses. (b) theek hai, chhodo. (c) → andar wala 40-line block alag
   function mein nikaalo, phir guard clauses.
   </details>

---

## Interview questions

1. "Pyramid of doom" / arrow code kya hai? Do tareeke isse theek karne ke.
2. Guard clause kya hai? Ek nested function ko guard-style mein convert karke dikhao.
3. `if (a && b)` aur nested `if (a) { if (b) }` — behaviour aur assembly mein farq?
4. "Single return per function" rule modern C++ mein kyun zaroori nahi?
5. Guard clause likhte waqt condition aksar ulti kyun karni padti hai? De Morgan ka role?
6. Guard-clause style branch predictor / I-cache ke liye kyun thoda behtar hai?

---

## Next
→ [`04-switch-statement.md`](04-switch-statement.md)

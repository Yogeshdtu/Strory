# 01 — `while` loop

## Prerequisites
- `06-CONDITIONS/01-if-statement.md` (condition = `bool` expression, truthiness)
- `06-CONDITIONS/07-conditional-bugs.md` (`=` vs `==`, stray `;`)
- `05-OPERATORS/02-increment-decrement.md` (`++`, `--`)

## Yeh topic abhi kyun
Ab tak program **ek baar** har line chalata tha, aur `if` se **faisla** leta tha.
Loops chautha building block hain — **repetition**. Ek hi kaam ko baar-baar, bina
copy-paste ke.

`while` sabse seedha loop hai: **"jab tak yeh sach hai, yeh block chalate raho."**
`for` aur range-`for` (files 03, 04) andar se `while` hi hain.

---

## Anatomy

```cpp
while (condition) {
    // body -- chalta hai jab tak condition true rahe
}
```

| Part | Kya | Rule |
|---|---|---|
| `while` | keyword | lowercase |
| `( condition )` | `bool` expression | har iteration se **pehle** check hoti hai |
| `{ body }` | statements | is course mein hamesha braces |

### Execution — step by step

```cpp
int n = 3;
while (n > 0) {
    std::cout << n << " ";
    --n;
}
std::cout << "done";
```

```
   n = 3 :  condition (3 > 0) true   -> body: print "3 ", n becomes 2
   n = 2 :  condition (2 > 0) true   -> body: print "2 ", n becomes 1
   n = 1 :  condition (1 > 0) true   -> body: print "1 ", n becomes 0
   n = 0 :  condition (0 > 0) FALSE  -> loop khatam
   print "done"

   Output:  3 2 1 done
```

**Check pehle, body baad mein.** Agar condition shuru mein hi false ho, body
**zero baar** chalti hai:

```cpp
int m = 0;
while (m > 0) {          // 0 > 0 false
    std::cout << "kabhi nahi";
}
```

---

## `while` ke 3 zaroori hisse (bhale hi syntax mein na dikhein)

Har sahi `while` loop mein yeh teen cheezein honi chahiye:

```cpp
int i = 0;                 // 1. INITIALIZATION -- loop se pehle
while (i < 5) {             // 2. CONDITION -- kab rukna hai
    std::cout << i << " ";
    ++i;                   // 3. UPDATE -- har iteration progress
}
```

**Update bhoolna = infinite loop.** `for` loop teeno ko ek line mein rakhta hai
isi liye — yeh galti kam hoti hai (file 03).

---

## Infinite loop — kabhi galti, kabhi jaan-boojh kar

### Galti se
```cpp
int i = 0;
while (i < 5) {
    std::cout << i;        // ⚠️ ++i bhool gaye -> i hamesha 0 -> forever
}
```

### Jaan-boojh kar (event loops, servers)
```cpp
while (true) {
    Message msg = receive();     // block hota hai jab tak message na aaye
    if (msg.type == QUIT) break; // <- nikalne ka raasta
    handle(msg);
}
```

`while (true)` valid aur common hai — par andar **`break`**, `return`, ya
`exit()` ka raasta hona chahiye. Trading systems, game loops, REPLs, OS
schedulers — sab aise chalte hain.

---

## ⚠️ Traps

### Trap 1 — stray semicolon
```cpp
while (i < 5);           // ⚠️ `;` = khali body -> i kabhi nahi badhta -> forever
{
    ++i;                // yeh block loop se bahar hai
}
```
`-Wall` → `-Wempty-body`. (Folder 06 file 07 se yaad hai.)

### Trap 2 — `=` ke bajaye `==`... ya intentional `=`
```cpp
while (n = getNext()) { }     // ⚠️ ya bug hai (== chahiye tha) ya jaan-boojh kar
while ((n = getNext()) != 0) { }   // ✅ intent saaf -- extra ( ) aur explicit compare
```

### Trap 3 — condition ka side effect har baar chalta hai
```cpp
while (expensiveCheck()) { }   // har iteration mein poora chalega -- hot loop mein soch-samajh ke
```

### Trap 4 — update galat jagah / galat direction
```cpp
int i = 10;
while (i > 0) {
    process(i);
    ++i;                // ⚠️ i badh raha hai -> i > 0 kabhi false nahi (overflow tak = UB)
}
```

### Trap 5 — floating-point loop variable
```cpp
double x = 0.0;
while (x != 1.0) { x += 0.1; }   // ⚠️ 0.1 exact nahi -> x kabhi thik 1.0 nahi -> forever
```
Integer counter use karo: `for (int k = 0; k < 10; ++k) { double x = k * 0.1; }`

---

## Andar kya hota hai

`while` = ek conditional backward jump.

```cpp
while (n > 0) {
    body();
    --n;
}
```

```asm
.loop:
        cmp     dword ptr [n], 0
        jle     .done            ; condition false -> bahar
        call    body
        dec     dword ptr [n]
        jmp     .loop            ; wapas upar -- BACKWARD jump
.done:
```

- Loop ka branch (`jle`) aksar **highly predictable** hota hai — 999 baar "andar
  raho", 1 baar "bahar". Branch predictor ~100% sahi (folder 06 file 08). Isliye
  loop ka control overhead lagbhag ~0 hota hai.
- `-O2` pe compiler loop ko unroll/vectorize kar sakta hai (file 09).

> **HFT relevance:** Har HFT process ke dil mein ek `while (running)` / `for (;;)`
> **busy-spin loop** hota hai jo network card se naya data poll karta hai — bina
> `sleep`, bina blocking syscall, taaki latency microseconds mein rahe. Yeh loop
> ek dedicated CPU core pe pinned hota hai. Detail folders 36, 41.

---

## Hands-on

`examples/01_loop_types.cpp` — `while`, `do-while`, `for` side by side:

```bash
./build.ps1 07-LOOPS/examples/01_loop_types.cpp
# ya: make FILE=07-LOOPS/examples/01_loop_types.cpp
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`while` body kam se kam ek baar chalti hai" | Nahi — condition pehle. 0 baar bhi ho sakta hai (`do-while` = min 1) |
| "`while (true)` hamesha galat hai" | Common aur valid — bas `break`/`return` ka raasta chahiye |
| "Update kahin bhi likh sakte ho" | Progress na ho to infinite loop |
| "`while (x);` kuch nahi karta" | Woh `;` poora loop body hai (empty) → aksar infinite |
| "Loop ka branch mehnga hai" | Predictable → predictor ~100% → ~free |

---

## Exercises

1. **Output predict karo:**
   ```cpp
   int i = 1;
   while (i < 100) { std::cout << i << " "; i *= 3; }
   ```
   <details><summary>Answer</summary>`1 3 9 27 81` — `i` 3x hota hai; `243 < 100` false.</details>

2. **0 iterations:** ek `while` likho jiski body kabhi na chale. Phir usko
   `do-while` bana do — ab kitni baar chali?

3. **Sum 1..N:** `while` se `1 + 2 + ... + 100` nikaalo. Answer `5050`.

4. **Digit count:** ek number `n` ke digits gino (`while (n > 0) { ++count; n /= 10; }`).
   `n = 0` pe kya aaya? (Bug: `0` ke liye `0` digits. Fix?)

5. **Infinite loop banao aur fix karo:**
   ```cpp
   int x = 10;
   while (x > 0) { std::cout << x << "\n"; }
   ```
   Chalao (`Ctrl+C` se roko), phir fix karo.

6. **`while` → `for`:** exercise 1 ko `for` loop mein badlo. Kaunsa zyada compact?

7. **Collatz:** `n` se shuru — agar even to `n /= 2`, odd to `n = 3*n + 1` — jab
   tak `n == 1` na ho. Steps gino. `n = 27` pe kitne steps? (111)

---

## Interview questions

1. `while` aur `do-while` mein fundamental fark?
2. Ek sahi `while` loop ke 3 zaroori elements?
3. `while (true)` kab justified hai?
4. `while (x != 1.0) x += 0.1;` mein kya bug?
5. Loop ka branch predictor ke liye kaisa hota hai — mehnga ya sasta? Kyun?
6. `while (n = f())` — bug ya feature? Kaise clear karein?

---

## Next
→ [`02-do-while-loop.md`](02-do-while-loop.md)

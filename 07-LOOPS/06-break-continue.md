# 06 — `break`, `continue`, aur `goto` kyun nahi

## Prerequisites
- [`03-for-loop.md`](03-for-loop.md), [`05-nested-loops.md`](05-nested-loops.md)
- `06-CONDITIONS/03-nested-conditions.md` (guard clauses)
- `06-CONDITIONS/04-switch-statement.md` (`break` switch mein)

## Yeh topic abhi kyun
Kabhi loop ko **beech mein rokna** hota hai (mil gaya jo dhoond rahe the), kabhi
ek iteration **skip** karni hoti hai (yeh element interesting nahi). `break` aur
`continue` yahi karte hain. Sahi use se loops saaf hote hain; galat use se
infinite loops aur "sirf ek level toota" wale bugs.

Aur `goto` — C++ mein hai, par 99% cases mein galat choice. Kyun, yeh bhi samjhenge.

---

## `break` — loop se turant bahar

```cpp
for (int i = 0; i < n; ++i) {
    if (a[i] == target) {
        found = i;
        break;              // loop KHATAM -- iteration-expression bhi skip
    }
}
// yahan aa jaate hain
```

- Loop **poora** khatam — condition dobara check nahi hoti
- `for` ka iteration-expression (`++i`) bhi skip
- Nested loops mein: **sirf apna (innermost)** loop todta hai

### `switch` ke `break` se alag nahi — bas context alag
Folder 06 file 04 se: `switch` mein `break` case ko rokta hai. Loop mein `break`
loop ko rokta hai. Ek `switch` **inside** a loop mein `break` **`switch` ko** todta
hai, loop ko nahi:

```cpp
for (...) {
    switch (x) {
        case 1: break;      // ⚠️ yeh switch se nikla, loop se nahi
    }
    // loop yahan continue karta hai
}
```

---

## `continue` — is iteration ka baaki chhodo, agli pe jao

```cpp
for (int i = 0; i < n; ++i) {
    if (a[i] < 0) continue;     // negative -> skip, seedha ++i aur agli iteration
    process(a[i]);              // sirf non-negative yahan pahunchte hain
}
```

- Body ka **baaki hissa** skip
- `for` loop: `continue` → **iteration-expression chalta hai** (`++i`) → phir condition
- `while` / `do-while`: `continue` → **seedha condition** (koi update nahi!)

### ⚠️ `while` + `continue` = infinite loop trap

```cpp
int i = 0;
while (i < n) {
    if (skip(i)) continue;     // ⚠️ ++i skip -> i stuck -> forever
    process(i);
    ++i;
}
```

`for` mein yeh bug nahi hota (iteration-expression `continue` ke baad bhi chalta
hai). Isi liye "skip" wale loops `for` se likho, ya `++i` ko `continue` se **pehle**:

```cpp
int i = 0;
while (i < n) {
    int cur = i;
    ++i;                       // pehle advance
    if (skip(cur)) continue;
    process(cur);
}
```

(`examples/04_loop_bugs.cpp` BUG 7 — chal ke dekho.)

---

## `continue` vs guard clause — dono theek

```cpp
// continue style
for (const auto& order : orders) {
    if (order.cancelled) continue;
    if (order.qty == 0)  continue;
    process(order);
}

// nested-if style (folder 06)
for (const auto& order : orders) {
    if (!order.cancelled && order.qty > 0) {
        process(order);
    }
}
```

`continue` style flatter hai jab kai skip-conditions hon. 1-2 conditions pe `if`
bhi fine. Judgement.

---

## Nested loops se nikalna — `break` sirf ek level

```cpp
bool found = false;
for (int i = 0; i < rows && !found; ++i) {       // outer condition mein flag
    for (int j = 0; j < cols; ++j) {
        if (grid[i][j] == target) {
            found = true;
            break;                               // inner se nikla
        }
    }
}
```

### Options (best → worst for readability)

1. **Function + `return`** — sabse saaf
   ```cpp
   std::optional<Cell> find(const Grid& g, int target) {
       for (int i = 0; i < g.rows; ++i)
           for (int j = 0; j < g.cols; ++j)
               if (g(i, j) == target) return Cell{i, j};
       return std::nullopt;
   }
   ```
2. **Flag** — `bool found` + outer condition mein check (upar wala)
3. **`goto`** — ek jagah jahan `goto` thoda defensible hai (neeche)

C++ mein **labelled break nahi hai** (Java/Rust ke ulat). Isliye yeh options.

---

## `goto` — hai, par lagbhag kabhi nahi

```cpp
for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < cols; ++j) {
        if (grid[i][j] == target) goto done;
    }
}
done:
    std::cout << "search khatam\n";
```

### Kyun avoid

- **Control flow follow karna mushkil** — code kahin se kahin kood jaata hai
- **Scope / initialization skip** kar sakta hai → subtle bugs (compiler kuch
  cases rok deta hai)
- **RAII ke saath fragile** — `goto` jo forward jump kare, kisi object ki
  construction bypass kar sakta hai
- Structured alternatives (function+return, flag, guard clauses, RAII) hmeshah
  maujood hain

### `goto` kab thoda defensible

- **Deep nested loops se single exit** (upar wala pattern) — jab function nikaalna
  awkward ho
- **C-style error cleanup ladder** (`goto cleanup;`) — par C++ mein yeh RAII ka
  kaam hai (folder 17), `goto` nahi

**Is course ka rule:** `goto` mat likho. Agar mann kare, matlab function nikaalne
ka time aa gaya (folder 08).

`break`/`continue` bhi "mini-goto" hain — par woh **structured** hain (sirf loop
boundary tak), isliye theek hain.

---

## ⚠️ Traps

### Trap 1 — `while` + `continue` → increment skip (upar)

### Trap 2 — `break` se galat loop tootne ki ummeed
```cpp
for (...) for (...) { if (x) break; }   // outer chalta rahega
```

### Trap 3 — `break` inside `switch` inside loop
```cpp
for (...) { switch (t) { case A: break; } doMore(); }   // break -> switch se; doMore() chalta hai
```

### Trap 4 — `continue` ke baad zaroori cleanup skip
```cpp
for (...) {
    lock();
    if (cond) continue;    // ⚠️ unlock() skip
    unlock();
}
```
RAII (`std::lock_guard`) use karo — scope end pe khud unlock (folder 17).

### Trap 5 — loop ke aakhri statements ke liye `continue`
```cpp
for (...) {
    if (!cond) continue;
    // ... 1 line ...
}
// ⚠️ agar body ka baaki hissa chhota hai to `if (cond) { ... }` clearer
```

---

## Andar kya hota hai

- `break` → loop ke bahar ke label pe ek `jmp`
- `continue` → `for` ki iteration-expression / `while` ki condition-check pe `jmp`
- Koi runtime cost nahi — bas ek unconditional jump. `-O2` aksar inhe loop ki
  natural structure mein merge kar deta hai.
- `break`/`continue` heavy loops branch-heavy dikh sakte hain — par jab woh
  branches predictable hon (rare skip), predictor sambhal leta hai (folder 06
  file 08).

> **HFT relevance:** Hot decode/matching loops mein `continue` se "is message ko
> chhod do" (malformed, duplicate seq num, wrong symbol) common pattern hai — fast
> reject, aage badho. `break` se "batch khatam / buffer drained". Deep-nested se
> nikalne ke liye HFT code **function + return** use karta hai, `goto` nahi —
> code review aur static analysis dono ke liye. Aur cleanup RAII se, kabhi
> `goto cleanup` se nahi.

---

## Hands-on

`examples/01_loop_types.cpp` (break se infinite loop se nikalna), aur
`examples/04_loop_bugs.cpp` BUG 7 (`while` + `continue` trap):

```bash
./build.ps1 07-LOOPS/examples/01_loop_types.cpp
./build.ps1 07-LOOPS/examples/04_loop_bugs.cpp
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`break` saare nested loops todta hai" | Sirf innermost |
| "`continue` `while` mein `for` jaisa" | `while`: seedha condition (update skip) → infinite risk |
| "C++ mein labelled break hai" | Nahi — flag / function+return / (rarely) `goto` |
| "`goto` kabhi use nahi hota" | Rare deep-loop-exit; par is course mein: mat likho |
| "`break`/`continue` mehnge hain" | Ek jump — free; `-O2` merge kar deta hai |

---

## Exercises

1. **Output:**
   ```cpp
   for (int i = 0; i < 6; ++i) {
       if (i == 2) continue;
       if (i == 4) break;
       std::cout << i << " ";
   }
   ```
   <details><summary>Answer</summary>`0 1 3 ` — `2` skip, `4` pe break.</details>

2. **`while` + `continue` bug:** yeh chalao (`Ctrl+C`), phir 2 tareeke se fix —
   ```cpp
   int i = 0;
   while (i < 5) {
       if (i == 2) continue;
       std::cout << i << " ";
       ++i;
   }
   ```

3. **First match:** `std::vector<int>` mein pehla even number aur uska index
   dhoondho — `break` se. Phir wahi function+`return` se (`std::optional`).

4. **Nested exit:** `n x n` multiplication table mein pehla product `> 50`
   dhoondho, dono loops se turant niklo. Flag se, phir function+return se.

5. **Skip list:** ek `vector<std::string>` process karo, `#` se shuru hone wali
   lines (comments) `continue` se skip.

6. **Refactor `goto`:** yeh `goto` wala code function+return mein badlo —
   ```cpp
   for (int i = 0; i < R; ++i)
       for (int j = 0; j < C; ++j)
           if (m[i][j] < 0) { neg = {i, j}; goto found; }
   found:
       report(neg);
   ```

7. **`continue` vs `if`:** yeh loop dono style mein likho, kaunsa readable —
   "process only rows where `row.active && row.qty > 0 && !row.expired`".

---

## Interview questions

1. `break` aur `continue` mein fark? `for` vs `while` mein `continue`?
2. Nested loops mein `break` kya todta hai? Sab se nikalne ke tareeke?
3. `while` + `continue` se infinite loop kaise banta hai? Fix?
4. C++ mein labelled break hai? Nahi to alternatives?
5. `goto` kyun avoid karte hain? Kaunsa 1 case thoda defensible?
6. Loop ke andar `switch` mein `break` — kya hota hai?
7. `continue` ke saath resource cleanup ka kya khayal (RAII)?

---

## Next
→ [`07-loop-bugs.md`](07-loop-bugs.md)

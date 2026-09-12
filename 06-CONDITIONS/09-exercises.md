# 09 — Folder 06 Revision + Exercises

## Prerequisites
Is folder ke saare lessons (01–08) aur chaaron examples chalaye hue.

---

## PART A — Concept check

### `if` / truthiness
1. `if` statement hai ya expression? `?:` se fark kya practical faayda deta hai?
2. `if (x)` kab true hai jab `x` `int` ho? `x = -1` pe?
3. Bina braces `if` kitni statements control karta hai?
4. `if (v.size())` vs `if (!v.empty())` — kaunsa aur kyun?

### `else` / chains
5. `else if` C++ keyword hai? Nahi to kaam kaise karta hai?
6. `if/else if` chain mein kitni conditions evaluate hoti hain worst case?
7. Overlapping ranges wali chain mein order kyun matter karta hai? Example.
8. Dangling else — `else` kis `if` se bind hota hai?

### nesting / guard clauses
9. "Pyramid of doom" kya hai? Do fixes.
10. Guard clause kya hai? Ek nested function ko convert karke dikhao.
11. `if (a && b)` aur nested `if (a) { if (b) }` — behaviour aur assembly mein fark?
12. Guard clause likhte waqt condition ulti kyun karni padti hai? De Morgan.

### switch
13. `switch` kis type pe chalta hai, kis pe nahi?
14. Fallthrough kya hai? Intentional fallthrough kaise document karte ho?
15. `case` label pe kya restrictions?
16. `case` ke andar variable — kya dikkat, kya fix?
17. `enum class` pe `switch` mein `default` chhodne ka faayda?

### switch vs if / compiler
18. `switch` ko compiler kin tareekon se implement karta hai? Kaunsa kab?
19. Jump table kya hai? Complexity? Kab banti hai?
20. `if/else if` chain kabhi jump table ban sakti hai?
21. `-O0` pe `switch` benchmark karna kyun galat hai?

### if-with-initializer
22. `if (init; cond)` — init variable ka scope? `else` mein visible?
23. C++17 se pehle same effect kaise, aur us tareeke ke 2 nuksaan?

### bugs
24. `if (x = 5)` — compile hota hai? Kaise pakde?
25. `&` vs `&&` — kaunsa condition mein galti se lage to crash?
26. `0.1 + 0.2 == 0.3` ka result aur kyun? Sahi comparison?
27. `if (cond);` (stray `;`) ka asar?
28. `a < b < c` C++ mein kya evaluate karta hai?
29. `for (size_t i = n-1; i >= 0; --i)` mein kya bug, 2 fix?

### branch prediction
30. CPU pipeline aur branch predictor — misprediction penalty (order of magnitude)?
31. Sorted array pe loop unsorted se tez kyun (same values)?
32. Branchless code kab tez, kab slow?
33. `[[likely]]` / `[[unlikely]]` — kya karte hain, risk kya?

---

## PART B — Output prediction

### B1
```cpp
int x = -3;
if (x) std::cout << "A ";
if (x > 0) std::cout << "B ";
if (!x) std::cout << "C ";
std::cout << (x ? "T" : "F");
```
<details><summary>Answer</summary>`A T` — `x` non-zero → `if (x)` true → "A ". `x > 0` false → no B. `!x` false → no C. `-3` truthy → ternary deta hai "T".</details>

### B2
```cpp
int score = 90;
if (score >= 70) std::cout << "C";
else if (score >= 80) std::cout << "B";
else if (score >= 90) std::cout << "A";
else std::cout << "F";
```
<details><summary>Answer</summary>`C` — pehla match (`90 >= 70`) jeet gaya. Chain ka order galat hai.</details>

### B3
```cpp
int level = 2;
switch (level) {
    case 1: std::cout << "1 ";
    case 2: std::cout << "2 ";
    case 3: std::cout << "3 "; break;
    case 4: std::cout << "4 ";
}
```
<details><summary>Answer</summary>`2 3 ` — entry at `case 2`, fallthrough to `case 3`, `break` pe ruka.</details>

### B4
```cpp
int a = 5, b = 3, c = 10;
if (a > b)
    if (b > c)
        std::cout << "X";
    else
        std::cout << "Y";
std::cout << "Z";
```
<details><summary>Answer</summary>`YZ` — `else` inner `if (b > c)` ka hai. `a>b` true, `b>c` false → "Y". Phir "Z" (if ke bahar).</details>

### B5
```cpp
std::cout << (0 < 5 < 3) << " " << (5 > 3 > 1) << " " << (2 < 1 < 5);
```
<details><summary>Answer</summary>`1 0 1` — `(0<5)<3` = `1<3` = 1. `(5>3)>1` = `1>1` = 0. `(2<1)<5` = `0<5` = 1.</details>

### B6
```cpp
if (int n = 7 % 3; n == 1) std::cout << "one ";
else std::cout << "other(" << n << ") ";
// n yahan?
```
<details><summary>Answer</summary>`one ` — `7 % 3 == 1`. `n` block ke bahar visible nahi (agar use karo to compile error).</details>

### B7
```cpp
std::vector<int> v;
std::cout << (v.size() - 1) << " " << (v.size() - 1 >= 0);
```
<details><summary>Answer</summary>`18446744073709551615 1` — `size()` unsigned, `0-1` wraps; `unsigned >= 0` hamesha true. (`-Wtype-limits` warn karta hai.)</details>

### B8
```cpp
bool ok = false;
if (ok = true) std::cout << "yes ";
std::cout << std::boolalpha << ok;
```
<details><summary>Answer</summary>`yes true` — `ok = true` assignment, condition true, `ok` ab `true`. (`-Wparentheses` warn.)</details>

---

## PART C — Find the bug

### C1
```cpp
if (temperature > 100)
    std::cout << "boiling\n";
    shutdown();
```
<details><summary>Answer</summary>Missing braces — `shutdown()` `if` ke bahar, hamesha chalta hai. `{ }` lagao.</details>

### C2
```cpp
switch (msgType) {
    case QUOTE: updateQuote();
    case TRADE: recordTrade(); break;
    default: break;
}
```
<details><summary>Answer</summary>`case QUOTE` mein `break` missing → `QUOTE` aane pe `recordTrade()` bhi chalta hai. `-Wimplicit-fallthrough` warn karta hai.</details>

### C3
```cpp
double ratio = filled / total;
if (ratio == 1.0) markComplete();
```
<details><summary>Answer</summary>Do bugs: (1) agar `filled`/`total` int hain → integer division. (2) float `==` — `filled==total` check karo, ya `ratio >= 1.0 - 1e-9`.</details>

### C4
```cpp
if (order && order->qty > 0 & order->price > 0)
    submit(order);
```
<details><summary>Answer</summary>`&` (bitwise) `order->price > 0` se pehle — short-circuit toota. Agar `order->qty > 0` false bhi ho, `order->price` phir bhi read hoga (yahan safe, par pattern galat). Sab jagah `&&`.</details>

### C5
```cpp
for (std::size_t i = items.size() - 1; i >= 0; --i)
    process(items[i]);
```
<details><summary>Answer</summary>Khali `items` pe `size()-1` wraps → huge. Aur `size_t >= 0` hamesha true → infinite loop + OOB. Fix: `for (std::size_t i = items.size(); i-- > 0;)` ya range-for reverse.</details>

### C6
```cpp
if (state = STATE_ERROR) {
    logError();
}
```
<details><summary>Answer</summary>`=` (assignment) — `state` ko `STATE_ERROR` set kar diya, condition = `STATE_ERROR` truthy. `==` chahiye. `-Wparentheses`.</details>

### C7
```cpp
char grade;
if (marks >= 90) grade = 'A';
else if (marks >= 75) grade = 'B';
else if (marks >= 60) grade = 'C';
std::cout << grade;
```
<details><summary>Answer</summary>`marks < 60` pe `grade` uninitialized → UB. Aakhri `else { grade = 'F'; }` chahiye. (`-Wmaybe-uninitialized`.)</details>

### C8
```cpp
switch (x) {
    case 1:
        int y = compute();
        std::cout << y;
        break;
    case 2:
        std::cout << "two";
        break;
}
```
<details><summary>Answer</summary>`case 1` mein `int y` — `case 2` ka jump `y` ki init bypass karta hai → error "crosses initialization". `case 1` ko `{ }` block mein daalo.</details>

---

## PART D — Practical tasks

### D1. Grade calculator — teen tareeke
Same spec, teen implementations, `static_assert`/tests ke saath:
```cpp
char gradeChain(int marks);   // if / else if chain
char gradeGuard(int marks);   // guard clauses (early return)
char gradeSwitch(int marks);  // switch (marks / 10) pe
```
Teenon same output dein `0..100` (aur `<0`, `>100` pe `'X'`). Kaunsa sabse readable?
`-O2 -S` se teenon ka assembly compare karo.

### D2. FizzBuzz — bina bug
Classic. `1..100`: 3 se divisible → `Fizz`, 5 se → `Buzz`, dono → `FizzBuzz`, warna
number. Common bug (Part B2 wala): `else if` chain jo `FizzBuzz` case miss kar de.
Do versions: ek buggy (dikhaao kyun galat), ek sahi.

### D3. State machine — `switch`
```cpp
enum class State { Idle, Connecting, Connected, Error };
State next(State cur, Event ev);   // switch on cur, andar switch on ev
```
`default` **mat** lagao — enum add karne pe compiler yaad dilaye. Har transition test.

### D4. Message dispatcher — order matters
```cpp
enum class MsgType { Quote, Trade, Heartbeat, OrderAck, Reject };
void dispatch(MsgType t);
```
Frequency: `Quote 85%, Trade 12%, Heartbeat 2%, OrderAck 0.8%, Reject 0.2%`.
(a) `if/else if` chain — best order? (b) `switch` version. (c) Benchmark dono
random weighted input pe, `-O2`. Ratio + explanation.

### D5. Safe container access
```cpp
std::optional<int> at(const std::vector<int>& v, long long index);
```
Negative index, out-of-range, empty — sab guard clauses se handle. `if`-with-
initializer use karo jahan fit ho. Unsigned/signed traps se bacho (`std::ssize`).

### D6. Branch prediction experiment
`04_branch_benchmark.cpp` ko extend karo — ek table banao:

| Data pattern | branch (ms) | branchless (ms) |
|---|---|---|
| all `< 128` (never taken) | ? | ? |
| all `>= 128` (always taken) | ? | ? |
| alternating T,N,T,N | ? | ? |
| random 50/50 | ? | ? |
| sorted (one flip) | ? | ? |

Har row ka result **explain** karo — predictor ka behaviour.

### D7. `[[likely]]` / `[[unlikely]]`
Ek packet-parse function likho: header check, length check, checksum check — teenon
`[[unlikely]]` error paths ke saath. `-O2 -S` se dekho error code function ke end
mein gaya. Bina attribute wale version se assembly compare.

### D8. Ternary vs if — assembly
```cpp
int max1(int a, int b) { return a > b ? a : b; }
int max2(int a, int b) { if (a > b) return a; return b; }
```
`-O2 -S` — dono ka assembly. Same? `cmov` bana? File 05 ka tareeka use karo.

---

## PART E — Self-assessment

```
[ ] Mujhe pata hai if statement hai, expression nahi -- aur ?: ka faayda
[ ] Main truthiness (int/pointer -> bool) samajhta hoon
[ ] Main hamesha braces lagata hoon, aur jaanta hoon kyun
[ ] Mujhe else-if chain ka order matter karna samajh aata hai
[ ] Mujhe dangling else problem pata hai
[ ] Main nested conditions ko && / guard clauses se flatten kar sakta hoon
[ ] Main guard clause likhte waqt condition ulti karna (De Morgan) jaanta hoon
[ ] Mujhe switch ka fallthrough aur break ka role pata hai
[ ] Main [[fallthrough]] aur grouped cases ka fark jaanta hoon
[ ] Mujhe case ke andar variable ka { } wala fix pata hai
[ ] Mujhe enum class + switch + no-default ka completeness faayda pata hai
[ ] Mujhe pata hai compiler switch ko jump table / arithmetic / compares bana sakta hai
[ ] Maine -O2 -S se switch ka assembly dekha hai
[ ] Main if-with-initializer (C++17) use kar sakta hoon aur scope samajhta hoon
[ ] Mujhe = vs ==, & vs && ke bugs aur unke warnings pata hain
[ ] Main float ko == se compare nahi karta -- epsilon use karta hoon
[ ] Mujhe stray semicolon aur chained comparison ke bugs pata hain
[ ] Mujhe unsigned condition ke traps (size()-1, >= 0) pata hain
[ ] Main -Wall -Wextra ke saath compile karta hoon (aur CI mein -Werror)
[ ] Mujhe CPU pipeline aur branch predictor ka basic idea hai
[ ] Maine sorted vs unsorted branch benchmark khud chalaya aur ratio dekha
[ ] Mujhe pata hai branchless kab better hai, kab nahi
[ ] Mujhe [[likely]]/[[unlikely]] ka use aur risk pata hai
[ ] Maine chaaron examples chalaye hain
```

**Scoring:**
- **21–24** → Excellent. Folder 07 (Loops) pe jao. 🎯
- **16–20** → Achha. Jo miss hua — un lessons ko dobara.
- **11–15** → Files 04, 05, 08 dobara karo, examples chalao.
- **< 11** → Poora folder dobara, har example run karo, har benchmark khud chalao.

---

## PART F — Challenge

### Challenge 1: "Traffic light + pedestrian controller"

Ek state machine banao (`switch`-based) jo connected projects ki taraf pehla kadam ho:

```cpp
enum class Light { Red, RedAmber, Green, Amber };
enum class Event { Timer, PedestrianButton, Emergency };

struct Controller {
    Light light = Light::Red;
    bool  pedestrianWaiting = false;
    int   ticksInState = 0;

    void onEvent(Event e);      // switch on light, andar switch/if on e
    std::string_view describe() const;
};
```

Requirements:
- `switch (light)` mein **koi `default` nahi** — `-Wswitch` clean hona chahiye
- `Emergency` event har state se seedha `Red` pe
- `PedestrianButton` sirf `Green` mein effect (flag set), warna ignore
- Guard clauses jahan fit ho
- `main()` mein events ki ek sequence feed karke transitions print karo
- `-Wall -Wextra -Wshadow -Werror` clean

### Challenge 2: "Condition bug hunt"

Ek program likho jo yeh **saare** condition bugs ek jagah demonstrate kare, aur har
ek ke liye:
1. Buggy version (compiler warning capture karo — comment mein exact text)
2. Fixed version
3. Ek input jahan bug **actually galat output** deta hai (sirf theory nahi)

Bugs: `= vs ==`, `& vs &&`, `float ==`, missing braces, stray `;`, `a<b<c`,
`switch` fallthrough, `size()-1` unsigned, uninitialized-in-some-branch,
dangling else.

`03_condition_bugs.cpp` ko starting point ki tarah use karo, extend karo.

### Challenge 3: "Dispatch benchmark suite"

`type` (enum, 8 values) pe dispatch ke 4 tareeke, sabka `-O2` benchmark:
1. `if / else if` chain (frequency order mein)
2. `switch`
3. Function-pointer array — `handlers[type]()`
4. `std::array<F, 8>` of `std::function` (dikhao yeh slow kyun)

Random weighted input pe. Table banao (ns/dispatch). Har result **explain** karo —
jump table, indirect branch prediction, `std::function` overhead. `-S` se
`switch` aur function-pointer version ka assembly compare.

---

## 🎉 Folder 06 complete

Aapne seekha:
- `if` / `else` / `else if` — anatomy, chains, order, dangling else
- Nested conditions — `&&` merge, guard clauses, early return
- `switch` — fallthrough, `[[fallthrough]]`, case scope, `enum class` completeness
- `switch` vs `if` — jump table / arithmetic / compares, real assembly aur benchmark
- `if`-with-initializer (C++17) — scope hygiene
- 7 classic condition bugs — aur konsa warning unhe pakadta hai
- Branch prediction — pehla parichay, **~7x measured** sorted vs unsorted, branchless

Ab aapka program **faisle** leta hai. Agla step: usko **repeat** karana.

---

## Next
→ [`../07-LOOPS/00-README.md`](../07-LOOPS/00-README.md)

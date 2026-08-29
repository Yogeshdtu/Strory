# 13 — Folder 04 Revision + Exercises

## Prerequisites
Is folder ke saare lessons (01–12)

---

## PART A — Concept check

### cout / cin
1. `std::cout` object hai ya function? Uska type kya hai?
2. `<<` chaining kaise kaam karti hai? Woh kya return karta hai?
3. `std::cout << a < b` kyun compile nahi hota?
4. `char*` aur `int*` print karne mein kya fark hai?
5. `>>` whitespace ke saath kya karta hai?
6. Galat type ka input dene pe kya hota hai? 4 cheezein.

### Buffering
7. Buffering kyun hoti hai?
8. `\n` aur `std::endl` mein exact fark kya hai?
9. Flush ke 5 triggers batao.
10. `cerr` unbuffered kyun hai?
11. Crash pe `cout` ka output kyun kho jaata hai, `cerr` ka kyun nahi?
12. Line-buffered aur fully-buffered mein fark? Kab kaunsa hota hai?

### getline / validation
13. `>>` ke baad `getline` kyun fail karta hai? 3 fixes batao.
14. Stream ke 4 states kaunse hain?
15. `clear()` aur `ignore()` dono kyun chahiye?
16. Input loop mein EOF check kyun zaroori hai?
17. `stoi` aur `from_chars` mein fark? `"12abc"` pe dono kya karte hain?

### Manipulators / format
18. `setw` sticky hai ya nahi? Baaki manipulators?
19. `setprecision` `fixed` ke saath aur bina uske kaise alag hai?
20. `hex` reset na karne se kya hota hai?
21. `std::format` `printf` se safe kyun hai?
22. `std::format` aur `std::print` mein fark?

### Files
23. `ofstream` default mein truncate karta hai ya append?
24. `binary` mode kab zaroori hai aur kyun?
25. RAII file streams mein kaise kaam karta hai?
26. Struct binary write karne mein 4 khatre kaunse hain?

### Performance
27. `sync_with_stdio(false)` kya karta hai? Side-effect?
28. `cin.tie(nullptr)` kyun aur kab? Side-effect?
29. HFT hot path mein logging ke 6 rules batao.

---

## PART B — Output prediction

### B1
```cpp
std::cout << 1 + 2 << " " << 2 * 3 << " " << (1 < 2) << "\n";
```
<details><summary>Answer</summary>`3 6 1`</details>

### B2
```cpp
std::cout << std::setw(6) << 1 << 2 << 3 << "\n";
std::cout << std::setfill('0') << std::setw(4) << 7 << "\n";
std::cout << std::setw(4) << 8 << "\n";
```
<details><summary>Answer</summary>

```
     123
0007
0008
```
`setw` sirf agli value pe laga (line 1). `setfill` **sticky** hai, isliye line 3
mein bhi zeros aaye.
</details>

### B3
```cpp
std::cout << std::hex << 255 << "\n";
std::cout << 100 << "\n";
std::cout << std::dec << 100 << "\n";
```
<details><summary>Answer</summary>

```
ff
64
100
```
`hex` sticky hai — reset karna zaroori tha.
</details>

### B4
```cpp
double x = 1234.5678;
std::cout << std::setprecision(3) << x << "\n";
std::cout << std::fixed << std::setprecision(3) << x << "\n";
```
<details><summary>Answer</summary>

```
1.23e+03
1234.568
```
Default mode mein 3 **significant** digits, `fixed` mein 3 **decimals**.
</details>

### B5
```cpp
std::cout << std::format("[{:<8}][{:>8}][{:^8}]\n", "ab", "cd", "ef");
std::cout << std::format("{:05.2f}\n", 3.14159);
std::cout << std::format("{0}-{1}-{0}\n", "X", "Y");
```
<details><summary>Answer</summary>

```
[ab      ][      cd][   ef   ]
03.14
X-Y-X
```
</details>

### B6
```cpp
{ std::ofstream out("t.txt"); out << "A\n"; }
{ std::ofstream out("t.txt"); out << "B\n"; }
// t.txt mein kya hai?
```
<details><summary>Answer</summary>
Sirf `B`. `ofstream` default se **truncate** karta hai. `std::ios::app` chahiye tha.
</details>

---

## PART C — Find the bug

### C1
```cpp
int n;
std::string name;
std::cin >> n;
std::getline(std::cin, name);
std::cout << n << " " << name;
```
<details><summary>Answer</summary>
Newline trap. `>>` ne `\n` buffer mein chhoda, `getline` ne use padh liya → `name` khali.
Fix: `std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');`
</details>

### C2
```cpp
int num;
while (true) {
    std::cin >> num;
    std::cout << num << "\n";
}
```
<details><summary>Answer</summary>
Galat input pe infinite loop. Fix: `clear()` + `ignore()` + EOF check.
</details>

### C3
```cpp
std::cout << std::setfill('*') << std::setw(10) << "abc" << "\n";
std::cout << "Normal text: " << 42 << "\n";
```
<details><summary>Answer</summary>
`setfill` sticky hai — reset nahi kiya. (Yahan `42` pe `setw` nahi hai isliye
dikhega nahi, par agle `setw` pe `*` aayenge.) Fix: `std::setfill(' ')`.
</details>

### C4
```cpp
for (int i = 0; i < 100000; ++i) {
    logFile << "Record " << i << std::endl;
}
```
<details><summary>Answer</summary>
`endl` har line pe flush → 100,000 syscalls. Fix: `"\n"` use karo, end mein ek baar flush.
</details>

### C5
```cpp
std::stringstream ss;
ss << "42";
int a; ss >> a;
ss.str("100");
int b; ss >> b;
```
<details><summary>Answer</summary>
`clear()` missing. Pehle read ke baad `eofbit` set hai, isliye `b` read nahi hoga.
</details>

### C6
```cpp
char buffer[10];
scanf("%s", buffer);
printf(buffer);
```
<details><summary>Answer</summary>
Do critical bugs:
1. `scanf("%s")` — **buffer overflow** (koi limit nahi). Fix: `%9s` ya `fgets`.
2. `printf(buffer)` — **format string vulnerability**. Fix: `printf("%s", buffer)`.
</details>

### C7
```cpp
struct Data { int x; long y; char c; };
std::ofstream out("data.bin");
Data d{1, 2, 'a'};
out.write(reinterpret_cast<const char*>(&d), sizeof(d));
```
<details><summary>Answer</summary>
Teen problems:
1. `std::ios::binary` missing — Windows pe corruption.
2. `long` platform-dependent — `int64_t` use karo.
3. Padding verify nahi kiya — `static_assert(sizeof(Data) == ...)` chahiye.
</details>

---

## PART D — Practical tasks

### D1. Formatted table
Ek program likho jo yeh exact output de (dono manipulators se aur `std::format` se):
```
Symbol          Price      Qty          Value
---------------------------------------------
NIFTY        21500.50      100    2150050.00
BANKNIFTY    46200.25       50    2310012.50
```

### D2. Robust menu
Ek menu-driven program:
- Options 1-5 dikhaye
- Galat input handle kare (letters, out of range, khali line)
- `q` pe exit kare
- EOF pe cleanly exit kare

### D3. CSV processor
Ek program jo CSV file padhe:
```
symbol,price,quantity,side
NIFTY,2150050,100,B
BANKNIFTY,4620025,50,S
```
Aur:
- Header skip kare
- Har row parse kare (`from_chars` se)
- Galat rows report kare, crash na ho
- Total value calculate kare
- Result formatted table mein print kare

### D4. Log file analyzer
Ek log file padho:
```
2026-08-28T10:30:00 INFO Order received id=1001
2026-08-28T10:30:01 ERROR Connection lost
```
Aur count karo: kitne INFO, kitne ERROR, kitne WARN.

### D5. Binary record store
1. `Record` struct banao (fixed-width types se)
2. `static_assert` se size verify karo
3. 100 records binary file mein likho
4. Wapas padho aur verify karo
5. File size check karo (`100 * sizeof(Record)` hona chahiye)

### D6. Hex dump utility
```cpp
void hexDump(const void* data, std::size_t len);
```
Output:
```
0000: 41 42 43 44 45 46 47 48  49 4a 4b 4c 4d 4e 4f 50  |ABCDEFGHIJKLMNOP|
0010: 51 52 53                                          |QRS|
```

### D7. Performance comparison
Benchmark banao: 200k lines output, in modes mein:
1. `cout << x << endl`
2. `cout << x << "\n"`
3. `sync_with_stdio(false)` + `"\n"`
4. String build karke ek `write`
5. `printf`
6. `std::format`

Table banao. Kaunsa kitna tez?

### D8. Simple async logger
```cpp
class AsyncLogger {
    // Producer: log() call karta hai -> queue mein daalta hai
    // Consumer: alag thread -> file mein likhta hai
public:
    void log(std::string_view msg);
};
```
Abhi `std::mutex` + `std::queue` se banao (lock-free version folder 41 mein).
**Producer ki latency measure karo** — sync logging se kitni kam hai?

---

## PART E — Self-assessment

```
[ ] Mujhe pata hai cout ek object hai, function nahi
[ ] Main << chaining ka mechanism samjha sakta hoon
[ ] Mujhe << ki precedence ka trap pata hai
[ ] Mujhe pata hai galat input pe stream ka kya hota hai
[ ] Main clear() + ignore() se recovery kar sakta hoon
[ ] Mujhe EOF check ki zarurat pata hai
[ ] Mujhe >> ke baad getline ka bug aur fix pata hai
[ ] Mujhe buffering samajh aati hai
[ ] Mujhe \n vs endl ka exact fark pata hai (aur cost)
[ ] Mujhe cerr unbuffered hone ka reason pata hai
[ ] Mujhe setw sticky nahi hai, yeh pata hai
[ ] Mujhe setprecision ka mode-dependent behaviour pata hai
[ ] Main std::format use kar sakta hoon
[ ] Mujhe pata hai printf type-safe kyun nahi hai
[ ] Mujhe format string vulnerability pata hai
[ ] Main files read/write kar sakta hoon error handling ke saath
[ ] Mujhe RAII ka file streams mein role pata hai
[ ] Mujhe binary I/O ke khatre pata hain
[ ] Mujhe from_chars vs stoi ka fark pata hai
[ ] Mujhe sync_with_stdio aur cin.tie pata hai
[ ] Mujhe HFT logging ke rules pata hain
[ ] Maine sabhi 8 examples chalaye hain
```

**Scoring:**
- **19-22** → Excellent. Folder 05 pe jao. 🎉
- **15-18** → Achha. Jo miss hua padho.
- **10-14** → Files 03, 06, 12 dobara karo.
- **< 10** → Poora folder dobara. Examples zaroor chalao.

---

## PART F — Challenge

### Challenge 1: "Market Data Replayer"

Ek program banao jo:
1. Ek CSV file padhe jisme market data hai:
   ```
   timestamp_ns,symbol,bid,ask,bid_qty,ask_qty
   1735689600000000000,NIFTY,2150000,2150050,100,150
   ```
2. Har row parse kare — **`from_chars` se, koi allocation nahi**
3. Galat rows gracefully skip kare (count karke report kare)
4. Statistics calculate kare:
   - Total rows, valid rows, invalid rows
   - Average spread (ask − bid)
   - Min/max spread
   - Total volume
5. Result ek formatted table mein de (`std::format` se)
6. **Parsing throughput measure kare** (rows per second)

Phir optimize karo:
- `string_view` se splitting
- String reuse (`getline` ke saath)
- Poori file ek baar mein padhna
- **Before/after numbers report karo**

Yeh exactly wahi hai jo folder 38 (Market Data) mein bada banayenge.

### Challenge 2: "Bulletproof Config Reader"

Ek config file parser banao:
```
# Comment line
max_orders = 1000
tick_size = 0.05
enable_logging = true
log_file = /var/log/trading.log
```

Requirements:
- Comments (`#`) skip kare
- Khali lines skip kare
- Whitespace trim kare
- Type-safe getters: `getInt()`, `getDouble()`, `getBool()`, `getString()`
- Missing key pe `std::optional` return kare
- Galat value pe clear error message de
- Windows `\r\n` handle kare

---

## Aapne Folder 04 complete kar liya! 🎉

Ab aapke programs **baat kar sakte hain** — input le sakte hain, formatted output de
sakte hain, files padh/likh sakte hain.

Aur aapko pata hai ki **I/O kitna mehnga hai** — jo HFT ke liye foundation knowledge hai.

---

## Next
→ [`../05-OPERATORS/00-README.md`](../05-OPERATORS/00-README.md)

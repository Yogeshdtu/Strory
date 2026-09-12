# 10 — Folder 10 Revision + Exercises

## Prerequisites
Lessons 01–09 aur saare 6 examples chalaye hue (`03_sso_demo` ko `-O0` pe).

---

## PART A — Concept check

1. C-string kya hai? `"hi"` ka array size?
2. `strlen` ki complexity? Loop condition mein kyun problem?
3. `strcpy` ka danger? Safer alternatives ke trade-offs?
4. `==` do `const char*` pe — kya compare hota hai? `std::string` pe?
5. `std::string` vs C-string — 4 safety wins?
6. `s.size()` complexity aur kyun (C-string se alag)?
7. `s.clear()` memory ka kya karta hai?
8. SSO kya hai? libstdc++ ka threshold? Portable?
9. Short vs long `std::string` — copy aur move cost?
10. Capacity growth pattern? `reserve` ka faayda?
11. `std::string_view` — contents, size, ownership?
12. `string_view` param vs `const std::string&` — literal ke saath farq?
13. `string_view::substr` vs `std::string::substr` — allocation?
14. `string_view` kab dangle karta hai (3 patterns)? Compiler warning deta hai?
15. `std::from_chars` vs `std::stoi` — 4 differences?
16. `from_chars` whitespace / partial parse / errors?
17. `std::string` allocation kahan-kahan chhupta hai (5)?
18. Hot-path parsing — `std::string` vs `string_view` fields, allocation farq?
19. `std::string` bytes store karta hai ya characters? `"café".size()`?
20. UTF-8 — ASCII se compatible kaise? `s[i]` / `substr` safe hai?

---

## PART B — Output prediction

(Saare jawab GCC 16.2 / libstdc++ pe chala ke check kiye.)

### B1
```cpp
char s[] = "abc";
std::cout << sizeof(s) << " " << std::strlen(s);
```
<details><summary>Answer</summary>`4 3` — array mein `'\0'` shaamil hai; `strlen` use nahi ginta.</details>

### B2
```cpp
std::string s = "hello";
s += " world";
std::cout << s.size() << " " << (s.find("world") != std::string::npos);
```
<details><summary>Answer</summary>`11 1`</details>

### B3
```cpp
std::string a = "cat", b = "cat";
std::cout << std::boolalpha << (a == b) << " " << (a.data() == b.data());
```
<details><summary>Answer</summary>`true false` — content barabar, storage alag-alag.</details>

### B4
```cpp
std::string s = "hello";
std::string_view v = s;
s = "a much longer string that forces a reallocation here";
std::cout << v;   // ??
```
<details><summary>Answer</summary>UB — `s` realloc hone ke baad `v` dangle karta hai. Purana data / garbage / crash kuch bhi. (GCC isko warning se nahi pakadta — lesson 09.)</details>

### B5
```cpp
int v = 0;
const char* in = "  42";
auto [p, ec] = std::from_chars(in, in + 4, v);
std::cout << (ec == std::errc{}) << " " << v;
```
<details><summary>Answer</summary>`0 0` — `from_chars` shuru ke whitespace skip nahi karta → `invalid_argument`, aur `v` ko haath nahi lagta (0 hi raha).</details>

### B6
```cpp
std::string s = "café";   // UTF-8: 'é' 2 bytes ka hai
std::cout << s.size();
```
<details><summary>Answer</summary>`5` — bytes, characters nahi.</details>

### B7
```cpp
std::string s;
std::cout << s.capacity() << " ";
for (int i = 0; i < 16; ++i) s += 'x';
std::cout << s.size() << "/" << s.capacity();
```
<details><summary>Answer</summary>libstdc++: `15 16/30` — SSO capacity 15 se shuru; 16th char heap allocation karwata hai, capacity double hoke 30.</details>

### B8
```cpp
std::string s = "a,b,,d";
int fields = 1;
for (char c : s) if (c == ',') ++fields;
std::cout << fields;
```
<details><summary>Answer</summary>`4` — teen commas → chaar fields (ek khaali).</details>

---

## PART C — Find the bug

### C1
```cpp
char name[8];
std::strcpy(name, userInput);   // userInput ki length pata nahi
```
<details><summary>Answer</summary>Buffer overflow — `strcpy` `name` ka size check nahi karta. `std::string` lo, ya explicit capacity check ke saath bounded copy.</details>

### C2
```cpp
std::string_view getName() {
    std::string full = fetchFromDb();
    return full;
}
```
<details><summary>Answer</summary>Local `std::string` ka view return → dangling. `std::string` (by value) return karo, ya caller ke data ka view. (GCC 16.2 `-Wall -Wextra` is pe warning nahi deta.)</details>

### C3
```cpp
const char* path = config["file"].c_str();
loadFile(path);
```
<details><summary>Answer</summary>Agar `config["file"]` temporary `std::string` lautata hai, to `.c_str()` turant dangle. Pehle `std::string` ko ek named variable mein rakho.</details>

### C4
```cpp
if (line.find("ERROR") >= 0) alert();
```
<details><summary>Answer</summary>`std::string::npos` bahut bada unsigned value hai → `>= 0` hamesha true → har line pe `alert()` chalega. `!= std::string::npos` likho.</details>

### C5
```cpp
std::string result;
for (const auto& tok : tokens) result = result + tok + " ";
```
<details><summary>Answer</summary>O(n²) — har `result + tok` nayi string banata hai (kyunki `result` lvalue hai, har baar copy). `result += tok; result += ' ';` (aur `reserve`).</details>

### C6
```cpp
std::string s = "hello world";
std::string_view w = s.substr(0, 5);
process(w);
```
<details><summary>Answer</summary>`std::string::substr` temporary `std::string` lautata hai; `w` usme dekhta hai → dangling. `std::string_view(s).substr(0, 5)` lo.</details>

### C7
```cpp
std::string s = "Δelta";           // UTF-8: 'Δ' 2 bytes ka hai
std::string first = s.substr(0, 1);
```
<details><summary>Answer</summary>2-byte `Δ` beech se kata → `first` mein aadha character (invalid UTF-8; size 1, jabki `s.size()` 6). Sirf pakki character boundaries pe slice karo.</details>

### C8
```cpp
for (char& c : s) c = std::tolower(c);
```
<details><summary>Answer</summary>Negative `char` (non-ASCII byte) ke saath `std::tolower(int)` UB hai. `std::tolower(static_cast<unsigned char>(c))`. (Aur yeh sirf ASCII ke liye hai.)</details>

---

## PART D — Practical tasks

### D1. String utility library (jahan ho sake zero-alloc)
```cpp
std::string_view trim(std::string_view);
std::vector<std::string_view> split(std::string_view, char);
bool iequalsAscii(std::string_view, std::string_view);          // case-insensitive ASCII
std::string join(std::span<const std::string_view>, std::string_view sep);   // ek alloc, reserved
std::string_view stripPrefix(std::string_view, std::string_view prefix);
```
Poora test suite: khaali, ek element, sab delimiter, koi delimiter nahi, unicode jaisa ka taisa.

### D2. `from_chars` wrappers
```cpp
std::optional<long long> parseInt(std::string_view);            // poori string, whitespace nahi
std::optional<double>    parseDouble(std::string_view);
std::optional<long long> parseIntSkipWs(std::string_view);
```
Peeche ka kachra reject karo. Overflow → `nullopt`. Har ek ke 10+ test cases.

### D3. Key-value config parser
String se `key = value` lines (`#` comments, whitespace, quoted values ke saath) parse karke
`std::unordered_map<std::string, std::string>` mein daalo. Jahan map udhaar le sake wahan zero-copy
(storage ke liye nahi le sakta — charcha karo). Errors pe line number batao.

### D4. Allocation ginne wala CSV parser
`06_csv_parser.cpp` lo. Global `operator new` counter jodo. `string_view` fields ke saath parsing ke
dauraan **0 allocations** saabit karo. Phir `Trade` ko owning `std::string` fields pe badlo — dobara
gino. Phir records vector pe `reserve` jodo — phir gino.

### D5. Fixed-width record formatter (zero heap)
`std::array<char, N>` se `FixedStr<N>` (`append(string_view)`, `appendInt`, `pad(char, n)`, `view()`).
Ek outbound order message (fixed-width symbol, right-justified price, side flag) **zero allocations** se
format karo. Kul width ka `static_assert`.

### D6. `std::from_chars` benchmark suite
`05_fast_parsing.cpp` ko dobara banao + `strtol`, haath ka digit loop, aur `first != last` kachre wala
`std::from_chars` jodo. Table: ns/parse. `-O2` aur `-O3 -march=native`.

### D7. UTF-8 toolkit
```cpp
std::size_t codepointCount(std::string_view);
bool        isValidUtf8(std::string_view);
std::string_view truncateCodepoints(std::string_view, std::size_t maxCP);
std::string      reverseCodepoints(std::string_view);
```
ASCII, 2/3/4-byte sequences, jaan-boojh kar toote bytes, aur khaali input se test karo.

---

## PART E — Self-assessment

```
[ ] C-string layout, '\0', strlen O(n), strcpy ka danger -- saaf
[ ] std::string API (size O(1), .at, +=, clear capacity rakhta hai)
[ ] SSO -- kya hai, threshold, -O0 pe khud naapa
[ ] Capacity growth ~2x, reserve() ka faayda
[ ] std::string_view -- non-owning, no-alloc, substr = view
[ ] string_view dangling ke patterns, kahan safe, aur compiler warn NAHI karta
[ ] string_view null-terminated nahi -- C API trap
[ ] from_chars vs stoi vs atoi vs stringstream (naapa ~8x-30x, dono compilers pe)
[ ] from_chars strictness (ws skip nahi, errc, partial parse)
[ ] Allocation kahan chhupta hai, zero-alloc alternatives
[ ] string_view fields mein parse = 0 allocations
[ ] UTF-8: std::string = bytes, size() != char count
[ ] substr/index/reverse UTF-8 pe kab safe
[ ] tolower(char) ka unsigned-char cast
[ ] Saare 6 examples chalaye
```

**Scoring:**
- **12–15** → Folder 11 (Structs) pe jao. 🎯
- **8–11** → Files 04, 05, 07, 09 dobara.
- **< 8** → Poora folder, `03_sso_demo` `-O0` pe, `05_fast_parsing`, `06_csv_parser`.

---

## PART F — Challenge

### Challenge 1: "Zero-allocation line protocol parser"

Newline se alag ASCII messages ki ek stream parse karo, is format mein:
```
<type>|<seq>|<symbol>|<field>=<val>,<field>=<val>,...
```
`struct Msg { char type; long long seq; std::string_view symbol; std::array<std::pair<std::string_view,
std::string_view>, 8> fields; size_t nFields; };` mein.

Shartein: parsing ke dauraan **0 heap allocations** (`operator new` counter se saabit karo), saare
fields input buffer ke andar `string_view`s, galat lines → saaf error (crash nahi), numbers ke liye
`from_chars`. Adhoore / zaroorat se bade / kachre input se fuzz karo — kabhi OOB nahi, kabhi allocation
nahi.

Phir: ek "buffer recycling" mode jodo jisme input buffer agle batch ke liye dobara use hota hai.
Dangling-view bug dikhao (store kiye `Msg` ab agla batch padh rahe hain), phir fix karo (`symbol` ko
SSO `std::string` mein copy karo, ya arena mein).

### Challenge 2: "String bug museum"

Lesson 09 ka har bug ek live demo ki tarah: buggy version + use pakadne wala tool (compiler warning ka
text — ya yeh saboot ki GCC ne warning *nahi* di; Linux pe ASan output; ya MinGW ke liye reasoning) +
fixed version + ek input jahan bug saaf galat behave kare. `04_string_view.cpp` ko extend karo.

### Challenge 3: "SSO + growth study"

`std::string` ko (`operator new` override se) instrument karo aur `-O0` pe yeh nikaalo:
- aapki stdlib ka exact SSO threshold
- `+=` se `N`-char string banane mein allocation count aur kul bytes, `N = 10, 100, 1000, 100000` ke liye —
  `reserve` ke saath aur bina
- ek table: `push_back` count vs reallocation count vs kul memcpy bytes
- WSL pe libc++ ho to wahan bhi — threshold alag aaya?

Likho: `N = 10^6` ke liye `reserve` kitne allocations bachata hai? (Is repo mein GCC 16.2 pe 1e6 appends
= 17 reallocations gine gaye — lesson 04.)

---

## 🎉 Folder 10 complete

Strings: C-strings (`char[]` + `'\0'`, manual, khatarnak), `std::string` (owning, safe, SSO — 15-char
threshold naapa), `std::string_view` (zero-copy view, dangling rule — aur compiler ki khamoshi),
`from_chars`/`to_chars` (`stoi`/`stringstream` se ~8x–30x tez, naapa), allocation se bachne ka playbook,
UTF-8 (bytes ≠ characters), aur bug catalogue.

Agla: **structs** — jude hue data ko ek saath bandhna, plus **padding & alignment** (HFT ka sabse bada
cache lever).

---

## Next
→ [`../11-STRUCTS/00-README.md`](../11-STRUCTS/00-README.md)

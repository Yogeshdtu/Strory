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
14. `string_view` kab dangle karta hai (3 patterns)?
15. `std::from_chars` vs `std::stoi` — 4 differences?
16. `from_chars` whitespace / partial parse / errors?
17. `std::string` allocation kahan-kahan chhupta hai (5)?
18. Hot-path parsing — `std::string` vs `string_view` fields, allocation farq?
19. `std::string` bytes store karta hai ya characters? `"café".size()`?
20. UTF-8 — ASCII se compatible kaise? `s[i]` / `substr` safe hai?

---

## PART B — Output prediction

### B1
```cpp
char s[] = "abc";
std::cout << sizeof(s) << " " << std::strlen(s);
```
<details><summary>Answer</summary>`4 3` — array includes `'\0'`; `strlen` doesn't count it.</details>

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
<details><summary>Answer</summary>`true false` — content equal, separate storage.</details>

### B4
```cpp
std::string s = "hello";
std::string_view v = s;
s = "a much longer string that forces a reallocation here";
std::cout << v;   // ??
```
<details><summary>Answer</summary>UB — `v` dangles after `s` reallocates. May print old data / garbage / crash.</details>

### B5
```cpp
int v = 0;
auto [p, ec] = std::from_chars("  42", "  42" + 4, v);
std::cout << (ec == std::errc{}) << " " << v;
```
<details><summary>Answer</summary>`0 0` — `from_chars` doesn't skip leading whitespace → `invalid_argument`, `v` untouched (0).</details>

### B6
```cpp
std::string s = "café";   // UTF-8: 'é' is 2 bytes
std::cout << s.size();
```
<details><summary>Answer</summary>`5` — bytes, not characters.</details>

### B7
```cpp
std::string s;
std::cout << s.capacity() << " ";
for (int i = 0; i < 16; ++i) s += 'x';
std::cout << s.size() << "/" << s.capacity();
```
<details><summary>Answer</summary>libstdc++: `15 16/30` — starts with SSO capacity 15; the 16th char forces a heap alloc, capacity doubles to 30.</details>

### B8
```cpp
std::string s = "a,b,,d";
int fields = 1;
for (char c : s) if (c == ',') ++fields;
std::cout << fields;
```
<details><summary>Answer</summary>`4` — three commas → four fields (one empty).</details>

---

## PART C — Find the bug

### C1
```cpp
char name[8];
std::strcpy(name, userInput);   // userInput length unknown
```
<details><summary>Answer</summary>Buffer overflow — `strcpy` doesn't check `name`'s size. Use `std::string`, or bounded copy with an explicit capacity check.</details>

### C2
```cpp
std::string_view getName() {
    std::string full = fetchFromDb();
    return full;
}
```
<details><summary>Answer</summary>Returns a view of a local `std::string` → dangling. Return `std::string` (by value), or a view of caller-owned data.</details>

### C3
```cpp
const char* path = config["file"].c_str();
loadFile(path);
```
<details><summary>Answer</summary>If `config["file"]` returns a temporary `std::string`, `.c_str()` dangles immediately. Bind the `std::string` to a named variable first.</details>

### C4
```cpp
if (line.find("ERROR") >= 0) alert();
```
<details><summary>Answer</summary>`std::string::npos` is a huge unsigned value → `>= 0` is always true → `alert()` fires on every line. Use `!= std::string::npos`.</details>

### C5
```cpp
std::string result;
for (const auto& tok : tokens) result = result + tok + " ";
```
<details><summary>Answer</summary>O(n²) — each `result + tok` builds a new string. `result += tok; result += ' ';` (and `reserve`).</details>

### C6
```cpp
std::string s = "hello world";
std::string_view w = s.substr(0, 5);
process(w);
```
<details><summary>Answer</summary>`std::string::substr` returns a temporary `std::string`; `w` views into it → dangling. `std::string_view(s).substr(0, 5)`.</details>

### C7
```cpp
std::string s = "Δelta";           // UTF-8: 'Δ' is 2 bytes
std::string first = s.substr(0, 1);
```
<details><summary>Answer</summary>Splits the 2-byte `Δ` → `first` is half a character (invalid UTF-8). Only slice at known character boundaries.</details>

### C8
```cpp
for (char& c : s) c = std::tolower(c);
```
<details><summary>Answer</summary>`std::tolower(int)` with a negative `char` (non-ASCII byte) is UB. `std::tolower(static_cast<unsigned char>(c))`. (Also ASCII-only.)</details>

---

## PART D — Practical tasks

### D1. String utility library (all zero-alloc where possible)
```cpp
std::string_view trim(std::string_view);
std::vector<std::string_view> split(std::string_view, char);
bool iequalsAscii(std::string_view, std::string_view);          // case-insensitive ASCII
std::string join(std::span<const std::string_view>, std::string_view sep);   // one alloc, reserved
std::string_view stripPrefix(std::string_view, std::string_view prefix);
```
Full test suite: empty, single, all-delim, no-delim, unicode-passthrough.

### D2. `from_chars` wrappers
```cpp
std::optional<long long> parseInt(std::string_view);            // whole string, no ws
std::optional<double>    parseDouble(std::string_view);
std::optional<long long> parseIntSkipWs(std::string_view);
```
Reject trailing junk. Handle overflow → `nullopt`. 10+ test cases each.

### D3. Key-value config parser
Parse `key = value` lines (with `#` comments, whitespace, quoted values) from a
string into `std::unordered_map<std::string, std::string>`. Zero-copy where the
map can borrow (it can't for storage — discuss). Report line numbers on errors.

### D4. Allocation-counting CSV parser
Take `06_csv_parser.cpp`. Add a global `operator new` counter. Prove **0
allocations** during parsing with `string_view` fields. Then switch `Trade` to
owning `std::string` fields — count again. Then add `reserve` on the records
vector — count again.

### D5. Fixed-width record formatter (zero heap)
`FixedStr<N>` (`append(string_view)`, `appendInt`, `pad(char, n)`, `view()`) using
`std::array<char, N>`. Format an outbound order message (fixed-width symbol,
right-justified price, side flag) with **zero allocations**. `static_assert` the
total width.

### D6. `std::from_chars` benchmark suite
Reproduce `05_fast_parsing.cpp` + add `strtol`, a hand-rolled digit loop, and
`std::from_chars` with `first != last` junk. Table: ns/parse. `-O2` and
`-O3 -march=native`.

### D7. UTF-8 toolkit
```cpp
std::size_t codepointCount(std::string_view);
bool        isValidUtf8(std::string_view);
std::string_view truncateCodepoints(std::string_view, std::size_t maxCP);
std::string      reverseCodepoints(std::string_view);
```
Test with ASCII, 2/3/4-byte sequences, deliberately broken bytes, empty.

---

## PART E — Self-assessment

```
[ ] C-string layout, '\0', strlen O(n), strcpy danger -- clear
[ ] std::string API (size O(1), .at, +=, clear keeps capacity)
[ ] SSO -- kya hai, threshold, -O0 pe measure kiya
[ ] Capacity growth ~2x, reserve() ka faayda
[ ] std::string_view -- non-owning, no-alloc, substr = view
[ ] string_view dangling ke patterns aur kahan safe
[ ] string_view not null-terminated -- C API trap
[ ] from_chars vs stoi vs atoi vs stringstream (measured ~8x-30x)
[ ] from_chars strictness (no ws skip, errc, partial parse)
[ ] Allocation kahan chhupta hai, zero-alloc alternatives
[ ] Parse into string_view fields = 0 allocations
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

Parse a stream of newline-delimited ASCII messages of the form:
```
<type>|<seq>|<symbol>|<field>=<val>,<field>=<val>,...
```
into a `struct Msg { char type; long long seq; std::string_view symbol;
std::array<std::pair<std::string_view, std::string_view>, 8> fields; size_t
nFields; };`

Requirements: **0 heap allocations** during parsing (prove with an `operator new`
counter), all fields are `string_view`s into the input buffer, malformed lines
→ a clean error (not a crash), `from_chars` for numbers. Fuzz it with truncated /
oversized / garbage input — never OOB, never allocate.

Then: add a "buffer recycling" mode where the input buffer is reused for the next
batch. Show the dangling-view bug (stored `Msg`s now read the next batch), then
fix it (copy `symbol` into an SSO `std::string`, or an arena).

### Challenge 2: "String bug museum"

Every bug from lesson 09 as a live demo: buggy version + the tool that catches it
(compiler warning text, ASan output if on Linux, or reasoning for MinGW) + fixed
version + an input where the bug visibly misbehaves. Extend `04_string_view.cpp`.

### Challenge 3: "SSO + growth study"

Instrument `std::string` (via `operator new` override) and produce, at `-O0`:
- exact SSO threshold on your stdlib
- allocation count and total bytes for building an `N`-char string via `+=` for
  `N = 10, 100, 1000, 100000` — with and without `reserve`
- a table: `push_back` count vs reallocation count vs total memcpy bytes
- repeat with libc++ if available (WSL) — different threshold?

Write up: how many allocations does `reserve` save for `N = 10^6`?

---

## 🎉 Folder 10 complete

Strings: C-strings (`char[]` + `'\0'`, manual, dangerous), `std::string`
(owning, safe, SSO — measured 15-char threshold), `std::string_view` (zero-copy
view, dangling rule), `from_chars`/`to_chars` (measured ~8x–30x vs `stoi`/
`stringstream`), the allocation-avoidance playbook, UTF-8 (bytes ≠ characters),
and the bug catalogue.

Agla: **structs** — related data bundled together, plus **padding & alignment**
(HFT's biggest cache lever).

---

## Next
→ [`../11-STRUCTS/00-README.md`](../11-STRUCTS/00-README.md)

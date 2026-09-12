# 09 — String bugs

## Prerequisites
- [`01-c-strings.md`](01-c-strings.md) … [`08-unicode-and-encoding.md`](08-unicode-and-encoding.md)
- `09-ARRAYS/11-common-array-bugs.md`, `08-FUNCTIONS/06-scope-and-lifetime.md`

## Yeh topic abhi kyun
String bugs mostly **lifetime** (dangling views/pointers) aur **encoding**
(byte vs character) ke hain. Kai compile ho jaate hain aur "kabhi kabhi" fail
karte hain. Yeh unka catalogue hai.

---

## Bug 1 — Dangling `std::string_view` (#1 string bug)

```cpp
// (a) temporary ka view
std::string_view sv = std::string("x") + "y";        // ⚠️ ; ke baad temp gaya

// (b) local ka view return
std::string_view name() { std::string s = "hi"; return s; }   // ⚠️

// (c) view source se zyada jee gaya
std::string_view later;
{ std::string s = "block"; later = s; }               // ⚠️ s destroy ho gaya

// (d) std::string se string_view, phir string badli
std::string s = "hello"; std::string_view v = s; s += " world";   // ⚠️ realloc -> v dangle

// (e) std::string::substr -> temp std::string -> view dangle
std::string_view w = s.substr(0, 3);                  // ⚠️ (std::string_view(s).substr(...) lo)
```

### ⚠️ Compiler yahan aapko nahi bachayega — chala ke dekha
In paanchon ko ek file mein daal ke GCC 16.2 pe `-Wall -Wextra` se compile kiya, `-O0` aur `-O2` dono
pe: **ek bhi warning nahi aayi** — (b) "local ka view return" pe bhi nahi. Aur `-Wdangling` naam ka
flag GCC mein **hai hi nahi** — lagaoge to `unrecognized command-line option '-Wdangling'` aur build
fail. (Woh Clang ka flag hai.)

GCC ke asli dangling warnings — `-Wreturn-local-addr` (local ka **pointer/reference** return),
`-Wdangling-reference` (function se laute temporary ka reference), `-Wdangling-pointer` — pointer aur
reference ke kuch cases pakadte hain, `string_view` wale nahi. Runtime pe ASan pakadta hai (Linux;
MinGW pe ASan nahi).

**Rule: view apne source se zyada nahi jeena chahiye.** Parameters: safe. Store/return kiya: ownership
socho. Compiler ki khamoshi ko "safe hai" mat samjho.

---

## Bug 2 — Dangling `.c_str()` / `.data()`

```cpp
const char* p = getConfig().c_str();       // ⚠️ temp std::string destroy -> p dangling
::open(p, O_RDONLY);

std::string s = "path";
const char* q = s.c_str();
s = "different much longer path value here";  // ⚠️ realloc -> q dangling
```

`.c_str()`/`.data()` string ke **abhi wale buffer ke andar** ka pointer dete hain. Koi bhi badlaav jo
realloc kare, ya string ka khatam hona, use invalid kar deta hai. Jab tak pointer use ho raha hai,
`std::string` ko zinda aur bina badle rakho.

---

## Bug 3 — Iterator / reference invalidation

```cpp
std::string s = "hello";
char& first = s[0];              // ya auto it = s.begin();
s += " world";                   // ⚠️ reallocation -> `first` / `it` dangling
s.insert(0, ">>> ");             // ⚠️ wahi baat

for (auto it = s.begin(); it != s.end(); ++it)
    if (*it == ' ') s.erase(it); // ⚠️ erase ne it invalid kar diya; ++it -> UB
```

Jo bhi operation realloc ya shift kare (`+=`, `insert`, `erase`, `resize`, alag length ka `replace`),
woh badlaav wali jagah ke baad ke iterators/pointers/references invalid kar deta hai. Pehle se
`reserve` karo, ya baad mein dobara lo; hataane ke liye `std::erase`/`erase_if` (C++20) lo.

---

## Bug 4 — `std::string_view` null-terminated nahi → C API

```cpp
std::string_view sv = std::string_view("hello world").substr(0, 5);  // "hello"
::printf("%s\n", sv.data());          // ⚠️ "hello world" print hota hai ("hello" ke baad '\0' nahi)
::open(sv.data(), O_RDONLY);          // ⚠️ path "hello world..." ban jaata hai

::printf("%.*s\n", (int)sv.size(), sv.data());   // ✅ length ki seema ke saath
::open(std::string(sv).c_str(), O_RDONLY);       // ✅ owning null-terminated copy
```

---

## Bug 5 — khaali string pe `s.size() - 1` (unsigned wrap)

```cpp
std::string s;
if (s.size() - 1 >= 0) { char last = s[s.size() - 1]; }   // ⚠️ 0 - 1 -> bahut bada -> s[huge] UB
if (!s.empty()) { char last = s.back(); }                  // ✅
```

`size()` unsigned hai. (Folder 06 file 07, folder 07 file 07.)

---

## Bug 6 — `find` ka result `>= 0` se check

```cpp
if (s.find("x") >= 0) { ... }         // ⚠️ npos bahut bada unsigned -> hamesha true
if (s.find("x") != std::string::npos) { ... }   // ✅
```

---

## Bug 7 — `+` chain / `= +` loop → O(n²)

```cpp
std::string out;
for (auto& part : parts) out = out + part + ",";   // ⚠️ har iteration `out` dobara banta hai

for (auto& part : parts) { out += part; out += ','; }   // ✅ O(n) amortized
out.reserve(total);  /* aur behtar */
```

---

## Bug 8 — Encoding: byte vs character (file 08)

```cpp
std::string name = "José";                    // 5 bytes
if (name.size() > 4) name = name.substr(0, 4); // ⚠️ 'é' beech se kata -> invalid UTF-8
std::reverse(name.begin(), name.end());        // ⚠️ multi-byte chars bigad jaate hain
```

Truncate / index sirf un jagahon pe jahan character shuru hona pakka pata ho.

---

## Bug 9 — seedhe `char` ke saath `std::tolower` / `std::toupper`

```cpp
for (char& c : s) c = std::toupper(c);        // ⚠️ c < 0 (non-ASCII byte) pe UB
for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));  // ✅
```

`<cctype>` functions ko aisa `int` chahiye jo valid `unsigned char` ho ya `EOF`. Negative `char` → UB.

---

## Bug 10 — `std::string` ko aise `const char*` se banana jo null ho sakta hai

```cpp
const char* p = maybeNull();
if (std::string(p) == "x") { ... }            // ⚠️ null se std::string banana
if (p && std::string_view(p) == "x") { ... }  // ✅
```

Standard ke hisaab se null `const char*` se string banana UB hai. libstdc++ (GCC 16.2) mein chala ke
dekha: yeh check karta hai aur exception phenkta hai — `basic_string: construction from null is not
valid`. Yaani program chupchaap nahi, `std::logic_error` ke saath girega — par doosri libraries pe yeh
guarantee nahi, isliye pehle `p` check karo.

---

## Bug 11 — `getline` aur peeche bacha `'\r'` (CRLF files)

```cpp
std::string line;
std::getline(in, line);                        // CRLF file pe: line ke aakhir mein '\r' bacha
if (!line.empty() && line.back() == '\r') line.pop_back();   // ✅ hata do
```

---

## Detection

| Bug | Tool |
|---|---|
| dangling view / `c_str` | GCC warnings **nahi pakadte** (test kiya); Clang `-Wdangling`; ASan (Linux); review |
| iterator invalidation | ASan, `_GLIBCXX_DEBUG` |
| unsigned wrap / `find` pe `>= 0` | `-Wtype-limits` (`-Wextra` mein, kabhi-kabhi), review |
| encoding | non-ASCII data wale tests; UTF-8 validators |
| `tolower(char)` | `-Wall` nahi pakadta — review / lint |
| null se `std::string` | libstdc++ exception phenkta hai; ASan / UBSan; review |

> **HFT relevance:** Zero-copy parser mein Bug 1/2 wala dangling view hi kaatta hai: parsed record ka
> `std::string_view` field receive buffer mein point karta hai jo agle packet ke liye recycle ho jaata
> hai → record ab agle message ke bytes padh raha hai. Aur jaisa upar dekha, compiler iski warning nahi
> deta. Fix: record ki zindagi tak buffer pakad ke rakho, ya field ko owning chhoti string / arena mein
> copy karo. Encoding bugs hot path pe kam hain (ASCII protocols) par reference data mein matter karte
> hain. Parsers pe `-Wall -Wextra -Werror` + ASan/UBSan + fuzzing. Folder 38, 45.

---

## Hands-on

`examples/04_string_view.cpp` (3 dangling patterns + null-termination), `examples/06_csv_parser.cpp` ("source buffer ke
andar views" wala ownership note):

```bash
./build.ps1 10-STRINGS/examples/04_string_view.cpp
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`string_view` copy karta hai, isliye safe hai" | Non-owning — source mara to dangle |
| "`.c_str()` ka pointer stable hai" | Badlaav / destruction se invalid |
| "`s.find(x) >= 0`" | `npos` bahut bada unsigned — `!= npos` |
| "`substr(0, n)` UTF-8 safe hai" | Sirf character boundaries pe |
| "`char` pe `tolower(c)` theek hai" | Negative pe UB — `unsigned char` mein cast karo |
| "Compiler dangling `string_view` pe warn karega" | GCC 16.2 `-Wall -Wextra` ne 5 mein se 0 pakde; `-Wdangling` GCC flag hi nahi |

---

## Exercises

1. **Dangling repro:** Bug 1(a)–(e) likho. GCC `-Wall -Wextra` kaunse pakadta hai? Clang ho to
   `-Wdangling` bhi try karo. (Linux) ek ko ASan ke neeche chalao.
   <details><summary>Answer (GCC wala hissa)</summary>

   GCC 16.2, `-O0` aur `-O2`: **koi warning nahi**, paanchon pe. `g++ -Wdangling` → `unrecognized
   command-line option`. Isliye `string_view` ke lifetime ke liye design (views sirf parameters mein)
   aur ASan pe bharosa karo, warnings pe nahi.
   </details>

2. **`c_str` lifetime:** `const char* p = std::string("x").c_str(); puts(p);` — `-Wall` kuch kehta hai?
   Behaviour? Do tareeke se fix karo.

3. **Invalidation:** `std::string s = "a b c d e"; for (auto it = s.begin(); it != s.end(); ++it) if
   (*it == ' ') s.erase(it);` — bug? `std::erase(s, ' ')` se fix karo.

4. **Non-null-terminated view → printf:** galat output dikhao, `%.*s` se fix karo.

5. **Encoding truncate:** `"naïve"` ko "4 characters" tak truncate karo — byte-offset wala version
   todta hai dikhao, phir code-point-safe version.

6. **`tolower` UB:** `std::string s = "café"; for (char& c : s) c = std::toupper(c);` —
   `-fsanitize=undefined` (Linux) se build karo. UBSan kya kehta hai? Fix karo.

7. **CRLF:** `\r\n` line endings wali file `getline` se padho; peeche ka `'\r'` dikhao; hatao.

8. **Null se string:** `const char* p = nullptr; std::string s(p);` — GCC 16.2 pe chalao. Kya hua?
   <details><summary>Answer</summary>

   libstdc++ `std::logic_error` phenkta hai: `basic_string: construction from null is not valid`
   (chala ke dekha). Standard ise UB kehta hai — doosri library pe crash ya kuch bhi ho sakta hai.
   </details>

---

## Interview questions

1. Dangling `std::string_view` — 3 ways it happens? Where is it safe?
2. `.c_str()` pointer kab invalidate hota hai?
3. `std::string` iterator/reference invalidation — kaunse operations?
4. `string_view.data()` C API ko dena kyun galat? Fix?
5. `s.find(x)` ka result kaise check karo (aur `>= 0` kyun galat)?
6. UTF-8 string ko truncate karna — safe kaise?
7. Kya GCC dangling `string_view` pe warning deta hai? Aap is bug se kaise bachoge?

---

## Next
→ [`10-exercises.md`](10-exercises.md)

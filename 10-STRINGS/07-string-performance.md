# 07 — String performance — allocation is the enemy

## Prerequisites
- [`04-string-internals-sso.md`](04-string-internals-sso.md), [`05-string-view.md`](05-string-view.md), [`06-string-conversions.md`](06-string-conversions.md)
- `09-ARRAYS/10-array-performance.md`

## Yeh topic abhi kyun
`std::string` ka har heap allocation ek kharcha hai — folder 36 mein naapa gaya: ek 64-byte
`new`/`delete` ka p50 ~40 ns aur p99.9 ~120 ns, aur mixed-size churn mein p99.9 ~2.6 µs tak
(`36-LOW-LATENCY-CPP/examples/01_allocation_cost.cpp`, GCC 15.1). Text-heavy code mein yeh jud jaata
hai. Yeh lesson: **allocations kahan chhupte hain, aur unhe kaise hataao** — HFT parsing ka core.

---

## Kahan allocation hota hai

GCC 16.2, `-O0` pe `operator new` gin ke check kiya:

```cpp
std::string s = "this is longer than SSO";    // 1 alloc  (chhoti strings: 0 -- file 04)
std::string t = s;                             // 1 alloc  (heap buffer ki copy)
s += " and more and more and more...";        // capacity bharne pe 1 alloc
std::string u = s.substr(5, 10);               // SSO se bada ho to 1 alloc (substr copy karta hai)
std::string v = a + b + c;                     // 20-20 chars ki a,b,c pe: 2 allocs (naapa)
void f(const std::string& p); f("literal");    // 0 alloc -- "literal" 7 chars, SSO mein aa gaya
f("this literal is longer than sso");          // 1 alloc -- 31 chars ka temp std::string
std::string w = std::to_string(x);             // chhote numbers SSO mein; bade string pe alloc
for (auto& p : parts) result = result + p;     // har iteration alloc -- O(n^2) kaam
```

**Zero-alloc alternatives:**

| Allocation lagta hai | Zero-alloc version |
|---|---|
| `void f(const std::string&)` + lamba literal | `void f(std::string_view)` |
| `s.substr(a, b)` | `std::string_view(s).substr(a, b)` |
| `split` → `vector<string>` | `split` → `vector<string_view>` |
| `a + b + c` | `out.reserve(...); out += a; out += b; out += c;` |
| loop mein `std::to_string(x)` | reused buffer mein `std::to_chars` |
| har iteration nayi `std::string` | ek `std::string`, `.clear()` karke dobara use |
| loop mein `s += x` | pehle `s.reserve(finalSize)` |

---

## `reserve()` — sabse bada faayda

```cpp
std::string s;
for (int i = 0; i < 100000; ++i) s += 'x';    // ⚠️ 13 reallocations (15 se shuru, 2x growth), O(n) copying

std::string s;
s.reserve(100000);                             // ✅ EK allocation
for (int i = 0; i < 100000; ++i) s += 'x';
```

(GCC 16.2 pe gina: capacity 15 → 30 → … → 122880, yaani **13 badlaav**. `std::vector<int>` capacity 1
se shuru hota hai, isliye wahan 100000 pe 18 allocations the — `09-ARRAYS/10`.)

`std::vector<std::string>` results, output buffers, JSON builders — sab pe yahi. Upper limit ka
andaaza lagao aur `reserve` karo.

---

## Buffers dobara use karo — har iteration naya allocation nahi

```cpp
// ❌? har line ke liye nayi std::string
for (std::string line; std::getline(in, line); ) { process(line); }
//   ^ getline iterations ke beech `line` ki capacity reuse karta hai -- yeh asal mein theek-thaak hai

// ❌ sach mein bura -- har record ke liye taaza string
for (auto& rec : records) {
    std::string key = rec.venue + ":" + rec.symbol;   // har iteration allocation
    map[key] = ...;
}

// ✅ reuse
std::string key;
key.reserve(32);
for (auto& rec : records) {
    key.clear();                                       // capacity bachi rehti hai
    key += rec.venue; key += ':'; key += rec.symbol;
    map[key] = ...;                                    // (transparent lookup ek aur alloc bachata hai)
}
```

`clear()` buffer rakh leta hai (file 02) — use dobara use karo.

---

## `string` mein nahi, `string_view` mein parse karo

```cpp
// ❌ har field ek heap allocation (jab SSO se bada ho)
struct Trade { std::string symbol; std::string side; double px; long qty; };

// ✅ fields receive buffer ke andar views hain -- ZERO allocations
struct Trade { std::string_view symbol; std::string_view side; double px; long qty; };
```

Is course mein aur jagah naapa: `for (auto s : names)` (copy) vs `for (const auto& s : names)` —
**~50x** slow (folder 07 file 04). Wahi sabak: copy se bacho.

`examples/06_csv_parser.cpp` — poori CSV ko `string_view` fields mein parse karta hai, **0
`std::string` allocations**.

---

## `from_chars` / `to_chars` — bina allocation ke number I/O

`std::stoi` / `std::to_string` / `std::stringstream` allocation / locale / exceptions ka kharcha
laate hain. `std::from_chars` / `std::to_chars` caller ke buffer mein kaam karte hain. **~8x–30x**
tez (file 06, `examples/05_fast_parsing.cpp`, dono compilers pe naapa).

---

## Pata ho ki data kitna bada hoga — chhote/static buffers

```cpp
// Fixed-width outbound field
std::array<char, 8> symbolField;              // stack, heap nahi
std::ranges::fill(symbolField, ' ');
std::ranges::copy(sym, symbolField.begin());

// bounded scratch ke liye "static vector of char"
template <std::size_t Cap> struct FixedStr {
    std::array<char, Cap> buf; std::size_t len = 0;
    void append(std::string_view s);          // assert: len + s.size() <= Cap
    std::string_view view() const { return {buf.data(), len}; }
};
```

C++26: `std::inplace_vector` (GCC 16.2 pe chal gaya — folder 22 file 15 ki probe); ab tak:
`boost::static_vector`, ya khud banao.

---

## `std::string` copy ki cost — length pe depend (file 04)

- Chhoti (≤ SSO threshold): copy = inline chars, **allocation nahi**.
- Lambi: `new` + `memcpy(len)`.
- **Move**: lambi strings → O(1) (pointer churaao); chhoti strings → inline chars copy (churaane ko
  pointer nahi).

Isliye **chhoti keys** (symbols, codes) ke liye `std::string` garam-ish paths pe bhi theek hai. Lamba
text → views + reserve + reuse.

---

## Andar kya hota hai

- `operator new` → allocator: free-list mein talaash, badi/pehli allocations pe shayad syscall
  (`mmap`/`brk`), multi-threaded mein shayad lock. Das se sau ns tak, aur **anishchit** — tail
  latency yahin se aati hai (folder 36 ke naape hue p99.9 numbers).
- Naye page ka pehla touch → page fault + zero-fill (folder 29).
- Capacity ke bahar `+=` → bada `new` + `memcpy` + `delete` — aur naya buffer cache mein thanda.
- `string_view` operations → sirf pointer arithmetic. `reserve` → ek allocation, baaki sab uspe.

> **HFT relevance:** Market-data / order-entry path pe target hai **har message pe zero
> allocations**. Tareeke: receive buffer pe `std::string_view` fields; numbers ke liye
> `std::from_chars`; pehle se allocate kiye aur dobara use hone wale scratch `std::string`s; bahar
> jaane wale fixed-width fields ke liye `std::array<char, N>`; jo cheez maalik honi hi chahiye uske
> liye arena/pool allocators (folder 36). Decode loop mein ek allocation = p99 latency spike
> (allocator lock / page fault), aur isko code review aur allocation-ginne wale tests pakadte hain.
> Folders 36, 38.

---

## Hands-on

```bash
g++ -std=c++20 -O0 10-STRINGS/examples/03_sso_demo.cpp -o sso && ./sso   # capacity growth
./build.ps1 fast 10-STRINGS/examples/05_fast_parsing.cpp                  # from_chars vs baaki
./build.ps1 10-STRINGS/examples/06_csv_parser.cpp                         # zero-copy parse
```

`06_csv_parser.cpp` mein global `operator new` counter jodo (`03_sso_demo.cpp` jaisa) — confirm karo
ki parsing ke dauraan 0 allocations hain.

---

## ⚠️ Traps

### Trap 1 — `const std::string&` param ko lambe literals ke saath call karna
```cpp
void log(const std::string& m);  log("a message longer than fifteen");   // ⚠️ temp std::string. std::string_view lo
```

### Trap 2 — view ke liye `substr`
```cpp
if (s.substr(0, 3) == "GET") ...   // ⚠️ nayi string banti hai. std::string_view(s).substr(0, 3)
```

### Trap 3 — `+` chain / `= +` loop
```cpp
for (...) result = result + piece;   // ⚠️ O(n^2). reserve + +=
```

### Trap 4 — har loop iteration nayi `std::string`
```cpp
for (...) { std::string tmp = build(); use(tmp); }   // ⚠️ har iteration alloc/free. Bahar nikaalo + clear()
```

### Trap 5 — string code ko `-O0` pe benchmark karke bharosa karna
Performance ke liye `-O2`; par dhyaan do ki allocation elision (file 04) kharcha chhupa sakta hai —
sirf time mat lo, **allocations gino**.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`std::string` sasta hai, har jagah lagao" | Lambi string ka har operation heap allocation |
| "`substr` ek view hai" | Copy karta hai — view chahiye to `string_view::substr` |
| "`reserve` micro-optimization hai" | O(n) allocations ko 1 bana deta hai |
| "`std::string` copy ki cost fixed hai" | Length pe depend (SSO vs heap) |
| "`std::string` fields mein parse karna theek hai" | `std::string_view` fields → 0 allocations |
| "`const std::string&` ko literal dena hamesha allocate karta hai" | Sirf SSO se bade literal pe (7-char: 0, 31-char: 1 — gina) |

---

## Exercises

1. **Allocation audit:** `06_csv_parser.cpp` mein `operator new` counter jodo. Parsing ke dauraan
   kitne allocations? Ab `Trade` ke fields `std::string` kar do — dobara gino.

2. **`reserve`:** `+=` se 1MB string banao, `reserve` ke saath aur bina. `-O0` (elision se bachne ke
   liye), allocations gino + time lo.

3. **`substr` vs view:** 1M baar, `s.substr(0, 5) == "hello"` vs `std::string_view(s).substr(0, 5) ==
   "hello"`. `-O2`, time lo. (Dhyaan: 5 chars SSO mein hain — to allocation nahi, phir bhi farq aaya?
   Lambi substring se bhi try karo.)

4. **Reuse:** 100k records ke liye `venue + ":" + symbol` keys banao — har iteration nayi `std::string`
   vs ek reused + `clear()`. Allocations gino.

5. **Param type:** `void handle(const std::string&)` vs `std::string_view`, 1M baar ek literal ke
   saath call. Temp allocations gino — pehle 7-char literal se, phir 30-char se.

6. **Fixed string:** `FixedStr<32>` implement karo (`append`, `view`, overflow assert). Usse ek
   fixed-width record bina heap ke format karo.

---

## Interview questions

1. `std::string` allocation kahan-kahan chhupta hai (5 places)?
2. `reserve()` ka faayda — quantify?
3. Parse karte waqt `std::string` vs `std::string_view` fields — allocation farq?
4. `const std::string&` param + literal call — kya hota hai?
5. Buffer reuse (`clear()` + `+=`) kyun better than fresh string per iteration?
6. HFT hot path pe string allocation target kya, aur kaise achieve karo?

---

## Next
→ [`08-unicode-and-encoding.md`](08-unicode-and-encoding.md)

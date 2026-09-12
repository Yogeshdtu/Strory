# 06 — C++ kya hai (aur HFT mein kyun)?

## Prerequisites
`05-compiler-vs-interpreter.md`

## Yeh topic abhi kyun
Aap agli 1-2 saal C++ ke saath bitane wale ho. Yeh jaan lena chahiye ki yeh language
**kya hai, kahan se aayi, kis cheez mein achhi hai, aur kis mein nahi.**

---

## Ek line mein

**C++ ek compiled, statically-typed, multi-paradigm systems programming language hai
jo aapko high-level abstractions deti hai bina low-level control chhode.**

Ab ise todte hain:

| Shabd | Matlab |
|---|---|
| **Compiled** | machine code pehle hi ban jaata hai (folder 01/05 mein padha) |
| **Statically typed** | har variable ka type compile time pe fix hota hai. `int` hamesha `int` rahega |
| **Multi-paradigm** | procedural + object-oriented + generic + functional — sab styles support karta hai |
| **Systems programming** | OS, drivers, databases, engines — jahan hardware ke paas jaana ho |
| **Zero-cost abstraction** | achhe abstractions use karo, performance nahi jaati |

---

## Chhota itihaas

| Saal | Kya hua |
|---|---|
| 1972 | **C** bani (Dennis Ritchie, Bell Labs) — fast, portable, minimal |
| 1979 | Bjarne Stroustrup ne "C with Classes" shuru ki |
| 1983 | Naam **C++** pada (`++` = "C se ek zyada", increment operator se) |
| 1998 | **C++98** — pehla ISO standard |
| 2011 | **C++11** — 🔥 *"jaise nayi language ho"*. auto, lambdas, move semantics, threads |
| 2014 | **C++14** — C++11 ki polish |
| 2017 | **C++17** — structured bindings, `optional`, `variant`, filesystem |
| 2020 | **C++20** — 🔥 concepts, ranges, coroutines, modules, `<=>` |
| 2023 | **C++23** — `std::expected`, `std::print`, deducing `this` |
| 2026 | **C++26** — reflection, contracts, senders/receivers (aa raha hai) |

**Yaad rakhne wali baat:** C++11 se pehle aur baad — do alag duniyaein hain.
Agar aapko koi tutorial `new`/`delete` sikha raha hai bina smart pointers ke, ya
`std::auto_ptr` bata raha hai — woh 15 saal purana hai, chhod do.

**Is course mein hum C++20 use karenge**, aur jahan relevant ho C++23 bhi.

---

## C++ ki philosophy (Stroustrup ke rules)

1. **"You don't pay for what you don't use."**
   Agar aap exceptions use nahi karte, unki cost nahi lagti. Agar virtual functions
   use nahi karte, vtable nahi banti. Yeh **zero-overhead principle** hai.

2. **"Leave no room for a lower-level language below C++, except assembly."**
   C++ hardware ke itna paas hai ki neeche sirf assembly bachti hai.

3. **"Trust the programmer."**
   C++ aapko rokta nahi. Aap apne pair pe goli maar sakte ho. Yeh feature bhi hai
   aur bug bhi.

---

## C++ kahan use hoti hai

| Domain | Examples |
|---|---|
| **Trading / HFT** | Jump, Citadel, Optiver, Jane Street (OCaml+C++), Tower, HRT, IMC |
| **Game engines** | Unreal Engine, Unity core, CryEngine |
| **Browsers** | Chrome (Blink), Firefox (Gecko), Safari (WebKit) |
| **Databases** | MySQL, MongoDB, ClickHouse, RocksDB |
| **Operating systems** | Windows kernel parts, macOS parts |
| **Embedded / automotive** | cars, robots, medical devices |
| **ML infrastructure** | TensorFlow core, PyTorch core (Python sirf upar ka layer hai) |
| **Graphics / VFX** | Adobe products, Blender, Maya |
| **Aerospace** | Mars rovers, flight software |

Dekha? Jahan bhi **performance** ya **hardware control** chahiye — C++.

---

## HFT mein C++ hi kyun? (yeh dhyaan se padho)

Yeh **poore course ki motivation** hai.

### 1. Predictable performance (sabse important)

HFT mein **consistency > raw speed**.

```
System A:  hamesha 5 µs                       <- YEH CHAHIYE
System B:  aksar 2 µs, kabhi kabhi 50 µs      <- YEH NAHI
```

System B ka average behtar hai, lekin woh 50 µs wala moment — usme aapka trade
miss ho jaayega, aur us ek moment mein aap paisa haar denge.

C++ mein koi garbage collector nahi hai. Koi JIT warm-up nahi. Koi interpreter nahi.
Jo aap likhte ho, wahi chalta hai — **har baar**.

### 2. Memory layout pe poora control

```cpp
struct Order {
    uint64_t id;        // 8 bytes
    uint32_t price;     // 4 bytes
    uint32_t quantity;  // 4 bytes
};                       // total: exactly 16 bytes, ek cache line ka chauthai
```

Aap **exactly** decide kar sakte ho ki data memory mein kaise pada hoga.
Java/Python mein aap sirf request kar sakte ho, guarantee nahi mil sakti.

Yeh matter kyun karta hai? Kyunki **cache misses hi HFT ke sabse bade dushman hain**
(folder 32 mein detail).

### 3. Zero-cost abstractions

Aap saaf, readable code likh sakte ho, aur compiler use aisi machine code mein badal
deta hai jaise aapne haath se assembly likhi ho.

```cpp
// Yeh saaf code...
for (const auto& order : orders) {
    process(order);
}
// ...compile hokar bilkul waisa hi banta hai jaise aap manually pointer loop likhte
```

### 4. No runtime, no dependencies

C++ binary chal jaati hai. Bas. Koi JVM nahi, koi Python interpreter nahi, koi
runtime install nahi karna.

### 5. Ecosystem aur legacy

Poora finance infrastructure C++ mein hai. Exchange APIs, FIX engines, market data
libraries — sab C++ mein.

---

## C++ ki kamzoriyaan (honest baat)

Main aapse jhooth nahi bolunga. C++ ke problems hain:

| Problem | Detail |
|---|---|
| **Seekhne mein mushkil** | Sabse complex mainstream language. Standard 2000+ pages ka hai |
| **Undefined Behaviour** | Ek chhoti galti aur kuch bhi ho sakta hai — crash, galat answer, ya "chalta hua dikhna" |
| **Memory safety nahi** | Buffer overflow, use-after-free, dangling pointers — sab possible |
| **Slow compile times** | Bade projects mein ghanton lag sakte hain |
| **Backward compatibility ka bojh** | 1980s ka C code aaj bhi valid hai, isliye purane wart bache hue hain |
| **Error messages** | Template errors 500 lines ke ho sakte hain (C++20 concepts se behtar hua hai) |

Rust in problems ko solve karta hai. Lekin HFT mein abhi C++ hi standard hai —
ecosystem, legacy code, aur hiring pool ki wajah se.

---

## C++ ke "paradigms" — ek preview

C++ mein aap 4 alag styles mein likh sakte ho:

### 1. Procedural (C jaisa)
```cpp
int add(int a, int b) { return a + b; }
```

### 2. Object-Oriented
```cpp
class Order {
    double price_;
public:
    double getPrice() const { return price_; }
};
```

### 3. Generic (templates)
```cpp
template <typename T>
T maximum(T a, T b) { return (a > b) ? a : b; }
```

### 4. Functional
```cpp
std::sort(v.begin(), v.end(), [](int a, int b) { return a > b; });
```

Aap sab seekhoge. Sab ki apni jagah hai.

> **HFT note:** HFT codebases aksar **kam OOP, zyada templates aur plain structs**
> use karti hain — kyunki virtual function calls mehnge hote hain aur data layout
> pe control chahiye. Isko "data-oriented design" kehte hain (folder 32).

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "C++ = C + classes" | 1983 mein sach tha. Aaj C++ bilkul alag language hai |
| "C++ purani hai, dead hai" | C++20/23/26 active development mein hain. TIOBE mein top 3 |
| "C++ sikhne se pehle C seekho" | **Nahi.** Yeh purana advice hai. Seedha modern C++ seekho |
| "C++ mein manual memory management zaroori hai" | Modern C++ mein RAII + smart pointers se `new`/`delete` shayad hi likhna pade |
| "Rust ne C++ ko replace kar diya" | Abhi nahi, HFT mein bilkul nahi. Shayad 10 saal mein |

---

## Exercises

1. `g++ --version` chalao. Aapke paas GCC ka kaunsa version hai?
   [GCC C++ support table](https://gcc.gnu.org/projects/cxx-status.html) dekho — kya aapka
   version C++20 support karta hai?

2. Test karo ki aapka compiler kaunsa standard use kar raha hai:
   ```bash
   cat > std.cpp << 'END'
   #include <iostream>
   int main() { std::cout << __cplusplus << "\n"; }
   END
   g++ std.cpp -o std && ./std
   g++ -std=c++20 std.cpp -o std && ./std
   ```
   <details><summary>Expected</summary>
   Bina flag: `201703` (C++17, modern GCC ka default) ya `201402`.
   `-std=c++20` ke saath: `202002`.
   `__cplusplus` ek predefined macro hai jo standard version batata hai.
   </details>

3. Google karo: "companies using C++ in India HFT". Kaunsi firms mili? (Quadeye,
   Graviton, Tower Research India, Optiver India, WorldQuant, Alphagrep, Estee...)

4. Apne shabdon mein likho: "zero-cost abstraction" ka matlab kya hai, aur woh HFT ke
   liye kyun important hai?

---

## Next
→ [`07-editor-ide-terminal.md`](07-editor-ide-terminal.md)

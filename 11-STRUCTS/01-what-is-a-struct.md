# 01 — Struct kya hai

## Prerequisites
- `09-ARRAYS/`, `10-STRINGS/`
- `08-FUNCTIONS/03-parameters-and-arguments.md` (pass by `const&`), `04-return-values.md` (RVO)

## Yeh folder kyun
Ab tak har variable akela tha. Struct **related data ko ek unit mein** bandhta
hai: ek `Order` ke andar `id`, `symbol`, `price`, `qty` — sab ek naam ke peeche.

Aur is folder mein **padding aur alignment** poora samjhenge — jo HFT mein cache
efficiency ka sabse bada lever hai (member reorder karke struct ka size aadha,
files 05, 07).

---

## Struct — ek naya type

```cpp
struct Point {
    double x;
    double y;
};   // <-- SEMICOLON zaroori (type define kar rahe ho -- folder 02 file 06 se)
```

`Point` ab ek **type** hai, `int` / `double` ki tarah. Uske instances bana sakte ho:

```cpp
Point p;              // ⚠️ members UNINITIALIZED (garbage -- local array jaisa)
Point q{3.0, 4.0};    // aggregate init -- members declaration ke order mein
```

Analogy: struct ek **form** ka design hai (naam, roll number, marks ke khaane). `Point p;` us form ki
ek khaali copy hai — khaano mein abhi kuch bhi likha ho sakta hai (garbage), jab tak aap bharo nahi.

### Members access — dot operator

```cpp
p.x = 3.0;
p.y = 4.0;
double d = p.x;
p.x += 1.0;
```

`p.x` = "`p` ka `x` member". Members badal sakte ho (jab tak `const` na ho).

---

## Struct kyun

```cpp
// ❌ Bina struct -- jude hue data bikhre pade hain
double x1, y1;
double x2, y2;
void move(double& x, double& y, double dx, double dy);   // bhaari

// ✅ Struct -- ek unit
struct Point { double x, y; };
Point a, b;
void move(Point& p, Point delta);
```

- **Grouping** — jo cheezein saath rehti hain, unka ek naam
- **Ek saath pass** — function ko `Point` do, `x` aur `y` alag-alag nahi
- **Ek saath return** — `Point midpoint(Point, Point)`
- **Records ka array** — `std::vector<Order>`, na ki `vector<double> prices; vector<int> qtys;`
  (haan, kabhi-kabhi SoA behtar hota hai — file 07)
- **Documentation** — `order.isBuy` akele `bool` se kahin zyada saaf

---

## Value semantics — copy matlab poori copy

```cpp
struct Order { std::uint64_t id; std::string symbol; double price; std::int64_t qty; };

Order a{1, "AAPL", 192.34, 100};
Order b = a;                     // POORI copy -- id, symbol (lambi ho to string alloc), price, qty
b.qty = 999;
// a.qty abhi bhi 100 -- b alag hai
```

Struct ka assignment / copy-construction = **member-by-member copy** (compiler khud banata hai; folder
18 mein ise customize karenge). By value pass karna → copy. Bade structs → `const&` se pass karo (file 04).

---

## Functions aur structs

```cpp
double distance(const Point& a, const Point& b) {        // const& se -- copy nahi
    double dx = a.x - b.x, dy = a.y - b.y;
    return std::sqrt(dx*dx + dy*dy);
}

Point midpoint(const Point& a, const Point& b) {
    return { (a.x + b.x)/2, (a.y + b.y)/2 };             // value se return -- RVO, free
}

void scale(Point& p, double k) { p.x *= k; p.y *= k; }   // ref se -- caller ka badalta hai
```

---

## `sizeof` — members + padding

```cpp
struct Point { double x, y; };
sizeof(Point)                   // 16  (8 + 8)

struct Mixed { char c; double d; int i; };
sizeof(Mixed)                   // 24, 13 NAHI -- padding (file 05)
```

Members memory mein ek ke baad ek hote hain (declaration order mein), par compiler alignment ke liye
beech mein **khaali jagah (padding)** daal deta hai. File 05 mein poori baat — aur reorder karke bachat.

---

## Andar kya hota hai

- Struct bas uske members hain jo memory mein **declaration order** mein rakhe gaye hain, beech mein
  padding ke saath taaki har member apni alignment pe baithe (file 05).
- `p.x` → `load [p ka address + x ka offset]` — ek fixed compile-time offset. Akele variable jitni hi cost.
- Copy → trivial members ke liye `sizeof` bytes ka `memcpy`; non-trivial (jaise `std::string`) ke liye
  member-by-member.
- Plain struct mein koi chhupa data nahi — na vtable, na header (woh `virtual` wali `class` mein hota
  hai, folder 16). `sizeof` bilkul layout jitna.

> **HFT relevance:** HFT mein structs hi data model *hain* — order records, book levels, wire messages,
> market-data snapshots. Kyunki plain struct ka layout bilkul uske members jitna hai (koi chhupa kharcha
> nahi), aap use `memcpy` kar sakte ho, uske `sizeof` pe `static_assert` laga sakte ho, network buffer
> pe map kar sakte ho, aur theek-theek soch sakte ho ki kaunse bytes kaunsi cache line mein aayenge.
> Files 05–08 isi layout ko control karne ke baare mein hain.

---

## Hands-on

`examples/01_struct_basics.cpp` — instances, dot access, `const&` params, value se return,
`vector<Point>`, `sizeof`:

```bash
./build.ps1 11-STRUCTS/examples/01_struct_basics.cpp
```

---

## ⚠️ Traps

### Trap 1 — `}` ke baad semicolon bhoolna
```cpp
struct P { int x; }        // ❌ ERROR (aksar AGLI line ki taraf ishaara karta hai)
int main() { ... }
```

### Trap 2 — uninitialized members
```cpp
Point p;                   // members garbage. Point p{}; -> zero
```

### Trap 3 — bada struct by value
```cpp
void f(BigStruct s);       // ⚠️ har call pe poori copy. const BigStruct& lo
```

### Trap 4 — structs ko `==` se compare karna
```cpp
if (a == b) { }            // ❌ default mein ERROR (operator== nahi hai). = default karo (folder 18) ya khud likho
```
`= default` sirf **member ya friend** ki tarah chalta hai — free function ki tarah nahi (Exercise 5).

### Trap 5 — `sizeof` ko members ka jod samajhna
```cpp
struct M { char c; int i; };  sizeof(M) == 8, 5 nahi   // padding (file 05)
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Struct ke members apne aap initialize hote hain" | Local: garbage. Zero ke liye `P p{};` |
| "`b = a` data share karta hai" | Poori member-by-member copy (alag-alag) |
| "Struct by value pass karna theek hai" | Copy hota hai — bade ke liye `const&` |
| "`sizeof(struct)` = members ke size ka jod" | + padding (file 05) |
| "Plain struct mein chhupa kharcha hai" | Nahi — layout bilkul members jitna |

---

## Exercises

1. **Define + use:** `struct Rect { double w, h; };` — `area(const Rect&)`, `Rect scaled(const Rect&,
   double)`, `bool isSquare(const Rect&)`. Test karo.

2. **Record:** `struct Student { std::string name; int rollNo; double gpa; };` — ek
   `std::vector<Student>` banao, sabse zyada GPA wala print karo.

3. **Value semantics:** `Student a = ...; Student b = a; b.gpa = 4.0;` — `a.gpa` badla? Kyun?

4. **sizeof surprise:** `struct A { char c; double d; };` aur `struct B { double d; char c; };` — dono ka
   `sizeof`. Same? (Jhalak — file 05.)

5. **`==`:** `if (rect1 == rect2)` try karo. Error aaya? Ab struct ke andar
   `bool operator==(const Rect&) const = default;` (C++20) jodo — ab?
   <details><summary>Answer</summary>

   Pehle: error — `Rect` ke liye `operator==` hai hi nahi. Member version jodne ke baad chal jaata hai,
   member-by-member compare. Dhyaan: agar yahi line struct ke **bahar** free function ki tarah likhoge —
   `bool operator==(const Rect&, const Rect&) = default;` — to GCC 16.2 error deta hai: `defaulted
   'bool operator==(...)' is not a friend of 'Rect'` (chala ke dekha). Defaulted comparison ya to member
   ho, ya struct ke andar `friend` declare ho.
   </details>

6. **By value cost:** `struct Big { double data[100]; };` — `void f(Big)` vs `void f(const Big&)`, 1M
   calls, `-O2`, time lo.

---

## Interview questions

1. Struct kya hai? `sizeof(struct)` kaise decide hota hai?
2. Struct copy / assignment kya karta hai (member-wise)?
3. Struct ko function ko kaise pass karo (big vs small)?
4. Plain struct mein koi hidden data hota hai? (`class` + `virtual` se fark?)
5. Struct return by value — cost? (RVO)
6. `p.x` access ki cost?

---

## Next
→ [`02-struct-initialization.md`](02-struct-initialization.md)

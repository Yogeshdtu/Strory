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

`Point` ab ek **type** hai, `int` / `double` ki tarah. Uske instances bana sakte
ho:

```cpp
Point p;              // ⚠️ members UNINITIALIZED (garbage -- like a local array)
Point q{3.0, 4.0};    // aggregate init -- members declaration order mein
```

### Members access — dot operator

```cpp
p.x = 3.0;
p.y = 4.0;
double d = p.x;
p.x += 1.0;
```

`p.x` = "`p` ka `x` member". Members mutable hain (jab tak `const` na ho).

---

## Kyun struct

```cpp
// ❌ Bina struct -- related cheezein bikhri hui
double x1, y1;
double x2, y2;
void move(double& x, double& y, double dx, double dy);   // awkward

// ✅ Struct -- ek unit
struct Point { double x, y; };
Point a, b;
void move(Point& p, Point delta);
```

- **Grouping** — jo cheezein saath rehti hain, ek naam
- **Pass as one** — function ko `Point` do, `x` aur `y` alag-alag nahi
- **Return as one** — `Point midpoint(Point, Point)`
- **Array of records** — `std::vector<Order>` not `vector<double> prices; vector<int> qtys;`
  (well, sometimes SoA — file 07)
- **Documentation** — `order.isBuy` >> a bare `bool`

---

## Value semantics — copy is a full copy

```cpp
struct Order { std::uint64_t id; std::string symbol; double price; std::int64_t qty; };

Order a{1, "AAPL", 192.34, 100};
Order b = a;                     // FULL copy -- id, symbol (string alloc if long), price, qty
b.qty = 999;
// a.qty still 100 -- b is independent
```

Struct assignment / copy-construction = **member-by-member copy** (compiler-
generated, folder 18 mein customize). Passing by value → copy. Big structs →
pass by `const&` (file 04).

---

## Functions with structs

```cpp
double distance(const Point& a, const Point& b) {        // by const& -- no copy
    double dx = a.x - b.x, dy = a.y - b.y;
    return std::sqrt(dx*dx + dy*dy);
}

Point midpoint(const Point& a, const Point& b) {
    return { (a.x + b.x)/2, (a.y + b.y)/2 };             // return by value -- RVO, free
}

void scale(Point& p, double k) { p.x *= k; p.y *= k; }   // by ref -- modifies caller's
```

---

## `sizeof` — members + padding

```cpp
struct Point { double x, y; };
sizeof(Point)                   // 16  (8 + 8)

struct Mixed { char c; double d; int i; };
sizeof(Mixed)                   // 24, NOT 13 -- padding (file 05)
```

Members contiguous hote hain (declaration order), par compiler alignment ke liye
**gaps (padding)** daal deta hai. File 05 mein poora — aur reorder se save.

---

## Andar kya hota hai

- A struct is just its members laid out in memory in **declaration order**, with
  padding inserted so each member is naturally aligned (file 05).
- `p.x` → `load [address_of_p + offset_of_x]` — a fixed compile-time offset. Same
  cost as a bare variable.
- Copy → `memcpy` of `sizeof` bytes (trivial members) or member-wise (non-trivial
  like `std::string`).
- No hidden data for a plain struct — no vtable, no header (that's `class` with
  `virtual`, folder 16). `sizeof` is exactly the layout.

> **HFT relevance:** Structs *are* the data model in HFT — order records, book
> levels, wire messages, market-data snapshots. Because a plain struct's layout
> is exactly its members (no hidden overhead), you can `memcpy` it, `static_assert`
> its `sizeof`, map it onto a network buffer, and reason about exactly which
> bytes land in a cache line. Files 05–08 are all about controlling that layout.

---

## Hands-on

`examples/01_struct_basics.cpp` — instances, dot access, `const&` params, return
by value, `vector<Point>`, `sizeof`:

```bash
./build.ps1 11-STRUCTS/examples/01_struct_basics.cpp
```

---

## ⚠️ Traps

### Trap 1 — missing semicolon after `}`
```cpp
struct P { int x; }        // ❌ ERROR (often points at the NEXT line)
int main() { ... }
```

### Trap 2 — uninitialized members
```cpp
Point p;                   // members garbage. Point p{}; -> zero
```

### Trap 3 — big struct by value
```cpp
void f(BigStruct s);       // ⚠️ full copy per call. const BigStruct&
```

### Trap 4 — comparing structs with `==`
```cpp
if (a == b) { }            // ❌ ERROR by default (no operator==). = default it (folder 18) or write it
```

### Trap 5 — expecting `sizeof` == sum of members
```cpp
struct M { char c; int i; };  sizeof(M) == 8, not 5   // padding (file 05)
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Struct ke members initialize hote hain" | Local: garbage. `P p{};` for zero |
| "`b = a` shares data" | Full member-wise copy (independent) |
| "Struct pass by value is fine" | Copies — `const&` for big ones |
| "`sizeof(struct)` = sum of member sizes" | + padding (file 05) |
| "Plain struct has hidden overhead" | No — layout is exactly its members |

---

## Exercises

1. **Define + use:** `struct Rect { double w, h; };` — `area(const Rect&)`,
   `Rect scaled(const Rect&, double)`, `bool isSquare(const Rect&)`. Test.

2. **Record:** `struct Student { std::string name; int rollNo; double gpa; };` —
   a `std::vector<Student>`, print the top-GPA one.

3. **Value semantics:** `Student a = ...; Student b = a; b.gpa = 4.0;` — is
   `a.gpa` changed? Why?

4. **sizeof surprise:** `struct A { char c; double d; };` and `struct B { double
   d; char c; };` — `sizeof` of each. Same? (Preview — file 05.)

5. **`==`:** try `if (rect1 == rect2)`. Error? Add `bool operator==(const Rect&,
   const Rect&) = default;` (C++20) — now?

6. **By value cost:** `struct Big { double data[100]; };` — `void f(Big)` vs
   `void f(const Big&)`, 1M calls, `-O2`, time.

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

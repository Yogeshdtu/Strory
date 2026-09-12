# 14 — Folder 12 Revision + Exercises

## Prerequisites
Lessons 01–13 aur saare 8 examples chalaye hue.

---

## PART A — Concept check

1. Pointer kya hai? `p`, `*p`, `&p` mein fark?
2. `int*` aur `char*` ka size — same? Kyun? Type kya batata hai?
3. `&x` — kis cheez ka le sakte ho? `&` ke 3 meanings?
4. `nullptr` vs `NULL` vs `0` — 3 problems with `NULL`/`0`?
5. Null dereference — kya hota hai? Address 0 unmapped kyun?
6. `p + 1` — kitne bytes? `p2 - p1` — bytes ya elements?
7. One-past-the-end pointer — form / compare / deref?
8. OOB pointer arithmetic — even without deref?
9. Array vs pointer — 4 differences? Decay?
10. `const int* p`, `int* const p`, `const int* const p` — kya lock?
11. `p->x` == ? Kab `->`, kab `.`?
12. Struct khud ko contain kar sakta hai? Kaise?
13. `int**` — `*pp`, `**pp`? Size? Cost (dependent loads)?
14. `void*` — deref / arithmetic kyun nahi? `T*<->void*` conversions?
15. Function pointer — declare, parens kyun? Indirect call cost?
16. Capture-less vs capturing lambda — `fp` conversion?
17. Dangling pointer — 5 ways? Read vs write danger?
18. `delete p` ke baad `p`? Best practice?
19. Container reallocation kya invalidate karta hai?
20. Pointer bugs ke against modern C++ defense (RAII, tools)?

---

## PART B — Output prediction

### B1
```cpp
int x = 10;
int* p = &x;
*p = 20;
x = 30;
std::cout << *p;
```
<details><summary>Answer</summary>`30` — `p` aur `x` ek hi memory ko alias karte hain.</details>

### B2
```cpp
int a[4] = {10, 20, 30, 40};
int* p = a;
std::cout << *(p + 2) << " " << p[3] << " " << (a + 3) - (a + 1);
```
<details><summary>Answer</summary>`30 40 2`</details>

### B3
```cpp
int a[3] = {1, 2, 3};
int* p = a;
int x = *p++;
int y = *p;
std::cout << x << " " << y;
```
<details><summary>Answer</summary>`1 2` — postfix `++` purana `p` return karta hai (→ `a[0]`), phir aage badhta hai; ab `*p` `a[1]` hai.</details>

### B4
```cpp
int x = 5, y = 9;
const int* p = &x;
p = &y;
std::cout << *p;
// *p = 1;   // kya yeh compile hoga?
```
<details><summary>Answer</summary>`9`. `*p = 1;` compile NAHI hoga (pointee const hai); dobara point karana theek hai.</details>

### B5
```cpp
struct N { int v; N* next; };
N c{3, nullptr}, b{2, &c}, a{1, &b};
std::cout << a.next->next->v;
```
<details><summary>Answer</summary>`3`</details>

### B6
```cpp
int v = 7;
int* p = &v;
int** pp = &p;
**pp = 42;
std::cout << v << " " << *p;
```
<details><summary>Answer</summary>`42 42`</details>

### B7
```cpp
int add(int a, int b) { return a + b; }
int (*op)(int, int) = add;
std::cout << op(3, 4) << " " << (*op)(10, 2) << " " << sizeof(op);
```
<details><summary>Answer</summary>`7 12 8`</details>

### B8
```cpp
int* dangler() { int x = 99; return &x; }
int* p = dangler();
std::cout << "reached";
```
<details><summary>Answer</summary>`reached` print ho jaayega — par `p` dangling hai; koi bhi `*p` UB hai. `-Wreturn-local-addr` warning deta hai.</details>

---

## PART C — Find the bug

### C1
```cpp
int* p;
*p = 42;
```
<details><summary>Answer</summary>Uninitialized (wild) pointer — kisi garbage address pe likh raha hai. `int* p = &x;` karo, ya `nullptr` + guard.</details>

### C2
```cpp
int* makeValue() {
    int result = compute();
    return &result;
}
```
<details><summary>Answer</summary>`&local` return kar raha hai → return hote hi dangling. Value se return karo (`int`).</details>

### C3
```cpp
int* p = new int(5);
delete p;
delete p;
```
<details><summary>Answer</summary>Double-free → heap corruption. Pehle `delete` ke baad `p = nullptr` karo; ya `std::unique_ptr` use karo (usse double-free ho hi nahi sakta).</details>

### C4
```cpp
std::vector<int> v = {1, 2, 3};
int* first = &v[0];
v.push_back(4);
std::cout << *first;
```
<details><summary>Answer</summary>`push_back` realloc kar sakta hai → `first` dangle ho jaata hai. Pehle `v.reserve(4)` karo, ya index use karo.</details>

### C5
```cpp
int arr[5];
int* end = &arr[5];
for (int* p = arr; p <= end; ++p) *p = 0;
```
<details><summary>Answer</summary>`p <= end` `arr[5]` (one-past-end) ko deref kar deta hai → OOB. `p != end` (ya `p < end`) likho.</details>

### C6
```cpp
void connect(int* fd) { *fd = ::socket(...); }
int fd;
connect(&fd);
```
<details><summary>Answer</summary>Yeh khud mein bug nahi hai — par `int*` output param C-style hai. `int& fd` (folder 13) zyada saaf hai aur null ho hi nahi sakta. (Agar `connect` null check na kare, to `connect(nullptr)` crash karega.)</details>

### C7
```cpp
char* greeting() {
    char buf[32];
    std::strcpy(buf, "hello");
    return buf;
}
```
<details><summary>Answer</summary>Local buffer ka pointer return kar raha hai (decay + local lifetime) → dangling. `std::string` return karo, ya caller ka buffer bharo.</details>

### C8
```cpp
int* a = new int[10];
delete a;
```
<details><summary>Answer</summary>`new[]` ki jodi `delete[]` ke saath hi banti hai. `delete[] a;` likho — ya `std::vector<int>` / `std::unique_ptr<int[]>` use karo.</details>

---

## PART D — Practical tasks

### D1. Pointer-based dynamic array (aur phir usse phenk dena)
Ek `DynArray` banao: `int* data; size_t size, cap;` ke saath `push_back` (jo
`new[]` + copy + `delete[]` se badhe), `operator[]`, aur destructor. Use
leak-free aur UAF-free karo. Phir dhyaan do: `std::vector` yeh sab pehle se sahi
karta hai — asli code mein aisa kabhi mat likhna.

### D2. Singly linked list
`struct Node { int v; Node* next; };` — `push_front`, `push_back`, `find`,
`erase(value)`, `reverse`, `clear` (har node free karo). Har ek ko test karo.
ASan ke neeche chalao (Linux) — zero leaks, zero UAF.

### D3. Function-pointer dispatch table
Ek "opcode VM" banao: `enum Op { Push, Add, Sub, Mul, Print };` aur ek
`std::array<void(*)(Stack&), N>` dispatch table. Ek chhota program chalao
`{Push 3, Push 4, Add, Print}`. Phir code size aur dispatch style ko `switch`
se compare karo.

### D4. Const-correct buffer API
```cpp
void fill(int* buf, size_t n, int value);
long long sum(const int* buf, size_t n);
void copy(int* dst, const int* src, size_t n);
```
Har ek ko mutable aur `const` arrays dono ke saath call karo — likho ki kaunse
combinations compile hue. Phir inhe `std::span<int>` / `std::span<const int>` se
dobara likho.

### D5. Pointer diagram tracer
`07_pointer_diagrams.cpp` ko aage badhao: operations ki ek script do (`p = &a`,
`*p = 5`, `p = &b`, `**pp = 1`, ...) aur har step ke baad memory table print
karo. `int`, `int*`, `int**` teeno handle karo.

### D6. Dangling-pointer museum
Lesson 13 ke catalog ka har bug ek live demo ki tarah (jahan hang ho sakta ho
wahan safety cap laga do): buggy version + woh tool/warning jo use pakadta hai
(exact text, ya "Linux pe ASan") + fixed version + ek aisa input jahan galti
saaf dikhti ho. `06_dangling_pointer.cpp` ko extend karo.

### D7. Ownership audit
Koi chhota module lo jo raw `new`/`delete` use karta ho. Uski ownership
`std::unique_ptr` / `std::vector` se dobara likho (folder 14/17 ki jhalak).
Non-owning access ke liye `T*` (borrow) ya `T&` / `std::span` use karo. Result
mein `delete` ek bhi nahi hona chahiye.

---

## PART E — Self-assessment

```
[ ] p / *p / &p, pointer = ek address, type = deref ki width -- saaf hai
[ ] &x (address-of), & ke 3 matlab, lvalue ki zaroorat
[ ] * (deref) vs * (declaration), *p ek lvalue hai
[ ] sirf nullptr (NULL/0 nahi), null-deref = design se segfault
[ ] p + n sizeof(*p) se scale hota hai; arr[i] == *(arr+i)
[ ] valid range: [begin, end]; OOB arithmetic = UB
[ ] array vs pointer: sizeof, &, assignability, decay
[ ] const int* vs int* const vs const int* const
[ ] p->x == (*p).x ; pointers ke liye ->, objects ke liye .
[ ] linked structures = struct jisme apna hi pointer ho
[ ] int** : do dependent loads, "pointer ko modify karo" wala case, int*& alternative
[ ] void* : na deref na arithmetic, use karne ke liye cast, inlining maar deta hai
[ ] function pointers: declaration, indirect call, bina capture lambda -> fp
[ ] virtual dispatch = compiler ki banayi function-pointer table
[ ] dangling: 5 tareeke, read vs write ka khatra, delete ke baad nullptr
[ ] pointer bug catalog + detection tools + RAII defense
[ ] saare 8 examples chalaye
```

**Scoring:**
- **14–17** → Folder 13 (References). 🎯
- **9–13** → Files 05, 07, 09, 12, 13 dobara.
- **< 9** → Poora folder, har example, memory diagrams khud banao.

---

## PART F — Challenge

### Challenge 1: "Intrusive doubly-linked list + free list"

```cpp
struct Order {
    std::uint64_t id;
    std::int64_t  price, qty;
    Order* prev = nullptr;
    Order* next = nullptr;
};

class OrderQueue {          // ek price level ki FIFO -- O(1) add/cancel, koi allocation nahi
    Order* head_ = nullptr;
    Order* tail_ = nullptr;
public:
    void pushBack(Order* o);
    void unlink(Order* o);          // `o` jahan bhi ho, wahan se hatao -- O(1)
    Order* popFront();
    bool empty() const { return head_ == nullptr; }
};
```

Storage: ek preallocated `std::array<Order, N>` pool + ek free list (jo `next` ke
zariye judi ho). Add/cancel/execute sirf pointers ko chhuein — **startup ke baad
heap bilkul nahi**. ASan ke neeche chalao (Linux): zero leaks, zero UAF, zero
dangling. Yahi folder 39 ka beej hai.

### Challenge 2: "Pointer bug museum"

Lesson 13 ka poora catalog ek instrumented program ki tarah (D6 dekho). Usme
`-fsanitize=address,undefined` ka run log (Linux/WSL se) daalo jo dikhaye ki
kaunsa tool kya pakadta hai, aur ek `-D_GLIBCXX_ASSERTIONS` ka run jo dikhaye ki
MinGW fallback kya pakadta hai.

### Challenge 3: "Mini `std::function` (cost samjho)"

`Fn<int(int)>` ko teen — asal mein chaar — tareeke se banao aur har ek ka 10M-call
loop `-O2` pe benchmark karo:
1. `int (*)(int)` — raw function pointer
2. ek small-buffer type-erased callable (`std::function` jaisa, par bina heap ke)
3. `std::function<int(int)>`
4. ek template parameter `F` (compiler ko concrete type dikhta hai)

Table banao: ns/call, inline hua ya nahi (`-S` se dekho), heap allocate hua ya
nahi. Phir samjhao ki HFT hot paths #4 (templates / CRTP) kyun use karte hain
aur #3 se kyun bachte hain.

---

## 🎉 Folder 12 complete

Pointers: `&`/`*`, `nullptr`, arithmetic (scaled, in-bounds), arrays↔pointers,
`const` combinations, `->`, `int**`, `void*`, function pointers (→ virtual
dispatch), dangling / use-after-free, aur poora bug catalog tooling ke saath.

Yeh "asli C++" ka darwaza tha. Agla: **references** — pointers ka safer, cleaner
cousin (na null, na re-bind, syntax bhi saaf).

---

## Next
→ [`../13-REFERENCES/00-README.md`](../13-REFERENCES/00-README.md)

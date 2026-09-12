# 08 — Pointers to structs — the `->` operator

## Prerequisites
- [`03-dereferencing.md`](03-dereferencing.md), [`04-nullptr.md`](04-nullptr.md)
- `11-STRUCTS/` (structs, layout)

## Yeh topic abhi kyun
Struct ka pointer + member access = `->`. Yeh linked lists, trees, aur har
pointer-based data structure ka rozmarra syntax hai.

---

## `p->member` ≡ `(*p).member`

```cpp
struct Order { std::uint64_t id; double price; std::int64_t qty; };

Order  ord{1001, 192.34, 100};
Order* p = &ord;

(*p).id        // 1001  -- pehle deref, phir .member
p->id          // 1001  -- chhota roop, yahi normally likhte hain
p->price = 193.00;   // pointer ke zariye likh bhi sakte ho
p->qty += 50;
```

`->` ki **definition hi** `(*p).` hai. Hamesha `->` use karo — parens + `.` bekaar
mein bhaari lagta hai.

---

## Struct ko pointer se pass karna

```cpp
void applyFill(Order* o, std::int64_t fillQty) {
    if (o == nullptr) return;        // ⚠️ pehle guard
    o->qty -= fillQty;
}

void printOrder(const Order* o) {    // sirf padhna hai -> const Order*
    if (!o) return;
    std::cout << o->id << " " << o->price << "\n";
    // o->qty = 0;   // ❌ ERROR -- const Order* hai
}
```

`Order*` → caller ka struct badal sakte ho. `const Order*` → sirf padh sakte ho.
(Jab pointer kabhi null ho hi nahi sakta, tab `Order&` reference — folder 13 —
zyada saaf choice hai.)

---

## Linked structures — struct jo struct pe point kare

```cpp
struct Node {
    int   value;
    Node* next;         // doosre Node ka pointer -- YAHI cheez list banati hai
};

Node n3{3, nullptr};
Node n2{2, &n3};
Node n1{1, &n2};

// list pe chalo
for (Node* cur = &n1; cur != nullptr; cur = cur->next)
    std::cout << cur->value << " ";      // 1 2 3

n1.next->next->value      // 3  -- chained ->
```

```
   n1 [1|•]──▶ n2 [2|•]──▶ n3 [3|null]
```

Trees: `struct TreeNode { int v; TreeNode* left; TreeNode* right; };`

⚠️ Ek `struct` apne aap ko **value ke roop mein** contain nahi kar sakta (size
infinite ho jaati) — par apne aap ka **pointer** rakh sakta hai. Isi wajah se
lists aur trees pointers pe bane hote hain.

---

## `this` — struct ka chhupa hua pointer (jhalak, folder 15)

Member function ke andar `this` current object ka pointer hota hai:

```cpp
struct Counter {
    int n = 0;
    void inc() { this->n++; }     // this->n  ==  n
    Counter* self() { return this; }
};
```

Asal mein member functions andar se `f(Counter* this, ...)` jaise hi hote hain.

---

## Andar kya hota hai

- `p->member` → `load [p + offset_of_member]` — ek add (jo addressing mode mein
  fold ho jaata hai) + ek load. Cost `struct.member` jitni hi, plus pointer
  follow karne ka.
- Chained `a->b->c` → do **dependent** loads (har ek pichhle ka intezaar karta
  hai) — ise **pointer-chase** kehte hain. Agar `b` aur `c` alag cache lines mein
  hain → do possible cache misses, woh bhi ek ke baad ek. Linked lists traverse
  karne mein isi wajah se slow hoti hain (folder 09 file 10, folder 32).
- Null `p->member` → `load [0 + offset]` → fault.

> **HFT relevance:** Order books har price level ki FIFO queue ke liye
> **intrusive linked lists** use karte hain (har `Order` ke andar `Order* next` /
> `prev`), jisse insert/cancel O(1) ho jaata hai bina kisi allocation ke. Par
> traversal ek pointer-chase hai (cache misses) — isliye *hot* path (best
> bid/ask, top-of-book) price levels ke flat array pe chalta hai, aur list sirf
> match ke waqt walk hoti hai. Matlab: validated non-null pointer pe `->` free
> hai; asli cost chase ki memory latency aur null-safety ki discipline hai.
> Folder 39.

---

## Hands-on

`examples/04_pointers_structs.cpp` — `->` vs `(*p).`, pointer params (const aur
non-const), aur ek linked-list walk:

```bash
./build.ps1 12-POINTERS/examples/04_pointers_structs.cpp
```

---

## ⚠️ Traps

### Trap 1 — pointer pe `.` lagana
```cpp
Order* p = &ord;  p.id = 1;   // ❌ ERROR -- p->id  (ya (*p).id)
```

### Trap 2 — non-pointer pe `->` lagana
```cpp
Order ord;  ord->id;   // ❌ ERROR -- ord.id  (ord pointer nahi hai)
```

### Trap 3 — bina null check ke `->`
```cpp
Node* n = find(key);   // nullptr ho sakta hai
n->value;              // ⚠️ miss pe crash
```

### Trap 4 — dangling `->`
```cpp
Node* p;
{ Node local{1, nullptr}; p = &local; }
p->value;              // ⚠️ local khatam ho chuka -> UB
```

### Trap 5 — `a->b->c` jahan `b` null ho sakta hai
```cpp
cfg->net->socket;      // ⚠️ agar cfg->net null hua -> crash. Har hop check karo
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`p->x` aur `(*p).x` alag hain" | Bilkul same — `->` sirf shorthand hai |
| "Struct pointer pe `.` lagao" | Pointers ke liye `->`, objects/references ke liye `.` |
| "`->` bounds/null check karta hai" | Nahi — null/dangling `p` → UB |
| "Struct khud ko contain kar sakta hai" | Sirf apne *pointer* ke zariye |
| "Chained `->` free hai" | Har hop ek dependent load hai (pointer-chase, cache misses) |

---

## Exercises

1. **`->` basics:** `struct P { int x, y; }; P p{3, 4}; P* pp = &p;` — `pp->x`,
   `(*pp).y` print karo; `pp->x = 30` set karo; phir `p.x` print karo.

2. **Linked list:** stack pe 4-node ki `Node` list banao; `int listSum(const Node*
   head)` likho (iterative). Phir `int listLen(const Node*)`.

3. **Recursive tree:** `struct T { int v; T* l; T* r; };` — ek chhota tree banao;
   `int treeSum(const T*)` recursively; aur `int height(const T*)`.

4. **Null hops:** `struct A { B* b; }; struct B { C* c; }; struct C { int v; };`
   — `a->b->c->v` safely nikalo jab koi bhi pointer null ho sakta ho.

5. **List ulti karo:** `Node* reverse(Node* head)` — saare `next` pointers palat
   do. Har step ke baad pointer state ka diagram banao.

6. **`this`:** `struct Acc { int total = 0; Acc& add(int n) { this->total += n;
   return *this; } };` — `acc.add(1).add(2).add(3);` chain karke chalao.

---

## Interview questions

1. `p->x` == ? Kab `->`, kab `.`?
2. Struct khud ko contain kar sakta hai? Kaise (list/tree)?
3. `a->b->c` ki cost — cache ke context mein?
4. `->` null/dangling `p` pe — kya?
5. `this` kya hai (member function ke andar)?
6. Intrusive linked list — kab use hoti hai, trade-off?

---

## Next
→ [`09-double-pointers.md`](09-double-pointers.md)

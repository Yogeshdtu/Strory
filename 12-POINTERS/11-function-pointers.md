# 11 — Function pointers

## Prerequisites
- [`03-dereferencing.md`](03-dereferencing.md)
- `08-FUNCTIONS/05-the-call-stack.md` (function ka bhi address hota hai), `08-FUNCTIONS/10-inline-functions.md`

## Yeh topic abhi kyun
Ek function ka bhi address hota hai (code segment mein). Function pointer us
address ko rakhta hai — **runtime pe "kaunsa function call karna hai"** decide
karo. Callbacks, dispatch tables, aur (andar se) virtual functions ka mechanism.

---

## Declare karna + use karna

```cpp
int add(int a, int b) { return a + b; }
int sub(int a, int b) { return a - b; }

int (*op)(int, int) = &add;    // & optional hai: `= add` bhi chalta hai
op(3, 4);                      // 7   -- (*op)(3, 4) bhi valid hai
op = sub;
op(10, 3);                     // 7

auto op2 = &add;               // zyada saaf -- type khud deduce ho jaata hai
using BinOp = int (*)(int, int);   // padhne mein aasaan banane ke liye
BinOp op3 = add;
```

Syntax hai: `return_type (*name)(param_types)`. `*name` ke aas-paas parens
**zaroori** hain (`int* f(int)` ek function hai jo `int*` return karta hai;
`int (*f)(int)` ek function ka pointer hai). Yahi wo jagah hai jahan log
sabse zyada phaste hain.

`sizeof(op)` == 8 (ek address hai).

---

## Callbacks — function ko argument ki tarah bhejna

```cpp
int apply(int x, int y, int (*fn)(int, int)) { return fn(x, y); }

apply(8, 5, add);      // 13
apply(8, 5, sub);      // 3

void forEach(const int* a, std::size_t n, void (*fn)(int)) {
    for (std::size_t i = 0; i < n; ++i) fn(a[i]);
}
forEach(data, n, [](int x) { std::cout << x*x << " "; });   // bina capture wala lambda -> fp
```

**Bina capture wala lambda function pointer mein convert ho jaata hai.** Par
*capture karne wala* lambda **nahi** hota (uske paas state hoti hai) — uske liye
`std::function` ya template use karo.

---

## Dispatch table — function pointers ka array

```cpp
using Handler = void (*)(const Message&);

Handler handlers[256] = {};
handlers['Q'] = handleQuote;
handlers['T'] = handleTrade;
handlers['H'] = handleHeartbeat;

void dispatch(const Message& m) {
    if (Handler h = handlers[m.type]) h(m);   // O(1) index -> indirect call
}
```

Jab key ek chhota integer ho, to function pointers ki indexed table `switch` ka
ek tez alternative hai (folder 06 file 05 — waise compiler `switch` ke liye
khud bhi aisi hi table bana sakta hai).

---

## Function pointer vs lambda vs `std::function`

| | `int (*fp)(int)` | bina capture lambda | capture wala lambda | `std::function<int(int)>` |
|---|---|---|---|---|
| State rakhta hai | ❌ | ❌ | ✅ | ✅ (koi bhi callable) |
| Size | 8 bytes | 1 byte (stateless) | captures jitna | ~32 bytes + shayad heap |
| Call ki cost | indirect call | inline / indirect | type pata ho to inline | **indirect + type-erasure** |
| `fp` mein convert | — | ✅ | ❌ | ❌ |

Hot paths ke liye: **templates** (`template <class F> void forEach(..., F fn)`) —
compiler ko concrete callable dikhta hai aur woh use **inline** kar deta hai.
`std::function` mein overhead hai (type erasure, shayad heap) — hot loops mein
isse bacho (folder 22, 36).

---

## Member function pointers (thoda sa)

```cpp
struct Widget { int scale(int x) { return x * factor; } int factor = 2; };

int (Widget::*mfp)(int) = &Widget::scale;    // pointer-to-member-function
Widget w;
(w.*mfp)(10);            // 20   -- iske liye ek object chahiye
Widget* pw = &w;
(pw->*mfp)(10);         // 20
```

Yeh free function pointer se **alag type** hai (isse ek `this` chahiye). Seedha
likha kam hi jaata hai — `std::function` / lambdas / `std::mem_fn` ise wrap kar
lete hain. (Folder 15, 22.)

---

## Virtual functions = chhupi hui function-pointer table

```cpp
struct Base { virtual void f(); };
```

Polymorphic class ke paas ek chhupi hui **vtable** hoti hai — function pointers
ka array — aur har object ke paas us table ka ek chhupa hua **vptr**. `obj->f()`
matlab: vptr load karo → `vtable[index]` load karo → indirect call. Matlab yeh
bilkul wahi dispatch-table pattern hai jo abhi upar dekha, bas compiler khud
bana deta hai. (Folder 16.)

---

## Andar kya hota hai

- Function pointer us function ki pehli instruction ka address rakhta hai
  (`.text` mein).
- `op(x, y)` → `call [op]` — ek **indirect call**. CPU ka indirect-branch
  predictor target ka andaaza lagata hai; galat nikla to ~15-20 cycles ka
  nuksaan (folder 06 file 08). *Direct* call (`call add`) hamesha predict ho
  jaati hai.
- Compiler aam taur pe function pointer ke through **inline nahi kar sakta**
  (use target pata hi nahi) — jab tak woh value ko dekh/prove na le
  (devirtualization, LTO).
- Bina capture wala lambda jab `fp` ki tarah pass hota hai → wahi indirect-call
  cost. Par jab **template parameter** ki tarah pass hota hai → concrete type
  pata hota hai → **inline ho jaata hai**. Yeh chhota sa fark hot loop mein bada
  hota hai.

> **HFT relevance:** Hot path pe indirect calls (function pointers,
> `std::function`, virtual) avoid kiye jaate hain: woh inlining rokte hain aur
> indirect-branch predictor pe dabaav daalte hain (tail-latency spikes). HFT
> dispatch inme se kuch use karta hai: message-type byte pe `switch` (jump table,
> achhe se predict hoti hai), `std::variant` + `std::visit` (inlined branch),
> templates / CRTP (static dispatch, poori tarah inline). Function-pointer tables
> sirf cold config/plugin registration aur C callbacks ke liye bachti hain.
> Folders 16, 22, 36, 38.

---

## Hands-on

`examples/05_function_pointers.cpp` — declare karna, callbacks, dispatch table,
aur bina capture wala lambda → fp:

```bash
./build.ps1 12-POINTERS/examples/05_function_pointers.cpp
```

---

## ⚠️ Traps

### Trap 1 — declaration mein parens bhool jaana
```cpp
int* f(int);      // function jo int* return karta hai
int (*f)(int);    // function ka pointer jo int return karta hai
```

### Trap 2 — capture wala lambda `fp` mein daalna
```cpp
int cap = 5;
int (*fp)(int) = [cap](int x) { return x + cap; };   // ❌ ERROR -- iske paas state hai
```

### Trap 3 — null function pointer call karna
```cpp
Handler h = nullptr;  h(msg);   // 💥 0 pe indirect call -> crash. Pehle check karo
```

### Trap 4 — hot loop mein `std::function`
```cpp
for (auto& x : bigVec) callback(x);   // ⚠️ agar callback std::function hai -> har call pe overhead
template <class F> void each(std::span<T> v, F f) { for (auto& x : v) f(x); }   // ✅ inline hota hai
```

### Trap 5 — member vs free function pointer ka type
```cpp
void (*fp)() = &Widget::method;   // ❌ -- member fp ka type void (Widget::*)() hai
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`int* f(int)` ek function pointer hai" | Woh function hai jo `int*` deta hai. Pointer `int (*f)(int)` hai |
| "Capture wale lambdas `fp` mein convert hote hain" | Sirf bina capture wale |
| "Function pointer call direct call jitni tez hai" | Indirect call — predictor pe depend, inlining nahi |
| "`std::function` == function pointer" | Type-erased, ~32 bytes, heap le sakta hai, slow |
| "Virtual dispatch jaadu hai" | Compiler ki banayi function-pointer table (vtable) hai |

---

## Exercises

1. **Declare + call:** `int (*op)(int, int);` — baari-baari `add`, `sub`, `mul`
   assign karo; har ek ko call karo. `sizeof(op)` kya hai?

2. **Callback:** `int reduce(const int* a, size_t n, int init, int (*f)(int,
   int))` implement karo; usse sum aur product dono nikalo.

3. **Dispatch table:** `enum class Cmd { Add, Sub, Mul };` → `std::array<int
   (*)(int, int), 3>` jo enum se index ho. `table[Cmd::Mul](6, 7)` call karo.

4. **Lambda conversion:** inme se kaunse `int (*fp)(int)` mein assign honge?
   `[](int x){return x;}`, `[k](int x){return x+k;}`, `[&](int x){return x+g;}`.

5. **Perf:** 10M ints ke loop mein ek `void (*)(int)` fp call karo, phir wahi
   lambda template parameter ki tarah pass karke. `-O2` pe time lo. Kitna fark?

6. **Member fp:** `struct M { int v = 10; int get() const { return v; } };` —
   `int (M::*mp)() const = &M::get;` — ek object se aur ek pointer se call karo.

---

## Interview questions

1. Function pointer declare kaise? Parens kyun zaroori?
2. Capture-less vs capturing lambda — `fp` conversion?
3. Function pointer call ki cost (indirect, inlining)?
4. `fp` vs lambda vs `std::function` — size, cost?
5. Virtual functions andar se kya use karti hain?
6. Hot path pe indirect calls kyun avoid — HFT alternatives?

---

## Next
→ [`12-dangling-pointers.md`](12-dangling-pointers.md)

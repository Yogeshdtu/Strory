# 01 — Pointer kya hai

## Prerequisites
- `11-STRUCTS/` (poora)
- `01-PROGRAMMING-BASICS/11-memory-basics.md` (RAM, addresses)
- `09-ARRAYS/05-array-decay.md` (array → pointer)
- `08-FUNCTIONS/05-the-call-stack.md` (addresses, frames)

## Yeh folder kyun
**Yahan se asli C++ shuru hoti hai.** Pointer wahi cheez hai jo C++ ko systems
language banati hai — direct memory access. Beginners ko yeh sabse zyada darata
hai, isliye hum diagrams ke saath, dheere-dheere karenge.

Har prerequisite (variables → addresses → memory → arrays → structs) ho chuka
hai. Ab aap taiyaar ho.

---

## Ek variable ka address hota hai

Har variable RAM mein kisi jagah rehta hai. Us jagah ka ek number hai — uska
**address**.

```cpp
int age = 25;
```

```
   Memory:
   address    | value
   -----------+------
   0x7ffc44   | 25       <- `age` yahan rehta hai
```

`&age` = "`age` ka address" (`0x7ffc44` jaisa kuch).

---

## Pointer = address rakhne wala variable

```cpp
int  age = 25;
int* p   = &age;      // p ek pointer hai; usme age ka address hai
```

```
   address    | value       | naam
   -----------+-------------+------
   0x7ffc38   | 0x7ffc44    | p       <- p mein age ka ADDRESS hai
   0x7ffc44   | 25          | age
```

- `p` khud bhi ek variable hai (uska bhi address hai — `&p`).
- `p` ki **value** ek address hai.
- `*p` = "jis address pe `p` point karta hai, wahan ki value" = `25`. Isse
  **dereference** kehte hain.

```cpp
std::cout << p;       // 0x7ffc44   (ek address)
std::cout << *p;      // 25         (age ki value)
std::cout << &p;      // 0x7ffc38   (p ka apna address)
```

---

## Pointer ke through value badlo

```cpp
*p = 30;              // "jis pe p point karta hai, usko 30 kar do"
std::cout << age;     // 30  -- age BADAL gaya
```

`p` aur `age` **ek hi memory** dekh rahe hain. `*p` se ya `age` se — dono se wahi
byte badalte hain.

---

## Pointer ka TYPE matter karta hai

```cpp
int*    pi;           // "pointer to int"
double* pd;           // "pointer to double"
char*   pc;           // "pointer to char"
```

- Sab pointers ka **size same** — 8 bytes (64-bit) — kyunki sabme ek address hai.
- Par **type batata hai deref pe kitne bytes** padhne/likhne hain: `*pi` 4 bytes,
  `*pd` 8 bytes, `*pc` 1 byte.
- Type mismatch = compile error: `int* p = &someDouble;` ❌ (`void*` ke alawa,
  file 10).

```cpp
std::cout << sizeof(int*);      // 8
std::cout << sizeof(char*);     // 8
```

---

## Pointer re-bindable hai (reference nahi)

```cpp
int a = 1, b = 2;
int* p = &a;          // p -> a
p = &b;               // p ab -> b   (reference yeh nahi kar sakti -- folder 13)
```

Ek pointer apni zindagi mein alag-alag objects ko point kar sakta hai.

---

## Kyun pointers chahiye

1. **Function ko caller ka data modify karne do** — `void f(int* p) { *p = 5; }`
   (file — swap, folder 08). References isse cleaner karti hain (folder 13).
2. **Dynamic memory** — `new int[n]` runtime-sized allocation ka handle (folder 14).
3. **Linked structures** — lists, trees: struct pointing to struct (file 08).
4. **Arrays / buffers pass karna** — decay se pointer (folder 09); `std::span`
   iske upar bana hai.
5. **Polymorphism** — base-class pointer to derived object (folder 16).
6. **"Optional" / "maybe absent"** — `nullptr` = "kuch nahi" (file 04).
7. **C APIs** — OS / library interfaces pointers mein baat karti hain.

---

## Andar kya hota hai

- A pointer is just an integer-sized value (8 bytes) holding a memory address.
- `&x` → the compiler emits the address of `x` (a stack offset, or a `.data`
  address, or...).
- `*p` → a memory access: `load [p]` (read) or `store [p], value` (write), of
  `sizeof(*p)` bytes.
- No bounds checking, no null checking — `*p` where `p` is null or garbage → UB
  (crash or corruption). **Speed for responsibility.**

> **HFT relevance:** Pointers are the substrate of everything low-level — mapping
> a struct onto a network buffer, walking an order-book's intrusive list,
> pointing into a preallocated arena, function-pointer / vtable dispatch. But
> **raw owning pointers** (`new`/`delete`) are avoided in modern HFT C++ —
> ownership goes through `std::unique_ptr` / containers / arenas (folders 14, 17),
> and *non-owning* access goes through references / `std::span` / `std::string_view`
> where possible. Raw pointers remain for interop, intrusive data structures, and
> the hottest hand-tuned code. This folder builds the mental model; folders 13–17
> build the safe patterns on top.

---

## Hands-on

`examples/01_basic_pointers.cpp` (`&`, `*`, modify-through), `examples/07_pointer_diagrams.cpp`
(memory state at each step):

```bash
./build.ps1 12-POINTERS/examples/01_basic_pointers.cpp
./build.ps1 12-POINTERS/examples/07_pointer_diagrams.cpp
```

---

## ⚠️ Traps

### Trap 1 — uninitialized pointer
```cpp
int* p;           // ⚠️ garbage address
*p = 5;           // 💥 writes to a random location -- UB
int* p = nullptr; // ✅ (still can't deref, but a clean crash if you do)
```

### Trap 2 — `*p` vs `p` confusion
```cpp
p = 5;            // ⚠️ sets the POINTER to address 5 (almost always wrong)
*p = 5;           // sets the pointed-to VALUE to 5
```

### Trap 3 — dangling: pointing at something that's gone
```cpp
int* p;
{ int x = 1; p = &x; }   // x destroyed
*p;                       // ⚠️ dangling -> UB (file 12)
```

### Trap 4 — type-punning without care
```cpp
double d = 1.0;
int* p = reinterpret_cast<int*>(&d);   // ⚠️ *p reads 4 bytes of a double -- aliasing UB (folder 25)
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Pointer aur uski pointee same cheez hain" | `p` = an address; `*p` = the value there |
| "`int*` bada hota hai `char*` se" | Sab 8 bytes (an address) |
| "`p = 5` deref karta hai" | Sets the pointer to address 5; `*p = 5` sets the value |
| "Pointer hamesha valid hota hai" | Uninitialized / dangling / null → UB on deref |
| "Reference bhi re-bindable hai" | No — pointer yes, reference no (folder 13) |

---

## Exercises

1. **Basics:** `int n = 100; int* p = &n;` — print `n`, `&n`, `p`, `*p`, `&p`.
   Which two are equal?

2. **Modify:** through `p`, change `n` to `250`. Then change `n` directly to `7`
   and print `*p`. Explain.

3. **Sizes:** print `sizeof` of `int*`, `double*`, `char*`, `void*`, and a
   function pointer. All the same?

4. **Diagram:** draw the memory table (like `07_pointer_diagrams.cpp`) for:
   ```cpp
   int a = 1, b = 2;
   int* p = &a;
   *p = 10;
   p = &b;
   *p = 20;
   int c = *p;
   ```

5. **Uninitialized danger:** `int* p; std::cout << p;` — what prints? Now
   `*p = 1;` — what happens? (Expect a crash — that's the point.)

6. **`*p` vs `p`:** `int x = 5; int* p = &x;` — what does `p = 10;` do (compile
   error or not)? `*p = 10;`?

---

## Interview questions

1. Pointer kya hai? `p`, `*p`, `&p` mein fark?
2. `int*` aur `char*` ka size — same ya alag? Kyun?
3. Pointer ka type kya batata hai (deref ke context mein)?
4. Pointer re-bindable hai? Reference?
5. Uninitialized pointer deref — kya hota hai?
6. Pointers kis-kis cheez ke liye chahiye (4 use cases)?

---

## Next
→ [`02-address-of-operator.md`](02-address-of-operator.md)

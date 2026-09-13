# 05 — Memory leaks

## Prerequisites
- [`04-new-and-delete.md`](04-new-and-delete.md)
- Folder 13 file 09 (reference bugs — overlap)

## Yeh topic abhi kyun
Leak = aapne memory allocate ki, aur uska **aakhri pointer kho diya** bina free
kiye. Program crash nahi karta, output galat nahi hota — bas RAM dheere-dheere
badhti hai. Long-running process (server, exchange gateway) ke liye yeh silent
killer hai. Pehchanna, dhoondhna, aur rokna — teeno.

---

## Leak kya hai (aur kya nahi)

**Leak:** allocated block, koi live pointer nahi, koi `free`/`delete` nahi hoga.

```cpp
void f() {
    int* p = new int[1000];
    // ... delete[] p;  <- bhool gaye
}   // p (stack) gaya. Heap block ab "orphan" -- kabhi free nahi hoga
```

**Leak NAHI:**
- Program end tak jaan-boojh kar rakhi memory (OS process exit pe sab wapas leta) —
  technically "still reachable", practically theek (par tools flag karte hain).
- `std::vector` / `std::string` ki memory — unka destructor free karta.

**Slow leak** sabse khatarnaak: per-request 1 KB leak × lakhon requests/din →
din baad OOM.

---

## Leak ke common patterns

### 1. Missing `delete` (early return / exception)

```cpp
Widget* w = new Widget();
if (!validate(w)) return;        // ⚠️ leak
process(w);
delete w;                        // sirf happy path pe
```

### 2. Pointer overwrite

```cpp
int* p = new int(1);
p = new int(2);                  // ⚠️ pehla block orphan -- leak
delete p;                        // sirf doosra free hua
```

### 3. Container of raw pointers

```cpp
std::vector<Order*> book;
book.push_back(new Order{...});
// ...
book.clear();                    // ⚠️ pointers gaye, Orders leak. Har ek delete karna tha
```

### 4. Cyclic `shared_ptr` (folder 17)

```cpp
struct Node { std::shared_ptr<Node> next; };
a->next = b;  b->next = a;       // ⚠️ refcount kabhi 0 nahi -> dono leak. weak_ptr todo cycle
```

### 5. Resource leak (memory se aage)

File handles, sockets, mutex locks, GPU buffers — same idea, RAII se same fix.

---

## Kaise dhoondhein

### Linux / macOS / WSL — asli tools

```bash
# AddressSanitizer + LeakSanitizer (LSan built-in on Linux)
g++ -std=c++20 -fsanitize=address -g prog.cpp -o prog && ./prog
#  exit pe:  "Direct leak of 4000 byte(s) in 1 object(s)" + allocation stack trace

# Valgrind (koi recompile nahi)
valgrind --leak-check=full --show-leak-kinds=all ./prog
#  "definitely lost" / "indirectly lost" / "still reachable" categories

# heaptrack -- allocation profiler (leaks + hot allocation sites + timeline)
heaptrack ./prog && heaptrack_gui heaptrack.prog.*.zst
```

### MinGW-w64 (`C:\mingw64`) — LSan/Valgrind NAHI

Is repo ke examples ek **counted `operator new`/`delete`** override use karte hain
taaki leak is platform pe bhi dikhe
([`examples/04_memory_leak.cpp`](examples/04_memory_leak.cpp)):

```cpp
long g_new = 0, g_del = 0;
void* operator new(std::size_t n)   { ++g_new; /* malloc */ }
void  operator delete(void* p) noexcept { if (p) { ++g_del; /* free */ } }
void* operator new[](std::size_t n) { return ::operator new(n); }   // MinGW: alag override zaroori
// exit pe: agar g_new != g_del -> LEAK: (g_new - g_del) blocks
```

Measured output: `LEAK: 1006 block(s) never freed  (~1024284 bytes)`.

**`operator new[]` alag se kyun?** Standard kehta hai default `operator new[]` aapke replace kiye `operator new`
ko bulaye. Linux pe aisa hi hota hai. Par MinGW pe default `operator new[]` `libstdc++-6.dll` ke **andar** hai,
aur Windows DLL ke andar ki call exe mein replace kiye function tak nahi pahunchti. GCC 16.2 pe naapa: sirf
`operator new` replace karke `new double[128]` → normal build mein count **0**, `-static` build mein **1**. Isi
wajah se library ke andar hone wali allocations (jaise `std::pmr::new_delete_resource`) bhi DLL build mein nahi
gini jaati (file 10, exercise 4).

Windows pe aur options: Visual Studio `_CrtDumpMemoryLeaks` (MSVC CRT),
Dr. Memory, Application Verifier.

---

## Kaise rokein — RAII (folder 17 preview)

**Rule: raw owning pointer mat rakho.** Ownership ko ek object mein baandho jiska
destructor free kare.

```cpp
// ❌ leak-prone
Widget* w = new Widget();
std::vector<Order*> book;

// ✅ RAII -- destructor guarantees cleanup
auto w = std::make_unique<Widget>();
std::vector<std::unique_ptr<Order>> book;      // ya std::vector<Order> agar polymorphism nahi
std::vector<Order> flat;                       // best -- ek allocation, zero leak risk
```

Har code path pe (return, break, exception) destructor chalta → `delete`
guaranteed. **Yeh leak problem ko structurally khatam karta hai**, tools se
dhoondhne se behtar.

---

## Andar kya hota hai

- Leaked block allocator ke "allocated" set mein rehta hai — free list mein wapas
  nahi jaata. Process ka **RSS** (resident set) badhta hai jaise-jaise leaked
  pages touch hote hain.
- OS process **exit** pe saari memory reclaim karta hai — isliye ek short CLI
  tool ka "leak at exit" harmless hai. Long-running process ke liye fatal.
- LSan exit pe **live pointer scan** karta hai (roots: globals, stack, registers)
  → jo heap blocks kisi se reachable nahi = leak, allocation stack ke saath.
- Valgrind har allocation ko shadow-track karta hai (~10-30x slow) → exact
  bytes + "lost" category.
- Counted `operator new` sirf **count** deta hai (kahan leak hua nahi) — quick
  signal, full diagnosis ke liye LSan/Valgrind chahiye.

> **HFT relevance:** trading systems ghanton/dinon chalte hain — ek slow leak
> (per-message, per-order) latency jitter (allocator zyada kaam), phir OOM kill
> laata hai. Practices: (1) hot path zero-alloc (koi runtime `new` hi nahi →
> leak ka sawaal nahi), (2) RAII / ownership types har jagah, (3) CI mein ASan+
> LSan build + soak test (ghanton chalao, RSS flat hona chahiye), (4) allocation
> counters production metrics mein (new/free rate, outstanding bytes).

---

## Hands-on

```bash
./build.ps1 14-MEMORY/examples/04_memory_leak.cpp
```

`LEAK: 1006 block(s)` report dekho. Phir har leaky function ko fix karo
(`delete[]` add, ya `std::vector<double>` use) — report `0 outstanding` hona
chahiye. Linux pe wahi file ASan se: `-fsanitize=address` → per-leak stack trace.

---

## ⚠️ Traps

### Trap 1 — "abhi to memory kaafi hai"
1 KB/request × 100 req/s × 86400 s = ~8 GB/din. Slow leak = time bomb.

### Trap 2 — `container.clear()` raw pointers ko free karta
```cpp
std::vector<T*> v;  v.clear();   // ⚠️ pointees leak. Pehle har delete, ya unique_ptr rakho
```

### Trap 3 — "still reachable" ko ignore karna
Valgrind "still reachable" aksar theek (global cache), par kabhi ek genuine
"maine free karna bhool gaya, par pointer abhi zinda hai" bhi — dekho.

### Trap 4 — exception path
```cpp
auto* p = new T();
mayThrow();            // ⚠️ throw -> delete p miss -> leak. RAII se fix
delete p;
```

### Trap 5 — self-cycle `shared_ptr`
```cpp
node->self = node;     // ⚠️ refcount >=1 hamesha -> never freed. weak_ptr
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Leak se program crash hota hai" | Nahi — chup-chaap RAM badhti, phir (kabhi) OOM |
| "Chhota leak matter nahi karta" | Long-running process mein per-op leak = OOM |
| "`vector<T*>::clear()` sab free karta" | Sirf pointer array; pointees leak |
| "GC nahi hai to leak inevitable hai" | RAII structurally rokta — better than GC yahan |
| "Exit pe OS clean karta, to theek" | CLI tool: haan. Server: nahi |

---

## Exercises

1. **Spot the leak:** har snippet — leak hai? kahan?
   ```cpp
   (a) int* p = new int(1); p = &someLocal;
   (b) auto v = std::make_unique<int[]>(10);
   (c) std::vector<std::string*> v; v.push_back(new std::string("x")); // v scope-exit
   (d) FILE* f = fopen("a","r"); if (!parse(f)) return; fclose(f);
   ```

   <details><summary>Answer</summary>

   (a) leak — `new int(1)` block orphan. (b) no leak — `unique_ptr<int[]>`
   frees. (c) leak — `string` pointees; `v` sirf pointer array free karta. (d)
   resource leak — `return` pe `fclose` miss (RAII wrapper chahiye).
   </details>

2. **Fix `04`:** [`examples/04_memory_leak.cpp`](examples/04_memory_leak.cpp) ke
   3 leaky functions ko fix karo (minimal change). Report `outstanding: 0`?

   <details><summary>Answer</summary>

   `leakOne` → `delete p;` (ya `int` by value). `leakInLoop` → `delete[] row;`
   loop ke andar (ya `std::vector<double>`). `leakViaContainerOfPointers` →
   `std::vector<std::string>` (no pointers), ya har `delete` before clear.
   </details>

3. **Overwrite leak:** `std::string* p = new std::string("a"); p = new
   std::string("b"); delete p;` — kitne bytes leak? Counter se verify.

   <details><summary>Answer</summary>

   Pehla `std::string` object (+ agar uska buffer heap pe tha) leak. Counter:
   `operator new` calls > `operator delete` calls by 1 (ya 2 agar dono strings
   long the aur pehle ka buffer bhi orphan).
   </details>

4. **Soak test idea:** ek loop jo 1e6 baar ek function call kare jo (buggy) ek
   `new int` leak kare. `/proc/self/statm` ya `GetProcessMemoryInfo` se RSS
   har 1e5 iterations pe print. Graph shape?

   <details><summary>Answer</summary>

   RSS monotonically upward (linear-ish) — leak ka signature. Fixed version:
   RSS flat (plateau) baad ke initial ramp ke.
   </details>

5. **Cycle:** `struct N { std::shared_ptr<N> next; }; auto a =
   std::make_shared<N>(); auto b = std::make_shared<N>(); a->next=b; b->next=a;` —
   `a`, `b` scope se nikal jaayein to memory free? `weak_ptr` se fix.

   <details><summary>Answer</summary>

   Free nahi — `a`↔`b` ek doosre ko zinda rakhte (refcount ≥ 1). Fix: ek
   direction ko `std::weak_ptr<N>` banao (parent→child strong, child→parent
   weak).
   </details>

---

## Interview questions

1. Memory leak ki exact definition? "Still reachable" leak hai?
2. Leak ke 4 common patterns bata.
3. `std::vector<T*>::clear()` — kya free hota, kya leak?
4. Leak dhoondhne ke tools — Linux vs Windows/MinGW?
5. RAII leak ko kaise **structurally** rokta hai (tools se dhoondhne se better)?
6. `shared_ptr` cycle leak — kaise banta, `weak_ptr` se kaise tootta?

---

## Next
→ [`06-use-after-free.md`](06-use-after-free.md)

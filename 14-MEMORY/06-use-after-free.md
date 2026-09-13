# 06 — Use-after-free, double-free

## Prerequisites
- [`05-memory-leaks.md`](05-memory-leaks.md)
- Folder 12 file 12 (dangling pointers), file 13 (bug catalog)

## Yeh topic abhi kyun
Leak "extra memory rakhna" hai — annoying, par memory-safe. **Use-after-free
(UAF)** aur **double-free** iske ulat: freed memory ko touch karna. Yeh
memory-**unsafe** — silent corruption, exploitable vulnerabilities, "test mein
chalta prod mein crash". Sabse mehnge bugs isi family ke hain.

---

## Use-after-free

Freed block ko padhna ya likhna:

```cpp
int* p = new int(42);
delete p;              // block allocator ko wapas
std::cout << *p;       // ⚠️ UAF read -- p dangling. abhi wahi bytes ho sakte, ya garbage
*p = 7;                // ⚠️ UAF write -- ab allocator ki free-list / kisi aur ka data corrupt
```

`delete p` ke baad `p` ek **dangling pointer** hai (folder 12 file 12). Value
badalti nahi — bas ab woh memory allocator ki hai.

### Kyun "kabhi chalta hai"

Freed block turant OS ko wapas nahi jaata — allocator ke free list mein baithta
hai. Agle `malloc` tak uske bytes wahi rehte hain. Isiliye UAF **aksar sahi
value deta hai** — jab tak block reuse na ho. Reuse hote hi: aap kisi aur ke
object ko padh/corrupt kar rahe ho.

```cpp
char* a = new char[16];  strcpy(a, "SECRET");
delete[] a;
char* b = new char[16];  strcpy(b, "public");   // allocator wahi block de sakta hai
std::cout << a;                                  // ⚠️ "public" -- a ab b ko alias karta
```

([`examples/05_use_after_free.cpp`](examples/05_use_after_free.cpp) — BUG 1 stale
read, BUG 2 block reuse.)

---

## Double-free

Ek block ko do baar free karna:

```cpp
int* p = new int(1);
delete p;
delete p;              // ⚠️ double-free -- allocator ki metadata / free-list corrupt
```

Ya do owners ka ek block:

```cpp
void consume(Widget* w) { /* ... */ delete w; }
Widget* w = new Widget();
consume(w);            // consume ne delete kiya
delete w;              // ⚠️ double-free -- ownership confusion
```

Effect: allocator ka internal state toot-ta hai → **agli** allocation crash
karti hai (ya, exploited, arbitrary write). glibc aksar `free(): double free
detected in tcache 2` ke saath abort karta hai. MinGW/UCRT (Windows heap) pe naapa (GCC 16.2, `-O0` aur `-O2`,
3/3 runs): **doosra `delete` wahin program khatam** — exit code `0xC0000374` (`STATUS_HEAP_CORRUPTION`), koi
message nahi (Git-Bash exit 127 dikhata hai). Is simple case mein heap ne pakad liya; zyada ulajhe cases (beech
mein doosri allocations) mein detection ki guarantee nahi.

---

## Related: use-after-return / use-after-scope

Same family — freed **stack** memory:

```cpp
int* dangle() { int x = 5; return &x; }     // ⚠️ x ka frame return pe gaya (folder 12/13)
int& refDangle() { int x = 5; return x; }   // ⚠️ -Wreturn-local-addr
```

ASan inhe `stack-use-after-return` / `stack-use-after-scope` bolta hai.

---

## Kaise pakdein

### Linux / macOS / WSL — AddressSanitizer

```bash
g++ -std=c++20 -fsanitize=address -g prog.cpp -o prog && ./prog
```

UAF pe ASan turant abort karta hai, do stack traces ke saath:

```
==12345==ERROR: AddressSanitizer: heap-use-after-free on address 0x...
  READ of size 4 at 0x... thread T0
    #0 ... main prog.cpp:12          <- yahan use hua
  freed by thread T0 here:
    #0 operator delete
    #1 ... main prog.cpp:10          <- yahan free hua
  previously allocated by thread T0 here:
    #0 operator new
    #1 ... main prog.cpp:8           <- yahan alloc hua
```

Double-free: `attempting double-free` + free stacks dono. ASan freed memory ko
**quarantine** mein rakhta hai (turant reuse nahi) → UAF reliably pakadta.

### MinGW-w64 — ASan nahi

- Counted `operator new`/`delete` (examples mein) — double-free/mismatch count
  se dikh sakta, par UAF-read nahi.
- `./build.ps1 san` → `-D_GLIBCXX_ASSERTIONS -fstack-protector-all` (STL bounds +
  stack canary; heap UAF nahi).
- Windows: Application Verifier (`+heap` → page-heap, har allocation apni page pe,
  UAF = instant fault), Dr. Memory.
- Best: bug ko Linux/WSL pe reproduce karke ASan.

### Compile-time help

`-Wuse-after-free` (GCC 12+, `-Wall`) simple intra-function cases pakadta:

```cpp
free(p); return *p;    // ⚠️ -Wuse-after-free
```

---

## Kaise rokein

1. **`delete` ke baad `p = nullptr;`** — `nullptr` ko delete karna **safe** hai
   (no-op). UAF-write null-deref crash ban jaata (loud), silent corruption
   nahi.
   ```cpp
   delete p;  p = nullptr;
   ```
2. **Clear ownership** — ek block ka ek owner. `std::unique_ptr` (move-only →
   compiler double-owner rokta). `shared_ptr` jab genuinely shared.
3. **RAII** — aap `delete` likhte hi nahi → double-free / mismatch ka scope hi
   nahi.
4. **Value semantics** — `std::vector<Order>` (pointers nahi) → koi manual
   free, koi dangling.
5. **Iterators/references ko mutation ke aar-paar mat pakdo** (folder 13 file 09
   — realloc UAF).

---

## Andar kya hota hai

- **`delete p`** → block header se size, phir tcache/bin mein daala. Bytes wahi
  rehte (allocator overwrite nahi karta, performance) → UAF-read purani value de
  deta. Debug allocators (`MALLOC_PERTURB_`, ASan) freed memory ko poison karte
  (`0xdd` / `0xfe`) taaki UAF turant dikhe.
- **UAF-write** free-list ke `next` pointer ko overwrite kar sakta → agli
  `malloc` attacker-controlled address de → classic exploitation primitive.
- **Double-free** → block do baar free-list mein → free list mein cycle → do
  `malloc` **same** address de dete → do "alag" objects ek memory pe → chaos.
- **ASan** har allocation ke around **redzones** + freed memory ke liye
  **quarantine** rakhta hai, aur har memory access se pehle **shadow memory**
  check karta (~2-3x slow, ~2x RAM) → UAF/OOB exact.

> **HFT relevance:** UAF/double-free = non-deterministic corruption jo aksar
> sirf load ke under (jab freed block reuse hota) dikhta — production mein
> ghante debugging. Defenses: (1) hot path zero-`delete` (pre-allocated pools;
> objects "return to pool", free hote hi nahi), (2) `unique_ptr` / move
> semantics ownership ko compiler-checked banate, (3) CI mein ASan build +
> stress test, (4) pool designs jo freed slot ko generation-tag karte (stale
> handle detect). "Ownership har jagah explicit" ek hard rule hoti hai.

---

## Hands-on

```bash
./build.ps1 14-MEMORY/examples/05_use_after_free.cpp
```

BUG 1: `delete` ke baad `*p` — stale/garbage value. BUG 2: freed block reuse
(allocator-dependent — kabhi same addr). BUG 3: double-free line comment-out —
enable karke (ya `san` build / Linux ASan) crash/abort dekho. Fir har bug ko
`p = nullptr;` + ownership se fix karo.

---

## ⚠️ Traps

### Trap 1 — `delete` ke baad pointer use
```cpp
delete p;  log(p->id);    // ⚠️ UAF. delete ke baad p = nullptr
```

### Trap 2 — do jagah delete (ownership confusion)
```cpp
cache.store(w);   // cache delete karega?  ya main?  -> double-free ya leak
```

### Trap 3 — freed vector element ka reference
```cpp
int& r = v[0];  v.clear();  r = 1;   // ⚠️ UAF (folder 13 file 09)
```

### Trap 4 — "value sahi aayi to bug nahi"
UAF aksar sahi value deta hai (block abhi reuse nahi hua). Test pass ≠ safe.

### Trap 5 — `delete this` ke baad member touch
```cpp
void Obj::destroy() { delete this; count_++; }   // ⚠️ UAF -- this freed
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "UAF hamesha crash karta" | Aksar chalta hai (stale value) — jab reuse ho tab corrupt |
| "`delete p` `p` ko nullptr kar deta" | Nahi — `p` dangling rehta. Khud `p = nullptr` |
| "`delete nullptr` crash" | Safe no-op — isiliye delete ke baad null set karo |
| "Double-free bas ek warning hai" | Allocator metadata corruption → arbitrary crash / exploit |
| "Test pass = memory safe" | UAF/OOB test mein chhup sakte — ASan/soak chahiye |

---

## Exercises

1. **UAF ya nahi:** har snippet —
   ```cpp
   (a) int* p = new int(1); delete p; int y = *p;
   (b) int* p = new int(1); delete p; p = nullptr; if (p) *p = 2;
   (c) std::string s = "hi"; const char* c = s.c_str(); s = "much longer ......"; puts(c);
   (d) auto p = std::make_unique<int>(1); int x = *p;
   ```

   <details><summary>Answer</summary>

   (a) UAF read. (b) safe — `p` null, `if (p)` false. (c) UAF — `s` reallocated,
   `c` stale (folder 10). (d) safe — `unique_ptr` alive.
   </details>

2. **Double-free reproduce:** `05` mein BUG 3 ka `delete p;` uncomment karo,
   `san` build ya Linux ASan pe chalao. Error message? (concept)

   <details><summary>Answer</summary>

   glibc/ASan: "attempting double-free" / "double free detected" + do free stack
   traces (dono `delete p` lines). MinGW: crash / heap corruption abort (kam
   informative).
   </details>

3. **Fix pattern:** ek function jo `new` karta, kaam karta, `delete` karta — par
   beech mein `throw` ho sakta hai. UAF nahi, par leak. `unique_ptr` se fix —
   ab exception pe kya (leak/UAF/safe)?

   <details><summary>Answer</summary>

   Raw: throw → leak (delete miss). `unique_ptr`: throw → stack unwind →
   `unique_ptr` destructor → `delete` → safe, no leak, no UAF.
   </details>

4. **Block reuse demo:** ek program — `new char[32]`, likho "AAAA", `delete[]`,
   phir turant `new char[32]`, likho "BBBB", pehle pointer se padho. Same block
   mila? (allocator pe depend — dono outcomes explain karo.)

   <details><summary>Answer</summary>

   Agar allocator ne wahi block diya (common for same-size immediate realloc) →
   pehla pointer "BBBB" padhega (UAF alias). Agar alag block → pehla pointer
   purani/garbage. Dono UB.
   </details>

5. **`delete this`:** ek class jisme `void suicide() { delete this; }` — call ke
   baad object ke members/methods touch karna kyun UAF? Yeh pattern kab (rarely)
   valid?

   <details><summary>Answer</summary>

   `delete this` ke baad `*this` freed — koi member access UAF. Valid sirf tab
   jab method `delete this` ke baad **kuch bhi** object ka touch na kare (last
   statement), aur caller object ko dobara use na kare. Reference-counted
   objects (`release()`) mein dikhta, warna avoid.
   </details>

---

## Interview questions

1. Use-after-free kya? Kyun "kabhi kabhi chalta hai"?
2. UAF-read vs UAF-write — kaunsa zyada khatarnaak, kyun?
3. Double-free allocator ko kaise todta hai?
4. `delete p;` ke baad `p` ka kya? Best practice?
5. ASan UAF kaise pakadta (quarantine, shadow, redzone)? MinGW pe kya alternative?
6. `delete this` — kab UB, kab (rarely) OK?

---

## Next
→ [`07-static-and-thread-local.md`](07-static-and-thread-local.md)

# 13 — Pointer bugs — full catalog

## Prerequisites
- [`01-what-is-a-pointer.md`](01-what-is-a-pointer.md) … [`12-dangling-pointers.md`](12-dangling-pointers.md)

## Yeh topic abhi kyun
Pointer bugs C++ ke sabse mehenge bugs hain (crashes, corruption, security).
Yeh unka poora catalog hai — pehchan, wajah, detection, fix. Ise bookmark kar lo.

---

## 1. Uninitialized (wild) pointer

```cpp
int* p;          // ⚠️ garbage address (local hai)
*p = 5;          // 💥 kisi random jagah likh diya
```
**Pakdo kaise:** `-Wuninitialized` (thoda-bahut), MSan, Valgrind. **Fix:** hamesha
initialize karo — `int* p = nullptr;` (ya usse behtar, `= &something`).

---

## 2. Null dereference

```cpp
int* p = nullptr;  *p;   // 💥 SIGSEGV
Node* n = find(k); n->v; // 💥 agar mila hi nahi
```
**Pakdo kaise:** crash, UBSan (`-fsanitize=null`), ASan. **Fix:** guard clause
(`if (p)`), pointer ki jagah reference, aur lookup ke result ko check karo.

---

## 3. Dangling / use-after-scope

```cpp
int* p; { int x = 1; p = &x; } *p;   // ⚠️ UB
int* f() { int x; return &x; }        // ⚠️ -Wreturn-local-addr
```
**Pakdo kaise:** `-Wdangling-pointer`, `-Wreturn-local-addr`, ASan
(stack-use-after-return/scope). **Fix:** lifetimes match karao; value se return karo.

---

## 4. Use-after-free (heap)

```cpp
int* p = new int(1);  delete p;  *p = 2;   // ⚠️ free ho chuki memory mein likh rahe ho
```
**Pakdo kaise:** ASan (heap-use-after-free), Valgrind. **Fix:** `delete` ke baad
`p = nullptr`; usse behtar — `std::unique_ptr` / containers (manual delete hi nahi).

---

## 5. Double-free

```cpp
delete p;  delete p;                        // ⚠️ UB (heap corruption)
int* a = new int; int* b = a; delete a; delete b;   // ⚠️ dono ek hi jagah -> double-free
```
**Pakdo kaise:** ASan, Valgrind, aur glibc ka apna "double free or corruption"
abort. **Fix:** ownership saaf rakho (ek hi maalik); `std::unique_ptr` (move hota
hai, double-free kar hi nahi sakta).

---

## 6. Memory leak

```cpp
int* p = new int[1000];  return;           // ⚠️ delete[] kabhi nahi hua -> leak
void f() { p = new T; if (err) return; delete p; }   // ⚠️ error path pe leak
```
**Pakdo kaise:** LSan (`-fsanitize=leak`), Valgrind, heaptrack. **Fix:** RAII —
`std::vector` / `std::unique_ptr`; har path free kare (ya free karne ko kuch ho hi na).

---

## 7. `new`/`delete` ka mismatch

```cpp
int* a = new int[10];  delete a;           // ⚠️ delete[] hona chahiye tha
int* b = new int;      delete[] b;         // ⚠️ delete hona chahiye tha
T* c = std::malloc(sizeof(T));  delete c;  // ⚠️ malloc <-> free, new <-> delete
```
**Pakdo kaise:** ASan ("alloc-dealloc-mismatch"), Valgrind. **Fix:** jodi milao;
ya raw allocation se bacho hi.

---

## 8. Pointer se out-of-bounds

```cpp
int* p = arr;  p[10] = 0;                   // ⚠️ raw pointers pe koi bounds check nahi
for (int* it = arr; it <= arr + n; ++it) *it;   // ⚠️ one-past-end deref kar raha hai
```
**Pakdo kaise:** ASan, `-Warray-bounds` (constant ke liye),
`-D_GLIBCXX_ASSERTIONS` (sirf STL). **Fix:** untrusted indices ke liye
`std::span` + `.at()`; aur half-open ranges use karo.

---

## 9. Invalid ho chuka pointer/iterator (container realloc)

```cpp
int* p = &v[0];  v.push_back(x);  *p;       // ⚠️ realloc -> dangle
auto it = m.begin();  m.erase(it);  ++it;   // ⚠️ erase ho gaya -> UB
```
**Pakdo kaise:** ASan, `-D_GLIBCXX_DEBUG`. **Fix:** `reserve()`; indices use karo;
modify ke baad dobara fetch karo; `erase` khud agla valid iterator return karta hai.

---

## 10. Pointer arithmetic ka UB

```cpp
int* p = arr - 1;                           // ⚠️ UB (array se pehle)
&a[0] - &b[0];                              // ⚠️ UB (alag arrays)
```
**Pakdo kaise:** UBSan (`-fsanitize=pointer-overflow`), aur code review. **Fix:**
ek hi array ke `[begin, end]` ke andar raho.

---

## 11. Type-punning / strict-aliasing violation

```cpp
float f = 1.0f;
int i = *reinterpret_cast<int*>(&f);        // ⚠️ float ke bytes ko int maan ke padha -> aliasing UB
```
**Pakdo kaise:** `-fstrict-aliasing -Wstrict-aliasing`, UBSan. **Fix:**
`std::memcpy` ya `std::bit_cast` (folder 11 file 08, folder 25).

---

## 12. Temporary ka pointer return karna

```cpp
const char* name() { return std::string("x").c_str(); }   // ⚠️ temp khatam -> dangling
```
**Pakdo kaise:** `-Wdangling` (thoda-bahut), ASan. **Fix:** value se return karo;
ya maalik ko zinda rakho.

---

## 13. Unaligned access (packed structs — folder 11 file 06)

```cpp
std::uint32_t* p = &packedStruct.field;    // ⚠️ misaligned pointer -> strict targets pe deref UB
```
**Pakdo kaise:** UBSan (`-fsanitize=alignment`), `-Waddress-of-packed-member`.
**Fix:** value ko `memcpy` karke bahar nikalo.

---

## 14. Ownership ka confusion (delete karega kaun?)

```cpp
Widget* w = factory.create();   // maalik caller hai ya factory?
// ... koi delete na kare to leak, dono karein to double-free
```
**Pakdo kaise:** review, ASan (double-free / leak). **Fix:** ownership ko **type
mein likho** — `std::unique_ptr<Widget>` return matlab "caller maalik hai"; raw
`Widget*` matlab "udhaar hai, delete mat karna" (folder 17).

---

## Ek page ka defense

```
COMPILE:  -Wall -Wextra -Wshadow -Wnull-dereference -Wreturn-local-addr
          -Wdangling -Wuninitialized  (aur CI mein -Werror)
RUNTIME:  -fsanitize=address,undefined   (Linux/Clang; MinGW pe nahi)
          -D_GLIBCXX_ASSERTIONS  (kahin bhi; is course ke GCC pe default on)
          Valgrind gehri heap analysis ke liye
DESIGN:   RAII -- std::vector / std::unique_ptr / std::string memory ke maalik
          Non-owning access -- references / std::span / std::string_view
          Growable storage mein pointers nahi, indices
          Ownership type mein likha ho
          Parsers ko fuzz karo; har test pe CI mein ASan
```

> **HFT relevance:** Yahan ka har bug ek production incident ki class hai — session
> ke beech crash, corrupt book, ya security hole. HFT codebases poore test suite
> pe ASan+UBSan chalate hain, saare parsers fuzz karte hain, raw owning pointers
> ban kar dete hain (arenas/pools/`unique_ptr`), growable storage mein pointers ki
> jagah stable indices/slabs use karte hain, aur lifetime/aliasing warnings pe
> `-Werror` lagate hain. Hot path aise design hota hai ki koi pointer dangle kar
> hi na sake (preallocated, reserved, single-writer). Folders 17, 27, 36, 45.

---

## Hands-on

`examples/06_dangling_pointer.cpp` (dangling/UAF/realloc), aur folder 09 ka
`06_oob_asan.cpp` (OOB):

```bash
./build.ps1 12-POINTERS/examples/06_dangling_pointer.cpp
./build.ps1 san 12-POINTERS/examples/06_dangling_pointer.cpp   # MinGW pe hardened fallback
```

---

## Exercises

1. **Pehchano:** har snippet ke liye upar wale catalog se bug ka naam batao:
   ```cpp
   int* p; *p = 1;
   int* q = new int; delete q; std::cout << *q;
   int* r = new int[5]; delete r;
   int a[3]; int* s = &a[3]; *s = 0;
   const char* t = getName().c_str(); puts(t);
   ```

2. **Har ek ko fix karo** (jahan ho sake wahan RAII se).

3. **ASan lab (Linux/WSL):** `06_dangling_pointer.cpp` ko
   `-fsanitize=address,undefined` se compile karo. BUG 1, BUG 2, BUG 3 ke liye
   exact error type aur line note karo.

4. **Error path pe leak:** ek function likho jo `new` kare, phir aisa kaam kare
   jo `throw`/jaldi `return` kar sakta hai, aur leak ho jaaye. Use
   `std::unique_ptr` se fix karo (RAII har exit pe free karta hai).

5. **Ownership type mein:** do function signatures design karo — ek jo owning
   handle return kare, doosra jo borrowed view. Ownership bilkul saaf hona chahiye.

6. **Warning audit:** apne pehle wale pointer exercises ko
   `-Wall -Wextra -Wshadow -Wnull-dereference -Wdangling -Wreturn-local-addr
   -Werror` se compile karo. Sab theek karo.

---

## Interview questions

1. Pointer bugs ki 6 classes batao. Har ek ko detect kaise karoge?
2. Use-after-free vs double-free — mechanism, aur khatra?
3. `new`/`delete` vs `new[]`/`delete[]` vs `malloc`/`free` — mismatch ka anjaam?
4. Container reallocation — kya invalid karta hai, kaise bachein?
5. Strict aliasing violation — kya hai, aur safe alternative?
6. Pointer bugs ke khilaf modern C++ defense (RAII, ownership, tools)?

---

## Next
→ [`14-exercises.md`](14-exercises.md)

# 12 — Dangling pointers, use-after-free

## Prerequisites
- [`03-dereferencing.md`](03-dereferencing.md), [`04-nullptr.md`](04-nullptr.md)
- `08-FUNCTIONS/06-scope-and-lifetime.md`, `09-ARRAYS/11-common-array-bugs.md`

## Yeh topic abhi kyun
**Dangling pointer** = pointer jiska target ki lifetime khatam ho gayi (local
scope se bahar, function return, `delete`, container reallocation). Uska deref
(read ya write) UB. Yeh C++ ke sabse ghaatak bugs hain — aksar chupchaap chalte
rehte hain, aur aksar security holes ban jaate hain.

---

## Pointer dangle kaise hota hai — 5 tareeke

### 1. Local ka pointer (use-after-return)
```cpp
int* danglingLocal() {
    int x = 42;
    return &x;              // ⚠️ return ke baad x ka stack frame khatam
}
int* p = danglingLocal();
*p;                         // ⚠️ UB -- reuse ho chuki stack slot padh rahe ho
```
Is khaas case ko `-Wreturn-local-addr` pakad leta hai.

### 2. Pointer apne block scope se zyada jee gaya
```cpp
int* p;
{ int t = 1; p = &t; }     // } pe t destroy ho gaya
*p;                         // ⚠️ dangling
```
Kuch cases `-Wdangling-pointer` pakadta hai.

### 3. Use-after-free (heap)
```cpp
int* p = new int(99);
delete p;                   // memory allocator ko wapas chali gayi
*p;                         // ⚠️ free ho chuki memory padh rahe ho (garbage / kisi aur ko de di gayi)
*p = 7;                     // ⚠️ free memory mein LIKH rahe ho -- allocator ya doosra object corrupt
delete p;                   // ⚠️ double-free -- yeh bhi UB
```

### 4. Container realloc pointers ko invalid kar deta hai
```cpp
std::vector<int> v = {1, 2, 3};
int* p = &v[0];             // p -> v ka abhi wala buffer
v.push_back(4);             // ⚠️ capacity bhar gayi -> naya buffer, purana free -> p dangle
*p;                         // ⚠️ UB
```
Yeh bhi: `insert`, `erase`, `resize`, `reserve` (badhne pe), `clear` + dobara
bharna, `std::string` ka mutation. (Folder 09 file 11, folder 07 file 07.)

### 5. Us element ka pointer jo erase/move ho gaya
```cpp
std::list<int> l = {1, 2, 3};
int* p = &l.front();
l.pop_front();              // ⚠️ woh node delete ho gaya -> p dangle
```

---

## Itna khatarnak kyun

| | Dangling READ | Dangling WRITE |
|---|---|---|
| Turant | purani value / garbage / (unmapped hua to) crash | jo ab wahan rehta hai use corrupt kar deta hai |
| Baad mein | galat results, galat branches | **der se, door kahin** crash ya galat data |
| Security | info leak (bagal ka secret data padh liya) | **arbitrary write** — classic exploit primitive (use-after-free) |

"Testing mein to chal raha tha" ka koi matlab nahi hai — UB tab tak theek dikh
sakta hai jab tak timing / allocator state / optimization na badle.

---

## Detection

| Tool | Kya pakadta hai |
|---|---|
| `-Wreturn-local-addr`, `-Wdangling-pointer` (`-Wall`) | kuch compile-time cases |
| **AddressSanitizer** (`-fsanitize=address`) | heap/stack use-after-free/return, exact line. **Sabse behtar.** Linux/macOS/Clang. ⚠️ **MinGW pe nahi** |
| **Valgrind memcheck** | heap UAF, leaks, uninitialized reads (Linux, slow) |
| `-D_GLIBCXX_DEBUG` / `_GLIBCXX_ASSERTIONS` | kuch iterator/container misuse |
| MSan (memory sanitizer) | uninitialized reads |

Is course ke MinGW toolchain pe ASan nahi hai → UAF demos ke liye WSL / Linux /
Clang use karo. `-D_GLIBCXX_ASSERTIONS` (is GCC pe default on) STL ka `[]` OOB
pakadta hai, par raw UAF nahi.

---

## Rokna kaise hai — asli fix

1. **Lifetimes match karao.** Pointer apne target se zyada nahi jeena chahiye.
   Agar yeh guarantee nahi de sakte, to raw pointer rakho hi mat.
2. **`&local` return mat karo.** Value se return karo (RVO, folder 08), ya
   out-parameter lo.
3. **Raw `new`/`delete` use mat karo.** `std::vector`, `std::unique_ptr`,
   `std::shared_ptr`, `std::string` memory ke maalik hain aur use theek waqt pe
   free karte hain (folders 14, 17). `delete` hi nahi → na double-free, na leak.
4. **Agar raw use karna hi pade, to `delete p;` ke baad `p = nullptr;` karo** —
   isse ek chupa hua UAF saaf null-deref crash ban jaata hai.
5. **Vectors ko `reserve()` karo** taaki woh kabhi realloc na hon, ya size badalne
   ke baad `&v[i]` **dobara lo**. Isse behtar: vector mein pointers ki jagah
   **indices** use karo.
6. **Non-owning access ke liye references / `std::span` / `std::string_view`** —
   yeh bhi dangle kar sakte hain, par lifetime ka rishta saaf dikhta hai aur yeh
   null nahi ho sakte.
7. **CI mein ASan/UBSan chalao, parsers ko fuzz karo.**

---

## Andar kya hota hai

- Function return hone ke baad uski stack slot agli call *reuse* kar leti hai —
  `&local` wale bytes mein ab kuch aur pada hai. `*p` unhi bytes ko padhta/likhta hai.
- `delete p` (ya `free`) block ko allocator ki free list mein daal deta hai; woh
  bytes agle `new` ko mil sakte hain aur wahan koi doosra object construct ho
  sakta hai. Tab `*p` usi object ko alias karta hai → write usse corrupt karta
  hai, read uska data dikhata hai.
- `vector` ka growth: naya bada buffer `new` karo, elements `move`/`copy` karo,
  purana buffer `delete` karo. Purane buffer ka koi bhi pointer/iterator ab free
  memory pe point kar raha hai.
- ASan free/out-of-scope memory ko redzone se "poison" karta hai aur ek
  shadow-memory map rakhta hai; poisoned memory ko chhua nahi ki report aa gayi.

> **HFT relevance:** Market-data / order-book path mein use-after-free matlab
> crash-ya-corruption, aur saath mein ek security issue. HFT code mein bachaav:
> **koi raw owning pointer nahi** (arenas / pools / `unique_ptr` maalik hain;
> folder 36), growable storage mein pointers ki jagah **indices ya stable slabs**,
> **preallocate + reserve** taaki hot path pe kuch realloc hi na ho,
> `std::span`/`string_view` ki lifetime har parse ke hisaab se sochi jaati hai
> (buffers recycle hote hain → jo fields rakhne hain unki copy lo), aur CI mein
> ASan/UBSan + fuzzing. Receive-buffer swap ke baad dangling `string_view`
> zero-copy parser ka classic bug hai (folder 10 file 09, folder 38).

---

## Hands-on

`examples/06_dangling_pointer.cpp` — 4 dangling patterns (use-after-return,
use-after-free, vector realloc, scope end) + unke fixes. Yeh 2 intentional
warnings ke saath compile hoti hai; chalao aur stale values dekho / (Linux +
ASan pe) exact reports:

```bash
./build.ps1 12-POINTERS/examples/06_dangling_pointer.cpp
# asli diagnosis ke liye Linux/WSL:
# g++ -std=c++20 -fsanitize=address,undefined -g 06_dangling_pointer.cpp -o dp && ./dp
```

---

## ⚠️ Traps

### Trap 1 — `&local` return karna
```cpp
int* f() { int x = 1; return &x; }   // ⚠️ -Wreturn-local-addr. Value se return karo
```

### Trap 2 — `delete p;` ke baad `p` use karna
```cpp
delete p;  *p = 5;   // ⚠️ UAF. delete p; p = nullptr; likho
```

### Trap 3 — `push_back` ke aar-paar vector ka pointer
```cpp
int* p = &v[0];  v.push_back(x);  *p;   // ⚠️ dangle ho sakta hai. reserve karo, ya index use karo
```

### Trap 4 — double-free
```cpp
delete p;  delete p;   // ⚠️ UB. (unique_ptr se yeh ho hi nahi sakta)
```

### Trap 5 — "mere machine pe to chal raha hai"
Jo UB chalta dikhta hai woh bhi UB hi hai — allocator, timing ya `-O` level
badla nahi ki toot gaya.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Dangling deref kabhi-kabhi sahi value deta hai" | UB — value kismat se aati hai; write memory corrupt karta hai |
| "`delete` ke baad pointer null ho jaata hai" | Woh waisa hi rehta hai — `nullptr` khud set karo |
| "Dangling pointer se sirf reads bure hain" | Writes zyada bure — kahin bhi corruption |
| "`push_back` purane elements ko nahi hilata" | Capacity bharne pe woh realloc karta hai |
| "Compiler dangling pointers pakad leta hai" | Sirf kuch; baaki ke liye ASan/Valgrind/tests |

---

## Exercises

1. **Use-after-return:** `06_dangling_pointer.cpp` ka BUG 1 chalao. `*p1` kya
   print karta hai? Function dobara call karo — `*p1` badla?

2. **UAF write:** `int* p = new int(1); delete p; *p = 999;` — `-O0` pe chalao.
   Crash hua? Ab Linux pe `-fsanitize=address` se — report kya kehti hai?

3. **Vector realloc:** `std::vector<int> v{1}; int* p = &v[0]; for (int i=0;i<100;
   ++i) v.push_back(i); std::cout << *p;` — value kya? Do tareeke se fix karo
   (`reserve`, index).

4. **Null karne ki aadat:** ek raw `new`/`delete` snippet ko aise badlo ki
   `delete` ke baad pointer `nullptr` ho jaaye. Dikhao ki ab `*p` saaf crash
   karta hai.

5. **Ownership fix:** `int* p = new int[10]; ... delete[] p;` ko `std::vector<int>`
   se badlo (folder 14 ki jhalak). `delete` nahi likha — to memory kahan gayi?

6. **Double-free:** `int* a = new int; int* b = a; delete a; delete b;` — UB hai.
   `std::unique_ptr` ise namumkin kaise bana deta hai?

---

## Interview questions

1. Dangling pointer kya hai? 5 tareeke jinse banta hai?
2. Dangling READ vs WRITE — kaunsa zyada khatarnak, kyun (security)?
3. `delete p` ke baad `p` ki value? Best practice?
4. `vector::push_back` pointers/iterators ko kab invalidate karta hai?
5. Dangling pointers detect karne ke tools?
6. Prevention — modern C++ approach (ownership, RAII)?

---

## Next
→ [`13-pointer-bugs-catalog.md`](13-pointer-bugs-catalog.md)

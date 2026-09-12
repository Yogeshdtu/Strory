# 03 — Pointers & memory problems

## Prerequisites
- `12-POINTERS/`, `13-REFERENCES/`, `14-MEMORY/`
- RAII: `17-RAII/`
- `18-COPY-MOVE/` (deep copy problems ke liye)
- Linked lists: `20-ALGORITHMS-DSA/06-linked-lists.md`

## Yeh file kya hai
25 problems — raw pointers, pointer arithmetic, manual memory, allocators,
linked structures, lifetime. Yeh woh area hai jahan HFT interview sabse deep
jaata hai.

Har problem: statement + `Pattern:` hint + `<details>`. Poora code →
[`11-solutions/03-pointers-memory-solutions.md`](11-solutions/03-pointers-memory-solutions.md).

Sab kuch sanitizer ke neeche test karo:
`g++ -std=c++20 -Wall -Wextra -g -fsanitize=address,undefined file.cpp` (Linux/
WSL — MinGW pe libasan nahi; folder 45 dekho).

---

## Part A — Easy (~10–15 min)

### A1. Swap via pointers
`void swap(int* a, int* b)` — bina temp value copy ke (ya XOR trick) aur temp ke
saath. Dono.
`Pattern:` dereference both, exchange.
<details><summary>Approach</summary>

`int t = *a; *a = *b; *b = t;`. XOR: `*a ^= *b; *b ^= *a; *a ^= *b;` — **par**
`a == b` pe zero kar deta hai, aur readability zero. Interview: mention the aliasing
bug. `O(1)`.
</details>

### A2. Reimplement strlen / strcpy / strcmp
`const char*` pe, standard library ke bina.
`Pattern:` walk to the `'\0'`.
<details><summary>Approach</summary>

`strlen`: `const char* p = s; while (*p) ++p; return p - s;`. `strcpy`: `while
((*d++ = *s++));` (classic; return original `d`). `strcmp`: advance while equal
and non-null; return `(unsigned char)*a - (unsigned char)*b`. Buffer size
`strcpy` mein caller ki zimmedari — isliye asli code mein `strncpy`/`snprintf`.
</details>

### A3. Array vs pointer decay
```cpp
int a[10];
sizeof(a);            // ?
void f(int p[10]) { sizeof(p); }   // ?
```
Dono `sizeof` kya, kyun alag?
<details><summary>Answer</summary>

`sizeof(a)` = `40` (10 × `int`). Function param `int p[10]` actually `int* p` hai
(array decays to pointer) → `sizeof(p)` = `8`. Isliye `sizeof(arr)/sizeof(arr[0])`
sirf real array pe kaam karta hai, decayed pointer pe nahi. Length pass karo ya
`std::span` / `std::array&`.
</details>

### A4. Walk a flat 2D array
`int* data` ek `rows × cols` matrix ko row-major flat store karta hai. `(r, c)`
element ka pointer? Poori row ka sum?
`Pattern:` `data + r * cols + c`.
<details><summary>Approach</summary>

`*(data + r * cols + c)`. Row sum: `data + r*cols` se `cols` elements. Row-major
walk cache-friendly (stride 1); column walk stride `cols` — slow (folder 32).
</details>

### A5. Classify these pointers
Har snippet: null / dangling / uninitialized / valid?
```cpp
int* a;                       // (1)
int* b = nullptr;             // (2)
int* c = new int(5); delete c;// (3) c abhi?
int* d = &(*std::vector<int>{1,2,3}.begin()); // (4)
```
<details><summary>Answer</summary>

(1) uninitialized — indeterminate, read = UB. (2) null — deref = UB par
well-defined "null". (3) dangling — `c` ab freed memory ko point karta hai;
`delete c` phir se = double free. `c = nullptr` set karo. (4) dangling — temporary
`vector` statement ke baad destroy, `d` garbage. ASan pakadta hai; Clang `-Wdangling` bhi (GCC 16.2 `-Wall` ne
temporary vector ke iterator pe warning nahi di — chala ke dekha).
</details>

### A6. `const` placement
Batao har ek kya lock karta hai:
`const int* p`, `int* const p`, `const int* const p`, `int const* p`.
<details><summary>Answer</summary>

`const int* p` == `int const* p`: pointee read-only, pointer reseat ho sakta.
`int* const p`: pointer fixed, pointee mutable. `const int* const p`: dono fixed.
Padho right-to-left: "`p` is a const pointer to const int".
</details>

### A7. Function pointer dispatch table
4 operations (`+ - * /`) ko `enum Op` se index kiya jaaye — array of function
pointers.
`Pattern:` `int (*table[4])(int,int)`.
<details><summary>Approach</summary>

`using Fn = int(*)(int,int); static const Fn table[] = {add, sub, mul, divi};`
`table[op](x, y)`. `O(1)` dispatch, no branch chain. HFT: message-type → handler
table (jump table) branch-mispredict se bachne ke liye.
</details>

### A8. Out-param vs return
Ek function jo `int` aur `bool ok` dono deta hai — teen designs: out-pointer,
`std::pair`, `std::optional`. Kab kaunsa?
<details><summary>Answer</summary>

`std::optional<int>` cleanest jab "no value" ek valid outcome hai. `std::pair` /
struct jab dono cheezein hamesha meaningful. Out-pointer (`bool f(int* result)`)
purani C-style API compatibility ya jab caller buffer ka owner hai. Hot path pe
`optional<int>` zero-overhead (no alloc).
</details>

---

## Part B — Medium (~25 min)

### B1. Bump / arena allocator
`Arena` — ek bade buffer se `allocate(size, align)` karta hai, individual free
nahi, poora `reset()` ek baar. Alignment sahi karo.
`Pattern:` `ptr = align_up(cur, align); cur = ptr + size;`
<details><summary>Approach</summary>

`cur = (cur + align - 1) & ~(align - 1)` (align power-of-two). `cur + size >
end` → nullptr / new block. `reset()` = `cur = base`. `allocate` `O(1)`, zero
per-object bookkeeping. HFT: per-message scratch memory.
</details>

### B2. Fixed-size free-list allocator
`Pool<T>` — `N` slots ek array mein, ek free list `next` index/pointer ke saath.
`acquire()` / `release(T*)` dono `O(1)`.
`Pattern:` intrusive singly linked free list through the storage.
<details><summary>Approach</summary>

Free slot ke andar hi `next` pointer store karo (union / raw bytes). `acquire`:
head pop, placement-new. `release`: `~T()`, push to head. Exhaustion → nullptr.
No `malloc` after construction. Solutions file mein poora code.
</details>

### B3. Aligned storage + placement new
`alignas` buffer mein `T` construct karo bina `new T`, phir manually destroy.
`Pattern:` `std::aligned_storage` / `alignas(T) std::byte[sizeof(T)]` + placement
`new`.
<details><summary>Approach</summary>

`alignas(T) std::byte buf[sizeof(T)]; T* p = ::new (buf) T(args…); … p->~T();`.
`std::launder` technically chahiye re-access ke liye C++17+. Yeh `optional`,
`variant`, small-buffer sabka core.
</details>

### B4. Small-buffer-optimized string
`SmallStr` — `<= 15` chars inline (no heap), bade strings heap pe. Move-safe.
`Pattern:` union { inline buf; heap ptr+cap; } + size + a "is-inline" flag.
<details><summary>Approach</summary>

`size <= 15` → `char buf[16]` (last byte spare / null). Warna `{char* data;
size_t cap;}`. `libstdc++` `std::string` yehi karta hai (15-char SSO). Move: heap
case pointer steal + source ko inline-empty; inline case memcpy. Destructor sirf
heap case pe `delete[]`.
</details>

### B5. Intrusive ref-counted handle
`RcPtr<T>` jahan `T` ke andar hi `int refcount` hai. `add_ref` / `release`
(zero pe `delete`).
`Pattern:` non-atomic vs atomic counter.
<details><summary>Approach</summary>

Copy → `++obj->rc`; destroy → `if (--obj->rc == 0) delete obj;`. Single-thread →
plain `int`. Shared across threads → `std::atomic<int>`, `fetch_add(relaxed)` for
inc, `fetch_sub(acq_rel)` for dec (release so prior writes flush, acquire on the
zero transition before `delete`). Intrusive = one alloc (vs `shared_ptr` ka
control block).
</details>

### B6. `unique_ptr` from scratch
Move-only `UniquePtr<T, Deleter>` — ctor, dtor, move ctor/assign, `release()`,
`reset()`, `operator*`/`->`/`bool`. Copy deleted.
`Pattern:` Rule of 5, "own exactly one".
<details><summary>Approach</summary>

Move: `ptr_ = std::exchange(o.ptr_, nullptr)`. Move-assign: self-check / `reset`
old, steal. Dtor: `if (ptr_) del_(ptr_)`. `release` = `exchange(ptr_, nullptr)`
(no delete). EBO ke liye deleter ko `[[no_unique_address]]` / base. Solutions
file.
</details>

### B7. `shared_ptr` control block sketch
Control block ka layout: strong count, weak count, deleter, aur (optional)
object storage (`make_shared`). `weak_ptr` implement mat karo — bas describe.
<details><summary>Answer</summary>

`shared_ptr` = `{T* ptr; ControlBlock* cb;}`. `cb` = `{atomic<long> strong;
atomic<long> weak; /* deleter, allocator */}`. `strong→0`: object destroy;
`weak→0` (aur strong bhi 0): control block free. `make_shared`: object + cb ek
allocation mein (cache-friendly, par weak refs poori memory ko zinda rakhte hain).
</details>

### B8. Circular buffer (raw pointers)
`RingBuf<T>` fixed capacity, `push` / `pop`, full/empty distinguish.
`Pattern:` head/tail indices, power-of-two mask ya `size` counter.
<details><summary>Approach</summary>

Capacity `CAP` (power of two): `head & (CAP-1)`. Full vs empty: ya to ek slot
khaali chhodo (`(tail+1)&mask == head` → full), ya alag `size_t count`. `push`/
`pop` `O(1)`, zero alloc. Lock-free single-producer/consumer version → file 08.
</details>

### B9. Deep copy vs shallow copy
Ek `struct Tree { int v; Tree* left; Tree* right; };` — copy ctor jo poora
subtree clone kare. Aur batao shallow copy kya todta hai.
`Pattern:` recursive clone.
<details><summary>Approach</summary>

`Tree(const Tree& o) : v(o.v), left(o.left ? new Tree(*o.left) : nullptr),
right(…) {}`. Dtor recursive delete. Shallow copy (default) → do objects same
children point karte → double delete + ek modify doosre ko dikhta. Rule of 3/5.
</details>

### B10. Detect a cycle in a linked list
`bool hasCycle(Node* head)` — `O(1)` space. Cycle ka **start node** bhi
nikaalo.
`Pattern:` Floyd's tortoise & hare.
<details><summary>Approach</summary>

`slow += 1, fast += 2`; milte hain → cycle. Start: ek pointer `head` se, doosra
meeting point se, dono `+1` — jahan milte hain wahi cycle start. `O(n)`/`O(1)`.
Proof: distance algebra (`x = (n-1)·loop + (loop - y)`).
</details>

### B11. Reverse a linked list
Iterative (`O(1)` space) aur recursive (`O(n)` stack). Dono, no leaks.
`Pattern:` three-pointer prev/cur/next.
<details><summary>Approach</summary>

`while (cur) { next = cur->next; cur->next = prev; prev = cur; cur = next; }`.
Recursive: tail reverse karo, `head->next->next = head; head->next = nullptr;`.
Long list pe recursion = stack overflow risk — mention.
</details>

### B12. Flatten a multilevel doubly linked list
Har node ka `next`, `prev`, aur optional `child` (ek aur aisi list). Ek flat
`next`/`prev` list banao, DFS order.
`Pattern:` iterative DFS with a stack, ya recursive splice.
<details><summary>Approach</summary>

Node pe `child` mile → `child` list ko `cur` aur `cur->next` ke beech splice
karo (`cur->next` ko stack pe push ya tail dhundh ke reconnect), `child =
nullptr`. `O(n)`. `prev` pointers bhi fix karo.
</details>

---

## Part C — Hard (~40+ min)

### C1. Implement `memmove`
`memcpy` overlapping regions pe UB hai; `memmove` sahi hota hai. Likho.
`Pattern:` copy direction depends on `src` vs `dst` order.
<details><summary>Approach</summary>

`dst < src` → forward copy (`i = 0..n`). `dst > src` → backward (`i = n-1..0`),
warna aage ki copy peeche padhne se pehle overwrite kar de. `dst == src` → nop.
`O(n)`. Real `memmove` word-at-a-time + alignment; interview mein byte loop kaafi.
</details>

### C2. Slab allocator with size classes
`SlabAlloc` — kai size classes (16, 32, 64, 128, …), har class ka apna free
list of fixed-size blocks, blocks bade "slab" pages se katte hain.
`Pattern:` size → class index (round up / `bit_ceil`), per-class free list, page
refill.
<details><summary>Approach</summary>

`allocate(n)`: class = smallest `>= n`; free list empty → ek nayi slab page
`mmap`/`malloc` karo, usse blocks kaat ke list bharo; head pop. `free(p, n)`:
class nikaalo, push. `O(1)` amortized. Kernel slab allocator ka simplified model.
</details>

### C3. Intrusive doubly linked list
`Node` payload ke andar `{Node* prev; Node* next;}` (hook). `O(1)` `unlink(Node*)`
bina list head ke. Sentinel node use karo.
`Pattern:` circular list with a dummy head; node knows its neighbours.
<details><summary>Approach</summary>

`unlink`: `n->prev->next = n->next; n->next->prev = n->prev;` — no search, no
alloc. Payload apni memory ka owner (pool). Linux kernel `list_head` yehi.
`std::list` ke against: no per-node alloc, node ko multiple lists mein rakh sakte.
</details>

### C4. Pool that outlives a `fork()` / arena lifetime
Discussion: ek memory pool jo `fork()` ke baad child mein bhi valid ho; aur
"arena per request" model mein object lifetime ko arena ke lifetime se kaise
bind karte ho.
<details><summary>Answer</summary>

`fork()` COW se child ko same virtual mapping deta hai — pool jo `mmap(MAP_SHARED
| MAP_ANONYMOUS)` ya pre-fork allocated hai, dono mein valid pointers. `MAP_PRIVATE`
→ writes diverge. Arena model: objects ko trivially-destructible rakho (ya arena
destructors ki list rakhe) → arena `reset()` = saare objects "gone" bina
per-object `~T()`. Dangling risk: arena ke bahar pointer leak na ho — lifetime
annotate karo.
</details>

### C5. Tagged pointer
`8`-byte aligned pointers ke low 3 bits free hain. Ek `TaggedPtr<T>` jo pointer +
3-bit tag ek `uintptr_t` mein pack kare.
`Pattern:` `bits = (uintptr_t)p | tag; p = bits & ~7; tag = bits & 7;`
<details><summary>Approach</summary>

Assert `alignof(T) >= 8`. `set(p, tag)`: `raw_ = reinterpret_cast<uintptr_t>(p) |
(tag & 7)`. `ptr()`: `reinterpret_cast<T*>(raw_ & ~uintptr_t{7})`. Use: lock-free
structures mein ABA counter, RB-tree node color, GC mark bit. Portability caveat:
alignment guarantee lazmi.
</details>

### C6. Hazard pointers (concept)
Lock-free stack se node `pop` karke `delete` karna — doosra thread abhi bhi us
node ko padh raha ho sakta hai. Hazard pointers isse kaise safe banate hain?
<details><summary>Answer</summary>

Har reader thread ek "hazard pointer" slot mein woh node publish karta hai jise
woh abhi access kar raha hai (`release` store). Jo thread `delete` karna chahta
hai woh node ko ek per-thread retire-list mein daalta hai; periodically saare
hazard slots scan karta hai — koi bhi retired node jo kisi hazard slot mein nahi,
usko `delete`. `O(1)` read overhead, deferred reclamation. RCU alternative:
readers free, writer grace-period ka wait karta hai. (Folder 28.)
</details>

---

## Next
→ [`04-oop-design-problems.md`](04-oop-design-problems.md)

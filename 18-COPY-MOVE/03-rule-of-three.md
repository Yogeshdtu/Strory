# 03 — Rule of Three

## Prerequisites
- [`01-copy-constructor.md`](01-copy-constructor.md), [`02-copy-assignment.md`](02-copy-assignment.md)
- Folder 15 file 06 (destructors), folder 14 file 06 (double-free)

## Yeh topic abhi kyun
**Rule of Three** (C++98 era): *agar aapki class ko in teen mein se ek chahiye,
to teenon chahiye* —
1. **Destructor**
2. **Copy constructor**
3. **Copy assignment operator**

Kyun: yeh teenon ek hi cheez manage karte hain — ek **raw resource** ka
ownership. Ek likha aur baaki chhode → compiler ke default (shallow) versions
resource ke saath galat kaam karte → leak / double-free.

---

## Kyun teenon ek saath jaate hain

```cpp
class Buffer {
    int* data_;
    size_t n_;
public:
    Buffer(size_t n) : data_(new int[n]), n_(n) {}
    ~Buffer() { delete[] data_; }          // (1) DESTRUCTOR -- releases the raw resource
    // (2) copy ctor?  (3) copy assign?  -- NOT declared -> compiler generates SHALLOW ones
};
```

- The **destructor** exists because the class owns a raw `int[]`.
- The compiler-generated **copy ctor** does `b.data_ = a.data_` (pointer bit-copy)
  → two `Buffer`s share one buffer → both destructors `delete[]` it →
  **double-free** (folder 14 file 06).
- The compiler-generated **copy assignment** does the same on `b = a` → plus it
  leaks `b`'s original buffer.

So: the moment you write `~Buffer()` to manage a raw resource, the default copy
operations become **wrong**. You must supply correct ones — hence all three.

---

## The correct triad

```cpp
class Buffer {
    size_t n_    = 0;
    int*   data_ = nullptr;
public:
    explicit Buffer(size_t n) : n_(n), data_(new int[n]) {}

    // (1) DESTRUCTOR
    ~Buffer() { delete[] data_; }

    // (2) COPY CONSTRUCTOR -- deep copy
    Buffer(const Buffer& o) : n_(o.n_), data_(new int[o.n_]) {
        std::memcpy(data_, o.data_, n_ * sizeof(int));
    }

    // (3) COPY ASSIGNMENT -- deep, self-safe, exception-safe
    Buffer& operator=(const Buffer& o) {
        if (this != &o) {
            int* fresh = new int[o.n_];
            std::memcpy(fresh, o.data_, o.n_ * sizeof(int));
            delete[] data_;
            data_ = fresh;
            n_    = o.n_;
        }
        return *this;
    }
};
```

`examples/02_rule_of_three.cpp` has this exact triad (with a counted `operator
new[]` proving no leak / no double-free) and a commented broken "destructor-only"
version.

---

## Rule of Three → Rule of Five (C++11) → Rule of Zero

| | What | When |
|---|---|---|
| **Rule of Three** | dtor + copy ctor + copy assign | class owns a raw resource (pre-C++11 style) |
| **Rule of Five** | + move ctor + move assign | same, plus you want move (file 09) |
| **Rule of Zero** | *none* — all five compiler-generated | every member is already RAII (best — file 10) |

Modern guidance: **aim for Rule of Zero.** If a class needs to manage a resource,
first ask "can a `std::vector` / `std::string` / `std::unique_ptr` / a tiny RAII
wrapper (folder 17 file 09) do it?" — almost always yes → the outer class writes
zero special members and they're all correct.

Rule of Three/Five is what you write **inside** those RAII types themselves, or
when interfacing a C resource with unusual copy semantics.

---

## The "declare one, get the others wrong" trap

Declaring **any** of the five affects generation of the others (file 09):

```cpp
class Widget {
    std::vector<int> data_;        // already RAII
public:
    ~Widget() { std::cout << "bye\n"; }   // ⚠️ user destructor added just for logging
    // consequences:
    //  - copy ctor / copy assign: still generated (but DEPRECATED -> -Wdeprecated-copy)
    //  - MOVE ctor / MOVE assign: NO LONGER generated!
    //    -> "moves" of Widget now fall back to COPYING data_ -> silently slow
};
```

So adding one special member for a trivial reason (logging, a breakpoint) can
**disable the implicit move operations** → `std::vector<Widget>` reallocation now
copies. If you truly need one of the five, you're back to Rule of Five: declare
all five, `= default` the ones you don't customize.

`-Wdeprecated-copy` (in `-Wextra`) flags the "user dtor + implicit copy" case.

---

## Non-copyable via Rule of Three

If a resource genuinely can't be copied (a socket, a lock, a `unique_ptr`), the
"Rule of Three" answer is to **`= delete` the copy operations**:

```cpp
class Socket {
    int fd_ = -1;
public:
    explicit Socket(int fd) : fd_(fd) {}
    ~Socket() { if (fd_ >= 0) ::close(fd_); }

    Socket(const Socket&)            = delete;   // no two owners of one fd
    Socket& operator=(const Socket&) = delete;
    // (then add move ops -> Rule of Five, or leave move-disabled too)
};
```

`= delete` is still "declaring" the copy operations — you're saying "copy is not
allowed", which is a valid, complete answer to Rule of Three.

---

## Andar kya hota hai

- The compiler-generated copy ctor / copy assign are member-wise. For a raw
  pointer member, "member-wise" = copy the pointer value → aliasing. There is no
  "the compiler should have known to deep-copy" — it can't; a pointer is just a
  number.
- A user destructor that does real work makes the class non-trivially
  destructible; combined with implicit copy it's a Rule-of-Three violation the
  compiler can only *warn* about (`-Wdeprecated-copy`), not fix.
- The deep-copy ctor/assign cost: `operator new` + `memcpy(n)` per copy —
  exactly what move semantics (file 06) removes for rvalue sources, and what
  Rule of Zero delegates to well-tested library types.

> **HFT relevance:** Rule of Three/Five is rarely written in HFT application code
> — hot types are POD (trivial everything, Rule of Zero by default) or non-
> copyable owners (`= delete` copy, move-only). Where a hand-rolled resource
> wrapper is unavoidable (a kernel-bypass NIC buffer, a hugepage region), the
> full Rule of Five is written and reviewed carefully — a mis-written copy
> assignment that forgets to release the old buffer is a slow leak; one that
> forgets the self-assignment / exception ordering is a crash under load. The
> "logging destructor disables moves" trap is caught in review and by
> `-Werror=deprecated-copy`.

---

## Hands-on

```bash
./build.ps1 18-COPY-MOVE/examples/02_rule_of_three.cpp
```

`IntArray` — correct triad, counted `operator new[]` shows `outstanding=0`
(clean). Uncomment the `#if 0` broken version (`main` with `BadArray b = a;`) on
a Linux box → `free(): double free detected` or a crash.

---

## ⚠️ Traps

### Trap 1 — destructor only (Rule of Three violated)
```cpp
class C { int* p_; public: ~C() { delete[] p_; } };   // ⚠️ C b = a; -> shallow -> double-free
```

### Trap 2 — logging destructor kills implicit move
```cpp
class C { std::vector<int> v_; public: ~C() { log(); } };   // moves now copy. `= default` all five, or drop the dtor
```

### Trap 3 — copy ctor without copy assign (or vice versa)
```cpp
class C { ...; C(const C&); };   // ⚠️ b = a; uses the SHALLOW default assignment. Declare both + dtor
```

### Trap 4 — writing Rule of Three when Rule of Zero would do
```cpp
class Packet { char* buf_; ~Packet(); Packet(const Packet&); Packet& operator=(const Packet&); };
// ⚠️ std::vector<char> buf_; -> zero special members, all correct
```

### Trap 5 — forgetting move ops when you need move (Rule of Five)
```cpp
class Buf { /* dtor + copy ctor + copy assign, no move */ };
std::vector<Buf> v; v.push_back(...);   // ⚠️ realloc COPIES (no move ctor). Add move ops or Rule of Zero
```

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Just a destructor is fine for a class with `new`" | Then copy ctor + copy assign too (or `= delete`) — Rule of Three |
| "Adding a destructor is harmless" | Suppresses implicit move; deprecates implicit copy |
| "Rule of Three is the goal" | Rule of **Zero** is — wrap the resource in an RAII member |
| "`= delete` doesn't count as 'declaring' a special member" | It does — it's a valid complete answer (non-copyable) |
| "Rule of Three still relevant in modern C++" | Mostly inside RAII types / C-resource wrappers; app code aims for Rule of Zero |

---

## Exercises

1. **Apply Rule of Three:** `class Str { char* p_; public: Str(const char* s) :
   p_(strdup(s)) {} ~Str() { free(p_); } };` — add the missing two. Use
   allocate-before-free in the assignment.

   <details><summary>Answer</summary>

   `Str(const Str& o) : p_(strdup(o.p_)) {}` and `Str& operator=(const Str& o) {
   if (this != &o) { char* fresh = strdup(o.p_); free(p_); p_ = fresh; } return
   *this; }`.
   </details>

2. **Rule of Zero it:** rewrite exercise 1's `Str` with a `std::string` member.
   How many special members do you write?

   <details><summary>Answer</summary>

   `class Str { std::string s_; public: Str(const char* x) : s_(x) {} const char*
   c_str() const { return s_.c_str(); } };` — **zero** special members. Copy/move/
   dtor all generated and correct.
   </details>

3. **Spot the regression:** a `class Histogram { std::array<int, 256> bins_{};
   };` (Rule of Zero, trivially copyable). Someone adds `~Histogram() {
   dumpToFile(bins_); }`. What changed for `std::is_trivially_copyable_v<Histogram>`
   and for `std::vector<Histogram>` reallocation?

   <details><summary>Answer</summary>

   `is_trivially_copyable_v` → now `false` (user-declared destructor). `std::vector<Histogram>`
   realloc: the implicit move was suppressed → it copies each `Histogram` (fine
   here, `std::array<int,256>` copy is a memcpy) — but the type is no longer
   memcpy-relocatable in principle, and if `bins_` were an owning type it'd be a
   real slowdown. Fix: don't put I/O in the destructor.
   </details>

4. **Non-copyable Rule of Three:** make a `class UniqueBuffer` that owns `int*
   data_` and forbids copying but allows nothing else (no move yet). What do you
   declare?

   <details><summary>Answer</summary>

   `~UniqueBuffer() { delete[] data_; }`, `UniqueBuffer(const UniqueBuffer&) =
   delete;`, `UniqueBuffer& operator=(const UniqueBuffer&) = delete;`. That's a
   complete Rule of Three (destructor + both copy ops, the copy ops being
   deleted). Add move ops later for Rule of Five, or leave it immovable.
   </details>

5. **Which rule:** for each class, is it Rule of Zero, or does it need Three/Five?
   (a) `{ int x; double y; }`, (b) `{ std::string s; std::vector<int> v; }`, (c)
   `{ int* p; }` with a `~C() { delete p; }`, (d) `{ std::unique_ptr<T> u; }`.

   <details><summary>Answer</summary>

   (a) Rule of Zero (trivial). (b) Rule of Zero (RAII members). (c) needs Rule of
   Three/Five (owns a raw resource) — better: make `p` a `unique_ptr`. (d) Rule
   of Zero (move-only, generated correctly).
   </details>

---

## Interview questions

1. Rule of Three — kaunse 3, aur kyun ek saath?
2. Sirf destructor likha (raw resource) — kya bug?
3. Rule of Three → Five → Zero — kab kaunsa?
4. "Logging destructor" — kya silently break hota?
5. `= delete` copy ops — Rule of Three ka valid jawaab hai?
6. Rule of Zero kaise achieve karte, aur woh better kyun?

---

## Next
→ [`04-value-categories.md`](04-value-categories.md)

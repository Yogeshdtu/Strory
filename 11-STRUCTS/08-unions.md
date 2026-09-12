# 08 — Unions

## Prerequisites
- [`05-padding-and-alignment.md`](05-padding-and-alignment.md), [`06-packed-structs.md`](06-packed-structs.md)
- `05-OPERATORS/05-bitwise-operators.md` (bit views), `10-STRINGS/01-c-strings.md`

## Yeh topic abhi kyun
`union` ke saare members **ek hi memory** pe rehte hain. Ek waqt mein ek hi
member valid. Yeh memory-tight variants, wire-protocol alternatives, aur low-level
bit views ke liye hai — par C++ mein iske sakht rules hain, aur galat member
padhna UB. Modern code: `std::variant` (file 09).

---

## Basic

```cpp
union Value {
    std::int32_t i;
    float        f;
    char         bytes[4];
};

sizeof(Value)      // 4  -- LARGEST member (sab overlap karte hain)
alignof(Value)     // 4  -- largest member's alignment
```

```
   Value v:
   ┌────┬────┬────┬────┐
   │           4 bytes  │   -- i, f, bytes[] SABHI yahi 4 bytes hain
   └────┴────┴────┴────┘
```

```cpp
Value v;
v.i = 0x41424344;      // ab `i` active hai
v.bytes[0];            // 0x44 (little-endian) -- SAME memory, char view
v.f = 1.0f;            // ab `f` active -- purani `i` value chali gayi
```

---

## 🔑 Active member rule

C++ mein: **sirf "last written" member ko padho.** Kisi aur member ko padhna =
technically **UB** (C mein yeh allowed hai — "type punning").

```cpp
Value v;
v.f = 1.0f;
std::int32_t bits = v.i;    // ⚠️ UB in C++ (reading `i` after writing `f`)
```

Practically GCC/Clang isse allow karte hain (documented extension), par portable /
standard code ise avoid karta hai.

### Safe type punning: `memcpy` ya `std::bit_cast`

```cpp
float src = 1.0f;
std::uint32_t bits;
std::memcpy(&bits, &src, sizeof(bits));           // ✅ always well-defined

std::uint32_t bits2 = std::bit_cast<std::uint32_t>(src);   // ✅ C++20 -- same, constexpr-friendly
```

**Use `memcpy` / `bit_cast` for bit reinterpretation, not a union.**

---

## Tagged union — track the active member

A raw union doesn't know which member is active. You track it with an `enum` tag
(this is exactly what `std::variant` does for you):

```cpp
struct TaggedValue {
    enum class Tag { Int, Float, Text } tag;
    union {
        std::int64_t i;
        double       d;
        char         text[8];
    };
};

void print(const TaggedValue& t) {
    switch (t.tag) {
        case TaggedValue::Tag::Int:   /* read t.i */   break;
        case TaggedValue::Tag::Float: /* read t.d */   break;
        case TaggedValue::Tag::Text:  /* read t.text */break;
    }
}
```

`examples/05_unions.cpp` — shared memory, type punning, tagged union.

---

## Non-trivial members — you manage lifetime

```cpp
union U {
    int i;
    std::string s;        // ⚠️ non-trivial -- has a constructor/destructor
    U() {}                // union needs a user-provided ctor now
    ~U() {}               // ...and dtor -- but which member to destroy? YOU decide
};
```

When a union has a member with a non-trivial ctor/dtor (`std::string`,
`std::vector`), the union's own ctor/dtor are **deleted** unless you write them —
and you must **manually** `placement new` the active member and explicitly call
its destructor. Extremely error-prone.

**→ Use `std::variant`.** It does all this correctly.

---

## Anonymous unions

```cpp
struct Packet {
    std::uint8_t type;
    union {                        // no name -- members accessed directly on Packet
        std::uint32_t asInt;
        float         asFloat;
        std::uint8_t  asBytes[4];
    };
};

Packet p;
p.asInt = 42;                     // p.asInt, not p.someUnionName.asInt
```

Common in wire structs where a field's interpretation depends on `type`.

---

## When to use a raw union

**Yes:**
- Fixed **wire/file formats** where a field is one of several interpretations,
  selected by a type byte (anonymous union in a packed struct).
- **Memory-critical** embedded code.
- With `memcpy`/`bit_cast` semantics documented (though `bit_cast` is cleaner).

**No (use `std::variant`):**
- A general "one of these types" value in application code.
- Anything with non-trivial members.
- Anywhere you'd otherwise hand-roll a tagged union.

---

## Andar kya hota hai

- A union is `sizeof` = max member size, `alignof` = max member alignment, all
  members at offset 0.
- Writing one member and reading another → the compiler just reads the same
  bytes; the standard calls it UB (so an optimizer *may* assume it doesn't
  happen), but GCC/Clang define it.
- `std::bit_cast<To>(from)` → the compiler emits a `memcpy` (often optimized to a
  register move / `movd`), no actual copy at `-O2`.
- A union with a `std::string` → the string's pointer/length/buffer overlap
  whatever else; constructing/destructing the wrong member corrupts the heap.

> **HFT relevance:** Anonymous unions appear in packed wire structs — e.g. a
> price field that's an `int64` mantissa or an IEEE double depending on a flag.
> Decoded via `memcpy` (file 06). For internal "event is a Quote or Trade or
> Reject" types, HFT code uses `std::variant` (file 09) or a hand-rolled tagged
> union with a plain-old-data payload (no `std::string` inside) — the discriminant
> + a `switch` compiles to a cheap branch, cheaper than virtual dispatch.
> Bit-level reinterpretation (float↔bits for fast math) uses `std::bit_cast`.

---

## Hands-on

`examples/05_unions.cpp` — shared memory, type punning (+ the safe `memcpy`
way), tagged union:

```bash
./build.ps1 11-STRUCTS/examples/05_unions.cpp
```

---

## ⚠️ Traps

### Trap 1 — reading the inactive member
```cpp
u.f = 1.0f;  int b = u.i;   // ⚠️ UB (standard). memcpy / bit_cast
```

### Trap 2 — union with `std::string`, no manual lifetime
```cpp
union U { int i; std::string s; };  U u; u.s = "x";   // ⚠️ s never constructed -> UB
```

### Trap 3 — forgetting the tag
```cpp
TaggedValue t;  t.d = 1.0;  /* forgot t.tag = Float */  print(t);   // reads wrong member
```

### Trap 4 — `sizeof(union)` = sum of members
```cpp
union U { char a[10]; int b; };  sizeof(U) == 12   // max(10, 4) rounded to align 4, NOT 14
```

### Trap 5 — using a union where `std::variant` fits
Application-level "one of" types → `std::variant`. Raw union → wire/embedded only.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Reading any union member is fine (type punning)" | UB in C++ — `memcpy` / `bit_cast` |
| "`sizeof(union)` = sum of members" | Max member (aligned) |
| "Union tracks which member is active" | No — you track it (tag), or use `std::variant` |
| "Union with `std::string` just works" | You must manually construct/destruct it |
| "Union is the way to do variant types" | `std::variant` for app code; union for wire/embedded |

---

## Exercises

1. **Bit view:** `union { float f; std::uint32_t bits; };` — set `f = 3.14f`,
   print `bits` in hex. Then do the same with `std::bit_cast`. Same result?

2. **Sizeof:** `union U { double d; char c[10]; std::int16_t s; };` —
   `sizeof(U)`, `alignof(U)`. Explain.

3. **Tagged union:** `TaggedNumber` holding `int` / `double` / `bool` with a tag.
   `add(TaggedNumber, TaggedNumber)` that promotes sensibly.

4. **Anonymous union in a packed struct:** `struct Field { uint8_t kind; union {
   int32_t i; float f; char s[4]; }; };` (packed) — decode 3 sample byte buffers.

5. **`std::string` union danger:** write `union U { int i; std::string s; };` with
   manual `placement new` / explicit `~std::string()`. Get it right. Then note how
   `std::variant` does it for free.

6. **Punning UB:** `float f = 1.0f; int i = *(int*)&f;` — build with `-fsanitize=
   undefined` (Linux) / `-fstrict-aliasing -O2`. What happens? Fix with `memcpy`.

---

## Interview questions

1. Union ke members memory mein kaise? `sizeof` / `alignof`?
2. "Active member" rule — inactive member padhna kya hai (C vs C++)?
3. Safe type punning kaise (union nahi to kya)?
4. Non-trivial member (`std::string`) wale union mein kya problem?
5. Tagged union kya hai? `std::variant` se relation?
6. Raw union kab use karo, kab `std::variant`?

---

## Next
→ [`09-std-variant.md`](09-std-variant.md)

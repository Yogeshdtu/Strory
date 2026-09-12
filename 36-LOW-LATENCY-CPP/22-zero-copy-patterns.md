# 22 — Zero-copy patterns: views, spans, in-place parsing

## Prerequisites
- **`09-ARRAYS/10-STRINGS`** (`std::span`, `std::string_view`, SSO),
  **`30-NETWORKING/12`** (`sendfile`/`splice`/`MSG_ZEROCOPY`),
  **`18-COPY-MOVE`** (move, lifetime)
- `04-allocation-avoidance.md`

## Yeh topic abhi kyun
Har copy = memory traffic + cache pollution + (for a growing buffer) an
allocation. A market-data message arrives once in a NIC buffer; if it's
copied 3 times before you read a price, that's ~3× the memory bandwidth and
3× the cache footprint for zero benefit. Zero-copy = **look at the bytes
where they already are.**

---

## The tools

| Tool | What it is | Use |
|---|---|---|
| `std::string_view` | `{const char*, size_t}` — a non-owning window into chars | parse a field without copying it out |
| `std::span<T>` | `{T*, size_t}` — a non-owning window into an array | pass a sub-array; iterate without copying |
| a plain `{ptr, len}` struct | the same, pre-C++17 / for wire buffers | RX buffer slices |
| `reinterpret_cast<const WireMsg*>(buf)` (+ care) | overlay a struct on received bytes | fixed-layout binary protocols — read fields in place |
| `std::bit_cast` | copy bytes into a typed value, no aliasing UB | read a scalar field from a buffer safely |
| `iovec` / `writev` / `readv` | scatter-gather I/O | send a header + payload without concatenating them |
| `sendfile` / `splice` | kernel moves bytes between fds, no userspace copy | file → socket (capture/replay), pipe plumbing |
| `MSG_ZEROCOPY` | kernel sends directly from your buffer (async completion) | large TX without a copy into kernel |
| kernel-bypass NIC rings | packet lands in a buffer you own; parse it there | the ultimate zero-copy RX (30/12) |

---

## In-place parsing

Fixed-layout binary protocol (ITCH-style): don't deserialize into owned
objects — overlay and read.
```cpp
#pragma pack(push, 1)
struct AddOrder {                        // matches the wire exactly
    uint16_t length;
    char     type;                       // 'A'
    uint64_t order_id;
    char     side;
    uint32_t qty;
    uint32_t symbol_id;
    int32_t  price;
};
#pragma pack(pop)

void on_bytes(const std::byte* buf, size_t len) {
    if (len < sizeof(AddOrder)) return;
    const auto* m = reinterpret_cast<const AddOrder*>(buf);   // overlay, no copy
    int32_t px = be32toh(m->price);                            // byte-swap if needed
    // ... use px, m->qty, m->symbol_id directly ...
}
```
Caveats:
- **Alignment**: `buf` may not be aligned for the struct's members. Either
  ensure the RX buffer is aligned and messages start on aligned offsets, or
  read each field with `memcpy`/`std::bit_cast` into a local (the compiler
  turns a 4-byte `memcpy` into a `mov`), or mark the struct `packed` (which
  makes member access do the unaligned load for you — a tiny cost on x86,
  a fault on strict targets).
- **Endianness**: wire is usually big-endian; swap on read (`be32toh` etc.,
  or `std::byteswap` C++23).
- **Lifetime**: `m` points into `buf` — valid only while `buf` is. Don't
  stash `m` past the buffer's reuse (same as the arena rule — lesson 08).
- **Strict aliasing**: `reinterpret_cast` of `std::byte*`/`char*` to a
  struct pointer to *read* is the accepted idiom (char types can alias
  anything); for a scalar prefer `std::bit_cast` (25/10, 33/09).

---

## Field access without copying out

```cpp
// a text protocol: "35=D\x0154=1\x0155=AAPL\x01..."
std::string_view field(std::string_view msg, int tag);   // returns a view into msg

std::string_view sym = field(msg, 55);                    // "AAPL" — no allocation
if (sym == "AAPL") { ... }                                // sv comparison, no copy
int64_t px = parse_fixed_point(field(msg, 44));           // std::from_chars, no string
```
`std::from_chars` / `std::to_chars` (`<charconv>`) parse/format numbers
**without** allocating, without a locale, without exceptions — the
zero-copy, zero-alloc way to convert between text and numbers.

---

## When you MUST copy — copy once, minimally

- If a value must **outlive** the source buffer (a `last_price`, a symbol
  you'll key a map by), copy it out — once, into a small owned field
  (a fixed `char[N]`, an `int64_t` — ideally parsed to a number so it's not
  a string at all).
- Copy the **minimum** — the 8 bytes you need, not the whole 200-byte
  message.
- Prefer **move** over copy for owned buffers passing between stages
  (`std::move` a `unique_ptr<Buffer>` / an index into a pool).

---

## ⚠️ Traps / Common mistakes

### Trap 1 — a view outliving its backing store
`std::string_view` / `std::span` / an overlay pointer into a RX or arena
buffer → dangling once the buffer is reused. Views die with the scope.

### Trap 2 — returning a `string_view` to a local
`std::string_view f() { std::string s = ...; return s; }` — dangles
immediately (`-Wdangling` may catch it). Return a view into a caller-owned
buffer, or return the `std::string`.

### Trap 3 — unaligned overlay reads
`reinterpret_cast<const uint64_t*>(buf + 3)` where `buf+3` isn't 8-aligned →
UB / slow / a fault on ARM. `memcpy`/`bit_cast` into a local, or a packed
struct, or align the buffer + message offsets.

### Trap 4 — forgetting endianness
Wire big-endian, host little-endian → prices/quantities come out garbage.
Swap on read; make it part of the accessor.

### Trap 5 — `std::stringstream` / `std::to_string` / `std::stod` for parsing
All allocate / use a locale / (some) throw. `std::from_chars` /
`std::to_chars` — no alloc, no locale, no throw, faster.

### Trap 6 — "zero-copy" that copies into `std::string` to compare
`if (std::string(sym_view) == "AAPL")` allocates. `if (sym_view == "AAPL")`
doesn't.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "parse = deserialize into objects" | overlay a struct / take views; read fields in place |
| "a `string_view` is a safe return value" | only if it views something the caller owns |
| "`reinterpret_cast` the buffer and go" | mind alignment, endianness, lifetime, aliasing |
| "`std::to_string` / `stod` to convert" | `std::to_chars` / `from_chars` — no alloc, no locale, no throw |
| "one more copy won't matter" | bandwidth + cache footprint + maybe an allocation, per message |

---

## Exercises

1. Ek FIX-ish text feed handler har message ke liye: split into fields
   (`std::vector<std::string>`), phir `std::stoi`/`std::stod` se parse, phir
   `std::map<std::string, double>` mein daale. Redesign zero-copy /
   zero-alloc.

   <details><summary>Answer</summary>

   Every line here allocates:
   - `std::vector<std::string> fields` — vector buffer + a `std::string` per
     field (heap for any field > SSO).
   - `std::stoi`/`std::stod` — locale, and `stod` can throw.
   - `std::map<std::string, double>` — a red-black-tree node + a `std::string`
     key copy per insert, and pointer-chasing lookups.
   Redesign:
   - **Don't split into strings.** Scan the message once; for each field,
     compute a `std::string_view` `{ptr, len}` into the received buffer.
   - **Parse numbers with `std::from_chars`** into `int64_t` fixed-point
     ticks (prices as integer ticks, not `double` — exact, fast, no locale,
     no throw). `from_chars` writes into a caller-provided variable, zero
     allocation.
   - **Key by `symbol_id`, not string.** Resolve the symbol view to a dense
     `int` id via a small perfect-hash / a `std::unordered_map<string,int>`
     built at **subscription time** (cold path). Then the hot-path store is
     `std::vector<Book>` (or `std::array`) indexed by id — O(1), contiguous,
     no allocation, no hashing on the hot path.
   - The buffer's bytes are read in place; nothing is copied out except the
     few `int64_t`s you actually keep in the book.
   Net: zero allocations, zero exceptions, no locale, no pointer-chasing
   container on the hot path.
   </details>

2. Ek binary protocol handler `reinterpret_cast<const Header*>(rx_buf +
   offset)` karta hai jahan messages back-to-back packed hain (variable
   length). Kabhi-kabhi field values garbage aate hain sirf kuch messages
   pe. Do likely causes.

   <details><summary>Answer</summary>

   (1) **Alignment.** Messages are packed back-to-back with variable
   lengths, so `offset` for the 2nd, 3rd, … message is whatever the previous
   lengths sum to — **not** necessarily a multiple of the `Header`'s
   alignment (say 8). `reinterpret_cast<const Header*>(rx_buf + 7)` and then
   reading a `uint64_t` member = an unaligned load: on x86 it's slow but
   *works*; if the compiler vectorized something or on ARM it faults or
   returns garbage; and if `Header` isn't `packed`, the compiler assumes
   alignment and may generate an aligned move that reads the wrong bytes.
   Fix: mark the wire struct `#pragma pack(1)` / `[[gnu::packed]]` (member
   access then does the correct unaligned load), or read each field via
   `memcpy`/`std::bit_cast` into a local, or ensure every message starts on
   an 8-byte boundary (pad on the wire, or copy each message to an aligned
   scratch — a small copy).
   (2) **Endianness / length trust.** If the length field used to advance
   `offset` is big-endian and read without swapping (or is attacker/error
   controlled and not bounds-checked), `offset` walks off into the wrong
   place → subsequent overlays read misaligned/wrong bytes. Fix: swap
   multi-byte fields on read, and **validate** `length` against the
   remaining buffer before advancing (`if (offset + len > rx_len) break;`).
   Also check `rx_buf` itself is aligned and that partial messages at the
   end of a read are carried over, not overlaid.
   </details>

---

## Interview questions

1. `string_view` / `span` — non-owning; the lifetime rule.
2. In-place parsing of a binary protocol — the 4 caveats (alignment, endianness, lifetime, aliasing).
3. `std::from_chars` / `to_chars` vs `stoi`/`to_string` — why they're the hot-path choice.
4. `iovec`/`writev`, `sendfile`/`splice`, `MSG_ZEROCOPY` — what each avoids copying.
5. When you *must* copy — copy once, copy minimal, prefer a number over a string.

---

## Next
→ [`23-compile-time-dispatch.md`](23-compile-time-dispatch.md)

# 13 — Virtual dispatch elimination: CRTP, variant, tables, tags

## Prerequisites
- **`16-OOP`** (vtable/vptr, virtual dispatch cost), **`21-TEMPLATES/07-08`**
  (CRTP, compile-time dispatch), **`33-COMPILER-OPTIMIZATION/07`** (devirtualization)
- `examples/07_dispatch_comparison.cpp`

## Yeh topic abhi kyun
`virtual` call = load the vptr, load the slot, **indirect call**, and the
callee is **not inlined**. On homogeneous data the indirect-call predictor
hides most of it; on **heterogeneous** data (types interleaved) it
mispredicts every type change → a jitter source. Yeh lesson: the 5
alternatives, measured, and when each is right.

---

## The 5 ways (measured — `07_dispatch_comparison.cpp`, is box, 4M calls)

| Mechanism | HOMOGENEOUS | HETEROGENEOUS | Inlined? | Set |
|---|---|---|---|---|
| `virtual` | 2.48 ns | **7.02 ns** | no | open (plugins) |
| `std::variant` + `visit` | 0.72 ns | 4.66 ns | yes (body) | closed |
| function-pointer table | 1.27 ns | 6.18 ns | no | open-ish |
| tag `enum` + `switch` | 0.77 ns | 4.53 ns | yes (body) | closed |
| **CRTP** (monomorphic) | **0.60 ns** | 0.59 ns | yes (all) | one type / closed |

- **Homogeneous**: the indirect-call predictor sees one target every call →
  `virtual`/fn-table work OK, but the body isn't inlined → CRTP/variant/switch
  still ~2–4× faster (inlined arithmetic + vectorization scope).
- **Heterogeneous**: `virtual`/fn-table's indirect call **mispredicts** on
  every type change → 2.5–5× slowdown. `variant`/`switch` also hit a
  jump-table, but the body is inlined → less damage. **CRTP is flat** (it's
  monomorphic — one type, fully resolved).

---

## Which to use

| You have | Use | Why |
|---|---|---|
| **one** concrete type, or a template that's instantiated per type | **CRTP** / plain templates | zero dispatch, full inline, vectorizes; but code per type (bloat), closed |
| a **closed** set of types known at compile time (Quote/Trade/Cancel; 3 fee kinds) | **`std::variant` + `visit`**, or a **tag `enum` + `switch`** | near-CRTP speed, one call site, add a type by adding a case; body inlined |
| an **open** set (user plugins, runtime-loaded strategies) | `virtual` (or a fn-table) | the only real option; keep the interface small, batch by type if possible |
| a hot dispatch that's **skewed** (90% one type) | peel + `[[likely]]` the common type, `switch` the rest (lesson 12 ex 2) | common path straight-line + inlined |
| you can **sort / bucket** the objects by type first | any of the above, applied per bucket | turns heterogeneous into homogeneous → predictor happy, and enables SIMD per bucket |

**HFT default: `std::variant` or a tag+switch** for message/event handling —
the set is closed (defined by the protocol), and you get inlined,
predictable dispatch. `virtual` only where the set is genuinely open.

---

## The "bucket by type" trick

If you have a heterogeneous collection but you process it in a loop, **group
by type** first:
```cpp
// instead of: for (auto& ev : events) ev->handle();   // mispredict per type change
// do: partition events into per-type vectors, then:
for (auto& q : quotes) handle_quote(q);                // homogeneous -> predicted + SIMD
for (auto& t : trades) handle_trade(t);
```
Costs a partition pass, but turns N indirect mispredicts into ~0 and lets
each per-type loop vectorize. Worth it when the loop is long and types
interleave.

---

## Devirtualization (when `virtual` stays)

The compiler *can* devirtualize (turn `virtual` into a direct/inlined call)
when it proves the dynamic type (folder 33/07):
- `final` on the class or method, and the static type is that class.
- The object's construction is visible at the call site.
- LTO (speculative devirt across TUs) / PGO (profile says it's ~always type X).

But **don't rely on it** for a heterogeneous container of base pointers —
the compiler can't prove anything there. Devirt is a bonus on the paths
where the type is statically knowable, not a strategy.

---

## ⚠️ Traps / Common mistakes

### Trap 1 — `virtual` for a closed set
Message types defined by your protocol = closed. Use `variant`/`switch`, not
`virtual` + a class hierarchy.

### Trap 2 — measuring dispatch on homogeneous data
Your benchmark uses all-one-type → `virtual` looks fine (~2.5 ns). Production
interleaves types → 7 ns + jitter. Benchmark with a representative mix.

### Trap 3 — CRTP for a heterogeneous collection
CRTP is monomorphic — you can't put `CFlat` and `CPct` in one
`std::vector<FeeBase*>` and dispatch. It's for "this whole pipeline is
templated on one strategy type", not for runtime-varying objects.

### Trap 4 — `std::variant` with many alternatives
`visit` on a 20-alternative variant → a 20-way jump table; fine, but the
`variant` is `sizeof(largest alternative)` + a tag → keep alternatives
similar in size, or store `variant<small types>` + pointers to big ones.

### Trap 5 — relying on devirtualization
`final` / LTO / PGO help on *statically knowable* paths. A `vector<Base*>`
loop won't devirtualize. Design the dispatch, don't hope.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`virtual` is fine, predictor hides it" | only on homogeneous data; heterogeneous → mispredict/type-change + jitter |
| "CRTP replaces `virtual`" | only for monomorphic code; can't hold mixed types in one container |
| "closed set → still use a class hierarchy" | `variant`/tag-switch: faster, inlined, one call site |
| "the compiler will devirtualize it" | only where the type is statically provable |
| "bucket-by-type is extra work" | turns N mispredicts + no-SIMD into ~0 + SIMD per bucket |

---

## Exercises

1. Ek market-data handler `std::vector<std::unique_ptr<Event>> batch` process
   karta, jahan `Event` ke 4 subtypes hain (Quote/Trade/Cancel/Status),
   interleaved as they arrive. `perf` dikhata `Event::handle` pe har call
   ~indirect-call mispredict. Do redesigns, trade-offs.

   <details><summary>Answer</summary>

   (1) **`std::variant<Quote, Trade, Cancel, Status>`** + `std::visit`. The
   set is closed (protocol-defined). `visit` compiles to a 4-way jump table
   on the tag, and each handler body is **inlined**. Measured shape (`07`):
   heterogeneous variant ~4.5 ns vs virtual ~7 ns, and the common cases
   inline + can be optimized. Store the batch as `std::vector<EventVar>` —
   no `unique_ptr`, no heap per event (pool/arena the vector's storage).
   Trade-off: `sizeof(EventVar)` = largest alternative + tag; adding a 5th
   type touches the variant declaration + every `visit` (compiler enforces
   exhaustiveness — a feature).
   (2) **Bucket by type**: as events arrive, append to per-type vectors
   (`quotes`, `trades`, ...). Then `for (auto& q : quotes) handle_quote(q);`
   etc. — each loop is homogeneous → the branch/target predictor is perfect,
   the handler inlines, and the loop can **vectorize** (process 4-8 quotes'
   price updates at once). Trade-off: a partition pass (cheap — one branch +
   a push per event), and you lose strict arrival ordering *within* the
   batch across types (usually fine for a batch; if ordering matters, keep
   a sequence number and merge).
   Both eliminate the indirect-call mispredict. Use (1) for
   ordering-sensitive per-event handling, (2) when you process in batches and
   want SIMD.
   </details>

2. Ek engineer sab `virtual` calls ko `std::function` se replace karta hai
   "kyunki woh flexible hai." Hot path pe yeh better ya worse, aur kyun?

   <details><summary>Answer</summary>

   **Worse.** `std::function` is *also* a type-erased indirect call (through
   its internal invoker pointer) — so you keep the indirect call + no-inline
   cost of `virtual`, and you **add**: (a) a possible **heap allocation** in
   the `std::function`'s constructor if the target/capture exceeds the SBO
   (~16 B) — measured in `08_std_function_cost.cpp`: a 64-byte capture →
   heap alloc in the ctor; (b) an extra indirection and a likely cache miss
   to reach the heap-allocated target; (c) larger objects (`sizeof(std::function)`
   ≈ 32 B vs 8 B for a `Base*`). Measured (`08`): `std::function` call ~3.1 ns
   vs a raw indirect call ~1.5 ns — **2× slower** than even a plain function
   pointer. `std::function` is a convenience for *storing* heterogeneous
   callables with owned lifetime; it is not a performance tool. On the hot
   path: CRTP / templates / `variant` / tag-switch (this lesson), or a plain
   function pointer / `function_ref` if you need type erasure without
   ownership (lesson 14).
   </details>

---

## Interview questions

1. The 5 dispatch mechanisms, measured cost on homo vs hetero data.
2. Why heterogeneous data hurts `virtual`/fn-table but less so `variant`/switch.
3. CRTP — what it's for, why it can't hold mixed types in one container.
4. "Bucket by type" — what it buys (predictor + SIMD), what it costs.
5. Devirtualization — when the compiler can, why not to rely on it.
6. Replacing `virtual` with `std::function` — why that's worse on the hot path.

---

## Next
→ [`14-std-function-cost.md`](14-std-function-cost.md)

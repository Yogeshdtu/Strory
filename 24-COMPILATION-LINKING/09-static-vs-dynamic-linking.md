# 09 — Static vs dynamic linking

## Prerequisites
- `08-object-files-elf.md`
- [`examples/03_static_library/`](examples/03_static_library/), [`examples/04_shared_library/`](examples/04_shared_library/)

## Yeh topic abhi kyun
Library ko binary mein **copy** karo (static, `.a`) ya run time pe **load** karo
(dynamic, `.so`/`.dll`)? Iska asar deployment, memory, security patching, aur —
HFT ke liye important — **call overhead + optimization + latency determinism** pe
padta hai. Low-latency shops overwhelmingly static link karte hain; kyun, yeh
file batati hai.

---

## Do modes — side by side

| | Static (`.a`) | Dynamic (`.so` / `.dll`) |
|---|---|---|
| Library code binary mein? | **Haan** — needed `.o` copy | Nahi — sirf reference |
| Resolve kab | link time (fixed) | load time / first call (PLT) |
| Runtime dependency | koi nahi | `.so` file present + resolvable |
| Deploy | ek file | app + libs + path resolution |
| Ek fix, N apps | har app rebuild | replace `.so`, apps as-is |
| Disk (many apps) | har app ki copy | ek shared copy |
| RAM (many procs) | private per proc | `.text` pages **shared** across procs |
| Startup cost | ~0 | loader: mmap libs, relocate, bind symbols |
| Cross-lib call | direct `call` | **PLT/GOT** indirect |
| Cross-lib inlining/devirt | with LTO, yes | no (separate compilation, interposition) |
| Binary size | bada | chhota |
| Security patch (e.g. libssl) | rebuild everything | update one `.so` |

---

## Static library (`.a`)

```bash
g++ -O2 -c add.cxx mul.cxx -o ...        # -> add.o, mul.o
ar rcs libcalc.a add.o mul.o             # archive: bundle + symbol index
g++ main.o -L. -lcalc -o app             # link: pull needed members INTO app
```

- `.a` = `.o` files ka `ar` archive + index. **Library nahi**, bundle.
- Linker **sirf zaroori members** pull karta — `nm app` mein `mul.o` ka code sirf
  tab jab kisi ne `mul` reference kiya. (`examples/03` ka `huge_unused` binary
  mein nahi aata.)
- **Link order matters** (GNU ld): libraries un `.o` ke **baad** jo unhe use karte
  (`g++ main.o -lcalc`, not `g++ -lcalc main.o`). Circular deps → `--start-group
  ... --end-group` ya library repeat.
- `-Wl,--gc-sections` + `-ffunction-sections` → member ke andar ke unused
  functions bhi drop.

---

## Shared library (`.so` / `.dll`)

```bash
# Linux
g++ -O2 -fPIC -c greet.cxx -o greet.o
g++ -shared greet.o -o libgreet.so -Wl,-soname,libgreet.so.1
g++ main.o -L. -lgreet -Wl,-rpath,'$ORIGIN' -o app
```

- **`-fPIC`** (Position-Independent Code) zaroori — `.so` kisi bhi address pe load
  ho sakta, toh code apne globals/functions ko `%rip`-relative + GOT ke through
  access karta (fixed addresses nahi baked).
- **soname** (`libgreet.so.1`) — ABI version. `libgreet.so.1.2.3` (real file),
  `libgreet.so.1` (soname symlink, "compatible" versions), `libgreet.so` (dev
  symlink). ABI break → bump soname.
- **Loader search** (Linux, roughly): `DT_RPATH` (deprecated) → `LD_LIBRARY_PATH`
  → `DT_RUNPATH` (`$ORIGIN` = "app ke paas") → `ldconfig` cache → `/lib`,
  `/usr/lib`. Windows: app dir → system → `PATH`.
- `ldd app` → resolved deps. `readelf -d app` → `NEEDED`, `RUNPATH`, `SONAME`.
- Missing → Linux `error while loading shared libraries`, Windows `... .dll was
  not found`.

---

## PLT / GOT — dynamic call ki keemat

App ko link time pe `greet::hello` ka **address nahi pata** (`.so` mein hai, load
addr abhi unknown). Mechanism:

- **GOT** (Global Offset Table) — pointers ka array `.so`/exe mein; loader (ya
  lazy: first call) real addresses bharta hai.
- **PLT** (Procedure Linkage Table) — har external function ke liye ek chhota stub
  jo GOT ke through jump karta.

```
call hello@plt
    hello@plt:  jmp  *hello@got(%rip)     ; first time -> resolver -> fills GOT
                                          ; after -> direct jump to real hello
```

Cost per cross-`.so` call:
- Ek **extra indirect jump** + ek **load** (GOT).
- GOT pointer branch-target-buffer / cache pe pressure.
- Lazy binding: **first** call ka resolver overhead (aur ek cold page fault).
- Compiler cross-`.so` calls ko **inline/devirtualize nahi** kar sakta (symbol
  interpose ho sakta hai — `LD_PRELOAD`).

Mitigations: `-fno-plt` (call via GOT directly, skip PLT stub — still indirect),
`-Wl,-z,now` (eager binding, no first-call cost, slower startup),
`-Bsymbolic` / `-fvisibility=hidden` (bind internal refs at link, no interpose),
protected/hidden visibility. **Ya bas static link.**

---

## Position-independent: PIC vs PIE

- **`-fPIC`** — for `.so` (mandatory) and, as `-fPIE` + `-pie`, for
  position-independent **executables** (default on most modern distros for ASLR /
  security).
- Cost: `%rip`-relative addressing everywhere, an extra register tied up as the
  GOT/PIC base on some ABIs, slightly larger code, GOT indirections for globals.
- **`-no-pie`** — non-position-independent executable: fixed load address, direct
  addressing, no GOT for its own symbols. Smaller/faster, no ASLR for the main
  binary. Some HFT builds use `-no-pie -static` for the last bit of determinism
  (weigh against the security trade-off).

---

## Andar kya hota hai (startup)

Dynamic executable launch:
1. Kernel exec → sees `PT_INTERP` → loads the dynamic loader (`ld.so`).
2. `ld.so` reads `NEEDED` → `mmap`s each `.so` (respecting RUNPATH etc.).
3. Applies **relative relocations** (`R_X86_64_RELATIVE`) for each loaded object
   (load_base + addend).
4. Resolves **symbol relocations** — eager (`-z now`) resolves all now; lazy
   (default) leaves function GOT slots pointing at the resolver.
5. Runs `.init_array` (global ctors), then `_start` → `main`.

Static executable: kernel maps it, runs `.init_array`, `main`. No loader, no
`.so` mmap, no symbol binding. Deterministic, faster start.

---

## > **HFT relevance**
> **Static linking is the low-latency default**, often `-static` (libc/libstdc++
> too), sometimes `-static -no-pie`. Reasons:
>
> - **No PLT/GOT indirection** — every call is a direct `call rel32`; no extra
>   indirect jump, no GOT load on the hot path, no interposition uncertainty.
> - **LTO across everything** (file 15) — the optimizer sees library code and can
>   inline/devirtualize into your hot loop. Impossible across a `.so` boundary.
> - **Deterministic startup + no first-call resolver fault** — a lazy-bound symbol
>   resolving on its first call mid-session is a latency spike; `-z now` fixes the
>   spike but not the indirection. Static removes both.
> - **No environment surprises** — no `LD_LIBRARY_PATH`, no "prod has libX.so.1.4,
>   staging 1.3", no accidental `LD_PRELOAD`.
> - **Immutable deploy artifact** — one binary, `scp`, done; trivially rollback-able.
>
> Costs accepted: bigger binaries, and a security fix means rebuild+redeploy (fine
> for a controlled fleet). Shared libraries still appear for **hot-swappable
> strategy plugins** (`dlopen` + `extern "C"` entry points — file 07) where the
> ability to reload a strategy without restarting the engine is worth the
> indirection on that (relatively cold) boundary.

---

## Hands-on

```bash
cd 24-COMPILATION-LINKING/examples/03_static_library && ./build.sh
cd ../04_shared_library && ./build.sh
```

- `03`: `nm app` — `calc::*` present, `huge_unused` absent (member selection).
- `04`: `objdump -p app.exe | grep 'DLL Name'` / `ldd app` — the dependency.
  Rebuild only the library, rerun app → new behaviour, app binary untouched.
- Compare `size` of a `-static` build vs a dynamic one of the same tiny program.
- `objdump -d -C app | grep -A3 '@plt'` on a dynamic build — see the PLT stubs.

---

## ⚠️ Traps

### Trap 1 — link order
```bash
g++ -lcalc main.o        # ⚠️ GNU ld: undefined reference (library before its user)
g++ main.o -lcalc        # ✅
```

### Trap 2 — `.so` not found at runtime despite linking fine
Link uses `-L`; **runtime** uses RUNPATH / `LD_LIBRARY_PATH` / `ldconfig`. Add
`-Wl,-rpath,'$ORIGIN'` or install the `.so` where the loader looks.

### Trap 3 — `-fPIC` missing when building a `.so`
```bash
g++ -shared a.o -o lib.so     # ⚠️ if a.o wasn't -fPIC: "recompile with -fPIC" (x86-64)
```

### Trap 4 — mixing a `.so` built against a different stdlib / `-D_GLIBCXX...`
ABI mismatch (file 04, 07) → crashes passing STL types across.

### Trap 5 — assuming a `.so` is "shared RAM" for your writable data
Only read-only pages (`.text`, `.rodata`) are shared. `.data`/`.bss` are
copy-on-write per process. Library `static` state is per-process.

### Trap 6 — expecting cross-`.so` calls to inline
They can't (separate compilation + interposition). If a hot helper lives in a
`.so`, its call cost includes PLT + no inlining. Move it into the binary or
static-link.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`.a` is a library you load" | `.a` is an archive of `.o`; needed members are **copied** into the binary at link |
| "dynamic linking is faster" | It's smaller and shareable; each cross-`.so` call pays PLT/GOT + no inlining |
| "static binary can't do ASLR" | The *main* binary's layout is fixed with `-no-pie`; `-static -pie` exists too |
| "link succeeded so the `.so` will be found at runtime" | Link path ≠ runtime path; set RUNPATH / install properly |
| "shared library = shared memory for everything" | Only R/O pages shared; writable data is per-process (COW) |
| "`-fPIC` is free" | Extra indirections + a tied-up register + bigger code; measurable in hot loops |

---

## Exercises

1. **Member selection:** `libmath.a` contains `sqrt.o`, `fft.o`, `stats.o`. Your
   program calls only `math::isqrt` (in `sqrt.o`). What ends up in the binary?

   <details><summary>Answer</summary>

   Just `sqrt.o` (and anything it references). `fft.o` and `stats.o` are not
   pulled from the archive — nothing references their symbols. `nm app` confirms
   only `sqrt.o`'s symbols are present.
   </details>

2. **PLT cost:** a hot loop calls `libm`'s `exp()` 100M times. Static vs dynamic
   `libm` — qualitatively, what's the difference per call?

   <details><summary>Answer</summary>

   Dynamic: `call exp@plt` → indirect `jmp *exp@got` → real `exp`; plus the
   compiler can't inline `exp` or const-fold around it, and the GOT slot competes
   for cache/BTB. Static (or `-flto` with a static libm): direct `call`, and the
   optimizer may even inline small parts / vectorize the loop. On a tight numeric
   loop this is a real, measurable win.
   </details>

3. **Deploy bug:** `ldd app` shows `libfeed.so => not found`. It linked fine on
   the build box. Two fixes.

   <details><summary>Answer</summary>

   (1) Install `libfeed.so` into a standard dir (`/usr/local/lib` + `ldconfig`) or
   set `LD_LIBRARY_PATH`. (2) Better: link the app with `-Wl,-rpath,'$ORIGIN'` (or
   `$ORIGIN/../lib`) and ship the `.so` alongside the binary. (3) Or static-link
   `libfeed` and stop worrying.
   </details>

4. **Why static for HFT:** give four concrete latency/perf reasons in one breath.

   <details><summary>Answer</summary>

   No PLT/GOT indirection on cross-library calls; LTO can inline/devirtualize
   library code into the hot path; deterministic startup with no lazy first-call
   resolver fault; no `LD_LIBRARY_PATH`/version-drift/`LD_PRELOAD` surprises —
   one immutable binary.
   </details>

5. **When dynamic wins:** name a case where an HFT system deliberately uses a
   shared library.

   <details><summary>Answer</summary>

   Hot-swappable strategy plugins: each strategy is a `.so` with `extern "C"`
   entry points, loaded via `dlopen`/`dlsym`. You can deploy/reload a strategy
   without restarting the engine. The plugin boundary is relatively cold
   (per-strategy setup, not per-tick), so the indirection cost is acceptable.
   </details>

---

## Interview questions

1. Static (`.a`) vs dynamic (`.so`) — 5 differences.
2. `.a` archive — member selection, link order rule.
3. PLT/GOT — what/why, cost per cross-`.so` call.
4. `-fPIC` — why mandatory for `.so`, what it costs.
5. Runtime library search order (Linux) — RPATH/RUNPATH/`LD_LIBRARY_PATH`/`$ORIGIN`.
6. soname / library versioning — how ABI compatibility is signalled.
7. Why do low-latency shops static-link? Costs accepted?
8. What is shared between processes for a `.so`, what isn't?

---

## Next
→ [`10-linker-errors.md`](10-linker-errors.md)

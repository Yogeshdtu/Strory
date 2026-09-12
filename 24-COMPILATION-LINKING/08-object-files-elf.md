# 08 — Object files & ELF: sections, symbols, relocations

## Prerequisites
- `07-name-mangling.md`
- [`examples/07_binary_inspection.sh`](examples/07_binary_inspection.sh)

## Yeh topic abhi kyun
`.o`, `.a`, `.so`, executable — sab **ELF** files hain (Linux). Ismein aapka code
sections mein bata hua hai, ek symbol table hai, aur relocations jo linker/loader
ko batati hain "yahan address bharna hai". Yeh structure samajhna = linker errors
debug karna, binary size analyze karna, aur "yeh symbol kahan se aaya" jaanna.

> **Platform note:** yeh box Windows hai → binaries **PE/COFF** hain, ELF nahi.
> Concepts wahi (sections, symbols, relocations); tool `readelf` ELF-only hai,
> Windows pe `objdump -h`/`-t`/`-p` use karo. HFT deployment Linux/ELF hai —
> isliye ELF hi padhte hain, PE equivalents note karke.

---

## ELF file types

| Ext | ELF type | Kya |
|---|---|---|
| `.o` | `ET_REL` (relocatable) | ek TU ka compiled output — abhi addresses tay nahi |
| `.a` | (ar archive) | `.o` files ka bundle + index (file 09) |
| `.so` | `ET_DYN` (shared object) | run time pe load hone wali library / PIE executable |
| (no ext) | `ET_EXEC` / `ET_DYN` | executable (modern: PIE = `ET_DYN`) |

```bash
readelf -h prog        # ELF header: class (64), machine (x86-64), type, entry point
file prog              # human summary
```

---

## Sections — code aur data ka bata-hua rup

| Section | Kya | Flags |
|---|---|---|
| `.text` | machine code | read + execute |
| `.rodata` | read-only data — string literals, `const` globals, jump tables, vtables | read |
| `.data` | initialized read-write globals (`int g = 5;`) | read + write; **file mein bytes** |
| `.bss` | zero-initialized globals (`int g;`, `static int a[1000];`) | read + write; **file mein 0 bytes** (loader zeroes) |
| `.symtab` / `.strtab` | symbol table + its string pool | — |
| `.rela.text` etc. | relocations for `.text` | — |
| `.eh_frame` / `.gcc_except_table` | exception unwinding tables (folder 23) | read |
| `.init_array` / `.fini_array` | global ctor/dtor function pointers (file 06) | — |
| `.debug_*` | DWARF debug info (`-g`) — huge, `strip`-able | — |
| `.comment` | compiler version string | — |
| `.note.*` | build id, ABI notes | — |

```bash
size prog              # text / data / bss totals
objdump -h prog        # every section: name, size, VMA, file offset, alignment
readelf -S prog        # same, ELF-specific detail
```

**`.bss` free lag sakta:** `static char buf[1<<20];` binary ka size 1 MB nahi
badhata — `.bss` file mein sirf "1 MB chahiye" likha hai, actual pages loader zero
karta hai. Isliye large zero-init arrays disk-cheap (par RAM cheap nahi at runtime).

---

## Symbols — `.symtab`

Har defined ya referenced naam ek symbol table entry:

```bash
nm -C prog.o
#  0000000000000000 T mathx::add(int, int)      <- T: defined in .text, external
#  0000000000000030 t helper()                  <- t: defined, local (internal linkage)
#                   U printf                    <- U: undefined (linker/loader supply kare)
#  0000000000000000 D g_config_version          <- D: defined in .data
#  0000000000000000 B g_buffer                  <- B: defined in .bss
#  0000000000000000 R kVersion                  <- R: defined in .rodata
#                   W operator new(unsigned long) <- W: weak (COMDAT / overridable)
```

| Code | Matlab |
|---|---|
| `T`/`t` | `.text` (code) — uppercase external, lowercase local |
| `D`/`d` | `.data` |
| `B`/`b` | `.bss` |
| `R`/`r` | `.rodata` |
| `U` | undefined — resolve at link (or load, for dynamic) |
| `W`/`w`/`V`/`v` | weak — merged/overridable (`inline`, templates) |
| `C` | common (tentative definition, mostly C) |
| `A` | absolute (fixed value) |

Dynamic symbol table (`.dynsym`) — shared libs ke liye alag, `nm -D` / `readelf
--dyn-syms`.

---

## Relocations — "address baad mein bharo"

Compiler `.o` banate waqt bahut addresses nahi jaanta:
- Doosri TU ke function ka address (`call add` — `add` kahan hoga?).
- Ek global variable ka final address.
- PIC code mein apne hi symbols ka address (`.so` kahan load hoga?).

Toh woh ek **relocation entry** chhodta hai: "is offset pe, is symbol ka address
(is formula se) patch karna".

```bash
objdump -dr prog.o         # disassembly WITH relocations inline
#   14: e8 00 00 00 00     call   19 <main+0x19>
#         15: R_X86_64_PLT32   mathx::add(int, int)-0x4    <- linker yahan real offset bharega
readelf -r prog.o          # relocation tables
```

- **Link-time relocations** — linker `.o` combine karte waqt resolve karta,
  final `.text` mein bytes patch. `ET_EXEC` mein koi runtime reloc nahi (fixed
  addresses).
- **Load-time / dynamic relocations** — PIE / `.so` ke liye. Loader (`ld.so`) load
  address decide karta phir GOT entries / `R_X86_64_RELATIVE` fixups karta. Yeh
  process startup cost hai (`-z now` = eager, `-z lazy` = on first call via PLT —
  file 09).

Common x86-64 reloc types: `R_X86_64_PC32` (pc-relative), `R_X86_64_PLT32` (via
PLT), `R_X86_64_GOTPCREL` (via GOT), `R_X86_64_64` (absolute 64-bit),
`R_X86_64_RELATIVE` (load-base + addend, for PIE).

---

## `.o` se executable — linker ka kaam

1. **Section merge** — sabhi `.o` ki `.text` ek `.text` mein, `.data` ek mein, etc.
2. **Symbol resolution** — har `U` ke liye ek `T`/`D` dhoondo (in `.o` files aur
   libraries). Nahi mila → `undefined reference` (file 10). Do mile → `multiple
   definition`.
3. **Relocation** — merge ke baad addresses tay → har relocation entry ka formula
   apply, bytes patch.
4. **Layout** — segments (`LOAD` program headers) banao: ek RX segment (`.text` +
   `.rodata`), ek RW (`.data` + `.bss`). Entry point set.
5. **Dynamic info** (agar shared libs) — `.dynamic`, `NEEDED` entries, PLT/GOT.

`readelf -l prog` — program headers (segments jo loader map karta).

---

## Andar kya hota hai (chhota)

- `-c` → assembler `.o` (`ET_REL`) banata: sections + symtab + relocs, no program
  headers, no fixed addresses.
- `-O2` inlined/DCE'd internal functions → symtab se gayab. Isliye release binary
  ka `nm` chhota.
- `-g` → `.debug_*` sections, aksar `.text` se bhi bade. `strip` / `objcopy
  --only-keep-debug` se alag.
- `-ffunction-sections -fdata-sections` → har function/global apni section →
  `-Wl,--gc-sections` unused ko drop kar sakta (chhota binary).
- `.bss` runtime pe demand-zero pages — first write pe page fault + zero.

---

## > **HFT relevance**
> - **Binary size analysis** — `size`, `bloaty` (`bloaty prog` → per-section /
>   per-symbol size). Hot `.text` chhota rakhna I-cache ke liye (folder 32). Bade
>   `.rodata` tables (lookup tables) sahi hain agar hot-accessed, warna cold.
> - **`.bss` for big pre-allocated pools** — startup pe `.bss` arena reserve karo
>   (disk-free), phir `mlock` / touch pages during warmup so first hot access
>   doesn't page-fault. Runtime allocation ke bina deterministic memory.
> - **`nm`/`objdump` in production debugging** (folder 45) — crash address →
>   `addr2line` / `objdump -d --start-address` se exact line; `nm -C` se mangled
>   frame names.
> - **`--gc-sections` + `-ffunction-sections`** — dead code strip, chhota image,
>   tighter I-cache footprint. LTO (file 15) aur aggressive.
> - **`readelf -d` / `ldd`** — deployment sanity: kaunse `.so` NEEDED, RUNPATH
>   kya. Static linking se yeh sab gayab (file 09).
> - **`.eh_frame` size** — `-fno-exceptions` (folder 23 file 09) se yeh section
>   shrink, `.text` denser.

---

## Hands-on

```bash
bash 24-COMPILATION-LINKING/examples/07_binary_inspection.sh

# ek chhota program:
cat > /tmp/e.cpp <<'EOF'
#include <cstdio>
int g_init = 42;                 // .data
int g_zero;                      // .bss
static char big[1<<20];          // .bss (1 MB, but ~0 file bytes)
const char* msg = "hello-elf";   // pointer in .data, string in .rodata
int add(int a, int b){ return a+b; }
int main(){ std::printf("%s %d %d %zu\n", msg, add(2,3), g_init+g_zero, sizeof big); }
EOF
g++ -O2 -g -c /tmp/e.cpp -o /tmp/e.o
objdump -h /tmp/e.o                 # sections
objdump -dr -C /tmp/e.o             # code + relocations
nm -C /tmp/e.o                      # symbols
g++ /tmp/e.o -o /tmp/e
size /tmp/e                         # note: big[] barely affects file size (.bss)
```

---

## ⚠️ Traps

### Trap 1 — `.bss` array binary size badhata samajhna
`static char buf[100<<20];` — file mein ~0 bytes (`.bss`). RAM at runtime: 100 MB
(on first touch). Disk-cheap, RAM-not.

### Trap 2 — `nm` bina `-C`
Mangled soup. `nm -C` / pipe `c++filt` (file 07).

### Trap 3 — `-g` binary "bloated" samajh ke ghabrana
`.debug_*` alag hai `.text` se. `strip` / ship a separate `.debug` file. Perf
same.

### Trap 4 — stripped binary ka `nm` khali
```bash
strip prog && nm prog     # "no symbols"
```
Debug builds / separate debug info rakho for production diagnosis.

### Trap 5 — reloc samajh ke bina "why is this call indirect"
`R_X86_64_PLT32` → external, via PLT (dynamic). `R_X86_64_PC32` → resolved
direct. Static link + LTO se PLT hatata hai (file 09, 15).

### Trap 6 — assuming a symbol you see in `.o` survives to the binary
`-O2` + internal linkage → inlined + removed from `.symtab`. `--gc-sections` →
unused external functions bhi drop.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`.o` mein final addresses hote" | `ET_REL` — no fixed addresses; relocations say "patch here" |
| "`.bss` array binary ko bada karta" | File mein ~0 bytes; RAM at runtime only |
| "`nm` output = readable" | `nm -C` for demangled; raw is `_Z...` |
| "`-g` slow karta runtime" | `.debug_*` sections — zero runtime cost; strippable |
| "linker bas files jodta" | Merge sections + resolve symbols + apply relocations + lay out segments |
| "har `.o` symbol binary mein aata" | Inlining / internal linkage / `--gc-sections` drop kar dete |

---

## Exercises

1. **Section for each:** `int a = 5;`, `int b;`, `const int c = 7;`, `static int
   d[1000];`, `"literal"`, `void f(){}`.

   <details><summary>Answer</summary>

   `a` → `.data`. `b` → `.bss`. `c` → `.rodata` (or optimized away if only used as
   a constant). `d` → `.bss` (zero-init). `"literal"` → `.rodata`. `f` → `.text`.
   </details>

2. **File size:** program A has `static int x[1000000];` (zero-init), program B has
   `static int y[1000000] = {1,1,...};` (all ones). Which binary is ~4 MB bigger?

   <details><summary>Answer</summary>

   B. `x` goes in `.bss` (no file bytes). `y` is explicitly initialized to
   non-zero → `.data` → the 4 MB of values are stored in the file. (An all-zero
   explicit init would still land in `.bss`.)
   </details>

3. **Reloc read:** `objdump -dr foo.o` shows `call ... <printf>` with
   `R_X86_64_PLT32 printf-0x4` next to it. What does the linker/loader do?

   <details><summary>Answer</summary>

   At link, `printf` is external (from libc) → the linker sets up a PLT stub and
   points this `call` at it (PLT32, pc-relative). At load/first-call, the dynamic
   loader resolves `printf`'s real address into the GOT; the PLT stub jumps
   through the GOT. (Static libc → resolved to a direct call, no PLT.)
   </details>

4. **Strip effect:** you `strip` a 12 MB binary and it becomes 3 MB. What was the
   9 MB, and what did you lose?

   <details><summary>Answer</summary>

   Mostly `.debug_*` (DWARF) plus `.symtab`/`.strtab`. You lose the ability to get
   function names and line numbers in backtraces/`addr2line` for that binary —
   keep a separate unstripped copy or a `.debug` file (`objcopy
   --only-keep-debug`).
   </details>

5. **`--gc-sections`:** you compile with `-ffunction-sections -fdata-sections
   -Wl,--gc-sections` and the binary shrinks 20%. What happened, and one risk.

   <details><summary>Answer</summary>

   Each function/global went into its own section; the linker discarded any
   section with no references from a kept section (dead code/data elimination at
   link). Risk: something reached only via a symbol name at runtime (e.g.
   `dlsym`, a plugin registration relying on a global constructor) can be
   collected — mark those `__attribute__((used))` / `KEEP()` in the linker
   script.
   </details>

---

## Interview questions

1. `.o` / `.a` / `.so` / executable — ELF types aur farq.
2. `.text` / `.rodata` / `.data` / `.bss` — kaunsa kya, file bytes kaun leta?
3. `.bss` array binary size kyun nahi badhata?
4. `nm` symbol codes: `T`, `t`, `U`, `W`, `D`, `B` — matlab.
5. Relocation kya hai — link-time vs load-time.
6. Linker ke 5 steps (merge → resolve → relocate → layout → dynamic).
7. `-g` binary bada — runtime pe asar? `strip` kya karta?
8. `--gc-sections` — kya, ek risk.

---

## Next
→ [`09-static-vs-dynamic-linking.md`](09-static-vs-dynamic-linking.md)

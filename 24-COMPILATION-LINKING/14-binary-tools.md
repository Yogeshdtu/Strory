# 14 — Binary tools: `nm`, `objdump`, `readelf`, `ldd`, `strings`, `size`, `strip`

## Prerequisites
- `07-name-mangling.md`, `08-object-files-elf.md`, `09-static-vs-dynamic-linking.md`
- [`examples/07_binary_inspection.sh`](examples/07_binary_inspection.sh)

## Yeh topic abhi kyun
Jab kuch link/load/runtime pe galat hota hai — "undefined reference to a function
that exists", "wrong library loaded", "binary is 200 MB", "which version is
deployed" — aap binary ko **kholke dekhte** ho. Yeh tools binutils ka core hain;
har systems C++ engineer ko inhe reflex se use aana chahiye.

> Windows/MinGW pe: `nm`, `objdump`, `size`, `strings`, `strip`, `c++filt` sab
> aate hain (binutils). `readelf`/`ldd` ELF/Linux ke liye — Windows binary PE hai,
> `objdump -h`/`-p` use karo. HFT deploy Linux hai.

---

## The toolbox — one-liner each

| Tool | Ek line mein |
|---|---|
| `file X` | X kis kism ka file hai (ELF/PE, 32/64, arch, stripped?) |
| `size X` | `.text` / `.data` / `.bss` byte totals |
| `nm -C X` | symbol table (kaun defined `T`, undefined `U`, local `t`, weak `W`) |
| `nm -CD X` | **dynamic** symbol table (`.so` exports/imports) |
| `c++filt <sym>` | ek mangled naam → readable |
| `objdump -d -C X` | disassembly (demangled) |
| `objdump -h X` | section headers (name, size, address) |
| `objdump -t X` | symbol table (nm ka cousin) |
| `objdump -r X` | relocations |
| `objdump -p X` | dynamic section / DLL deps / RUNPATH |
| `readelf -a X` | **everything** (ELF header, sections, symbols, dynamic, notes) |
| `readelf -d X` | dynamic section: `NEEDED`, `RUNPATH`, `SONAME` |
| `ldd X` | resolved shared-library dependencies (runtime) |
| `strings X` | printable text embedded in the binary |
| `strip X` | remove symbol/debug info (smaller binary) |
| `addr2line -e X 0x...` | address → `file:line` (needs `-g` / debug info) |
| `nm -S --size-sort X` | symbols sorted by size (what's fat?) |
| `bloaty X` | per-section / per-symbol size breakdown (best size analyzer) |
| `objcopy` | copy/transform (strip, keep-debug, add sections) |

---

## Task-oriented recipes

### "undefined reference to `foo(int)` but foo exists"
```bash
nm -C somelib.a | grep foo            # what signature is actually defined?
nm -C -D somelib.so | grep foo        # for a shared lib
```
Compare the exact demangled signature to your call site (file 07, 10). Usually a
`const`, `&`, or param-type mismatch → different mangled name.

### "which library is actually loaded / missing"
```bash
ldd ./app                             # Linux: resolved deps + paths
objdump -p app.exe | grep 'DLL Name'  # Windows
readelf -d ./app | grep -E 'NEEDED|RUNPATH'
LD_DEBUG=libs ./app                   # verbose loader trace (Linux)
```

### "why is this binary 180 MB"
```bash
size ./app                            # sections
bloaty ./app                          # per-symbol/section — the fat is usually .text bloat or debug
nm -S --size-sort -C ./app | tail -30 # biggest symbols
objdump -h ./app | grep debug         # .debug_* often the bulk if -g and not stripped
strip -s ./app                        # or objcopy --only-keep-debug ./app app.debug
```

### "what version / build is deployed"
```bash
strings ./app | grep -iE 'version|build|git|[0-9]\.[0-9]\.[0-9]'
readelf -n ./app                      # .note.gnu.build-id
```
(Better: bake a `const char kBuildInfo[] = "v1.4.2 " GIT_SHA;` and `strings | grep`.)

### "crash at address 0x4011a7"
```bash
addr2line -f -C -e ./app 0x4011a7     # function + file:line  (needs debug info)
objdump -d -C ./app --start-address=0x401190 --stop-address=0x4011c0
```

### "is this symbol exported from my .so"
```bash
nm -CD --defined-only libmine.so | grep MyClass
readelf --dyn-syms libmine.so | grep MyClass
```

### "did --gc-sections drop my plugin registration"
```bash
nm -C ./app | grep register_strategy   # missing? mark it __attribute__((used)) / KEEP()
```

---

## `nm` symbol codes (recap from file 08)

```
T  text, defined, external        t  text, defined, local
D  data, defined, external        d  data, local
B  bss (zero-init), external      b  bss, local
R  rodata, external               r  rodata, local
U  UNDEFINED (linker/loader must provide)
W  weak (inline/template/overridable)   V  weak object
C  common (tentative def)         A  absolute
```

`nm -C` demangle, `nm -D` dynamic table, `nm -u` only undefined, `nm --defined-only`,
`nm -S` show sizes, `nm --size-sort`.

---

## `objdump` cheatsheet

```bash
objdump -d -C -M intel bin              # disassemble, demangle, Intel syntax
objdump -d -C bin --disassemble='Book::push'   # just one function (GNU binutils 2.36+)
objdump -S -C bin                       # interleave source (needs -g)
objdump -h bin                          # sections
objdump -t bin                          # symbols
objdump -T bin                          # dynamic symbols
objdump -r bin.o                        # relocations
objdump -p bin                          # dynamic/PE headers (deps, RUNPATH)
objdump -x bin                          # all headers
```

Is repo mein: `./build.ps1 asm FILE=...` = `objdump`-style demangled assembly.

---

## `readelf` (ELF only)

```bash
readelf -h bin      # ELF header: class, machine, type, entry
readelf -S bin      # section headers
readelf -l bin      # program headers (LOAD segments — what the loader maps)
readelf -s bin      # symbol table
readelf -d bin      # dynamic section (NEEDED, RUNPATH, SONAME, FLAGS)
readelf -r bin      # relocations
readelf -n bin      # notes (build-id, ABI)
readelf --debug-dump=info bin   # DWARF
```

---

## `strip` / `objcopy` — deploy hygiene

```bash
# strip in place (production deploy):
strip -s ./app

# keep debug info separately (recommended):
objcopy --only-keep-debug ./app app.debug
strip -s ./app
objcopy --add-gnu-debuglink=app.debug ./app
# now: gdb ./app  finds  app.debug  for symbols
```

Stripped binary is smaller and reveals less; the separate `.debug` gives you
symbolized backtraces when you need them.

---

## > **HFT relevance**
> - **Production debugging** (folder 45): a stripped prod binary crashes → you have
>   the address + the matching `.debug` file → `addr2line` / `gdb` → exact line.
>   Keep every deployed binary's build-id and debug file archived.
> - **Binary size / I-cache** (folder 32): `bloaty` + `nm --size-sort` to find
>   bloated `.text` (template over-instantiation, `-O3` code growth). `-Os` for
>   cold code, `--gc-sections`, `extern template` (folder 21).
> - **Deploy sanity**: `ldd` / `readelf -d` to confirm a static binary has **no**
>   surprise `.so` deps; `readelf -n` build-id matches what you built.
> - **`nm -C` reflex** for every "the function is right there" linker/loader
>   mystery (file 07, 10).
> - **`objdump -d` / `./build.ps1 asm`** in the optimize loop — did the compiler
>   vectorize the hot loop? inline the accessor? emit the branchless cmov? (folder
>   33, 34.)
> - **`strings` audit** — no secrets, no debug paths, expected version string in
>   the shipped artifact.

---

## Hands-on

```bash
bash 24-COMPILATION-LINKING/examples/07_binary_inspection.sh   # the guided tour

# build something with symbols and poke it:
g++ -O2 -g 24-COMPILATION-LINKING/examples/09_linkage_storage.cpp -o /tmp/ls -pthread
file /tmp/ls
size /tmp/ls
nm -C /tmp/ls | grep -E ' [TtWD] ' | head
nm -S --size-sort -C /tmp/ls | tail -10
objdump -d -C /tmp/ls | grep -A15 'next_seq'
strip -s /tmp/ls && nm /tmp/ls    # "no symbols"
```

---

## ⚠️ Traps

### Trap 1 — `nm` without `-C`
Mangled `_Z...` soup. Always `nm -C` (or `| c++filt`).

### Trap 2 — `nm` on a `.so` showing nothing useful
Use `nm -D` (dynamic symbols) for shared objects; the regular `.symtab` may be
stripped.

### Trap 3 — `readelf` on a Windows PE / macOS Mach-O
ELF-only. Use `objdump` (cross-format) or `dumpbin` (MSVC) / `otool` (macOS).

### Trap 4 — `addr2line` on a stripped binary
No output / `??:0`. Needs `-g` info or the separate `.debug` file
(`--add-gnu-debuglink`).

### Trap 5 — `ldd` on an untrusted binary
`ldd` may *run* parts of the loader for that binary. For untrusted files use
`objdump -p` / `readelf -d` instead.

### Trap 6 — trusting `size` after `strip` on PE
PE `size` reports section sizes that don't shrink from symbol stripping; check
`nm` ("no symbols") to confirm the strip worked.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "`nm` output is unreadable" | `nm -C` demangles; `-D` for shared libs |
| "`strip` speeds up the program" | Zero runtime effect — just smaller file, fewer symbols |
| "`ldd` just prints deps" | It can invoke the loader; use `objdump -p`/`readelf -d` for untrusted |
| "`readelf` works on any binary" | ELF only; `objdump` is cross-format |
| "you can't debug a stripped prod binary" | You can, with the archived matching `.debug` + build-id |
| "big binary = bloated code" | Often it's `.debug_*` (unstripped `-g`) — check with `size`/`bloaty` |

---

## Exercises

1. **Which tool:** (a) "does libfoo.so export `Engine::run()`?" (b) "what's the
   biggest function in my binary?" (c) "crash at `0x40128f`, which line?" (d) "does
   this static binary secretly depend on a `.so`?"

   <details><summary>Answer</summary>

   (a) `nm -CD libfoo.so | grep 'Engine::run'` (or `readelf --dyn-syms`).
   (b) `nm -S --size-sort -C bin | tail` (or `bloaty bin`). (c) `addr2line -f -C
   -e bin 0x40128f`. (d) `ldd bin` / `readelf -d bin` — should show no `NEEDED`
   beyond maybe the loader; ideally nothing.
   </details>

2. **Undefined reference:** `nm -C main.o` shows `U Logger::write(char const*)`;
   `nm -C liblog.a` shows `T Logger::write(std::string_view)`. Diagnosis + fix.

   <details><summary>Answer</summary>

   Signature mismatch — the call site (or header) resolves to `write(const
   char*)`, but the library only defines the `string_view` overload → different
   mangled names → undefined. Fix the header/call to use `std::string_view`
   (or add the missing overload to the library).
   </details>

3. **Size hunt:** `size app` → `text 210000000`. `bloaty app` → 60% in one
   namespace of template instantiations. Two mitigations.

   <details><summary>Answer</summary>

   (1) `extern template` for the common instantiations + one explicit
   instantiation in a `.cpp` (folder 21) — stops every TU re-emitting them.
   (2) Factor type-independent code out of the template into a non-template base;
   consider type erasure at cold boundaries; `-Os` for cold code;
   `-ffunction-sections -Wl,--gc-sections` + ICF (`-Wl,--icf=all` with lld/gold).
   </details>

4. **Deploy check:** you build with `-O2 -g`, then need to ship. Commands to
   produce a small binary but keep debuggability.

   <details><summary>Answer</summary>

   ```bash
   objcopy --only-keep-debug app app.debug
   strip -s app
   objcopy --add-gnu-debuglink=app.debug app
   # ship `app`; archive `app.debug` + its build-id (readelf -n app)
   ```
   </details>

5. **Assembly check:** you optimized a hot loop and want to confirm it vectorized.
   One command.

   <details><summary>Answer</summary>

   `objdump -d -C -M intel ./app | grep -A40 '<hot_function>'` and look for SIMD
   registers/ops (`xmm`/`ymm`/`zmm`, `vaddpd`, `vmovups`, `vpaddd`) in the loop
   body. (Or `./build.ps1 asm FILE=...`, or compile with `-fopt-info-vec`.)
   </details>

---

## Interview questions

1. `nm` symbol codes — `T`/`t`/`U`/`W`/`D`/`B`.
2. `nm` vs `nm -D` — kab kaunsa.
3. `objdump` vs `readelf` — kab kaunsa (format support).
4. `ldd` kya karta, aur untrusted binary pe kyun careful?
5. `strip` — runtime pe asar? Debuggability kaise bachao?
6. Address → source line — kaunsa tool, kya chahiye?
7. Binary size analyze karne ke liye kaunse tools/steps?
8. "Function exists but undefined reference" — kaunse tools se debug?

---

## Next
→ [`15-lto-and-pgo.md`](15-lto-and-pgo.md)

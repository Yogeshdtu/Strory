# 11 — Make: rules, variables, patterns, dependency tracking

## Prerequisites
- `01-translation-units.md` (separate compilation)
- [`examples/05_makefile_project/`](examples/05_makefile_project/)

## Yeh topic abhi kyun
50-file project mein har baar poora rebuild = minutes barbaad. Make (aur CMake,
file 12) ka ek hi core kaam hai: **sirf woh cheezein rebuild karo jo stale hain**,
aur wahi. Make ka model chhota hai — target, prerequisites, recipe, timestamps —
par usse theek se samajhna zaroori hai, khaas kar **auto dependency tracking**
(bina iske Make C++ ke liye adhoora hai).

---

## Core model

```make
target: prerequisite1 prerequisite2
	recipe-command          # <- TAB, spaces nahi
```

Make ka algorithm:
1. `target` ke saare `prerequisites` pehle build karo (recursively).
2. Agar `target` **exist nahi karta**, ya koi prerequisite `target` se **newer**
   hai (mtime) → `recipe` chalao.
3. Warna kuch mat karo ("up to date").

Yeh **timestamp-based incremental build** hai. `make` (no args) → **first target**
(default goal) build karta.

⚠️ Recipe lines **TAB** se shuru hoti hain (spaces → `missing separator` error).

---

## Ek chhota C++ Makefile

```make
CXX      := g++
CXXFLAGS := -std=c++20 -O2 -g -Wall -Wextra -MMD -MP -Iinclude
BINDIR   := build
SRCS     := $(wildcard src/*.cxx)
OBJS     := $(patsubst src/%.cxx,$(BINDIR)/%.o,$(SRCS))
DEPS     := $(OBJS:.o=.d)
TARGET   := $(BINDIR)/app

.PHONY: all clean run
all: $(TARGET)

$(TARGET): $(OBJS) | $(BINDIR)
	$(CXX) $(OBJS) -o $@

$(BINDIR)/%.o: src/%.cxx | $(BINDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BINDIR):
	@mkdir -p $(BINDIR)

-include $(DEPS)

run: $(TARGET) ; @./$(TARGET)
clean: ; $(RM) -r $(BINDIR)
```

(`examples/05_makefile_project/Makefile` isi ka thoda bada version, walkthrough
uske README mein.)

---

## Variables

```make
CXX  := g++            # immediate (:=)  — RHS abhi expand
FLAGS = -O2 $(EXTRA)   # deferred (=)     — har use pe expand ($(EXTRA) baad mein set ho sakta)
FLAGS += -Wall         # append
OPT  ?= -O2            # set ONLY if not already set (env / command line override)
```

- **Command line override:** `make CXXFLAGS="-O0 -g"` — recipe-set variables ko
  override kar deta hai (`?=` / `override` se bacho).
- **Automatic variables** (recipe ke andar):
  | Var | Kya |
  |---|---|
  | `$@` | target |
  | `$<` | pehla prerequisite |
  | `$^` | saare prerequisites (dedup) |
  | `$+` | saare prerequisites (with dups) |
  | `$*` | pattern stem (`%` ne kya match kiya) |
- **Functions:** `$(wildcard src/*.cxx)`, `$(patsubst %.cxx,%.o,$(x))`,
  `$(subst a,b,$(x))`, `$(filter %.o,$(x))`, `$(dir ...)`, `$(notdir ...)`,
  `$(shell git rev-parse HEAD)`, `$(foreach ...)`, `$(if ...)`.

---

## Pattern rules

```make
$(BINDIR)/%.o: src/%.cxx
	$(CXX) $(CXXFLAGS) -c $< -o $@
```

`%` — wildcard stem. Ek rule, `src/hash.cxx → build/hash.o`, `src/util.cxx →
build/util.o`, sabke liye. Make built-in pattern rules bhi rakhta hai (`%.o: %.cpp`)
— aksar aap apne likhte ho for control.

---

## Auto dependency tracking — **the crucial part**

Bina iske, ye Makefile **galat** hai:

```make
build/hash.o: src/hash.cxx      # <- headers list nahi!
```

`hash.cxx` `#include "engine.hpp"` karta hai. Aap `engine.hpp` badalte ho → Make ko
kuch pata nahi (`hash.cxx` ki mtime nahi badli) → `hash.o` rebuild **nahi** hota →
**stale binary**, silent bug.

**Fix: compiler se dependencies nikalo.** `-MMD -MP`:

```make
CXXFLAGS += -MMD -MP
...
-include $(DEPS)     # DEPS = $(OBJS:.o=.d)
```

- `-MMD` → compile karte waqt compiler ek `build/hash.d` bhi likhta hai:
  ```make
  build/hash.o: src/hash.cxx include/engine.hpp include/config.hpp
  ```
- `-MP` → har header ke liye ek empty phony target add karta (`include/engine.hpp:`)
  taaki header delete karne pe Make error na de.
- `-include $(DEPS)` → yeh `.d` files ko Makefile mein pull karta (pehli baar
  nahi hoti → `-` = "missing OK").

Ab `engine.hpp` badla → `hash.d` ne bataya `hash.o` uspe depend karta → `hash.o`
rebuild. **Yehi C++ Makefile ki asli test hai.**

---

## `.PHONY` targets

```make
.PHONY: all clean run test install
clean:
	$(RM) -r $(BINDIR)
```

`clean` koi file nahi banata — agar `clean` naam ki file exist kare, Make usse
"up to date" samajh ke recipe skip kar deta. `.PHONY` batata hai "yeh target
hamesha run karo".

---

## Order-only prerequisites

```make
$(BINDIR)/%.o: src/%.cxx | $(BINDIR)
```

`| $(BINDIR)` — "`build/` exist karna chahiye, par uska **timestamp mat dekho**".
Bina `|`, har baar jab koi file `build/` mein likhi jaati hai, `build/` ka mtime
badhta → saare `.o` stale samjhe jate → sab rebuild. Order-only fixes that.

---

## Parallel builds

```bash
make -j8            # 8 recipes at once (independent targets)
make -j             # unlimited
make -j8 -l4        # -j8 but back off if load avg > 4
```

Make prerequisite graph se independent targets nikalkar parallel chalata hai.
Correct `.PHONY` + real dependencies zaroori (warna race conditions). `make -Otarget`
(`-O` = output sync) taaki parallel logs interleave na hon.

---

## Common recipes

```make
.PHONY: all clean run test asm

run: $(TARGET)
	@./$(TARGET) $(ARGS)

test: $(TARGET)
	@./$(TARGET) --test || (echo "FAIL" && exit 1)

# generated header
version.hpp:
	@echo '#define BUILD_SHA "$(shell git rev-parse --short HEAD)"' > $@

# multiple exes
BINS := server client bench
all: $(BINS)
server: server.o net.o ; $(CXX) $^ -o $@
```

---

## Andar kya hota hai

- Make ek DAG banata hai (targets → prerequisites), phir post-order traverse: har
  node pe "koi prereq mujhse newer?" → recipe.
- `mtime` granularity: modern filesystems ns; kuch (FAT, network) 1–2s → spurious
  rebuilds ya missed rebuilds. `make` clock skew ke liye sensitive (build server
  vs NFS).
- `-MMD` compiler ka feature hai (Make ka nahi) — GCC/Clang preprocessing ke
  dauraan include graph already jaante hain, usse `.d` mein dump kar dete.
- Recipe har line ek naya shell (`/bin/sh`) — `cd x` agli line pe effect nahi;
  `cd x && cmd` ek line mein, ya `.ONESHELL:`.

---

## > **HFT relevance**
> - **Iteration speed** — auto-deps + `-j` = "edit one file, `make`, run" in
>   seconds. Ek missing `-MMD` = din mein kai baar stale binary chase karna.
> - **Reproducible flag sets** — Makefile mein exact `-O2 -march=native -flto
>   -fno-exceptions ...` ek jagah; koi "maine locally `-O3` try kiya tha" drift
>   nahi (file 04 ka ODR risk).
> - **`make bench` / `make asm FILE=...` / `make san`** targets — is repo ka
>   `Makefile` / `build.ps1` bilkul yeh pattern hai (folder root dekho). Fast
>   feedback loops for measure→profile→optimize (folder 35).
> - **Big shops:** raw Make se aage (Bazel, Buck, custom) — hermetic, cached,
>   distributed builds — par model wahi: declare deps, rebuild only stale.
>   CMake (file 12) → Ninja usually the practical choice.

---

## Hands-on

```bash
cd 24-COMPILATION-LINKING/examples/05_makefile_project
make            # full build
make            # "Nothing to be done" (incremental)
touch src/util.cxx && make          # only util.o + link
touch include/engine.hpp && make    # ALL .o rebuild (auto-deps!)
cat build/hash.d                    # see what -MMD wrote
make -j4 rebuild                    # parallel clean build
make CXXFLAGS="-std=c++20 -O0 -g"   # override from CLI
```

Experiment: remove `-MMD -MP` and `-include $(DEPS)`, then `touch
include/engine.hpp && make` → nothing rebuilds → run the (stale) binary → observe
it didn't pick up your header change.

---

## ⚠️ Traps

### Trap 1 — spaces instead of TAB
`Makefile:5: *** missing separator. Stop.` — recipe lines need a literal TAB.

### Trap 2 — no auto-dependency tracking
Header change → no rebuild → stale binary. `-MMD -MP` + `-include $(DEPS)`.

### Trap 3 — `.PHONY` bhoolna
A file named `test` or `clean` in the tree → Make thinks the target is up to date.

### Trap 4 — `:=` vs `=` confusion
`FLAGS = $(shell slow-cmd)` with `=` → runs `slow-cmd` **every time** `$(FLAGS)` is
expanded. Use `:=` for anything expensive or order-sensitive.

### Trap 5 — recipe multi-line assuming shared shell
```make
setup:
	cd build       # new shell
	cmake ..        # runs in the ORIGINAL dir
```
`cd build && cmake ..` on one line, or `.ONESHELL:`.

### Trap 6 — `build/` mtime causing full rebuilds
Directory as a normal prerequisite → every write bumps its mtime → everything
stale. Use `| $(BINDIR)` (order-only).

### Trap 7 — parallel build races
`make -j` with a target that two rules write, or missing deps → intermittent
failures. Model the real dependencies; make output dirs order-only.

---

## Common galat samajh

| ❌ Galat | ✅ Sahi |
|---|---|
| "Make content ko dekhta hai" | Timestamps (mtime) — content nahi (unless you add hashing) |
| "recipe indent spaces se bhi chalega" | TAB required |
| "Make headers ki dependency khud samajhta" | Nahi — `-MMD -MP` + `-include *.d` se batao |
| "`.PHONY` optional detail hai" | Bina it, a same-named file breaks the target |
| "`=` aur `:=` same" | `=` deferred (re-expands), `:=` immediate |
| "`-j` bas fast karta, koi risk nahi" | Missing deps / shared outputs → races |

---

## Exercises

1. **Predict:** given the small Makefile above, you run `make`, then immediately
   `touch src/hash.cxx`, then `make`. What runs?

   <details><summary>Answer</summary>

   `build/hash.o` recompiles (its prereq `src/hash.cxx` is now newer), then the
   link rule reruns (`hash.o` is newer than `build/app`). `util.o`, `engine.o`,
   `main.o` are untouched.
   </details>

2. **Auto-dep test:** with `-MMD -MP` removed, you edit a `constexpr int` in
   `include/engine.hpp` and run `make`. What happens, and why is it dangerous?

   <details><summary>Answer</summary>

   Nothing rebuilds — no `.o` lists `engine.hpp` as a prerequisite, and the
   `.cxx` files' mtimes didn't change. The binary keeps the old constant. Silent
   stale build — exactly the class of bug auto-deps prevent.
   </details>

3. **Automatic vars:** in `foo.o: foo.cxx bar.hpp`, what are `$@`, `$<`, `$^`?

   <details><summary>Answer</summary>

   `$@` = `foo.o`. `$<` = `foo.cxx` (first prereq). `$^` = `foo.cxx bar.hpp` (all
   prereqs, de-duplicated).
   </details>

4. **Order-only:** why `$(BINDIR)/%.o: src/%.cxx | $(BINDIR)` and not
   `$(BINDIR)/%.o: src/%.cxx $(BINDIR)`?

   <details><summary>Answer</summary>

   Without `|`, `$(BINDIR)` is a normal prerequisite: every time a file is written
   into `build/`, the directory's mtime updates, making every `.o` appear
   out-of-date → full rebuild each run. `|` makes it order-only: "must exist,
   don't compare timestamps."
   </details>

5. **Phony fail:** you add a `test` target but `make test` says "'test' is up to
   date" and doesn't run. There's a `test/` directory. Fix?

   <details><summary>Answer</summary>

   Add `.PHONY: test` (and any other non-file targets). Make was treating the
   `test/` directory as the target and finding it "newer than its (no)
   prerequisites."
   </details>

---

## Interview questions

1. Make ka core algorithm — target/prereq/recipe/timestamp.
2. Auto dependency tracking kyun zaroori C++ mein — `-MMD -MP` kya karte?
3. `:=` vs `=` vs `?=` — farq.
4. `$@`, `$<`, `$^` — automatic variables.
5. Pattern rule (`%`) — ek example.
6. `.PHONY` — kya, kyun.
7. Order-only prerequisite (`|`) — kis problem ko solve karta?
8. `make -j` ke saath kya galat ho sakta agar deps incomplete hon?

---

## Next
→ [`12-cmake.md`](12-cmake.md)

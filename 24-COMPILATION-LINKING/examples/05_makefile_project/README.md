# 05 — A proper Makefile

**Lesson:** 11 (make)

## Layout
```
include/engine.hpp
src/hash.cxx  src/util.cxx  src/engine.cxx  src/main.cxx
Makefile
build/            <- generated: *.o, *.d, engine_demo[.exe]
```

## Use
```bash
make            # ya: mingw32-make   (Windows MinGW)
make run
make clean
make rebuild
touch src/util.cxx && make      # sirf util.o + link  (incremental)
touch include/engine.hpp && make # saare .o rebuild   (auto dependency)
```

## Makefile ke parts (line by line)

### Variables
```make
CXX      := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -O2 -g -MMD -MP -Iinclude
BINDIR   := build
```
- `:=` **immediate** assignment (RHS abhi expand). `=` recursive (use pe expand).
- `-MMD -MP` → compiler har `.o` ke saath ek `.d` file likhta hai jisme us `.o`
  ki **header dependencies** hain. `-MP` phony targets add karta (deleted header
  se error na aaye).
- `-Iinclude` → `#include "engine.hpp"` ko `include/` mein dhoondo.

### OS-specific target name
```make
ifeq ($(OS),Windows_NT)
  EXE := .exe
endif
TARGET := $(BINDIR)/engine_demo$(EXE)
```
Windows pe `g++ -o foo` → `foo.exe`. Agar target ka naam `foo` rahe to Make ko
woh file kabhi "milegi nahi" → har baar relink. `$(OS)` env var Windows pe
`Windows_NT` hota hai.

### Source → object list (transform functions)
```make
SRCS := $(wildcard src/*.cxx)                       # src/hash.cxx src/util.cxx ...
OBJS := $(patsubst src/%.cxx,$(BINDIR)/%.o,$(SRCS)) # build/hash.o build/util.o ...
DEPS := $(OBJS:.o=.d)                               # build/hash.d ...
```

### The link rule
```make
$(TARGET): $(OBJS) | $(BINDIR)
	$(CXX) $(OBJS) $(LDFLAGS) -o $@
```
- Prereqs = saari `.o`. Koi bhi `.o` `$(TARGET)` se **newer** → relink.
- `| $(BINDIR)` = **order-only prerequisite** — "`build/` exist kare" par uska
  timestamp mat dekho (warna har naya file `build/` ka mtime badal ke sab relink
  kara deta).
- `$@` = target ka naam. `$<` = pehla prereq. `$^` = saare prereqs.

### Pattern rule
```make
$(BINDIR)/%.o: src/%.cxx | $(BINDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@
```
Ek rule, saari `src/X.cxx → build/X.o`. `%` = stem match.

### Auto-generated dependencies
```make
-include $(DEPS)
```
`-` = "file na mile to error mat do" (pehli baar `.d` nahi hoti). Jab `.d` aa
jaati hain, unme `build/hash.o: src/hash.cxx include/engine.hpp` jaisi lines hoti
hain → header badla to sahi `.o` rebuild. **Yeh** proper C/C++ Makefile ki asli
baat hai — bina iske, header change silently ignore ho jaata (stale builds).

### `.PHONY`
```make
.PHONY: all run clean rebuild
```
`run`/`clean` real files nahi hain — `.PHONY` batata hai "koi `run` naam ki file
ho tab bhi recipe chalao".

## Try karo

- `make` do baar chalao → doosri baar "Nothing to be done for 'all'".
- `touch src/hash.cxx` → sirf `hash.o` + link.
- `touch include/engine.hpp` → **saari** `.o` (auto-dep). `-MMD` hata ke dekho —
  ab header change pe kuch rebuild nahi hota (stale binary — silent bug).
- `make -j4` → parallel compile (independent `.o` ek saath).
- `make CXXFLAGS="-std=c++20 -O0 -g"` → command line se variable override.
- `cat build/hash.d` → dekho compiler ne kaunsi dependencies likhin.

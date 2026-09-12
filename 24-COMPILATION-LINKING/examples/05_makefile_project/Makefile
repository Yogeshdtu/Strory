# ============================================================
# Ek proper (chhota) Makefile — pattern rules + auto dependency
# tracking. Lesson 11.
# ============================================================

# ---- variables (recursively expanded '=', ya immediate ':=') ----
CXX      := g++
CXXSTD   := -std=c++20
CXXFLAGS := $(CXXSTD) -Wall -Wextra -Wpedantic -O2 -g -MMD -MP -Iinclude
LDFLAGS  :=
BINDIR   := build

# Windows pe g++ output ".exe" banata hai -> target ka naam match hona chahiye
# warna Make hamesha "stale" samjhega. ($(OS) Windows pe "Windows_NT" hota hai.)
ifeq ($(OS),Windows_NT)
  EXE := .exe
else
  EXE :=
endif
TARGET   := $(BINDIR)/engine_demo$(EXE)

# ---- source discovery -> object list ----
SRCS := $(wildcard src/*.cxx)
OBJS := $(patsubst src/%.cxx,$(BINDIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

# ---- default goal ----
.PHONY: all
all: $(TARGET)

# ---- link step: sab .o -> exe ----
$(TARGET): $(OBJS) | $(BINDIR)
	$(CXX) $(OBJS) $(LDFLAGS) -o $@
	@echo "built $@"

# ---- pattern rule: har src/X.cxx -> build/X.o ----
# $< = pehla prereq (source), $@ = target (object)
$(BINDIR)/%.o: src/%.cxx | $(BINDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ---- order-only prereq: build dir exist kare (timestamp na dekho) ----
$(BINDIR):
	@mkdir -p $(BINDIR)

# ---- auto-generated header dependencies (-MMD ne .d files banayi) ----
-include $(DEPS)

.PHONY: run clean rebuild
run: $(TARGET)
	@echo "---- run ----"
	@./$(TARGET)

clean:
	$(RM) -r $(BINDIR)

rebuild: clean all

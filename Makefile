# ============================================================
#  CPP-MASTERY — build helper
# ============================================================
#  Ek single file compile aur chalane ke liye:
#      make FILE=02-CPP-FIRST-STEPS/examples/01_hello_world.cpp
#
#  Sirf compile (chalao mat):
#      make build FILE=path/to/file.cpp
#
#  Sanitizers ke saath (memory/UB bugs pakadne ke liye):
#      make san FILE=path/to/file.cpp
#
#  Optimized build (benchmarks ke liye -- ZAROORI):
#      make fast FILE=path/to/file.cpp
#
#  Assembly dekhne ke liye:
#      make asm FILE=path/to/file.cpp
#
#  Preprocessor output dekhne ke liye:
#      make pp FILE=path/to/file.cpp
#
#  Ek folder ke saare examples compile karo:
#      make folder DIR=03-VARIABLES-DATA-TYPES
#
#  Saare examples check karo (poore repo mein):
#      make checkall
#
#  Safai:
#      make clean
# ============================================================

CXX      ?= g++
STD      ?= c++20

# Yeh warnings HAMESHA on rehni chahiye.
WARN      = -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion \
            -Wcast-align -Wunused -Wnull-dereference -Wdouble-promotion

# Debug build: warnings + debug symbols + no optimization
DEBUG_FLAGS = -std=$(STD) $(WARN) -g -O0

# Fast build: benchmarks ke liye. -O0 pe benchmark karna BEKAAR hai.
FAST_FLAGS  = -std=$(STD) -Wall -Wextra -O2 -g

# Sanitizer build: runtime bugs pakadta hai
SAN_FLAGS   = -std=$(STD) $(WARN) -g -O1 -fsanitize=address,undefined \
              -fno-omit-frame-pointer

BUILD_DIR = .build

.PHONY: run build fast san asm pp folder checkall clean help

# Default target
run: build
	@echo "──────────── OUTPUT ────────────"
	@$(BUILD_DIR)/$(notdir $(basename $(FILE)))

build: guard
	@mkdir -p $(BUILD_DIR)
	@echo "Compiling $(FILE) [debug]..."
	@$(CXX) $(DEBUG_FLAGS) "$(FILE)" -o $(BUILD_DIR)/$(notdir $(basename $(FILE)))
	@echo "OK -> $(BUILD_DIR)/$(notdir $(basename $(FILE)))"

fast: guard
	@mkdir -p $(BUILD_DIR)
	@echo "Compiling $(FILE) [-O2]..."
	@$(CXX) $(FAST_FLAGS) "$(FILE)" -o $(BUILD_DIR)/$(notdir $(basename $(FILE)))
	@echo "──────────── OUTPUT ────────────"
	@$(BUILD_DIR)/$(notdir $(basename $(FILE)))

san: guard
	@mkdir -p $(BUILD_DIR)
	@echo "Compiling $(FILE) [sanitizers]..."
	@$(CXX) $(SAN_FLAGS) "$(FILE)" -o $(BUILD_DIR)/$(notdir $(basename $(FILE)))_san
	@echo "──────────── OUTPUT ────────────"
	@$(BUILD_DIR)/$(notdir $(basename $(FILE)))_san

asm: guard
	@mkdir -p $(BUILD_DIR)
	@$(CXX) -std=$(STD) -O2 -S -masm=intel "$(FILE)" -o - | c++filt | \
	  grep -v "^\s*\." | head -80

pp: guard
	@$(CXX) -std=$(STD) -E "$(FILE)" | tail -40

folder:
	@if [ -z "$(DIR)" ]; then echo "Usage: make folder DIR=03-VARIABLES-DATA-TYPES"; exit 1; fi
	@mkdir -p $(BUILD_DIR)
	@fail=0; \
	for f in $(DIR)/examples/*.cpp; do \
	  [ -e "$$f" ] || continue; \
	  printf "%-55s " "$$f"; \
	  if $(CXX) $(DEBUG_FLAGS) "$$f" -o $(BUILD_DIR)/tmp 2>/dev/null; then \
	    echo "OK"; \
	  else echo "FAIL"; fail=1; fi; \
	done; \
	exit $$fail

checkall:
	@mkdir -p $(BUILD_DIR)
	@ok=0; bad=0; \
	for f in $$(find . -name '*.cpp' -not -path './$(BUILD_DIR)/*'); do \
	  if $(CXX) -std=$(STD) -Wall -Wextra "$$f" -o $(BUILD_DIR)/tmp 2>/dev/null; then \
	    ok=$$((ok+1)); \
	  else \
	    bad=$$((bad+1)); echo "FAIL: $$f"; \
	  fi; \
	done; \
	echo "──────────────────────────"; \
	echo "Compiled OK : $$ok"; \
	echo "Failed      : $$bad  (broken_on_purpose files ka fail hona EXPECTED hai)"

guard:
	@if [ -z "$(FILE)" ]; then \
	  echo "Usage: make FILE=path/to/file.cpp"; \
	  echo "Try:   make help"; exit 1; fi
	@if [ ! -f "$(FILE)" ]; then echo "File nahi mili: $(FILE)"; exit 1; fi

clean:
	@rm -rf $(BUILD_DIR)
	@echo "Saaf ho gaya."

help:
	@sed -n '2,32p' Makefile | sed 's/^# \{0,1\}//'

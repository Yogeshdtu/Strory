#!/usr/bin/env bash
# 07_pgo_workflow.sh
# ============================================================
# Profile-Guided Optimization (PGO) ka poora workflow, GCC.
# Portable (Linux / Git-Bash / MinGW). `folder`/`checkall` ise skip karte
# (yeh .sh hai, .cpp nahi).
#
# Idea: compiler guess karta kaunsi branch hot hai, kaunsa function inline
# karna, loop kitna unroll. PGO usse GUESS nahi karne deta -- ek asli run
# ka profile de do, phir woh data se decide karta.
#
# 3 steps:
#   1. -fprofile-generate  se build -> instrumented binary
#   2. representative workload pe chalao -> .gcda profile files
#   3. -fprofile-use se REBUILD -> profile-optimized binary
# ============================================================
set -euo pipefail
cd "$(dirname "$0")"
SRC="${1:-03_vectorization.cpp}"     # koi bhi is folder ka example
CXX="${CXX:-g++}"
STD="-std=c++20"
mkdir -p _pgo
PROF="$PWD/_pgo"

echo "=== 0. baseline (-O2, no PGO) ==="
$CXX $STD -O2 "$SRC" -o _pgo/base
time ./_pgo/base >/dev/null

echo
echo "=== 1. build instrumented (-fprofile-generate) ==="
$CXX $STD -O2 -fprofile-generate="$PROF" "$SRC" -o _pgo/instr
# instrumented binary ~2-4x dheema chalta -- yeh normal hai

echo "=== 2. run the representative workload (writes *.gcda) ==="
# ASLI production jaisa input do. Yahan example khud ek workload hai.
./_pgo/instr >/dev/null
./_pgo/instr >/dev/null      # a couple of runs = more stable profile
ls -la "$PROF"/*.gcda 2>/dev/null || echo "  (.gcda files "$PROF" mein)"

echo
echo "=== 3. rebuild with the profile (-fprofile-use) ==="
# -fprofile-correction: multi-threaded / slightly-stale profile ko tolerate
$CXX $STD -O2 -fprofile-use="$PROF" -fprofile-correction "$SRC" -o _pgo/opt
time ./_pgo/opt >/dev/null

echo
echo "=== compare sizes ==="
size _pgo/base _pgo/opt 2>/dev/null || true

cat <<'EOF'

Kya PGO badalta:
  * branch layout    -- asli-hot path straight-line, cold path out-of-line
  * inlining         -- sirf woh callsites inline jo profile mein hot the
  * loop unroll/vec  -- trip-count distribution se decide (hot short loops)
  * function ordering -- hot functions ko paas rakho -> I-cache/iTLB
  * register alloc / spill placement -- spills ko cold paths mein

Typical real gains: 5-20% on branchy / large codebases (interpreters,
compilers, databases, trading engines). Tight numeric kernels: less
(compiler already had enough info).

HFT release recipe (folder 24 lesson 15):  -O2 (ya -O3) + -march=native
  + -flto + PGO, profile = a REPLAYED capture of a real trading session.
Validate on P50/P99 of the replay, NOT a micro-benchmark.

Gotchas:
  * profile must be REPRESENTATIVE -- wrong workload = pessimization
  * -fprofile-generate binary is slow + writes files (don't ship it)
  * stale profile after code change -> -Wcoverage-mismatch, rebuild profile
  * AutoFDO (perf-based, no instrumented build) is the modern alternative
EOF

#!/usr/bin/env bash
# ============================================================
# 02_profile_analysis.sh -- pipeline ka bottleneck DHOONDHO (Linux, perf).
#
# Yeh 01_baseline_pipeline ko profile karta: kahan time ja raha hai, KYUN
# (cache miss? branch miss? frontend? divide?), aur kaunsa lesson us signal
# ko address karta.
#
# Windows/MinGW box pe `perf` nahi hota -> yeh script yahan CHALAYI NAHI
# GAYI. `bash -n` se syntax-check kiya gaya. Linux pe chalao:
#     sudo apt install linux-tools-common linux-tools-$(uname -r)
#     ./02_profile_analysis.sh
#
# Neeche har command ke saath: kya dekhna hai + kaunsa lesson.
# ============================================================
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN="${HERE}/base"
SRC="${HERE}/01_baseline_pipeline.cpp"

# 0. -O2 + frame pointers (accurate stacks). -fno-omit-frame-pointer zaroori.
g++ -std=c++20 -O2 -g -fno-omit-frame-pointer -Wall -Wextra "${SRC}" -o "${BIN}"

echo "=============================================================="
echo " 1. perf stat -- top-line health (IPC, miss rates)"
echo "=============================================================="
# Dekho:
#   - instructions per cycle (IPC): < 1.0 -> stalled (memory ya frontend)
#   - branch-misses %: > 2-3% of branches -> branch predictor hurt (lesson 04/05, 36/06)
#   - LLC-load-misses: high -> data cache problem (lesson 07 struct layout, 36/10)
#   - stalled-cycles-frontend high -> I-cache / decode (lesson 06)
perf stat -d -d -d \
    -e task-clock,cycles,instructions,branches,branch-misses \
    -e L1-dcache-loads,L1-dcache-load-misses,LLC-loads,LLC-load-misses \
    -e L1-icache-load-misses,iTLB-load-misses,stalled-cycles-frontend,stalled-cycles-backend \
    "${BIN}"

echo
echo "=============================================================="
echo " 2. perf record + report -- kaunsa FUNCTION"
echo "=============================================================="
# --call-graph dwarf: inlined-heavy C++ ke liye dwarf unwinding LBR se behtar.
perf record -F 2000 --call-graph dwarf -o /tmp/pl.data -- "${BIN}"
perf report -i /tmp/pl.data --stdio --percent-limit 1 | head -40
# Umeed: std::stod / std::string / __dtoa / std::_Rb_tree (map) top pe.
# -> parse + book bottleneck, jaisa 01 ka (B) attribution kehta.

echo
echo "=============================================================="
echo " 3. perf annotate -- us function ki kaunsi LINE / instruction"
echo "=============================================================="
HOT_FN="$(perf report -i /tmp/pl.data --stdio --percent-limit 1 \
          | awk '/%/{print $NF; exit}')"
echo "hottest symbol: ${HOT_FN:-<none>}"
perf annotate -i /tmp/pl.data --stdio --percent-limit 2 "${HOT_FN:-main}" | head -40
# Dekho: kaunsi machine instruction pe sample-count zyada. `div`, `idiv` ->
# lesson 10 (avoid division). Load jiske aage stall -> cache miss -> lesson 07.

echo
echo "=============================================================="
echo " 4. flamegraph -- poora call tree ek nazar mein (optional)"
echo "=============================================================="
if [ -d "${FLAMEGRAPH_DIR:-/opt/FlameGraph}" ]; then
    FG="${FLAMEGRAPH_DIR:-/opt/FlameGraph}"
    perf script -i /tmp/pl.data | "${FG}/stackcollapse-perf.pl" \
        | "${FG}/flamegraph.pl" > /tmp/pl.svg
    echo "wrote /tmp/pl.svg  (browser mein kholo)"
else
    echo "FlameGraph nahi mila -- skip. git clone https://github.com/brendangregg/FlameGraph"
fi

echo
echo "=============================================================="
echo " 5. perf stat -- SIRF divide ports (Amdahl: division ka hissa)"
echo "=============================================================="
# Intel: arith.divider_active. AMD Zen: alternate name -- `perf list | grep -i div`.
perf stat -e cycles,arith.divider_active "${BIN}" 2>/dev/null \
    || echo "(divider event is machine pe available nahi -- perf list se dekho)"

echo
echo "=============================================================="
echo " 6. perf c2c -- false sharing (jab pipeline multi-threaded ho -- 08)"
echo "=============================================================="
echo "  perf c2c record -- <multi-threaded-bin>   &&   perf c2c report"
echo "  'HITM' (modified line dusre core se) high -> false sharing -> lesson 08."

cat <<'NOTES'

--------------------------------------------------------------------
SIGNAL -> LESSON map (jo perf bataye, wahan jao):
--------------------------------------------------------------------
  low IPC + high LLC-load-misses         -> 07 struct layout, 11 lookup tables
                                            (data cache), 36/10 cache locality
  high branch-misses                     -> 04/05 hot-cold path, 36/06 branchless
  high stalled-cycles-frontend / iTLB    -> 06 I-cache layout, PGO, BOLT
  `idiv` / arith.divider_active high      -> 10 avoiding division, 09 fixed-point
  time in std::stod/_Rb_tree/malloc      -> 13/14 case studies (hand-parse,
                                            flat book, preallocation 36/04-05)
  HITM in perf c2c (multi-threaded)      -> 08 false sharing

Rule: pehle (1) se pta karo KYA bound hai (frontend/backend/memory/branch/
divide), PHIR (2)->(3) se KAHAN, PHIR wahi ek cheez fix karo, PHIR
re-measure (01 ka number). Ek baar mein ek change. -- 01-optimization-methodology.md
NOTES

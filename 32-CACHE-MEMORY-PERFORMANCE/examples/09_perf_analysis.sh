#!/usr/bin/env bash
# 09_perf_analysis.sh
# ============================================================
# `perf` se cache/memory behaviour naapne ka workflow. LINUX-ONLY (is repo
# ka box Windows/MinGW hai -> yeh script yahan chalegi nahi; WSL ya Linux
# target box pe chalao). `folder`/`checkall` ise compile nahi karte (.sh).
#
# Install:  Debian/Ubuntu:  sudo apt install linux-tools-common linux-tools-$(uname -r)
#           Fedora:         sudo dnf install perf
# Permission (ek baar):     sudo sysctl kernel.perf_event_paranoid=1   # ya -1 dev box pe
# ============================================================

set -euo pipefail
PROG="${1:?usage: ./09_perf_analysis.sh ./your_program [args...]}"
shift || true
ARGS=("$@")

echo "================================================================"
echo " 1. Top-level: kitne cache misses, kitni miss rate, IPC"
echo "================================================================"
# cache-misses / cache-references = LLC miss ratio. instructions/cycles = IPC.
perf stat -e cycles,instructions,cache-references,cache-misses,\
L1-dcache-loads,L1-dcache-load-misses,LLC-loads,LLC-load-misses,\
dTLB-loads,dTLB-load-misses,page-faults \
    "$PROG" "${ARGS[@]}"

echo
echo "================================================================"
echo " 2. MPKI (misses per 1000 instructions) -- normalized metric"
echo "================================================================"
# perf 'metricgroup' -- newer perf: -M. Warna khud calc: misses / (instr/1000).
perf stat -M l1d_mpki,l2_mpki,llc_mpki "$PROG" "${ARGS[@]}" 2>/dev/null \
    || echo "  (is perf build mein -M metrics nahi; khud: L1-dcache-load-misses / instructions * 1000)"

echo
echo "================================================================"
echo " 3. Top-down: bottleneck frontend / backend / bad-spec / retiring"
echo "================================================================"
# 'backend bound -> memory bound' = cache/DRAM stalls. Yeh sabse tez triage.
perf stat --topdown -a --no-merge "$PROG" "${ARGS[@]}" 2>/dev/null \
    || perf stat -e "{cycles,instructions}" --metric-only "$PROG" "${ARGS[@]}" \
    || true

echo
echo "================================================================"
echo " 4. Localize: kaunsi line/instruction miss kar rahi"
echo "================================================================"
perf record -e cache-misses -c 10000 -g -o perf.data -- "$PROG" "${ARGS[@]}"
perf report -i perf.data --stdio --sort=dso,symbol | head -40
echo "  (interactive: perf report -i perf.data ; phir 'a' se annotate)"

echo
echo "================================================================"
echo " 5. False sharing / cross-core line bouncing: perf c2c"
echo "================================================================"
# HITM (modified line doosre core se aayi) = false/true sharing ka signature.
perf c2c record -o c2c.data -- "$PROG" "${ARGS[@]}" 2>/dev/null && \
perf c2c report -i c2c.data --stdio | head -50 \
    || echo "  (perf c2c ke liye kernel >= 4.10 + hardware support chahiye)"

echo
echo "================================================================"
echo " 6. Bina root / bina perf: cachegrind (simulated, ~40x slow, deterministic)"
echo "================================================================"
echo "  valgrind --tool=cachegrind --cache-sim=yes $PROG ${ARGS[*]}"
echo "  -> D1 miss rate, LLd miss rate, LLi miss rate; cg_annotate se per-line."

# ---- kya dekhna ----
# * cache-misses / cache-references > ~10%  -> memory-bound suspect
# * L1-dcache-load-misses MPKI > ~20        -> L1 working set / access pattern
# * LLC-load-misses MPKI > ~5               -> DRAM-bound; blocking/SoA/smaller types
# * dTLB-load-misses high                   -> huge pages (lesson 11)
# * IPC < 1 on a compute loop               -> stalling; topdown se kis pe
# * perf c2c HITM remote                    -> false sharing -> pad to 64B (example 04)

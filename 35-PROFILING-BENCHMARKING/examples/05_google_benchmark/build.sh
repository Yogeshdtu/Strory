#!/usr/bin/env bash
# build.sh -- Google-Benchmark-style example (Linux / Git Bash), local shim.
# folder/checkall ise skip karte (.cxx/.hpp).
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p _out

FLAGS="-std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Wsign-conversion"

echo "== build (minibench shim, no dependency) =="
g++ $FLAGS bench.cxx -o _out/bench
./_out/bench

cat <<'EOF'

-- ASLI Google Benchmark ke saath (library installed) --
   sudo apt install libbenchmark-dev          # ya vcpkg / brew / source
   bench.cxx mein: #include "minibench.hpp"  ->  #include <benchmark/benchmark.h>
   g++ -std=c++20 -O2 bench.cxx -lbenchmark -lpthread -o bench
   ./bench --benchmark_repetitions=10 --benchmark_report_aggregates_only=true
   ./bench --benchmark_filter='BM_memcpy.*' --benchmark_format=json > out.json

   Real GB extra deta: CPU-time vs wall-time, repeats + mean/median/stddev/cv,
   RegisterBenchmark (runtime), templated benchmarks, complexity (BigO),
   --benchmark_min_time, --benchmark_counters_tabular, manual timers.
EOF

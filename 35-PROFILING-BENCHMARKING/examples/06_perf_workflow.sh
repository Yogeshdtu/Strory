#!/usr/bin/env bash
# 06_perf_workflow.sh
# ============================================================
# `perf` ka poora practical workflow -- Linux only (perf = kernel tool,
# Windows/MinGW pe nahi). `folder`/`checkall` ise skip karte (.sh).
#
# Yeh script khud ek chhota demo program banata jisme JAAN-BOOJH kar 2
# bottleneck hain:
#   1. per-element `%` (modulo) -- slow integer divide (folder 31/09)
#   2. random-stride array walk  -- cache misses (folder 32)
# ...phir perf se dono ko dhoondta.
#
# Steps:  count (perf stat)  ->  sample (perf record/report)  ->
#         annotate (source+asm)  ->  top-down (bound kis cheez se)
# ============================================================
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p _perf
CXX="${CXX:-g++}"

# ---- 0. the demo program --------------------------------------------
cat > _perf/demo.cpp <<'CPP'
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <random>

// BOTTLENECK 1: runtime `%` -- divider unit, ~20-30 cyc, poorly pipelined
static std::uint64_t sum_mod(const std::vector<std::uint32_t>& a, std::uint32_t m) {
    std::uint64_t s = 0;
    for (std::uint32_t x : a) s += x % m;            // <- hot: div
    return s;
}

// BOTTLENECK 2: pointer-chase / random stride -- every access ~an L3/DRAM miss
static std::uint64_t chase(const std::vector<std::uint32_t>& idx,
                           const std::vector<std::uint32_t>& data) {
    std::uint64_t s = 0, p = 0;
    for (std::size_t i = 0; i < idx.size(); ++i) { p = idx[p]; s += data[p]; }
    return s;
}

int main(int argc, char**) {
    const std::size_t N = 1u << 24;                  // 16 M
    std::vector<std::uint32_t> a(N), data(N), idx(N);
    std::mt19937 rng(1);
    for (auto& x : a) x = rng();
    for (auto& x : data) x = rng();
    for (std::size_t i = 0; i < N; ++i) idx[i] = rng() % N;   // random permutation-ish

    std::uint64_t acc = 0;
    for (int r = 0; r < 8; ++r) {
        acc += sum_mod(a, 1000003u + static_cast<std::uint32_t>(r));
        acc += chase(idx, data);
    }
    std::printf("%llu %d\n", (unsigned long long)acc, argc);
    return 0;
}
CPP

echo "=== build (-O2 -g -fno-omit-frame-pointer) ==="
# -g                       : source line info for `perf annotate`
# -fno-omit-frame-pointer  : reliable stacks for `perf record -g` (ya --call-graph dwarf)
$CXX -std=c++20 -O2 -g -fno-omit-frame-pointer _perf/demo.cpp -o _perf/demo

# perf allowed hai? (paranoid level)
echo "  kernel.perf_event_paranoid = $(cat /proc/sys/kernel/perf_event_paranoid 2>/dev/null || echo '?')"
echo "  (>1 -> 'sudo sysctl kernel.perf_event_paranoid=1', ya perf ko sudo se chalao)"

# ============================================================
#  1. perf stat -- COUNTING mode. "Kya bura hai" ka overview.
# ============================================================
echo
echo "=== 1. perf stat (counters) ==="
perf stat -r 3 ./_perf/demo || true
# Padhna:
#   insn per cycle (IPC)  -- <1.0 -> stalls (memory/deps). ~2-4 -> compute-bound & healthy
#   branch-misses %       -- >2-3% -> branchy hot path (folder 33/08)
#   the "time elapsed"    -- run-to-run variance dekho

echo
echo "=== 1b. perf stat -d -d (cache breakdown) ==="
perf stat -d -d ./_perf/demo || true
#   L1-dcache-load-misses, LLC-load-misses -- `chase()` yahan chamkega

echo
echo "=== 1c. perf stat topdown (bound kis se?) ==="
# Modern Intel/AMD: ek hi run mein Frontend/Backend/Retiring/BadSpec %
perf stat -M TopdownL1 ./_perf/demo 2>/dev/null || \
perf stat --topdown ./_perf/demo 2>/dev/null || \
echo "  (topdown events is CPU/perf pe available nahi)"

# ============================================================
#  2. perf record + report -- SAMPLING mode. "Time kahan ja raha".
# ============================================================
echo
echo "=== 2. perf record -g  +  perf report ==="
# -F 999   : 999 Hz sampling (997/999 -- prime, taaki periodic load se sync na ho)
# -g       : call graphs (frame-pointer). Bade C++ pe: --call-graph dwarf
perf record -F 999 -g -o _perf/perf.data ./_perf/demo
echo "--- top functions (self time) ---"
perf report -i _perf/perf.data --stdio --sort=overhead --percent-limit 1 2>/dev/null | grep -E '^\s+[0-9]' | head -15
# Expect: sum_mod aur chase dono top pe (~mila-jula), baaki noise

# ============================================================
#  3. perf annotate -- hot function ke andar, LINE / INSTRUCTION level
# ============================================================
echo
echo "=== 3. perf annotate sum_mod (source + %) ==="
perf annotate -i _perf/perf.data --stdio -l sum_mod 2>/dev/null | head -40 || true
#   `div`/`idiv` instruction pe bada % -> yeh modulo. Fix: agar `m` compile-
#   time hota to compiler reciprocal-multiply karta (folder 34/08). Runtime
#   `m` ke liye: libdivide, ya algorithm badlo.

echo
echo "=== 3b. perf annotate chase ==="
perf annotate -i _perf/perf.data --stdio -l chase 2>/dev/null | head -30 || true
#   `mov (...,%rax,4), %e..` load pe bada % -> cache miss. Fix: layout
#   badlo (SoA/blocking -- folder 32), prefetch, ya access pattern.

cat <<'EOF'

============================================================
 perf CHEAT-SHEET
============================================================
 perf list                          -- saare events (hardware + software + PMU)
 perf stat -e cycles,instructions,cache-misses,branch-misses ./app
 perf stat -r 5 ./app               -- 5 runs + mean/stddev
 perf stat -d -d -d ./app           -- progressively more cache detail
 perf stat -M TopdownL1 ./app       -- frontend/backend/retiring/bad-spec %
 perf stat -p <pid> -- sleep 10     -- ek chal rahe process ko 10s observe

 perf record -F 999 -g ./app        -- sample at 999 Hz with call graphs
 perf record --call-graph dwarf ./app  -- DWARF unwind (no -fno-omit-fp needed, bada .data)
 perf record -e cache-misses -c 10000 ./app  -- har 10k cache-misses pe ek sample
 perf report --stdio                -- text report
 perf report                        -- interactive TUI (arrows, 'a' = annotate)
 perf annotate <fn> --stdio -l      -- source+asm+% for one function

 perf top                           -- system-wide live "top" of functions
 perf sched / perf lock / perf mem / perf c2c   -- specialised
 perf diff old.data new.data        -- do profiles compare karo

 GOTCHAS:
  * kernel.perf_event_paranoid too high -> `sudo sysctl -w kernel.perf_event_paranoid=1`
  * symbols missing ("[unknown]") -> build with -g ; don't strip ; --call-graph dwarf
  * skid: an event's sample can be attributed a few instructions late -> use
    PEBS/IBS events (`:pp` suffix) for precise attribution
  * VM/container: many PMU events unavailable -> perf falls back to software events
  * frequency scaling se run-to-run variance -> pin freq (folder 31/13), -r N
EOF

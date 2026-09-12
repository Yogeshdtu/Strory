#!/usr/bin/env bash
# 07_flamegraph.sh
# ============================================================
# Flame graph banana aur padhna -- Linux only (perf + FlameGraph scripts).
# `folder`/`checkall` ise skip karte (.sh).
#
# Flame graph = ek SVG jo dikhata "CPU time kis call-stack mein gaya".
#   x-axis  = % of samples (WIDTH = time). NOTE: x alphabetical hai,
#             chronological NAHI -- left-to-right time ka order nahi.
#   y-axis  = stack depth (neeche main, upar leaf).
#   ek box  = ek function. Box jitna chauda, utna zyada time uske (ya
#             uske callees ke) andar.
#   "plateau" (upar chauda flat box) = wahan asli kaam ho raha / wahan
#             atka hua.
# ============================================================
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p _flame
CXX="${CXX:-g++}"

# ---- 0. demo: 3 functions, alag-alag cost -------------------------
cat > _flame/demo.cpp <<'CPP'
#include <cstdint>
#include <cstdio>
#include <cmath>
static double heavy(std::uint64_t n){ double s=0; for(std::uint64_t i=0;i<n;i++) s+=std::sqrt((double)(i*2654435761u%97+1)); return s; }
static double light(std::uint64_t n){ double s=0; for(std::uint64_t i=0;i<n;i++) s+=(double)(i&7); return s; }
static double mid(std::uint64_t n){ return heavy(n/4) + light(n); }
int main(){
    double s=0;
    for(int r=0;r<200;r++){ s+=heavy(2'000'000); s+=mid(2'000'000); s+=light(2'000'000); }
    std::printf("%f\n", s);
}
CPP

echo "=== build ==="
$CXX -std=c++20 -O2 -g -fno-omit-frame-pointer _flame/demo.cpp -o _flame/demo

# ---- 1. FlameGraph scripts (Brendan Gregg) -----------------------
FG="${FG:-$HOME/FlameGraph}"
if [ ! -x "$FG/flamegraph.pl" ]; then
    echo "FlameGraph scripts nahi mile ($FG). Le aao:"
    echo "   git clone https://github.com/brendangregg/FlameGraph ~/FlameGraph"
    echo "   (ya FG=/path/to/FlameGraph is script ko do)"
    echo
    echo "Aage ka flow (jab scripts mil jaayein):"
fi

# ============================================================
#  2. record -> fold -> render
# ============================================================
cat <<'EOF'
=== on-CPU flame graph (3 commands) ===
  perf record -F 999 -g -- ./_flame/demo           # 999 Hz, call graphs
  perf script | "$FG"/stackcollapse-perf.pl > _flame/out.folded
  "$FG"/flamegraph.pl _flame/out.folded > _flame/cpu.svg
  # _flame/cpu.svg ko browser mein kholo -- boxes clickable (zoom), Ctrl-F search

  # bada C++ / missing frames -> DWARF unwind:
  perf record -F 999 --call-graph dwarf -- ./_flame/demo
EOF

# actually run it if perf + scripts available
if command -v perf >/dev/null && [ -x "$FG/flamegraph.pl" ]; then
    echo
    echo "=== running for real ==="
    perf record -F 999 -g -o _flame/perf.data -- ./_flame/demo >/dev/null
    perf script -i _flame/perf.data | "$FG"/stackcollapse-perf.pl > _flame/out.folded
    "$FG"/flamegraph.pl --title "demo on-CPU" _flame/out.folded > _flame/cpu.svg
    echo "  -> _flame/cpu.svg  ($(wc -l < _flame/out.folded) unique stacks)"
    echo "  top folded stacks by sample count:"
    sort -t' ' -k2 -nr _flame/out.folded | head -5
    echo
    echo "  EXPECT: heavy() sabse chauda plateau (~direct + mid ke through),"
    echo "  light() patla, mid() ek frame jiske upar heavy+light baante hue."
fi

cat <<'EOF'

============================================================
 FLAME GRAPH -- padhne ke rules
============================================================
  * WIDTH = time (samples). Sirf width dekho, colour random hota (hue = hash).
  * Upar ka chauda flat box ("plateau") = wahi hot leaf -- optimize wahan.
  * x-axis ALPHABETICAL hai, time-order nahi -- "pehle-baad" mat padho.
  * Ek hi function do jagah patla-patla -> alag call paths se; wing merge
    nahi hote jab tak same parent na ho.
  * "[unknown]" / tute frames -> -g nahi, ya stripped, ya JIT -> --call-graph dwarf.

 VARIANTS:
  * Icicle graph      -- ulta (root upar). `flamegraph.pl --inverted`
  * Differential      -- do profiles ka diff (red = badha, blue = ghata):
        "$FG"/difffolded.pl old.folded new.folded | "$FG"/flamegraph.pl > diff.svg
  * Off-CPU flame     -- kahan BLOCKED (I/O, lock, sleep) -- perf sched ya
        bpftrace/offcputime (BCC). On-CPU + off-CPU = poori tasveer.
  * Alloc / page-fault flame -- `perf record -e page-faults -g` phir wahi pipeline.

 ALTERNATIVES: `hotspot` (Qt GUI for perf.data, flame graph built-in),
   `perf report --stdio -g 'graph,0.5,caller'` (text tree),
   Firefox Profiler (import perf.data), Speedscope (import `perf script`).
EOF

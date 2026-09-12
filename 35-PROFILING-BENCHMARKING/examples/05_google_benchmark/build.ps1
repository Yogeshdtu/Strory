# build.ps1 -- Google-Benchmark-style example, built with the local shim.
# `folder`/`checkall` ise skip karte (.cxx/.hpp -- .cpp nahi).
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
$out = Join-Path $PSScriptRoot '_out'
New-Item -ItemType Directory -Force -Path $out | Out-Null

$flags = @('-std=c++20', '-O2', '-Wall', '-Wextra', '-Wshadow', '-Wconversion', '-Wsign-conversion')

Write-Host "== build (minibench shim, no dependency) ==" -ForegroundColor Cyan
& g++ @flags bench.cxx -o "$out\bench.exe"
& "$out\bench.exe"

Write-Host @"

-- ASLI Google Benchmark ke saath (Linux, library installed) --
   sudo apt install libbenchmark-dev      # ya vcpkg / brew / build from source
   In bench.cxx: `#include "minibench.hpp"` ko `#include <benchmark/benchmark.h>` kar do
   g++ -std=c++20 -O2 bench.cxx -lbenchmark -lpthread -o bench
   ./bench --benchmark_repetitions=10 --benchmark_report_aggregates_only=true
   ./bench --benchmark_filter='BM_memcpy.*' --benchmark_format=json > out.json
"@ -ForegroundColor DarkGray

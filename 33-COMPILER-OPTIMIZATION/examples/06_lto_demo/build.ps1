# build.ps1 -- LTO demo: build both ways, run both, compare.
$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot
Set-Location $here
$out = Join-Path $here '_out'
New-Item -ItemType Directory -Force -Path $out | Out-Null

Write-Host "== NO LTO ==" -ForegroundColor Cyan
& g++ -std=c++20 -O2 -Wall -Wextra -c main.cxx  -o "$out\main_nolto.o"
& g++ -std=c++20 -O2 -Wall -Wextra -c mathx.cxx -o "$out\mathx_nolto.o"
& g++ -std=c++20 -O2 "$out\main_nolto.o" "$out\mathx_nolto.o" -o "$out\demo_nolto.exe"
& "$out\demo_nolto.exe"

Write-Host "`n== WITH LTO ==" -ForegroundColor Cyan
& g++ -std=c++20 -O2 -flto -D__LTO_BUILD__ -Wall -Wextra -c main.cxx  -o "$out\main_lto.o"
& g++ -std=c++20 -O2 -flto -Wall -Wextra -c mathx.cxx -o "$out\mathx_lto.o"
& g++ -std=c++20 -O2 -flto "$out\main_lto.o" "$out\mathx_lto.o" -o "$out\demo_lto.exe"
& "$out\demo_lto.exe"

Write-Host "`n(Compare ns/elem. -flto inlines hot_transform across the TU boundary.)" -ForegroundColor DarkGray

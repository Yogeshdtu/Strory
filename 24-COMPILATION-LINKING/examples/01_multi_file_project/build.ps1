# Multi-file build (PowerShell) — har .cxx apni .obj, phir link.
$ErrorActionPreference = 'Stop'
$CXX  = if ($env:CXX) { $env:CXX } else { 'g++' }
$STD  = '-std=c++20'
$WARN = '-Wall','-Wextra','-Wpedantic','-Wshadow','-Wconversion'

Write-Host "== compile (har TU alag) =="
& $CXX $STD @WARN -c mathx.cxx -o mathx.o
& $CXX $STD @WARN -c stats.cxx -o stats.o
& $CXX $STD @WARN -c main.cxx  -o main.o

Write-Host "== link =="
& $CXX mathx.o stats.o main.o -o app.exe

Write-Host "== run =="
& .\app.exe

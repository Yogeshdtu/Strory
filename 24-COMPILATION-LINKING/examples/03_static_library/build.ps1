# Static library build (PowerShell). Inspection commands ke liye build.sh dekho.
$ErrorActionPreference = 'Stop'
$CXX = if ($env:CXX) { $env:CXX } else { 'g++' }
$STD = '-std=c++20'

Write-Host "== 1. compile sources -> .o =="
& $CXX $STD -O2 -c add.cxx    -o add.o
& $CXX $STD -O2 -c mul.cxx    -o mul.o
& $CXX $STD -O2 -c unused.cxx -o unused.o

Write-Host "== 2. archive .o -> libcalc.a =="
if (Test-Path libcalc.a) { Remove-Item libcalc.a }
& ar rcs libcalc.a add.o mul.o unused.o
& ar t libcalc.a

Write-Host "== 3. link app =="
& $CXX $STD -O2 -c main.cxx -o main.o
& $CXX main.o -L. -lcalc -o app.exe

Write-Host "== 4. run =="
& .\app.exe

Write-Host "== 5. app mein calc:: symbols (huge_unused NAHI hona chahiye) =="
& nm -C app.exe | Select-String 'calc::'
Write-Host "== app.exe size =="
& size app.exe

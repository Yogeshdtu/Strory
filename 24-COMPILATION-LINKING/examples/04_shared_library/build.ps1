# Shared library build (PowerShell / Windows) — greet.dll
$ErrorActionPreference = 'Stop'
$CXX = if ($env:CXX) { $env:CXX } else { 'g++' }
$STD = '-std=c++20'

Write-Host "== compile + link shared library (greet.dll + import lib) =="
& $CXX $STD -O2 -c greet.cxx -o greet.o
& $CXX -shared greet.o -o greet.dll -Wl,--out-implib,libgreet.dll.a

Write-Host "== link app against import lib =="
& $CXX $STD -O2 main.cxx -L. -lgreet -o app.exe

Write-Host "== run (greet.dll same folder -> loader finds it) =="
& .\app.exe

Write-Host "== app.exe ka DLL dependency list =="
& objdump -p app.exe | Select-String 'DLL Name'

Write-Host ""
Write-Host "greet.dll ko hata ke app.exe chalao -> 'code execution cannot proceed' (DLL missing)."

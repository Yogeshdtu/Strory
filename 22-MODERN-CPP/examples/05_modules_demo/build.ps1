# build.ps1 -- build the C++20 modules demo (NOT picked up by ../../build.ps1 folder)
#   needs GCC with -fmodules-ts (verified on MinGW-w64 ucrt GCC 15.1.0)
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    g++ -std=c++20 -fmodules-ts -Wall -Wextra -c geometry.ixx -o geometry.o
    g++ -std=c++20 -fmodules-ts -Wall -Wextra main.cxx geometry.o -o modules_demo.exe
    Write-Host "built modules_demo.exe" -ForegroundColor Green
    & .\modules_demo.exe
}
finally {
    Pop-Location
}

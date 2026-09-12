<#
  build.ps1 — CPP-MASTERY build helper (Windows / PowerShell-native)
  =================================================================
  Yeh Makefile ka PowerShell version hai. Use karo jab `make` ya Git Bash
  available na ho. Sirf g++ chahiye PATH pe (MinGW-w64: C:\mingw64\bin).

  TARGETS
    .\build.ps1 <file.cpp>            debug compile + run   (default)
    .\build.ps1 build   <file.cpp>    sirf compile (debug warnings + -g -O0)
    .\build.ps1 fast    <file.cpp>    -O2 compile + run     (benchmarks ke liye ZAROORI)
    .\build.ps1 san     <file.cpp>    ASan + UBSan + run    (MinGW pe flaky — neeche note)
    .\build.ps1 asm     <file.cpp>    Intel-syntax assembly (demangled, pehli 80 lines)
    .\build.ps1 pp      <file.cpp>    preprocessor output   (aakhri 40 lines)
    .\build.ps1 folder  <DIR>         DIR\examples\*.cpp — sab compile karo
    .\build.ps1 checkall             poore repo ke *.cpp ka compile-check
    .\build.ps1 clean               .build\ hata do
    .\build.ps1 help

  NOTE (san): AddressSanitizer MinGW/Windows pe reliably kaam nahi karta.
  Link error aaye to Git Bash se `mingw32-make san FILE=...`, ya WSL/Linux use karo.

  NOTE (*.linux.cpp): jin examples ko Linux syscalls chahiye (mmap, epoll, fork,
  sched_setaffinity, POSIX sockets ...) unka naam `NN_name.linux.cpp` hota hai.
  `folder` aur `checkall` inhe SKIP karte hain (is Windows/MinGW box pe compile
  nahi hoti). Inhe Linux ya WSL pe verify karo:
      g++ -std=c++20 -O2 -pthread file.linux.cpp -o file && ./file

  NOTE (*.cpp23.cpp): C++23 examples (deducing this, std::generator, flat_map,
  std::print ...) ka naam `NN_name.cpp23.cpp` hota hai. Har target inhe apne aap
  -std=c++23 se compile karta hai, aur -lstdc++exp link karta hai (MinGW pe
  std::print ka terminal code usi library mein hai).

  NOTE (execution policy): agar "running scripts is disabled" error aaye:
      Set-ExecutionPolicy -Scope CurrentUser RemoteSigned
  ya ek baar ke liye:
      powershell -ExecutionPolicy Bypass -File .\build.ps1 checkall
#>

[CmdletBinding()]
param(
    [Parameter(Position = 0)] [string] $Command = 'help',
    [Parameter(Position = 1)] [string] $Target
)

# --- Agar pehla arg seedha .cpp file hai to 'run' maan lo -------------------
if ($Command -match '\.cpp$' -and -not $Target) {
    $Target  = $Command
    $Command = 'run'
}

$root     = $PSScriptRoot
$buildDir = Join-Path $root '.build'
$CXX = if ($env:CXX) { $env:CXX } else { 'g++' }
$STD = if ($env:STD) { $env:STD } else { 'c++20' }

# Yeh warnings HAMESHA on rehni chahiye (Makefile ke $(WARN) jaisa).
$WARN = @(
    '-Wall', '-Wextra', '-Wpedantic', '-Wshadow', '-Wconversion', '-Wsign-conversion',
    '-Wcast-align', '-Wunused', '-Wnull-dereference', '-Wdouble-promotion'
)
$DEBUG_FLAGS = @("-std=$STD") + $WARN + @('-g', '-O0')
$FAST_FLAGS  = @("-std=$STD", '-Wall', '-Wextra', '-O2', '-g')
$SAN_FLAGS   = @("-std=$STD") + $WARN + @('-g', '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer')
# Fallback jab libasan/libubsan na ho (e.g. MinGW-w64 on Windows): STL bounds
# checks + stack canaries. Raw C-array OOB NAHI pakadta -- uske liye Linux/Clang + ASan.
$HARDEN_FLAGS = @("-std=$STD") + $WARN + @('-g', '-O1',
    '-D_GLIBCXX_ASSERTIONS', '-fstack-protector-all', '-fno-omit-frame-pointer')
$CHECK_FLAGS = @("-std=$STD", '-Wall', '-Wextra')

function Die  ($m) { Write-Host $m -ForegroundColor Red; exit 1 }
function Info ($m) { Write-Host $m -ForegroundColor Cyan }
function Good ($m) { Write-Host $m -ForegroundColor Green }

function Assert-Cxx {
    if (-not (Get-Command $CXX -ErrorAction SilentlyContinue)) {
        Die "'$CXX' PATH pe nahi mila. MinGW-w64 install karo (C:\mingw64\bin) ya `$env:CXX set karo."
    }
}
function Ensure-BuildDir {
    if (-not (Test-Path $buildDir)) { New-Item -ItemType Directory -Path $buildDir -Force | Out-Null }
}
function Resolve-Src ($p) {
    if (-not $p)             { Die "File chahiye. Usage: .\build.ps1 <file.cpp>" }
    if (-not (Test-Path $p)) { Die "File nahi mili: $p" }
    (Resolve-Path -LiteralPath $p).Path
}
function Out-Exe ($src) {
    Join-Path $buildDir ([IO.Path]::GetFileNameWithoutExtension($src) + '.exe')
}

# *.cpp23.cpp files C++23 hain: -std=c++23 flags ke END mein (g++ aakhri -std maanta hai),
# aur -lstdc++exp SOURCE ke BAAD (linker libraries order mein padhta hai).
function Is-Cpp23 ($src) { return ($src -like '*.cpp23.cpp') }
function Std-For  ($src) { if (Is-Cpp23 $src) { 'c++23' } else { $STD } }
function Flags-For { param([string[]] $Flags, [string] $Src)
    if (Is-Cpp23 $Src) { return $Flags + @('-std=c++23') } else { return $Flags } }
function Libs-For ($src) { if (Is-Cpp23 $src) { return @('-lstdc++exp') } else { return @() } }

# Chup-chaap compile (folder/checkall ke liye). Return $true agar exit code 0.
function Try-Compile {
    param([string[]] $Flags, [string] $Src, [string] $OutExe)
    $errPath = Join-Path $buildDir '_stderr.txt'
    $outPath = Join-Path $buildDir '_stdout.txt'
    $Flags   = @(Flags-For $Flags $Src)
    $argLine = ($Flags -join ' ') + " `"$Src`" -o `"$OutExe`" " + (@(Libs-For $Src) -join ' ')
    $p = Start-Process -FilePath $CXX -ArgumentList $argLine -NoNewWindow -Wait -PassThru `
        -RedirectStandardError $errPath -RedirectStandardOutput $outPath
    return ($p.ExitCode -eq 0)
}

# Compile jisme warnings/errors console pe dikhein (single-file targets ke liye).
function Do-Compile {
    param([string[]] $Flags, [string] $Src, [string] $OutExe)
    # @( ) zaroori: PowerShell ek-element array ko function return pe string bana deta hai,
    # aur string ko @splat karne se g++ ko alag-alag characters milte hain.
    $Flags = @(Flags-For $Flags $Src)
    $libs  = @(Libs-For $Src)
    Info "Compiling $Src"
    Write-Host "  $CXX $($Flags -join ' ') `"$Src`" -o `"$OutExe`" $($libs -join ' ')" -ForegroundColor DarkGray
    & $CXX @Flags $Src -o $OutExe @libs
    if ($LASTEXITCODE -ne 0) { Die "Compile FAIL (exit $LASTEXITCODE)" }
    Good "OK -> $OutExe"
}
function Do-Run ($exe) {
    Write-Host "---------------- OUTPUT ----------------" -ForegroundColor DarkGray
    & $exe
    $code = $LASTEXITCODE
    Write-Host "---------------------------------------" -ForegroundColor DarkGray
    Write-Host "exit code: $code" -ForegroundColor DarkGray
}

$usage = @'
build.ps1 — CPP-MASTERY build helper (PowerShell)

  .\build.ps1 <file.cpp>            debug compile + run  (default)
  .\build.ps1 build   <file.cpp>   sirf compile
  .\build.ps1 fast    <file.cpp>   -O2 compile + run    (benchmarks)
  .\build.ps1 san     <file.cpp>   ASan + UBSan + run   (MinGW pe flaky)
  .\build.ps1 asm     <file.cpp>   Intel assembly (demangled)
  .\build.ps1 pp      <file.cpp>   preprocessor output
  .\build.ps1 folder  <DIR>        DIR\examples\*.cpp sab compile
  .\build.ps1 checkall            poore repo ka compile-check
  .\build.ps1 clean              .build\ hata do

  *.linux.cpp files (Linux-only syscalls) folder/checkall mein SKIP hoti hain --
  unhe Linux/WSL pe verify karo: g++ -std=c++20 -O2 -pthread f.linux.cpp -o f
  *.cpp23.cpp files apne aap -std=c++23 + -lstdc++exp se build hoti hain.

  Override: $env:CXX (default g++), $env:STD (default c++20)
'@

switch ($Command.ToLower()) {

    'run' {
        Assert-Cxx; Ensure-BuildDir
        $src = Resolve-Src $Target; $exe = Out-Exe $src
        Do-Compile $DEBUG_FLAGS $src $exe
        Do-Run $exe
        break
    }
    'build' {
        Assert-Cxx; Ensure-BuildDir
        $src = Resolve-Src $Target; $exe = Out-Exe $src
        Do-Compile $DEBUG_FLAGS $src $exe
        break
    }
    'fast' {
        Assert-Cxx; Ensure-BuildDir
        $src = Resolve-Src $Target; $exe = Out-Exe $src
        Do-Compile $FAST_FLAGS $src $exe
        Do-Run $exe
        break
    }
    'san' {
        Assert-Cxx; Ensure-BuildDir
        $src = Resolve-Src $Target; $exe = Out-Exe $src
        # ASan/UBSan try karo; agar link fail ho (MinGW) to hardened build pe fall back
        if (Try-Compile $SAN_FLAGS $src $exe) {
            Info "sanitizers (ASan + UBSan)"
            Write-Host "  $CXX $($SAN_FLAGS -join ' ') ..." -ForegroundColor DarkGray
            Good "OK -> $exe"
        }
        else {
            Info "ASan/UBSan is toolchain pe nahi mile (libasan/libubsan absent)."
            Info "Fallback: -D_GLIBCXX_ASSERTIONS + -fstack-protector-all (STL bounds + canaries)."
            Do-Compile $HARDEN_FLAGS $src $exe
        }
        Do-Run $exe
        break
    }
    'asm' {
        Assert-Cxx
        $src = Resolve-Src $Target
        $asm = & $CXX "-std=$(Std-For $src)" '-O2' '-S' '-masm=intel' $src '-o' '-'
        if (Get-Command c++filt -ErrorAction SilentlyContinue) { $asm = $asm | & c++filt }
        $asm | Where-Object { $_ -notmatch '^\s*\.' } | Select-Object -First 80
        break
    }
    'pp' {
        Assert-Cxx
        $src = Resolve-Src $Target
        (& $CXX "-std=$(Std-For $src)" '-E' $src) | Select-Object -Last 40
        break
    }
    'folder' {
        Assert-Cxx; Ensure-BuildDir
        if (-not $Target) { Die "Usage: .\build.ps1 folder <DIR>   (e.g. 06-CONDITIONS)" }
        $dir   = if ([IO.Path]::IsPathRooted($Target)) { $Target } else { Join-Path $root $Target }
        $exDir = Join-Path $dir 'examples'
        if (-not (Test-Path $exDir)) { Die "examples\ folder nahi mila: $exDir" }
        $files = Get-ChildItem -LiteralPath $exDir -Filter *.cpp -File | Sort-Object Name
        if (-not $files) { Die "Koi .cpp nahi mila: $exDir" }
        Info "toolchain: $((& $CXX --version | Select-Object -First 1))"
        $bad = 0; $expected = 0; $skipped = 0
        foreach ($f in $files) {
            $name     = $f.Name.PadRight(52)
            $isBroken = $f.Name -like '*broken*'
            if ($f.Name -like '*.linux.cpp') {
                Write-Host "$name SKIP (linux-only)" -ForegroundColor DarkGray
                $skipped++
                continue
            }
            if (Try-Compile $DEBUG_FLAGS $f.FullName (Join-Path $buildDir 'tmp.exe')) {
                if ($isBroken) { Write-Host "$name WARN (broken file compile ho gayi?)" -ForegroundColor Yellow }
                else           { Write-Host "$name OK" -ForegroundColor Green }
            }
            else {
                if ($isBroken) { Write-Host "$name FAIL (expected)" -ForegroundColor DarkYellow; $expected++ }
                else           { Write-Host "$name FAIL" -ForegroundColor Red; $bad++ }
            }
        }
        Write-Host ('-' * 64)
        Write-Host "FAIL (real)     : $bad"
        Write-Host "FAIL (expected) : $expected  (broken_on_purpose)"
        Write-Host "SKIP (linux)    : $skipped  (*.linux.cpp -- Linux/WSL pe verify karo)"
        if ($bad -gt 0) { exit 1 }
        break
    }
    'checkall' {
        Assert-Cxx; Ensure-BuildDir
        $files = Get-ChildItem -LiteralPath $root -Filter *.cpp -File -Recurse |
            Where-Object { $_.FullName -notlike "*\.build\*" -and $_.FullName -notlike "*\build\*" } | Sort-Object FullName
        $total = $files.Count
        Info "toolchain: $((& $CXX --version | Select-Object -First 1))"
        Info "$total .cpp files mile`n"
        $ok = 0; $bad = 0; $expected = 0; $skipped = 0; $i = 0; $fails = @()
        foreach ($f in $files) {
            $i++
            Write-Host -NoNewline ("`rChecking {0}/{1} ..." -f $i, $total)
            $isBroken = $f.Name -like '*broken*'
            if ($f.Name -like '*.linux.cpp') { $skipped++; continue }
            if (Try-Compile $CHECK_FLAGS $f.FullName (Join-Path $buildDir 'tmp.exe')) {
                $ok++
            }
            elseif ($isBroken) { $expected++ }
            else { $bad++; $fails += $f.FullName }
        }
        Write-Host ("`r" + (' ' * 40) + "`r") -NoNewline
        foreach ($p in $fails) {
            Write-Host ("FAIL: " + $p.Substring($root.Length).TrimStart('\')) -ForegroundColor Red
        }
        Write-Host ('-' * 30)
        Write-Host "Compiled OK     : $ok"
        Write-Host "Failed (real)   : $bad"
        Write-Host "Failed (expected): $expected  (broken_on_purpose files ka fail hona sahi hai)"
        Write-Host "Skipped (linux) : $skipped  (*.linux.cpp -- Linux-only, is box pe verify nahi hoti)"
        if ($bad -gt 0) { exit 1 }
        break
    }
    'clean' {
        if (Test-Path $buildDir) { Remove-Item -Recurse -Force $buildDir; Good ".build\ saaf ho gaya." }
        else { Write-Host "Pehle se saaf hai." }
        break
    }
    default {
        Write-Host $usage
        break
    }
}

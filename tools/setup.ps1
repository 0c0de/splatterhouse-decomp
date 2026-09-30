#Requires -Version 5.1
# Setup del entorno en Windows para Splatterhouse Recompiled
param(
    [switch]$InstallTools
)

$ErrorActionPreference = "Stop"
Write-Host "[setup] Splatterhouse Recompiled - Setup Windows" -ForegroundColor Cyan

function Test-Cmd($name) { $null -ne (Get-Command $name -ErrorAction SilentlyContinue) }

# 1. Git
if (-not (Test-Cmd git)) {
    Write-Host "[setup] Git no encontrado." -ForegroundColor Yellow
    if ($InstallTools) {
        winget install --id Git.Git -e --accept-package-agreements --accept-source-agreements
    } else {
        Write-Host "  Instala Git: winget install Git.Git  (o https://git-scm.com/download/win)" -ForegroundColor Yellow
    }
} else { Write-Host "[ok] git $(git --version)" -ForegroundColor Green }

# 2. CMake
if (-not (Test-Cmd cmake)) {
    Write-Host "[setup] CMake no encontrado." -ForegroundColor Yellow
    if ($InstallTools) { winget install --id Kitware.CMake -e --accept-package-agreements --accept-source-agreements }
    else { Write-Host "  winget install Kitware.CMake" -ForegroundColor Yellow }
} else { Write-Host "[ok] cmake $(cmake --version | Select-Object -First 1)" -ForegroundColor Green }

# 3. Ninja
if (-not (Test-Cmd ninja)) {
    Write-Host "[setup] Ninja no encontrado." -ForegroundColor Yellow
    if ($InstallTools) { winget install --id Ninja-build.Ninja -e --accept-package-agreements --accept-source-agreements }
    else { Write-Host "  winget install Ninja-build.Ninja" -ForegroundColor Yellow }
} else { Write-Host "[ok] ninja $(ninja --version)" -ForegroundColor Green }

# 4. Clang (ReXGlue requiere Clang 18+, 20.x recomendado)
if (Test-Cmd clang) {
    Write-Host "[ok] clang $(clang --version | Select-Object -First 1)" -ForegroundColor Green
    $clangVer = (clang --version)[0]
    if ($clangVer -match "version (\d+)\." -and [int]$Matches[1] -lt 18) {
        Write-Host "[warn] ReXGlue requiere Clang 18+ (encontrado: $clangVer)." -ForegroundColor Yellow
        Write-Host "  Instala LLVM con: winget install LLVM.LLVM" -ForegroundColor Yellow
    }
} else {
    Write-Host "[warn] Clang no encontrado. ReXGlue lo requiere (winget install LLVM.LLVM)." -ForegroundColor Yellow
    Write-Host "  En Windows tambien sirve el 'C++ Clang Compiler for Windows' de Visual Studio 2022." -ForegroundColor Yellow
}

# 5. Submodules (rexglue-sdk; los submodulos internos se piden con --recursive)
if (Test-Path ".git") {
    Write-Host "[setup] Inicializando submodulo rexglue-sdk..." -ForegroundColor Cyan
    git submodule update --init extern/rexglue-sdk
    Write-Host "[ok] rexglue-sdk listo" -ForegroundColor Green
} else {
    Write-Host "[info] No es repo git aun. Inicializa con: git init; git submodule..." -ForegroundColor Yellow
    Write-Host "  O clona con --recursive si ya es repo remoto." -ForegroundColor Yellow
}

# 6. Build bootstrap
Write-Host "`n[setup] Para compilar (sin XEX):" -ForegroundColor Cyan
Write-Host "  cmake --preset win-amd64-release"
Write-Host "  cmake --build out/build/win-amd64-release -j"
Write-Host "`n[setup] Para codegen con XEX (requiere rexglue CLI):" -ForegroundColor Cyan
Write-Host "  python tools/recompile.py --xex game/default.dec.xex"
Write-Host "  cmake --build out/build/win-amd64-release -j"

Write-Host "`n[setup] Hecho." -ForegroundColor Green

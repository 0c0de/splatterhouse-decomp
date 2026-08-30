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

# 4. Clang / MSVC
if (Test-Cmd clang) { Write-Host "[ok] clang $(clang --version | Select-Object -First 1)" -ForegroundColor Green }
else { Write-Host "[info] Clang no encontrado, se usara MSVC (Visual Studio 2022 requerido)." -ForegroundColor Yellow }

if (-not (Test-Cmd cl) -and -not (Test-Cmd clang)) {
    Write-Host "[warn] Ni cl.exe ni clang encontrados. Instala Visual Studio 2022 con 'Desktop development with C++'." -ForegroundColor Yellow
    Write-Host "  https://visualstudio.microsoft.com/downloads/" -ForegroundColor Yellow
}

# 5. Submodules
if (Test-Path ".git") {
    Write-Host "[setup] Inicializando submodulos..." -ForegroundColor Cyan
    git submodule update --init --recursive
    Write-Host "[ok] Submodulos listos" -ForegroundColor Green
} else {
    Write-Host "[info] No es repo git aun. Inicializa con: git init; git submodule..." -ForegroundColor Yellow
    Write-Host "  O clona con --recursive si ya es repo remoto." -ForegroundColor Yellow
}

# 6. Build bootstrap
Write-Host "`n[setup] Para compilar (sin XEX):" -ForegroundColor Cyan
Write-Host "  cmake --preset windows-release"
Write-Host "  cmake --build build/windows-release -j"
Write-Host "`n[setup] Para recompilar con XEX:"
Write-Host "  python tools/recompile.py --xex game/default.dec.xex"
Write-Host "  cmake --preset windows-release && cmake --build build/windows-release -j"

Write-Host "`n[setup] Hecho." -ForegroundColor Green

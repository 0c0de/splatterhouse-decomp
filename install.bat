@echo off
setlocal enableextensions
title Splatterhouse Recompiled - Instalador

set "ROOT=%~dp0"
set "TOOL=%ROOT%tools\extract-xiso.exe"
set "XEXTOOL=%ROOT%tools\xextool.exe"
set "ASSETS=%ROOT%assets"

echo ===============================================
echo   Splatterhouse Recompiled - Instalador
echo ===============================================
echo.

set "ISO=%~1"
if not "%ISO%"=="" goto have_iso

echo Arrastra aqui la ISO de Splatterhouse, o pega su ruta, y pulsa Enter:
set /p ISO=
if "%ISO%"=="" goto no_iso

:have_iso
set "ISO=%ISO:"=%"
if not exist "%ISO%" goto no_iso_file
if not exist "%TOOL%" goto no_tool

rem Config por defecto (sin esto no se carga la GPU -> pantalla negra)
if exist "%ROOT%splatterhouse.toml" goto have_toml
echo Creando splatterhouse.toml por defecto...

rem Resolucion interna por GPU. En la ruta ROV (interlock) el coste escala con
rem los pixeles y en AMD/Intel es mucho mas caro que en NVIDIA (issue #3): en
rem combate con varios enemigos 1080p puede ir a pocos FPS. El juego es de 2010
rem y renderiza a 720p, asi que FSR lo devuelve a la resolucion de la ventana.
set "VIDEO_W=1920"
set "VIDEO_H=1080"
set "GPU="
for /f "usebackq delims=" %%G in (`powershell -NoProfile -Command "(Get-CimInstance Win32_VideoController | Where-Object { $_.AdapterRAM -gt 0 -and $_.Name -notmatch 'Basic|Virtual|Remote|Parsec|Sunshine|Meta' } | Sort-Object AdapterRAM -Descending | Select-Object -First 1 -ExpandProperty Name)" 2^>nul`) do set "GPU=%%G"
if not defined GPU goto have_gpu
echo   GPU detectada: %GPU%
echo %GPU% > "%TEMP%\sh_gpu.txt"
findstr /I /C:"AMD" /C:"Radeon" /C:"Intel" /C:"Arc" "%TEMP%\sh_gpu.txt" >nul
if errorlevel 1 goto have_gpu
set "VIDEO_W=1280"
set "VIDEO_H=720"
echo   AMD/Intel: resolucion interna 1280x720 + FSR (se puede subir en F5)
:have_gpu
echo # Splatterhouse Recompiled - configuracion> "%ROOT%splatterhouse.toml"
echo log_level = "warn">> "%ROOT%splatterhouse.toml"
echo log_file = "splatterhouse.log">> "%ROOT%splatterhouse.toml"
echo gpu_plugin = "xenos">> "%ROOT%splatterhouse.toml"
echo render_target_path_d3d12 = "rov">> "%ROOT%splatterhouse.toml"
echo present_effect = "fsr">> "%ROOT%splatterhouse.toml"
echo video_mode_width = %VIDEO_W%>> "%ROOT%splatterhouse.toml"
echo video_mode_height = %VIDEO_H%>> "%ROOT%splatterhouse.toml"

echo fullscreen = true>> "%ROOT%splatterhouse.toml"
echo vsync = false>> "%ROOT%splatterhouse.toml"
echo d3d12_present_frame_limiter = true>> "%ROOT%splatterhouse.toml"
echo d3d12_present_frame_limiter_fps = 60>> "%ROOT%splatterhouse.toml"
echo d3d12_allow_variable_refresh_rate_and_tearing = false>> "%ROOT%splatterhouse.toml"
echo d3d12_present_adaptive_tearing = true>> "%ROOT%splatterhouse.toml"
echo video_mode_refresh_rate = 60>> "%ROOT%splatterhouse.toml"
echo bind_graphics = "F5">> "%ROOT%splatterhouse.toml"
echo sh_language = "auto">> "%ROOT%splatterhouse.toml"
:have_toml

echo.
echo Extrayendo el juego...
echo   ISO    : %ISO%
echo   Destino: %ASSETS%
echo.

set "TMP=%TEMP%\splatterhouse_install"
if exist "%TMP%" rmdir /S /Q "%TMP%"
mkdir "%TMP%"

pushd "%TMP%"
"%TOOL%" -x -s "%ISO%"
set "RC=%ERRORLEVEL%"
popd
if not "%RC%"=="0" goto failed

rem El contenido queda en %TMP%\<nombre del iso> (o un nivel mas abajo si la
rem ISO trae una carpeta extra).
set "SRC="
for /d %%D in ("%TMP%\*") do if exist "%%D\default.xex" set "SRC=%%D"
if not defined SRC for /d %%D in ("%TMP%\*") do for /d %%E in ("%%D\*") do if exist "%%E\default.xex" set "SRC=%%E"
if not defined SRC goto failed

if exist "%ASSETS%" goto copy_into
move "%SRC%" "%ASSETS%" >nul
goto ok

:copy_into
xcopy /E /I /Y "%SRC%\*" "%ASSETS%" >nul
rmdir /S /Q "%SRC%"

:ok
rem Verificacion: sin la carpeta data el juego arranca a pantalla negra y
rem termina con un access violation (issue #4). Mejor avisar aqui.
if not exist "%ASSETS%\default.xex" goto bad_extract
if not exist "%ASSETS%\data" goto bad_extract
if not exist "%XEXTOOL%" goto skip_xex
if not exist "%ASSETS%\default.xex" goto skip_xex
echo.
echo Desencriptando el XEX a default.dec.xex...
"%XEXTOOL%" -e u -c b -o "%ASSETS%\default.dec.xex" "%ASSETS%\default.xex"
if errorlevel 1 echo   Aviso: xextool fallo; se intentara con el default.xex original.
:skip_xex

rem Cache de shaders precompilada incluida en el release (evita tirones al
rem compilar shaders la primera vez; ver docs/rov-performance.md).
if not exist "%ROOT%shadercache" goto skip_shaders
for /f "usebackq delims=" %%D in (`powershell -NoProfile -Command "[Environment]::GetFolderPath('MyDocuments')"`) do set "DOCS=%%D"
if "%DOCS%"=="" goto skip_shaders
set "SHCACHE=%DOCS%\splatterhouse\cache\shaders\shareable"
if not exist "%SHCACHE%" mkdir "%SHCACHE%"
echo Copiando cache de shaders precompilada...
xcopy /Y /I "%ROOT%shadercache\*" "%SHCACHE%" >nul
:skip_shaders

rmdir /S /Q "%TMP%"
echo.
echo Listo. Ejecuta splatterhouse.exe para jugar.
goto done

:no_iso
echo No se indico ninguna ISO.
goto done
:no_iso_file
echo No encuentro la ISO: %ISO%
goto done
:no_tool
echo No encuentro tools\extract-xiso.exe
goto done
:failed
echo.
echo La extraccion no se completo.
echo Comprueba que la ISO sea de Splatterhouse y que la ruta no tenga caracteres raros.
goto done

:bad_extract
echo.
echo La extraccion no dejo la estructura esperada (default.xex + carpeta data).
echo Comprueba que la ISO sea la de Splatterhouse de Xbox 360 y vuelve a intentarlo.
rmdir /S /Q "%TMP%" 2>nul

:done
echo.
pause

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
echo # Splatterhouse Recompiled - configuracion> "%ROOT%splatterhouse.toml"
echo log_level = "warn">> "%ROOT%splatterhouse.toml"
echo log_file = "splatterhouse.log">> "%ROOT%splatterhouse.toml"
echo gpu_plugin = "xenos">> "%ROOT%splatterhouse.toml"
echo render_target_path_d3d12 = "rov">> "%ROOT%splatterhouse.toml"
echo present_effect = "fsr">> "%ROOT%splatterhouse.toml"
echo video_mode_width = 1920>> "%ROOT%splatterhouse.toml"
echo video_mode_height = 1080>> "%ROOT%splatterhouse.toml"
echo fullscreen = true>> "%ROOT%splatterhouse.toml"
echo vsync = false>> "%ROOT%splatterhouse.toml"
echo d3d12_present_frame_limiter = true>> "%ROOT%splatterhouse.toml"
echo d3d12_present_frame_limiter_fps = 60>> "%ROOT%splatterhouse.toml"
echo d3d12_allow_variable_refresh_rate_and_tearing = false>> "%ROOT%splatterhouse.toml"
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

rem El contenido queda en %TMP%\<nombre del iso>
set "SRC="
for /d %%D in ("%TMP%\*") do if exist "%%D\default.xex" set "SRC=%%D"
if not defined SRC goto failed

if exist "%ASSETS%" goto copy_into
move "%SRC%" "%ASSETS%" >nul
goto ok

:copy_into
xcopy /E /I /Y "%SRC%\*" "%ASSETS%" >nul
rmdir /S /Q "%SRC%"

:ok
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

:done
echo.
pause

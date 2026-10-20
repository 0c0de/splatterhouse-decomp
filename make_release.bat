@echo off
rem ============================================================
rem  Splatterhouse Recompiled - Empaquetar release
rem  Crea "splatterhouse-decomp-release\" con todo lo necesario
rem  para que el usuario final lo use (arrastrar la ISO a install.bat).
rem ============================================================
setlocal enableextensions
title Splatterhouse Recompiled - Crear release

set "ROOT=%~dp0"
set "OUT=%ROOT%splatterhouse-decomp-release"
set "BUILD=%ROOT%out\build\win-amd64-relwithdebinfo"
set "SDK=%ROOT%extern\rexglue-sdk\out\win-amd64"

echo ===============================================
echo   Splatterhouse Recompiled - Crear release
echo ===============================================
echo   Origen build : %BUILD%
echo   Destino      : %OUT%
echo.

if not exist "%BUILD%\splatterhouse.exe" goto no_build
if not exist "%ROOT%install.bat" goto no_install

rem Limpiar release anterior.
if exist "%OUT%" rmdir /S /Q "%OUT%"
mkdir "%OUT%"
mkdir "%OUT%\tools"
mkdir "%OUT%\docs"
mkdir "%OUT%\assets"

echo [1/7] Copiando ejecutable...
copy /Y "%BUILD%\splatterhouse.exe" "%OUT%\splatterhouse.exe" >nul
if errorlevel 1 goto failed

echo [2/7] Copiando DLLs del runtime...
copy /Y "%BUILD%\rexruntimerd.dll"  "%OUT%\" >nul
if errorlevel 1 goto failed
copy /Y "%BUILD%\rexgpu-xenosrd.dll" "%OUT%\" >nul
if errorlevel 1 goto failed
copy /Y "%BUILD%\TracyClientrd.dll"  "%OUT%\" >nul
if errorlevel 1 goto failed
rem FidelityFX (necesaria si el runtime se compilo con REXGLUE_ENABLE_FIDELITYFX=ON).
if exist "%BUILD%\amd_fidelityfx_dx12drel.dll" copy /Y "%BUILD%\amd_fidelityfx_dx12drel.dll" "%OUT%\" >nul

echo [3/7] Copiando instalador y herramientas...
copy /Y "%ROOT%install.bat" "%OUT%\install.bat" >nul
copy /Y "%ROOT%tools\extract-xiso.exe" "%OUT%\tools\" >nul
if errorlevel 1 goto failed
copy /Y "%ROOT%tools\xextool.exe" "%OUT%\tools\" >nul
if errorlevel 1 goto failed

echo [4/7] Configuracion por defecto...
rem NO se copia el splatterhouse.toml del build: puede ser la config de
rem desarrollo (backend vulkan, log_level=info...). Lo crea install.bat en el
rem equipo del usuario, que ademas elige la resolucion interna segun la GPU.
if exist "%OUT%\splatterhouse.toml" del "%OUT%\splatterhouse.toml"
echo        se genera en la instalacion (install.bat)

echo [5/7] Copiando cache de shaders precompilada...
set "SHCACHE_SRC="
if exist "%ROOT%shadercache" set "SHCACHE_SRC=%ROOT%shadercache"
for /f "usebackq delims=" %%D in (`powershell -NoProfile -Command "[Environment]::GetFolderPath('MyDocuments')"`) do set "DOCS=%%D"
if not defined SHCACHE_SRC if exist "%DOCS%\splatterhouse\cache\shaders\shareable" set "SHCACHE_SRC=%DOCS%\splatterhouse\cache\shaders\shareable"
if not defined SHCACHE_SRC goto skip_shaders
mkdir "%OUT%\shadercache" 2>nul
xcopy /Y /I "%SHCACHE_SRC%\*" "%OUT%\shadercache\" >nul
echo        desde: %SHCACHE_SRC%
:skip_shaders

echo [6/7] Copiando documentacion...
if exist "%ROOT%README.md" copy /Y "%ROOT%README.md" "%OUT%\README.md" >nul
if exist "%ROOT%docs" xcopy /E /I /Y "%ROOT%docs\*" "%OUT%\docs\" >nul

echo [7/7] Creando LEEME...
> "%OUT%\LEEME.txt" echo Splatterhouse Recompiled - Port nativo (Xbox 360 a PC)
>>"%OUT%\LEEME.txt" echo =====================================================
>>"%OUT%\LEEME.txt" echo.
>>"%OUT%\LEEME.txt" echo USO:
>>"%OUT%\LEEME.txt" echo   1. Arrastra la ISO de Splatterhouse sobre install.bat
>>"%OUT%\LEEME.txt" echo   2. Espera a que extraiga el juego a "assets\"
>>"%OUT%\LEEME.txt" echo   3. Ejecuta splatterhouse.exe
>>"%OUT%\LEEME.txt" echo.
>>"%OUT%\LEEME.txt" echo OPCIONES EN JUEGO:
>>"%OUT%\LEEME.txt" echo   F5  - Menu de opciones (graficos, idioma, desbloquear niveles)
>>"%OUT%\LEEME.txt" echo   F3  - Overlay de debug (FPS)
>>"%OUT%\LEEME.txt" echo.
>>"%OUT%\LEEME.txt" echo DLC:
>>"%OUT%\LEEME.txt" echo   Crea una carpeta "dlc" junto a splatterhouse.exe y deja ahi
>>"%OUT%\LEEME.txt" echo   los paquetes DLC (CON/PIRS/LIVE). El port los instala solo.
>>"%OUT%\LEEME.txt" echo   Tambien vale: %%USERPROFILE%%\Documents\splatterhouse\4E4D07F0\00000002\
>>"%OUT%\LEEME.txt" echo.
>>"%OUT%\LEEME.txt" echo ULTRAWIDE:
>>"%OUT%\LEEME.txt" echo   F5 -^> Resolucion interna -^> 2560x1080 o 3440x1440 (el juego
>>"%OUT%\LEEME.txt" echo   renderiza nativo a 21:9; sin bandas ni estiramiento).
>>"%OUT%\LEEME.txt" echo.
>>"%OUT%\LEEME.txt" echo SI EL JUEGO SE CIERRA AL ARRANCAR:
>>"%OUT%\LEEME.txt" echo   Falta el contenido del juego. Mira assets_error.txt junto al
>>"%OUT%\LEEME.txt" echo   ejecutable (dice que carpeta ha comprobado y que falta).
>>"%OUT%\LEEME.txt" echo.
>>"%OUT%\LEEME.txt" echo AVISO LEGAL: necesitas tu propia copia legitima del juego.
>>"%OUT%\LEEME.txt" echo No se incluye ningun dato del juego en este paquete.
>>"%OUT%\LEEME.txt" echo.

echo.
echo ===============================================
echo   Release creado en:
echo   %OUT%
echo ===============================================
echo.
dir /B "%OUT%"
goto done

:no_build
echo ERROR: no existe "%BUILD%\splatterhouse.exe".
echo Compila primero (preset win-amd64-relwithdebinfo):
echo   cmake --preset win-amd64-relwithdebinfo
echo   cmake --build out\build\win-amd64-relwithdebinfo --target splatterhouse -j 6
goto done

:no_install
echo ERROR: no encuentro install.bat en la raiz.
goto done

:failed
echo ERROR: fallo copiando ficheros.

:done
echo.
pause

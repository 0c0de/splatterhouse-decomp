@echo off
rem Empaqueta la cache de shaders del SDK (generada al jugar) en "shadercache\"
rem del release, para distribuirla con install.bat.
rem Uso: juega una vez (recorre niveles/menus/cinematicas) y luego ejecuta este script.
setlocal enableextensions
title Splatterhouse Recompiled - Empaquetar cache de shaders

set "ROOT=%~dp0.."
for /f "usebackq delims=" %%D in (`powershell -NoProfile -Command "[Environment]::GetFolderPath('MyDocuments')"`) do set "DOCS=%%D"
set "SRC=%DOCS%\splatterhouse\cache\shaders\shareable"
set "DST=%ROOT%\shadercache"

echo ===============================================
echo   Empaquetar cache de shaders
echo ===============================================
echo.
echo   Origen : %SRC%
echo   Destino: %DST%
echo.

if not exist "%SRC%" goto no_src
if not exist "%DST%" mkdir "%DST%"

echo Copiando...
xcopy /Y /I "%SRC%\*" "%DST%" >nul
if errorlevel 1 goto failed

echo.
echo Listo. La carpeta "shadercache" ya se puede distribuir en el release.
goto done

:no_src
echo No existe la cache de shaders. Juega una vez antes de empaquetar.
goto done

:failed
echo Error copiando la cache.

:done
echo.
pause

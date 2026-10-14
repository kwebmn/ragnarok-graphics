@echo off
rem uaRO Graphics: puts our ddraw.dll into this uaRO client folder.
rem dgVoodoo's DDraw.dll and D3DImm.dll move into the dgvoodoo folder: uaRO Graphics passes the game to them.
rem Running it again is safe. uninstall.bat puts everything back.
setlocal
cd /d "%~dp0"
if not exist "uaRO.exe" (
    echo uaRO.exe is not in this folder.
    echo Unpack the whole archive into the uaRO client folder, next to uaRO.exe, and run install.bat again.
    goto :fail
)
if not exist "uaro-graphics\ddraw.dll" (
    echo uaro-graphics\ddraw.dll is missing: unpack the whole archive, keeping its folders.
    goto :fail
)
tasklist /fi "imagename eq uaro-graphics.exe" 2>nul | find /i "uaro-graphics.exe" >nul && (
    echo uaRO Graphics Settings is open: close it and run install.bat again.
    goto :fail
)
rem A client under Program Files can only be changed with administrator rights.
(echo test) > "uaro-graphics-write-test.tmp" 2>nul || goto :noaccess
del "uaro-graphics-write-test.tmp"
if not exist "dgvoodoo" mkdir "dgvoodoo" || goto :noaccess
rem The ddraw.dll in this folder is dgVoodoo's unless it is already ours.
if exist "ddraw.dll" (
    findstr /m /c:"uaRO Graphics" "ddraw.dll" >nul 2>&1
    if errorlevel 1 move /y "ddraw.dll" "dgvoodoo\DDraw.dll" >nul || goto :noaccess
)
if exist "D3DImm.dll" move /y "D3DImm.dll" "dgvoodoo\D3DImm.dll" >nul || goto :noaccess
if exist "dgVoodoo.conf" copy /y "dgVoodoo.conf" "dgvoodoo\dgVoodoo.conf" >nul
copy /y "uaro-graphics\ddraw.dll" "ddraw.dll" >nul || goto :noaccess
rem The settings app, uaRO Graphics Settings, lives next to uaRO.exe.
if exist "uaro-graphics\uaro-graphics.exe" copy /y "uaro-graphics\uaro-graphics.exe" "uaro-graphics.exe" >nul || goto :noaccess
rem A settings file older than 0.3 holds the defaults and the renderer choice: it is replaced by the new one,
rem with every new setting and its notes, and keeps that choice.
if exist "uaro-graphics.ini" (
    findstr /c:"high_resolution" "uaro-graphics.ini" >nul 2>&1
    if errorlevel 1 (
        set "uaro="
        findstr /i /r /c:"^ *backend *= *uaro" "uaro-graphics.ini" >nul 2>&1 && set "uaro=1"
        copy /y "uaro-graphics\uaro-graphics.ini" "uaro-graphics.ini" >nul
        if defined uaro powershell -NoProfile -ExecutionPolicy Bypass -Command "$f='uaro-graphics.ini'; (Get-Content -LiteralPath $f) -replace '^backend = dgvoodoo$','backend = uaro' | Set-Content -LiteralPath $f -Encoding ASCII"
    )
)
if not exist "uaro-graphics.ini" copy "uaro-graphics\uaro-graphics.ini" "uaro-graphics.ini" >nul
rem A 0.3 settings file has no sharpen key yet: it is added at the end, every other setting stays.
findstr /c:"sharpen" "uaro-graphics.ini" >nul 2>&1
if errorlevel 1 (
    >>"uaro-graphics.ini" echo.
    >>"uaro-graphics.ini" echo [quality]
    >>"uaro-graphics.ini" echo ; sharpening of the picture on screen, 0-100: 0 = off, more = crisper detail
    >>"uaro-graphics.ini" echo sharpen = 50
    >>"uaro-graphics.ini" echo ; in the game, Pause or Scroll Lock switches all improvements off and on, to compare
)
rem A settings file older than 0.5 has no crisp_text key: it is added at the end the same way.
findstr /c:"crisp_text" "uaro-graphics.ini" >nul 2>&1
if errorlevel 1 (
    >>"uaro-graphics.ini" echo.
    >>"uaro-graphics.ini" echo [display]
    >>"uaro-graphics.ini" echo ; 1 = the game's text, window frames and fills at the real pixels of the window when the game is enlarged; 0 = as version 0.4
    >>"uaro-graphics.ini" echo crisp_text = 1
)
rem A settings file older than 0.7 has no msaa key: the smoothing settings are added at the end the same way.
findstr /c:"msaa" "uaro-graphics.ini" >nul 2>&1
if errorlevel 1 (
    >>"uaro-graphics.ini" echo.
    >>"uaro-graphics.ini" echo [quality]
    >>"uaro-graphics.ini" echo ; edge smoothing (MSAA): samples per pixel - 0 = off, 2, 4 or 8
    >>"uaro-graphics.ini" echo msaa = 4
    >>"uaro-graphics.ini" echo ; 1 = with msaa, soft edges where textures are cut out (leaves, fences)
    >>"uaro-graphics.ini" echo alpha_to_coverage = 1
    >>"uaro-graphics.ini" echo ; anisotropic filtering of the world: 0 = off, 2, 4, 8 or 16
    >>"uaro-graphics.ini" echo anisotropy = 16
)
rem update.bat calls install.bat /quiet and reports the result itself.
if /i "%~1"=="/quiet" exit /b 0
echo.
echo uaRO Graphics is installed. Start the game as usual.
echo Every setting is in uaro-graphics.exe (uaRO Graphics Settings) in this folder.
echo To draw with the uaRO Graphics renderer (Direct3D 12), choose it there or run use-uaro.bat; use-dgvoodoo.bat switches back.
echo The log is uaro-graphics.log in this folder. To undo everything, run uninstall.bat.
echo.
pause
exit /b 0

:noaccess
echo.
echo Cannot write to this folder. Right-click install.bat and choose "Run as administrator".
:fail
echo.
if /i not "%~1"=="/quiet" pause
exit /b 1

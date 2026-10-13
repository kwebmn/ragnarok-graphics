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
rem The settings file of version 0.1 holds only defaults; 0.2 changes some (mode = auto), so it is replaced.
if exist "uaro-graphics.ini" (
    findstr /c:"not in version 0.1 yet" "uaro-graphics.ini" >nul 2>&1
    if not errorlevel 1 copy /y "uaro-graphics\uaro-graphics.ini" "uaro-graphics.ini" >nul
)
if not exist "uaro-graphics.ini" copy "uaro-graphics\uaro-graphics.ini" "uaro-graphics.ini" >nul
echo.
echo uaRO Graphics is installed. Start the game as usual.
echo To draw with the uaRO Graphics renderer (Direct3D 12), run use-uaro.bat; use-dgvoodoo.bat switches back.
echo The log is uaro-graphics.log in this folder. To undo everything, run uninstall.bat.
echo.
pause
exit /b 0

:noaccess
echo.
echo Cannot write to this folder. Right-click install.bat and choose "Run as administrator".
:fail
echo.
pause
exit /b 1

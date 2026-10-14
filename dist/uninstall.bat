@echo off
rem uaRO Graphics: removes our ddraw.dll and puts dgVoodoo back where it was.
setlocal
cd /d "%~dp0"
tasklist /fi "imagename eq uaro-graphics.exe" 2>nul | find /i "uaro-graphics.exe" >nul && (
    echo uaRO Graphics Settings is open: close it and run uninstall.bat again.
    echo.
    pause
    exit /b 1
)
(echo test) > "uaro-graphics-write-test.tmp" 2>nul || goto :noaccess
del "uaro-graphics-write-test.tmp"
if exist "ddraw.dll" (
    findstr /m /c:"uaRO Graphics" "ddraw.dll" >nul 2>&1
    if not errorlevel 1 del /f "ddraw.dll"
)
if exist "dgvoodoo\DDraw.dll" move /y "dgvoodoo\DDraw.dll" "DDraw.dll" >nul || goto :noaccess
if exist "dgvoodoo\D3DImm.dll" move /y "dgvoodoo\D3DImm.dll" "D3DImm.dll" >nul || goto :noaccess
if exist "dgvoodoo\dgVoodoo.conf" del /f "dgvoodoo\dgVoodoo.conf"
if exist "uaro-graphics.exe" del /f "uaro-graphics.exe"
rem The list of pipelines met in the game is a cache of uaRO Graphics: it goes too.
if exist "uaro-graphics-pipelines.txt" del /f "uaro-graphics-pipelines.txt"
rmdir "dgvoodoo" 2>nul
echo.
echo uaRO Graphics is removed and dgVoodoo is back in place.
echo uaro-graphics.ini and uaro-graphics.log stay here; delete them if you like.
echo The uaro-graphics-backup folder keeps the copies of your settings the settings app made.
echo upscaled.grf keeps the AI textures made so far; delete it if you like.
echo.
pause
exit /b 0

:noaccess
echo.
echo Cannot write to this folder. Right-click uninstall.bat and choose "Run as administrator".
echo.
pause
exit /b 1

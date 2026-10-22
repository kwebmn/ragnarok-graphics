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
rem 2.8: dgVoodoo back from uaro-graphics\dgvoodoo (2.x kept it in dgvoodoo in the root)
for %%d in ("uaro-graphics\dgvoodoo" "dgvoodoo") do (
    if exist "%%~d\DDraw.dll" move /y "%%~d\DDraw.dll" "DDraw.dll" >nul || goto :noaccess
    if exist "%%~d\D3DImm.dll" move /y "%%~d\D3DImm.dll" "D3DImm.dll" >nul || goto :noaccess
    if exist "%%~d\dgVoodoo.conf" del /f "%%~d\dgVoodoo.conf"
)
if exist "uaro-graphics.exe" del /f "uaro-graphics.exe"
rem The list of pipelines met in the game is a cache of uaRO Graphics: it goes too.
if exist "uaro-graphics\cache" rmdir /s /q "uaro-graphics\cache"
if exist "uaro-graphics-pipelines.txt" del /f "uaro-graphics-pipelines.txt"
rem The shaders compiled at the first start and kept for the next ones are a cache too.
if exist "uaro-graphics\shaders-32.bin" del /f "uaro-graphics\shaders-32.bin"
if exist "uaro-graphics\shaders-64.bin" del /f "uaro-graphics\shaders-64.bin"
if exist "uaro-graphics\shaders-d3d11.bin" del /f "uaro-graphics\shaders-d3d11.bin"
rem 0.49.1: the pictures the texture pack had none of, saved for the developers.
if exist "uaro-graphics\missing" rmdir /s /q "uaro-graphics\missing"
if exist "uaro-graphics\crash.dmp" del /f "uaro-graphics\crash.dmp"
if exist "uaro-graphics\reports.txt" del /f "uaro-graphics\reports.txt"
rem The texture pack builder's status for the settings app goes too.
if exist "uaro-graphics\pack-status.txt" del /f "uaro-graphics\pack-status.txt"
if exist "uaro-graphics\pack-status.txt.part" del /f "uaro-graphics\pack-status.txt.part"
if exist "uaro-graphics\pack-status.txt.stop" del /f "uaro-graphics\pack-status.txt.stop"
rmdir "uaro-graphics\dgvoodoo" 2>nul
rmdir "dgvoodoo" 2>nul
echo.
echo uaRO Graphics is removed and dgVoodoo is back in place.
echo The uaro-graphics folder keeps your settings (uaro-graphics.ini), the logs (logs), the copies of your
echo settings the settings app made (backup) and the texture pack (textures\uaro-textures.zip); delete it if
echo you like. upscaled.grf of the versions before 0.48 is not used any more; delete it.
echo.
pause
exit /b 0

:noaccess
echo.
echo Cannot write to this folder. Right-click uninstall.bat and choose "Run as administrator".
echo.
pause
exit /b 1

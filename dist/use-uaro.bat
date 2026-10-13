@echo off
rem uaRO Graphics: the game draws with the uaRO Graphics renderer (Direct3D 12). use-dgvoodoo.bat switches back.
setlocal
cd /d "%~dp0"
if not exist "uaro-graphics.ini" (
    echo uaro-graphics.ini is not in this folder: run install.bat first.
    goto :fail
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$f='uaro-graphics.ini'; $t=@(Get-Content -LiteralPath $f); if ($t -match '^\s*backend\s*=') { $t = $t -replace '^\s*backend\s*=.*$','backend = uaro' } else { $t += '[renderer]'; $t += 'backend = uaro' }; Set-Content -LiteralPath $f -Value $t -Encoding ASCII"
if errorlevel 1 goto :noaccess
echo.
echo The game now draws with uaRO Graphics (Direct3D 12). Start it as usual.
echo If something looks wrong: send uaro-graphics.log and run use-dgvoodoo.bat to go back.
echo.
pause
exit /b 0

:noaccess
echo.
echo Cannot change uaro-graphics.ini. Right-click use-uaro.bat and choose "Run as administrator".
:fail
echo.
pause
exit /b 1

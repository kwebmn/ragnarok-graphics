@echo off
rem uaRO Graphics: the game draws with dgVoodoo again. use-uaro.bat switches to the uaRO Graphics renderer.
setlocal
cd /d "%~dp0"
if not exist "uaro-graphics.ini" (
    echo uaro-graphics.ini is not in this folder: run install.bat first.
    goto :fail
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$f='uaro-graphics.ini'; $t=@(Get-Content -LiteralPath $f); if ($t -match '^\s*backend\s*=') { $t = $t -replace '^\s*backend\s*=.*$','backend = dgvoodoo' } else { $t += '[renderer]'; $t += 'backend = dgvoodoo' }; Set-Content -LiteralPath $f -Value $t -Encoding ASCII"
if errorlevel 1 goto :noaccess
echo.
echo The game draws with dgVoodoo again. Start it as usual.
echo.
pause
exit /b 0

:noaccess
echo.
echo Cannot change uaro-graphics.ini. Right-click use-dgvoodoo.bat and choose "Run as administrator".
:fail
echo.
pause
exit /b 1

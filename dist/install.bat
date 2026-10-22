@echo off
rem uaRO Graphics: the settings app installs, updates and removes itself (2.10). An older update.bat (2.x) runs this
rem file once, after it put the new files into uaro-graphics\; uaro-graphics.exe --install finishes the install and
rem its next start takes the .bat files away. Unpacked by hand: run uaro-graphics-setup.exe instead, or this file.
cd /d "%~dp0"
if not exist "uaRO.exe" exit /b 1
if not exist "uaro-graphics\uaro-graphics.exe" exit /b 1
rem An update.bat of 0.11.0 or older copied only the files of uaro-graphics\, not its folders (ai, remix).
if exist "uaro-graphics-update\files\uaro-graphics\ddraw.dll" xcopy /e /i /y /q "uaro-graphics-update\files\uaro-graphics" "uaro-graphics" >nul
start "" /wait "uaro-graphics\uaro-graphics.exe" --install %1
exit /b %errorlevel%

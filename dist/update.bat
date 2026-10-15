@echo off
rem uaRO Graphics: downloads the newest build from GitHub and installs it into this uaRO client folder.
rem The renderer choice and uaro-graphics.ini stay. If anything fails, nothing in this folder is changed.
setlocal
cd /d "%~dp0"
set "url=https://github.com/kwebmn/ragnarok-graphics/raw/main/release/uaro-graphics.zip"
set "raw=https://raw.githubusercontent.com/kwebmn/ragnarok-graphics"
set "api=https://api.github.com/repos/kwebmn/ragnarok-graphics/commits/main"
set "accept=Accept: application/vnd.github.sha"
set "work=uaro-graphics-update"
if not exist "uaRO.exe" (
    echo uaRO.exe is not in this folder: update.bat belongs in the uaRO client folder, next to uaRO.exe.
    goto :fail
)
tasklist /fi "imagename eq uaRO.exe" 2>nul | find /i "uaRO.exe" >nul && (
    echo The game is running: close it and run update.bat again.
    goto :fail
)
rem The settings app may have started this update: give it up to 10 seconds to close, its exe is replaced too.
set /a waited=0
:waitapp
tasklist /fi "imagename eq uaro-graphics.exe" 2>nul | find /i "uaro-graphics.exe" >nul || goto :appclosed
if %waited% geq 10 (
    echo uaRO Graphics Settings is open: close it and run update.bat again.
    goto :fail
)
timeout /t 1 /nobreak >nul
set /a waited+=1
goto :waitapp
:appclosed
rem 0.45.2: the texture pack being made keeps uaro-graphics\uaro-render64.exe open - it would not be replaced.
tasklist /fi "imagename eq uaro-render64.exe" 2>nul | find /i "uaro-render64.exe" >nul && (
    echo uaro-render64.exe is running - the texture pack is being made, or the game left it behind.
    echo Stop it on the Texture pack page of the settings or in the Task Manager, then run update.bat again.
    goto :fail
)
where curl >nul 2>&1 || goto :oldwindows
where tar >nul 2>&1 || goto :oldwindows
rem A client under Program Files can only be changed with administrator rights.
(echo test) > "uaro-graphics-write-test.tmp" 2>nul || goto :noaccess
del "uaro-graphics-write-test.tmp"
set "was=unknown"
if exist "uaro-graphics\version.txt" set /p was=<"uaro-graphics\version.txt"
rem Download and unpack into a folder of their own first: a failed or broken download changes nothing else.
if exist "%work%" rmdir /s /q "%work%"
mkdir "%work%\files" || goto :noaccess
echo Downloading the newest uaRO Graphics...
rem The usual link can show a release up to 5 minutes late, from GitHub's cache (0.11.2): the archive is taken
rem by the number of the newest commit, which no cache keeps old - only hex digits are taken for it. Without an
rem answer that way, the usual link.
set "fresh="
set "got="
for /f "usebackq delims=" %%s in (`curl -fsSL -H "%accept%" "%api%" 2^>nul ^| findstr /x /r "[0-9a-f]*"`) do set "fresh=%%s"
if defined fresh curl -fsSL -o "%work%\uaro-graphics.zip" "%raw%/%fresh%/release/uaro-graphics.zip" 2>nul && set "got=1"
if not defined got curl -fsSL -o "%work%\uaro-graphics.zip" "%url%" || (
    echo Cannot download %url%
    echo Check the internet connection and try again later.
    goto :fail
)
tar -xf "%work%\uaro-graphics.zip" -C "%work%\files" 2>nul || (
    echo The downloaded archive is broken. Try again later.
    goto :fail
)
if not exist "%work%\files\uaro-graphics\ddraw.dll" (
    echo The downloaded archive is not a uaRO Graphics package.
    goto :fail
)
set "now=unknown"
if exist "%work%\files\uaro-graphics\version.txt" set /p now=<"%work%\files\uaro-graphics\version.txt"
rem Every file but this script, which replaces itself last; then the same steps as install.bat.
if not exist "uaro-graphics" mkdir "uaro-graphics" || goto :noaccess
rem With its folders: the AI model (ai) and RTX Remix's settings (remix) - 0.11.0 and older left them out.
xcopy /e /i /y /q "%work%\files\uaro-graphics" "uaro-graphics" >nul || goto :noaccess
for %%f in ("%work%\files\*") do (
    if /i not "%%~nxf"=="update.bat" copy /y "%%~f" . >nul || goto :noaccess
)
call install.bat /quiet || goto :fail
rem cmd reads this whole block before running it, so replacing this file inside it is safe.
(
    if exist "%work%\files\update.bat" copy /y "%work%\files\update.bat" "%~f0" >nul
    rmdir /s /q "%work%"
    echo.
    echo uaRO Graphics %now% is installed, it was %was%. Start the game as usual.
    echo.
    pause
    exit /b 0
)

:oldwindows
echo This Windows has no curl or tar: they come with Windows 10 version 1803 and newer.
echo Download %url%
echo yourself, unpack it into this folder and run install.bat.
goto :fail

:noaccess
echo.
echo Cannot write to this folder. Right-click update.bat and choose "Run as administrator".
:fail
if exist "%work%" rmdir /s /q "%work%"
echo.
pause
exit /b 1

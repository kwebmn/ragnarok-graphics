@echo off
rem uaRO Graphics: NVIDIA DLSS (GeForce RTX cards) comes in a package of its own - its DLLs are large and seldom
rem change. install.bat runs this with /quiet: it downloads the package when the one here is not the one this build
rem expects (uaro-graphics\dlss-version.txt), and only on a computer with an NVIDIA graphics card.
setlocal
cd /d "%~dp0"
set "url=https://github.com/kwebmn/ragnarok-graphics/raw/main/release/uaro-graphics-dlss.zip"
set "work=uaro-graphics-dlss-download"
set "want="
set "have="
if exist "uaro-graphics\dlss-version.txt" set /p want=<"uaro-graphics\dlss-version.txt"
if exist "uaro-graphics\dlss\version.txt" set /p have=<"uaro-graphics\dlss\version.txt"
if not defined want goto :done
if "%want%"=="%have%" goto :done
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    "if ((Get-CimInstance Win32_VideoController).Name -match 'NVIDIA') { exit 0 } else { exit 1 }" >nul 2>&1 || (
    if /i not "%~1"=="/quiet" echo There is no NVIDIA graphics card here: DLSS is not needed.
    goto :done
)
where curl >nul 2>&1 || goto :done
where tar >nul 2>&1 || goto :done
echo Downloading NVIDIA DLSS for uaRO Graphics (34 MB)...
if exist "%work%" rmdir /s /q "%work%"
mkdir "%work%\files" || goto :failed
rem 1.5.11: as update.bat - where the certificate's revocation cannot be checked, best effort, or none
curl -fsSL -o "%work%\dlss.zip" "%url%" 2>nul || curl -fsSL --ssl-revoke-best-effort -o "%work%\dlss.zip" "%url%" 2>nul ^
    || curl -fsSL --ssl-no-revoke -o "%work%\dlss.zip" "%url%" || goto :failed
tar -xf "%work%\dlss.zip" -C "%work%\files" 2>nul || goto :failed
if not exist "%work%\files\uaro-graphics\dlss\sl.interposer.dll" goto :failed
if not exist "uaro-graphics\dlss" mkdir "uaro-graphics\dlss" || goto :failed
copy /y "%work%\files\uaro-graphics\dlss\*" "uaro-graphics\dlss\" >nul || goto :failed
rmdir /s /q "%work%"
echo NVIDIA DLSS is installed.
goto :done

:failed
if exist "%work%" rmdir /s /q "%work%"
echo NVIDIA DLSS could not be downloaded: everything else works and DLSS stays off. Run get-dlss.bat later.
:done
if /i not "%~1"=="/quiet" pause
exit /b 0

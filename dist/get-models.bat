@echo off
rem uaRO Graphics: the AI models that come apart from the package (2.5.27) - Game AI 2.0 (29 MB), wanted only for
rem making pictures (the texture pack, [ai] generate). The AI textures page starts this when a group's model is Game
rem AI and its file is not here. It downloads the models' package when the one here is not the one this build
rem expects (uaro-graphics\models-version.txt).
setlocal
cd /d "%~dp0"
set "url=https://github.com/kwebmn/ragnarok-graphics/raw/main/release/uaro-graphics-models.zip"
set "work=uaro-graphics-models-download"
set "want="
set "have="
if exist "uaro-graphics\models-version.txt" set /p want=<"uaro-graphics\models-version.txt"
if exist "uaro-graphics\ai\models-version.txt" set /p have=<"uaro-graphics\ai\models-version.txt"
if not defined want set "want=?"
if "%want%"=="%have%" if exist "uaro-graphics\ai\4x-gameai-2.0.bin" (
    echo The AI models are here already.
    goto :done
)
where curl >nul 2>&1 || goto :notools
where tar >nul 2>&1 || goto :notools
echo Downloading the AI models for uaRO Graphics (29 MB)...
if exist "%work%" rmdir /s /q "%work%"
mkdir "%work%\files" || goto :failed
rem as update.bat - where the certificate's revocation cannot be checked, best effort, or none
curl -fsSL -o "%work%\models.zip" "%url%" 2>nul || curl -fsSL --ssl-revoke-best-effort -o "%work%\models.zip" "%url%" 2>nul ^
    || curl -fsSL --ssl-no-revoke -o "%work%\models.zip" "%url%" || goto :failed
tar -xf "%work%\models.zip" -C "%work%\files" 2>nul || goto :failed
if not exist "%work%\files\uaro-graphics\ai\4x-gameai-2.0.bin" goto :failed
if not exist "uaro-graphics\ai" mkdir "uaro-graphics\ai" || goto :failed
copy /y "%work%\files\uaro-graphics\ai\*" "uaro-graphics\ai\" >nul || goto :failed
rmdir /s /q "%work%"
echo The AI models are installed.
goto :done

:notools
echo There is no curl or tar here (Windows 10 1803 and later have them): the AI models cannot be downloaded.
goto :done

:failed
if exist "%work%" rmdir /s /q "%work%"
echo The AI models could not be downloaded. Run get-models.bat again later.
:done
if /i not "%~1"=="/quiet" pause

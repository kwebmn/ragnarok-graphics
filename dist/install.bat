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
rem An update.bat of 0.11.0 or older copies only the files of uaro-graphics, not its folders (the AI model in ai,
rem RTX Remix's settings in remix), and then runs this new install.bat: the update it unpacked is still here.
if /i "%~1"=="/quiet" if exist "uaro-graphics-update\files\uaro-graphics\ddraw.dll" (
    xcopy /e /i /y /q "uaro-graphics-update\files\uaro-graphics" "uaro-graphics" >nul || goto :noaccess
)
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
rem A settings file older than 0.7 has no msaa key: the smoothing settings are added at the end the same way, as the
rem preset the file holds has them - Speed (no sharpening, no crisp text) and No improvements (also no high resolution)
rem get less, so an update never makes a weak card work harder (0.7.0 review).
findstr /c:"msaa" "uaro-graphics.ini" >nul 2>&1
if not errorlevel 1 goto :smoothing_done
set "msaa=4"
set "a2c=1"
set "aniso=16"
findstr /i /r /c:"^ *sharpen *= *0 *$" /c:"^ *sharpen *= *0[^0-9]" "uaro-graphics.ini" >nul 2>&1 ^
    || goto :smoothing_write
findstr /i /r /c:"^ *crisp_text *= *0 *$" /c:"^ *crisp_text *= *0[^0-9]" "uaro-graphics.ini" >nul 2>&1 ^
    || goto :smoothing_write
set "msaa=0"
set "aniso=4"
findstr /i /r /c:"^ *high_resolution *= *0 *$" /c:"^ *high_resolution *= *0[^0-9]" "uaro-graphics.ini" >nul 2>&1 ^
    || goto :smoothing_write
set "a2c=0"
set "aniso=0"
:smoothing_write
>>"uaro-graphics.ini" echo.
>>"uaro-graphics.ini" echo [quality]
>>"uaro-graphics.ini" echo ; edge smoothing (MSAA): samples per pixel - 0 = off, 2, 4 or 8
>>"uaro-graphics.ini" echo msaa = %msaa%
>>"uaro-graphics.ini" echo ; 1 = with msaa, soft edges where textures are cut out (leaves, fences)
>>"uaro-graphics.ini" echo alpha_to_coverage = %a2c%
>>"uaro-graphics.ini" echo ; anisotropic filtering of the world: 0 = off, 2, 4, 8 or 16
>>"uaro-graphics.ini" echo anisotropy = %aniso%
:smoothing_done
rem 0.17: the AI textures take textures of 16 pixels and more, not 64 - shadows and a map's small things. The settings
rem file 0.12-0.16 put in place says min_size = 64 under that version's note: both change, once.
findstr /x /c:"; the textures it takes, by their sides in pixels: from min_size to max_size, 16-1024" "uaro-graphics.ini" >nul 2>&1 && findstr /x /c:"min_size = 64" "uaro-graphics.ini" >nul 2>&1 && powershell -NoProfile -ExecutionPolicy Bypass -Command "$f='uaro-graphics.ini'; (Get-Content -LiteralPath $f) -replace '^min_size = 64$','min_size = 16' -replace '^; the textures it takes, by their sides in pixels: from min_size to max_size, 16-1024$','; the textures of the world it takes, by their sides in pixels (16 since 0.17)' | Set-Content -LiteralPath $f -Encoding ASCII"
rem 0.18-0.22: the effects page's keys, off, with their notes, at the end of a settings file from before they came
rem - all of them, or 0.19's water after the [effects] 0.18 added.
findstr /i /l /c:"[effects]" "uaro-graphics.ini" >nul 2>&1
if errorlevel 1 (
    >>"uaro-graphics.ini" echo.
    >>"uaro-graphics.ini" echo [effects]
    >>"uaro-graphics.ini" echo ; modern effects over the game's picture with the uaro renderer, also in a running game
    >>"uaro-graphics.ini" echo ; 1 = the light of skills, portals and lamps shines with a soft glow - bloom;
    >>"uaro-graphics.ini" echo ;     the rest of the picture keeps its brightness
    >>"uaro-graphics.ini" echo glow = 0
    >>"uaro-graphics.ini" echo ; how bright the glow is, 0-200 %%; how far it spreads, 0-100 %%
    >>"uaro-graphics.ini" echo glow_strength = 100
    >>"uaro-graphics.ini" echo glow_size = 50
    >>"uaro-graphics.ini" echo ; 0-100 %%: how much the effects' light falls on what is around them
    >>"uaro-graphics.ini" echo glow_light = 50
    >>"uaro-graphics.ini" echo ; 1 = soft shadows where surfaces meet: at walls, in corners, under trees
    >>"uaro-graphics.ini" echo ao = 0
    >>"uaro-graphics.ini" echo ; 0-100 %%: how dark the shadows are, how far they reach from a corner
    >>"uaro-graphics.ini" echo ao_strength = 60
    >>"uaro-graphics.ini" echo ao_size = 50
    >>"uaro-graphics.ini" echo ; 1 = the maps' water drawn anew: waves, reflections, the sun's glints,
    >>"uaro-graphics.ini" echo ;     the bed seen through it, foam
    >>"uaro-graphics.ini" echo water = 0
    >>"uaro-graphics.ini" echo ; 0-100 %%: how rough it is - 0 = a still mirror, how much it reflects,
    >>"uaro-graphics.ini" echo ;     how deep one sees into it
    >>"uaro-graphics.ini" echo water_waves = 50
    >>"uaro-graphics.ini" echo water_reflections = 50
    >>"uaro-graphics.ini" echo water_clarity = 50
    >>"uaro-graphics.ini" echo ; 1 = the picture's colours to taste: saturation 0-200 %%, contrast and brightness
    >>"uaro-graphics.ini" echo ;     50-150 %% - 100 as the game, warmth 0-100 %% - 50 as the game, vignette 0-100 %%
    >>"uaro-graphics.ini" echo grade = 0
    >>"uaro-graphics.ini" echo grade_saturation = 115
    >>"uaro-graphics.ini" echo grade_contrast = 108
    >>"uaro-graphics.ini" echo grade_brightness = 100
    >>"uaro-graphics.ini" echo grade_warmth = 50
    >>"uaro-graphics.ini" echo grade_vignette = 25
    >>"uaro-graphics.ini" echo ; 1 = depth of field: sharp around your character, the farther and the nearer softer;
    >>"uaro-graphics.ini" echo ;     0-100 %%: how soft, how deep the sharp zone is
    >>"uaro-graphics.ini" echo dof = 0
    >>"uaro-graphics.ini" echo dof_strength = 60
    >>"uaro-graphics.ini" echo dof_range = 40
)
findstr /i /l /c:"water_waves" "uaro-graphics.ini" >nul 2>&1
if errorlevel 1 (
    >>"uaro-graphics.ini" echo.
    >>"uaro-graphics.ini" echo [effects]
    >>"uaro-graphics.ini" echo ; 1 = the maps' water drawn anew: waves, reflections, the sun's glints,
    >>"uaro-graphics.ini" echo ;     the bed seen through it, foam
    >>"uaro-graphics.ini" echo water = 0
    >>"uaro-graphics.ini" echo ; 0-100 %%: how rough it is - 0 = a still mirror, how much it reflects,
    >>"uaro-graphics.ini" echo ;     how deep one sees into it
    >>"uaro-graphics.ini" echo water_waves = 50
    >>"uaro-graphics.ini" echo water_reflections = 50
    >>"uaro-graphics.ini" echo water_clarity = 50
)
rem 0.20: the soft shadows and the glow's light, after the [effects] 0.18 and 0.19 added.
findstr /i /l /c:"ao_strength" "uaro-graphics.ini" >nul 2>&1
if errorlevel 1 (
    >>"uaro-graphics.ini" echo.
    >>"uaro-graphics.ini" echo [effects]
    >>"uaro-graphics.ini" echo ; 1 = soft shadows where surfaces meet: at walls, in corners, under trees
    >>"uaro-graphics.ini" echo ao = 0
    >>"uaro-graphics.ini" echo ; 0-100 %%: how dark the shadows are, how far they reach from a corner
    >>"uaro-graphics.ini" echo ao_strength = 60
    >>"uaro-graphics.ini" echo ao_size = 50
    >>"uaro-graphics.ini" echo ; 0-100 %%: how much the effects' light falls on what is around them
    >>"uaro-graphics.ini" echo glow_light = 50
)
rem 0.21: the colours, after the [effects] 0.18-0.20 added.
findstr /i /l /c:"grade_saturation" "uaro-graphics.ini" >nul 2>&1
if errorlevel 1 (
    >>"uaro-graphics.ini" echo.
    >>"uaro-graphics.ini" echo [effects]
    >>"uaro-graphics.ini" echo ; 1 = the picture's colours to taste: saturation 0-200 %%, contrast and brightness
    >>"uaro-graphics.ini" echo ;     50-150 %% - 100 as the game, warmth 0-100 %% - 50 as the game, vignette 0-100 %%
    >>"uaro-graphics.ini" echo grade = 0
    >>"uaro-graphics.ini" echo grade_saturation = 115
    >>"uaro-graphics.ini" echo grade_contrast = 108
    >>"uaro-graphics.ini" echo grade_brightness = 100
    >>"uaro-graphics.ini" echo grade_warmth = 50
    >>"uaro-graphics.ini" echo grade_vignette = 25
)
rem 0.22: the depth of field, after the [effects] 0.18-0.21 added.
findstr /i /l /c:"dof_strength" "uaro-graphics.ini" >nul 2>&1
if errorlevel 1 (
    >>"uaro-graphics.ini" echo.
    >>"uaro-graphics.ini" echo [effects]
    >>"uaro-graphics.ini" echo ; 1 = depth of field: sharp around your character, the farther and the nearer softer;
    >>"uaro-graphics.ini" echo ;     0-100 %%: how soft, how deep the sharp zone is
    >>"uaro-graphics.ini" echo dof = 0
    >>"uaro-graphics.ini" echo dof_strength = 60
    >>"uaro-graphics.ini" echo dof_range = 40
)
rem 0.22.6: the smooth map light, on, after the [effects] 0.18-0.22 added.
findstr /i /l /c:"smooth_light" "uaro-graphics.ini" >nul 2>&1
if errorlevel 1 (
    >>"uaro-graphics.ini" echo.
    >>"uaro-graphics.ini" echo [effects]
    >>"uaro-graphics.ini" echo ; 1 = the map's baked light - torch pools, shadows on the ground -
    >>"uaro-graphics.ini" echo ;     smooth instead of blotches
    >>"uaro-graphics.ini" echo smooth_light = 1
)
rem Part 3: NVIDIA DLSS's own package, downloaded when it is missing or another version (get-dlss.bat).
if exist "get-dlss.bat" call get-dlss.bat /quiet
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

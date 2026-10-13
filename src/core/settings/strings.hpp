#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace uagfx::settings {

// The settings app speaks English, Ukrainian and Russian (stage 2 spec §6); the report and the log stay English.
enum class Lang : uint8_t { En, Uk, Ru };
constexpr int kLangCount = 3;

std::optional<Lang> parseLang(const std::string& token);  // "en", "uk", "ru", any case
const char* langToken(Lang l);                            // [utility] language
const char* langName(Lang l);                             // in its own language
// The language of Windows' menus (GetUserDefaultUILanguage): Ukrainian or Russian, else English.
Lang langForWindows(uint16_t langId);

enum class Text : uint16_t {
    AppTitle, PageRender, PageScreen, PageQuality, PageFrames, PageDiagnostics, PageService,
    Apply, ApplyAndPlay, Revert, Close,
    StatusRunning, StatusClosed, StatusPending, StatusApplied, ElevatedBanner,
    AfterRestart, LiveNote, LiveNoteRunning, AskUnsaved,
    RenderHeading, RenderUaro, RenderDgVoodoo, RenderUaroHint, RenderDgVoodooHint, RenderNeedsWindows10,
    RenderNoD3D12, InstalledOurs, InstalledNoDriver, DgVoodooFound, DgVoodooMissing, PauseNote,
    ScreenShow, ScreenFullscreen, ScreenWindow, ScreenMonitor, MonitorItem, MonitorMain, ScreenUi, ScreenUiHint,
    ScreenUiExact, ScreenUiDgVoodoo, ScreenMode, ScreenModeHint, ScreenWindowSize, ScreenWindowSizeHint, WindowItem,
    ScreenResult, ScreenCrispest, ScreenTooBig, ScreenTooSmall, ScreenBadNumber, ScreenNowWindow,
    ScreenNowFullscreen, ScreenSetupDiffers, ShowWindowWord, ShowFullscreenWord, ScreenAsSetup, ScreenRoext,
    ScreenFirstRun, ScreenAltTab,
    QualityHighRes, QualityHighResHint, QualityRenderScale, QualityRenderScaleHint, QualitySharpen,
    QualitySharpenHint, QualityUiFilter, QualityUiFilterHint, UiSharp, UiSmooth, QualityCrispText,
    QualityCrispTextHint,
    FramesVsync, FramesVsyncHint, FramesFpsLimit, FramesFpsLimitHint, FramesTearing, FramesTearingHint,
    DiagCheck, DiagResult, DiagFix, DiagOpenLog, DiagOpenFolder, DiagReport, DiagRecheck, DiagVerboseLog,
    DiagVerboseLogHint, DiagReportDone, DiagWindows, DiagWindowsValue, DiagWindowsOld, DiagGpu, DiagGpuValue,
    DiagD3D12, DiagD3D12Ok, DiagD3D12Failed, DiagMonitor, DiagMonitorValue, Yes, No, DiagDdraw,
    DiagDdrawOther, DiagDdrawNone, DiagDgVoodooOk, DiagDgVoodooMissing, DiagModuleRoext, DiagModuleOther,
    DiagModuleOff, DiagInput, DiagInputNone, DiagFolder, DiagFolderOk, DiagFolderReadOnly, DiagFolderProgramFiles,
    DiagApp, DiagElevated, DiagGame, DiagGameRunning, DiagGameClosed, DiagCompat, DiagCompatUser, DiagCompatAll,
    CompatRunAsAdmin, CompatOldWindows, CompatDpi, CompatColors, CompatOther, CompatAllUsersHint, DiagRegistry,
    DiagRegistryValue, DiagRegistryNone, DiagRegistryDiffers, DiagLogRenderer, DiagLogErrors, DiagLogErrorsNone,
    DiagLogErrorsSome, DiagLogFps, DiagLog, DiagLogNone, DiagFixed,
    ServicePresets, PresetMaximum, PresetBalance, PresetSpeed, PresetPlain, PresetNote, PresetCustom,
    ServiceBackups, BackupRestore, BackupPrune, BackupNote, BackupNone, BackupAsk, ServiceReset, ResetButton,
    ResetAsk, ServiceVersions, VersionsValue, Unknown,
    ErrWriteFile, ErrWriteRegistry, ErrGameRunning, ScreenWaits, ErrBackup, ErrScreen, ErrLaunch, ErrRestore,
    Count
};

const char* tr(Lang l, Text t);  // UTF-8
// "{1}", "{2}", ... replaced by the arguments.
std::string fmt(const std::string& pattern, const std::vector<std::string>& args);

}  // namespace uagfx::settings

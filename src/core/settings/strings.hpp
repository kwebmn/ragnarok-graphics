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
    DiagD3D12, DiagD3D12Ok, DiagD3D12Failed, DiagMonitor, DiagMonitorValue, DiagMonitorWork, DiagMonitorWorkValue,
    Yes, No, DiagDdraw,
    DiagDdrawOther, DiagDdrawNone, DiagDgVoodooOk, DiagDgVoodooMissing, DiagModuleRoext, DiagModuleOther,
    DiagModuleOff, DiagInput, DiagInputNone, DiagFolder, DiagFolderOk, DiagFolderReadOnly, DiagFolderProgramFiles,
    DiagApp, DiagElevated, DiagGame, DiagGameRunning, DiagGameClosed, DiagCompat, DiagCompatUser, DiagCompatAll,
    CompatRunAsAdmin, CompatOldWindows, CompatDpi, CompatColors, CompatOther, CompatAllUsersHint, DiagRegistry,
    DiagRegistryValue, DiagRegistryNone, DiagRegistryDiffers, DiagLogRenderer, DiagLogCrisp, DiagLogErrors,
    DiagLogErrorsNone,
    DiagLogErrorsSome, DiagLogFps, DiagLog, DiagLogNone, DiagFixed,
    ServicePresets, PresetMaximum, PresetBalance, PresetSpeed, PresetPlain, PresetNote, PresetCustom,
    ServiceBackups, BackupRestore, BackupPrune, BackupNote, BackupNone, BackupAsk, ServiceUaroGraphics, ResetButton,
    ResetAsk, VersionsValue, Unknown,
    ErrWriteFile, ErrWriteRegistry, ErrGameRunning, ApplyWaits, ErrBackup, ErrScreen, ErrLaunch, ErrRestore,
    PageSound, PageGame, PageChat,
    GameQualityHeading, GameSprites, GameSpritesHint, GameTextures, GameTexturesHint, LevelHigh, LevelHalf, LevelLow,
    GameMipmaps, GameMipmapsHint, GameLightmaps, GameLightmapsHint, GameFog, GameFogHint, GameClosedOnly,
    SoundMode, SoundModeHint, SoundOff, Sound2D, Sound3D, SoundRate, SoundRateHint, Rate22050, Rate11025, Rate8000,
    SoundBits, SoundBitsHint, Bits16, Bits8, SoundVoices, SoundVoicesHint, Voices48, Voices32, Voices16,
    SoundSpeakers, SoundSpeakersHint, Speakers2, SpeakersPhones, SpeakersSurround, Speakers4, Speakers51, Speakers71,
    SoundMusic, SoundMusicVolume, SoundEffects, SoundEffectsVolume, SoundVolumeHint,
    GameEffects, GameMiss, GameSkillSnap, GameAttackSnap, GameItemSnap, GameCamera, GameHomunAi, GameMercAi,
    GameSkin, GameSkinHint, SkinNone, SkinMissing, GameTips, GameIntro, GameMinimap, GameStacks,
    ChatWhisperStrangers, ChatWhisperFriends, ChatWhisperSound, ChatFriendsNote, ChatRememberLogin, ChatForgetLogin,
    ChatForgetAsk, ChatLoginForgotten, ChatMacrosHeading, ChatMacrosHint,
    ServiceGameSettings, GameSave, GameLoad, LayoutReset, GameSettingsNote, GameLoadAsk, LayoutResetAsk, GameSaved,
    GameLoaded, LayoutResetDone, ErrReadFile, FileFilterReg, DiagGpuValueCapped, SoundSetupDiffers,
    GameSettingsRunning, ErrPartialFile,
    PageSmoothing, SmoothMsaa, SmoothMsaaHint, SmoothA2c, SmoothA2cHint, SmoothAniso, SmoothAnisoHint, SmoothOff,
    Factor2, Factor4, Factor8, Factor16, RenderBridge, RenderBridgeHint,
    SmoothDlss, SmoothDlssHint, DlssDlaa, DlssQuality, DlssBalanced, DlssPerformance, DlssUltra,
    FramesGeneration, FramesGenerationHint, Times3, AiEnable, AiEnableHint, AiBudget, AiBudgetHint,
    AiDenoise, AiDenoiseHint, RenderRemix, RenderRemixHint,
    PageAi, PageAiCache, AiPreset, AiPresetHint, AiPresetRecommended, AiPresetAll, AiPresetCustom, AiWhat, AiGround,
    AiGroundHint, AiObjects, AiObjectsHint, AiSprites, AiSpritesHint, AiEffects, AiEffectsHint, AiModelWorld,
    AiModelWorldHint, AiModelSprites, AiModelSpritesHint, AiModelGeneral, AiModelAnime, AiScale, AiScaleHint,
    AiScale4, AiScale2, AiSizes, AiSizesHint, AiAhead, AiAheadHint, AiOffNote,
    AiCacheSummary, AiCacheNone, AiCacheUnreadable, AiCacheMap, AiCacheColGround, AiCacheColObjects,
    AiCacheColSprites, AiCacheColEffects, AiCacheColTotal, AiCacheColSize, AiCacheAllMaps, AiCacheNoMap,
    AiCacheDeleteKinds, AiCacheDelete, AiCacheDeleteOld, AiCacheDeleteAll, AiCacheOldNote, AiCacheRunning,
    AiCacheAskDelete, AiCacheAskAll, AiCacheDeleted, AiCacheDeletedAll, AiCacheNothing, AiCacheError, SizeMb, SizeGb,
    AiIcons, AiIconsHint, AiCacheColIcons, AiUiPieces, AiUiPiecesHint, AiPlayers, AiPlayersHint, AiNpcs, AiNpcsHint,
    AiItems, AiItemsHint,
    Count
};

const char* tr(Lang l, Text t);  // UTF-8
// "{1}", "{2}", ... replaced by the arguments.
std::string fmt(const std::string& pattern, const std::vector<std::string>& args);

}  // namespace uagfx::settings

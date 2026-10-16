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
// 0.48: the language the settings app starts in - the one chosen in it, else English (until 0.47 the language of
// Windows).
Lang startLang(std::optional<Lang> chosen);

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
    PageEffects, FxIntro, FxGlow, FxGlowHint, FxGlowStrength, FxGlowStrengthHint, FxGlowSize, FxGlowSizeHint,
    FxWater, FxWaterHint, FxWaterWaves, FxWaterWavesHint, FxWaterReflections, FxWaterReflectionsHint, FxWaterClarity,
    FxWaterClarityHint, FxGlowLight, FxGlowLightHint, FxAo, FxAoHint, FxAoStrength, FxAoStrengthHint, FxAoSize,
    FxAoSizeHint, FxGrade, FxGradeHint, FxGradeSaturation, FxGradeSaturationHint, FxGradeContrast,
    FxGradeContrastHint, FxGradeBrightness, FxGradeBrightnessHint, FxGradeWarmth, FxGradeWarmthHint,
    FxGradeVignette, FxGradeVignetteHint, FxDof, FxDofHint, FxDofStrength, FxDofStrengthHint, FxDofRange,
    FxDofRangeHint, FxSmoothLight, FxSmoothLightHint, FxTorch, FxTorchHint, FxTorchGlow, FxTorchGlowHint, FxTorchLight,
    FxTorchLightHint, FxTorchFlicker, FxTorchFlickerHint, FxObjects, FxObjectsIntro, FxTorchFlame, FxTorchFlameHint,
    FxTorchBrightness, FxTorchBrightnessHint, FxSmoke, FxSmokeHint, FxSmokeDensity, FxSmokeDensityHint, FxLamps,
    FxLampsHint, FxLampsLight, FxLampsLightHint, FxFireLight, FxFireLightHint, FxFireLightStrength,
    FxFireLightStrengthHint, FxAura, FxAuraHint, FxAuraBrightness, FxAuraBrightnessHint,
    FxFireflies, FxFirefliesHint, FxFirefliesBrightness, FxFirefliesBrightnessHint, FxMapGlows, FxMapGlowsHint,
    FxMapGlowsBrightness, FxMapGlowsBrightnessHint, FxRays, FxRaysHint, FxRaysBrightness, FxRaysBrightnessHint,
    FxLava, FxLavaHint, FxLavaBrightness, FxLavaBrightnessHint, FxActorShadows, FxActorShadowsHint,
    FxActorShadowsStrength, FxActorShadowsStrengthHint, FxClouds, FxCloudsHint, FxCloudsStrength,
    FxCloudsStrengthHint, FxHaze, FxHazeHint, FxHazeStrength, FxHazeStrengthHint,
    FxHeat, FxHeatHint, FxHeatStrength, FxHeatStrengthHint, FxSkillColours, FxSkillColoursHint,
    FxSkillsSoft, FxSkillsSoftHint, FxSkillBrightness, FxSkillBrightnessHint, FxPortals, FxPortalsHint,
    FxPortalsStrength, FxPortalsStrengthHint, FxWeather, FxWeatherHint, FxWeatherAmount, FxWeatherAmountHint,
    QualityWindowArt, QualityWindowArtHint, WindowArtXbr, WindowArtAi, WindowArtPixels,
    AiGenerate, AiGenerateHint, AiPackNote,
    PagePack, PackIntro, PackHas, PackNone, PackWhat, PackWorld, PackWorldHint, PackEffects, PackEffectsHint,
    PackMonsters, PackMonstersHint, PackPlayers, PackPlayersHint, PackNpcs, PackNpcsHint, PackItems, PackItemsHint,
    PackInterface, PackInterfaceHint, PackQuality, PackQualityHint, PackQualitySmall, PackQualityBalanced,
    PackQualityLossless, PackNets, PackMake, PackStop, PackAnew, PackCopy, PackAskAnew, PackNoHost,
    PackNothing, PackStarting, PackReading, PackBuilding, PackBuildingNoEta, PackFile, PackDone,
    PackStopped, PackFailed, PackGone, PackGameRunning, PackBackground, 
    TimeHoursMinutes, TimeMinutes, TimeSeconds,
    RenderApi, RenderApiHint, RenderApiD3d12, RenderApiD3d11, RenderApiAuto, RenderUseD3d11, DiagD3D11,
    DiagD3D11Ok, DiagD3D11Failed, AiD3d11Note,
    AiCacheAskOld, AiCacheOldDeleted,
    ServiceUpdate, UpdateCheck, UpdateNow, UpdateChecking, UpdateNewer, UpdateLatest, UpdateFailed, UpdateAsk,
    UpdateGameRunning, UpdateNote, UpdateError,
    // 0.50: the automatic setup
    AutoHeading, AutoButton, AutoNote, AutoFound, AutoTierNoDriver, AutoTierOld, AutoTierLow, AutoTierMiddle,
    AutoTierHigh, AutoAiAll, AutoAiSprites, AutoAiNoPack, AutoAiOff, AutoAsk, AutoPending,
    // 0.50.2: the graphics card
    RenderGpu, RenderGpuHint, RenderGpuAuto, RenderGpuItem, RenderGpuMissing,
    // 0.51: reports to the developers
    ReportHeading, ReportNote, ReportComment, ReportCommentCue, ReportSend, ReportAsk, ReportSent, ReportFailed,
    ReportSentList, ReportAuto, ReportAutoHint,
    // 0.53: the texture pack from the site
    PackFetchHeading, PackFetchNote, PackFetchButton, PackFetchStop, PackFetchListing, PackFetchProgress, PackFetchDone,
    PackFetchUpToDate, PackFetchFailed, PackFetchStopped, PackMakeHeading,
    // 0.54
    FramesFpsCounter, FramesFpsCounterHint,
    // 0.55: profiles
    ServiceProfiles, ProfileNote, ProfileName, ProfileNameCue, ProfileSave, ProfileLoad, ProfileDelete, ProfileNone,
    ProfileSaved, ProfileLoaded, ProfileAskLoad, ProfileAskDelete, ProfileBadName,
    // 0.56
    PackBodyColours, PackBodyColoursHint,
    // 0.57
    AiCompress, AiCompressHint, AiCompressAuto, AiCompressOn, AiCompressOff,
    // 0.62
    QualityUiNative, QualityUiNativeHint,
    // 0.63
    FxNight, FxNightHint, FxNightStrength, FxNightStrengthHint, FxNightClock, FxNightClockHint,
    // 0.64
    FxFoliage, FxFoliageHint, FxFoliageStrength, FxFoliageStrengthHint,
    // 0.65
    QualityHdr, QualityHdrHint,
    // 0.67
    FxSoftParticles, FxSoftParticlesHint,
    // 0.68
    FxRimLight, FxRimLightHint, FxRimStrength, FxRimStrengthHint,
    // 0.69
    FxSky, FxSkyHint, FxSkyClouds, FxSkyCloudsHint, FxSkyStrength, FxSkyStrengthHint,
    // 0.70
    FxMotes, FxMotesHint, FxMotesAmount, FxMotesAmountHint,
    // 0.71
    FxRelief, FxReliefHint, FxReliefStrength, FxReliefStrengthHint,
    // 0.72
    FxFloorReflections, FxFloorReflectionsHint, FxFloorReflectStrength, FxFloorReflectStrengthHint,
    // 0.73
    FxShafts, FxShaftsHint, FxShaftsStrength, FxShaftsStrengthHint,
    // 0.74
    FxDayCycle, FxDayCycleHint,
    // 0.75
    SmoothTaa, SmoothTaaHint,
    // 0.76
    FramesKeepFps, FramesKeepFpsHint, FxCostMs,
    // 0.77
    FxSunShadows, FxSunShadowsHint, FxSunShadowsStrength, FxSunShadowsStrengthHint,
    // 0.78
    FxBloom, FxBloomHint, FxBloomStrength, FxBloomStrengthHint, FxFilmic, FxFilmicHint,
    // 0.79
    FxRegionLooks, FxRegionLooksHint, FxRegionLooksStrength, FxRegionLooksStrengthHint,
    // 0.80
    FxFootprints, FxFootprintsHint, FxStorms, FxStormsHint,
    // 0.81
    FxGrass, FxGrassHint, FxGrassAmount, FxGrassAmountHint,
    // 0.82
    FxSkillSparks, FxSkillSparksHint, FxSkillSparksAmount, FxSkillSparksAmountHint,
    // 0.83
    FxFilmGrain, FxFilmGrainHint, FxFilmGrainAmount, FxFilmGrainAmountHint, FxMapFade, FxMapFadeHint,
    // 0.84
    FxRoundLight, FxRoundLightHint,
    // 0.85
    FramesSavePower, FramesSavePowerHint,
    // 0.87
    DiagProfile, DiagProfileHint,
    GfxSoundOurs, GfxSoundRate, GfxSoundRateHint, GfxSoundRateGame, GfxSoundRate44, GfxSoundRate48, GfxSoundVoices,
    GfxSoundVoicesHint,
    GfxSoundBackground, GfxSoundBackgroundHint, GfxSoundLimiter, GfxSoundLimiterHint,
    GfxSoundStereo, GfxSoundStereoHint, GfxSoundSteal, GfxSoundStealHint, GfxSoundCrossfade, GfxSoundCrossfadeHint,
    // 0.93
    FxAppear, FxAppearHint,
    // 0.94
    FxRipples, FxRipplesHint,
    // 0.95
    FxGlint, FxGlintHint,
    // 0.96
    FxHitFlash, FxHitFlashHint,
    // 0.97
    FxWetGround, FxWetGroundHint,
    Count
};

const char* tr(Lang l, Text t);  // UTF-8
// "{1}", "{2}", ... replaced by the arguments.
std::string fmt(const std::string& pattern, const std::vector<std::string>& args);

}  // namespace uagfx::settings

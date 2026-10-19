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
    RenderHeading, RenderNeedsWindows10,
    RenderNoD3D12, InstalledOurs, InstalledNoDriver, PauseNote,
    ScreenShow, ScreenFullscreen, ScreenWindow, ScreenMonitor, MonitorItem, MonitorMain, ScreenUi, ScreenUiHint,
    ScreenUiExact, ScreenWindowSize, ScreenWindowSizeHint, WindowItem,
    ScreenResult, ScreenCrispest, ScreenTooBig, ScreenTooSmall, ScreenBadNumber, ScreenNowWindow,
    ScreenNowFullscreen, ScreenRoext,
    ScreenFirstRun, ScreenAltTab,
    QualityHighRes, QualityHighResHint, QualityRenderScale, QualityRenderScaleHint, QualityWorldScale,
    QualityWorldScaleHint, QualityFullColour, QualityFullColourHint, QualitySharpen,
    QualitySharpenHint, QualityUiFilter, QualityUiFilterHint, UiSharp, UiSmooth, QualityCrispText,
    QualityCrispTextHint,
    FramesVsync, FramesVsyncHint, FramesFpsLimit, FramesFpsLimitHint, FramesTearing, FramesTearingHint,
    DiagCheck, DiagResult, DiagFix, DiagOpenLog, DiagOpenFolder, DiagReport, DiagRecheck, DiagVerboseLog,
    DiagVerboseLogHint, DiagReportDone, DiagWindows, DiagWindowsValue, DiagWindowsOld, DiagGpu, DiagGpuValue,
    DiagD3D12, DiagD3D12Ok, DiagD3D12Failed, DiagMonitor, DiagMonitorValue, DiagMonitorWork, DiagMonitorWorkValue,
    Yes, No, DiagDdraw,
    DiagDdrawOther, DiagDdrawNone, DiagModuleRoext, DiagModuleOther,
    DiagModuleOff, DiagInput, DiagInputNone, DiagFolder, DiagFolderOk, DiagFolderReadOnly, DiagFolderProgramFiles,
    DiagApp, DiagElevated, DiagGame, DiagGameRunning, DiagGameClosed, DiagCompat, DiagCompatUser, DiagCompatAll,
    CompatRunAsAdmin, CompatOldWindows, CompatDpi, CompatColors, CompatOther, CompatAllUsersHint, DiagRegistry,
    DiagRegistryValue, DiagRegistryNone, DiagLogRenderer, DiagLogCrisp, DiagLogErrors,
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
    GameLoaded, LayoutResetDone, ErrReadFile, FileFilterReg, DiagGpuValueCapped, GameSettingsRunning, ErrPartialFile,
    PageSmoothing, SmoothMsaa, SmoothMsaaHint, SmoothA2c, SmoothA2cHint, SmoothAniso, SmoothAnisoHint, SmoothOff,
    Factor2, Factor4, Factor8, Factor16, RenderBridge, RenderBridgeHint, RenderThread, RenderThreadHint,
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
    PageEffects, FxGlow, FxGlowHint, FxGlowStrength, FxGlowStrengthHint, FxGlowSize, FxGlowSizeHint,
    FxWater, FxWaterHint, FxWaterWaves, FxWaterWavesHint, FxWaterReflections, FxWaterReflectionsHint, FxWaterClarity,
    FxWaterClarityHint, FxGlowLight, FxGlowLightHint, FxAo, FxAoHint, FxAoStrength, FxAoStrengthHint, FxAoSize,
    FxAoSizeHint, FxGrade, FxGradeHint, FxGradeSaturation, FxGradeSaturationHint, FxGradeContrast,
    FxGradeContrastHint, FxGradeBrightness, FxGradeBrightnessHint, FxGradeWarmth, FxGradeWarmthHint,
    FxGradeVignette, FxGradeVignetteHint, FxDof, FxDofHint, FxDofStrength, FxDofStrengthHint, FxDofRange,
    FxDofRangeHint, FxSmoothLight, FxSmoothLightHint, FxTorch, FxTorchHint, FxTorchGlow, FxTorchGlowHint, FxTorchLight,
    FxTorchLightHint, FxTorchFlicker, FxTorchFlickerHint, FxTorchFlame, FxTorchFlameHint,
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
    PackQualityLossless, PackMake, PackStop, PackAnew, PackCopy, PackAskAnew, PackNoHost,
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
    // 0.53 (the site's download gone in 2.5.28)
    PackMakeHeading,
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
    // 0.98
    FxFlare, FxFlareHint,
    GfxSoundEngine, GfxSoundEngineHint, GfxSoundReverb, GfxSoundReverbHint,
    GfxSoundHeadphones, GfxSoundHeadphonesHint,
    GfxSoundDuck, GfxSoundDuckHint,
    AutoUiWhole, AutoDlaa, AutoDlssQuality, AutoHdr, AutoSoundEngine,
    // 1.4: the Gamepad page (uaRO Pad)
    PagePad, PadHeading, PadIntro, PadStateOff, PadStateOn, PadStateOld, PadStateOther, PadSwitchOn, PadSwitchOff,
    PadUpdate, PadNoPackage, PadOnDone, PadOffDone, PadFailed, PadRunning, PadController, PadType, PadTypeHint,
    PadTypeAuto, PadTypeSony, PadLightbar, PadLightbarHint, ColourBlue, ColourRed, ColourGreen, ColourPurple,
    ColourOrange, ColourWhite, ColourOff, ColourOwn, PadTouchpad, PadTouchpadHint, TouchNothing, TouchBack, TouchStart,
    PadRumble, PadRumbleHint, PadStartsOn, PadStartsOnHint, PadMenuLanguage, PadMenuLanguageHint, PadSettingsNote,
    PadNpcHint, PadNpcHintHint,
    AiModelAnime6b,  // 1.5
    // 1.6: a model for each group, the preview
    AiGroupsHeading, AiGroupMonsters, AiGroupNpcs, AiGroupItems, AiGroupModelHint, AiModelSet, AiModelSetHint,
    AiModelSetFast, AiModelSetBalanced,
    AiModelSetMaximum, AiModelSetCustom, AiGroupInPack, AiGroupInPackOther, AiNoPackYet, AiUnknownSprites, AiRedo,
    AiRedoHint, AiPreviewHeading, AiPreviewGroup, AiPreviewShow, AiPreviewHint, AiPreviewWorking, AiPreviewFailed,
    AiPreviewFrom, AiPreviewGame, PackRedo, PackRedoHint,
    PackNetsGroups,
    AiPreviewZoom, AiPreviewWhole, AiPreviewZoomHint,  // 1.6.1
    AiPreviewOpen, AiPreviewWindowHint,  // 1.6.2
    AiSmall, AiSmallHint, AiSmallSide, AiSmallModel,  // 1.6.3
    // 1.7: the living world
    FxTreeVolume, FxTreeVolumeHint, FxLeafLight, FxLeafLightHint, FxTreeTints, FxTreeTintsHint,
    FxGusts, FxGustsHint, FxDapple, FxDappleHint,
    // 1.7.1
    FxFlowers, FxFlowersHint, FxDew, FxDewHint, FxMorningFog, FxMorningFogHint, FxSandDrift, FxSandDriftHint,
    FxSnowSparkle, FxSnowSparkleHint, FxWetWalls, FxWetWallsHint,
    // 1.7.2
    FxShoreFoam, FxShoreFoamHint, FxFish, FxFishHint, FxSwamp, FxSwampHint,
    // 1.7.3
    FxButterflies, FxButterfliesHint, FxBirds, FxBirdsHint, FxLeafFall, FxLeafFallHint, FxEmbers, FxEmbersHint,
    FxBreath, FxBreathHint, FxSplashes, FxSplashesHint, FxCaveDrips, FxCaveDripsHint, FxUnderwater, FxUnderwaterHint,
    // 1.8
    FxMaterials, FxMaterialsHint, FxMaterialsStrength, FxMaterialsStrengthHint,
    FxTier, FxTierHint, FxTierAuto, FxTierClassic, FxTierRemaster,
    ReportShots, ReportShotsHint, ReportSentShots,
    // 1.9
    FxGroundSeams, FxGroundSeamsHint,
    ConsoleTitle, ConsoleKeys,
    // 1.11
    FxSkyLight, FxSkyLightHint, FxBounceLight, FxBounceLightHint,
    // 1.13
    FxVolumetricFog, FxVolumetricFogHint, FxVolumetricFogDensity, FxVolumetricFogDensityHint,
    // 1.15
    FxSmoothHills, FxSmoothHillsHint, FxSmoothHillsStrength, FxSmoothHillsStrengthHint,
    // 1.16
    FxWorldReflections, FxWorldReflectionsHint,
    // 1.17
    FxSpriteLight, FxSpriteLightHint, FxSpriteLightStrength, FxSpriteLightStrengthHint,
    // 1.18
    FxEdgeLand, FxEdgeLandHint,
    // 1.20
    FxSeasons, FxSeasonsHint, FxRainWet, FxRainWetHint,
    // 1.21
    FxNightWindows, FxNightWindowsHint,
    // 1.22
    FxSharedWind, FxSharedWindHint,
    // 1.23
    FxLitDust, FxLitDustHint,
    // 2.2: the Effects page's sets and groups
    FxPreset, FxPresetHint, FxPresetAuto, FxPresetOff, FxPresetWeak, FxPresetMedium, FxPresetStrong, FxPresetCustom,
    FxSetIntro, FxGroupLight, FxGroupLightHint, FxGroupShadows, FxGroupShadowsHint, FxGroupSurfaces,
    FxGroupSurfacesHint, FxGroupNature, FxGroupNatureHint, FxGroupWater, FxGroupWaterHint, FxGroupAir, FxGroupAirHint,
    FxGroupLife, FxGroupLifeHint, FxGroupSkills, FxGroupSkillsHint, FxGroupPicture, FxGroupPictureHint, FxGroupStrength,
    FxGroupStrengthHint, FxDetails, FxDetailsHide, FxDetailsNote,
    // 2.2.1
    FxGroupFixes, FxGroupFixesHint,
    StatsSend, StatsSendHint, StatsAsk,
    // 2.2.2
    StatsSendNow, StatsState, StatsNever, StatsSentNow, StatsNothing, StatsFailed,
    // 2.2.5: one map's textures on the Texture pack page
    PackMapHeading, PackMapNote, PackMapName, PackMapCue, PackMapNameHint, PackMapGround, PackMapGroundHint,
    PackMapModels, PackMapModelsHint, PackMapModel, PackMapModelHint, PackMapRemake, PackMapDrop, PackMapAskRemake,
    PackMapAskDrop, PackMapBadName, PackDropped, PackWriting,
    // 2.3: uaRO Studio
    StudioTitle, StudioNoGame, StudioCopyFailed, StudioPause, StudioResume, StudioPaused, StudioEngine,
    StudioEngineNone, StudioEngineNewer, StudioJobsIdle, StudioJobRunning, StudioJobsWaiting, StudioStop,
    StudioTabTextures, StudioTabPack, StudioTabLog,
    StudioNavAll, StudioNavMaps, StudioNavFolders, StudioNavStates, StudioNavMissing, StudioNavInPack, StudioNavOwn,
    StudioNavFlagged, StudioNavOverrides, StudioNavOrphans, StudioNavModels, StudioNavReports, StudioFilterCue,
    StudioColPicture, StudioColFile, StudioColState, StudioColModel, StudioColFlags, StudioColDetail, StudioColSize,
    StudioColDate, StudioStateMissing, StudioStateInPack, StudioStateOwn, StudioStateOrphan, StudioFlagSmoothed,
    StudioFlagWashed, StudioFlagBanded, StudioFlagRinging, StudioFlagFringe, StudioFlagAlpha, StudioFlagSeams,
    StudioRowsCount, StudioCatalogEmpty,
    StudioViewPair, StudioViewWipe, StudioViewTiled, StudioViewMap, StudioViewDiff, StudioViewSmooth, StudioVsPack,
    StudioVsMade, StudioInfoFile, StudioInfoArchive, StudioInfoShadowed, StudioInfoKey, StudioInfoLow,
    StudioInfoPicture, StudioInfoNoPicture, StudioInfoMade, StudioInfoFrom, StudioInfoMaps, StudioInfoAtlas,
    StudioInfoOnePicture, StudioInfoColour, StudioInfoMetrics, StudioInfoMaterial, StudioInfoOwn, StudioInfoOrphan,
    StudioMakeAgain, StudioKeep, StudioDiscard, StudioTakeOut, StudioOwnAlways, StudioOwnNot,
    StudioMakeTitle, StudioMakeModel, StudioMakeScale, StudioMakeDenoise, StudioMakeOwnDenoise, StudioMakeBefore,
    StudioMakeDeblock, StudioMakeKey, StudioMakeAfter, StudioMakeTone, StudioMakeGrain, StudioMakeSharpen,
    StudioMakeAlpha, StudioMakeBlend, StudioMakeOriginal, StudioMakeTiles, StudioMakeGo, StudioCancel, StudioNone,
    StudioMakeMany, StudioJobMake, StudioJobKeep, StudioJobTakeOut, StudioJobOwn, StudioJobFailed,
    StudioPackPictures, StudioPackSize, StudioPackByGroup, StudioPackByModel, StudioPackByKind, StudioPackRead,
    StudioPackCompact, StudioPackCompactNote, StudioJobCompact, StudioPackGameOpen, StudioPackNone, StudioLogDone,
    DiagXray, DiagXrayHint,
    StudioViewMaterial, StudioMatGloss, StudioMatRelief, StudioMatHeight, StudioMatSave, StudioJobMaterial,
    // 2.4: the parts of the effects
    FxWaterFoam, FxWaterFoamHint, FxWaterGlints, FxWaterGlintsHint, FxCaustics, FxCausticsHint, FxWaterfalls,
    FxWaterfallsHint, FxWetSand, FxWetSandHint, FxStepDust, FxStepDustHint, FxSeasonSnow, FxSeasonSnowHint,
    FxSeasonGrass, FxSeasonGrassHint, FxPollen, FxPollenHint, FxIndoorDust, FxIndoorDustHint, FxRainSplashes,
    FxRainSplashesHint, FxStars, FxStarsHint, FxLowFog, FxLowFogHint, FxDesertHeat, FxDesertHeatHint, FxSkyRays,
    FxSkyRaysHint, FxTreeSeasons, FxTreeSeasonsHint, FxDragonflies, FxDragonfliesHint,
    DiagRegistryOld,  // 2.4.2
    DlssCustom, DlssScale, DlssScaleHint, DlssWidth, DlssHeight, DlssSizeHint, DlssModel, DlssModelHint, DlssModelAuto,
    DlssModelK, DlssModelJ, DlssModelL, DlssModelM, DlssCustomNow,
    // 2.5.19: the Service page's Palettes.grf
    ServicePalettes, PalettesNote, PalettesFromFile, PalettesFromFolder, PalettesWorking, PalettesDone, PalettesLeft,
    PalettesFailed, PalettesSameFile, PalettesFolderTitle, FileFilterGrfZip, FileFilterGrf,
    // 2.5.20: the light modes
    QualitySuperLight, QualitySuperLightHint, FramesBattery, FramesBatteryHint, FramesBatteryFps, FramesBatteryFpsHint,
    AiDyes, AiDyesHint,  // 2.5.23
    FramesReflex, FramesReflexHint,  // 2.5.26
    AiModelGameAi, AiGameAiMissing, AiGetModels, AiGetModelsFailed,  // 2.5.27
    PackUnread, PackCompact, PackCompactNote, PackCompactAsk, PackCompacting, PackCompacted,  // 2.5.28
    PackCompactFailed,
    // 2.5.28: the 38 groups and their sections on the AI page
    AiSectionGround, AiGroupGroundTown, AiGroupGroundField, AiGroupGroundDungeon, AiGroupGroundIndoor,
    AiGroupGroundOther, AiSectionModels, AiGroupBuildingsTown, AiGroupModelsField, AiGroupModelsDungeon,
    AiGroupFurniture, AiGroupModelsOther, AiGroupTrees, AiGroupPlants, AiGroupWater, AiSectionCharacters,
    AiGroupBodies, AiGroupMounted, AiGroupHeads, AiGroupWeapons, AiGroupShields, AiGroupHeadgear, AiGroupGarments,
    AiSectionMonsters, AiGroupBosses, AiGroupHomunculi, AiSectionItems, AiGroupItemIcons, AiGroupItemPictures,
    AiGroupCardPictures, AiSectionEffects, AiGroupSkillAnimations, AiGroupEffectSprites, AiSectionInterface,
    AiGroupEmotions, AiGroupCursor, AiGroupWindows, AiGroupMenuIcons, AiGroupPortraits, AiGroupMaps, AiGroupLoading,
    AiGroupLogin,
    // 2.6: the modes and the list's new entries
    ModeSimple, ModeDetailed, ModeDeveloper, ViewMain, ViewGraphics, ViewGameChat, PageDebug, AiTabWhat, AiTabModels,
    // 2.6: the Debug page's test switches
    DebugDlssJitter, DebugDlssJitterHint, DebugDlssMotion, DebugDlssMotionHint, DebugPresent, DebugPresentHint,
    DebugPresentAuto, DebugPresentWindow, DebugPresentComposition, DebugDpiScale, DebugDpiScaleHint, DebugVariants,
    DebugVariantsHint, DebugStateCache, DebugStateCacheHint, DebugMergeDraws, DebugMergeDrawsHint, DebugFastPick,
    DebugFastPickHint, DebugFastFtol, DebugFastFtolHint, DebugFastTextures, DebugFastTexturesHint,
    DeveloperOnly, ReportProblem,  // 2.6: the list's heading, the header's button
    DebugTools, DebugStudio, DebugCompact, DebugPalettes, DebugPreview, DebugKeys,  // 2.6: the Debug page's tools
    // 2.6: the Main page
    MainIntro, MainPicture, MainQualityLow, MainQualityMedium, MainQualityHigh, MainQualitySuperLight, MainQualityOwn,
    MainQualityHint, MainOther, MainPad, MainPadHint, MainRecommended, MainWhatsNew, MainState, MainDriver,
    MainUpdateNone, MainUpdateNew, MainNoD3d, MainDlss, MainNoDlss, MainHd, MainPack, MainNoPack, MainMakePack,
    MainMore, NotesNone,
    SearchCue, SearchFound, SearchNothing, SearchMany, SearchHow,  // 2.6: the header's search
    // 2.6: the status bar, the "changed" dots, the page's reset, "What's new"
    StripNoPack, DotHint, DotOn, DotOff, DotLegend, ResetPage, ResetPageAsk, WhatsNewTitle,
    ConsoleGroupOff,  // 2.6.8: an effect on whose group is off
    Count
};

const char* tr(Lang l, Text t);  // UTF-8
// "{1}", "{2}", ... replaced by the arguments.
std::string fmt(const std::string& pattern, const std::vector<std::string>& args);

}  // namespace uagfx::settings

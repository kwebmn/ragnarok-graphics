#include "core/settings/schema.hpp"

#include "core/effect_groups.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <unordered_map>

namespace uagfx::settings {

Text pageName(Page p) {
    switch (p) {
        case Page::Render: return Text::PageRender;
        case Page::Screen: return Text::PageScreen;
        case Page::Quality: return Text::PageQuality;
        case Page::Smoothing: return Text::PageSmoothing;
        case Page::Effects: return Text::PageEffects;
        case Page::Ai: return Text::PageAi;
        case Page::AiCache: return Text::PageAiCache;
        case Page::Pack: return Text::PagePack;
        case Page::Frames: return Text::PageFrames;
        case Page::Sound: return Text::PageSound;
        case Page::Pad: return Text::PagePad;
        case Page::Game: return Text::PageGame;
        case Page::Chat: return Text::PageChat;
        case Page::Diagnostics: return Text::PageDiagnostics;
        case Page::Debug: return Text::PageDebug;
        case Page::Launcher: return Text::ViewPlay;
        case Page::Interface: return Text::InterfaceHeading;  // 3.0.11-beta.3
        case Page::Service: break;
    }
    return Text::PageService;
}

const std::vector<IniSetting>& iniSettings() {
    using E = EffectSettings;  // 2.2.2: [effects] entries bound to their fields
    static const std::vector<IniSetting> list{
        {"renderer", "api", Page::Render, Control::Choice, 0, 0, 0, {"d3d12", "d3d11", "auto", "system"},
         {Text::RenderApiD3d12, Text::RenderApiD3d11, Text::RenderApiAuto, Text::RenderApiSystem}, Text::RenderApi,
         Text::RenderApiHint, ""},
        // 0.50.2: a card's name as the Renderer page lists them, or auto (a list of its own there, not a text field)
        {"renderer", "gpu", Page::Render, Control::Line, 0, 0, 0, {}, {}, Text::RenderGpu, Text::RenderGpuHint, ""},
        // 0.51: the Diagnostics page's own check box
        {"report", "auto", Page::Diagnostics, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::ReportAuto,
         Text::ReportAutoHint, ""},
        {"stats", "send", Page::Diagnostics, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::StatsSend,
         Text::StatsSendHint, ""},  // 2.2.1
        // 2.9: uaRO Pad inside the driver, on by default; the Main page shows it too
        {"pad", "enabled", Page::Pad, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::PadEnabled, Text::PadEnabledHint,
         ""},
        // 2.11: a direct start of uaRO.exe opens the launcher «Гра» unless this is ticked
        {"launcher", "direct_start", Page::Launcher, Control::Check, 0, 1, 0, {"0", "1"}, {}, Text::LauncherDirect,
         Text::LauncherDirectHint, ""},
        // 3.0.1-beta.5: our own libraries in place of the game's old ones (core/client_libs.hpp), «Сервіс»
        {"client", "cps", Page::Service, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::ClientCps, Text::ClientCpsHint,
         ""},
        {"client", "bink", Page::Service, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::ClientBink,
         Text::ClientBinkHint, ""},
        {"client", "ijl", Page::Service, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::ClientIjl, Text::ClientIjlHint,
         ""},  // 3.0.1-beta.6
        {"client", "mss", Page::Service, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::ClientMss, Text::ClientMssHint,
         ""},  // 3.0.1-beta.7
        {"client", "roext", Page::Service, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::ClientRoext,
         Text::ClientRoextHint, ""},  // 3.0.1-beta.8: ROExt's work in the driver; its own rows on «Гра й чат»
        {"client", "updates", Page::Service, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::ClientUpdates,
         Text::ClientUpdatesHint, ""},  // 3.0.2-beta.5: the client's graphics updates (updates\)
        {"roext", "window_lock", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::RoextLock,
         Text::RoextLockHint, ""},
        {"roext", "window_on_top", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::RoextOnTop,
         Text::RoextOnTopHint, ""},
        {"roext", "mouse_buttons", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::RoextMouse,
         Text::RoextMouseHint, ""},
        {"roext", "alt_f4", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::RoextAltF4, Text::RoextAltF4Hint,
         ""},
        {"roext", "minimize_key", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::RoextMinimize,
         Text::RoextMinimizeHint, ""},
        {"roext", "free_cpu", Page::Game, Control::Slider, 0, 1000, 1, {}, {}, Text::RoextFreeCpu,
         Text::RoextFreeCpuHint, "ms"},
        {"roext", "code_page", Page::Game, Control::Number, -1, 65535, 1, {}, {}, Text::RoextCodePage,
         Text::RoextCodePageHint, ""},
        {"roext", "window_x", Page::Game, Control::Number, -100000, 100000, 1, {}, {}, Text::RoextWindowX,
         Text::RoextWindowPlaceHint, ""},
        {"roext", "window_y", Page::Game, Control::Number, -100000, 100000, 1, {}, {}, Text::RoextWindowY,
         Text::RoextWindowPlaceHint, ""},
        // 3.0.3-beta.2: several game windows at once
        {"windows", "title_name", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::WindowsTitleName,
         Text::WindowsTitleNameHint, ""},
        {"windows", "remember_place", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::WindowsRememberPlace,
         Text::WindowsRememberPlaceHint, ""},
        {"windows", "start_count", Page::Game, Control::Slider, 1, 8, 1, {}, {}, Text::WindowsStartCount,
         Text::WindowsStartCountHint, ""},  // 3.0.4-beta.4: a set of game windows by «Грати»
        {"windows", "switch_keys", Page::Game, Control::Choice, 0, 0, 0, {"all", "back", "off"},
         {Text::WindowsKeysAll, Text::WindowsKeysBack, Text::WindowsKeysOff}, Text::WindowsKeys,
         Text::WindowsKeysHint, ""},  // 3.0.3-beta.6: the keys that switch the game windows
        // 3.0.11-beta.1: the new interface's minimap prototype (stage 1) - off unless asked for
        {"interface", "minimap", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::InterfaceMinimap,
         Text::InterfaceMinimapHint, ""},
        // 3.0.11-beta.6: its hints from the game's reference - each its own switch
        {"interface", "minimap_npcs", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfaceMinimapNpcs, Text::InterfaceMinimapNpcsHint, ""},
        {"interface", "minimap_warps", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfaceMinimapWarps, Text::InterfaceMinimapWarpsHint, ""},
        {"interface", "minimap_mobs", Page::Interface, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::InterfaceMinimapMobs, Text::InterfaceMinimapMobsHint, ""},
        // 3.0.3-beta.8: the game windows' panel in the game's picture - a section of its own («Панель вікон»)
        {"panel", "show", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::WindowsPanel,
         Text::WindowsPanelHint, ""},
        {"panel", "layout", Page::Game, Control::Choice, 0, 0, 0, {"row", "column", "icons"},  // 3.0.3-beta.11: icons
         {Text::WindowsPanelRow, Text::WindowsPanelColumn, Text::WindowsPanelIcons}, Text::WindowsPanelLayout,
         Text::WindowsPanelLayoutHint, ""},
        {"panel", "size", Page::Game, Control::Slider, 75, 150, 25, {}, {}, Text::WindowsPanelSize,
         Text::WindowsPanelSizeHint, " %"},
        {"panel", "opacity", Page::Game, Control::Slider, 40, 100, 10, {}, {}, Text::PanelOpacity,
         Text::PanelOpacityHint, " %"},
        // 3.0.4-beta.5: the camera - building maps as free as outdoors (a section of its own, «Камера»)
        {"camera", "indoor", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::CameraIndoor,
         Text::CameraIndoorHint, ""},
        // 3.0.4-beta.17: the camera no further out than the character is drawn (R-35N5QN)
        {"camera", "zoom_limit", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::CameraZoomLimit,
         Text::CameraZoomLimitHint, ""},
        // 3.0.4-beta.6: the time left on the buff icons
        {"buffs", "timers", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::BuffTimers,
         Text::BuffTimersHint, ""},
        // 3.0.4-beta.3: notifications while the game is behind other windows - a section of its own («Сповіщення»)
        {"notify", "on", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::NotifyOn, Text::NotifyOnHint, ""},
        {"notify", "whisper", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::NotifyWhisper,
         Text::NotifyKindHint, ""},
        {"notify", "party", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::NotifyParty,
         Text::NotifyKindHint, ""},
        {"notify", "trade", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::NotifyTrade,
         Text::NotifyKindHint, ""},
        {"notify", "guild", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::NotifyGuild,
         Text::NotifyKindHint, ""},
        {"notify", "friend", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::NotifyFriend,
         Text::NotifyKindHint, ""},
        {"notify", "death", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::NotifyDeath,
         Text::NotifyKindHint, ""},
        // 3.0.7-beta.6 (the user chose 1, 5, 6, 7, 8, 9, 10 «и чтобы это в конфигураторе настраивалось»)
        {"notify", "attack", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::NotifyAttack,
         Text::NotifyAttackHint, ""},
        // 3.0.7-beta.7 (the user: «надо сделать чтобы когда хп и сп восстановились»)
        {"notify", "recovered", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::NotifyRecovered,
         Text::NotifyRecoveredHint, ""},
        {"notify", "recovered_percent", Page::Game, Control::Slider, kRecoveredLeast, kRecoveredMost, 5, {}, {},
         Text::NotifyRecoveredPercent, Text::NotifyRecoveredPercentHint, " %"},
        {"notify", "buff_ending", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::NotifyBuffEnding,
         Text::NotifyBuffEndingHint, ""},
        {"notify", "buff_seconds", Page::Game, Control::Slider, kBuffSecondsLeast, kBuffSecondsMost, 1, {}, {},
         Text::NotifyBuffSeconds, Text::NotifyBuffSecondsHint, " s"},
        {"notify", "party_death", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::NotifyPartyDeath,
         Text::NotifyPartyDeathHint, ""},
        {"notify", "party_low_hp", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::NotifyPartyLowHp,
         Text::NotifyPartyLowHpHint, ""},
        {"notify", "party_hp", Page::Game, Control::Slider, kPartyHpLeast, kPartyHpMost, 5, {}, {},
         Text::NotifyPartyHp, Text::NotifyPartyHpHint, " %"},
        {"notify", "party_chat", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::NotifyPartyChat,
         Text::NotifyChatHint, ""},
        {"notify", "guild_chat", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::NotifyGuildChat,
         Text::NotifyChatHint, ""},
        {"notify", "mention", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::NotifyMention,
         Text::NotifyMentionHint, ""},
        {"notify", "disconnect", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::NotifyDisconnect,
         Text::NotifyDisconnectHint, ""},
        {"notify", "sound", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::NotifySound,
         Text::NotifySoundHint, ""},
        // 3.0.9-beta.4: the developer answered a report
        {"notify", "report_answer", Page::Game, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::NotifyAnswer,
         Text::NotifyAnswerHint, ""},
        {"renderer", "bridge", Page::Render, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::RenderBridge,
         Text::RenderBridgeHint, ""},
        // 2.12: Direct3D 11 without the risky parts, also by itself after two failed starts (core/safe_mode)
        {"renderer", "safe_mode", Page::Render, Control::Check, 0, 1, 0, {"0", "1"}, {}, Text::RenderSafeMode,
         Text::RenderSafeModeHint, ""},
        {"renderer", "render_thread", Page::Debug, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::RenderThread,
         Text::RenderThreadHint, ""},  // 2.5.12
        {"renderer", "remix", Page::Debug, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::RenderRemix,
         Text::RenderRemixHint, ""},
        {"quality", "super_light", Page::Quality, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::QualitySuperLight,
         Text::QualitySuperLightHint, ""},  // 2.5.20: first - over the rest of the page
        {"display", "high_resolution", Page::Quality, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::QualityHighRes,
         Text::QualityHighResHint, ""},
        {"quality", "render_scale", Page::Quality, Control::Slider, 50, 200, 10, {}, {}, Text::QualityRenderScale,
         Text::QualityRenderScaleHint, " %"},
        {"quality", "world_scale", Page::Quality, Control::Slider, 50, 100, 5, {}, {}, Text::QualityWorldScale,
         Text::QualityWorldScaleHint, " %"},  // 2.5.16
        {"quality", "full_colour", Page::Quality, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::QualityFullColour,
         Text::QualityFullColourHint, ""},  // 2.5.18
        {"quality", "sharpen", Page::Quality, Control::Slider, 0, 100, 5, {}, {}, Text::QualitySharpen,
         Text::QualitySharpenHint, ""},
        {"display", "ui_filter", Page::Quality, Control::Choice, 0, 0, 0, {"sharp", "smooth"},
         {Text::UiSharp, Text::UiSmooth}, Text::QualityUiFilter, Text::QualityUiFilterHint, ""},
        {"display", "hdr", Page::Quality, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::QualityHdr,
         Text::QualityHdrHint, ""},
        {"display", "ui_native", Page::Quality, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::QualityUiNative,
         Text::QualityUiNativeHint, ""},
        {"display", "crisp_text", Page::Quality, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::QualityCrispText,
         Text::QualityCrispTextHint, ""},
        {"display", "window_art", Page::Quality, Control::Choice, 0, 0, 0, {"xbr", "ai", "pixels"},
         {Text::WindowArtXbr, Text::WindowArtAi, Text::WindowArtPixels}, Text::QualityWindowArt,
         Text::QualityWindowArtHint, ""},
        {"quality", "dlss", Page::Smoothing, Control::Choice, 0, 0, 0,
         {"off", "dlaa", "quality", "balanced", "performance", "ultra", "custom"},
         {Text::SmoothOff, Text::DlssDlaa, Text::DlssQuality, Text::DlssBalanced, Text::DlssPerformance,
          Text::DlssUltra, Text::DlssCustom},
         Text::SmoothDlss, Text::SmoothDlssHint, ""},
        // 2.4.2: DLSS at the player's own size and its network
        {"quality", "dlss_scale", Page::Smoothing, Control::Slider, 33, 100, 1, {}, {}, Text::DlssScale,
         Text::DlssScaleHint, " %"},
        {"quality", "dlss_width", Page::Smoothing, Control::Number, 0, 16384, 1, {}, {}, Text::DlssWidth,
         Text::DlssSizeHint, ""},
        {"quality", "dlss_height", Page::Smoothing, Control::Number, 0, 16384, 1, {}, {}, Text::DlssHeight,
         Text::DlssSizeHint, ""},
        {"quality", "dlss_model", Page::Smoothing, Control::Choice, 0, 0, 0, {"auto", "k", "j", "l", "m"},
         {Text::DlssModelAuto, Text::DlssModelK, Text::DlssModelJ, Text::DlssModelL, Text::DlssModelM},
         Text::DlssModel, Text::DlssModelHint, ""},
        {"quality", "taa", Page::Smoothing, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::SmoothTaa,
         Text::SmoothTaaHint, ""},
        {"quality", "msaa", Page::Smoothing, Control::Choice, 0, 0, 0, {"0", "2", "4", "8"},
         {Text::SmoothOff, Text::Factor2, Text::Factor4, Text::Factor8}, Text::SmoothMsaa, Text::SmoothMsaaHint, ""},
        {"quality", "alpha_to_coverage", Page::Smoothing, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::SmoothA2c,
         Text::SmoothA2cHint, ""},
        {"quality", "anisotropy", Page::Smoothing, Control::Choice, 0, 0, 0, {"0", "2", "4", "8", "16"},
         {Text::SmoothOff, Text::Factor2, Text::Factor4, Text::Factor8, Text::Factor16}, Text::SmoothAniso,
         Text::SmoothAnisoHint, ""},
        {"effects", "smooth_light", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxSmoothLight,
         Text::FxSmoothLightHint, "", &E::smoothLight},
        {"effects", "torch", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxTorch, Text::FxTorchHint,
         "", &E::torch},
        {"effects", "torch_flame", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxTorchFlame,
         Text::FxTorchFlameHint, "", &E::torchFlame},
        {"effects", "torch_brightness", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxTorchBrightness,
         Text::FxTorchBrightnessHint, " %", nullptr, &E::torchBrightness},
        {"effects", "torch_glow", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxTorchGlow,
         Text::FxTorchGlowHint, " %", nullptr, &E::torchGlow},
        {"effects", "torch_light", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxTorchLight,
         Text::FxTorchLightHint, " %", nullptr, &E::torchLight},
        {"effects", "torch_flicker", Page::Effects, Control::Slider, 0, 100, 10, {}, {}, Text::FxTorchFlicker,
         Text::FxTorchFlickerHint, " %", nullptr, &E::torchFlicker},
        {"effects", "smoke", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxSmoke, Text::FxSmokeHint,
         "", &E::smoke},
        {"effects", "smoke_density", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxSmokeDensity,
         Text::FxSmokeDensityHint, " %", nullptr, &E::smokeDensity},
        {"effects", "lamps", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxLamps, Text::FxLampsHint,
         "", &E::lamps},
        {"effects", "lamps_light", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxLampsLight,
         Text::FxLampsLightHint, " %", nullptr, &E::lampsLight},
        {"effects", "ao", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxAo, Text::FxAoHint, "",
         &E::ao},
        {"effects", "ao_strength", Page::Effects, Control::Slider, 0, 100, 10, {}, {}, Text::FxAoStrength,
         Text::FxAoStrengthHint, " %", nullptr, &E::aoStrength},
        {"effects", "ao_size", Page::Effects, Control::Slider, 0, 100, 10, {}, {}, Text::FxAoSize, Text::FxAoSizeHint,
         " %", nullptr, &E::aoSize},
        {"effects", "glow", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxGlow, Text::FxGlowHint,
         "", &E::glow},
        {"effects", "glow_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxGlowStrength,
         Text::FxGlowStrengthHint, " %", nullptr, &E::glowStrength},
        {"effects", "glow_size", Page::Effects, Control::Slider, 0, 100, 10, {}, {}, Text::FxGlowSize,
         Text::FxGlowSizeHint, " %", nullptr, &E::glowSize},
        {"effects", "glow_light", Page::Effects, Control::Slider, 0, 100, 10, {}, {}, Text::FxGlowLight,
         Text::FxGlowLightHint, " %", nullptr, &E::glowLight},
        {"effects", "fire_light", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxFireLight,
         Text::FxFireLightHint, "", &E::fireLight},
        {"effects", "fire_light_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {},
         Text::FxFireLightStrength, Text::FxFireLightStrengthHint, " %", nullptr, &E::fireLightStrength},
        {"effects", "skill_colours", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxSkillColours,
         Text::FxSkillColoursHint, "", &E::skillColours},
        {"effects", "light_orbs", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxLightOrbs,
         Text::FxLightOrbsHint, "", &E::lightOrbs},  // 2.7.25: a part of fire_light
        {"effects", "aura", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxAura, Text::FxAuraHint,
         "", &E::aura},
        {"effects", "aura_brightness", Page::Effects, Control::Slider, 0, 100, 10, {}, {}, Text::FxAuraBrightness,
         Text::FxAuraBrightnessHint, " %", nullptr, &E::auraBrightness},
        {"effects", "skills_soft", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxSkillsSoft,
         Text::FxSkillsSoftHint, "", &E::skillsSoft},
        {"effects", "skill_brightness", Page::Effects, Control::Slider, 0, 100, 5, {}, {}, Text::FxSkillBrightness,
         Text::FxSkillBrightnessHint, " %", nullptr, &E::skillBrightness},
        {"effects", "fireflies", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxFireflies,
         Text::FxFirefliesHint, "", &E::fireflies},
        {"effects", "fireflies_brightness", Page::Effects, Control::Slider, 0, 200, 10, {}, {},
         Text::FxFirefliesBrightness, Text::FxFirefliesBrightnessHint, " %", nullptr, &E::firefliesBrightness},
        {"effects", "firefly_dots", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxFireflyDots,
         Text::FxFireflyDotsHint, "", &E::fireflyDots},  // 3.0.7-beta.3: a part of fireflies
        {"effects", "map_glows", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxMapGlows,
         Text::FxMapGlowsHint, "", &E::mapGlows},
        {"effects", "map_glows_brightness", Page::Effects, Control::Slider, 0, 200, 10, {}, {},
         Text::FxMapGlowsBrightness, Text::FxMapGlowsBrightnessHint, " %", nullptr, &E::mapGlowsBrightness},
        {"effects", "map_glow_orbs", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxMapGlowOrbs,
         Text::FxMapGlowOrbsHint, "", &E::mapGlowOrbs},  // 3.0.7-beta.1: a part of map_glows
        {"effects", "rays", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxRays, Text::FxRaysHint,
         "", &E::rays},
        {"effects", "rays_brightness", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxRaysBrightness,
         Text::FxRaysBrightnessHint, " %", nullptr, &E::raysBrightness},
        {"effects", "lava", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxLava, Text::FxLavaHint,
         "", &E::lava},
        {"effects", "lava_brightness", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxLavaBrightness,
         Text::FxLavaBrightnessHint, " %", nullptr, &E::lavaBrightness},
        {"effects", "actor_shadows", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxActorShadows,
         Text::FxActorShadowsHint, "", &E::actorShadows},
        {"effects", "actor_shadows_strength", Page::Effects, Control::Slider, 0, 100, 5, {}, {},
         Text::FxActorShadowsStrength, Text::FxActorShadowsStrengthHint, " %", nullptr, &E::actorShadowsStrength},
        {"effects", "actor_shadows_spot", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::FxActorShadowsSpot, Text::FxActorShadowsSpotHint, "", &E::actorShadowsSpot},  // 2.6.15
        {"effects", "heat", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxHeat, Text::FxHeatHint,
         "", &E::heat},
        {"effects", "heat_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxHeatStrength,
         Text::FxHeatStrengthHint, " %", nullptr, &E::heatStrength},
        {"effects", "portals", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxPortals,
         Text::FxPortalsHint, "", &E::portals},
        {"effects", "portals_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxPortalsStrength,
         Text::FxPortalsStrengthHint, " %", nullptr, &E::portalsStrength},
        {"effects", "portal_new", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxPortalNew,
         Text::FxPortalNewHint, "", &E::portalNew},  // 2.7.48: a part of portals
        {"effects", "weather", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxWeather,
         Text::FxWeatherHint, "", &E::weather},
        {"effects", "weather_amount", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxWeatherAmount,
         Text::FxWeatherAmountHint, " %", nullptr, &E::weatherAmount},
        {"effects", "grade", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxGrade, Text::FxGradeHint,
         "", &E::grade},
        {"effects", "grade_saturation", Page::Effects, Control::Slider, 0, 200, 5, {}, {}, Text::FxGradeSaturation,
         Text::FxGradeSaturationHint, " %", nullptr, &E::gradeSaturation},
        {"effects", "grade_contrast", Page::Effects, Control::Slider, 50, 150, 2, {}, {}, Text::FxGradeContrast,
         Text::FxGradeContrastHint, " %", nullptr, &E::gradeContrast},
        {"effects", "grade_brightness", Page::Effects, Control::Slider, 50, 150, 2, {}, {}, Text::FxGradeBrightness,
         Text::FxGradeBrightnessHint, " %", nullptr, &E::gradeBrightness},
        {"effects", "grade_warmth", Page::Effects, Control::Slider, 0, 100, 5, {}, {}, Text::FxGradeWarmth,
         Text::FxGradeWarmthHint, " %", nullptr, &E::gradeWarmth},
        {"effects", "grade_vignette", Page::Effects, Control::Slider, 0, 100, 5, {}, {}, Text::FxGradeVignette,
         Text::FxGradeVignetteHint, " %", nullptr, &E::gradeVignette},
        {"effects", "foliage", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxFoliage,
         Text::FxFoliageHint, "", &E::foliage},
        {"effects", "foliage_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxFoliageStrength,
         Text::FxFoliageStrengthHint, " %", nullptr, &E::foliageStrength},
        {"effects", "soft_particles", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxSoftParticles,
         Text::FxSoftParticlesHint, "", &E::softParticles},
        {"effects", "rim_light", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxRimLight,
         Text::FxRimLightHint, "", &E::rimLight},
        {"effects", "rim_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxRimStrength,
         Text::FxRimStrengthHint, " %", nullptr, &E::rimStrength},
        {"effects", "motes", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxMotes,
         Text::FxMotesHint, "", &E::motes},
        {"effects", "motes_amount", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxMotesAmount,
         Text::FxMotesAmountHint, " %", nullptr, &E::motesAmount},
        {"effects", "relief", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxRelief,
         Text::FxReliefHint, "", &E::relief},
        {"effects", "relief_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxReliefStrength,
         Text::FxReliefStrengthHint, " %", nullptr, &E::reliefStrength},
        {"effects", "floor_reflections", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::FxFloorReflections, Text::FxFloorReflectionsHint, "", &E::floorReflections},
        {"effects", "floor_reflect_strength", Page::Effects, Control::Slider, 0, 100, 5, {}, {},
         Text::FxFloorReflectStrength, Text::FxFloorReflectStrengthHint, " %", nullptr, &E::floorReflectStrength},
        {"effects", "shafts", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxShafts,
         Text::FxShaftsHint, "", &E::shafts},
        {"effects", "shafts_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxShaftsStrength,
         Text::FxShaftsStrengthHint, " %", nullptr, &E::shaftsStrength},
        {"effects", "shafts_lamps", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxShaftsLamps,
         Text::FxShaftsLampsHint, "", &E::shaftsLamps},  // 3.0.7-beta.5: a part of shafts
        {"effects", "day_cycle", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxDayCycle,
         Text::FxDayCycleHint, "", &E::dayCycle},
        {"effects", "bloom", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxBloom,
         Text::FxBloomHint, "", &E::bloom},
        {"effects", "bloom_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxBloomStrength,
         Text::FxBloomStrengthHint, " %", nullptr, &E::bloomStrength},
        {"effects", "region_looks", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxRegionLooks,
         Text::FxRegionLooksHint, "", &E::regionLooks},
        {"effects", "region_looks_strength", Page::Effects, Control::Slider, 0, 100, 10, {}, {},
         Text::FxRegionLooksStrength, Text::FxRegionLooksStrengthHint, " %", nullptr, &E::regionLooksStrength},
        {"effects", "footprints", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxFootprints,
         Text::FxFootprintsHint, "", &E::footprints},
        {"effects", "storms", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxStorms,
         Text::FxStormsHint, "", &E::storms},
        {"effects", "grass", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxGrass,
         Text::FxGrassHint, "", &E::grass},
        {"effects", "grass_amount", Page::Effects, Control::Slider, 0, 100, 5, {}, {}, Text::FxGrassAmount,
         Text::FxGrassAmountHint, " %", nullptr, &E::grassAmount},
        {"effects", "skill_sparks", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxSkillSparks,
         Text::FxSkillSparksHint, "", &E::skillSparks},
        {"effects", "skill_sparks_amount", Page::Effects, Control::Slider, 0, 200, 10, {}, {},
         Text::FxSkillSparksAmount, Text::FxSkillSparksAmountHint, " %", nullptr, &E::skillSparksAmount},
        {"effects", "map_fade", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxMapFade,
         Text::FxMapFadeHint, "", &E::mapFade},
        {"effects", "no_game_fog", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxNoGameFog,
         Text::FxNoGameFogHint, "", &E::noGameFog},  // 2.6.10
        {"effects", "no_server_night", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxNoServerNight,
         Text::FxNoServerNightHint, "", &E::noServerNight},  // 2.6.13
        {"effects", "moon_light", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxMoonLight,
         Text::FxMoonLightHint, "", &E::moonLight},  // 2.7.18
        {"effects", "moon_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxMoonStrength,
         Text::FxMoonStrengthHint, " %", nullptr, &E::moonStrength},
        {"effects", "fireflies_light", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxFirefliesLight,
         Text::FxFirefliesLightHint, "", &E::firefliesLight},  // 2.7.18: a part of night_fireflies
        {"effects", "moon_beams", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxMoonBeams,
         Text::FxMoonBeamsHint, "", &E::moonBeams},  // 2.7.20: a part of moon_light
        {"effects", "round_light", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxRoundLight,
         Text::FxRoundLightHint, "", &E::roundLight},
        {"effects", "ripples", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxRipples,
         Text::FxRipplesHint, "", &E::ripples},
        {"effects", "glint", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxGlint,
         Text::FxGlintHint, "", &E::glint},
        {"effects", "hit_flash", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxHitFlash,
         Text::FxHitFlashHint, "", &E::hitFlash},
        {"effects", "wet_ground", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxWetGround,
         Text::FxWetGroundHint, "", &E::wetGround},
        {"effects", "tree_volume", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxTreeVolume,
         Text::FxTreeVolumeHint, "", &E::treeVolume},
        {"effects", "leaf_light", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxLeafLight,
         Text::FxLeafLightHint, "", &E::leafLight},
        {"effects", "tree_tints", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxTreeTints,
         Text::FxTreeTintsHint, "", &E::treeTints},
        {"effects", "flowers", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxFlowers,
         Text::FxFlowersHint, "", &E::flowers},
        {"effects", "dew", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxDew,
         Text::FxDewHint, "", &E::dew},
        {"effects", "morning_fog", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxMorningFog,
         Text::FxMorningFogHint, "", &E::morningFog},
        {"effects", "sand_drift", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxSandDrift,
         Text::FxSandDriftHint, "", &E::sandDrift},
        {"effects", "wet_walls", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxWetWalls,
         Text::FxWetWallsHint, "", &E::wetWalls},
        {"effects", "swamp", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxSwamp,
         Text::FxSwampHint, "", &E::swamp},
        {"effects", "butterflies", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxButterflies,
         Text::FxButterfliesHint, "", &E::butterflies},
        {"effects", "birds", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxBirds,
         Text::FxBirdsHint, "", &E::birds},
        {"effects", "night_fireflies", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::FxNightFireflies, Text::FxNightFirefliesHint, "", &E::nightFireflies},  // 2.6.11
        {"effects", "leaf_fall", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxLeafFall,
         Text::FxLeafFallHint, "", &E::leafFall},
        {"effects", "embers", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxEmbers,
         Text::FxEmbersHint, "", &E::embers},
        {"effects", "splashes", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxSplashes,
         Text::FxSplashesHint, "", &E::splashes},
        {"effects", "cave_drips", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxCaveDrips,
         Text::FxCaveDripsHint, "", &E::caveDrips},
        {"effects", "underwater", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxUnderwater,
         Text::FxUnderwaterHint, "", &E::underwater},
        // 2.2: the set for the card, the groups and their strength (core/effect_groups.hpp)
        {"effects", "preset", Page::Effects, Control::Choice, 0, 0, 0,
         {"auto", "off", "weak", "medium", "strong", "custom"},
         {Text::FxPresetAuto, Text::FxPresetOff, Text::FxPresetWeak, Text::FxPresetMedium, Text::FxPresetStrong,
          Text::FxPresetCustom},
         Text::FxPreset, Text::FxPresetHint, ""},
        // 2.6.22: the groups by what the eye is on (core/effect_groups.hpp), a strength where they have one
        {"effects", "group_time", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxGroupTime,
         Text::FxGroupTimeHint, ""},
        {"effects", "group_weather", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxGroupWeather,
         Text::FxGroupWeatherHint, ""},
        {"effects", "group_weather_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {},
         Text::FxGroupStrength, Text::FxGroupStrengthHint, " %"},
        {"effects", "group_characters", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxGroupCharacters,
         Text::FxGroupCharactersHint, ""},
        {"effects", "group_characters_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {},
         Text::FxGroupStrength, Text::FxGroupStrengthHint, " %"},
        {"effects", "group_skill_effects", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::FxGroupSkillEffects, Text::FxGroupSkillEffectsHint, ""},
        {"effects", "group_skill_effects_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {},
         Text::FxGroupStrength, Text::FxGroupStrengthHint, " %"},
        {"effects", "group_fire", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxGroupFire,
         Text::FxGroupFireHint, ""},
        {"effects", "group_fire_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {},
         Text::FxGroupStrength, Text::FxGroupStrengthHint, " %"},
        {"effects", "group_lights", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxGroupLights,
         Text::FxGroupLightsHint, ""},
        {"effects", "group_lights_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {},
         Text::FxGroupStrength, Text::FxGroupStrengthHint, " %"},
        {"effects", "group_ground", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxGroupGround,
         Text::FxGroupGroundHint, ""},
        {"effects", "group_ground_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {},
         Text::FxGroupStrength, Text::FxGroupStrengthHint, " %"},
        {"effects", "group_plants", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxGroupPlants,
         Text::FxGroupPlantsHint, ""},
        {"effects", "group_plants_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {},
         Text::FxGroupStrength, Text::FxGroupStrengthHint, " %"},
        {"effects", "group_waters", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxGroupWaters,
         Text::FxGroupWatersHint, ""},
        {"effects", "group_waters_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {},
         Text::FxGroupStrength, Text::FxGroupStrengthHint, " %"},
        {"effects", "group_creatures", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxGroupCreatures,
         Text::FxGroupCreaturesHint, ""},
        {"effects", "group_caves", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxGroupCaves,
         Text::FxGroupCavesHint, ""},
        {"effects", "group_sky", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxGroupSky,
         Text::FxGroupSkyHint, ""},
        {"effects", "group_sky_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {},
         Text::FxGroupStrength, Text::FxGroupStrengthHint, " %"},
        {"effects", "group_screen", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxGroupScreen,
         Text::FxGroupScreenHint, ""},
        {"effects", "group_screen_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {},
         Text::FxGroupStrength, Text::FxGroupStrengthHint, " %"},
        {"effects", "group_game_fixes", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxGroupGameFixes,
         Text::FxGroupGameFixesHint, ""},
        {"effects", "tier", Page::Effects, Control::Choice, 0, 0, 0, {"auto", "classic", "remaster"},
         {Text::FxTierAuto, Text::FxTierClassic, Text::FxTierRemaster}, Text::FxTier, Text::FxTierHint, ""},
        // 2.6.11: the time of day for all that goes by the hour (core/daylight.hpp Clock)
        {"effects", "clock", Page::Effects, Control::Choice, 0, 0, 0,
         {"auto", "server", "dawn", "morning", "day", "sunset", "night", "deep_night"},  // 2.6.19, 2.7.16
         {Text::FxClockAuto, Text::FxClockServer, Text::FxClockDawn, Text::FxClockMorning, Text::FxClockDay,
          Text::FxClockSunset, Text::FxClockNight, Text::FxClockDeepNight},
         Text::FxClock, Text::FxClockHint, ""},
        // 2.6.22: the season for all that goes by the year (core/seasons.hpp Choice)
        {"effects", "season", Page::Effects, Control::Choice, 0, 0, 0, {"auto", "spring", "summer", "autumn", "winter"},
         {Text::FxSeasonAuto, Text::FxSeasonSpring, Text::FxSeasonSummer, Text::FxSeasonAutumn, Text::FxSeasonWinter},
         Text::FxSeason, Text::FxSeasonHint, ""},
        // 2.6.22: the weather for all that goes by it (core/weather.hpp Choice)
        {"effects", "weather_kind", Page::Effects, Control::Choice, 0, 0, 0,
         {"auto", "clear", "rain", "snow", "leaves", "sand"},
         {Text::FxWeatherKindAuto, Text::FxWeatherKindClear, Text::FxWeatherKindRain, Text::FxWeatherKindSnow,
          Text::FxWeatherKindLeaves, Text::FxWeatherKindSand},
         Text::FxWeatherKind, Text::FxWeatherKindHint, ""},
        {"effects", "materials", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxMaterials,
         Text::FxMaterialsHint, "", &E::materials},
        {"effects", "materials_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {}, Text::FxMaterialsStrength,
         Text::FxMaterialsStrengthHint, " %", nullptr, &E::materialsStrength},
        {"effects", "ground_seams", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxGroundSeams,
         Text::FxGroundSeamsHint, "", &E::groundSeams},
        {"effects", "smooth_hills", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxSmoothHills,
         Text::FxSmoothHillsHint, "", &E::smoothHills},
        {"effects", "smooth_hills_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {},
         Text::FxSmoothHillsStrength, Text::FxSmoothHillsStrengthHint, " %", nullptr, &E::smoothHillsStrength},
        {"effects", "own_ground", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxOwnGround,
         Text::FxOwnGroundHint, "", &E::ownGround},  // 3.0.4-beta.10: the own world's prototype
        {"effects", "own_far_models", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxOwnFarModels,
         Text::FxOwnFarModelsHint, "", &E::ownFarModels},  // 3.0.4-beta.14: a part of own_ground
        {"effects", "world_reflections", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {},
         Text::FxWorldReflections, Text::FxWorldReflectionsHint, "", &E::worldReflections},
        {"effects", "sprite_light", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxSpriteLight,
         Text::FxSpriteLightHint, "", &E::spriteLight},
        {"effects", "sprite_light_strength", Page::Effects, Control::Slider, 0, 200, 10, {}, {},
         Text::FxSpriteLightStrength, Text::FxSpriteLightStrengthHint, " %", nullptr, &E::spriteLightStrength},
        {"effects", "seasons", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxSeasons,
         Text::FxSeasonsHint, "", &E::seasons},
        {"effects", "rain_wet", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxRainWet,
         Text::FxRainWetHint, "", &E::rainWet},
        {"effects", "night_windows", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxNightWindows,
         Text::FxNightWindowsHint, "", &E::nightWindows},
        {"effects", "shared_wind", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxSharedWind,
         Text::FxSharedWindHint, "", &E::sharedWind},
        {"effects", "lit_dust", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxLitDust,
         Text::FxLitDustHint, "", &E::litDust},
        // 2.4: the parts of the effects, each a box under its effect (the water's foam, glints, ...)
        {"effects", "water_foam", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxWaterFoam,
         Text::FxWaterFoamHint, "", &E::waterFoam},
        {"effects", "water_glints", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxWaterGlints,
         Text::FxWaterGlintsHint, "", &E::waterGlints},
        {"effects", "caustics", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxCaustics,
         Text::FxCausticsHint, "", &E::caustics},
        {"effects", "waterfalls", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxWaterfalls,
         Text::FxWaterfallsHint, "", &E::waterfalls},
        {"effects", "step_dust", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxStepDust,
         Text::FxStepDustHint, "", &E::stepDust},
        {"effects", "season_snow", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxSeasonSnow,
         Text::FxSeasonSnowHint, "", &E::seasonSnow},
        {"effects", "season_grass", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxSeasonGrass,
         Text::FxSeasonGrassHint, "", &E::seasonGrass},
        {"effects", "pollen", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxPollen,
         Text::FxPollenHint, "", &E::pollen},
        {"effects", "indoor_dust", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxIndoorDust,
         Text::FxIndoorDustHint, "", &E::indoorDust},
        {"effects", "rain_splashes", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxRainSplashes,
         Text::FxRainSplashesHint, "", &E::rainSplashes},
        {"effects", "desert_heat", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxDesertHeat,
         Text::FxDesertHeatHint, "", &E::desertHeat},
        {"effects", "sky_rays", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxSkyRays,
         Text::FxSkyRaysHint, "", &E::skyRays},
        {"effects", "tree_seasons", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxTreeSeasons,
         Text::FxTreeSeasonsHint, "", &E::treeSeasons},
        {"effects", "dragonflies", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxDragonflies,
         Text::FxDragonfliesHint, "", &E::dragonflies},
        {"effects", "night_strength", Page::Effects, Control::Slider, 0, 100, 5, {}, {}, Text::FxNightStrength,
         Text::FxNightStrengthHint, " %", nullptr, &E::nightStrength},
        {"effects", "water", Page::Effects, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FxWater, Text::FxWaterHint,
         "", &E::water},
        {"effects", "water_waves", Page::Effects, Control::Slider, 0, 100, 10, {}, {}, Text::FxWaterWaves,
         Text::FxWaterWavesHint, " %", nullptr, &E::waterWaves},
        {"effects", "water_reflections", Page::Effects, Control::Slider, 0, 100, 10, {}, {},
         Text::FxWaterReflections, Text::FxWaterReflectionsHint, " %", nullptr, &E::waterReflections},
        {"effects", "water_clarity", Page::Effects, Control::Slider, 0, 100, 10, {}, {}, Text::FxWaterClarity,
         Text::FxWaterClarityHint, " %", nullptr, &E::waterClarity},
        {"ai", "enabled", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiEnable, Text::AiEnableHint, ""},
        {"ai", "ground", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiGround, Text::AiGroundHint, ""},
        {"ai", "objects", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiObjects, Text::AiObjectsHint,
         ""},
        {"ai", "monsters", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiSprites, Text::AiSpritesHint,
         ""},
        {"ai", "players", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiPlayers, Text::AiPlayersHint, ""},
        {"ai", "npcs", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiNpcs, Text::AiNpcsHint, ""},
        {"ai", "items", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiItems, Text::AiItemsHint, ""},
        {"ai", "dyes", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiDyes, Text::AiDyesHint, ""},
        {"ai", "effects", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiEffects, Text::AiEffectsHint,
         ""},
        {"ai", "icons", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiIcons, Text::AiIconsHint, ""},
        {"ai", "ui_pieces", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiUiPieces, Text::AiUiPiecesHint,
         ""},
        {"ai", "model", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiModelWorld, Text::AiModelWorldHint, ""},
        {"ai", "sprite_model", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiModelSprites, Text::AiModelSpritesHint, ""},
        // 1.6.3: small pictures by a model of their own
        {"ai", "small_pictures", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiSmall, Text::AiSmallHint,
         ""},
        {"ai", "small_side", Page::Ai, Control::Number, 16, 256, 1, {}, {}, Text::AiSmallSide, Text::AiSmallHint, ""},
        {"ai", "small_model", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi}, Text::AiSmallModel,
         Text::AiSmallHint, ""},
        // 1.6: a model for each group of pictures; 2.5.28: 38 of them (core/ai/groups.hpp)
        {"ai", "model_ground_town", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupGroundTown, Text::AiGroupModelHint, ""},
        {"ai", "model_ground_field", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupGroundField, Text::AiGroupModelHint, ""},
        {"ai", "model_ground_dungeon", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupGroundDungeon, Text::AiGroupModelHint, ""},
        {"ai", "model_ground_indoor", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupGroundIndoor, Text::AiGroupModelHint, ""},
        {"ai", "model_ground_other", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupGroundOther, Text::AiGroupModelHint, ""},
        {"ai", "model_buildings_town", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupBuildingsTown, Text::AiGroupModelHint, ""},
        {"ai", "model_models_field", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupModelsField, Text::AiGroupModelHint, ""},
        {"ai", "model_models_dungeon", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupModelsDungeon, Text::AiGroupModelHint, ""},
        {"ai", "model_furniture", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupFurniture, Text::AiGroupModelHint, ""},
        {"ai", "model_models_other", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupModelsOther, Text::AiGroupModelHint, ""},
        {"ai", "model_trees", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupTrees, Text::AiGroupModelHint, ""},
        {"ai", "model_plants", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupPlants, Text::AiGroupModelHint, ""},
        {"ai", "model_water", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupWater, Text::AiGroupModelHint, ""},
        {"ai", "model_bodies", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupBodies, Text::AiGroupModelHint, ""},
        {"ai", "model_mounted", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupMounted, Text::AiGroupModelHint, ""},
        {"ai", "model_heads", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupHeads, Text::AiGroupModelHint, ""},
        {"ai", "model_weapons", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupWeapons, Text::AiGroupModelHint, ""},
        {"ai", "model_shields", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupShields, Text::AiGroupModelHint, ""},
        {"ai", "model_headgear", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupHeadgear, Text::AiGroupModelHint, ""},
        {"ai", "model_garments", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupGarments, Text::AiGroupModelHint, ""},
        {"ai", "model_monsters", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupMonsters, Text::AiGroupModelHint, ""},
        {"ai", "model_bosses", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupBosses, Text::AiGroupModelHint, ""},
        {"ai", "model_homunculi", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupHomunculi, Text::AiGroupModelHint, ""},
        {"ai", "model_npcs", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupNpcs, Text::AiGroupModelHint, ""},
        {"ai", "model_items_ground", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupItems, Text::AiGroupModelHint, ""},
        {"ai", "model_item_icons", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupItemIcons, Text::AiGroupModelHint, ""},
        {"ai", "model_item_pictures", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupItemPictures, Text::AiGroupModelHint, ""},
        {"ai", "model_card_pictures", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupCardPictures, Text::AiGroupModelHint, ""},
        {"ai", "model_skill_animations", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupSkillAnimations, Text::AiGroupModelHint, ""},
        {"ai", "model_effect_sprites", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupEffectSprites, Text::AiGroupModelHint, ""},
        {"ai", "model_emotions", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupEmotions, Text::AiGroupModelHint, ""},
        {"ai", "model_cursor", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupCursor, Text::AiGroupModelHint, ""},
        {"ai", "model_windows", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupWindows, Text::AiGroupModelHint, ""},
        {"ai", "model_menu_icons", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupMenuIcons, Text::AiGroupModelHint, ""},
        {"ai", "model_portraits", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupPortraits, Text::AiGroupModelHint, ""},
        {"ai", "model_maps", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupMaps, Text::AiGroupModelHint, ""},
        {"ai", "model_loading", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupLoading, Text::AiGroupModelHint, ""},
        {"ai", "model_login", Page::Ai, Control::Choice, 0, 0, 0, {"general", "anime", "anime6b", "gameai"},
         {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b, Text::AiModelGameAi},
         Text::AiGroupLogin, Text::AiGroupModelHint, ""},
        {"ai", "denoise", Page::Ai, Control::Slider, 0, 100, 5, {}, {}, Text::AiDenoise, Text::AiDenoiseHint, " %"},
        {"ai", "scale", Page::Ai, Control::Choice, 0, 0, 0, {"4", "2"}, {Text::AiScale4, Text::AiScale2},
         Text::AiScale, Text::AiScaleHint, ""},
        {"ai", "budget", Page::Ai, Control::Number, 1, 20, 1, {}, {}, Text::AiBudget, Text::AiBudgetHint, ""},
        {"ai", "min_size", Page::Ai, Control::Number, 16, 1024, 1, {}, {}, Text::AiSizes, Text::AiSizesHint, ""},
        {"ai", "max_size", Page::Ai, Control::Number, 16, 1024, 1, {}, {}, Text::AiSizes, Text::AiSizesHint, ""},
        {"ai", "compress", Page::Ai, Control::Choice, 0, 0, 0, {"auto", "on", "off"},
         {Text::AiCompressAuto, Text::AiCompressOn, Text::AiCompressOff}, Text::AiCompress, Text::AiCompressHint, ""},
        {"ai", "ahead", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiAhead, Text::AiAheadHint, ""},
        {"ai", "generate", Page::Ai, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::AiGenerate, Text::AiGenerateHint,
         ""},
        {"frames", "vsync", Page::Frames, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FramesVsync,
         Text::FramesVsyncHint, ""},
        {"frames", "frame_generation", Page::Frames, Control::Choice, 0, 0, 0, {"off", "2x", "3x", "4x"},
         {Text::SmoothOff, Text::Factor2, Text::Times3, Text::Factor4}, Text::FramesGeneration,
         Text::FramesGenerationHint, ""},
        {"frames", "reflex", Page::Frames, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FramesReflex,
         Text::FramesReflexHint, ""},  // 2.5.26
        {"frames", "fps_counter", Page::Frames, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FramesFpsCounter,
         Text::FramesFpsCounterHint, ""},
        {"frames", "fps_limit", Page::Frames, Control::Number, 0, 1000, 1, {}, {}, Text::FramesFpsLimit,
         Text::FramesFpsLimitHint, ""},
        {"frames", "save_power", Page::Frames, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FramesSavePower,
         Text::FramesSavePowerHint, ""},
        {"frames", "battery_saver", Page::Frames, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FramesBattery,
         Text::FramesBatteryHint, ""},  // 2.5.20
        {"windows", "others_light", Page::Frames, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::WindowsOthersLight,
         Text::WindowsOthersLightHint, ""},  // 3.0.3-beta.3: several game windows
        {"frames", "battery_fps", Page::Frames, Control::Slider, 15, 60, 5, {}, {}, Text::FramesBatteryFps,
         Text::FramesBatteryFpsHint, ""},
        {"frames", "keep_fps", Page::Frames, Control::Number, 0, 240, 1, {}, {}, Text::FramesKeepFps,
         Text::FramesKeepFpsHint, ""},
        {"frames", "allow_tearing", Page::Frames, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FramesTearing,
         Text::FramesTearingHint, ""},
        // 0.91: the game's sound through the driver
        {"sound", "rate", Page::Sound, Control::Choice, 0, 0, 0, {"0", "44100", "48000"},
         {Text::GfxSoundRateGame, Text::GfxSoundRate44, Text::GfxSoundRate48}, Text::GfxSoundRate,
         Text::GfxSoundRateHint, ""},
        {"sound", "voices", Page::Sound, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::GfxSoundVoices,
         Text::GfxSoundVoicesHint, ""},
        {"sound", "background", Page::Sound, Control::Slider, 0, 100, 5, {}, {}, Text::GfxSoundBackground,
         Text::GfxSoundBackgroundHint, "%"},
        {"windows", "others_music", Page::Sound, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::WindowsOthersMusic,
         Text::WindowsOthersMusicHint, ""},  // 3.0.3-beta.3: several game windows
        {"windows", "others_sounds", Page::Sound, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::WindowsOthersSounds,
         Text::WindowsOthersSoundsHint, ""},  // 3.0.3-beta.9
        {"sound", "limiter", Page::Sound, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::GfxSoundLimiter,
         Text::GfxSoundLimiterHint, ""},
        {"sound", "stereo", Page::Sound, Control::Slider, 0, 100, 5, {}, {}, Text::GfxSoundStereo,
         Text::GfxSoundStereoHint, "%"},  // 0.92
        {"sound", "steal", Page::Sound, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::GfxSoundSteal,
         Text::GfxSoundStealHint, ""},
        {"sound", "crossfade", Page::Sound, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::GfxSoundCrossfade,
         Text::GfxSoundCrossfadeHint, ""},
        {"sound", "engine", Page::Sound, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::GfxSoundEngine,
         Text::GfxSoundEngineHint, ""},  // 1.1
        {"sound", "reverb", Page::Sound, Control::Slider, 0, 100, 5, {}, {}, Text::GfxSoundReverb,
         Text::GfxSoundReverbHint, "%"},
        {"sound", "headphones", Page::Sound, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::GfxSoundHeadphones,
         Text::GfxSoundHeadphonesHint, ""},  // 1.2
        {"sound", "duck", Page::Sound, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::GfxSoundDuck,
         Text::GfxSoundDuckHint, ""},  // 1.3
        // 3.0.4-beta.7: the maps' own sounds
        {"sound", "ambience", Page::Sound, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::GfxSoundAmbience,
         Text::GfxSoundAmbienceHint, ""},
        {"sound", "ambience_volume", Page::Sound, Control::Slider, 0, 100, 5, {}, {}, Text::GfxSoundAmbienceVolume,
         Text::GfxSoundAmbienceVolumeHint, "%"},
        // 3.0.4-beta.1: the output device - the page's own list of Windows' devices (app/page_settings.cpp)
        {"sound", "output", Page::Sound, Control::Line, 0, 0, 0, {}, {}, Text::SoundOutput, Text::SoundOutputHint,
         ""},
        // 2.6: the Debug page - the log, the profiler and X-ray from the Diagnostics page, then the test switches
        // the ini alone held before
        {"debug", "log", Page::Debug, Control::Check, 0, 1, 1, {"normal", "verbose"}, {}, Text::DiagVerboseLog,
         Text::DiagVerboseLogHint, ""},
        {"debug", "profile", Page::Debug, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::DiagProfile,
         Text::DiagProfileHint, ""},
        {"debug", "xray", Page::Debug, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::DiagXray,
         Text::DiagXrayHint, ""},  // 2.3
        {"debug", "seams_view", Page::Debug, Control::Choice, 0, 0, 0, {"0", "1", "2"},  // 2.7.8
         {Text::DebugSeamsOff, Text::DebugSeamsColour, Text::DebugSeamsOnTop}, Text::DebugSeams,
         Text::DebugSeamsHint, ""},
        {"debug", "weak_card", Page::Debug, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::DebugWeakCard,
         Text::DebugWeakCardHint, ""},  // 3.0.4-beta.35
        {"quality", "dlss_jitter", Page::Debug, Control::Check, 0, 1, 1, {"normal", "inverted"}, {},
         Text::DebugDlssJitter, Text::DebugDlssJitterHint, ""},
        {"quality", "dlss_motion", Page::Debug, Control::Check, 0, 1, 1, {"normal", "inverted"}, {},
         Text::DebugDlssMotion, Text::DebugDlssMotionHint, ""},
        {"renderer", "bridge_present", Page::Debug, Control::Choice, 0, 0, 0, {"auto", "window", "composition"},
         {Text::DebugPresentAuto, Text::DebugPresentWindow, Text::DebugPresentComposition}, Text::DebugPresent,
         Text::DebugPresentHint, ""},
        {"renderer", "bridge_dpi_scale", Page::Debug, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::DebugDpiScale,
         Text::DebugDpiScaleHint, ""},
        {"debug", "shader_variants", Page::Debug, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::DebugVariants,
         Text::DebugVariantsHint, ""},
        {"debug", "state_cache", Page::Debug, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::DebugStateCache,
         Text::DebugStateCacheHint, ""},
        {"debug", "merge_draws", Page::Debug, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::DebugMergeDraws,
         Text::DebugMergeDrawsHint, ""},
        {"debug", "fast_pick", Page::Debug, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::DebugFastPick,
         Text::DebugFastPickHint, ""},
        {"debug", "fast_ftol", Page::Debug, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::DebugFastFtol,
         Text::DebugFastFtolHint, ""},
        {"debug", "fast_textures", Page::Debug, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::DebugFastTextures,
         Text::DebugFastTexturesHint, ""},
    };
    return list;
}

const IniSetting* findSetting(const std::string& sectionKey) {
    // 2.2.2: by an index made once - parseConfig asks for each [effects] line now
    static const std::unordered_map<std::string, const IniSetting*> index = [] {
        std::unordered_map<std::string, const IniSetting*> m;
        for (const IniSetting& s : iniSettings()) m.emplace(std::string(s.section) + "." + s.key, &s);
        return m;
    }();
    const auto it = index.find(sectionKey);
    return it == index.end() ? nullptr : it->second;
}

std::string sectionKey(const IniSetting& s) { return std::string(s.section) + "." + s.key; }

std::string tokenOf(const IniSetting& s, const Config& c) {
    if (s.flag) return c.effects.*s.flag ? s.tokens[1] : s.tokens[0];  // 2.2.2: [effects] by the entry's field
    if (s.number) return std::to_string(c.effects.*s.number);
    const std::string k = sectionKey(s);
    if (k == "renderer.api") return apiToken(c.api);
    if (k == "renderer.gpu") return c.gpu.empty() ? "auto" : c.gpu;
    if (k == "report.auto") return c.reportAuto ? "1" : "0";
    if (k == "pad.enabled") return c.padEnabled ? "1" : "0";  // 2.9
    if (k == "launcher.direct_start") return c.directStart ? "1" : "0";  // 2.11
    if (k == "client.cps") return c.clientCps ? "1" : "0";  // 3.0.1-beta.5
    if (k == "client.bink") return c.clientBink ? "1" : "0";
    if (k == "client.ijl") return c.clientIjl ? "1" : "0";  // 3.0.1-beta.6
    if (k == "client.mss") return c.clientMss ? "1" : "0";  // 3.0.1-beta.7
    if (k == "client.roext") return c.clientRoext ? "1" : "0";  // 3.0.1-beta.8
    if (k == "client.updates") return c.clientUpdates ? "1" : "0";  // 3.0.2-beta.5
    if (k == "roext.window_lock") return c.roext.windowLock ? "1" : "0";
    if (k == "roext.window_on_top") return c.roext.windowOnTop ? "1" : "0";
    if (k == "roext.mouse_buttons") return c.roext.mouseButtons ? "1" : "0";
    if (k == "roext.alt_f4") return c.roext.altF4 ? "1" : "0";
    if (k == "roext.minimize_key") return c.roext.minimizeKey ? "1" : "0";
    if (k == "roext.free_cpu") return std::to_string(c.roext.freeCpu);
    if (k == "roext.code_page") return std::to_string(c.roext.codePage);
    if (k == "roext.window_x") return std::to_string(c.roext.windowX);
    if (k == "roext.window_y") return std::to_string(c.roext.windowY);
    if (k == "windows.title_name") return c.windowTitleName ? "1" : "0";  // 3.0.3-beta.2
    if (k == "windows.remember_place") return c.windowRememberPlace ? "1" : "0";
    if (k == "windows.others_light") return c.windowOthersLight ? "1" : "0";  // 3.0.3-beta.3
    if (k == "windows.others_music") return c.windowOthersMusic ? "1" : "0";
    if (k == "windows.others_sounds") return c.windowOthersSounds ? "1" : "0";  // 3.0.3-beta.9
    if (k == "windows.switch_keys") return windowKeysToken(c.windowKeys);  // 3.0.3-beta.6
    if (k == "panel.show") return c.panelShow ? "1" : "0";  // 3.0.3-beta.8
    if (k == "interface.minimap") return c.newMinimap ? "1" : "0";  // 3.0.11-beta.1
    if (k == "interface.minimap_npcs") return c.minimapNpcs ? "1" : "0";  // 3.0.11-beta.6
    if (k == "interface.minimap_warps") return c.minimapWarps ? "1" : "0";
    if (k == "interface.minimap_mobs") return c.minimapMobs ? "1" : "0";
    if (k == "windows.start_count") return std::to_string(c.windowStartCount);  // 3.0.4-beta.4
    if (k == "camera.indoor") return c.cameraIndoor ? "1" : "0";                // 3.0.4-beta.5
    if (k == "camera.zoom_limit") return c.cameraZoomLimit ? "1" : "0";         // 3.0.4-beta.17
    if (k == "buffs.timers") return c.buffTimers ? "1" : "0";                   // 3.0.4-beta.6
    if (k.compare(0, 7, "notify.") == 0) {                   // 3.0.4-beta.3
        const NotifySettings& n = c.notify;
        const std::string w = k.substr(7);
        if (w == "buff_seconds") return std::to_string(n.buffSeconds);  // 3.0.7-beta.6
        if (w == "party_hp") return std::to_string(n.partyHp);
        if (w == "recovered_percent") return std::to_string(n.recoveredPercent);  // 3.0.7-beta.7
        for (size_t i = 0; i < kNotifyBoxCount; ++i)
            if (w == kNotifyBoxes[i].key) return n.*kNotifyBoxes[i].field ? "1" : "0";
        return "0";
    }
    if (k == "panel.layout") return panelLayoutToken(c.panelLayout);
    if (k == "panel.size") return std::to_string(c.panelSize);
    if (k == "panel.opacity") return std::to_string(c.panelOpacity);
    if (k == "renderer.safe_mode") return c.safeMode ? "1" : "0";  // 2.12
    if (k == "stats.send") return c.statsSend ? "1" : "0";
    if (k == "renderer.bridge") return c.bridge ? "1" : "0";
    if (k == "renderer.render_thread") return c.renderThread ? "1" : "0";
    if (k == "renderer.remix") return c.remix ? "1" : "0";
    if (k == "display.high_resolution") return c.highResolution ? "1" : "0";
    if (k == "quality.render_scale") return std::to_string(c.renderScale);
    if (k == "quality.world_scale") return std::to_string(c.worldScale);  // 2.5.16
    if (k == "quality.full_colour") return c.fullColour ? "1" : "0";        // 2.5.18
    if (k == "quality.super_light") return c.superLight ? "1" : "0";        // 2.5.20
    if (k == "frames.battery_saver") return c.batterySaver ? "1" : "0";
    if (k == "frames.battery_fps") return std::to_string(c.batteryFps);
    if (k == "quality.sharpen") return std::to_string(c.sharpen);
    if (k == "display.ui_filter") return uiFilterToken(c.uiFilter);
    if (k == "display.window_art") return windowArtToken(c.windowArt);
    if (k == "display.crisp_text") return c.crispText ? "1" : "0";
    if (k == "quality.dlss") return c.dlss;
    if (k == "quality.dlss_scale") return std::to_string(c.dlssScale);  // 2.4.2
    if (k == "quality.dlss_width") return std::to_string(c.dlssWidth);
    if (k == "quality.dlss_height") return std::to_string(c.dlssHeight);
    if (k == "quality.dlss_model") return c.dlssModel;
    if (k == "quality.msaa") return std::to_string(c.msaa);
    if (k == "quality.taa") return c.taa ? "1" : "0";
    if (k == "quality.alpha_to_coverage") return c.alphaToCoverage ? "1" : "0";
    const AiSettings& a = c.ai;
    if (k == "ai.enabled") return a.enabled ? "1" : "0";
    if (k == "ai.ground") return a.ground ? "1" : "0";
    if (k == "ai.objects") return a.objects ? "1" : "0";
    if (k == "ai.monsters") return a.monsters ? "1" : "0";
    if (k == "ai.players") return a.players ? "1" : "0";
    if (k == "ai.npcs") return a.npcs ? "1" : "0";
    if (k == "ai.items") return a.items ? "1" : "0";
    if (k == "ai.effects") return a.effects ? "1" : "0";
    if (k == "ai.icons") return a.icons ? "1" : "0";
    if (k == "ai.ui_pieces") return a.uiPieces ? "1" : "0";
    if (k == "ai.model") return a.model;
    if (k == "ai.sprite_model") return a.spriteModel;
    if (k == "ai.small_pictures") return a.smallPictures ? "1" : "0";  // 1.6.3
    if (k == "ai.small_side") return std::to_string(a.smallSide);
    if (k == "ai.small_model") return a.smallModel;
    if (k.rfind("ai.model_", 0) == 0) {  // 1.6: a group's, as it comes out (its own, or model / sprite_model)
        ai::Group g;
        if (ai::parseGroup(k.substr(9), g)) return a.modelFor(g);
    }
    if (k == "ai.denoise") return std::to_string(a.denoise);
    if (k == "ai.scale") return std::to_string(a.scale);
    if (k == "ai.budget") return std::to_string(a.budget);
    if (k == "ai.min_size") return std::to_string(a.minSize);
    if (k == "ai.max_size") return std::to_string(a.maxSize);
    if (k == "ai.ahead") return a.ahead ? "1" : "0";
    if (k == "ai.dyes") return a.dyes ? "1" : "0";  // 2.5.23
    if (k == "ai.compress") return a.compress;
    if (k == "ai.generate") return a.generate ? "1" : "0";
    if (k == "quality.anisotropy") return std::to_string(c.anisotropy);
    if (k == "effects.preset") return effect_groups::presetToken(c.effects.preset);  // 2.2
    for (int g = 0; g < kEffectGroups; ++g) {
        const std::string word = std::string("effects.group_") + effect_groups::groupWord(g);
        if (k == word) return c.effects.groups[g] ? "1" : "0";
        if (k == word + "_strength") return std::to_string(c.effects.groupStrength[g]);
    }
    if (k == "effects.tier") return c.effects.tier == 1 ? "classic" : c.effects.tier == 2 ? "remaster" : "auto";
    if (k == "effects.clock")  // 2.6.11
        return s.tokens[c.effects.clock > 0 && c.effects.clock < int(s.tokens.size()) ? size_t(c.effects.clock) : 0];
    if (k == "effects.season")  // 2.6.22
        return s.tokens[c.effects.season > 0 && c.effects.season < int(s.tokens.size()) ? size_t(c.effects.season) : 0];
    if (k == "effects.weather_kind")  // 2.6.22
        return s.tokens[c.effects.weatherKind > 0 && c.effects.weatherKind < int(s.tokens.size())
                            ? size_t(c.effects.weatherKind)
                            : 0];
    if (k == "frames.vsync") return c.vsync ? "1" : "0";
    if (k == "frames.fps_limit") return std::to_string(c.fpsLimit);
    if (k == "frames.keep_fps") return std::to_string(c.keepFps);
    if (k == "debug.profile") return c.profile ? "1" : "0";
    if (k == "debug.xray") return c.xray ? "1" : "0";  // 2.3
    if (k == "debug.seams_view") return std::to_string(c.seamsView >= 0 && c.seamsView <= 2 ? c.seamsView : 0);
    if (k == "debug.weak_card") return c.weakCard ? "1" : "0";  // 3.0.4-beta.35
    if (k == "frames.save_power") return c.savePower ? "1" : "0";
    if (k == "frames.fps_counter") return c.fpsCounter ? "1" : "0";
    if (k == "display.ui_native") return c.uiNative ? "1" : "0";
    if (k == "display.hdr") return c.hdr ? "1" : "0";
    if (k == "frames.frame_generation") return c.frameGeneration ? std::to_string(c.frameGeneration) + "x" : "off";
    if (k == "frames.allow_tearing") return c.allowTearing ? "1" : "0";
    if (k == "frames.reflex") return c.reflex ? "1" : "0";  // 2.5.26
    if (k == "sound.rate") return std::to_string(c.sound.rate);  // 0.91
    if (k == "sound.voices") return c.sound.voices ? "1" : "0";
    if (k == "sound.background") return std::to_string(c.sound.background);
    if (k == "sound.limiter") return c.sound.limiter ? "1" : "0";
    if (k == "sound.stereo") return std::to_string(c.sound.stereo);  // 0.92
    if (k == "sound.steal") return c.sound.steal ? "1" : "0";
    if (k == "sound.crossfade") return c.sound.crossfade ? "1" : "0";
    if (k == "sound.engine") return c.sound.engine ? "1" : "0";  // 1.1
    if (k == "sound.reverb") return std::to_string(c.sound.reverb);
    if (k == "sound.headphones") return c.sound.headphones ? "1" : "0";  // 1.2
    if (k == "sound.duck") return c.sound.duck ? "1" : "0";              // 1.3
    if (k == "sound.output") return c.sound.output.empty() ? "auto" : c.sound.output;  // 3.0.4-beta.1
    if (k == "sound.ambience") return c.sound.ambience ? "1" : "0";                    // 3.0.4-beta.7
    if (k == "sound.ambience_volume") return std::to_string(c.sound.ambienceVolume);
    if (k == "debug.log") return c.logLevel == LogLevel::Debug ? "verbose" : "normal";
    // 2.6: the Debug page's test switches
    if (k == "quality.dlss_jitter") return c.dlssJitterInverted ? "inverted" : "normal";
    if (k == "quality.dlss_motion") return c.dlssMotionInverted ? "inverted" : "normal";
    if (k == "renderer.bridge_present") return c.bridgePresent;
    if (k == "renderer.bridge_dpi_scale") return c.bridgeDpiScale ? "1" : "0";
    if (k == "debug.shader_variants") return c.shaderVariants ? "1" : "0";
    if (k == "debug.state_cache") return c.stateCache ? "1" : "0";
    if (k == "debug.merge_draws") return c.mergeDraws ? "1" : "0";
    if (k == "debug.fast_pick") return c.fastPick ? "1" : "0";
    if (k == "debug.fast_ftol") return c.fastFtol ? "1" : "0";
    if (k == "debug.fast_textures") return c.fastTextures ? "1" : "0";
    return "";
}

bool validToken(const IniSetting& s, const std::string& token) {
    if (s.control == Control::Line)  // 0.50.2: a graphics card's name - one line, no comment or section in it
        return !token.empty() && token.find_first_of("\r\n;[") == std::string::npos;
    if (s.control == Control::Slider || s.control == Control::Number) {
        char* end = nullptr;
        const long n = std::strtol(token.c_str(), &end, 10);
        return !token.empty() && *end == '\0' && n >= s.min && n <= s.max;
    }
    return std::any_of(s.tokens.begin(), s.tokens.end(), [&](const char* t) { return token == t; });
}

}  // namespace uagfx::settings

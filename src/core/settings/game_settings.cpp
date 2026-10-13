#include "core/settings/game_settings.hpp"

#include <algorithm>
#include <cstdlib>

namespace uagfx::settings {
namespace {

RegSetting check(const char* subkey, const char* name, Page page, RegKind kind, uint32_t fallback, Text label,
                 Text hint, bool gameWrites) {
    return RegSetting{subkey, name, page, Control::Check, kind, fallback, {}, {}, 0, 1, 1, label, hint, gameWrites, ""};
}

RegSetting choice(const char* subkey, const char* name, Page page, uint32_t fallback, std::vector<uint32_t> values,
                  std::vector<Text> choices, Text label, Text hint, bool gameWrites) {
    return RegSetting{subkey, name, page, Control::Choice, RegKind::Number, fallback, std::move(values),
                      std::move(choices), 0, 0, 0, label, hint, gameWrites, ""};
}

RegSetting slider(const char* subkey, const char* name, Page page, uint32_t fallback, Text label, Text hint) {
    return RegSetting{subkey, name, page, Control::Slider, RegKind::Number, fallback, {}, {}, 0, 127, 1, label, hint,
                      true, ""};
}

RegSetting macro(const char* name) {
    return RegSetting{"ShortCutList", name, Page::Chat, Control::Line, RegKind::Line, 0, {}, {}, 0, 49, 0,
                      Text::ChatMacrosHeading, Text::ChatMacrosHint, true, ""};
}

size_t letters(const std::string& utf8) {  // a UTF-8 letter's later bytes are 10xxxxxx
    return size_t(
        std::count_if(utf8.begin(), utf8.end(), [](char c) { return (static_cast<unsigned char>(c) & 0xC0) != 0x80; }));
}

}  // namespace

const std::vector<RegSetting>& regSettings() {
    using P = Page;
    using K = RegKind;
    static const std::vector<RegSetting> list{
        // Quality (the game's own): registry.md §1.
        choice("", "SPRITEMODE", P::Quality, 2, {2, 1, 0}, {Text::LevelHigh, Text::LevelHalf, Text::LevelLow},
               Text::GameSprites, Text::GameSpritesHint, true),
        choice("", "TEXTUREMODE", P::Quality, 2, {2, 1, 0}, {Text::LevelHigh, Text::LevelHalf, Text::LevelLow},
               Text::GameTextures, Text::GameTexturesHint, true),
        check("", "TRILINEARFILTER", P::Quality, K::Flag, 0, Text::GameMipmaps, Text::GameMipmapsHint, true),
        check("", "ISLIGHTMAP", P::Quality, K::FlagOne, 1, Text::GameLightmaps, Text::GameLightmapsHint, false),
        check("", "FOG", P::Quality, K::Flag, 1, Text::GameFog, Text::GameFogHint, true),
        // Sound: the device the game never writes; the Option values at exit.
        choice("", "SOUNDMODE", P::Sound, 0, {0, 1, 2}, {Text::SoundOff, Text::Sound2D, Text::Sound3D}, Text::SoundMode,
               Text::SoundModeHint, false),
        choice("", "DIGITALRATETYPE", P::Sound, 0, {0, 1, 2}, {Text::Rate22050, Text::Rate11025, Text::Rate8000},
               Text::SoundRate, Text::SoundRateHint, false),
        choice("", "DIGITALBITSTYPE", P::Sound, 0, {0, 1}, {Text::Bits16, Text::Bits8}, Text::SoundBits,
               Text::SoundBitsHint, false),
        choice("", "NUMSAMPLETYPE", P::Sound, 2, {0, 1, 2}, {Text::Voices48, Text::Voices32, Text::Voices16},
               Text::SoundVoices, Text::SoundVoicesHint, false),
        choice("", "SPEAKERTYPE", P::Sound, 0, {0, 1, 2, 3, 4, 5},
               {Text::Speakers2, Text::SpeakersPhones, Text::SpeakersSurround, Text::Speakers4, Text::Speakers51,
                Text::Speakers71},
               Text::SoundSpeakers, Text::SoundSpeakersHint, false),
        check("Option", "bgmIsPaused", P::Sound, K::NotFlag, 0, Text::SoundMusic, Text::SoundMusic, true),
        slider("Option", "streamVolume", P::Sound, 100, Text::SoundMusicVolume, Text::SoundVolumeHint),
        check("Option", "isSoundOn", P::Sound, K::Flag, 1, Text::SoundEffects, Text::SoundEffects, true),
        slider("Option", "soundVolume", P::Sound, 100, Text::SoundEffectsVolume, Text::SoundVolumeHint),
        // Game: registry.md §4, §1, §5.
        check("Option", "isEffectOn", P::Game, K::Flag, 1, Text::GameEffects, Text::GameEffects, true),
        check("Option", "m_bMakeMissEffect", P::Game, K::Flag, 1, Text::GameMiss, Text::GameMiss, true),
        check("Option", "m_monsterSnapOn_Skill", P::Game, K::Flag, 1, Text::GameSkillSnap, Text::GameSkillSnap, true),
        check("Option", "m_monsterSnapOn_NoSkill", P::Game, K::Flag, 0, Text::GameAttackSnap, Text::GameAttackSnap,
              true),
        check("Option", "m_isItemSnap", P::Game, K::Flag, 0, Text::GameItemSnap, Text::GameItemSnap, true),
        check("Option", "g_isFixedCamera", P::Game, K::Flag, 0, Text::GameCamera, Text::GameCamera, true),
        check("Option", "onHoUserAI", P::Game, K::Flag, 0, Text::GameHomunAi, Text::GameHomunAi, true),
        check("Option", "onMerUserAI", P::Game, K::Flag, 0, Text::GameMercAi, Text::GameMercAi, true),
        check("", "SHOWTIPSATSTARTUP", P::Game, K::Flag, 1, Text::GameTips, Text::GameTips, true),
        check("", "BINKMODE", P::Game, K::NotFlag, 0, Text::GameIntro, Text::GameIntro, true),
        check("UIRectInfo", "M_MINIMAPARGB", P::Game, K::Argb, 0xAAFFFFFF, Text::GameMinimap, Text::GameMinimap, true),
        check("UIRectInfo", "M_ISDRAGALL", P::Game, K::Flag, 1, Text::GameStacks, Text::GameStacks, true),
        RegSetting{"Option", "SKINNAME", P::Game, Control::Choice, K::Line, 0, {}, {}, 0, 127, 0, Text::GameSkin,
                   Text::GameSkinHint, true, "<Basic Skin>"},
        // Chat and login: registry.md §4, §1, §6.
        check("Option", "m_isShowWhisperWnd", P::Chat, K::Flag, 1, Text::ChatWhisperStrangers,
              Text::ChatWhisperStrangers, true),
        check("Option", "m_isShowWhisperWnd_Friend", P::Chat, K::Flag, 1, Text::ChatWhisperFriends,
              Text::ChatFriendsNote, true),
        check("Option", "m_isPlayWhisperOpenSound", P::Chat, K::Flag, 1, Text::ChatWhisperSound,
              Text::ChatWhisperSound, true),
        check("", "CHECKSAVE", P::Chat, K::FlagOne, 0, Text::ChatRememberLogin, Text::ChatRememberLogin, true),
        macro("0"), macro("1"), macro("2"), macro("3"), macro("4"), macro("5"), macro("6"), macro("7"), macro("8"),
        macro("9"),
    };
    return list;
}

std::string regKey(const RegSetting& s) {
    return *s.subkey ? std::string(s.subkey) + "\\" + s.name : std::string(s.name);
}

const RegSetting* findRegSetting(const std::string& key) {
    for (const RegSetting& s : regSettings())
        if (regKey(s) == key) return &s;
    return nullptr;
}

std::string regWordOf(const RegSetting& s, const GameRegistry& r) {
    const auto v = effectiveValue(r, s.subkey, s.name);
    if (s.kind == RegKind::Line) return v && v->isSz() ? v->text : s.fallbackText;
    const uint32_t n = v && v->asDword() ? *v->asDword() : s.fallback;
    switch (s.kind) {
        case RegKind::Flag: return n != 0 ? "1" : "0";
        case RegKind::FlagOne: return n == 1 ? "1" : "0";
        case RegKind::NotFlag: return n == 0 ? "1" : "0";
        case RegKind::Argb: return n != 0xFFFFFFFFu ? "1" : "0";
        case RegKind::Number:
        case RegKind::Line: break;
    }
    if (s.control == Control::Slider)
        return std::to_string(std::clamp(int64_t(n), int64_t(s.min), int64_t(s.max)));
    if (std::find(s.values.begin(), s.values.end(), n) != s.values.end()) return std::to_string(n);
    return std::to_string(*std::max_element(s.values.begin(), s.values.end()));  // the game's rule for the rest
}

bool validRegWord(const RegSetting& s, const std::string& word) {
    if (s.kind == RegKind::Line)
        return letters(word) <= size_t(s.max) && word.find_first_of(std::string("\r\n\0", 3)) == std::string::npos &&
               (s.control != Control::Choice || !word.empty());
    char* end = nullptr;
    const long n = std::strtol(word.c_str(), &end, 10);
    if (word.empty() || *end != '\0') return false;
    if (s.control == Control::Check) return word == "0" || word == "1";
    if (s.control == Control::Slider) return n >= s.min && n <= s.max;
    return n >= 0 && std::find(s.values.begin(), s.values.end(), uint32_t(n)) != s.values.end();
}

RegValue regValueOf(const RegSetting& s, const std::string& word) {
    const bool on = word == "1";
    switch (s.kind) {
        case RegKind::Flag:
        case RegKind::FlagOne: return RegValue::dword(on ? 1 : 0);
        case RegKind::NotFlag: return RegValue::dword(on ? 0 : 1);
        case RegKind::Argb: return RegValue::dword(on ? 0xAAFFFFFFu : 0xFFFFFFFFu);
        case RegKind::Line: return RegValue::sz(word);
        case RegKind::Number: break;
    }
    return RegValue::dword(uint32_t(std::strtoul(word.c_str(), nullptr, 10)));
}

const char* builtInMacro(int index) {
    static const char* const kMacros[] = {"/!", "/?", "/ho", "/lv", "/swt", "/ic", "/an", "/ag", "/$", "/..."};
    return index >= 0 && index < 10 ? kMacros[index] : "";
}

}  // namespace uagfx::settings

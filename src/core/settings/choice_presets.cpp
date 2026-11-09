#include "core/settings/choice_presets.hpp"

#include "core/settings/ini_edit.hpp"
#include "core/settings/schema.hpp"

namespace uagfx::settings::presets {
namespace {

using Keys = std::vector<std::pair<std::string, std::string>>;

// The new interface's screen (what [interface] minimap turns on with it, config.cpp's newMinimap list) ...
const char* const kScreen[] = {"buffs", "character", "target", "party", "tracker", "hotbar", "cast", "chat"};
const char* const kScreenOn[] = {"column", "bars", "1", "1", "1", "1", "1", "1"};
// ... and its windows
const char* const kWindows[] = {
    "items", "status", "skills", "equip", "npc_talk", "shop", "storage", "trade", "menu", "cards", "asks",
    "messages", "login", "char_select", "char_make", "cart", "counts", "vending", "picks", "party_setup", "pet",
    "homun", "merc", "options", "social", "servers", "guild", "my_shop", "whisper_setup", "macros", "tips",
    "emotes", "commands", "chat_rooms", "detail_console", "popups", "whispers"};

Keys interfaceKeys(Ui u) {
    Keys k{{"interface.minimap", u == Ui::Classic ? "0" : "1"}};
    for (size_t i = 0; i < std::size(kScreen); ++i)
        k.emplace_back(std::string("interface.") + kScreen[i], u == Ui::Classic ? (i < 2 ? "off" : "0") : kScreenOn[i]);
    for (const char* w : kWindows) k.emplace_back(std::string("interface.") + w, u == Ui::New ? "1" : "0");
    return k;
}

}  // namespace

Suggestion suggest(int cardLevel, unsigned long long vramMb, bool integrated) {
    Suggestion s;
    s.picture = int(cardLevel >= 3 ? Picture::Remaster : cardLevel == 2 ? Picture::Better : Picture::Auto);
    s.textures = int(integrated || (vramMb >= 2048 && vramMb < 4096) ? Textures::HdLight
                     : vramMb >= 4096                                 ? Textures::Hd
                                                                      : Textures::Original);
    return s;
}

Text headOf(Choice c) {
    return c == Choice::Picture ? Text::LookPicture : c == Choice::Interface ? Text::LookUi : Text::LookTextures;
}

Text nameOf(Choice c, int variant) {
    static const Text picture[] = {Text::LookAuto, Text::LookGame, Text::LookBetter, Text::LookRemaster};
    static const Text ui[] = {Text::LookUiNew, Text::LookUiScreen, Text::LookUiClassic};
    static const Text textures[] = {Text::LookTexHd, Text::LookTexLight, Text::LookTexOriginal};
    if (variant < 0 || variant >= variants(c)) return Text::LookAuto;
    return c == Choice::Picture ? picture[variant] : c == Choice::Interface ? ui[variant] : textures[variant];
}

int variants(Choice c) { return c == Choice::Picture ? 4 : 3; }

std::vector<std::pair<std::string, std::string>> keysOf(Choice c, int variant) {
    switch (c) {
        case Choice::Picture: {
            static const char* const preset[] = {"auto", "off", "medium", "strong"};
            static const char* const tier[] = {"auto", "classic", "classic", "remaster"};
            if (variant < 0 || variant >= 4) return {};
            return {{"effects.preset", preset[variant]}, {"effects.tier", tier[variant]}};
        }
        case Choice::Interface:
            if (variant < 0 || variant >= 3) return {};
            return interfaceKeys(Ui(variant));
        case Choice::Textures:
            switch (Textures(variant)) {
                case Textures::Hd: return {{"ai.enabled", "1"}, {"ai.scale", "4"}, {"ai.compress", "auto"}};
                case Textures::HdLight: return {{"ai.enabled", "1"}, {"ai.scale", "2"}, {"ai.compress", "on"}};
                case Textures::Original: return {{"ai.enabled", "0"}};
            }
    }
    return {};
}

Match match(Choice c, const Config& config) {
    Match best{0, -1};
    for (int v = 0; v < variants(c); ++v) {
        int changed = 0;
        for (const auto& [key, token] : keysOf(c, v)) {
            const IniSetting* s = findSetting(key);
            if (s && tokenOf(*s, config) != token) ++changed;
        }
        if (best.changed < 0 || changed < best.changed) best = Match{v, changed};
    }
    if (best.changed < 0) best.changed = 0;
    return best;
}

std::string apply(const std::string& ini, Choice c, int variant) {
    std::vector<IniEdit> edits;
    for (const auto& [key, token] : keysOf(c, variant)) {
        const size_t dot = key.find('.');
        edits.push_back(IniEdit{key.substr(0, dot), key.substr(dot + 1), token});
    }
    return applyIniEdits(ini, edits);
}

}  // namespace uagfx::settings::presets

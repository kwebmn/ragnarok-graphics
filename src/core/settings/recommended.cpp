#include "core/settings/recommended.hpp"

#include "core/settings/schema.hpp"

namespace uagfx::settings {

std::map<std::string, std::string> recommendedTokens(const Config& defaults, const AutoSetup& a) {
    std::map<std::string, std::string> tokens;
    for (const IniSetting& s : iniSettings()) tokens[sectionKey(s)] = tokenOf(s, defaults);
    for (const IniEdit& e : a.edits) {
        const std::string key = e.section + "." + e.key;
        if (findSetting(key)) tokens[key] = e.value;
    }
    return tokens;
}

}  // namespace uagfx::settings

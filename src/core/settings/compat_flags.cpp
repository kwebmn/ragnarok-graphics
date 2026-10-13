#include "core/settings/compat_flags.hpp"

#include <cctype>
#include <sstream>

namespace uagfx::settings {
namespace {

std::string upper(std::string s) {
    for (char& ch : s) ch = char(std::toupper(static_cast<unsigned char>(ch)));
    return s;
}

}  // namespace

CompatLayers parseLayers(const std::string& value) {
    CompatLayers l;
    std::istringstream in(value);
    std::string word;
    while (in >> word) {
        if (word == "~")
            l.tilde = true;
        else
            l.flags.push_back(word);
    }
    return l;
}

std::string formatLayers(const CompatLayers& l) {
    if (l.flags.empty()) return "";
    std::string out = l.tilde ? "~" : "";
    for (const std::string& f : l.flags) out += (out.empty() ? "" : " ") + f;
    return out;
}

bool removeLayer(CompatLayers& l, const std::string& flag) {
    const size_t before = l.flags.size();
    for (size_t i = l.flags.size(); i-- > 0;)
        if (upper(l.flags[i]) == upper(flag)) l.flags.erase(l.flags.begin() + long(i));
    return l.flags.size() != before;
}

LayerEffect layerEffect(const std::string& flag) {
    const std::string f = upper(flag);
    if (f == "RUNASADMIN") return LayerEffect::RunAsAdmin;
    for (const char* os : {"WIN95", "WIN98", "NT4SP5", "WIN2000", "WINXP", "WINXPSP2", "WINXPSP3", "WINSRV03SP1",
                           "WINSRV08SP1", "VISTARTM", "VISTASP1", "VISTASP2", "WIN7RTM", "WIN8RTM", "WIN81RTM"})
        if (f == os) return LayerEffect::OldWindows;
    for (const char* dpi : {"HIGHDPIAWARE", "DPIUNAWARE", "GDIDPISCALING", "PERPROCESSSYSTEMDPIFORCEOFF",
                            "PERPROCESSSYSTEMDPIFORCEON"})
        if (f == dpi) return LayerEffect::Dpi;
    for (const char* c : {"256COLOR", "16BITCOLOR", "640X480"})
        if (f == c) return LayerEffect::Colors;
    return LayerEffect::Other;
}

bool samePath(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        char x = char(std::tolower(static_cast<unsigned char>(a[i])));
        char y = char(std::tolower(static_cast<unsigned char>(b[i])));
        if (x == '/') x = '\\';
        if (y == '/') y = '\\';
        if (x != y) return false;
    }
    return true;
}

}  // namespace uagfx::settings

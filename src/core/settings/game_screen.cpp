#include "core/settings/game_screen.hpp"

#include <cstdlib>

namespace uagfx::settings {
namespace {

bool same(const std::string& a, const std::string& b) { return !NameLess{}(a, b) && !NameLess{}(b, a); }

}  // namespace

std::optional<GameScreen> parseGameScreen(const std::string& text) {
    GameScreen s;
    size_t at = 0;
    while (at < text.size()) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(at, end - at);
        at = end + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = line.substr(0, eq);
        const unsigned long v = std::strtoul(line.c_str() + eq + 1, nullptr, 10);
        if (key == "fullscreen") s.fullscreen = v != 0;
        if (key == "width") s.width = uint32_t(v);
        if (key == "height") s.height = uint32_t(v);
    }
    if (s.width < 1 || s.height < 1 || s.width > 16384 || s.height > 16384) return std::nullopt;
    return s;
}

std::string gameScreenText(const GameScreen& s) {
    return "fullscreen=" + std::to_string(s.fullscreen ? 1 : 0) + "\r\nwidth=" + std::to_string(s.width) +
           "\r\nheight=" + std::to_string(s.height) + "\r\n";
}

const std::vector<std::string>& screenValueNames() {
    static const std::vector<std::string> names{"ISFULLSCREENMODE", "WIDTH",      "HEIGHT",    "BITPERPIXEL",
                                                "DEVICENAME",       "GUIDDEVICE", "GUIDDRIVER"};
    return names;
}

bool screenValue(const GameScreen& s, const std::string& name, RegValue& out) {
    if (same(name, "ISFULLSCREENMODE")) out = RegValue::dword(s.fullscreen ? 1 : 0);
    else if (same(name, "WIDTH")) out = RegValue::dword(s.width);
    else if (same(name, "HEIGHT")) out = RegValue::dword(s.height);
    else if (same(name, "BITPERPIXEL")) out = RegValue::dword(32);
    else if (same(name, "DEVICENAME")) out = RegValue::sz("Direct3D HAL");
    else if (same(name, "GUIDDEVICE")) out = RegValue::binary(halDeviceGuid());
    else if (same(name, "GUIDDRIVER")) out = RegValue::binary(std::vector<uint8_t>(16, 0));
    else return false;
    return true;
}

bool takeScreenWrite(GameScreen& s, const std::string& name, const RegValue& v) {
    bool known = false;
    for (const std::string& n : screenValueNames()) known = known || same(n, name);
    if (!known) return false;
    const bool number = same(name, "ISFULLSCREENMODE") || same(name, "WIDTH") || same(name, "HEIGHT");
    if (!number) return true;
    const auto d = v.asDword();
    if (!d) return false;
    if (same(name, "ISFULLSCREENMODE")) s.fullscreen = *d != 0;
    if (same(name, "WIDTH") && *d > 0 && *d <= 16384) s.width = *d;
    if (same(name, "HEIGHT") && *d > 0 && *d <= 16384) s.height = *d;
    return true;
}

std::optional<GameScreen> screenOf(const GameRegistry& r) {
    const auto dw = [&r](const char* name) -> std::optional<uint32_t> {
        const auto v = effectiveValue(r, "", name);
        return v ? v->asDword() : std::nullopt;
    };
    const auto w = dw("WIDTH"), h = dw("HEIGHT"), f = dw("ISFULLSCREENMODE");
    if (!w || !h || !*w || !*h) return std::nullopt;
    return GameScreen{f && *f != 0, *w, *h};
}

bool hasOwnSetup(const RegCopy& common) {
    return common.find("", "WIDTH") && common.find("", "HEIGHT") && common.find("", "DEVICENAME");
}

std::vector<RegWrite> sharedRepair(const GameRegistry& shared) {
    std::vector<RegWrite> out;
    if (!hasOwnSetup(shared.common)) return out;
    for (const std::string& name : screenValueNames())
        if (shared.personal.find("", name)) out.push_back(RegWrite{"", name, RegOp::Delete, RegValue{}});
    return out;
}

}  // namespace uagfx::settings

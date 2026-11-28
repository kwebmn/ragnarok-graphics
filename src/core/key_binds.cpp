#include "core/key_binds.hpp"

namespace uagfx::keybinds {
namespace {

constexpr unsigned kScans[kTokenCount] = {0x29, 0x39, 0x0F, 0x10, 0x12, 0x13, 0x21,
                                          0x2E, 0x22, 0x14, 0x2C, 0x2D, 0x2F, 0x30};
constexpr const char* kKeys[kBinds] = {"walk_toggle", "attack",  "pick_up",     "talk",
                                       "sit",         "target",  "camera_left", "camera_right"};

}  // namespace

Binds defaults() { return Binds{}; }

unsigned scanOf(const std::string& token) {
    for (int i = 0; i < kTokenCount; ++i)
        if (token == kTokens[i]) return kScans[i];
    return 0;
}

const char* tokenOf(unsigned scan) {
    for (int i = 0; i < kTokenCount; ++i)
        if (kScans[i] == scan) return kTokens[i];
    return "";
}

const char* settingKey(Bind b) { return unsigned(b) < unsigned(kBinds) ? kKeys[int(b)] : ""; }

std::string walkLine(const std::string& form, const Binds& b, const std::function<std::string(unsigned scan)>& name) {
    std::string out = form;
    for (int i = kBinds; i-- > 0;) {  // {8} before {1}: no «{1}» inside «{18}» ever, but kept simple and safe
        const std::string mark = "{" + std::to_string(i + 1) + "}";
        for (size_t at = out.find(mark); at != std::string::npos; at = out.find(mark, at)) {
            const std::string n = name ? name(b.scan[i]) : std::string(tokenOf(b.scan[i]));
            out.replace(at, mark.size(), n);
            at += n.size();
        }
    }
    return out;
}

std::string keyName(unsigned scan, const std::string& spaceWord) {
    if (scan == 0x29) return "`";
    if (scan == 0x39) return spaceWord;
    std::string t = tokenOf(scan);
    if (t == "tab") return "Tab";
    for (char& c : t) c = char(c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c);
    return t;
}

std::string keysList(const std::string& list, const std::string& walkForm, const Binds& b,
                     const std::string& spaceWord) {
    std::string out;
    size_t at = 0;
    while (at <= list.size()) {
        size_t end = list.find('\n', at);
        if (end == std::string::npos) end = list.size();
        const std::string line = list.substr(at, end - at);
        out += line.rfind("` ", 0) == 0
                   ? walkLine(walkForm, b, [&](unsigned scan) { return keyName(scan, spaceWord); })
                   : line;
        if (end < list.size()) out += '\n';
        at = end + 1;
    }
    return out;
}

Bind boundTo(const Binds& b, unsigned scan, bool& found) {
    found = false;
    for (int i = 0; i < kBinds; ++i)
        if (b.scan[i] == scan) {
            found = true;
            return Bind(i);
        }
    return Bind::Toggle;
}

}  // namespace uagfx::keybinds

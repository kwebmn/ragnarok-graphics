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

#include "core/closed_windows.hpp"

#include <sstream>

namespace uagfx::closedwin {

const char* keyOf(Window w) {
    static const char* const kKeys[size_t(Window::Count)] = {"hunt", "mvp", "party", "target"};
    return size_t(w) < size_t(Window::Count) ? kKeys[size_t(w)] : "";
}

Set read(const std::string& text) {
    Set s;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        for (size_t i = 0; i < size_t(Window::Count); ++i)
            if (line == keyOf(Window(i))) s.set(i);
    }
    return s;
}

std::string write(const Set& s) {
    std::string out;
    for (size_t i = 0; i < size_t(Window::Count); ++i)
        if (s.test(i)) out += std::string(keyOf(Window(i))) + "\n";
    return out;
}

std::vector<Own> row(const Switches& on) {
    std::vector<Own> out;
    if (on.reference) out.push_back(Own::Reference);
    if (on.hunt) out.push_back(Own::Hunt);
    if (on.mvp) out.push_back(Own::Mvp);
    if (on.tracker) out.push_back(Own::Tracker);
    if (on.party) out.push_back(Own::Party);
    if (on.recap) out.push_back(Own::Recap);
    out.push_back(Own::Settings);
    return out;
}

bool ownOfKey(unsigned vk, const Keys& keys, Own& out) {
    for (const Own o : {Own::Hunt, Own::Mvp, Own::Tracker, Own::Party, Own::Recap})
        if (keys[size_t(o)] && unsigned(keys[size_t(o)]) == vk) {
            out = o;
            return true;
        }
    return false;
}

std::string keyName(Own o, const Keys& keys) {
    if (o == Own::Reference) return "Ctrl+F";
    if (o == Own::Settings) return "Alt+O";
    const char c = size_t(o) < keys.size() ? keys[size_t(o)] : 0;
    return c ? std::string("Ctrl+") + c : std::string();
}

}  // namespace uagfx::closedwin

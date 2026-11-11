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

bool ownOfKey(unsigned vk, Own& out) {
    switch (vk) {
        case 'H': out = Own::Hunt; return true;
        case 'B': out = Own::Mvp; return true;
        case 'J': out = Own::Tracker; return true;
        case 'G': out = Own::Party; return true;
        case 'K': out = Own::Recap; return true;
        default: return false;
    }
}

const char* keyName(Own o) {
    static const char* const kNames[size_t(Own::Count)] = {"Ctrl+F", "Ctrl+H", "Ctrl+B", "Ctrl+J",
                                                          "Ctrl+G", "Ctrl+K", "Alt+O"};
    return size_t(o) < size_t(Own::Count) ? kNames[size_t(o)] : "";
}

}  // namespace uagfx::closedwin

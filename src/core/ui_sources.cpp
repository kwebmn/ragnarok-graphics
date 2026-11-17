#include "core/ui_sources.hpp"

#include <cstdio>

#include "core/window_panel.hpp"

namespace uagfx::uisources {

std::string Sources::name(const panel::Picture* p, int generation, std::string& previous,
                          std::vector<std::string>& release) {
    if (!p) {
        if (!previous.empty()) leave(previous, release), previous.clear();
        return {};
    }
    char buffer[48];
    std::snprintf(buffer, sizeof buffer, "pic:%llx:%d", (unsigned long long)uintptr_t(p), generation);
    const std::string source = buffer;
    if (previous == source) return previous;
    if (!previous.empty()) leave(previous, release);
    Kept& k = kept_[source];
    if (k.places++ == 0) {  // new: the pixels as they are now, while the picture surely lives
        const size_t n = p->width > 0 && p->height > 0 ? size_t(p->width) * size_t(p->height) : 0;
        if (n && p->px.size() >= n) {
            k.width = p->width;
            k.height = p->height;
            k.px = std::make_shared<const std::vector<uint32_t>>(p->px.begin(), p->px.begin() + std::ptrdiff_t(n));
        }
        release.push_back(source);
    }
    previous = source;
    return previous;
}

const Kept* Sources::find(const std::string& source) const {
    const auto it = kept_.find(source);
    return it == kept_.end() ? nullptr : &it->second;
}

void Sources::leave(const std::string& source, std::vector<std::string>& release) {
    const auto it = kept_.find(source);
    if (it != kept_.end() && --it->second.places > 0) return;  // another place still shows it
    if (it != kept_.end()) kept_.erase(it);
    release.push_back(source);
}

}  // namespace uagfx::uisources

#include "core/window_plan.hpp"

#include <cstdint>

namespace uagfx {

WindowAction windowAction(const Config& config, bool gameFullscreen) {
    switch (config.mode) {
        case DisplayMode::Window: return WindowAction::Window;
        case DisplayMode::Borderless: return WindowAction::Borderless;
        case DisplayMode::Auto: break;
    }
    if (gameFullscreen) return WindowAction::Borderless;
    return config.windowWidth || config.windowHeight ? WindowAction::Window : WindowAction::Leave;
}

Rect fitPicture(int width, int height, int outWidth, int outHeight) {
    if (width <= 0 || height <= 0 || outWidth <= 0 || outHeight <= 0) return Rect{};
    int w = outWidth;  // full width, unless the height then does not fit
    int h = int((int64_t(height) * outWidth + width / 2) / width);
    if (h > outHeight) {
        h = outHeight;
        w = int((int64_t(width) * outHeight + height / 2) / height);
    }
    const int left = (outWidth - w) / 2;
    const int top = (outHeight - h) / 2;
    return Rect{left, top, left + w, top + h};
}

int monitorIndex(const std::vector<bool>& primary, int wanted) {
    std::vector<int> order;
    for (size_t i = 0; i < primary.size(); ++i)
        if (primary[i]) order.push_back(int(i));
    for (size_t i = 0; i < primary.size(); ++i)
        if (!primary[i]) order.push_back(int(i));
    return wanted >= 0 && size_t(wanted) < order.size() ? order[size_t(wanted)] : -1;
}

}  // namespace uagfx

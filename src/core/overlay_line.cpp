#include "core/overlay_line.hpp"

namespace uagfx::overlayline {

std::wstring line(bool counter, bool photo, const std::wstring& note, const std::wstring& counterText) {
    if (photo) return L"";
    if (note.empty()) return counter ? counterText : L"";
    return counter ? note + L"  ·  " + counterText : note;
}

}  // namespace uagfx::overlayline

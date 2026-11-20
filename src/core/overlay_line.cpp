#include "core/overlay_line.hpp"

namespace uagfx::overlayline {

std::wstring line(bool counter, bool photo, const std::wstring& note, const std::wstring& counterText) {
    if (photo) return L"";
    if (note.empty()) return counter ? counterText : L"";
    return counter ? note + L"  ·  " + counterText : note;
}

void Notes::say(const std::wstring& note, uint64_t now, bool enhanced, bool evenOff) {
    if (!enhanced && !evenOff) return;
    shown_ = note;
    until_ = now + kSayMs;
}

void Notes::hold(const std::wstring& note, bool enhanced) {
    if (!enhanced) return;
    shown_ = note;
    until_ = 0;  // keep_fps's line ends no other
}

void Notes::toggled() {
    shown_.clear();
    until_ = 0;
}

void Notes::tick(uint64_t now) {
    if (!until_ || now < until_) return;
    shown_.clear();
    until_ = 0;
}

}  // namespace uagfx::overlayline

#pragma once
#include <cstdint>
#include <string>

namespace uagfx::overlayline {

// 3.0.22-beta.28 (the owner's video session 20.11: Pause's «uaRO Graphics вимкнено…» stood in the photo mode's
// recordings): what the line in the game window's top left corner says - the frame counter (Ctrl+Shift+F11) when
// `counter`, after the note (Pause's line, the graphics test's, the frame keeper's, the card's) when there is one;
// nothing at all in the photo mode (Ctrl+Alt+F12, a big shot): a clean picture for a shot or a recording.
// "" - the line's window hidden.
std::wstring line(bool counter, bool photo, const std::wstring& note, const std::wstring& counterText);

// 3.0.22-beta.29 (the owner: «убери надпись про паузу ... пускай будет тупо старый клиент без надписи»): the corner's
// note. Pause shows the old client alone: pressed, it takes away whatever was said, and while the improvements are off
// nothing of the driver's is said - but the graphics test's answer to its start under Pause (`evenOff`). A note said
// goes after 10 s; one held (the graphics test's progress) stays until the next.
class Notes {
public:
    static constexpr uint64_t kSayMs = 10000;
    void say(const std::wstring& note, uint64_t now, bool enhanced, bool evenOff = false);
    void hold(const std::wstring& note, bool enhanced);
    void toggled();
    void tick(uint64_t now);
    const std::wstring& shown() const { return shown_; }

private:
    std::wstring shown_;
    uint64_t until_ = 0;  // when a note said goes; 0 - none (or one held)
};

}  // namespace uagfx::overlayline

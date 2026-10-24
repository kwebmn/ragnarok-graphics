#pragma once
#include <cstdint>
#include <string>

#include "core/config.hpp"
#include "core/settings/strings.hpp"

namespace uagfx::notify {

// 3.0.4-beta.3 (the user's idea 2): a game window behind others tells of what wants the player (win/notifier.cpp):
// its taskbar button flashes and Windows shows a notification - [notify], a box for each kind.
enum class Kind : uint8_t { Whisper, Party, Trade, Guild, Friend, Death };
constexpr int kKinds = 6;

// Whether to tell: notifications on, this kind on, the game window not in front (in front the player sees it)
bool wanted(const NotifySettings& s, Kind k, bool inFront);

// The notification's words (UTF-8) in `lang`: who (a name, a party's, a guild's) and what (a whisper's text)
std::string text(settings::Lang lang, Kind k, const std::string& who, const std::string& what);

// A burst (a flood of whispers) shows once: one notification of a kind every kGapMs.
class Gate {
public:
    static constexpr uint64_t kGapMs = 3000;
    bool pass(Kind k, uint64_t nowMs);

private:
    uint64_t last_[kKinds] = {};
    bool seen_[kKinds] = {};
};

}  // namespace uagfx::notify

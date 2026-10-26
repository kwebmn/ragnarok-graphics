#pragma once
#include <cstdint>
#include <string>

#include "core/config.hpp"
#include "core/settings/strings.hpp"

namespace uagfx::notify {

// 3.0.4-beta.3 (the user's idea 2): a game window behind others tells of what wants the player (win/notifier.cpp):
// its taskbar button flashes and Windows shows a notification - [notify], a box for each kind. 3.0.7-beta.6 (the user
// chose 1, 5, 6, 7, 8, 9, 10): an attack, a buff ending, a death or low HP in the party, the party's and the guild's
// chat, the character's name said near, the server gone.
enum class Kind : uint8_t {
    Whisper, Party, Trade, Guild, Friend, Death,
    Attack, BuffEnding, PartyDeath, PartyLowHp, PartyChat, GuildChat, Mention, Disconnect
};
constexpr int kKinds = 14;

// Whether to tell: notifications on, this kind on, the game window not in front (in front the player sees it). What
// every game window of the party or the guild hears (its chat, its deaths and low HP) is told by the main window
// alone (`main`: in front, or the game last in front) - several windows of one player told it each
bool wanted(const NotifySettings& s, Kind k, bool inFront, bool main = true);

// The notification's words (UTF-8) in `lang`: who (a name, a party's, a guild's, a buff's) and what (a whisper's or
// a chat's text, an attack's damage, the seconds or the per cent left)
std::string text(settings::Lang lang, Kind k, const std::string& who, const std::string& what);

// An attack told once - again only after kCalmMs without a hit (the taskbar button flashes all the while)
class Attacks {
public:
    static constexpr uint64_t kCalmMs = 30000;
    bool hit(uint64_t nowMs);  // true: tell

private:
    uint64_t last_ = 0;
    bool seen_ = false;
};

// A chat line as the server sends it, "Name : words": who and what (none: who empty, what the whole)
void speaker(const std::string& line, std::string& who, std::string& what);
// The name said in `text` (UTF-8 both): letters' case aside (Latin, Cyrillic), a whole word - not inside another
bool mentions(const std::string& text, const std::string& name);

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

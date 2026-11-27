#pragma once
#include <cstdint>
#include <string>

#include "core/net_trail.hpp"
#include "core/settings/strings.hpp"

// 3.0.24-beta.31 (R-S2JJXG, R-V7SV57, R-XQ93QH, R-P4HVX6, R-MWNRVM: five «drops» of one laptop - each the map
// server's notice 1 some 41 s after the map): the server's daily play-time limit (uaRO's map server,
// pc_playlimit_check). Its words come as one's own chat line (0x008e, English, no «name :») - «Daily play-time limit
// (8 h) reached. You will be disconnected in 40 seconds. The limit resets at 00:00 server time (in 11 h 42 min). Buy
// more time with @buyplaytime <minutes>, or pay as you play with @autopay on (1 zeny a minute).», before it «Daily
// play time: 10 minute(s) left today. See @playtime. Buy more time ...» - then its kick, the notice 0x0081 code 1;
// kicked too often, the login is refused a while (0x006a code 6, until a time). Told in the player's words (the
// chat's line, the box after the kick); its kick no drop for a report.
namespace uagfx::playlimit {

struct Notice {
    bool reached = false;    // the limit's words (else the minutes left's)
    int hours = 0;           // the day's limit (reached)
    int kickInS = 0;         // the disconnect in (reached)
    int resetMin = -1;       // minutes to the reset at 00:00 server time (-1: not said)
    int minutesLeft = -1;    // the warning's
    int zenyPerMinute = -1;  // @autopay's price (-1: not offered)
    bool canBuy = false;     // @buyplaytime offered
};
// The server's words (from their first letter: one's own public words come as «Name : words»); false: others
bool read(const std::string& text, Notice& n);

struct Words {  // strings.cpp's - «{1}» the numbers
    std::string reached, kicked, left, reset, buy, autopay;
};
Words wordsIn(settings::Lang l);
// The chat's line (kicked false), or the box's after the kick («Сервер від'єднав: ...»); the reset `pastMin` minutes
// nearer than the words said
std::string words(const Notice& n, const Words& w, bool kicked, int pastMin = 0);
// The login refused (code 6) after a limit's kick in this game: why
std::string blockedWords(settings::Lang l);

constexpr int kKickCode = 1;               // the notice 0x0081's «server closed»
constexpr uint32_t kKickSlackMs = 30000;   // past the words' «in N seconds»
constexpr uint32_t kBlockRecentMs = 3600000;  // a refusal this long after a kick: the limit's block
// The drop `d` the limit's kick: the map server's notice 1 within the reached words' kickInS + kKickSlackMs of them
// (they came at `saidMs`, the game's clock)
bool kickOf(const nettrail::Drop& d, uint32_t saidMs, const Notice& n);

}  // namespace uagfx::playlimit

#pragma once
#include <cstdint>
#include <string>

namespace uagfx::ping {

// 3.0.11-beta.56 (the owner: «сделать» - how many have uaRO Graphics, and on which versions): the settings app's
// anonymous mark at its update check - POST uaro.kiev.ua/api/graphics/ping {"id","version","channel"} at most every 12
// hours: this install's random number (uaro-graphics\install-id.txt, made once; nothing of the computer or the player
// in it), the version and the channel. The site keeps the number's first and last day, its version and its count; no
// address. Whatever the statistics' choice: no settings, no play times - only that it is there.
constexpr long long kEverySeconds = 12 * 3600;

bool due(long long now, long long last);    // never told, 12 hours since, or a clock gone back
bool validId(const std::string& id);        // 16 lower-case hex digits
std::string newId(uint64_t seed);           // 16 lower-case hex digits from a seed's mixed bits (splitmix64)
std::string idOf(const std::string& text);  // the file's first line when a valid number, else ""
// The request's body: {"id":"...","version":"...","channel":"release"} (the version its digits, dots and '-'s);
// 3.0.11-beta.61 (the autopilot): `first` - the first game's outcome (core/first_game json, an object) added once as
// "first", the app's tip saying so
std::string body(const std::string& id, const std::string& version, const std::string& channel,
                 const std::string& first = "");

}  // namespace uagfx::ping

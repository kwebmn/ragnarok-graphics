#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace uagfx::accounts {

// 3.0.11-beta.175 (the login concept's shelf): the player's game accounts kept on this computer - the shelf the login
// screen shows (a click enters). The login, the password, a label of the player's, «Одразу в гру» (the last character
// entered at once), the character the login last entered with. Kept DPAPI-protected for this Windows user in
// %LOCALAPPDATA%\uaroGraphics\accounts.dat (win/accounts_store); never in a log or a report.
struct LastChar {
    std::string name, job, map;  // UTF-8
    int jobId = 0, level = 0, slot = -1, sex = 0, hair = 0, hairColour = 0;
};
struct Account {
    std::string login, password, label;  // the game's words (the login Latin as a rule), the label UTF-8
    bool keepPassword = true;  // false: the password not written (asked at each login)
    bool autoEnter = true;     // «Одразу в гру»: past the character select with the last one
    bool refused = false;      // the server refused its password the last time
    LastChar last;
    long long usedAt = 0;  // unix time of its last login
};
struct Shelf {
    std::vector<Account> list;  // the player's order
    bool addAfterLogin = true;  // a login that entered put on the shelf by itself
};

std::string toJson(const Shelf& s);
bool fromJson(const std::string& text, Shelf& out);  // false: not the shelf's words (out empty then)
// A login entered: on the shelf (at its end) or, already there in any case, its password and time new
void upsert(Shelf& s, const std::string& login, const std::string& password, long long now);
bool remove(Shelf& s, const std::string& login);
bool move(Shelf& s, size_t index, int delta);  // false: no such place
void recordChar(Shelf& s, const std::string& login, const LastChar& c, long long now);
// A login's own number (any case the same; FNV-1a 64 of its lower case) - for the logs and the shared block, never
// its words
uint64_t loginHash(const std::string& login);

}  // namespace uagfx::accounts

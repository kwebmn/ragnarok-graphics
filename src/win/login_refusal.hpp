#pragma once
#include <optional>
#include <string>

namespace uagfx::win::loginrefusal {

// 3.0.11-beta.172 (the login concept: the server's answer in words): the server's refusal of a login (0x006a) seen
// as the game takes it - uaRO.exe FUN_005A40C0 (thiscall: the login mode, the packet; its code at +2, the date words
// at +3 for code 6) sent through ours first. While the game is inside it its box comes (win/msg_box answers the ones
// the new login card shows in words); after it the card takes the code. On the game's thread.
void install();
struct Seen {
    int code = -1;
    std::string until;  // the date words of a forbidden login, the game's code page; "" none
};
int handlingCode();          // the code the game is handling now (its box about to come), -1 none
const std::string& handlingUntil();
std::optional<Seen> take();  // the last refusal, once

}  // namespace uagfx::win::loginrefusal

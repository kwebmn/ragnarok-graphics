#pragma once
#include <windows.h>

#include <string>
#include <vector>

#include "core/frame.hpp"
#include "core/game_packets.hpp"
#include "core/window_panel.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::partywin {

// 3.0.11-beta.39 (stage 7 of the new interface; the owner: «делай»): [interface] party - the party's panel at the
// top left under the character block (core/party_panel). Its members from the zone server's packets: the list
// (0x00fb: each seat's map, leader, online), one joining or changed (0x01e9), one gone (0x0105), their HP as the server
// sends it to those near (0x0106; the character's own never); a member seen dying (0x0080) at 0. On the game's thread.
void packet(const packets::Event& e);
// 3.0.11-beta.282 (the owner's feature 18): [interface] party_alarm - one of the party here falling under 30 % HP: a
// short chime and its row pulsing red 3 s (core/party_alarm)
void setAlarm(bool on);
void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);

// 3.0.11-beta.83 (the owner's 3: «участники на миникарте и большой карте»): the party's members on the character's
// map at the cells the server told the party (0x0107, fresh), their faces at `faceSide` - for the minimap
struct OnMap {
    int x = 0, y = 0;
    std::string name;  // UTF-8
    const panel::Picture* face = nullptr;
    bool dead = false;
};
std::vector<OnMap> onMap(int faceSide);
// 3.0.11-beta.85: the members online on the character's map and not dead, in the panel's order - whom the pad's
// support skill may go on (uapad::Controller::setFriends)
std::vector<uint32_t> hereIds();
// 3.0.16-beta.38 (R-GF8Z5A): this account is in the character's party - the minimap shows a guild mate who is so
// only as the party's member (one mark, not a triangle and a face by turns)
bool inParty(uint32_t account);
// 3.0.11-beta.216: its box in the target while it shows (the mercenary's block stands under it); false: not shown
bool panelBox(int& x, int& y, int& w, int& h);
// 3.0.11-beta.235 (the character block's party line): the party's members in the game and all, the character with
// them, as the panel last read them; false none known
bool counts(int& online, int& total);
// 3.0.18-beta.141 (the friends and party card made richer): the others as the panel last read them - face, class and
// level, HP, dead, here (cells away) or elsewhere (the map's words); a way to one (as a click on their row); the
// experience share as the server told it (0x0101: -1 not told, 0 each, 1 even)
struct Person {
    std::string name, job, where;  // UTF-8; where "" here
    int percent = -1, cells = -1;
    bool dead = false, online = true;
    const panel::Picture* face = nullptr;
};
std::vector<Person> people();
bool routeTo(const std::string& name);
int expShare();

}  // namespace uagfx::win::partywin

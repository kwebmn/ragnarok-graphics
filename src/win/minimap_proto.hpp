#pragma once
#include "core/game_packets.hpp"
#include <windows.h>

#include <string>
#include <utility>
#include <vector>

#include "core/config.hpp"
#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::minimapproto {

// 3.0.11-beta.1: [interface] minimap = 1 - the new interface's minimap prototype (stage 1, the owner's «давай»
// 2026-10-28; core/minimap_proto). Each frame on the game's thread: the game's own minimap hidden as Ctrl+Tab hides it
// (each time the game makes it again: a new map, Ctrl+Tab; back when switched off), the driver's panel in its corner
// with the map's picture round the player and the player's cell under it (to set against /where); the mouse over it
// kept from the game. The log tells what each step did. 3.0.11-beta.6: its hints from the game's reference
// (win/reference_load) - the NPCs and the warps on it (the pointer over one names it), the map's monsters under it.
struct Show {
    bool on = false, npcs = true, warps = true, mobs = true;
    WayOnGround way = WayOnGround::Steps;  // 3.0.11-beta.33: [interface] minimap_way - the way on the ground
    bool reach = false;  // 3.0.11-beta.65: [interface] minimap_reach - what cannot be reached on foot dimmed
    bool hd = true;      // 3.0.11-beta.72: [interface] minimap_hd - the map's picture 4x bigger when there is one
    // 3.0.11-beta.119 (R-5Z6J2S): [interface] minimap_kafra / minimap_party - the Kafras, the party's faces; each
    // layer (with npcs and warps) also switched by the maps' own buttons, written to uaro-graphics.ini
    bool kafra = true, party = true;
    bool viewFan = true;  // 3.0.11-beta.187: [interface] minimap_view - the camera's fan round the arrow
    bool classic = false;  // 3.0.11-beta.222: [interface] minimap_classic - the marks as round dots
    // 3.0.11-beta.120 (the owner: «карта ... какаято не красивая»): [interface] minimap_style = scheme - the maps drawn
    // from the server's walkable cells (core/minimap_proto schemePixels) in place of the game's picture; switched by
    // the maps' own button too
    bool scheme = false;
    // 3.0.11-beta.122: [interface] net_badge - the round trip (win/net_watch); 3.0.11-beta.127 on its own plate under
    // the bar with a graph of the last minute
    bool netBadge = true;
};
void frame(HWND game, const Show& show, double k, int targetWidth, int targetHeight);
// 3.0.11-beta.35: its panel's box in the target while it shows (the buffs' column stands under it); false: not shown
bool panelBox(int& x, int& y, int& w, int& h);
void draw(FrameRecord& record, gpu::Renderer& renderer);
// 3.0.11-beta.201: the big map - after the panels, under the windows (win/context)
// 3.0.11-beta.260 (the tester: «інвентар та меню нпц перекриває, і клік по мапі не переводить її на передній план»):
// as a window among them - opened or pressed it comes over the windows, a press in a window or another one opened
// puts it under them again; drawBig(over) draws it where it stands now (called under and over the windows)
void drawBig(FrameRecord& record, gpu::Renderer& renderer, bool over = false);
bool bigOnTop();
void lowerBig();
// the mouse of the big map alone while it lies over the windows (before theirs)
bool mouseBig(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
// 3.0.11-beta.31: and the wheel over the map zooms it; the big map's clicks first
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
// 3.0.11-beta.4 (the owner: «на Ctrl+Tab ничего не происходит»): Ctrl+Tab while the prototype is on - its look goes
// round as the game's own minimap's (opaque, see-through, hidden); true: the key is the prototype's, not the game's
bool key(unsigned vk);
// 3.0.11-beta.9: «вести ›» in a map's card (core/route_follow) - that way on the new minimap: a line to its next move,
// two lines under the map (where to, how many moves left, where to take the move); made again wherever the player
// goes; «Ви на місці» on the map, then it stops. canFollow: the new minimap on
bool canFollow();
// 3.0.18-beta.240 (the reference's «одна карта», the owner 13.11): the map's layers (minimap::Layer) and «Схема» as the
// minimap shows them - «Довідник»'s location map shows by them and switches them the same way (uaro-graphics.ini told,
// the minimap's buttons and the big map's chips too)
bool layerShown(int layer);
void switchLayerFromReference(int layer);
bool schemeShown();
// 3.0.11-beta.201: the server's marks (0x0144) and the guild's mates (0x01eb) from the packets (win/packet_tap)
void packet(const packets::Event& e);
// 3.0.11-beta.213 (the owner: «вывод команды @mobsearch показывался на миникарте ... чтобы это можно было потом
// убрать»): the chat's lines as the game pushes them (UTF-8) - @mobsearch's answer read (core/mob_search): its
// monsters' cells marked on the maps while the plate under the bar shows the search; its ✕, another search or another
// map clears them
void heard(const std::string& line, unsigned long long ms);
// 3.0.11-beta.90 (the controller's windows' tile): the big map opened or closed; false: the new minimap is off
bool showBig(bool open);
// 3.0.11-beta.93: the big map shows now (the controller's B closes it)
bool bigShown();
void follow(const std::string& target, bool walk);
void unfollow();
bool following(std::string& target, bool& walk);  // false: none
// 3.0.11-beta.70: the way's two lines as the minimap shows them under it (where to next, how many moves; where to take
// it) - the tracker's way; empty: none followed
std::vector<std::pair<std::string, std::string>> routeLines();
// 3.0.11-beta.71 (the owner: «хочется на карте прокладывать путь»): a click on the minimap or its big map sets a way
// there (Shift: one more point, a right click or ✕ takes it away; Ctrl on a warp of the big map: its map's card) - its
// last point, its lines, taken away (the tracker's entry)
bool plotted(int& x, int& y);
// 3.0.11-beta.82 (the party's «путь к участнику»): a way set to that cell of the map the player is on, as a click on
// the map sets one; false - the new minimap off, or the cell not on its map
bool plotTo(int x, int y);
std::vector<std::pair<std::string, std::string>> plotLines();
void clearPlot();

// 3.0.18-beta.133 (the pad analysis: the big map on the controller): while it is open - the left stick moves a
// cross over its cells (from the character's), X jumps to the next NPC / warp / Kafra, A plots the way there, Y
// clears it, B closes; false - the big map is not open. The cross's middle in the target and its side (none: false)
struct BigPad {
    float lx = 0, ly = 0, seconds = 0;
    bool plot = false, next = false, clear = false, close = false;
};
bool padBig(const BigPad& p);
bool padCross(int& x, int& y, float& side);

}  // namespace uagfx::win::minimapproto

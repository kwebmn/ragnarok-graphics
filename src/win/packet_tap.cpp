#include "win/item_net.hpp"
#include "win/context.hpp"
#include "win/social_card.hpp"
#include "win/death_recap_watch.hpp"
#include "win/ground_items.hpp"
#include "win/hunt_panel.hpp"
#include "win/mvp_panel.hpp"
#include "win/my_shop.hpp"
#include "win/server_card.hpp"
#include "win/skill_delay_watch.hpp"
#include "win/sphere_watch.hpp"
#include "win/combo_watch.hpp"
#include "win/tk_mission.hpp"
#include "win/upkeep_watch.hpp"
#include "win/packet_tap.hpp"

#include <windows.h>

#include <cstdint>
#include <cstring>

#include "core/game_packets.hpp"
#include "win/buff_overlay.hpp"
#include "win/char_block.hpp"
#include "win/exp_gain.hpp"
#include "win/party_panel.hpp"
#include "win/cast_bar.hpp"
#include "win/chat_window.hpp"
#include "win/item_window.hpp"
#include "win/minimap_proto.hpp"
#include "win/mob_hp.hpp"
#include "win/npc_tag.hpp"
#include "win/net_trail.hpp"
#include "win/net_watch.hpp"
#include "win/ask_card.hpp"
#include "win/npc_talk.hpp"
#include "win/storage_window.hpp"
#include "win/vend_shop.hpp"
#include "win/world_signs.hpp"
#include "win/target_card.hpp"
#include "win/ground_units.hpp"
#include "win/notifier.hpp"
#include "win/threat_arc.hpp"

namespace uagfx::win {
namespace {

constexpr uint32_t kCallAt = 0x579B02, kTakeId = 0x419350;
const uint8_t kCall[] = {0xE8, 0x49, 0xF8, 0xE9, 0xFF};
// push ebp; mov ebp, esp; mov eax, [ebp+8]; mov ax, [eax]; pop ebp; ret 4 - the id from the packet's first bytes
const uint8_t kTakeIdCode[] = {0x55, 0x8B, 0xEC, 0x8B, 0x45, 0x08, 0x66, 0x8B, 0x00, 0x5D, 0xC2, 0x04, 0x00};
// The loop's buffer [ebp-0x54b4] holds the whole packet and the frame goes on past it: this much is read at most
// (a whisper names its own length; the rest are a few dozen bytes)
constexpr size_t kReadable = 2048;

using TakeFn = uint16_t(__attribute__((thiscall)) *)(void* connection, const uint8_t* packet);
TakeFn g_take = nullptr;
bool g_on = false;

// 3.0.7-beta.6 (the user's notification 9): the game mode's loop polls its connection first (`call 0x418c60` at
// 0x579a65, CRagConnection: the socket read and written, false when the server closed it - the socket shut, and
// the game shows its own «disconnected» at once: CModeMgr's vfunc 0x14 (1)). A socket the game shut itself (a map on
// another server) is polled as fine - only the server's closing fails it.
constexpr uint32_t kPollAt = 0x579A65, kPoll = 0x418C60;
const uint8_t kPollCall[] = {0xE8, 0xF6, 0xF1, 0xE9, 0xFF};
// push esi; mov esi, ecx; cmp dword [esi+4], -1; je +0x1f; call 0x418eb0
const uint8_t kPollCode[] = {0x56, 0x8B, 0xF1, 0x83, 0x7E, 0x04, 0xFF, 0x74, 0x1F, 0xE8, 0x42, 0x02, 0x00, 0x00};
using PollFn = char(__attribute__((thiscall)) *)(void* connection);
PollFn g_poll = nullptr;

char __attribute__((thiscall)) poll(void* connection) {
    netwatchwin::polled();  // 3.0.11-beta.131: the game reads its socket now
    nettrailwin::beforePoll();  // 3.0.20-beta.60: the connections' trail
    const char fine = g_poll(connection);
    nettrailwin::polled(nettrail::Mode::Game, fine != 0);
    if (!fine) notifier::disconnected();
    return fine;
}

// 3.0.20-beta.60 (the owner: «клиент выбрасывает с дисконнектом, надо его отловить»): the login mode's loop too
// (FUN_005a1df0: the account, the character and the map server's connection until the map is entered) - its
// question to the connection at 0x5A1E15 and its packets' ids at 0x5A1E90, and the game's four connects (FUN_00418af0:
// 0x5A101E the account server, 0x5A12C1 the character server, 0x5A18F5 the map server, 0x57EE80 a map on another
// server) - for the connections' trail (win/net_trail.cpp)
constexpr uint32_t kLoginPollAt = 0x5A1E15, kLoginTakeAt = 0x5A1E90, kConnect = 0x418AF0;
const uint8_t kLoginPollCall[] = {0xE8, 0x46, 0x6E, 0xE7, 0xFF};
const uint8_t kLoginTakeCall[] = {0xE8, 0xBB, 0x74, 0xE7, 0xFF};
// push ebp; mov ebp, esp; sub esp, 8; push ebx; push esi; mov esi, ecx; push edi; lea edi, [esi+0x58]
const uint8_t kConnectCode[] = {0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x08, 0x53, 0x56, 0x8B, 0xF1, 0x57, 0x8D, 0x7E, 0x58};
struct ConnectSite {
    uint32_t at;
    uint8_t call[5];
};
const ConnectSite kAccountSite{0x5A101E, {0xE8, 0xCD, 0x7A, 0xE7, 0xFF}};
const ConnectSite kCharSite{0x5A12C1, {0xE8, 0x2A, 0x78, 0xE7, 0xFF}};
const ConnectSite kZoneSite{0x5A18F5, {0xE8, 0xF6, 0x71, 0xE7, 0xFF}};
const ConnectSite kMovedSite{0x57EE80, {0xE8, 0x6B, 0x9C, 0xE9, 0xFF}};
using ConnectFn = uint32_t(__attribute__((thiscall)) *)(void* connection, char* address);
ConnectFn g_connect = nullptr;

char __attribute__((thiscall)) pollLogin(void* connection) {
    nettrailwin::beforePoll();
    const char fine = g_poll(connection);
    nettrailwin::polled(nettrail::Mode::Login, fine != 0);
    return fine;
}

uint32_t connectAs(void* connection, char* address, nettrail::Role role) {
    const uint32_t answer = g_connect(connection, address);  // its low byte: 1 the connect went off
    nettrailwin::connected(connection, address, role, (answer & 0xFF) != 0);
    return answer;
}
uint32_t __attribute__((thiscall)) connectAccount(void* connection, char* address) {
    return connectAs(connection, address, nettrail::Role::Account);
}
uint32_t __attribute__((thiscall)) connectChar(void* connection, char* address) {
    return connectAs(connection, address, nettrail::Role::Char);
}
uint32_t __attribute__((thiscall)) connectZone(void* connection, char* address) {
    return connectAs(connection, address, nettrail::Role::Zone);
}

// The call at `at` (its bytes `call`, the function's first bytes `code`) through `to` instead
bool redirect(uint32_t at, const uint8_t* call, uint32_t fn, const uint8_t* code, size_t codeSize, void* to) {
    auto* p = reinterpret_cast<uint8_t*>(uintptr_t(at));
    if (std::memcmp(p, call, 5) != 0 || std::memcmp(reinterpret_cast<const void*>(uintptr_t(fn)), code, codeSize) != 0)
        return false;
    DWORD was = 0;
    if (!VirtualProtect(p + 1, 4, PAGE_EXECUTE_READWRITE, &was)) return false;
    const int32_t rel = int32_t(reinterpret_cast<uintptr_t>(to) - (uintptr_t(at) + 5));
    std::memcpy(p + 1, &rel, 4);
    DWORD unused = 0;
    VirtualProtect(p + 1, 4, was, &unused);
    FlushInstructionCache(GetCurrentProcess(), p, 5);
    return true;
}

uint16_t __attribute__((thiscall)) takeLogin(void* connection, const uint8_t* packet) {
    const uint16_t id = g_take(connection, packet);
    nettrailwin::packet(nettrail::Mode::Login, id, packet, 4);  // 3.0.20-beta.60: its id and first value
    return id;
}

uint16_t __attribute__((thiscall)) take(void* connection, const uint8_t* packet) {
    const uint16_t id = g_take(connection, packet);
    nettrailwin::packet(nettrail::Mode::Game, id, packet, 4);  // 3.0.20-beta.60: the connections' trail
    netwatchwin::packet(connection, id, packet);  // 3.0.11-beta.122: the connection watched, the position fixes
    groundunitswin::packet(id, packet);           // 3.0.11-beta.143: the own walls and pillars standing
    groundwin::packet(id, packet);  // 3.0.11-beta.283: the items on the ground
    skilldelaywin::packet(id, packet);            // 3.0.11-beta.276: the delay after the own skills
    spherewatch::packet(id, packet);              // 3.0.16-beta.8: the own spirit spheres (coins)
    combowatch::packet(id, packet);               // 3.0.16-beta.13: the own combo window
    upkeepwatch::packet(id, packet);              // 3.0.16-beta.13: broken gear, the pet's and homunculus's hunger
    tkmissionwin::packet(id, packet);             // 3.0.22-beta.4: the Taekwon mission's monster
    itemnet::packet(id, packet);                  // 3.0.24-beta.4: the things as the server tells them
    socialcardwin::rawPacket(id, packet);         // 3.0.24-beta.27: the friends' accounts (the party's invite)
    const packets::Event e = packets::parse(packet, kReadable);
    if (!std::holds_alternative<std::monostate>(e)) {
        notifier::packet(e);
        buffoverlay::packet(e);  // 3.0.4-beta.6
        targetpanel::packet(e);  // 3.0.11-beta.38: the target's card - who the character fights
        partywin::packet(e);     // 3.0.11-beta.39: the party's panel
        chatwin::packet(e);      // 3.0.11-beta.41: the chat's battle log
        castwin::packet(e);      // 3.0.11-beta.48: the cast's label - which skill
        mobhpwin::packet(e);     // 3.0.11-beta.95: the monsters' HP bars
        threatarcwin::packet(e);  // 3.0.19-beta.36: the 3D view's threat's arc
        if (const auto* h = std::get_if<packets::Hit>(&e))  // 3.0.13-beta.20: a friend's target - its effects theirs
            Context::get().noteHit(h->source, h->target);
        npcwin::packet(e);       // 3.0.11-beta.98: the NPC talk's words
        askcardwin::packet(e);   // 3.0.11-beta.150: the requests' names
        itemswin::packet(e);     // 3.0.11-beta.107: an item come into the bag told the log
        storagewin::packet(e);   // 3.0.11-beta.115: the storage's places used of the most
        vendshopwin::packet(e);  // 3.0.11-beta.156: the shops' signs
        myshopwin::packet(e);    // 3.0.18-beta.143: one's own shop's sales
        servercardwin::packet(e);  // 3.0.18-beta.153: the servers' players
        worldsigns::packet(e);   // 3.0.16-beta.7: the shops' and the rooms' signs as plates
        minimapproto::packet(e);  // 3.0.11-beta.201: the server's marks, the guild's mates
        charpanel::packet(e);     // 3.0.11-beta.235: the session's kills and deaths
        expgainwin::packet(e);    // 3.0.11-beta.237: a kill's experience over the monster
        npctagwin::packet(e);     // 3.0.11-beta.256: the NPCs' names for the controller's plate
        mvpwin::packet(e);  // 3.0.11-beta.280: the bosses seen dying
        huntwin::packet(e);  // 3.0.11-beta.278: the character's hits and the kills for «Полювання»
        deathrecapwin::packet(e);  // 3.0.11-beta.277: the hits on the character for its death screen
    }
    return id;
}

}  // namespace

void installPacketTap() {
    static bool done = false;
    if (done) return;
    done = true;
    g_take = reinterpret_cast<TakeFn>(uintptr_t(kTakeId));
    // not this uaRO.exe's packet loop: none (logged with the notifications' first frame)
    if (!redirect(kCallAt, kCall, kTakeId, kTakeIdCode, sizeof kTakeIdCode, reinterpret_cast<void*>(&take))) return;
    g_on = true;
    g_poll = reinterpret_cast<PollFn>(uintptr_t(kPoll));  // 3.0.7-beta.6: the server gone
    redirect(kPollAt, kPollCall, kPoll, kPollCode, sizeof kPollCode, reinterpret_cast<void*>(&poll));
    // 3.0.20-beta.60: the login mode's loop and the connects, for the connections' trail
    nettrailwin::install();
    int sites = 0;  // of the six: told with the trail's first line
    sites += redirect(kLoginPollAt, kLoginPollCall, kPoll, kPollCode, sizeof kPollCode,
                      reinterpret_cast<void*>(&pollLogin));
    sites += redirect(kLoginTakeAt, kLoginTakeCall, kTakeId, kTakeIdCode, sizeof kTakeIdCode,
                      reinterpret_cast<void*>(&takeLogin));
    g_connect = reinterpret_cast<ConnectFn>(uintptr_t(kConnect));
    const struct {
        const ConnectSite& site;
        void* to;
    } connects[] = {{kAccountSite, reinterpret_cast<void*>(&connectAccount)},
                    {kCharSite, reinterpret_cast<void*>(&connectChar)},
                    {kZoneSite, reinterpret_cast<void*>(&connectZone)},
                    {kMovedSite, reinterpret_cast<void*>(&connectZone)}};
    for (const auto& c : connects)
        sites += redirect(c.site.at, c.site.call, kConnect, kConnectCode, sizeof kConnectCode, c.to);
    nettrailwin::sitesTaken(sites, 6);
}

bool packetTapOn() { return g_on; }

}  // namespace uagfx::win

#include "win/window_stack.hpp"
#include "win/uaui.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <vector>

#include <optional>
#include <string>

#include "core/window_sizes.hpp"
#include "pad/win/game_window.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"
#include "win/cart_window.hpp"
#include "win/char_make.hpp"
#include "win/char_select.hpp"
#include "win/chat_window.hpp"
#include "win/ask_card.hpp"
#include "win/equip_window.hpp"
#include "win/guild_card.hpp"
#include "win/homun_card.hpp"
#include "win/info_card.hpp"
#include "win/item_window.hpp"
#include "win/login_card.hpp"
#include "win/macro_card.hpp"
#include "win/menu_window.hpp"
#include "win/merc_card.hpp"
#include "win/msg_box.hpp"
#include "win/my_shop.hpp"
#include "win/vend_setup.hpp"
#include "win/log.hpp"
#include "win/npc_talk.hpp"
#include "win/options_card.hpp"
#include "win/party_setup.hpp"
#include "win/pet_card.hpp"
#include "win/pick_card.hpp"
#include "win/reference_window.hpp"
#include "win/server_card.hpp"
#include "win/shop_window.hpp"
#include "win/skill_window.hpp"
#include "win/social_card.hpp"
#include "win/stat_window.hpp"
#include "win/storage_window.hpp"
#include "win/tip_window.hpp"
#include "win/emote_window.hpp"
#include "win/command_window.hpp"
#include "win/chatroom_window.hpp"
#include "win/room_window.hpp"
#include "win/popup_window.hpp"
#include "win/whisper_window.hpp"
#include "win/trade_window.hpp"
#include "win/vend_shop.hpp"
#include "win/whisper_setup.hpp"

namespace uagfx::win::windowstack {
namespace {

using winstack::Win;
using uapad::gamewindow::Blocker;

constexpr const char* kNames[] = {"«Речі»",  "«Характеристики»", "«Навички»", "«Спорядження»",
                                  "the shop", "the NPC talk",      "«Довідник»", "«Склад»", "«Обмін»",
                                  "the menu", "the item card", "the skill card", "the request",
                                  "the message box", "the login", "the character select", "the character make",
                                  "«Візок»", "«Лавка»", "the choice of things", "the party's settings",
                                  "the pet", "the homunculus", "the mercenary", "the game's options",
                                  "the friends and the party", "the server select", "the guild", "one's own shop",
                                  "the whisper's settings", "the chat's macros", "the tip of the day",
                                  "the emotions", "the command list", "the chat room",
                                  "the chat room's own", "a popup menu", "the whisper cards",
                                  "the shop's opening", "a slot's card"};
static_assert(std::size(kNames) == size_t(winstack::kWins), "a name for each window");
constexpr int kMostTold = 40;

struct Dock {  // 3.0.11-beta.111: «Спорядження» beside «Речі» (3.0.11-beta.115: or «Склад», while it is open)
    bool on = false;
    Win partner = Win::Equip;
    winstack::Side side = winstack::Side::Left;  // the partner's side of the bag
    bool unsaved = false;                        // the follower moved during a drag, its place not kept yet
};

struct State {
    winstack::Stack stack;
    Dock dock;
    std::optional<Win> told;  // the top the log named last
    int tells = 0;
};

// 3.0.18-beta.151: the windows one can size by their corner - those laid out in rem by their own scale
const char* sizeKey(Win w) {
    switch (w) {
        case Win::Items: return "items";
        case Win::Equip: return "equip";
        case Win::Stats: return "stats";
        case Win::Skills: return "skills";
        case Win::Storage: return "storage";
        case Win::Cart: return "cart";
        case Win::Trade: return "trade";
        case Win::Shop: return "shop";
        case Win::Guild: return "guild";
        // 3.0.18-beta.152: the companions (their own scale) and the live windows (their document zoomed)
        case Win::Homun: return "homun";
        case Win::Merc: return "merc";
        case Win::Npc: return "npc";
        case Win::Vending: return "vending";
        case Win::VendSetup: return "shop_setup";
        case Win::MyShop: return "my_shop";
        case Win::Reference: return "reference";  // 3.0.18-beta.239: its box, not a zoom (win/reference_window)
        case Win::Pick: return "pick";
        case Win::Pet: return "pet";
        case Win::Options: return "options";
        case Win::Social: return "social";
        case Win::Room: return "chat_room";
        case Win::ChatRoom: return "chat_room_make";
        case Win::Macros: return "macros";
        case Win::WhisperSetup: return "whisper_setup";
        case Win::Commands: return "commands";
        default: return nullptr;
    }
}

struct Sizing {
    bool read = false;
    windowsizes::Sizes sizes;
    std::optional<Win> pulling;  // its corner held
    float from = 1, w0 = 0, h0 = 0, x0 = 0, y0 = 0;
    int targetW = 0, targetH = 0;
    double k = 0;
    std::optional<Win> hot;  // the corner under the pointer
    winstack::Box hotBox{};
};

Sizing& sizing() {
    static Sizing z;
    if (!z.read) {
        z.read = true;
        std::string text;
        if (readTextFile(gameDir() + "\\uaro-graphics\\window-sizes.txt", text))
            z.sizes = windowsizes::Sizes::parse(text);
    }
    return z;
}

State& state() {
    static State s;
    return s;
}

const char* nameOf(Win w) { return kNames[int(w)]; }

void tell(State& s, const char* what, Win w) {
    if (s.tells >= kMostTold) return;
    ++s.tells;
    LOG_INFO("windows: %s %s (%zu shown)", nameOf(w), what, s.stack.bottomUp().size());
}

bool shown(Win w) {
    switch (w) {
        case Win::Items: return itemswin::shown();
        case Win::Stats: return statswin::shown();
        case Win::Skills: return skillswin::shown();
        case Win::Equip: return equipwin::shown();
        case Win::Shop: return shopwin::shown();
        case Win::Npc: return npcwin::shown();
        case Win::Reference: return referencewin::isOpen();
        case Win::Storage: return storagewin::shown();
        case Win::Trade: return tradewin::shown();
        case Win::Menu: return menuwin::shown();  // 3.0.11-beta.140
        case Win::ItemCard: return infocardwin::shown(infocardwin::Kind::Item);  // 3.0.11-beta.149
        case Win::SkillCard: return infocardwin::shown(infocardwin::Kind::Skill);
        case Win::Ask: return askcardwin::shown();  // 3.0.11-beta.150
        case Win::Message: return msgboxwin::shown();  // 3.0.11-beta.151
        case Win::Login: return loginwin::shown();
        case Win::CharSelect: return charselectwin::shown();  // 3.0.11-beta.152
        case Win::CharMake: return charmakewin::shown();  // 3.0.11-beta.153
        case Win::Cart: return cartwin::shown();  // 3.0.11-beta.154
        case Win::Vending: return vendshopwin::shown();  // 3.0.11-beta.156
        case Win::Pick: return pickcardwin::shown();  // 3.0.11-beta.157
        case Win::PartySetup: return partysetupwin::shown();  // 3.0.11-beta.158
        case Win::Pet: return petcardwin::shown();  // 3.0.11-beta.159
        case Win::Homun: return homuncardwin::shown();  // 3.0.11-beta.160
        case Win::Merc: return merccardwin::shown();  // 3.0.11-beta.161
        case Win::Options: return optionscardwin::shown();  // 3.0.11-beta.162
        case Win::Social: return socialcardwin::shown();  // 3.0.11-beta.163
        case Win::Servers: return servercardwin::shown();  // 3.0.11-beta.164
        case Win::Guild: return guildcardwin::shown();  // 3.0.11-beta.165
        case Win::MyShop: return myshopwin::shown();  // 3.0.11-beta.166
        case Win::WhisperSetup: return whispersetupwin::shown();  // 3.0.11-beta.168
        case Win::Macros: return macrocardwin::shown();  // 3.0.11-beta.169
        case Win::Tip: return tipwin::shown();  // 3.0.11-beta.204
        case Win::Emotes: return emotewin::shown();  // 3.0.11-beta.205
        case Win::Commands: return commandwin::shown();  // 3.0.11-beta.206
        case Win::ChatRoom: return chatroomwin::shown();  // 3.0.11-beta.207
        case Win::Room: return roomwin::shown();  // 3.0.11-beta.208
        case Win::Popup: return popupwin::shown();  // 3.0.11-beta.211
        case Win::Whispers: return whisperwin::shown();  // 3.0.11-beta.212
        case Win::VendSetup: return vendsetupwin::shown();  // 3.0.16-beta.33
        case Win::SlotCard: return infocardwin::shown(infocardwin::Kind::Card);  // 3.0.18-beta.139
    }
    return false;
}

Blocker blockerOf(Win w) {
    switch (w) {
        case Win::Items: return Blocker::Items;
        case Win::Stats: return Blocker::Stats;
        case Win::Skills: return Blocker::Skills;
        case Win::Equip: return Blocker::Equip;
        case Win::Shop: return Blocker::Shop;
        case Win::Npc: return Blocker::Npc;
        case Win::Storage: return Blocker::Storage;
        case Win::Trade: return Blocker::Trade;
        case Win::Menu: return Blocker::Menu;
        case Win::ItemCard: return Blocker::ItemCard;
        case Win::SkillCard: return Blocker::SkillCard;
        case Win::Ask: return Blocker::Ask;
        case Win::Message: return Blocker::Message;
        case Win::Login: return Blocker::Login;
        case Win::CharSelect: return Blocker::CharSelect;
        case Win::CharMake: return Blocker::CharMake;
        case Win::Cart: return Blocker::Cart;
        case Win::Vending: return Blocker::Vending;
        case Win::Pick: return Blocker::Pick;
        case Win::PartySetup: return Blocker::PartySetup;
        case Win::Pet: return Blocker::Pet;
        case Win::Homun: return Blocker::Homun;
        case Win::Merc: return Blocker::Merc;
        case Win::Options: return Blocker::Options;
        case Win::Social: return Blocker::Social;
        case Win::Servers: return Blocker::Servers;
        case Win::Guild: return Blocker::Guild;
        case Win::MyShop: return Blocker::MyShop;
        case Win::WhisperSetup: return Blocker::WhisperSetup;
        case Win::Macros: return Blocker::Macros;
        case Win::Tip: return Blocker::Tip;
        case Win::Emotes: return Blocker::Emotes;
        case Win::Commands: return Blocker::Commands;
        case Win::ChatRoom: return Blocker::ChatRoom;
        case Win::Room: return Blocker::Room;
        case Win::Popup: return Blocker::Popup;
        case Win::Whispers: return Blocker::Whispers;
        case Win::VendSetup: return Blocker::VendSetup;
        case Win::SlotCard: return Blocker::SlotCard;
        case Win::Reference: break;
    }
    return Blocker::Reference;
}

void drawOne(Win w, FrameRecord& record, gpu::Renderer& renderer) {
    switch (w) {
        case Win::Items: itemswin::draw(record, renderer); break;
        case Win::Stats: statswin::draw(record, renderer); break;
        case Win::Skills: skillswin::draw(record, renderer); break;
        case Win::Equip: equipwin::draw(record, renderer); break;
        case Win::Shop: shopwin::draw(record, renderer); break;
        case Win::Npc: npcwin::draw(record, renderer); break;
        case Win::Reference: referencewin::draw(record, renderer); break;
        case Win::Storage: storagewin::draw(record, renderer); break;
        case Win::Trade: tradewin::draw(record, renderer); break;
        case Win::Menu: menuwin::draw(record, renderer); break;
        case Win::ItemCard: infocardwin::draw(infocardwin::Kind::Item, record, renderer); break;
        case Win::SkillCard: infocardwin::draw(infocardwin::Kind::Skill, record, renderer); break;
        case Win::Ask: askcardwin::draw(record, renderer); break;
        case Win::Message: msgboxwin::draw(record, renderer); break;
        case Win::Login: loginwin::draw(record, renderer); break;
        case Win::CharSelect: charselectwin::draw(record, renderer); break;
        case Win::CharMake: charmakewin::draw(record, renderer); break;
        case Win::Cart: cartwin::draw(record, renderer); break;
        case Win::Vending: vendshopwin::draw(record, renderer); break;
        case Win::Pick: pickcardwin::draw(record, renderer); break;
        case Win::PartySetup: partysetupwin::draw(record, renderer); break;
        case Win::Pet: petcardwin::draw(record, renderer); break;
        case Win::Homun: homuncardwin::draw(record, renderer); break;
        case Win::Merc: merccardwin::draw(record, renderer); break;
        case Win::Options: optionscardwin::draw(record, renderer); break;
        case Win::Social: socialcardwin::draw(record, renderer); break;
        case Win::Servers: servercardwin::draw(record, renderer); break;
        case Win::Guild: guildcardwin::draw(record, renderer); break;
        case Win::MyShop: myshopwin::draw(record, renderer); break;
        case Win::WhisperSetup: whispersetupwin::draw(record, renderer); break;
        case Win::Macros: macrocardwin::draw(record, renderer); break;
        case Win::Tip: tipwin::draw(record, renderer); break;
        case Win::Emotes: emotewin::draw(record, renderer); break;
        case Win::Commands: commandwin::draw(record, renderer); break;
        case Win::ChatRoom: chatroomwin::draw(record, renderer); break;
        case Win::Room: roomwin::draw(record, renderer); break;
        case Win::Popup: popupwin::draw(record, renderer); break;
        case Win::Whispers: whisperwin::draw(record, renderer); break;
        case Win::VendSetup: vendsetupwin::draw(record, renderer); break;
        case Win::SlotCard: infocardwin::draw(infocardwin::Kind::Card, record, renderer); break;
    }
}

bool mouseOne(Win w, HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    switch (w) {
        case Win::Items: return itemswin::mouse(game, m, wp, lp, result);
        case Win::Stats: return statswin::mouse(game, m, wp, lp, result);
        case Win::Skills: return skillswin::mouse(game, m, wp, lp, result);
        case Win::Equip: return equipwin::mouse(game, m, wp, lp, result);
        case Win::Shop: return shopwin::mouse(game, m, wp, lp, result);
        case Win::Npc: return npcwin::mouse(game, m, wp, lp, result);
        case Win::Reference: return referencewin::mouse(game, m, wp, lp, result);
        case Win::Storage: return storagewin::mouse(game, m, wp, lp, result);
        case Win::Trade: return tradewin::mouse(game, m, wp, lp, result);
        case Win::Menu: return menuwin::mouse(game, m, wp, lp, result);
        case Win::ItemCard: return infocardwin::mouse(infocardwin::Kind::Item, game, m, wp, lp, result);
        case Win::SkillCard: return infocardwin::mouse(infocardwin::Kind::Skill, game, m, wp, lp, result);
        case Win::Ask: return askcardwin::mouse(game, m, wp, lp, result);
        case Win::Message: return msgboxwin::mouse(game, m, wp, lp, result);
        case Win::Login: return loginwin::mouse(game, m, wp, lp, result);
        case Win::CharSelect: return charselectwin::mouse(game, m, wp, lp, result);
        case Win::CharMake: return charmakewin::mouse(game, m, wp, lp, result);
        case Win::Cart: return cartwin::mouse(game, m, wp, lp, result);
        case Win::Vending: return vendshopwin::mouse(game, m, wp, lp, result);
        case Win::Pick: return pickcardwin::mouse(game, m, wp, lp, result);
        case Win::PartySetup: return partysetupwin::mouse(game, m, wp, lp, result);
        case Win::Pet: return petcardwin::mouse(game, m, wp, lp, result);
        case Win::Homun: return homuncardwin::mouse(game, m, wp, lp, result);
        case Win::Merc: return merccardwin::mouse(game, m, wp, lp, result);
        case Win::Options: return optionscardwin::mouse(game, m, wp, lp, result);
        case Win::Social: return socialcardwin::mouse(game, m, wp, lp, result);
        case Win::Servers: return servercardwin::mouse(game, m, wp, lp, result);
        case Win::Guild: return guildcardwin::mouse(game, m, wp, lp, result);
        case Win::MyShop: return myshopwin::mouse(game, m, wp, lp, result);
        case Win::WhisperSetup: return whispersetupwin::mouse(game, m, wp, lp, result);
        case Win::Macros: return macrocardwin::mouse(game, m, wp, lp, result);
        case Win::Tip: return tipwin::mouse(game, m, wp, lp, result);
        case Win::Emotes: return emotewin::mouse(game, m, wp, lp, result);
        case Win::Commands: return commandwin::mouse(game, m, wp, lp, result);
        case Win::ChatRoom: return chatroomwin::mouse(game, m, wp, lp, result);
        case Win::Room: return roomwin::mouse(game, m, wp, lp, result);
        case Win::Popup: return popupwin::mouse(game, m, wp, lp, result);
        case Win::Whispers: return whisperwin::mouse(game, m, wp, lp, result);
        case Win::VendSetup: return vendsetupwin::mouse(game, m, wp, lp, result);
        case Win::SlotCard: return infocardwin::mouse(infocardwin::Kind::Card, game, m, wp, lp, result);
    }
    return false;
}

bool keyOne(Win w, UINT m, WPARAM wp, LPARAM lp) {  // the windows with keys of their own
    switch (w) {
        case Win::Items: return itemswin::key(m, wp, lp);  // its search's while it types
        case Win::Shop: return shopwin::key(m, wp, lp);
        case Win::Npc: return npcwin::key(m, wp, lp);
        case Win::Reference: return referencewin::key(m, wp, lp);
        case Win::Storage: return storagewin::key(m, wp, lp);  // 3.0.11-beta.115: its search's
        case Win::Trade: return tradewin::key(m, wp, lp);      // 3.0.11-beta.116: its zeny field's
        case Win::Menu: return menuwin::key(m, wp, lp);        // 3.0.11-beta.140: ↑ ↓ Enter
        case Win::ItemCard: return infocardwin::key(infocardwin::Kind::Item, m, wp, lp);  // .149: ↑ ↓ scroll
        case Win::SkillCard: return infocardwin::key(infocardwin::Kind::Skill, m, wp, lp);
        case Win::Message: return msgboxwin::key(m, wp, lp);  // 3.0.11-beta.151: Enter, Esc - the box's answers
        case Win::Login: return loginwin::key(m, wp, lp);     // 3.0.11-beta.176: «Полиця»'s
        case Win::Cart: return cartwin::key(m, wp, lp);       // 3.0.11-beta.154: its search's
        case Win::Tip: return tipwin::key(m, wp, lp);         // 3.0.11-beta.204: ← →
        case Win::Guild: return guildcardwin::key(m, wp, lp);  // 3.0.11-beta.271: the notice's fields
        case Win::SlotCard: return infocardwin::key(infocardwin::Kind::Card, m, wp, lp);
        case Win::VendSetup: return vendsetupwin::key(m, wp, lp);  // 3.0.18-beta.105: Tab / Enter between fields
        case Win::Ask: return askcardwin::key(m, wp, lp);  // 3.0.18-beta.190: Enter on an empty count - the most
        case Win::CharMake: return charmakewin::key(m, wp, lp);  // 3.0.18-beta.228: ↑ ↓ the rows, ← → change
        default: return false;
    }
}

bool movable(Win w) {  // .140: the menu centred; .149: a card beside the cursor that opened it
    return w != Win::Npc && w != Win::Reference && w != Win::Menu && w != Win::ItemCard && w != Win::SkillCard &&
           w != Win::Ask && w != Win::Message && w != Win::Login && w != Win::CharSelect && w != Win::CharMake &&
           w != Win::Vending && w != Win::Pick && w != Win::PartySetup && w != Win::Pet && w != Win::Homun &&
           w != Win::Merc && w != Win::Options && w != Win::Social && w != Win::Servers && w != Win::Guild &&
           w != Win::MyShop && w != Win::WhisperSetup && w != Win::Macros && w != Win::Tip &&
           w != Win::Emotes && w != Win::Commands && w != Win::ChatRoom && w != Win::Room && w != Win::Popup &&
           w != Win::Whispers && w != Win::VendSetup && w != Win::SlotCard;
}

bool boxOf(Win w, winstack::Box& b) {
    switch (w) {
        case Win::Items: return itemswin::box(b.x, b.y, b.w, b.h);
        case Win::Stats: return statswin::box(b.x, b.y, b.w, b.h);
        case Win::Skills: return skillswin::box(b.x, b.y, b.w, b.h);
        case Win::Equip: return equipwin::box(b.x, b.y, b.w, b.h);
        case Win::Shop: return shopwin::box(b.x, b.y, b.w, b.h);
        case Win::Npc: return npcwin::box(b.x, b.y, b.w, b.h);
        case Win::Reference: return referencewin::box(b.x, b.y, b.w, b.h);
        case Win::Storage: return storagewin::box(b.x, b.y, b.w, b.h);
        case Win::Trade: return tradewin::box(b.x, b.y, b.w, b.h);
        case Win::Menu: return menuwin::box(b.x, b.y, b.w, b.h);
        case Win::ItemCard: return infocardwin::box(infocardwin::Kind::Item, b.x, b.y, b.w, b.h);
        case Win::SkillCard: return infocardwin::box(infocardwin::Kind::Skill, b.x, b.y, b.w, b.h);
        case Win::Ask: return askcardwin::box(b.x, b.y, b.w, b.h);
        case Win::Message: return msgboxwin::box(b.x, b.y, b.w, b.h);
        case Win::Login: return loginwin::box(b.x, b.y, b.w, b.h);
        case Win::CharSelect: return charselectwin::box(b.x, b.y, b.w, b.h);
        case Win::CharMake: return charmakewin::box(b.x, b.y, b.w, b.h);
        case Win::Cart: return cartwin::box(b.x, b.y, b.w, b.h);
        case Win::Vending: return vendshopwin::box(b.x, b.y, b.w, b.h);
        case Win::Pick: return pickcardwin::box(b.x, b.y, b.w, b.h);
        case Win::PartySetup: return partysetupwin::box(b.x, b.y, b.w, b.h);
        case Win::Pet: return petcardwin::box(b.x, b.y, b.w, b.h);
        case Win::Homun: return homuncardwin::box(b.x, b.y, b.w, b.h);
        case Win::Merc: return merccardwin::box(b.x, b.y, b.w, b.h);
        case Win::Options: return optionscardwin::box(b.x, b.y, b.w, b.h);
        case Win::Social: return socialcardwin::box(b.x, b.y, b.w, b.h);
        case Win::Servers: return servercardwin::box(b.x, b.y, b.w, b.h);
        case Win::Guild: return guildcardwin::box(b.x, b.y, b.w, b.h);
        case Win::MyShop: return myshopwin::box(b.x, b.y, b.w, b.h);
        case Win::WhisperSetup: return whispersetupwin::box(b.x, b.y, b.w, b.h);
        case Win::Macros: return macrocardwin::box(b.x, b.y, b.w, b.h);
        case Win::Tip: return tipwin::box(b.x, b.y, b.w, b.h);
        case Win::Emotes: return emotewin::box(b.x, b.y, b.w, b.h);
        case Win::Commands: return commandwin::box(b.x, b.y, b.w, b.h);
        case Win::ChatRoom: return chatroomwin::box(b.x, b.y, b.w, b.h);
        case Win::Room: return roomwin::box(b.x, b.y, b.w, b.h);
        case Win::Popup: return popupwin::box(b.x, b.y, b.w, b.h);
        case Win::Whispers: return whisperwin::box(b.x, b.y, b.w, b.h);
        case Win::VendSetup: return vendsetupwin::box(b.x, b.y, b.w, b.h);
        case Win::SlotCard: return infocardwin::box(infocardwin::Kind::Card, b.x, b.y, b.w, b.h);
    }
    return false;
}

void moveTo(Win w, int x, int y, bool keep = true) {
    switch (w) {
        case Win::Items: itemswin::moveTo(x, y, keep); break;
        case Win::Stats: statswin::moveTo(x, y); break;
        case Win::Skills: skillswin::moveTo(x, y); break;
        case Win::Equip: equipwin::moveTo(x, y, keep); break;
        case Win::Shop: shopwin::moveTo(x, y); break;
        case Win::Storage: storagewin::moveTo(x, y, keep); break;
        case Win::Cart: cartwin::moveTo(x, y, keep); break;  // 3.0.11-beta.154
        case Win::Trade: tradewin::moveTo(x, y, keep); break;
        default: break;
    }
}

// 3.0.11-beta.111 (question 1: «два окна вплотную друг к другу как одно окно»): «Спорядження» and «Речі» opened
// together stand side by side, tops in line (core/window_stack docked) - the gear left of the bag where there is room;
// the one dragged by its title leads, the other follows (kept when the drag ends); Shift held while dragging - apart.
// 3.0.11-beta.115: «Склад» in the gear's place while it is open (the storage with its bag beside it)
void dockFrame(State& s, bool opened, int targetW, int targetH) {
    const Win partner = shown(Win::Storage) ? Win::Storage : shown(Win::Trade) ? Win::Trade : Win::Equip;
    if (s.dock.on && s.dock.partner != partner) s.dock = Dock{};  // another partner: docked anew when it opens
    winstack::Box bag, gear;
    if (!boxOf(Win::Items, bag) || !boxOf(partner, gear)) {
        s.dock = Dock{};
        return;
    }
    const bool bagMoving = itemswin::moving();
    const bool gearMoving = partner == Win::Storage ? storagewin::moving()
                            : partner == Win::Trade ? tradewin::moving()
                                                    : equipwin::moving();
    const bool dragged = bagMoving || gearMoving;
    if (opened && !s.dock.on) {
        s.dock.on = true;
        s.dock.partner = partner;
        s.dock.side = winstack::dockSide(bag, gear, targetW);
        if (s.tells < kMostTold) {
            ++s.tells;
            LOG_INFO("windows: %s docked %s of «Речі»", nameOf(partner),
                     s.dock.side == winstack::Side::Left ? "left" : "right");
        }
    }
    if (!s.dock.on) return;
    if (dragged && (GetAsyncKeyState(VK_SHIFT) & 0x8000)) {
        s.dock.on = false;
        if (s.tells < kMostTold) {
            ++s.tells;
            LOG_INFO("windows: %s and «Речі» apart (dragged with Shift)", nameOf(partner));
        }
        return;
    }
    const bool gearLeads = gearMoving && !bagMoving;
    winstack::Box want = gearLeads ? winstack::docked(gear, bag, winstack::opposite(s.dock.side))
                                   : winstack::docked(bag, gear, s.dock.side);
    want.x = std::clamp(want.x, 0, std::max(0, targetW - want.w));  // the screen's edge: as near as it goes
    want.y = std::clamp(want.y, 0, std::max(0, targetH - want.h));
    const Win follower = gearLeads ? Win::Items : partner;
    const winstack::Box at = gearLeads ? bag : gear;
    if (want.x != at.x || want.y != at.y) {
        moveTo(follower, want.x, want.y, !dragged);
        s.dock.unsaved = dragged;
    } else if (!dragged && s.dock.unsaved) {
        moveTo(follower, at.x, at.y, true);  // the drag over: the follower's place kept too
        s.dock.unsaved = false;
    }
}

// 3.0.11-beta.109 (question 4 «ок»): a window just opened where another one stands - beside it (8 game pixels apart),
// else 24 down and right of it (core/window_stack place)
void placeNew(State& s, Win w, int targetW, int targetH, double k) {
    winstack::Box b;
    if (!movable(w) || !boxOf(w, b) || targetW <= 0 || targetH <= 0) return;
    std::vector<winstack::Box> open;
    for (const Win o : s.stack.bottomUp()) {
        winstack::Box ob;
        if (boxOf(o, ob)) open.push_back(ob);
    }
    const int gap = int(std::lround(8 * k)), step = int(std::lround(24 * k));
    const winstack::Box to = winstack::place(b, open, targetW, targetH, gap, step);
    if (to.x == b.x && to.y == b.y) return;
    moveTo(w, to.x, to.y);
    if (s.tells < kMostTold) {
        ++s.tells;
        LOG_INFO("windows: %s opened where another stands - at %d, %d instead of %d, %d", nameOf(w), to.x, to.y, b.x,
                 b.y);
    }
}

// Esc on the top one: closed as its own ✕ does (the NPC talk and the shop have an Esc of their own)
bool close(Win w) {
    switch (w) {
        case Win::Items: itemswin::padCommand(itemswin::PadCommand::Close); return true;
        case Win::Stats: statswin::padCommand(statswin::PadCommand::Close); return true;
        case Win::Skills: skillswin::padCommand(skillswin::PadCommand::Close); return true;
        case Win::Equip: equipwin::padCommand(equipwin::PadCommand::Close); return true;
        case Win::Reference: referencewin::padCommand(referencewin::PadCommand::Close); return true;
        case Win::Storage: storagewin::padCommand(storagewin::PadCommand::Close); return true;
        case Win::Trade: tradewin::padCommand(tradewin::PadCommand::Cancel); return true;
        case Win::ItemCard: infocardwin::close(infocardwin::Kind::Item); return true;  // 3.0.11-beta.149
        case Win::SkillCard: infocardwin::close(infocardwin::Kind::Skill); return true;
        case Win::Ask: return askcardwin::close();  // 3.0.11-beta.150: a request declined (an input: the game's)
        case Win::Message: return msgboxwin::close();  // 3.0.11-beta.151
        case Win::Cart: cartwin::padCommand(cartwin::PadCommand::Close); return true;  // 3.0.11-beta.154
        case Win::Vending: return vendshopwin::close();  // 3.0.11-beta.156: «Скасувати»
        case Win::Pick: return pickcardwin::close();     // 3.0.11-beta.157: cancel
        case Win::PartySetup: return partysetupwin::close();  // 3.0.11-beta.158: cancel
        case Win::Pet: return petcardwin::close();              // 3.0.11-beta.159
        case Win::Homun: return homuncardwin::close();          // 3.0.11-beta.160
        case Win::Merc: return merccardwin::close();            // 3.0.11-beta.161
        case Win::Options: return optionscardwin::close();      // 3.0.11-beta.162
        case Win::Social: return socialcardwin::close();        // 3.0.11-beta.163
        case Win::Guild: return guildcardwin::close();          // 3.0.11-beta.165
        case Win::WhisperSetup: return whispersetupwin::close();  // 3.0.11-beta.168
        case Win::Macros: return macrocardwin::close();         // 3.0.11-beta.169
        case Win::Tip: return tipwin::close();                  // 3.0.11-beta.204
        case Win::Emotes: return emotewin::close();             // 3.0.11-beta.205
        case Win::Commands: return commandwin::close();         // 3.0.11-beta.206
        case Win::Popup: return popupwin::close();              // 3.0.11-beta.211
        case Win::Whispers: return whisperwin::close();         // 3.0.11-beta.212
        case Win::SlotCard: infocardwin::close(infocardwin::Kind::Card); return true;  // 3.0.18-beta.139
        case Win::VendSetup: return vendsetupwin::close();      // 3.0.16-beta.33: «Скасувати»
        case Win::ChatRoom: return chatroomwin::close();        // 3.0.11-beta.207
        default: return false;
    }
}

}  // namespace

// 3.0.18-beta.151: the corner of a window one can size - the box's bottom right, 18 of the game's pixels
static bool inCorner(const winstack::Box& b, float x, float y, double k) {
    const float g = float(18 * (k > 0 ? k : 1));
    return x >= b.x + b.w - g && x <= b.x + b.w + 2 && y >= b.y + b.h - g && y <= b.y + b.h + 2;
}

static void gripDoc(const winstack::Box* b, double k) {
    static uaui::Doc doc;
    static float side = 0;
    if (!b || !uaui::ready()) {
        doc.show(false);
        return;
    }
    if (!doc.made()) {
        doc.passive = true;
        doc.top = true;
        if (!doc.make("window size grip", R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="wgr" data-model="wgrip" data-style-width="side + 'px'" data-style-height="side + 'px'"><div class="c"
  data-style-width="side + 'px'" data-style-height="side + 'px'"></div></body></rml>)RML",
                      [&](Rml::DataModelConstructor& m) { m.Bind("side", &side); }))
            return;
    }
    const float g = float(16 * (k > 0 ? k : 1));
    if (std::fabs(g - side) > 0.5f) side = g, doc.dirtyAll();
    doc.place(int(std::lround(b->x + b->w - g - 3)), int(std::lround(b->y + b->h - g - 3)));
    doc.show(true);
}

static void sizingFrame(State& s, int targetWidth, int targetHeight, double k) {
    Sizing& z = sizing();
    z.targetW = targetWidth, z.targetH = targetHeight, z.k = k;
    int cx = 0, cy = 0;
    if (z.pulling) {
        const int button = GetSystemMetrics(SM_SWAPBUTTON) ? VK_RBUTTON : VK_LBUTTON;
        const char* key = sizeKey(*z.pulling);
        if (!key || !shown(*z.pulling) || !readGameCursor(cx, cy) || GetAsyncKeyState(button) >= 0) {
            if (key) {
                writeFileAtomic(gameDir() + "\\uaro-graphics\\window-sizes.txt", z.sizes.text());
                LOG_INFO("windows: %s sized to %.0f %%", key, double(z.sizes.of(key) * 100));
            }
            z.pulling.reset();
        } else {
            const float x = float(cx * k), y = float(cy * k);
            z.sizes.set(key, windowsizes::pulled(z.from, z.w0, z.h0, z.w0 + (x - z.x0), z.h0 + (y - z.y0)));
        }
    }
    z.hot.reset();
    if (!z.pulling && readHoverCursor(cx, cy)) {
        const float x = float(cx * k), y = float(cy * k);
        for (const Win w : s.stack.topDown()) {
            winstack::Box b{};
            if (!boxOf(w, b) || x < b.x || y < b.y || x > b.x + b.w + 2 || y > b.y + b.h + 2) continue;
            if (sizeKey(w) && inCorner(b, x, y, k)) z.hot = w, z.hotBox = b;
            break;  // the top one under the pointer alone
        }
    } else if (z.pulling) {
        winstack::Box b{};
        if (boxOf(*z.pulling, b)) z.hot = z.pulling, z.hotBox = b;
    }
    gripDoc(z.hot ? &z.hotBox : nullptr, k);
}

float sizeOf(winstack::Win w) {
    const char* key = sizeKey(w);
    return key ? sizing().sizes.of(key) : 1.0f;
}

void frame(int targetWidth, int targetHeight, double k) {
    State& s = state();
    bool pairOpened = false;  // 3.0.11-beta.111: «Речі» or «Спорядження» opened this frame
    for (int i = 0; i < winstack::kWins; ++i) {
        const Win w = Win(i);
        const bool now = shown(w);
        const auto& order = s.stack.bottomUp();
        if (now && std::find(order.begin(), order.end(), w) == order.end()) {
            placeNew(s, w, targetWidth, targetHeight, k);  // the others' places before it joins them
            pairOpened = pairOpened || w == Win::Items || w == Win::Equip || w == Win::Storage || w == Win::Trade;
        }
        s.stack.see(w, now);
    }
    dockFrame(s, pairOpened, targetWidth, targetHeight);
    std::vector<Blocker> order;
    for (const Win w : s.stack.topDown()) order.push_back(blockerOf(w));
    uapad::gamewindow::setOrder(order);
    const std::optional<Win> t = s.stack.top();
    if (t != s.told && t && s.stack.bottomUp().size() >= 2) tell(s, "on top", *t);
    s.told = t;
    sizingFrame(s, targetWidth, targetHeight, k);  // 3.0.18-beta.151
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    int rank = 0;  // 3.0.18-beta.43: uaUI's windows raised to their places too
    for (const Win w : state().stack.bottomUp()) {
        uaui::setWindowRank(rank++, marked(w));
        drawOne(w, record, renderer);
    }
    uaui::setWindowRank(90);  // the windows outside the stack (the controller's tiles, the cards) over them
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    {  // 3.0.18-beta.151: a press on a window's corner - its size pulled till let go
        Sizing& z = sizing();
        if (z.pulling) {
            result = 0;
            return true;
        }
        float x = 0, y = 0;
        if (m == WM_LBUTTONDOWN && z.k > 0 &&
            pointerInTarget(game, m, lp, z.k, z.targetW, z.targetH, x, y, nullptr))
            for (const Win w : s.stack.topDown()) {
                winstack::Box b{};
                if (!boxOf(w, b) || x < b.x || y < b.y || x > b.x + b.w + 2 || y > b.y + b.h + 2) continue;
                if (sizeKey(w) && inCorner(b, x, y, z.k)) {
                    z.pulling = w;
                    z.from = z.sizes.of(sizeKey(w));
                    z.w0 = b.w, z.h0 = b.h, z.x0 = x, z.y0 = y;
                    s.stack.raise(w);
                    result = 0;
                    return true;
                }
                break;
            }
    }
    const bool press = m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDOWN || m == WM_RBUTTONDBLCLK ||
                       m == WM_MBUTTONDOWN;
    for (const Win w : s.stack.topDown()) {
        if (!mouseOne(w, game, m, wp, lp, result)) continue;
        if (press && s.stack.top() != w) {
            s.stack.raise(w);
            s.told = w;
            tell(s, "raised by a click", w);
        }
        // 3.0.11-beta.229 (R-ZUY4CM: «описание под плашкой меню»): a right click that showed a card (a skill's, a
        // thing's) keeps the card over the window clicked, also when the card was open already and only changed
        if (m == WM_RBUTTONDOWN)
            for (const Win card : {Win::SkillCard, Win::ItemCard, Win::SlotCard})
                if (card != w && shown(card) && s.stack.top() != card) {
                    s.stack.raise(card);
                    s.told = card;
                    tell(s, "on top after a right click", card);
                }
        return true;
    }
    return false;
}

bool key(UINT m, WPARAM wp, LPARAM lp) {
    State& s = state();
    const std::optional<Win> t = s.stack.top();
    if (t && keyOne(*t, m, wp, lp)) return true;  // the top's own keys first
    if (t && m == WM_KEYDOWN && wp == VK_ESCAPE && chatwin::inputEmpty() && close(*t)) {
        tell(s, "closed by Esc (the top one)", *t);
        return true;
    }
    for (const Win w : s.stack.topDown())
        if ((!t || w != *t) && keyOne(w, m, wp, lp)) return true;
    return !referencewin::isOpen() && referencewin::key(m, wp, lp);  // Ctrl+F opens it
}

std::optional<winstack::Win> top() { return state().stack.top(); }

bool padCross(winstack::Win from, bool right) {
    State& s = state();
    // «Склад» or «Обмін» and «Речі» (the gear's slots go round with left and right of their own: no way back yet)
    if (!s.dock.on || s.dock.partner == Win::Equip || !shown(Win::Items) || !shown(s.dock.partner)) return false;
    const bool partnerLeft = s.dock.side == winstack::Side::Left;
    const Win to = from == Win::Items && right != partnerLeft       ? s.dock.partner
                   : from == s.dock.partner && right == partnerLeft ? Win::Items
                                                                    : from;
    if (to == from) return false;
    s.stack.raise(to);
    tell(s, "raised by the controller's D-pad past its neighbour's edge", to);
    return true;
}

bool marked(winstack::Win w) { return state().stack.marked(w); }

}  // namespace uagfx::win::windowstack

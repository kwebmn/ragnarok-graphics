#include "win/hotbar.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "core/buff_panel.hpp"
#include "core/char_status.hpp"
#include "core/hotbar_bars.hpp"
#include "core/hotbar_panel.hpp"
#include "core/skill_area.hpp"
#include "core/layout.hpp"
#include "core/settings/strings.hpp"
#include "core/skill_costs.hpp"
#include "core/ui_style.hpp"
#include "core/window_drag.hpp"
#include "pad/win/client_bar.hpp"
#include "pad/win/game_window.hpp"
#include "pad/win/runtime.hpp"
#include "win/ammo_watch.hpp"
#include "win/chat_window.hpp"
#include "win/client_tables.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/ground_units.hpp"
#include "core/potion_warn.hpp"
#include "core/supplies.hpp"
#include "core/ui_chime.hpp"
#include "win/hud_edit.hpp"
#include "win/skill_delay_watch.hpp"
#include "win/sphere_watch.hpp"
#include "win/combo_watch.hpp"
#include "win/buff_overlay.hpp"
#include "win/sound_hook.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/reference_load.hpp"
#include "win/ui_cost.hpp"
#include "win/uaui.hpp"
#include "core/pad_glyphs.hpp"
#include "core/ui_icons.hpp"

namespace uagfx::win::hotbarwin {
namespace {

namespace hb = uagfx::hotbar;

// uaRO.exe's: the session's slots (27, a row 9: an MSVC 6 std::string 16 bytes each - its c_str at +4, its length at
// +8 - and a skill's level), its bag (a list: a node's next at +0, its ITEM_INFO at +8 - m_num +0x10, m_itemName
// +0x2C, identified +0x3C), the row shown (F12 goes round them, FUN_004d8f10) and the battle mode's flag
constexpr uint32_t kSession = 0x771148, kSlotNames = kSession + 0x314, kSlotLevels = kSession + 0x4C4;
constexpr uint32_t kBag = kSession + 0xF1C, kRow = 0x771458, kBattle = 0x772D54;
constexpr uint32_t kItemCount = 8 + 0x10, kItemName = 8 + 0x2C, kItemKnown = 8 + 0x3C;
// The windows' manager: its bar (UI window 0x24, its class's vtable; FUN_004d42d0 makes it in the list always), the
// player's own choice for it (+1000, SHORTCUTWNDINFO_SHOW), its windows' list; a window's shown flag and place; the
// bar's SendMsg FUN_0046e470 (message 0x29: a slot used - its cell, in the battle mode its row)
constexpr uint32_t kWindowMgr = 0x727ED8, kBar = kWindowMgr + 0x9C, kBarClass = 0x6985D0;
constexpr uint32_t kBarChoice = kWindowMgr + 1000;
constexpr uint32_t kWindows = 0x38, kShown = 0x38, kRect = 0x14, kSendMsgSlot = 0x94, kSendMsg = 0x46E470;
constexpr int kUseMsg = 0x29, kSlots = 27;
// The bar's drop (FUN_0046dae0, its class's slot + 0x2C; its x and y the window's own - FUN_0046ebe0 finds a cell by
// x 4 + 29 a cell, y 0-23 - and the slot the row shown's)
constexpr uint32_t kDropSlot = 0x2C, kBarDrop = 0x46DAE0;
constexpr int kCellX0 = 4, kCellW = 29, kCellY = 12;
using DropFn = void(__attribute__((thiscall)) *)(uint32_t self, int x, int y, uint32_t drag);
// 3.0.11-beta.147: a cell's card - the bar's right click (FUN_0046e2e0, its class's slot + 0x80: a skill's description
// window 0x2E told 0x3D, an item's card 0xC told 0x18; its x and y the window's own as the drop's)
constexpr uint32_t kInfoSlot = 0x80, kBarInfo = 0x46E2E0;
using InfoFn = void(__attribute__((thiscall)) *)(uint32_t self, int x, int y);
// 3.0.11-beta.147: the battle mode as the game's /battlemode turns it (FUN_00567900, its command 0x86 at 0x568C31, its
// line's tail at 0x56AFE7 - both byte for byte): the flag flipped (1 - it), the windows' manager's message 3
// (FUN_004db310 - its chat window shown, told 0x10), then 1 with the mode's line (FUN_00504fb0, cdecl: msgstringtable
// 0x311 on, 0x312 off) in the game's colour
constexpr uint32_t kBattleCode = 0x568C31, kLineCode = 0x56AFE7, kPush = 0x4DB310, kMsgString = 0x504FB0;
constexpr int kBattleOnLine = 0x311, kBattleOffLine = 0x312, kLineColour = 0x64FFFF;
const uint8_t kBattleCodeBytes[] = {0xA1, 0x54, 0x2D, 0x77, 0x00, 0xBA, 0x01, 0x00, 0x00, 0x00, 0x56, 0x56, 0x2B,
                                    0xD0, 0x56, 0x6A, 0x03, 0xB9, 0xD8, 0x7E, 0x72, 0x00, 0x89, 0x15, 0x54, 0x2D,
                                    0x77, 0x00, 0xE8, 0xBE, 0x26, 0xF7};
const uint8_t kLineCodeBytes[] = {0xE8, 0xC4, 0x9F, 0xF9, 0xFF, 0x83, 0xC4, 0x04, 0x50, 0x6A, 0x01,
                                  0xB9, 0xD8, 0x7E, 0x72, 0x00, 0xE8, 0x14, 0x03, 0xF7, 0xFF};
using PushFn = int(__attribute__((thiscall)) *)(uint32_t manager, int msg, const char* text, int colour, int more);
using MsgStringFn = const char* (*)(int id);
constexpr ULONGLONG kUnseenUpMs = 250;  // the button up this long with no message of it: the press forgotten
// 3.0.11-beta.45 (the concept's cell states): the session's skills (a list: a node's next at +0; its id +0xC, its
// learned level +0x14, that level's SP +0x18 - FUN_00643040 keeps 0x010f's there), the bar's own each slot's skill id
// (+0x64) and whether it is a skill (+0xD0) - SendMsg 0x17 fills them anew at each change, 0x29 uses them; the game
// mode (its class's vtable) waiting for a skill's target: its state +0x3C4 (1 a place, 2 an enemy, 3, 4 a friend, 5
// a trap - SendMsg 0x4F; 0 none) and that skill's id +0x3C8; the chat's input taking the keys (the battle mode's
// letters are the chat's then, FUN_004d8f10)
constexpr uint32_t kSkills = kSession + 0x2D8, kSkillId = 0xC, kSkillLevel = 0x14, kSkillSp = 0x18;
constexpr uint32_t kBarIds = 0x64, kBarIsSkill = 0xD0;
constexpr uint32_t kModeClass = 0x69F388, kWaitState = 0x3C4, kWaitSkill = 0x3C8, kChatTyping = 0x772D58;
constexpr ULONGLONG kTipMs = 400;  // the concept's: a tip after 0.4 s
constexpr ULONGLONG kReadMs = 150, kPressMs = 160;
constexpr size_t kMostNodes = 1024;
constexpr ULONGLONG kAsideMs = 1500;  // 3.0.11-beta.117: a window over its place this long moves it aside
using SendMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);
// 3.0.11-beta.78 (the owner: «на панели скилов не те скилы появляются, с других аккаунтов, плюс появляются те которых
// на чаре нету»): the game keeps its bar in the registry by the character's place on the selection screen
// (FUN_004d0500: SITEM<slot><langtype><page x 3 + index>), every account's alike - each character's own is put into
// the session as that loader does: the slots and their levels cleared (FUN_006278e0, FUN_006425d0, ECX the session),
// then for each slot its row byte (0x771458) and an item's FUN_006279b0 (name, slot), a skill's level FUN_006425f0
// (level, slot) and FUN_00642630 (name, level, slot) - thiscall, the callee's stack; the bar told 0x17 after
constexpr uint32_t kClearSlots = 0x6278E0, kSetItem = 0x6279B0, kClearLevels = 0x6425D0, kSetLevel = 0x6425F0;
constexpr uint32_t kSetSkill = 0x642630, kSkillName = 0x24;  // and a skill node's AEGIS name (char*)
constexpr int kRefreshMsg = 0x17;
const uint8_t kClearSlotsCode[] = {0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x08, 0x53, 0x56, 0x57, 0x8D, 0x99, 0x18, 0x03};
const uint8_t kSetItemCode[] = {0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x0C, 0x8B, 0xD1, 0x33, 0xC0, 0x53, 0x56, 0x8A, 0x82};
const uint8_t kClearLevelsCode[] = {0x57, 0x8D, 0xB9, 0xC4, 0x04, 0x00, 0x00, 0xB9, 0x1B, 0x00, 0x00, 0x00};
const uint8_t kSetLevelCode[] = {0x55, 0x8B, 0xEC, 0x8B, 0x45, 0x0C, 0x8B, 0x55, 0x08, 0x89, 0x94, 0x81, 0xC4, 0x04};
const uint8_t kSetSkillCode[] = {0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x10, 0x53, 0x56, 0x8B, 0xF1, 0x33, 0xC0, 0x57};
using ClearFn = void(__attribute__((thiscall)) *)(uint32_t session);
using SetItemFn = void(__attribute__((thiscall)) *)(uint32_t session, const char* name, int slot);
using SetLevelFn = void(__attribute__((thiscall)) *)(uint32_t session, int level, int slot);
using SetSkillFn = void(__attribute__((thiscall)) *)(uint32_t session, const char* name, int level, int slot);
constexpr ULONGLONG kBarReadMs = 500, kBarSettleMs = 1000, kLearnedWaitMs = 10000;

struct Slot {
    hb::Slot what;
    int level = 0;
    int skillId = 0;  // 3.0.11-beta.45: the game's bar's
};

struct Learned {
    int level = 0, sp = 0;
};

struct State {
    bool uiShown = false;  // 3.0.18-beta.21: drawn by uaUI
    bool delayRing = true;  // 3.0.11-beta.276: [interface] delay_ring
    bool potionWarn = true;  // 3.0.11-beta.281: [interface] potion_warn
    potionwarn::Watch potions;
    bool suppliesOn = false;      // 3.0.16-beta.8: [interface] hotbar_supplies
    bool needsOn = false, comboOn = false;  // 3.0.16-beta.13: [interface] skill_needs, combo_window
    int lackTold = 0, comboTold = 0;
    potionwarn::Watch supplyWatch;
    std::string suppliesTold;  // the pick last told the log
    int supplyChimes = 0;
    std::set<std::string> healing;  // the bar's healing items now (their counts red when low)
    int potionTold = 0;
    // 3.0.11-beta.117 (R-8YW96B, R-5JGPFX: it jumped right for a second while walking): moved aside only for a window
    // standing over its place 1.5 s; what was there told the log (the first few)
    hb::Settle settle;
    std::string overWhat;
    int asideTold = 0;
    bool on = false, shown = false, known = false, checked = false, hid = false, oldTold = false;
    bool padTold = false;  // 3.0.11-beta.86: the controller's buttons shown, as last told the log
    unsigned codePage = 0;  // 3.0.11-beta.91: the names' (slotWords)
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    hb::Mode mode = hb::Mode::Normal;
    int row = 0;
    Slot slots[kSlots];
    std::map<std::string, int> bag;
    std::map<int, Learned> skills;  // 3.0.11-beta.45: by the skill's id
    int sp = -1, waiting = 0;       // the SP there (-1 unknown), the skill the game waits a target for
    std::string map;                // the map's code, its flags the reference's
    std::vector<std::string> flags;
    ULONGLONG hoverSince = 0;
    int lastHover = -1;
    bool pricesTold = false;
    int waitsTold = 0, barredTold = 0;
    std::string barred;  // the cells «не тут» last logged
    ULONGLONG readAt = 0;
    std::string told;  // the slots last logged
    std::vector<hb::Cell> cells;
    hb::Layout layout;
    int hover = -1, pressed = -1;
    bool hoverToggle = false;  // 3.0.11-beta.147: the battle mode's button
    ULONGLONG pressedUntil = 0;
    // 3.0.11-beta.147 (the owner: drag a cell's icon to another cell, off the bar to take it off; «иногда панель сама
    // двигается»): a press on a cell's icon (lifted once held and moved - core/hotbar_panel Lift), the game's slot it
    // keeps, the pointer (the picture's pixels), well off the bar or not; the button seen up since (its message to
    // come)
    hb::Lift lift;
    int liftSlot = -1;
    float liftX = 0, liftY = 0;
    bool liftAway = false;
    bool liftReal = false;  // the mouse's own button down at the press (the controller's posted presses are not)
    ULONGLONG liftUpSince = 0;
    panel::Picture liftPicture;  // the lifted icon at the cursor
    uint32_t liftTexture = 0;
    int liftTexW = 0, liftTexH = 0;
    bool liftUpload = false;
    std::string liftKey;
    int dragsTold = 0, cardsTold = 0;
    bool battleChecked = false, battleCan = false;
    std::string key;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    int usesLogged = 0;
    // 3.0.11-beta.78: each character's own bar (core/hotbar_bars, uaro-graphics\hotbar-bars.txt)
    bool barsChecked = false, barsCan = false, barsRead = false;
    hb::Bars bars;
    std::string barOf;  // the character whose own bar the game's is now ("" none yet)
    int profile = 1;    // 3.0.11-beta.288 (the owner's feature 20): its bar's profile now (1-3, Ctrl+Shift+1..3)
    ULONGLONG profileShownAt = 0;
    int levelTold = 0;  // 3.0.11-beta.289
    enum class BarStep : uint8_t { None, Put, Kept } barStep = BarStep::None;
    ULONGLONG barSince = 0, barReadAt = 0, barChangedAt = 0;
    hb::Bar barKept;                // as last kept for barOf
    std::set<std::string> learned;  // the character's learned skills' AEGIS names (the session's list)
    bool learnedCame = false;       // that list there at all
    // 3.0.11-beta.125: the ammo's pill - its warning shown by how low it fell, the one last told the log
    hb::AmmoWarn ammoWarn;
    int ammoTold = 0;
    bool belowTold = false;  // 3.0.12-beta.2
    bool ammoOn = true;
    bool countsOn = true;  // 3.0.11-beta.143: [interface] ground_counts
    bool timersOn = false;  // 3.0.16-beta.22: [interface] ground_timers - their time on the slots too
    std::vector<groundtimers::Ring> rings;  // the frame's
    int timersTold = 0;
};

State& state() {
    static State s;
    return s;
}

// 3.0.11-beta.281 (the owner's feature 16): the bar's healing items (the reference pack's type 0) - a fall to 10 or
// fewer a soft chime, none left a lower one (the arrows' chimes, core/ui_chime); their counts red from 10 down
void potionsLow(State& s) {
    std::map<int, int> counts;
    s.healing.clear();
    const auto index = reference::index();
    if (!index) return;
    for (const Slot& at : s.slots) {
        if (at.what.kind != hb::Slot::Kind::Item) continue;
        const int id = std::atoi(at.what.id.c_str());
        const auto it = index->items.find(id);
        if (id <= 0 || it == index->items.end() || it->second.type != 0) continue;
        s.healing.insert(at.what.id);
        const auto b = s.bag.find(at.what.id);
        counts[id] = b != s.bag.end() ? b->second : 0;
    }
    const int fresh = s.potions.step(counts, !s.bag.empty());
    if (!s.potionWarn || fresh < 2) return;
    static const std::vector<uint8_t> low = chime::wav(chime::Kind::AmmoLow), out = chime::wav(chime::Kind::AmmoOut);
    const bool played = win::playOwn(fresh == 3 ? out.data() : low.data());
    if (s.potionTold++ < 20)
        LOG_INFO("hotbar: a healing item %s - the chime %s", fresh == 3 ? "ran out" : "fell to 10 or fewer",
                 played ? "played" : "not played: the game's sound is off");
}

std::string say(settings::Text t) { return settings::tr(language(), t); }

uint32_t read32(uint32_t at) {
    uint32_t v = 0;
    return readGame(at, &v, 4) ? v : 0;
}

bool write32(uint32_t at, uint32_t v) {
    SIZE_T n = 0;
    return WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(uintptr_t(at)), &v, 4, &n) && n == 4;
}

bool write8(uint32_t at, uint8_t v) {
    SIZE_T n = 0;
    return WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(uintptr_t(at)), &v, 1, &n) && n == 1;
}

bool sameCode(uint32_t at, const uint8_t* code, size_t size) {
    uint8_t got[32] = {};
    return size <= sizeof got && readGame(at, got, size) && std::memcmp(got, code, size) == 0;
}

std::string readText(uint32_t object) {  // an MSVC 6 std::string
    const uint32_t ptr = read32(object + 4), len = read32(object + 8);
    if (!ptr || !len || len > 64) return "";
    std::string out(len, '\0');
    return readGame(ptr, out.data(), len) ? out : "";
}

// The game's bar when it is that window of this build
uint32_t gameBar() {
    const uint32_t window = read32(kBar);
    return window && read32(window) == kBarClass ? window : 0;
}

// Its shown flag held: 0 while this bar shows (each frame - the game shows it again on a new map and on F12); off -
// back as the player had it
void holdGameBar(State& s, bool on) {
    const uint32_t window = gameBar();
    if (!window) {
        if (on && !s.oldTold) {
            s.oldTold = true;
            LOG_INFO("hotbar: the game's hotkeys bar is not where this build has it - it stays as it is");
        }
        return;
    }
    const uint32_t shown = read32(window + kShown);
    if (on) {
        if (shown && write32(window + kShown, 0)) {
            if (!s.hid) LOG_INFO("hotbar: the game's hotkeys bar hidden for the new one (its shown flag 0)");
            s.hid = true;
        }
    } else if (s.hid) {
        const uint32_t want = read32(kBarChoice) ? 1 : 0;
        if (shown != want) write32(window + kShown, want);
        s.hid = false;
        LOG_INFO("hotbar: the game's hotkeys bar back as the player had it (%s)", want ? "shown" : "closed");
    }
}

// One pass over the bag: each identified item's count by its number (as the game finds a slot's item by its name)
std::map<std::string, int> readBag() {
    std::map<std::string, int> out;
    const uint32_t head = read32(kBag);
    if (!head) return out;
    uint32_t node = read32(head);
    for (size_t n = 0; node && node != head && n < kMostNodes; ++n, node = read32(node)) {
        uint8_t raw[kItemKnown + 4] = {};
        if (!readGame(node, raw, sizeof raw)) break;
        int32_t count = 0;
        uint32_t ptr = 0, len = 0;
        std::memcpy(&count, raw + kItemCount, 4);
        std::memcpy(&ptr, raw + kItemName + 4, 4);
        std::memcpy(&len, raw + kItemName + 8, 4);
        if (!raw[kItemKnown] || !ptr || !len || len > 64) continue;
        std::string name(len, '\0');
        if (readGame(ptr, name.data(), len)) out[name] += std::max(0, count);
    }
    return out;
}

// 3.0.11-beta.45: the session's skills - each learned one's level and that level's SP, as 0x010f gave them
std::map<int, Learned> readSkills() {
    std::map<int, Learned> out;
    const uint32_t head = read32(kSkills);
    if (!head) return out;
    uint32_t node = read32(head);
    for (size_t n = 0; node && node != head && n < kMostNodes; ++n, node = read32(node)) {
        uint8_t raw[kSkillSp + 4] = {};
        if (!readGame(node, raw, sizeof raw)) break;
        int32_t id = 0, level = 0, sp = 0;
        std::memcpy(&id, raw + kSkillId, 4);
        std::memcpy(&level, raw + kSkillLevel, 4);
        std::memcpy(&sp, raw + kSkillSp, 4);
        if (id > 0 && id < 0x10000 && level >= 0 && level < 100) out[id] = Learned{level, sp};
    }
    return out;
}

// Once: whether the server's table and the game's own words of the learned levels' prices agree
void tellPrices(State& s) {
    if (s.pricesTold || s.skills.empty()) return;
    s.pricesTold = true;
    int same = 0, unknown = 0;
    std::string differ;
    for (const auto& [id, l] : s.skills) {
        if (l.level <= 0) continue;
        const int table = skillcost::sp(id, l.level);
        if (table < 0) ++unknown;
        else if (table == l.sp) ++same;
        else if (differ.size() < 160)
            differ += " " + std::to_string(id) + " Lv" + std::to_string(l.level) + " " + std::to_string(table) + "/" +
                      std::to_string(l.sp);
    }
    LOG_INFO("hotbar: SP prices - the server's table and the game's own agree for %d learned skills, %d not in the "
             "table%s%s",
             same, unknown, differ.empty() ? "" : "; differ (id, level, table/game):", differ.c_str());
}

// The skill the game waits a target for (its mode's state), 0 none
int waitingSkill() {
    const uint32_t mode = readGameMode();
    if (!mode || read32(mode) != kModeClass) return 0;
    const uint32_t state = read32(mode + kWaitState);
    return state >= 1 && state <= 5 ? int(read32(mode + kWaitSkill)) : 0;
}

// 3.0.11-beta.78: the character's learned skills' AEGIS names (the session's list - a node's name at +0x24 - those
// above level 0); `came`: the list there at all (the server sent it)
std::set<std::string> readLearned(bool* came = nullptr) {
    std::set<std::string> out;
    if (came) *came = false;
    const uint32_t head = read32(kSkills);
    if (!head) return out;
    uint32_t node = read32(head);
    for (size_t n = 0; node && node != head && n < kMostNodes; ++n, node = read32(node)) {
        if (came) *came = true;
        const uint32_t name = read32(node + kSkillName);
        const int level = int(read32(node + kSkillLevel));
        char text[48] = {};
        if (level <= 0 || level >= 100 || !name || !readGame(name, text, sizeof text - 1)) continue;
        text[sizeof text - 1] = '\0';
        if (text[0]) out.insert(text);
    }
    return out;
}

// The game's bar as its session has it now
hb::Bar barNow() {
    hb::Bar bar;
    for (int j = 0; j < kSlots; ++j) {
        std::string v = readText(kSlotNames + 16 * uint32_t(j));
        bar.slots[size_t(j)] = v == "nothing" ? "" : v;
        const int level = int(read32(kSlotLevels + 4 * uint32_t(j)));
        bar.levels[size_t(j)] = level > 0 && level < 100 ? level : 0;
    }
    return bar;
}

// A bar put into the game's session as its loader puts the registry's (FUN_004d0500), the bar told to fill its own
void putBar(const hb::Bar& bar) {
    uint8_t row = 0;
    readGame(kRow, &row, 1);
    reinterpret_cast<ClearFn>(uintptr_t(kClearSlots))(kSession);
    reinterpret_cast<ClearFn>(uintptr_t(kClearLevels))(kSession);
    for (int j = 0; j < kSlots; ++j) {
        write8(kRow, uint8_t(j / 9));  // the setters' row (each keeps a name once a row)
        const std::string& v = bar.slots[size_t(j)];
        const int level = bar.levels[size_t(j)];
        reinterpret_cast<SetLevelFn>(uintptr_t(kSetLevel))(kSession, level, j);
        if (v.size() > 2 && v[0] == '#' && v[1] == '#')
            reinterpret_cast<SetSkillFn>(uintptr_t(kSetSkill))(kSession, v.c_str(), level, j);
        else
            reinterpret_cast<SetItemFn>(uintptr_t(kSetItem))(kSession, v.c_str(), j);  // "" - "nothing"
    }
    write8(kRow, row < 3 ? row : 0);
    if (const uint32_t window = gameBar())
        reinterpret_cast<SendMsgFn>(uintptr_t(kSendMsg))(window, 0, kRefreshMsg, 0, 0, 0);
}

// The character's bar kept: the file read again first (another game window keeps its characters' there too)
void keepBar(State& s, const std::string& name, const hb::Bar& bar) {
    const std::string path = layout::join(gameDir(), layout::kHotbarBars);
    std::string text;
    if (readTextFile(path, text))
        for (const auto& [other, kept] : hb::parseBars(text))
            if (other != name) s.bars[other] = kept;
    s.bars[name] = bar;
    if (!writeFileAtomic(path, hb::barsText(s.bars))) LOG_INFO("hotbar: %s cannot be written", layout::kHotbarBars);
}

// 3.0.11-beta.288 (the owner's feature 20): a character's bar's profile kept as «<name>#<n>» (the first its name alone)
std::string profileKey(const std::string& name, int profile) {
    return profile <= 1 ? name : name + "#" + std::to_string(profile);
}

int filled(const hb::Bar& bar) {
    int n = 0;
    for (const std::string& v : bar.slots) n += !v.empty();
    return n;
}

// 3.0.11-beta.78: [interface] hotbar_per_character - the character's own bar put into the game when it comes in (by
// its name); met the first time, the game's own kept without the skills it has not learned; its changes kept
void ownBar(State& s, bool own, unsigned codePage) {
    if (!s.barsChecked) {
        s.barsChecked = true;
        s.barsCan = sameCode(kClearSlots, kClearSlotsCode, sizeof kClearSlotsCode) &&
                    sameCode(kSetItem, kSetItemCode, sizeof kSetItemCode) &&
                    sameCode(kClearLevels, kClearLevelsCode, sizeof kClearLevelsCode) &&
                    sameCode(kSetLevel, kSetLevelCode, sizeof kSetLevelCode) &&
                    sameCode(kSetSkill, kSetSkillCode, sizeof kSetSkillCode);
        if (!s.barsCan)
            LOG_INFO("hotbar: the game's bar setters are not this build's - no bar of each character's own");
    }
    if (!own || !s.barsCan) {
        s.barStep = State::BarStep::None;
        s.barOf.clear();
        return;
    }
    if (!s.barsRead) {
        s.barsRead = true;
        std::string text;
        if (readTextFile(layout::join(gameDir(), layout::kHotbarBars), text)) s.bars = hb::parseBars(text);
        LOG_INFO("hotbar: %zu characters' own bars known (%s)", s.bars.size(), layout::kHotbarBars);
    }
    const ULONGLONG now = GetTickCount64();
    const std::string name = readOwnName();
    if (name.empty()) return;
    if (name != s.barOf) {  // another character came in: its own bar
        s.barOf = name;
        s.profile = 1;  // 3.0.11-beta.288: its first profile
        s.barStep = State::BarStep::Put;
        s.barSince = now;
        s.barChangedAt = 0;
    }
    if (s.barStep == State::BarStep::Put) {
        const auto it = s.bars.find(name);
        if (it != s.bars.end()) {
            putBar(it->second);
            s.barKept = it->second;
            s.barStep = State::BarStep::Kept;
            LOG_INFO("hotbar: %s's own bar put back - %d cells", utf8Of(widened(name, codePage)).c_str(),
                     filled(it->second));
            return;
        }
        // met the first time: the game's own (by the selection screen's place) less the skills it has not learned
        bool came = false;
        const std::set<std::string> learned = readLearned(&came);
        if (!came && now - s.barSince < kLearnedWaitMs) return;  // its skills not come yet
        hb::Bar bar = barNow();
        const int dropped = came ? hb::dropUnlearned(bar, learned) : 0;
        if (dropped) putBar(bar);
        keepBar(s, name, bar);
        s.barKept = bar;
        s.barStep = State::BarStep::Kept;
        LOG_INFO("hotbar: %s met the first time - the game's bar kept as its own (%d cells), %d skills it has not "
                 "learned taken out",
                 utf8Of(widened(name, codePage)).c_str(), filled(bar), dropped);
        return;
    }
    if (s.barStep != State::BarStep::Kept || now - s.barReadAt < kBarReadMs) return;
    s.barReadAt = now;
    const hb::Bar bar = barNow();  // the player's own changes kept (a second still)
    if (bar == s.barKept) {
        s.barChangedAt = 0;
    } else if (!s.barChangedAt) {
        s.barChangedAt = now;
    } else if (now - s.barChangedAt >= kBarSettleMs) {
        s.barChangedAt = 0;
        s.barKept = bar;
        keepBar(s, profileKey(name, s.profile), bar);  // 3.0.11-beta.288: its profile's
        LOG_INFO("hotbar: %s's own bar kept (profile %d) - %d cells", utf8Of(widened(name, codePage)).c_str(),
                 s.profile, filled(bar));
    }
}

void readSlots(State& s) {
    const uint32_t bar = gameBar();
    for (int j = 0; j < kSlots; ++j) {
        s.slots[j].what = hb::slotOf(readText(kSlotNames + 16 * uint32_t(j)));
        const int level = int(read32(kSlotLevels + 4 * uint32_t(j)));
        s.slots[j].level = level > 0 && level < 100 ? level : 0;
        const bool skill = bar && read32(bar + kBarIsSkill + 4 * uint32_t(j));
        s.slots[j].skillId = skill ? int(read32(bar + kBarIds + 4 * uint32_t(j)) & 0xFFFF) : 0;
    }
    s.bag = readBag();
    potionsLow(s);  // 3.0.11-beta.281
    s.skills = readSkills();
    bool came = false;  // 3.0.11-beta.78: a skill's cell greyed when the character has not learned it
    s.learned = readLearned(&came);
    s.learnedCame = came;
    tellPrices(s);
    charstatus::Status st;
    s.sp = readOwnStatus(st) && st.spMost > 0 ? st.sp : -1;
    s.map = readGameMap();
    s.flags.clear();
    if (const std::shared_ptr<const refindex::Index> ix = reference::index()) {
        const auto m = ix->maps.find(s.map);
        if (m != ix->maps.end()) s.flags = m->second.flags;
    }
    std::string told;
    for (int j = 0; j < kSlots; ++j) {
        const Slot& slot = s.slots[j];
        if (slot.what.kind == hb::Slot::Kind::None) continue;
        told += " " + std::to_string(j / 9 + 1) + ":F" + std::to_string(j % 9 + 1) + "=" + slot.what.id;
        if (slot.what.kind == hb::Slot::Kind::Skill) told += " Lv" + std::to_string(slot.level);
    }
    if (told != s.told) {
        s.told = told;
        LOG_INFO("hotbar: the game's slots -%s", told.empty() ? " none" : told.c_str());
    }
}

// 3.0.11-beta.74: the shared icons (win/client_tables - the AI pack's when it has them, else the game's own)
const panel::Picture* iconOf(State& s, const std::string& file, int side) {
    return clienttables::icon(file, side, 4 * s.scale);
}

// 3.0.11-beta.143 (the owner's concept «маг, прист и хант на геймпаде»): a ground skill's own count on its slot - its
// walls standing of the most at once (core/ground_units: Fire Wall, Quagmire, Fire Pillar), its edge flashing amber
// 1.5 s after one burnt out; a trap's Traps (item 1065) in the bag
void groundCount(State& s, uint16_t skill, hb::Cell& c) {
    constexpr uint64_t kFlashMs = 1500;
    if (const int most = skillarea::most(skill); most > 0) {
        const groundunits::Tracker& t = groundunitswin::tracker();
        if (const int n = t.standing(skill); n > 0) c.standing = std::to_string(n) + "/" + std::to_string(most);
        const uint64_t out = t.burntOut(skill), now = GetTickCount64();
        if (out > 0 && now >= out && now - out < kFlashMs) c.flash = 1.0f - float(now - out) / float(kFlashMs);
    } else if (skillarea::kindOf(skill) == skillarea::Kind::Trap) {
        const auto b = s.bag.find("1065");
        const int n = b != s.bag.end() ? b->second : 0;
        c.standing = "\xC3\x97" + std::to_string(n);
        c.standingOut = n == 0;
    }
}

// 3.0.16-beta.22 (the owner: the time ring hidden under the character standing on it - «1»): a ground skill's own
// casts standing (win/ground_units rings, the frame's) - the soonest to end: its share left, its seconds, «×n» of them
void timerLook(State& s, uint16_t skill, hb::Cell& c) {
    const uint64_t now = GetTickCount64();
    const groundtimers::Ring* soonest = nullptr;
    int n = 0;
    for (const groundtimers::Ring& r : s.rings)
        if (r.skill == skill && r.endMs > now) {
            ++n;
            if (!soonest || r.endMs < soonest->endMs) soonest = &r;
        }
    if (!soonest) return;
    c.timer = std::max(0.02f, soonest->left(now));
    c.timerText = std::to_string(int((soonest->endMs - now + 999) / 1000));
    if (n > 1) c.timerText += " \xC3\x97" + std::to_string(n);
    if (s.timersTold++ < 10) LOG_INFO("hotbar: skill %u's time on its slot (%s)", unsigned(skill), c.timerText.c_str());
}

// 3.0.16-beta.13 (the owner's pick 1 of «20 идей»): what a skill lacks for one use - a gem (or other item) none in the
// bag, fewer spheres than it takes (Asura in a combo takes what is left); an empty bag (a map loading) tells nothing
void needs(State& s, uint16_t skill, int level, hb::Cell& c, unsigned codePage) {
    if (s.bag.empty()) return;
    for (int id : supplies::itemsAt(skill, level)) {
        const auto b = s.bag.find(std::to_string(id));
        if (b != s.bag.end() && b->second > 0) continue;
        const auto& items = clienttables::itemNames();
        const auto n = items.find(std::to_string(id));
        const std::string name =
            n != items.end() ? utf8Of(widened(hb::spaced(n->second), codePage)) : "#" + std::to_string(id);
        c.lack = "\xC3\x97" "0";  // «×0»
        c.lackWhy = settings::fmt(say(settings::Text::HotbarNoItem), {name});
        if (s.lackTold++ < 10) LOG_INFO("hotbar: skill %u lacks item #%d", unsigned(skill), id);
        return;
    }
    int need = supplies::spheresFor(skill, level);
    const std::vector<uint16_t> chain = combowatch::next();
    if (skill == 271 && std::find(chain.begin(), chain.end(), uint16_t(271)) != chain.end()) need = 1;
    const int have = spherewatch::count();
    if (need > 0 && have < need) {
        c.lack = std::to_string(have) + "/" + std::to_string(need);
        c.lackWhy = settings::fmt(say(settings::Text::HotbarNoSpheres), {std::to_string(need), std::to_string(have)});
    }
}

// 3.0.16-beta.13 (the owner's picks 5, 6): it chains now (the combo's window), Asura ready (5 spheres, Fury - the
// status icon 86 - on, not short of SP)
void comboLook(State& s, uint16_t skill, hb::Cell& c) {
    const std::vector<uint16_t> chain = combowatch::next();
    if (std::find(chain.begin(), chain.end(), skill) != chain.end()) {
        c.combo = std::max(0.02f, combowatch::left());
        if (s.comboTold++ < 10) LOG_INFO("hotbar: skill %u chains now (the combo's window)", unsigned(skill));
    }
    if (skill == 271 && spherewatch::count() >= 5 && buffoverlay::timers().on(86) && c.lack.empty() &&
        !(c.spNeed > 0 && c.spHave < c.spNeed))
        c.ready = true;
}

hb::Cell cellOf(State& s, int slot, const std::string& key, unsigned codePage) {
    hb::Cell c;
    c.key = key;
    const Slot& at = s.slots[slot];
    const int side = hb::iconSide(s.mode, s.scale);
    if (at.what.kind == hb::Slot::Kind::Skill) {
        const auto& skills = clienttables::skillNames();
        const auto n = skills.find(at.what.id);
        c.name = n != skills.end() ? hb::spaced(n->second) : at.what.id;
        c.level = at.level;
        c.icon = iconOf(s, hb::lowered(at.what.id), side);
        c.unlearned = s.learnedCame && !s.learned.count(at.what.id);  // 3.0.11-beta.78
        c.selected = s.waiting > 0 && at.skillId == s.waiting;
        const auto learned = s.skills.find(at.skillId);
        const int level = at.level > 0 ? at.level : learned != s.skills.end() ? learned->second.level : 0;
        c.levelMost = learned != s.skills.end() ? learned->second.level : 0;  // 3.0.11-beta.289: the wheel's
        int price = at.skillId > 0 ? skillcost::sp(at.skillId, level) : -1;
        if (learned != s.skills.end() && learned->second.level == level && learned->second.sp > 0)
            price = learned->second.sp;  // the server's own word for the level learned
        if (s.sp >= 0 && price > 0) {
            c.spNeed = price;
            c.spHave = s.sp;
        }
        if (s.countsOn && at.skillId > 0) groundCount(s, uint16_t(at.skillId), c);  // 3.0.11-beta.143
        if (s.needsOn && at.skillId > 0 && !c.unlearned) needs(s, uint16_t(at.skillId), level, c, codePage);  // .13
        if (s.comboOn && at.skillId > 0 && !c.unlearned) comboLook(s, uint16_t(at.skillId), c);
        if (s.timersOn && at.skillId > 0) timerLook(s, uint16_t(at.skillId), c);  // 3.0.16-beta.22
    } else if (at.what.kind == hb::Slot::Kind::Item) {
        const auto& items = clienttables::itemNames();
        const auto n = items.find(at.what.id);
        c.name = n != items.end() ? utf8Of(widened(hb::spaced(n->second), codePage)) : "#" + at.what.id;
        const auto b = s.bag.find(at.what.id);
        c.count = b != s.bag.end() ? b->second : 0;
        c.low = s.potionWarn && s.healing.count(at.what.id) && potionwarn::low(c.count);  // 3.0.11-beta.281
        const auto& files = clienttables::itemFiles();
        const auto f = files.find(at.what.id);
        if (f != files.end()) c.icon = iconOf(s, f->second, side);
    }
    c.notHere = hb::notHere(at.what, s.flags);
    return c;
}

// 3.0.11-beta.125 (the owner: «когда играю арчером, есть стрелы, и не видно сколько осталось ... чтобы словить момент
// когда они заканчиваются»): the ammo worn on its pill - how many, its picture, the warning's words as it falls
hb::Ammo ammoOf(State& s, ULONGLONG now, bool on) {
    hb::Ammo a;
    if (on != s.ammoOn) {
        s.ammoOn = on;
        LOG_INFO("hotbar: [interface] hotbar_ammo = %d", on ? 1 : 0);
    }
    if (!on) return a;
    const ammowatch::Ammo& w = ammowatch::read(now);
    const int warn = s.ammoWarn.now(w.count, now);
    if (warn != s.ammoTold) {
        s.ammoTold = warn;
        if (warn)
            LOG_INFO("hotbar: the ammo's pill warns - %s (%d left)",
                     warn == 3 ? "none left" : warn == 2 ? "10 or fewer" : "under 50", w.count);
    }
    if (w.count < 0) return a;
    a.count = w.count;
    const auto& files = clienttables::itemFiles();
    const auto f = w.id ? files.find(std::to_string(w.id)) : files.end();
    if (f != files.end()) a.icon = iconOf(s, f->second, hb::ammoIconSide(s.scale));
    if (warn)
        a.warning = settings::fmt(say(warn == 3 ? settings::Text::HotbarAmmoOut : settings::Text::HotbarAmmoLow),
                                  {say(w.arrows ? settings::Text::CharArrows : settings::Text::CharAmmo)});
    return a;
}

// 3.0.16-beta.8 (the owner: «для магов и пристов показывать количество гемов, а для монков например еще и сферы»):
// what the bar's learned skills spend (core/supplies) - each item's picture and its count in the bag, the spirit
// spheres as dots; a fall to 10 or fewer and to none the arrows' chimes (as the healing items')
std::vector<hb::Supply> suppliesOf(State& s) {
    std::vector<hb::Supply> out;
    if (!s.suppliesOn) return out;
    std::vector<uint16_t> skills;
    for (const Slot& at : s.slots)
        if (at.what.kind == hb::Slot::Kind::Skill && at.skillId > 0 &&
            !(s.learnedCame && !s.learned.count(at.what.id)))
            skills.push_back(uint16_t(at.skillId));
    const int spheres = spherewatch::count();
    const supplies::Pick p = supplies::pick(skills, spheres);
    std::map<int, int> counts;
    const auto& files = clienttables::itemFiles();
    std::string told;
    for (int id : p.items) {
        hb::Supply x;
        const auto b = s.bag.find(std::to_string(id));
        x.count = b != s.bag.end() ? b->second : 0;
        counts[id] = x.count;
        const auto f = files.find(std::to_string(id));
        if (f != files.end()) x.icon = iconOf(s, f->second, hb::ammoIconSide(s.scale));
        out.push_back(x);
        told += "#" + std::to_string(id) + " ";
    }
    if (p.spheres) {
        hb::Supply x;
        x.dots = p.sphereMost;
        x.count = std::min(spheres, p.sphereMost);
        out.push_back(x);
        told += "spheres/" + std::to_string(p.sphereMost);
    }
    if (told != s.suppliesTold) {
        s.suppliesTold = told;
        LOG_INFO("hotbar: the supplies' pills - %s", told.empty() ? "none" : told.c_str());
    }
    const int fresh = s.supplyWatch.step(counts, !s.bag.empty());
    if (fresh >= 2) {
        static const std::vector<uint8_t> low = chime::wav(chime::Kind::AmmoLow),
                                          none = chime::wav(chime::Kind::AmmoOut);
        const bool played = win::playOwn(fresh == 3 ? none.data() : low.data());
        if (s.supplyChimes++ < 20)
            LOG_INFO("hotbar: a supply %s - the chime %s", fresh == 3 ? "ran out" : "fell to 10 or fewer",
                     played ? "played" : "not played: the game's sound is off");
    }
    return out;
}

// The game's shown windows over the box (the target's pixels), and the new chat (3.0.11-beta.41): the right edge and
// the top of those; false none (3.0.11-beta.117: `what` - which, for the log)
bool windowsOver(const State& s, float x0, float y0, float x1, float y1, float& right, float& top, std::string& what) {
    const uint32_t head = read32(kWindowMgr + kWindows), bar = gameBar();
    if (!head || s.k <= 0) return false;
    bool any = false;
    int cx = 0, cy = 0, cw = 0, ch = 0;
    if (chatwin::panelBox(cx, cy, cw, ch) && float(cx) < x1 && float(cx + cw) > x0 && float(cy) < y1 &&
        float(cy + ch) > y0) {
        right = float(cx + cw);
        top = float(cy);
        any = true;
        what = "the new chat";
    }
    uint32_t node = read32(head);
    for (size_t n = 0; node && node != head && n < kMostNodes; ++n, node = read32(node)) {
        const uint32_t window = read32(node + 8);
        int32_t r[4] = {0, 0, 0, 0};
        if (!window || window == bar || !read32(window + kShown) || !readGame(window + kRect, r, sizeof r)) continue;
        const float wx0 = float(r[0]) * s.k, wy0 = float(r[1]) * s.k;
        const float wx1 = float(r[0] + r[2]) * s.k, wy1 = float(r[1] + r[3]) * s.k;
        if (r[2] <= 0 || r[3] <= 0 || wx0 >= x1 || wx1 <= x0 || wy0 >= y1 || wy1 <= y0) continue;
        if (wx1 - wx0 > 0.9f * float(s.targetW) || wy1 - wy0 > 0.6f * float(s.targetH)) continue;  // a screen's own
        right = any ? std::max(right, wx1) : wx1;
        top = any ? std::min(top, wy0) : wy0;
        any = true;
        char text[96];
        std::snprintf(text, sizeof text, "the game's window (its class 0x%X at %d, %d %dx%d)", read32(window), r[0],
                      r[1], r[2], r[3]);
        what = text;
    }
    return any;
}

// Its place: the bottom middle - moved right past the game's windows there (its chat), or up over them while it stays
// in the screen's lower part (else over them)
void place(State& s) {
    const float s8 = 8 * s.scale;
    hb::Layout L = hb::layout(s.mode, s.targetW / 2, s.targetH - int(s8), s.scale);
    const float x0 = float(L.x) + L.glass.x, y0 = float(L.y) + L.glass.y;
    float right = 0, top = 0;
    std::string what;
    const bool over = windowsOver(s, x0, y0, x0 + L.glass.w, y0 + L.glass.h, right, top, what);
    if (s.settle.settled(over, GetTickCount64(), kAsideMs)) {
        if (what != s.overWhat && s.asideTold < 10) {
            ++s.asideTold;
            LOG_INFO("hotbar: moved aside - %s stands over its place", what.c_str());
        }
        s.overWhat = what;
        const float shift = right + s8 - x0;
        if (x0 + shift + L.glass.w <= float(s.targetW) - s8)
            L = hb::layout(s.mode, s.targetW / 2 + int(std::lround(shift)), s.targetH - int(s8), s.scale);
        else if (top - s8 - L.glass.h >= 0.55f * float(s.targetH))
            L = hb::layout(s.mode, s.targetW / 2, int(std::lround(top - s8)), s.scale);
    }
    hudedit::place(hudlayout::Part::Hotbar, s.k, s.targetW, s.targetH, L.w, L.h, L.x, L.y);  // 3.0.11-beta.75
    if (float(L.y) + float(L.h) / 2 < float(s.targetH) / 2) {  // 3.0.12-beta.2: in the upper half - the room under it
        hb::flip(L);
        if (!s.belowTold) LOG_INFO("hotbar: in the screen's upper half - its tips and the ammo's pill under it");
        s.belowTold = true;
    }
    s.layout = L;
}

hb::Texts texts() {
    hb::Texts t;
    t.battle = say(settings::Text::HotbarBattle);
    t.battleHint = say(settings::Text::HotbarBattleHint);
    t.battleOn = say(settings::Text::HotbarBattleOn);  // 3.0.11-beta.147
    t.battleOff = say(settings::Text::HotbarBattleOff);
    t.use = say(settings::Text::HotbarUse);
    t.wheel = say(settings::Text::HotbarWheelLevel);  // 3.0.11-beta.289
    t.unlearned = say(settings::Text::HotbarUnlearned);  // 3.0.11-beta.78
    t.noSp = say(settings::Text::HotbarNoSp);
    t.notHere = say(settings::Text::HotbarNotHere);
    t.reasons.assign(size_t(hb::NotHere::kCount), "");
    const std::pair<hb::NotHere, settings::Text> reasons[] = {
        {hb::NotHere::Teleport, settings::Text::HotbarNoTeleport},
        {hb::NotHere::Return, settings::Text::HotbarNoReturn},
        {hb::NotHere::Warp, settings::Text::HotbarNoWarp},
        {hb::NotHere::Branch, settings::Text::HotbarNoBranch},
        {hb::NotHere::IceWall, settings::Text::HotbarNoIceWall},
        {hb::NotHere::Call, settings::Text::HotbarNoCall},
        {hb::NotHere::Skills, settings::Text::HotbarNoSkills},
        {hb::NotHere::GuildWar, settings::Text::HotbarGuildWar},
        {hb::NotHere::OnlyGuildWar, settings::Text::HotbarOnlyGuildWar}};
    for (const auto& [why, words] : reasons) t.reasons[size_t(why)] = say(words);
    return t;
}

// ---- 3.0.18-beta.21 (the owner: «Переписывать 73 части в разметку RmlUi вручную»): the bar as an RmlUi document while
// [interface] uaui is on - every part core hotbar's view() places and says (the cells' states, plates, bars, the
// delay's sweep, the badges, the pills, the tips); drawn only (passive): its clicks, drags, wheel and keys stay this
// part's own; its picture not painted then ----

const char* kDoc = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="hb" data-model="hotbar" data-style-font-size="s + 'px'"
 data-style-width="w + 'px'" data-style-height="h + 'px'">
<div class="glass" data-style-left="glass.x + 'px'" data-style-top="glass.y + 'px'"
  data-style-width="glass.w + 'px'" data-style-height="glass.h + 'px'" data-style-border-radius="10 * s + 'px'"
  data-style-background-color="ground"></div>
<span class="chip" data-if="chip.on" data-style-left="chip.b.x + 'px'" data-style-top="chip.b.y + 'px'"
  data-style-width="chip.b.w + 'px'" data-style-height="chip.b.h + 'px'"
  data-style-line-height="chip.b.h + 'px'" data-style-border-radius="chip.b.h / 2 + 'px'">{{chip.text}}</span>
<span class="hint" data-if="hint.on" data-style-left="hint.b.x + 'px'" data-style-top="hint.b.y + 'px'"
  data-style-width="hint.b.w + 'px'" data-style-height="hint.b.h + 'px'"
  data-style-line-height="hint.b.h + 'px'">{{hint.text}}</span>
<div class="rowbox" data-if="rowbox.w > 0" data-style-left="rowbox.x + 'px'" data-style-top="rowbox.y + 'px'"
  data-style-width="rowbox.w + 'px'" data-style-height="rowbox.h + 'px'" data-style-border-radius="7 * s + 'px'"></div>
<span class="rownum" data-if="rownum.on" data-style-left="rownum.b.x + 'px'" data-style-top="rownum.b.y + 'px'"
  data-style-width="rownum.b.w + 'px'" data-style-height="rownum.b.h + 'px'">{{rownum.text}}</span>
<span class="f12" data-if="f12.on" data-style-left="f12.b.x + 'px'" data-style-top="f12.b.y + 'px'"
  data-style-width="f12.b.w + 'px'" data-style-height="f12.b.h + 'px'">{{f12.text}}</span>
<span class="label" data-for="l : labels" data-style-left="l.b.x + 'px'" data-style-top="l.b.y + 'px'"
  data-style-width="l.b.w + 'px'" data-style-height="l.b.h + 'px'"
  data-style-line-height="l.b.h + 'px'">{{l.text}}</span>
<div data-for="c : cells" data-attr-class="'a ' + c.cls"
  data-style-transform-origin="(c.b.x + c.b.w / 2) + 'px ' + (c.b.y + c.b.h / 2) + 'px'">
 <div data-attr-class="'cell ' + c.cls" data-style-left="c.b.x + 'px'" data-style-top="c.b.y + 'px'"
  data-style-width="c.b.w + 'px'" data-style-height="c.b.h + 'px'" data-style-border-radius="7 * s + 'px'"></div>
 <img class="a" data-if="c.icon != ''" data-attr-src="c.icon" data-style-left="c.ib.x + 'px'"
  data-style-top="c.ib.y + 'px'" data-style-width="c.ib.w + 'px'" data-style-height="c.ib.h + 'px'"
  data-style-opacity="c.op"/>
 <ua-ring class="a"
  data-if="c.delay > 0" sector="1" square="1" fill="#00000000" track="#0000008c" style="position: absolute;"
   data-style-left="c.b.x + 'px'" data-style-top="c.b.y + 'px'" data-style-width="c.b.w + 'px'"
  data-style-height="c.b.h + 'px'" data-attr-value="1 - c.delay"></ua-ring>
 <img class="a" data-if="c.blocked != ''" data-attr-src="c.blocked" data-style-left="c.bb.x + 'px'"
  data-style-top="c.bb.y + 'px'" data-style-width="c.bb.w + 'px'" data-style-height="c.bb.h + 'px'"/>
 <span class="plate " data-if="c.key.on" data-style-left="c.key.b.x + 'px'" data-style-top="c.key.b.y + 'px'"
  data-style-width="c.key.b.w + 'px'" data-style-height="c.key.b.h + 'px'"
  data-style-line-height="c.key.b.h + 'px'" data-style-color="c.key.colour"
  data-style-border-radius="3 * s + 'px'">{{c.key.text}}</span>
 <span class="plate " data-if="c.level.on" data-style-left="c.level.b.x + 'px'"
  data-style-top="c.level.b.y + 'px'" data-style-width="c.level.b.w + 'px'"
  data-style-height="c.level.b.h + 'px'" data-style-line-height="c.level.b.h + 'px'"
  data-style-color="c.level.colour" data-style-border-radius="3 * s + 'px'">{{c.level.text}}</span>
 <span class="plate " data-if="c.count.on" data-style-left="c.count.b.x + 'px'"
  data-style-top="c.count.b.y + 'px'" data-style-width="c.count.b.w + 'px'"
  data-style-height="c.count.b.h + 'px'" data-style-line-height="c.count.b.h + 'px'"
  data-style-color="c.count.colour" data-style-border-radius="3 * s + 'px'">{{c.count.text}}</span>
 <span class="plate " data-if="c.standing.on" data-style-left="c.standing.b.x + 'px'"
  data-style-top="c.standing.b.y + 'px'" data-style-width="c.standing.b.w + 'px'"
  data-style-height="c.standing.b.h + 'px'" data-style-line-height="c.standing.b.h + 'px'"
  data-style-color="c.standing.colour" data-style-border-radius="3 * s + 'px'">{{c.standing.text}}</span>
 <div class="flash" data-if="c.flash > 0" data-style-left="c.b.x + 'px'" data-style-top="c.b.y + 'px'"
  data-style-width="c.b.w + 'px'" data-style-height="c.b.h + 'px'" data-style-opacity="c.flash"
  data-style-border-radius="7 * s + 'px'"></div>
 <span class="plate sp" data-if="c.sp.on" data-style-left="c.sp.b.x + 'px'" data-style-top="c.sp.b.y + 'px'"
  data-style-width="c.sp.b.w + 'px'" data-style-height="c.sp.b.h + 'px'"
  data-style-line-height="c.sp.b.h + 'px'" data-style-color="c.sp.colour"
  data-style-border-radius="3 * s + 'px'">{{c.sp.text}}</span>
 <span class="plate lack" data-if="c.lack.on" data-style-left="c.lack.b.x + 'px'"
  data-style-top="c.lack.b.y + 'px'" data-style-width="c.lack.b.w + 'px'" data-style-height="c.lack.b.h + 'px'"
  data-style-line-height="c.lack.b.h + 'px'" data-style-color="c.lack.colour"
  data-style-border-radius="3 * s + 'px'">{{c.lack.text}}</span>
 <div class="bar combo" data-if="c.combo" data-style-left="c.b.x + 4 * s + 'px'"
  data-style-top="c.b.y + c.b.h - 6 * s + 'px'"
   data-style-width="c.comboW + 'px'" data-style-border-radius="1.5 * s + 'px'"></div>
 <div class="bar timer" data-if="c.timer.on" data-style-left="c.b.x + 4 * s + 'px'"
  data-style-top="c.b.y + 3 * s + 'px'"
   data-style-width="c.timerW + 'px'" data-style-border-radius="1.5 * s + 'px'"></div>
 <span class="plate timer" data-if="c.timer.on" data-style-left="c.timer.b.x + 'px'"
  data-style-top="c.timer.b.y + 'px'" data-style-width="c.timer.b.w + 'px'"
  data-style-height="c.timer.b.h + 'px'" data-style-line-height="c.timer.b.h + 'px'"
  data-style-border-radius="4 * s + 'px'">{{c.timer.text}}</span>
 <div class="shade" data-if="c.padless" data-style-left="c.b.x + 'px'" data-style-top="c.b.y + 'px'"
  data-style-width="c.b.w + 'px'" data-style-height="c.b.h + 'px'" data-style-border-radius="7 * s + 'px'"></div>
 <img class="a" data-if="c.pad != ''" data-attr-src="c.pad" data-style-left="c.pb.x + 'px'"
  data-style-top="c.pb.y + 'px'" data-style-width="c.pb.w + 'px'" data-style-height="c.pb.h + 'px'"/>
</div>
<div data-attr-class="toggle_cls" data-style-left="toggle.x + 'px'" data-style-top="toggle.y + 'px'"
  data-style-width="toggle.w + 'px'" data-style-height="toggle.h + 'px'"
  data-style-border-radius="7 * s + 'px'"><img class="a" data-attr-src="toggle_icon"
  data-style-left="toggle.w / 2 - 8 * s + 'px'" data-style-top="toggle.h / 2 - 8 * s + 'px'"
  data-style-width="16 * s + 'px'" data-style-height="16 * s + 'px'"/></div>
<span class="profile" data-if="profile.on" data-style-left="profile.b.x + 'px'"
  data-style-top="profile.b.y + 'px'" data-style-width="profile.b.w + 'px'"
  data-style-height="profile.b.h + 'px'">{{profile.text}}</span>
<div class="a" data-for="p : pills">
 <div class="pill" data-style-left="p.b.x + 'px'" data-style-top="p.b.y + 'px'" data-style-width="p.b.w + 'px'"
  data-style-height="p.b.h + 'px'" data-style-border-radius="p.b.h / 2 + 'px'" data-style-border-color="p.edge"
  data-style-border-width="p.ew + 'px'"></div>
 <img class="a" data-if="p.icon != ''" data-attr-src="p.icon" data-style-left="p.ib.x + 'px'"
  data-style-top="p.ib.y + 'px'" data-style-width="p.ib.w + 'px'" data-style-height="p.ib.h + 'px'"/>
 <span class="pilltext" data-if="p.text.on" data-style-left="p.text.b.x + 'px'"
  data-style-top="p.text.b.y + 'px'" data-style-width="p.text.b.w + 'px'" data-style-height="p.text.b.h + 'px'"
  data-style-line-height="p.text.b.h + 'px'" data-style-color="p.text.colour">{{p.text.text}}</span>
 <div class="dot" data-for="d : p.dots" data-class-on="d.on" data-style-left="d.b.x + 'px'"
  data-style-top="d.b.y + 'px'" data-style-width="d.b.w + 'px'" data-style-height="d.b.h + 'px'"
  data-style-border-radius="d.b.h / 2 + 'px'"></div>
</div>
<div class="tip" data-if="tip.w > 0" data-style-left="tip.x + 'px'" data-style-top="tip.y + 'px'"
  data-style-width="tip.w + 'px'" data-style-height="tip.h + 'px'" data-style-border-radius="8 * s + 'px'"></div>
<span class="tipline" data-for="l, i : tiplines" data-class-first="i == 0" data-style-left="l.b.x + 'px'"
  data-style-top="l.b.y + 'px'" data-style-width="l.b.w + 'px'" data-style-height="l.b.h + 'px'"
  data-style-color="l.colour">{{l.text}}</span>
<span class="ttip" data-if="ttip.on" data-style-left="ttip.b.x + 'px'" data-style-top="ttip.b.y + 'px'"
  data-style-width="ttip.b.w + 'px'" data-style-height="ttip.b.h + 'px'"
  data-style-line-height="ttip.b.h + 'px'" data-style-border-radius="8 * s + 'px'">{{ttip.text}}</span>
</body></rml>)RML";

struct BoxV {
    float x = 0, y = 0, w = 0, h = 0;
};
struct PlateV {
    BoxV b;
    Rml::String text, colour;
    bool on = false;
};
struct CellV {
    BoxV b, ib, bb, pb;
    Rml::String cls, icon, blocked, pad;
    float op = 1, delay = 0, flash = 0, comboW = 0, timerW = 0;
    bool combo = false, padless = false;
    PlateV key, level, count, standing, sp, lack, timer;
};
struct DotV {
    BoxV b;
    bool on = false;
};
struct PillV {
    BoxV b, ib;
    Rml::String edge, icon;
    float ew = 1;
    PlateV text;
    std::vector<DotV> dots;
};

struct Ui {
    uaui::Doc doc;
    float w = 0, h = 0, s = 1;
    BoxV glass, rowbox, toggle, tip;
    Rml::String ground = "#00000000", toggleCls, toggleIcon;  // 3.0.18-beta.65: never an empty style value
    PlateV chip, hint, rownum, f12, profile, ttip;
    std::vector<PlateV> labels, tiplines;
    std::vector<CellV> cells;
    std::vector<PillV> pills;
    std::string key;
    std::map<std::string, panel::Picture> made;  // icons drawn once: grey copies, the crossed circle, badges
    std::map<std::string, std::string> sources;
};

Ui& ui() {
    static Ui u;
    return u;
}

BoxV bv(const panel::Box& b) { return BoxV{b.x, b.y, b.w, b.h}; }

std::string hex(uint32_t rgb, int alpha = -1) {
    char b[16];
    if (alpha < 0) std::snprintf(b, sizeof b, "#%06x", unsigned(uaui::themed(rgb) & 0xFFFFFF));
    else std::snprintf(b, sizeof b, "#%06x%02x", unsigned(uaui::themed(rgb) & 0xFFFFFF), alpha & 0xFF);
    return b;
}

PlateV pv(const hb::Plate& p) { return PlateV{bv(p.box), p.text, hex(p.rgb), p.box.w > 0 && !p.text.empty()}; }

std::string source(Ui& u, const std::string& key, const panel::Picture* p) {
    if (!p || p->width <= 0) return {};
    return uaui::picture(p, uaui::stamp(p), u.sources[key]);
}

// A picture of the driver's made once by its key (and drawn by `paint` into a side x side picture)
template <typename F>
std::string madeOnce(Ui& u, const std::string& key, int w, int h, F paint) {
    auto it = u.made.find(key);
    if (it == u.made.end()) {
        panel::Picture p;
        p.width = std::max(1, w), p.height = std::max(1, h);
        p.px.assign(size_t(p.width) * size_t(p.height), 0u);
        paint(p);
        it = u.made.emplace(key, std::move(p)).first;
    }
    return source(u, "m:" + key, &it->second);
}

std::string greyOf(Ui& u, const panel::Picture* icon) {  // as the old picture's dimmed icons: by brightness
    if (!icon) return {};
    char key[40];
    std::snprintf(key, sizeof key, "grey:%llx:%d", (unsigned long long)uintptr_t(icon), uaui::stamp(icon));
    return madeOnce(u, key, icon->width, icon->height, [&](panel::Picture& p) {
        for (size_t i = 0; i < p.px.size() && i < icon->px.size(); ++i) {
            const uint32_t c = icon->px[i];
            const uint32_t v = (((c >> 16) & 0xFF) * 77 + ((c >> 8) & 0xFF) * 150 + (c & 0xFF) * 29) >> 8;
            p.px[i] = (c & 0xFF000000u) | v << 16 | v << 8 | v;
        }
    });
}

bool uiFrame(State& s, const hb::Show& show) {
    Ui& u = ui();
    if (!uaui::ready()) {
        u.doc.show(false);
        return false;
    }
    if (!u.doc.made()) {
        u.doc.passive = true;
        if (!u.doc.make("hotbar", kDoc, [&](Rml::DataModelConstructor& m) {
                if (auto b = m.RegisterStruct<BoxV>()) {
                    b.RegisterMember("x", &BoxV::x);
                    b.RegisterMember("y", &BoxV::y);
                    b.RegisterMember("w", &BoxV::w);
                    b.RegisterMember("h", &BoxV::h);
                }
                if (auto p = m.RegisterStruct<PlateV>()) {
                    p.RegisterMember("b", &PlateV::b);
                    p.RegisterMember("text", &PlateV::text);
                    p.RegisterMember("colour", &PlateV::colour);
                    p.RegisterMember("on", &PlateV::on);
                }
                if (auto c = m.RegisterStruct<CellV>()) {
                    c.RegisterMember("b", &CellV::b);
                    c.RegisterMember("ib", &CellV::ib);
                    c.RegisterMember("bb", &CellV::bb);
                    c.RegisterMember("pb", &CellV::pb);
                    c.RegisterMember("cls", &CellV::cls);
                    c.RegisterMember("icon", &CellV::icon);
                    c.RegisterMember("blocked", &CellV::blocked);
                    c.RegisterMember("pad", &CellV::pad);
                    c.RegisterMember("op", &CellV::op);
                    c.RegisterMember("delay", &CellV::delay);
                    c.RegisterMember("flash", &CellV::flash);
                    c.RegisterMember("comboW", &CellV::comboW);
                    c.RegisterMember("timerW", &CellV::timerW);
                    c.RegisterMember("combo", &CellV::combo);
                    c.RegisterMember("padless", &CellV::padless);
                    c.RegisterMember("key", &CellV::key);
                    c.RegisterMember("level", &CellV::level);
                    c.RegisterMember("count", &CellV::count);
                    c.RegisterMember("standing", &CellV::standing);
                    c.RegisterMember("sp", &CellV::sp);
                    c.RegisterMember("lack", &CellV::lack);
                    c.RegisterMember("timer", &CellV::timer);
                }
                if (auto d = m.RegisterStruct<DotV>()) {
                    d.RegisterMember("b", &DotV::b);
                    d.RegisterMember("on", &DotV::on);
                }
                m.RegisterArray<std::vector<DotV>>();
                if (auto p = m.RegisterStruct<PillV>()) {
                    p.RegisterMember("b", &PillV::b);
                    p.RegisterMember("ib", &PillV::ib);
                    p.RegisterMember("edge", &PillV::edge);
                    p.RegisterMember("ew", &PillV::ew);
                    p.RegisterMember("icon", &PillV::icon);
                    p.RegisterMember("text", &PillV::text);
                    p.RegisterMember("dots", &PillV::dots);
                }
                m.RegisterArray<std::vector<PlateV>>();
                m.RegisterArray<std::vector<CellV>>();
                m.RegisterArray<std::vector<PillV>>();
                m.Bind("w", &u.w);
                m.Bind("h", &u.h);
                m.Bind("s", &u.s);
                m.Bind("glass", &u.glass);
                m.Bind("ground", &u.ground);
                m.Bind("rowbox", &u.rowbox);
                m.Bind("toggle", &u.toggle);
                m.Bind("toggle_cls", &u.toggleCls);
                m.Bind("toggle_icon", &u.toggleIcon);
                m.Bind("tip", &u.tip);
                m.Bind("chip", &u.chip);
                m.Bind("hint", &u.hint);
                m.Bind("rownum", &u.rownum);
                m.Bind("f12", &u.f12);
                m.Bind("profile", &u.profile);
                m.Bind("ttip", &u.ttip);
                m.Bind("labels", &u.labels);
                m.Bind("tiplines", &u.tiplines);
                m.Bind("cells", &u.cells);
                m.Bind("pills", &u.pills);
            }))
            return false;
    }
    const std::string key = s.key;  // the picture's key: everything it shows
    if (key != u.key) {
        u.key = key;
        const hb::Layout& L = s.layout;
        const hb::Texts t = texts();
        const hb::View v = hb::view(L, s.cells, s.row, show, t);
        const float sc = L.s;
        u.w = float(L.w), u.h = float(L.h), u.s = sc;
        u.glass = bv(L.glass);
        u.ground = hex(0x10141C, int(std::lround(uistyle::panel() * 255)));
        u.rowbox = bv(L.row);
        u.chip = pv(v.chip), u.hint = pv(v.hint), u.rownum = pv(v.row), u.f12 = pv(v.f12);
        u.profile = pv(v.profile), u.ttip = pv(v.toggleTip);
        u.labels.clear();
        for (const hb::Plate& p : v.rowLabels) u.labels.push_back(pv(p));
        u.tip = bv(v.tip);
        u.tiplines.clear();
        for (const hb::Plate& p : v.tipLines) u.tiplines.push_back(pv(p));
        const bool battle = L.mode == hb::Mode::Battle;
        u.toggle = bv(L.toggle);
        u.toggleCls = std::string("toggle") + (battle ? " battle" : "") + (show.hoverToggle ? " hot" : "");
        const int icon16 = std::max(8, int(std::ceil(16 * sc)));
        const uint32_t ti = battle ? 0xFFFFFF : show.hoverToggle ? 0xE6CB8A : 0x9AA3B5;
        u.toggleIcon = madeOnce(u, "battle:" + std::to_string(icon16) + hex(ti), icon16, icon16, [&](panel::Picture& p) {
            uiicons::draw(p, uiicons::Icon::Battle, 0, 0, float(icon16), ti, 1.0f, 2.0f);
        });
        u.cells.clear();
        for (const hb::CellView& c : v.cells) {
            CellV x;
            x.b = bv(c.box);
            x.cls = std::string(c.hot ? " hot" : "") + (c.down ? " down" : "") + (c.lit ? " lit" : "") +
                    (c.chosen ? " chosen" : "") + (c.under ? " under" : "") + (c.combo ? " combo" : "") +
                    (c.ready ? " ready" : "");
            if (c.icon) {
                x.icon = c.grey ? greyOf(u, c.icon) : source(u, "i:" + std::to_string(uintptr_t(c.icon)), c.icon);
                // the icon centred in its cell at its own size, as the picture's put()
                x.ib = BoxV{std::round(c.box.x + (c.box.w - float(c.icon->width)) / 2),
                            std::round(c.box.y + (c.box.h - float(c.icon->height)) / 2), float(c.icon->width),
                            float(c.icon->height)};
            }
            x.op = c.iconOpacity;
            x.delay = c.delay;
            if (c.barred) {
                const float side = std::min(c.box.w, c.box.h) * 0.7f;
                const int ps = std::max(8, int(std::ceil(side)));
                x.blocked = madeOnce(u, "blocked:" + std::to_string(ps), ps, ps, [&](panel::Picture& p) {
                    uiicons::draw(p, uiicons::Icon::Blocked, 0, 0, float(ps), 0xECEFF5, 0.9f, 2.0f);
                });
                x.bb = BoxV{c.box.x + (c.box.w - side) / 2, c.box.y + (c.box.h - side) / 2, side, side};
            }
            x.key = pv(c.key), x.level = pv(c.level), x.count = pv(c.count), x.standing = pv(c.standing);
            x.sp = pv(c.sp), x.lack = pv(c.lack), x.timer = pv(c.timer);
            x.flash = std::min(1.0f, c.flash);
            x.combo = c.combo, x.comboW = c.comboW, x.timerW = c.timerW, x.padless = c.padless;
            if (c.pad >= 0) {  // its button's badge with its letter
                const int ps = std::max(8, int(std::ceil(c.padBox.w)));
                const padglyph::Family f = show.sony ? padglyph::Family::Sony : padglyph::Family::Xbox;
                x.pad = madeOnce(u, "pad:" + std::to_string(c.pad) + (show.sony ? "s" : "x") + std::to_string(ps), ps,
                                 ps, [&](panel::Picture& p) {
                                     padglyph::paint(p, padglyph::Button(c.pad), f, 0, 0, float(ps));
                                     const padglyph::Letter l = padglyph::letter(padglyph::Button(c.pad), f, 0, 0,
                                                                                 float(ps));
                                     if (!l.text.empty())
                                         drawWords(p, {Words{l.box, utf8Wide(l.text), l.rgb, l.px, true, DT_CENTER,
                                                             false}});
                                 });
                x.pb = bv(c.padBox);
            }
            u.cells.push_back(std::move(x));
        }
        u.pills.clear();
        for (const hb::PillView& p : v.pills) {
            PillV x;
            x.b = bv(p.box);
            x.edge = p.low ? hex(p.edge, 230) : hex(0xFFFFFF, 31);
            x.ew = (p.low ? 1.5f : 1.0f) * sc;
            if (p.icon) {
                x.icon = source(u, "i:" + std::to_string(uintptr_t(p.icon)), p.icon);
                x.ib = BoxV{std::round(p.iconBox.x + (p.iconBox.w - float(p.icon->width)) / 2),
                            std::round(p.iconBox.y + (p.iconBox.h - float(p.icon->height)) / 2),
                            float(p.icon->width), float(p.icon->height)};
            }
            x.text = pv(p.text);
            for (int d = 0; d < p.dots; ++d)
                x.dots.push_back(DotV{BoxV{p.box.x + 10 * sc + float(d) * (p.dot + p.dotGap),
                                           p.box.y + (p.box.h - p.dot) / 2, p.dot, p.dot},
                                      d < p.lit});
            u.pills.push_back(std::move(x));
        }
        u.doc.dirtyAll();
    }
    u.doc.place(s.layout.x, s.layout.y);
    u.doc.show(true);
    return true;
}

// The states' changes in the log: the game waiting a target (the first ones), the cells the map forbids
void tellStates(State& s, int waitingWas) {
    if (s.waiting != waitingWas && s.waiting > 0 && s.waitsTold < 10) {
        ++s.waitsTold;
        std::string cell = "none of its cells";
        for (const hb::Cell& c : s.cells)
            if (c.selected) cell = "its cell " + c.key + " outlined";
        LOG_INFO("hotbar: the game waits for the target of the skill %d - %s", s.waiting, cell.c_str());
    }
    std::string barred;
    for (const hb::Cell& c : s.cells)
        if (c.notHere != hb::NotHere::No) barred += " " + c.key + "=" + std::to_string(int(c.notHere));
    if (barred != s.barred && s.barredTold < 20) {
        ++s.barredTold;
        s.barred = barred;
        LOG_INFO("hotbar: on %s (flags %zu) «не тут»:%s", s.map.c_str(), s.flags.size(),
                 barred.empty() ? " none" : barred.c_str());
    }
}

void block(const State& s, HWND game) {  // the mouse's clicks over it are its own, not the world's
    RECT client{};
    if (s.targetW <= 0 || s.targetH <= 0 || !GetClientRect(game, &client)) return;
    const panel::Box& b = s.layout.glass;
    const float x0 = float(s.layout.x) + b.x, y0 = float(s.layout.y) + b.y;
    const RECT r{LONG(int64_t(x0) * client.right / s.targetW), LONG(int64_t(y0) * client.bottom / s.targetH),
                 LONG(int64_t(x0 + b.w) * client.right / s.targetW + 1),
                 LONG(int64_t(y0 + b.h) * client.bottom / s.targetH + 1)};
    uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Hotbar, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Hotbar, nullptr, nullptr, false);
    s.shown = false;
    s.lift = hb::Lift{};  // 3.0.11-beta.147: a press held as it went (a map's loading) forgotten
}

// A slot used as its key would: the bar's message 0x29 (its cell; the battle mode's row - the normal mode's the game
// takes as shown)
bool use(const State& s, int cell) {
    const uint32_t window = gameBar();
    if (!window || read32(read32(window) + kSendMsgSlot) != kSendMsg) return false;
    const int row = s.mode == hb::Mode::Battle ? hb::gameRow(cell / 9) : 0;
    reinterpret_cast<SendMsgFn>(uintptr_t(kSendMsg))(window, 0, kUseMsg, cell % 9, row, 0);
    return true;
}

// 3.0.11-beta.147: a cell's card as the game's own bar shows it on a right click (the battle mode's row the cell's for
// the call's while, as the drop's)
bool card(const State& s, int cell) {
    const uint32_t bar = gameBar();
    if (!bar || read32(read32(bar) + kInfoSlot) != kBarInfo) return false;
    uint8_t row = 0;
    readGame(kRow, &row, 1);
    const uint8_t want = s.mode == hb::Mode::Battle ? uint8_t(hb::gameRow(cell / 9)) : row;
    if (want != row) write8(kRow, want);
    reinterpret_cast<InfoFn>(uintptr_t(kBarInfo))(bar, kCellX0 + kCellW * (cell % 9) + kCellW / 2, kCellY);
    if (want != row) write8(kRow, row);
    return true;
}

// 3.0.11-beta.147 (the owner: «предусмотри возможность чтобы можно было батлмод кнопкой на этой панели включать»): its
// button turns the battle mode as the game's /battlemode does
void toggleBattle(State& s, unsigned codePage) {
    if (!s.battleChecked) {
        s.battleChecked = true;
        s.battleCan = sameCode(kBattleCode, kBattleCodeBytes, sizeof kBattleCodeBytes) &&
                      sameCode(kLineCode, kLineCodeBytes, sizeof kLineCodeBytes);
        if (!s.battleCan) LOG_INFO("hotbar: the game's /battlemode is not where this build has it - its button idle");
    }
    if (!s.battleCan) return;
    const uint32_t on = 1 - read32(kBattle);
    write32(kBattle, on);
    reinterpret_cast<PushFn>(uintptr_t(kPush))(kWindowMgr, 3, nullptr, 0, 0);
    const char* line = reinterpret_cast<MsgStringFn>(uintptr_t(kMsgString))(on ? kBattleOnLine : kBattleOffLine);
    reinterpret_cast<PushFn>(uintptr_t(kPush))(kWindowMgr, 1, line, kLineColour, 0);
    s.key.clear();
    LOG_INFO("hotbar: the battle mode turned %s by its button, as the game's /battlemode (its line «%s»)",
             on ? "on" : "off", line ? utf8Of(widened(line, codePage)).c_str() : "");
}

bool buttonDown() {  // the logical left button (the physical right one when Windows swaps them)
    return GetAsyncKeyState(GetSystemMetrics(SM_SWAPBUTTON) ? VK_RBUTTON : VK_LBUTTON) < 0;
}

const char* nameOf(const State& s, int cell) {
    return cell >= 0 && cell < int(s.cells.size()) && !s.cells[size_t(cell)].name.empty()
               ? s.cells[size_t(cell)].name.c_str()
               : "empty";
}
const char* keyOf(const State& s, int cell) {
    return cell >= 0 && cell < int(s.cells.size()) ? s.cells[size_t(cell)].key.c_str() : "?";
}

// 3.0.11-beta.147: the press on a cell's icon followed each frame - lifted once held and moved, where the pointer is;
// the mouse's button up with no message of it a while (another part took it, the focus gone) - forgotten, nothing done
void liftStep(State& s, ULONGLONG now) {
    if (s.lift.cell < 0) return;
    if (s.liftReal && !buttonDown()) {
        if (!s.liftUpSince) s.liftUpSince = now;
        if (now - s.liftUpSince >= kUnseenUpMs) {
            LOG_INFO("hotbar: the press on the cell %s let go unseen - nothing done", keyOf(s, s.lift.cell));
            s.lift = hb::Lift{};
            s.pressedUntil = 0;
        }
        return;
    }
    s.liftUpSince = 0;
    int cx = 0, cy = 0;
    if (!readGameCursor(cx, cy)) return;
    s.liftX = float(cx) * s.k - float(s.layout.x);
    s.liftY = float(cy) * s.k - float(s.layout.y);
    const bool was = s.lift.dragging;
    if (hb::moved(s.lift, s.liftX, s.liftY, now, s.scale) && !was) {
        s.pressedUntil = 0;
        if (s.dragsTold < 20)
            LOG_INFO("hotbar: the cell %s (%s) lifted - held %llu ms and moved", keyOf(s, s.lift.cell),
                     nameOf(s, s.lift.cell), static_cast<unsigned long long>(now - s.lift.at));
    }
    s.liftAway = s.lift.dragging && hb::away(s.layout, s.liftX, s.liftY);
}

// The lifted icon's picture at the cursor: painted anew when its icon, its size or «away» change
void liftPaint(State& s) {
    if (!s.lift.dragging || s.lift.cell < 0 || s.lift.cell >= int(s.cells.size())) return;
    const panel::Picture* icon = s.cells[size_t(s.lift.cell)].icon;
    const int side = hb::liftedSide(s.mode, s.scale);
    const std::string key = std::to_string(side) + (s.liftAway ? "a" : "") +
                            std::to_string(static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(icon)));
    if (key == s.liftKey) return;
    s.liftKey = key;
    if (!panel::blank(s.liftPicture, side, side)) return;
    hb::paintLifted(s.liftPicture, icon, s.liftAway, s.scale);
    s.liftUpload = true;
}

// The press let go at (x, y) of the picture: not lifted - the pressed cell used (a quick click whose pointer slipped
// too); lifted - over another cell the two slots swapped, well off the bar its slot emptied, else nothing
void finish(State& s, float x, float y) {
    const hb::Lift lift = s.lift;
    s.lift = hb::Lift{};
    s.liftUpSince = 0;
    s.key.clear();
    const int cell = lift.cell;
    if (hb::gameSlot(s.mode, s.row, cell) != s.liftSlot || cell >= int(s.cells.size())) {
        s.pressedUntil = 0;
        LOG_INFO("hotbar: the bar changed under the press (its mode or row) - nothing done");
        return;
    }
    if (!lift.dragging) {
        const bool sent = use(s, cell);
        s.pressed = cell;
        s.pressedUntil = GetTickCount64() + kPressMs;
        if (s.usesLogged++ < 10)
            LOG_INFO("hotbar: the cell %s (%s) %s", keyOf(s, cell), nameOf(s, cell),
                     sent ? "used through the game's bar (its message 0x29)"
                          : "not used - the game's bar is not where this build has it");
        return;
    }
    s.pressedUntil = 0;
    const hb::Release r = hb::letGo(s.layout, cell, x, y);
    if (r.what == hb::Let::Nothing) {
        if (s.dragsTold++ < 20) LOG_INFO("hotbar: the cell %s let go on the bar - nothing moved", keyOf(s, cell));
        return;
    }
    if (!s.barsCan) {
        LOG_INFO("hotbar: the game's bar setters are not this build's - the cell %s not moved", keyOf(s, cell));
        return;
    }
    hb::Bar bar = barNow();
    const int to = r.what == hb::Let::Swap ? hb::gameSlot(s.mode, s.row, r.cell) : -1;
    if (r.what == hb::Let::Swap) hb::swapSlots(bar, s.liftSlot, to);
    else hb::emptySlot(bar, s.liftSlot);
    putBar(bar);
    s.readAt = 0;  // its cells read again at once
    if (s.dragsTold++ < 20) {
        if (r.what == hb::Let::Swap)
            LOG_INFO("hotbar: the cell %s (%s) dragged to %s (%s) - the game's slots %d and %d swapped", keyOf(s, cell),
                     nameOf(s, cell), keyOf(s, r.cell), nameOf(s, r.cell), s.liftSlot, to);
        else
            LOG_INFO("hotbar: the cell %s (%s) dragged off the bar - the game's slot %d emptied", keyOf(s, cell),
                     nameOf(s, cell), s.liftSlot);
    }
}

}  // namespace

std::vector<barhint::Skill> barSkills() {
    std::vector<barhint::Skill> out;
    const auto& names = clienttables::skillNames();
    for (int j = 0; j < kSlots; ++j) {
        const hb::Slot slot = hb::slotOf(readText(kSlotNames + 16 * uint32_t(j)));
        if (slot.kind != hb::Slot::Kind::Skill || slot.id.empty()) continue;
        const auto n = names.find(slot.id);
        out.push_back(barhint::Skill{slot.id, n != names.end() ? hb::spaced(n->second) : slot.id});
    }
    return out;
}

// 3.0.11-beta.288 (the owner's feature 20): Ctrl+Shift+1..3 - the character's bar's profile: the bar now kept as the
// profile it was, the one asked put in (met the first time: the bar now copied to it)
bool profileKeyPress(UINT m, WPARAM wp, LPARAM lp) {
    State& s = state();
    (void)lp;
    if (m != WM_KEYDOWN || wp < '1' || wp > '3' || GetKeyState(VK_CONTROL) >= 0 || GetKeyState(VK_SHIFT) >= 0 ||
        GetKeyState(VK_MENU) < 0)
        return false;
    if (s.barStep != State::BarStep::Kept || s.barOf.empty() || !inGameWorld()) return false;
    const int want = int(wp - '0');
    if (want == s.profile) return true;
    const hb::Bar now = barNow();
    keepBar(s, profileKey(s.barOf, s.profile), now);
    const auto it = s.bars.find(profileKey(s.barOf, want));
    const bool known = it != s.bars.end();
    const hb::Bar next = known ? it->second : now;
    if (known) putBar(next);
    else keepBar(s, profileKey(s.barOf, want), next);
    s.barKept = next;
    s.barChangedAt = 0;
    LOG_INFO("hotbar: profile %d -> %d (%s, %d cells)", s.profile, want, known ? "put in" : "new - the bar copied",
             filled(next));
    s.profile = want;
    s.profileShownAt = GetTickCount64();
    return true;
}

int profileNow() { return state().profile; }

void setPotionWarn(bool on) {  // 3.0.11-beta.281
    State& s = state();
    if (on != s.potionWarn) LOG_INFO("hotbar: [interface] potion_warn = %d", on ? 1 : 0);
    s.potionWarn = on;
}

void setTimers(bool on) {  // 3.0.16-beta.22
    State& s = state();
    if (on != s.timersOn) LOG_INFO("hotbar: [interface] ground_timers = %d (the time on the slots)", on ? 1 : 0);
    s.timersOn = on;
}

void setNeeds(bool needs, bool comboWindow) {  // 3.0.16-beta.13
    State& s = state();
    if (needs != s.needsOn) LOG_INFO("hotbar: [interface] skill_needs = %d", needs ? 1 : 0);
    if (comboWindow != s.comboOn) LOG_INFO("hotbar: [interface] combo_window = %d", comboWindow ? 1 : 0);
    s.needsOn = needs;
    s.comboOn = comboWindow;
}

void setSupplies(bool on) {  // 3.0.16-beta.8
    State& s = state();
    if (on != s.suppliesOn) {
        LOG_INFO("hotbar: [interface] hotbar_supplies = %d", on ? 1 : 0);
        s.key.clear();
    }
    s.suppliesOn = on;
}

void setDelayRing(bool on) {  // 3.0.11-beta.276
    State& s = state();
    if (on != s.delayRing) LOG_INFO("hotbar: [interface] delay_ring = %d", on ? 1 : 0);
    s.delayRing = on;
}

void frame(HWND game, bool on, bool ownBars, bool padHints, bool ammo, bool counts, unsigned codePage, double k,
           int targetWidth, int targetHeight) {
    State& s = state();
    if (counts != s.countsOn) {  // 3.0.11-beta.143
        s.countsOn = counts;
        s.key.clear();
        LOG_INFO("hotbar: [interface] ground_counts = %d", counts ? 1 : 0);
    }
    if (on != s.on) {
        s.on = on;
        if (!on) s.lift = hb::Lift{};
        s.key.clear();
        LOG_INFO("hotbar: [interface] hotbar = %d", on ? 1 : 0);
    }
    if (!s.checked) {  // this build's session: the functions that use its slots' places, byte for byte
        s.checked = true;
        s.known = uapad::clientbar::supported();
        if (!s.known) LOG_INFO("hotbar: the game's slots are not where this build has them - no new bar");
    }
    const bool world = inGameWorld();
    if (world && s.known) holdGameBar(s, on);
    s.codePage = codePage;
    if (world && s.known) ownBar(s, ownBars, codePage);  // 3.0.11-beta.78: each character's own bar, on or not
    if (!on || !s.known || !world || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        hide(s);
        return;
    }
    const float scale = uistyle::partScale(k) * hudedit::sizeOf(hudlayout::Part::Hotbar);  // 3.0.11-beta.75: its size
    s.k = float(k);
    s.scale = scale;
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    const ULONGLONG now = GetTickCount64();
    if (now - s.readAt >= kReadMs) {
        s.readAt = now;
        readSlots(s);
    }
    const int waitingWas = s.waiting;
    s.waiting = waitingSkill();
    uint8_t row = 0;
    s.row = readGame(kRow, &row, 1) && row < 3 ? row : 0;
    s.mode = read32(kBattle) ? hb::Mode::Battle : hb::Mode::Normal;
    place(s);
    s.cells.clear();
    // 3.0.11-beta.86 (the concept «Контроллер в интерфейсе uaRO», stage 2): played with the controller - its buttons
    const uapad::runtime::Look pad = padHints ? uapad::runtime::look() : uapad::runtime::Look{};
    const auto padOf = [&](int slot) {  // the controller's button that uses this slot, -1 none
        for (size_t i = 0; i < pad.bindings.size(); ++i)
            if (pad.bindings[i].kind == uapad::Action::Kind::Slot && int(pad.bindings[i].slot) == slot) return int(i);
        return -1;
    };
    if (s.timersOn) s.rings = groundunitswin::rings();  // 3.0.16-beta.22: the own walls' and traps' time
    else s.rings.clear();
    if (s.mode == hb::Mode::Battle) {
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 9; ++c) {
                s.cells.push_back(cellOf(s, hb::gameRow(r) * 9 + c, hb::battleKey(r, c), codePage));
                s.cells.back().pad = padOf(hb::gameRow(r) * 9 + c);
            }
    } else {
        for (int c = 0; c < 9; ++c) {
            s.cells.push_back(cellOf(s, s.row * 9 + c, "F" + std::to_string(c + 1), codePage));
            s.cells.back().pad = padOf(s.row * 9 + c);
        }
    }
    s.hover = -1;
    s.hoverToggle = false;
    int cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) {
        const hb::Hit h = hb::hitTest(s.layout, float(cx) * float(k) - float(s.layout.x),
                                      float(cy) * float(k) - float(s.layout.y));
        if (h.part == hb::Part::Cell) s.hover = h.cell;
        s.hoverToggle = h.part == hb::Part::Toggle;
    }
    liftStep(s, now);  // 3.0.11-beta.147: a press on a cell's icon - lifted, let go unseen
    const bool dragging = s.lift.dragging;
    liftPaint(s);
    if (s.hover != s.lastHover) {
        s.lastHover = s.hover;
        s.hoverSince = now;
    }
    hb::Show show;
    show.hover = dragging ? -1 : s.hover;
    show.pressed = now < s.pressedUntil ? s.pressed : -1;
    show.tip = !dragging && s.hover >= 0 && now - s.hoverSince >= kTipMs;
    show.hoverToggle = s.hoverToggle && !dragging;
    show.lifted = dragging ? s.lift.cell : -1;  // 3.0.11-beta.147: its cell faded, the one under it outlined
    show.over = dragging ? s.hover : -1;
    show.pad = pad.shown;  // 3.0.11-beta.86
    show.dropping = windrag::now().kind == windrag::Kind::Item || windrag::now().kind == windrag::Kind::Skill;  // .110
    show.sony = pad.sony;
    show.ammo = ammoOf(s, now, ammo);  // 3.0.11-beta.125
    show.supplies = suppliesOf(s);     // 3.0.16-beta.8
    show.delay = s.delayRing ? skilldelaywin::left() : 0.0f;  // 3.0.11-beta.276: the delay after a skill
    show.profile = s.barStep == State::BarStep::Kept ? s.profile : 1;  // 3.0.11-beta.288
    if (show.pad != s.padTold) {
        s.padTold = show.pad;
        LOG_INFO("hotbar: %s", show.pad ? (show.sony ? "the controller's buttons (PlayStation)"
                                                     : "the controller's buttons (Xbox)")
                                        : "the keys' names");
    }
    tellStates(s, waitingWas);
    std::string key = std::to_string(int(s.mode)) + ';' + std::to_string(s.row) + ';' + std::to_string(scale) + ';' +
                      std::to_string(s.layout.w) + 'x' + std::to_string(s.layout.h) +
                      (s.layout.below ? "v" : "") + ';' + std::to_string(show.hover) +
                      ',' + std::to_string(show.pressed) + (show.tip ? "t" : "") + (show.hoverToggle ? "e" : "") +
                      ',' + std::to_string(show.lifted) + ',' + std::to_string(show.over) +
                      (show.pad ? (show.sony ? "S" : "X") : "") + (show.dropping ? "D" : "") + ';' +
                      std::to_string(show.ammo.count) + (show.ammo.icon ? "i" : "") + show.ammo.warning + ';' +
                      std::to_string(int(show.delay * 40)) + 'p' + std::to_string(show.profile) + ';';  // .276, .288
    for (const hb::Supply& x : show.supplies)  // 3.0.16-beta.8
        key += std::to_string(x.count) + '/' + std::to_string(x.dots) + (x.icon ? "i" : "") + ',';
    for (size_t i = 0; i < s.cells.size(); ++i) {
        const hb::Cell& c = s.cells[i];
        key += c.key + '|' + c.name + '|' + std::to_string(c.level) + '|' + std::to_string(c.count) +
               (c.icon ? "i" : "") + (c.low ? "l" : "") + (c.selected ? "s" : "") + (c.spHave < c.spNeed ? "d" : "") +
               std::to_string(int(c.notHere)) + ',' + std::to_string(c.pad) + ',' + c.standing +
               std::to_string(int(c.flash * 10)) + c.lack + ',' + std::to_string(int(c.combo * 24)) +
               (c.ready ? "R" : "") + c.timerText + ',' + std::to_string(int(c.timer * 40)) +
               ';';  // 3.0.11-beta.143: the flash in ten steps; .13 lacks, combo, ready; .22 the time left
        if (int(i) == show.hover && show.tip)  // its tip's numbers
            key += std::to_string(c.spNeed) + '/' + std::to_string(c.spHave) + ';';
    }
    if (const std::string drawn = key + '#' + std::to_string(uistyle::generation()); drawn != s.key) {  // its grounds
        s.key = drawn;
    }
    s.uiShown = uiFrame(s, show);
    s.shown = true;
    block(s, game);
}

void key(UINT m, WPARAM wp, LPARAM lp) {
    State& s = state();
    if (!s.shown || m != WM_KEYDOWN || (lp & (LPARAM(1) << 30))) return;  // a key held: its first press only
    int cell = -1;
    if (s.mode == hb::Mode::Normal && wp >= VK_F1 && wp <= VK_F9) {
        cell = int(wp - VK_F1);
    } else if (s.mode == hb::Mode::Battle && !read32(kChatTyping)) {
        // FUN_004d8f10's scan codes: Q..O the shown row 1, A..L the second, Z..M , . the third
        const unsigned scan = unsigned(lp >> 16) & 0xFF;
        if (scan >= 0x10 && scan <= 0x18) cell = int(scan - 0x10);
        else if (scan >= 0x1E && scan <= 0x26) cell = 9 + int(scan - 0x1E);
        else if (scan >= 0x2C && scan <= 0x34) cell = 18 + int(scan - 0x2C);
    }
    if (cell < 0 || cell >= int(s.cells.size())) return;
    s.pressed = cell;
    s.pressedUntil = GetTickCount64() + kPressMs;
}

void padUsed(int slot) {
    State& s = state();
    if (!s.shown || slot < 0) return;
    for (int cell = 0; cell < int(s.cells.size()); ++cell) {
        if (hb::gameSlot(s.mode, s.row, cell) != slot) continue;
        s.pressed = cell;
        s.pressedUntil = GetTickCount64() + kPressMs;
        return;
    }
}

bool drop(int x, int y, uint32_t drag) {
    State& s = state();
    if (!s.shown || s.k <= 0) return false;
    const hb::Hit h = hb::hitTest(s.layout, float(x) * s.k - float(s.layout.x), float(y) * s.k - float(s.layout.y));
    if (h.part != hb::Part::Cell || h.cell < 0 || h.cell >= int(s.cells.size())) return false;
    const uint32_t bar = gameBar();
    if (!bar || read32(read32(bar) + kDropSlot) != kBarDrop) {
        LOG_INFO("hotbar: a drop on the cell %s not taken - the game's bar is not where this build has it",
                 s.cells[size_t(h.cell)].key.c_str());
        return false;
    }
    uint8_t row = 0;  // the battle mode: the cell's row for the drop's while, the one shown back after
    readGame(kRow, &row, 1);
    const uint8_t want = s.mode == hb::Mode::Battle ? uint8_t(hb::gameRow(h.cell / 9)) : row;
    SIZE_T n = 0;
    if (want != row) WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(uintptr_t(kRow)), &want, 1, &n);
    reinterpret_cast<DropFn>(uintptr_t(kBarDrop))(bar, kCellX0 + kCellW * (h.cell % 9) + kCellW / 2, kCellY, drag);
    if (want != row) WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(uintptr_t(kRow)), &row, 1, &n);
    s.readAt = 0;  // its slots read again at once
    LOG_INFO("hotbar: a drop on the cell %s (the game's row %d) given to the game's bar as on its own cell",
             s.cells[size_t(h.cell)].key.c_str(), int(want) + 1);
    return true;
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.shown) ui().doc.show(false);  // 3.0.18-beta.21
    const bool byUaui = s.uiShown && s.shown;  // uaUI draws the bar; the lifted icon stays this part's
    if (!s.shown || s.k <= 0 || (!byUaui && (s.picture.width <= 0 || s.picture.height <= 0))) return;
    int cursor = -1, cx = 0, cy = 0;  // under the game's own cursor, as the minimap
    if (readGameCursor(cx, cy)) cursor = panel::cursorDraw(record, float(cx), float(cy));
    if (!byUaui)
        drawPart(record, renderer, s.picture, s.texture, s.texW, s.texH, s.upload, s.layout.x, s.layout.y, s.k);
    if (s.lift.dragging && s.liftPicture.width > 0) {  // 3.0.11-beta.147: the lifted icon, its middle at the pointer
        const int x = s.layout.x + int(std::lround(s.liftX)) - s.liftPicture.width / 2;
        const int y = s.layout.y + int(std::lround(s.liftY)) - s.liftPicture.height / 2;
        drawPart(record, renderer, s.liftPicture, s.liftTexture, s.liftTexW, s.liftTexH, s.liftUpload, x, y, s.k);
    }
    if (cursor >= 0) record.moveLastBefore(record.commands().size() - size_t(cursor) - 1, size_t(cursor));
}

// 3.0.11-beta.289 (the owner's feature 21: «Teleport 1-го рівня з панелі»): the wheel over a skill's cell - the level
// it is used at, 1 .. the one learned (the game's own per cell, its S_SKILL_USE_LEVEL; Teleport at 1 warps at once
// on uaRO - skip_teleport_lv1_menu), set by the game's setter as its loader sets it
void wheelLevel(State& s, int cell, int step) {
    const int slot = hb::gameSlot(s.mode, s.row, cell);
    if (slot < 0 || slot >= kSlots) return;
    Slot& at = s.slots[slot];
    if (at.what.kind != hb::Slot::Kind::Skill || at.skillId <= 0) return;
    const auto learned = s.skills.find(at.skillId);
    const int most = learned != s.skills.end() ? learned->second.level : 0;
    if (most <= 1) return;
    const int now = at.level > 0 ? at.level : most, want = std::clamp(now + step, 1, most);
    if (want == now) return;
    static const int can = sameCode(kSetLevel, kSetLevelCode, sizeof kSetLevelCode) ? 1 : 0;
    if (!can) {
        if (s.levelTold++ < 3) LOG_INFO("hotbar: the game's level setter is not this build's - no level by the wheel");
        return;
    }
    uint8_t row = 0;
    readGame(kRow, &row, 1);
    write8(kRow, uint8_t(slot / 9));
    reinterpret_cast<SetLevelFn>(uintptr_t(kSetLevel))(kSession, want, slot);
    write8(kRow, row < 3 ? row : 0);
    if (const uint32_t window = gameBar())
        reinterpret_cast<SendMsgFn>(uintptr_t(kSendMsg))(window, 0, kRefreshMsg, 0, 0, 0);
    at.level = want;
    s.key.clear();
    if (s.levelTold++ < 20) LOG_INFO("hotbar: %s used at level %d now (the wheel)", nameOf(s, cell), want);
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    if (!s.shown) return false;
    const bool button = m == WM_LBUTTONDOWN || m == WM_LBUTTONUP || m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDOWN ||
                        m == WM_RBUTTONUP || m == WM_RBUTTONDBLCLK || m == WM_MBUTTONDOWN || m == WM_MBUTTONUP;
    if (!button && m != WM_MOUSEWHEEL) return false;
    float tx = 0, ty = 0;  // 3.0.11-beta.43: where the game's cursor stands, as the hover
    if (!pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty)) return false;
    const hb::Hit h = hb::hitTest(s.layout, tx - float(s.layout.x), ty - float(s.layout.y));
    if (h.part == hb::Part::None) return false;
    const bool filled = h.part == hb::Part::Cell && h.cell >= 0 && h.cell < int(s.cells.size()) &&
                        !s.cells[size_t(h.cell)].name.empty();
    if (m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK) {
        if (h.part == hb::Part::Toggle) {  // 3.0.11-beta.147: the battle mode's button
            toggleBattle(s, s.codePage);
        } else if (filled) {  // 3.0.11-beta.147: used when let go - or lifted, held and moved
            hb::press(s.lift, h.cell, tx - float(s.layout.x), ty - float(s.layout.y), GetTickCount64());
            s.liftSlot = hb::gameSlot(s.mode, s.row, h.cell);
            s.liftReal = buttonDown();
            s.liftUpSince = 0;
            s.pressed = h.cell;
            s.pressedUntil = ~0ULL;  // shown pressed while held
            s.key.clear();
        }
    } else if (m == WM_MOUSEWHEEL && filled) {  // 3.0.11-beta.289: a skill's level to use, 1 .. learned
        wheelLevel(s, h.cell, GET_WHEEL_DELTA_WPARAM(wp) > 0 ? 1 : -1);
    } else if (m == WM_RBUTTONDOWN && filled) {  // 3.0.11-beta.147: its card, the game's own
        const bool shownCard = card(s, h.cell);
        if (s.cardsTold++ < 10)
            LOG_INFO("hotbar: the cell %s (%s) right-clicked - %s", keyOf(s, h.cell), nameOf(s, h.cell),
                     shownCard ? "its card shown by the game's bar (its right click)"
                               : "no card - the game's bar is not where this build has it");
    }
    result = 0;
    return true;
}

bool ownsPress(HWND game, LPARAM lp) {
    const State& s = state();
    float tx = 0, ty = 0;
    if (!s.shown || !pointerInTarget(game, WM_LBUTTONDOWN, lp, s.k, s.targetW, s.targetH, tx, ty)) return false;
    const hb::Hit h = hb::hitTest(s.layout, tx - float(s.layout.x), ty - float(s.layout.y));
    return h.part == hb::Part::Cell && h.cell >= 0 && h.cell < int(s.cells.size()) &&
           !s.cells[size_t(h.cell)].name.empty();
}

bool held(HWND game, UINT m, WPARAM, LPARAM lp, LRESULT& result) {
    State& s = state();
    if (s.lift.cell < 0) return false;
    const bool button = m == WM_LBUTTONDOWN || m == WM_LBUTTONUP || m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDOWN ||
                        m == WM_RBUTTONUP || m == WM_RBUTTONDBLCLK || m == WM_MBUTTONDOWN || m == WM_MBUTTONUP;
    if (!button) return false;
    if (m == WM_LBUTTONUP) {
        float tx = 0, ty = 0;
        if (pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty))
            finish(s, tx - float(s.layout.x), ty - float(s.layout.y));
        else
            s.lift = hb::Lift{};
    } else if (m == WM_RBUTTONDOWN && s.lift.dragging) {  // a right click while it is lifted: put back
        LOG_INFO("hotbar: the cell %s put back (a right click while lifted)", keyOf(s, s.lift.cell));
        s.lift = hb::Lift{};
        s.key.clear();
    }
    result = 0;
    return true;
}

void beforeGameDraw() {
    State& s = state();
    if (s.on && s.known && inGameWorld()) holdGameBar(s, true);
}

bool assign(int slot, const std::string& value, int level) {
    State& s = state();
    if (slot < 0 || slot >= kSlots || !s.known || !inGameWorld()) return false;
    hb::Bar bar = barNow();
    bar.slots[size_t(slot)] = value;
    bar.levels[size_t(slot)] = level;
    putBar(bar);
    s.readAt = 0;  // its cells read again at once
    LOG_INFO("hotbar: the slot %d given %s (the controller: its X, then A on the cell)", slot,
             value.empty() ? "nothing" : value.c_str());
    return true;
}

std::string slotWords(int slot) {
    State& s = state();
    if (slot < 0 || slot >= kSlots) return "";
    return cellOf(s, slot, "", s.codePage).name;
}

std::vector<uagfx::padcarry::Place> cellPlaces() {  // 3.0.11-beta.121
    const State& s = state();
    std::vector<uagfx::padcarry::Place> out;
    if (!s.shown) return out;
    uint8_t row = 0;
    readGame(kRow, &row, 1);
    for (size_t i = 0; i < s.layout.cells.size(); ++i) {
        const panel::Box& b = s.layout.cells[i];
        const int slot = hb::gameSlot(s.mode, int(row), int(i));
        if (slot < 0 || slot >= kSlots) continue;
        uagfx::padcarry::Place p;
        p.to = uagfx::padcarry::To::Cell;
        p.index = slot;
        p.x = float(s.layout.x) + b.x;
        p.y = float(s.layout.y) + b.y;
        p.w = b.w;
        p.h = b.h;
        out.push_back(p);
    }
    return out;
}

}  // namespace uagfx::win::hotbarwin

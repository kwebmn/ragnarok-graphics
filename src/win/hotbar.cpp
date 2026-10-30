#include "win/hotbar.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "core/buff_panel.hpp"
#include "core/char_status.hpp"
#include "core/hotbar_bars.hpp"
#include "core/hotbar_panel.hpp"
#include "core/layout.hpp"
#include "core/settings/strings.hpp"
#include "core/skill_costs.hpp"
#include "core/ui_style.hpp"
#include "pad/win/client_bar.hpp"
#include "pad/win/game_window.hpp"
#include "win/chat_window.hpp"
#include "win/client_tables.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/hud_edit.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/reference_load.hpp"
#include "win/ui_cost.hpp"

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
    bool on = false, shown = false, known = false, checked = false, hid = false, oldTold = false;
    bool editing = false, editShown = false;
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
    bool hoverEdit = false;
    ULONGLONG pressedUntil = 0;
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
    enum class BarStep : uint8_t { None, Put, Kept } barStep = BarStep::None;
    ULONGLONG barSince = 0, barReadAt = 0, barChangedAt = 0;
    hb::Bar barKept;                // as last kept for barOf
    std::set<std::string> learned;  // the character's learned skills' AEGIS names (the session's list)
    bool learnedCame = false;       // that list there at all
};

State& state() {
    static State s;
    return s;
}

std::string say(settings::Text t) { return settings::tr(startup().lang, t); }

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

// Its shown flag held: 0 while this bar shows (each frame - the game shows it again on a new map and on F12), 1 while
// the player edits the game's own; off - back as the player had it
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
    if (on && s.editing) {
        if (s.editShown && !shown) {  // the player closed it with its own button: done
            s.editing = s.editShown = false;
            s.key.clear();
            LOG_INFO("hotbar: the game's own bar closed - editing done");
        } else if (!shown && write32(window + kShown, 1)) {
            s.editShown = true;
            LOG_INFO("hotbar: the game's own bar shown to edit it");
        } else {
            s.editShown = true;
        }
        s.hid = true;
    } else if (on) {
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
        keepBar(s, name, bar);
        LOG_INFO("hotbar: %s's own bar kept - %d cells", utf8Of(widened(name, codePage)).c_str(), filled(bar));
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
        int price = at.skillId > 0 ? skillcost::sp(at.skillId, level) : -1;
        if (learned != s.skills.end() && learned->second.level == level && learned->second.sp > 0)
            price = learned->second.sp;  // the server's own word for the level learned
        if (s.sp >= 0 && price > 0) {
            c.spNeed = price;
            c.spHave = s.sp;
        }
    } else if (at.what.kind == hb::Slot::Kind::Item) {
        const auto& items = clienttables::itemNames();
        const auto n = items.find(at.what.id);
        c.name = n != items.end() ? utf8Of(widened(hb::spaced(n->second), codePage)) : "#" + at.what.id;
        const auto b = s.bag.find(at.what.id);
        c.count = b != s.bag.end() ? b->second : 0;
        const auto& files = clienttables::itemFiles();
        const auto f = files.find(at.what.id);
        if (f != files.end()) c.icon = iconOf(s, f->second, side);
    }
    c.notHere = hb::notHere(at.what, s.flags);
    return c;
}

// The game's shown windows over the box (the target's pixels), and the new chat (3.0.11-beta.41): the right edge and
// the top of those; false none
bool windowsOver(const State& s, float x0, float y0, float x1, float y1, float& right, float& top) {
    const uint32_t head = read32(kWindowMgr + kWindows), bar = gameBar();
    if (!head || s.k <= 0) return false;
    bool any = false;
    int cx = 0, cy = 0, cw = 0, ch = 0;
    if (chatwin::panelBox(cx, cy, cw, ch) && float(cx) < x1 && float(cx + cw) > x0 && float(cy) < y1 &&
        float(cy + ch) > y0) {
        right = float(cx + cw);
        top = float(cy);
        any = true;
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
    }
    return any;
}

// Its place: the bottom middle - moved right past the game's windows there (its chat), or up over them while it stays
// in the screen's lower part (else over them); editing, the done button beside the game's own bar
void place(State& s) {
    const float s8 = 8 * s.scale;
    if (s.mode == hb::Mode::Editing) {
        int32_t r[4] = {0, 0, 0, 0};
        const uint32_t bar = gameBar();
        hb::Layout L = hb::layout(hb::Mode::Editing, s.targetW / 2, s.targetH - int(s8), s.scale);
        if (bar && readGame(bar + kRect, r, sizeof r) && r[2] > 0 && r[3] > 0) {
            const float right = float(r[0] + r[2]) * s.k, bottom = float(r[1] + r[3]) * s.k;
            const int centre = int(std::lround(right + s8 + L.glass.w / 2));
            L = hb::layout(hb::Mode::Editing, std::min(centre, s.targetW - int(L.glass.w / 2 + s8)),
                           int(std::lround(bottom)), s.scale);
        }
        s.layout = L;
        return;
    }
    hb::Layout L = hb::layout(s.mode, s.targetW / 2, s.targetH - int(s8), s.scale);
    const float x0 = float(L.x) + L.glass.x, y0 = float(L.y) + L.glass.y;
    float right = 0, top = 0;
    if (windowsOver(s, x0, y0, x0 + L.glass.w, y0 + L.glass.h, right, top)) {
        const float shift = right + s8 - x0;
        if (x0 + shift + L.glass.w <= float(s.targetW) - s8)
            L = hb::layout(s.mode, s.targetW / 2 + int(std::lround(shift)), s.targetH - int(s8), s.scale);
        else if (top - s8 - L.glass.h >= 0.55f * float(s.targetH))
            L = hb::layout(s.mode, s.targetW / 2, int(std::lround(top - s8)), s.scale);
    }
    hudedit::place(hudlayout::Part::Hotbar, s.k, s.targetW, s.targetH, L.w, L.h, L.x, L.y);  // 3.0.11-beta.75
    s.layout = L;
}

hb::Texts texts() {
    hb::Texts t;
    t.battle = say(settings::Text::HotbarBattle);
    t.battleHint = say(settings::Text::HotbarBattleHint);
    t.edit = say(settings::Text::HotbarEdit);
    t.done = say(settings::Text::HotbarDone);
    t.use = say(settings::Text::HotbarUse);
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

std::vector<Words> gdiWords(const std::vector<hb::Words>& in) {
    std::vector<Words> out;
    for (const hb::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == hb::Align::Right    ? DT_RIGHT
                                 : w.align == hb::Align::Center ? DT_CENTER
                                                                : DT_LEFT),
                            false});
    return out;
}

void render(State& s, const hb::Show& show) {
    const uicost::Paint cost("hotbar", s.layout.w, s.layout.h);  // 3.0.11-beta.55
    if (!panel::blank(s.picture, s.layout.w, s.layout.h)) return;  // 3.0.11-beta.57: in the memory it had
    const hb::Texts t = texts();
    hb::paint(s.picture, s.layout, s.cells, show, t);
    drawWords(s.picture, gdiWords(hb::words(s.layout, s.cells, s.row, show, t)));
    s.upload = true;
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

void frame(HWND game, bool on, bool ownBars, unsigned codePage, double k, int targetWidth, int targetHeight) {
    State& s = state();
    if (on != s.on) {
        s.on = on;
        if (!on) s.editing = s.editShown = false;
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
    s.mode = s.editing ? hb::Mode::Editing : read32(kBattle) ? hb::Mode::Battle : hb::Mode::Normal;
    place(s);
    s.cells.clear();
    if (s.mode == hb::Mode::Battle) {
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 9; ++c)
                s.cells.push_back(cellOf(s, hb::gameRow(r) * 9 + c, hb::battleKey(r, c), codePage));
    } else if (s.mode == hb::Mode::Normal) {
        for (int c = 0; c < 9; ++c) s.cells.push_back(cellOf(s, s.row * 9 + c, "F" + std::to_string(c + 1), codePage));
    }
    s.hover = -1;
    s.hoverEdit = false;
    int cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) {
        const hb::Hit h = hb::hitTest(s.layout, float(cx) * float(k) - float(s.layout.x),
                                      float(cy) * float(k) - float(s.layout.y));
        if (h.part == hb::Part::Cell) s.hover = h.cell;
        s.hoverEdit = h.part == hb::Part::Edit;
    }
    if (s.hover != s.lastHover) {
        s.lastHover = s.hover;
        s.hoverSince = now;
    }
    hb::Show show;
    show.hover = s.hover;
    show.pressed = now < s.pressedUntil ? s.pressed : -1;
    show.tip = s.hover >= 0 && now - s.hoverSince >= kTipMs;
    show.hoverEdit = s.hoverEdit;
    tellStates(s, waitingWas);
    std::string key = std::to_string(int(s.mode)) + ';' + std::to_string(s.row) + ';' + std::to_string(scale) + ';' +
                      std::to_string(s.layout.x) + ',' + std::to_string(s.layout.y) + ';' + std::to_string(show.hover) +
                      ',' + std::to_string(show.pressed) + (show.tip ? "t" : "") + (s.hoverEdit ? "e" : "") + ';';
    for (size_t i = 0; i < s.cells.size(); ++i) {
        const hb::Cell& c = s.cells[i];
        key += c.key + '|' + c.name + '|' + std::to_string(c.level) + '|' + std::to_string(c.count) +
               (c.icon ? "i" : "") + (c.selected ? "s" : "") + (c.spHave < c.spNeed ? "d" : "") +
               std::to_string(int(c.notHere)) + ';';
        if (int(i) == show.hover && show.tip)  // its tip's numbers
            key += std::to_string(c.spNeed) + '/' + std::to_string(c.spHave) + ';';
    }
    if (const std::string drawn = key + '#' + std::to_string(uistyle::generation()); drawn != s.key) {  // its grounds
        s.key = drawn;
        render(s, show);
    }
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

bool drop(int x, int y, uint32_t drag) {
    State& s = state();
    if (!s.shown || s.k <= 0 || s.mode == hb::Mode::Editing) return false;
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
    if (!s.shown || s.k <= 0 || s.picture.width <= 0 || s.picture.height <= 0) return;
    int cursor = -1, cx = 0, cy = 0;  // under the game's own cursor, as the minimap
    if (readGameCursor(cx, cy)) cursor = panel::cursorDraw(record, float(cx), float(cy));
    drawPart(record, renderer, s.picture, s.texture, s.texW, s.texH, s.upload, s.layout.x, s.layout.y, s.k);
    if (cursor >= 0) record.moveLastBefore(record.commands().size() - size_t(cursor) - 1, size_t(cursor));
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    if (!s.shown) return false;
    const bool button = m == WM_LBUTTONDOWN || m == WM_LBUTTONUP || m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDOWN ||
                        m == WM_RBUTTONUP || m == WM_RBUTTONDBLCLK || m == WM_MBUTTONDOWN || m == WM_MBUTTONUP;
    if (!button && m != WM_MOUSEWHEEL) return false;
    (void)wp;
    float tx = 0, ty = 0;  // 3.0.11-beta.43: where the game's cursor stands, as the hover
    if (!pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty)) return false;
    const hb::Hit h = hb::hitTest(s.layout, tx - float(s.layout.x), ty - float(s.layout.y));
    if (h.part == hb::Part::None) return false;
    if (m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK) {
        if (h.part == hb::Part::Edit) {
            s.editing = !s.editing;
            s.editShown = false;
            s.key.clear();
            LOG_INFO("hotbar: %s", s.editing ? "editing - the game's own bar shown, this one's done button beside it"
                                             : "editing done - the new bar back");
        } else if (h.part == hb::Part::Cell && h.cell >= 0 && h.cell < int(s.cells.size())) {
            const bool sent = use(s, h.cell);
            s.pressed = h.cell;
            s.pressedUntil = GetTickCount64() + kPressMs;
            if (s.usesLogged++ < 10)
                LOG_INFO("hotbar: the cell %s (%s) %s", s.cells[size_t(h.cell)].key.c_str(),
                         s.cells[size_t(h.cell)].name.empty() ? "empty" : s.cells[size_t(h.cell)].name.c_str(),
                         sent ? "used through the game's bar (its message 0x29)"
                              : "not used - the game's bar is not where this build has it");
        }
    }
    result = 0;
    return true;
}

void beforeGameDraw() {
    State& s = state();
    if (s.on && s.known && inGameWorld()) holdGameBar(s, true);
}

}  // namespace uagfx::win::hotbarwin

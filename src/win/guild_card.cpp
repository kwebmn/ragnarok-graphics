#include "win/card_drag.hpp"
#include "win/game_memory.hpp"
#include "win/guild_card.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "core/char_block.hpp"
#include "core/guild_card.hpp"
#include "core/guild_skills.hpp"
#include "core/guild_tabs.hpp"
#include "core/item_window.hpp"
#include "core/pad_carry.hpp"
#include "core/pad_glyphs.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "core/window_drag.hpp"
#include "pad/win/game_window.hpp"
#include "pad/win/runtime.hpp"
#include "win/carry_icon.hpp"
#include "win/chat_window.hpp"
#include "win/client_tables.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/pad_carry.hpp"
#include "win/paths.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/window_input.hpp"
#include "win/window_shadow.hpp"
#include "win/uaui.hpp"
#include "win/window_stack.hpp"
#include "uikit/guild_live.hpp"
#include "win/live_part.hpp"
#include "win/ui_sheet.hpp"
#include "win/words.hpp"

namespace uagfx::win::guildcardwin {
namespace {

namespace gc = uagfx::guildcard;
namespace gs = uagfx::guildskills;
using uapad::gamewindow::Blocker;

// uaRO.exe's: the guild's six windows (their classes' vtables and SendMsgs, in the manager from + 0x170), a window's
// shown flag; the guild object: its name's, master's and territory's words (a string's pointer and count), its
// numbers (the level, the members of the most, the average level, the experience and the next, the tax points), its
// members' list (a node: next +0, the member +8 - the account +0, the character +4, its name's words +0xC / +0x10,
// job +0x20, title +0x30 / +0x34, level +0x3C, online +0x58)
constexpr uint32_t kWindowMgr = 0x727ED8, kShown = 0x38, kSendMsgSlot = 0x94;
constexpr uint32_t kClasses[gc::kWindows] = {0x69A988, 0x69AAC8, 0x69AB64, 0x69ACA4, 0x69AD40, 0x69ADDC};
constexpr uint32_t kSends[gc::kWindows] = {0x49D980, 0x4A1A50, 0x4A3630, 0x4A5920, 0x4A6AF0, 0x4A7580};
constexpr uint32_t kName = 0x74B654, kMaster = 0x74B664, kTerritory = 0x74B6A8, kLevel = 0x74B690;
constexpr uint32_t kMost = 0x74B69C, kAverage = 0x74B6A0, kExp = 0x74B6B4, kNextExp = 0x74B6B8, kTax = 0x74B6C0;
constexpr uint32_t kMembers = 0x74B70C, kMember = 8;
constexpr uint32_t kRights = 0x74B6D0;  // 3.0.11-beta.210: the tabs' rights (FUN_0049c4d0 checks them)
// 3.0.19-beta.1: the guild's id and its emblem's (the packet 0x01B6's +2 and +0x2A, kept by FUN_00590c60)
constexpr uint32_t kGuildId = 0x74B6D4, kEmblemId = 0x74B6D8;
constexpr int kButtonMsg = 6;
constexpr size_t kMostMembers = 100;
// 3.0.11-beta.220: the game mode's SendMsg (FUN_005707d0, its vtable + 0x14) - 0x4A raises a skill (the packet 0x0112,
// the server's guild_skillup for the guild's 10000 on)
constexpr uint32_t kModeNow = 0x74B57C, kModeMsgSlot = 0x14, kModeMsg = 0x5707D0;
constexpr int kRaiseMsg = 0x4A;
using ModeMsgFn = int(__attribute__((thiscall)) *)(uint32_t mode, int msg, int a, int b, int c);
// 3.0.22-beta.26 (docs/superpowers/plans/2026-11-19-guild-window.md): the guild's skills as the game keeps them - a
// std::list at the session + 0x2E4 (FUN_00642940; its count + 0x2E8), a node's record at +8 (an 8-int record: the id
// +4, its kind +8, level +0xC, SP +0x10, «can rise» +0x14, range +0x18, its name +0x1C); the points left 0x771FA8 (the
// 0x0162 handler's); its window's «use» = the game mode's 0x7E (the record, the level - FUN_004a5920's 0x3A), a drag
// onto the hotbar 0x51 (id, name, level) let go by 0x12 at the cursor (as «Навички»); the player's own account
constexpr uint32_t kSession = 0x771148, kGuildSkills = kSession + 0x2E4, kGuildPoints = 0x771FA8;
constexpr uint32_t kOwnAccount = 0x771F7C;
constexpr int kUseMsg = 0x7E, kDragMsg = 0x51, kDropMsg = 0x12;
// 3.0.18-beta.141: the game's friend request by a name (FUN_004cec50: the name's 24 bytes - the packet 0x0202)
constexpr uint32_t kAddFriend = 0x4CEC50;
using AddFriendFn = void(__attribute__((stdcall)) *)(const char* name24);
// The member card's actions: «Написати», «Запросити в групу», «У друзі»; the master's «Посада й вигнання…» - the game's
// own members' window (3.0.22-beta.27)
constexpr uint32_t kActsNow = (1u << int(gc::Act::Whisper)) | (1u << int(gc::Act::Party)) |
                              (1u << int(gc::Act::Friend)) | (1u << int(gc::Act::Position));
// 3.0.22-beta.27: the game mode's 0x41 - the packet 0x00fc, a party invite to an account (FUN_005707d0's case 0x41:
// the id 0x00fc and its first argument's four bytes)
constexpr int kPartyInviteMsg = 0x41;

struct GuildSkill {  // as the game keeps it
    uint32_t record = 0, name = 0;
    int id = 0, inf = 0, level = 0, sp = 0;
    bool up = false;
};

struct State {
    CardDrag move;  // 3.0.18-beta.92: moved by its empty part
    LivePart<uikit::guild::Model> live;
    int sheetGen = 0;
    bool sheetShown = false;
    WindowShadow shadow;
    bool on = false, shown = false;
    uint32_t hidden = 0;
    unsigned codePage = 0;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    gc::Info info;
    std::vector<gc::Member> members;
    gc::Show show;
    gc::Layout layout;
    std::string key;
    int told = 0;
    guildtabs::Data tabs;   // 3.0.11-beta.220: what the server sent (the game's code page)
    guildtabs::Data words;  // 3.0.22-beta.26: the same in UTF-8
    uint32_t editing = 0;   // 3.0.11-beta.220: the game's own tab window «Змінити» showed (0 none)
    // 3.0.19-beta.1: the guild's emblem from the game's file at the head's size and «Інформація»'s
    panel::Picture emblemSmall, emblemBig;
    std::string emblemFor;
    ULONGLONG emblemAt = 0;
    std::vector<GuildSkill> skills;  // 3.0.22-beta.26
    int points = 0;
    std::vector<gs::Row> rows;
    ULONGLONG usedAt = 0;  // the four war skills' block began (the server's 0x011a for one of the player's)
    windrag::Press press;  // a skill pressed: a click, or a drag onto the hotbar
    int pressSkill = 0;
    panel::Picture dragIcon;
    const panel::Picture* dragOf = nullptr;
    uint32_t dragTexture = 0;
    int dragW = 0, dragH = 0;
    bool dragUpload = false;
    ULONGLONG meAt = 0;
    bool noGuild = false;  // 3.0.22-beta.27: Alt+G with no guild - how to join and how to make one
};

State& state() {
    static State s;
    return s;
}

// The guild's tab open now (its window), 0 none; `tab` its number
uint32_t gameGuild(int* tab = nullptr) {
    for (int i = 0; i < gc::kWindows; ++i) {
        const uint32_t w = read32(kWindowMgr + gc::kFirstAt + 4 * uint32_t(i));
        if (w && read32(w) == kClasses[i] && read32(read32(w) + kSendMsgSlot) == kSends[i]) {
            if (tab) *tab = i;
            return w;
        }
    }
    return 0;
}

// 3.0.11-beta.210: the game's own tab switched to `tab` (its message 0x45: the game checks the rights)
void switchTab(State& s, int tab) {
    int now = -1;
    const uint32_t window = gameGuild(&now);
    if (!window || tab == now || !gc::allowed(tab, read32(kRights))) return;
    if (s.told++ < 30) LOG_INFO("guild: the game's tab %d", tab);
    reinterpret_cast<SendMsgFn>(uintptr_t(read32(read32(window) + kSendMsgSlot)))(window, 0, gc::kTabMsg, tab, 0, 0);
    s.show.typing = false;
    s.key.clear();
}

void hold(State& s, uint32_t window, bool on) {
    if (on && window) {
        if (read32(window + kShown) && write32(window + kShown, 0) && s.hidden != window) {
            LOG_INFO("guild: the game's guild window hidden for the new card (its shown flag 0)");
            s.hidden = window;
        }
    } else if (window && s.hidden == window) {
        write32(window + kShown, 1);
        s.hidden = 0;
    } else if (!window) {
        s.hidden = 0;
    }
}

std::string text(const State& s, uint32_t ptr, uint32_t len) {
    if (!ptr || !len || len > 64) return "";
    char buf[65] = {};
    return readGame(ptr, buf, len) ? utf8Of(widened(std::string(buf, len), s.codePage)) : "";
}

std::string utf(const State& s, const std::string& game) { return utf8Of(widened(game, s.codePage)); }

void readGuild(State& s) {
    gc::Info& g = s.info;
    g.name = text(s, read32(kName), read32(kName + 4));
    g.master = text(s, read32(kMaster), read32(kMaster + 4));
    g.territory = text(s, read32(kTerritory), read32(kTerritory + 4));
    g.level = int32_t(read32(kLevel));
    g.most = int32_t(read32(kMost));
    g.average = int32_t(read32(kAverage));
    g.exp = int32_t(read32(kExp));
    g.nextExp = int32_t(read32(kNextExp));
    g.tax = int32_t(read32(kTax));
    s.members.clear();
    const uint32_t head = read32(kMembers);
    if (head) {
        uint32_t node = read32(head);
        for (size_t n = 0; node && node != head && n < kMostMembers; ++n, node = read32(node)) {
            uint8_t raw[0x5C] = {};
            if (!readGame(node + kMember, raw, sizeof raw)) break;
            uint32_t f[0x17];
            std::memcpy(f, raw, sizeof f);
            gc::Member m;
            m.account = f[0];
            m.character = f[1];
            m.name = text(s, f[3], f[4]);
            m.title = text(s, f[12], f[13]);
            const char* job = charblock::jobName(int(f[8]));
            m.job = job && *job ? job : "#" + std::to_string(f[8]);
            m.level = int(f[15]);
            m.online = f[22] != 0;
            s.members.push_back(std::move(m));
        }
    }
    g.members = int(s.members.size());
}

// 3.0.22-beta.26: the guild's skills as the game keeps them (the server's 0x0162 made into its list)
void readSkills(State& s) {
    s.skills.clear();
    const uint32_t head = read32(kGuildSkills);
    if (head) {
        uint32_t node = read32(head);
        for (int n = 0; node && node != head && n < 40; ++n, node = read32(node)) {
            int32_t r[8] = {};
            if (!readGame(node + 8, r, sizeof r)) break;
            if (r[1] >= 10000 && r[1] < 10100)
                s.skills.push_back(GuildSkill{node + 8, uint32_t(r[7]), r[1], r[2], r[3], r[4], r[5] != 0});
        }
    }
    s.points = int32_t(read32(kGuildPoints));
    if (s.points < 0 || s.points > 1000) s.points = 0;
    std::vector<gs::Sent> sent;
    for (const GuildSkill& k : s.skills) sent.push_back(gs::Sent{k.id, k.level, k.inf, k.sp, k.up});
    if (sent.empty())  // the game's list not made yet: what the server's packet said
        for (const guildtabs::Skill& k : s.tabs.skills) sent.push_back(gs::Sent{k.id, k.lv, k.inf, k.sp, k.up});
    s.rows = gs::rows(sent, s.points, gc::isMaster(s.show.rights), s.show.coolMs);
}

// 3.0.19-beta.1 (the testers: «не видно значок ГІ твоеї»): the guild's emblem as the game keeps it -
// <client>\_tmpEmblem\<server>_<guild>_<emblem>.ebm (uaRO.exe FUN_0063aab0 writes it from the packet 0x0152; the
// server's name is the session's, so any) at the head's and «Інформація»'s sizes; looked for again every 3 s while
// not there
void readEmblem(State& s) {
    const uint32_t guild = read32(kGuildId), emblem = read32(kEmblemId);
    const int small = std::max(8, int(std::lround(28 * s.scale))), big = std::max(16, int(std::lround(48 * s.scale)));
    if (!guild) return;
    const std::string want = std::to_string(guild) + '_' + std::to_string(emblem) + '_' + std::to_string(small);
    const ULONGLONG now = GetTickCount64();
    if (want == s.emblemFor && (s.emblemSmall.width > 0 || now - s.emblemAt < 3000)) return;
    const bool again = want == s.emblemFor;
    s.emblemFor = want;
    s.emblemAt = now;
    const std::string dir = gameDir() + "\\_tmpEmblem\\";
    WIN32_FIND_DATAA fd{};
    const std::string pattern = dir + "*_" + std::to_string(guild) + '_' + std::to_string(emblem) + ".ebm";
    HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
    std::string file, bytes;
    FILETIME newest{};
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!file.empty() && CompareFileTime(&fd.ftLastWriteTime, &newest) <= 0) continue;
            file = fd.cFileName;
            newest = fd.ftLastWriteTime;
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    if (!file.empty()) {
        HANDLE f = CreateFileA((dir + file).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                               OPEN_EXISTING, 0, nullptr);
        if (f != INVALID_HANDLE_VALUE) {
            LARGE_INTEGER size{};
            if (GetFileSizeEx(f, &size) && size.QuadPart > 0 && size.QuadPart < (1 << 20)) {
                bytes.resize(size_t(size.QuadPart));
                DWORD got = 0;
                if (!ReadFile(f, bytes.data(), DWORD(bytes.size()), &got, nullptr)) got = 0;
                bytes.resize(got);
            }
            CloseHandle(f);
        }
    }
    const auto picture = [&](panel::Picture& p, int side) {
        std::vector<uint32_t> px = gc::emblemPixels(bytes, side);
        p = panel::Picture{};
        if (px.empty()) return;
        p.width = p.height = side;
        p.px = std::move(px);
    };
    picture(s.emblemSmall, small);
    picture(s.emblemBig, big);
    if (!again || s.emblemSmall.width > 0)
        LOG_INFO("guild: emblem %u of the guild %u - %s", emblem, guild,
                 s.emblemSmall.width > 0 ? ("_tmpEmblem\\" + file).c_str()
                 : file.empty()          ? "no file in _tmpEmblem yet (looked for again every 3 s)"
                                         : ("_tmpEmblem\\" + file + " not an emblem (" + std::to_string(bytes.size()) +
                                            " bytes)").c_str());
}

gc::Texts texts() {
    using T = settings::Text;
    gc::Texts t;
    t.title = say(T::GuildTitle);
    t.hotkey = "Alt+G";
    const T tabs[gc::kTabs] = {T::GuildInfo,   T::GuildMembers,     T::GuildTabPositions,
                               T::GuildTabSkills, T::GuildTabExpelled, T::GuildTabNotice};
    for (int i = 0; i < gc::kTabs; ++i) t.tabs[i] = say(tabs[i]);
    t.sub = say(T::GuildWSub);
    t.none = say(T::GuildNone);
    t.infoSub = say(T::GuildWInfoSub);
    t.expLine = say(T::GuildWExpLine);
    t.master = say(T::GuildMaster);
    t.average = say(T::GuildAverage);
    t.castles = say(T::GuildWCastles);
    t.yourTax = say(T::GuildWYourTax);
    t.yourTaxValue = say(T::GuildWYourTaxValue);
    t.allies = say(T::GuildWAllies);
    t.enemies = say(T::GuildWEnemies);
    t.notice = say(T::GuildTabNotice);
    t.gifts = say(T::GuildWGifts);
    t.nearMaster = say(T::GuildWNearMaster);
    t.warOnly = say(T::GuildWWarOnly);
    t.online = say(T::GuildWOnline);
    t.offline = say(T::GuildWOffline);
    t.allMembers = say(T::GuildWAllMembers);
    t.search = say(T::GuildWSearch);
    t.count = say(T::GuildWCount);
    t.colName = say(T::GuildWColName);
    t.colJob = say(T::GuildWColJob);
    t.colLevel = say(T::GuildWColLevel);
    t.colPosition = say(T::GuildWColPosition);
    t.colGiven = say(T::GuildWColGiven);
    t.inGame = say(T::GuildWInGame);
    t.away = say(T::GuildWAway);
    t.position = say(T::GuildWColPosition);
    t.given = say(T::GuildWGiven);
    t.givenValue = say(T::GuildWGivenValue);
    t.whisper = say(T::GuildWWhisper);
    t.party = say(T::GuildWParty);
    t.friendly = say(T::GuildWFriend);
    t.setPosition = say(T::GuildWSetPosition);
    t.expel = say(T::GuildWExpel);
    t.rightsNote = say(T::GuildWRightsNote);
    t.membersHints = say(T::GuildWMembersHints);
    t.colRights = say(T::GuildWColRights);
    t.colTax = say(T::GuildWColTax);
    t.colPeople = say(T::GuildWColPeople);
    t.invite = say(T::GuildRightInvite);
    t.expelRight = say(T::GuildRightExpel);
    t.noRights = say(T::GuildWNoRights);
    t.folded = say(T::GuildWFolded);
    t.edit = say(T::GuildEdit);
    t.editNote = say(T::GuildWEditNote);
    t.peopleOn = say(T::GuildWPeopleOn);
    const T groups[gs::kGroups] = {T::GuildWGroupCastle, T::GuildWGroupGrowth, T::GuildWGroupAuras, T::GuildWGroupWar};
    for (int i = 0; i < gs::kGroups; ++i) t.groups[i] = say(groups[i]);
    t.groupsMore[2] = say(T::GuildWGroupAurasMore);
    t.groupsMore[3] = say(T::GuildWGroupWarMore);
    t.kinds[0] = say(T::GuildWPassive);
    t.kinds[1] = say(T::GuildWAura);
    t.kinds[2] = say(T::GuildWActive);
    t.sp = say(T::GuildWSp);
    for (int i = 0; i < 14; ++i) {
        t.lines[i] = say(T(int(T::GuildWLine0) + i));
        t.about[i] = say(T(int(T::GuildWAbout0) + i));
    }
    t.levelsLabel = say(T::GuildWLevels);
    t.youLabel = say(T::GuildWYou);
    t.youNear = say(T::GuildWYouNear);
    t.whereLabel = say(T::GuildWWhere);
    t.coolLabel = say(T::GuildWCool);
    t.needsLabel = say(T::GuildWNeeds);
    t.whereAura = say(T::GuildWWhereAura);
    t.whereWar = say(T::GuildWWhereWar);
    t.coolShared = say(T::GuildWCoolShared);
    t.ready = say(T::GuildWReady);
    t.wait = say(T::GuildWWait);
    t.use = say(T::GuildWUse);
    t.raiseTo = say(T::GuildWRaiseTo);
    t.places = say(T::GuildWPlaces);
    t.need = say(T::GuildWNeed);
    t.points = say(T::GuildSkillPoints);
    t.memberFoot = say(T::GuildWMemberFoot);
    t.skillHints = say(T::GuildWSkillHints);
    t.whoRaises = say(T::GuildWWhoRaises);
    t.whoRaisesText = say(T::GuildWWhoRaisesText);
    t.coolRow = say(T::GuildWCoolRow);
    t.levelOf = say(T::GuildWLevelOf);
    t.expelledHead = say(T::GuildTabExpelled);
    t.account = say(T::GuildWAccount);
    t.reason = say(T::GuildWReason);
    t.fieldTitle = say(T::GuildNoticeSubject);  // 3.0.11-beta.271
    t.fieldBody = say(T::GuildNoticeBody);
    t.save = say(T::GuildNoticeSave);
    t.cancel = say(T::MsgCancel);
    t.editHint = say(T::GuildNoticeHint);
    t.counter = say(T::GuildWCount);
    t.preview = say(T::GuildWPreview);
    t.previewNote = say(T::GuildWPreviewNote);
    t.noticeEmpty = say(T::GuildWNoticeEmpty);
    t.padTabs = say(T::GuildWPadTabs);
    t.padPick = say(T::GuildWPadPick);
    t.padUse = say(T::GuildWUse);
    t.padRaise = say(T::GuildWPadRaise);
    t.padAssign = say(T::GuildWPadAssign);
    t.padClose = say(T::PlateClose);
    t.padActions = say(T::GuildWWhisper);  // A on a member: «Написати»
    t.noGuild = say(T::GuildWNoGuild);       // 3.0.22-beta.27
    t.noGuildSub = say(T::GuildWNoGuildSub);
    t.join = say(T::GuildWJoin);
    t.joinText = say(T::GuildWJoinText);
    t.make = say(T::GuildWMake);
    t.makeText = say(T::GuildWMakeText);
    return t;
}

// The server's packets in UTF-8 for the views
void wordsOf(State& s) {
    guildtabs::Data w = s.tabs;
    for (guildtabs::Position& p : w.positions) p.name = utf(s, p.name);
    for (guildtabs::Expelled& e : w.expelled) e.name = utf(s, e.name), e.account = utf(s, e.account),
                                               e.reason = utf(s, e.reason);
    w.subject = utf(s, w.subject);
    w.body = utf(s, w.body);
    for (guildtabs::Alliance& a : w.alliances) a.name = utf(s, a.name);
    for (guildtabs::Member& m : w.members) m.name = utf(s, m.name);
    s.words = std::move(w);
}

void render(State& s) { ++s.sheetGen; }

void block(const State& s, HWND game) {
    RECT client{};
    if (s.targetW <= 0 || s.targetH <= 0 || !GetClientRect(game, &client)) return;
    const panel::Box& b = s.layout.glass;
    const float x0 = float(s.layout.x) + b.x, y0 = float(s.layout.y) + b.y;
    const RECT r = clientBox(client, s.targetW, s.targetH, x0, y0, b.w, b.h);
    uapad::gamewindow::setBlocked(Blocker::Guild, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(Blocker::Guild, nullptr, nullptr, false);
    s.shown = false;
    s.show.typing = false;
}

bool modeMsg(int msg, int a, int b, int c) {
    const uint32_t mode = read32(kModeNow);
    if (!mode || read32(read32(mode) + kModeMsgSlot) != kModeMsg) return false;
    reinterpret_cast<ModeMsgFn>(uintptr_t(kModeMsg))(mode, msg, a, b, c);
    return true;
}

const GuildSkill* skillOf(const State& s, int id) {
    for (const GuildSkill& k : s.skills)
        if (k.id == id) return &k;
    return nullptr;
}

const gs::Row* rowOf(const State& s, int id) {
    for (const gs::Row& r : s.rows)
        if (r.id == id) return &r;
    return nullptr;
}

// 3.0.11-beta.220: a guild skill's «+» - the game mode's 0x4A as its own window's (the packet 0x0112)
void raiseSkill(State& s, int id, const char* by) {
    const gs::Row* r = rowOf(s, id);
    if (!r || !r->plus) return;
    const bool sent = modeMsg(kRaiseMsg, id, 0, 0);
    if (s.told++ < 30)
        LOG_INFO("guild: %s its skill %d +1 (level %d) %s", by, id, r->level,
                 sent ? "asked (0x4A, the packet 0x0112)" : "not asked");
    s.key.clear();
}

// 3.0.22-beta.26: «Використати» - the game mode's 0x7E with the game's own record, as its guild window's «use»
void useSkill(State& s, int id, const char* by) {
    const gs::Row* r = rowOf(s, id);
    const GuildSkill* k = skillOf(s, id);
    if (!r || !r->usable) return;
    if (r->coolMs > 0) {
        if (s.told++ < 30) LOG_INFO("guild: %s %d - waits %d s more (the shared block)", by, id, r->coolMs / 1000);
        return;
    }
    const bool sent = k && k->record && modeMsg(kUseMsg, int(k->record), k->level, 0);
    if (s.told++ < 30)
        LOG_INFO("guild: %s uses its skill %d at level %d %s", by, id, r->level,
                 sent ? "as the game's own window (0x7E)" : "not sent (not in the game's list)");
}

// A skill dragged out and let go: the game's drag of it begun (0x51) and let go where the game's cursor is (0x12)
const char* dropSkill(State& s, int id) {
    const gs::Row* r = rowOf(s, id);
    const GuildSkill* k = skillOf(s, id);
    if (!r || !r->usable || !k || !k->name) return "not dropped (not one to use)";
    int cx = 0, cy = 0;
    if (!readGameCursor(cx, cy)) return "not dropped (no cursor)";
    if (!modeMsg(kDragMsg, id, int(k->name), k->level)) return "not dropped (the game mode not found)";
    modeMsg(kDropMsg, cx, cy, 0);
    return "let go through the game's own drop";
}

// 3.0.11-beta.271: the notice edited on the card - saved through the game mode's message 0x83 (the guild, the subject,
// the body: the packet 0x016e, as the game's own notice window sends it)
constexpr int kNoticeMsg = 0x83;

std::string gameBytes(const std::string& utf8, unsigned codePage, size_t most) {
    const std::wstring w = utf8Wide(utf8);
    std::string out;
    for (const wchar_t c : w) {  // whole letters only, up to the server's room
        char buf[8] = {};
        const int n = WideCharToMultiByte(codePage ? codePage : CP_ACP, 0, &c, 1, buf, int(sizeof buf), nullptr,
                                          nullptr);
        if (n <= 0 || out.size() + size_t(n) > most) break;
        out.append(buf, size_t(n));
    }
    return out;
}

void startNotice(State& s) {
    s.show.editing = true;
    s.show.subject = s.words.subject;
    s.show.body = s.words.body;
    s.show.field = 0;
    LOG_INFO("guild: the notice edited on the card");
    s.key.clear();
}

void saveNotice(State& s) {
    const uint32_t mode = read32(kModeNow), guild = read32(kGuildId);
    const std::string subject = gameBytes(s.show.subject, s.codePage, gc::kSubjectMost);
    const std::string body = gameBytes(s.show.body, s.codePage, gc::kBodyMost);
    const bool sent = mode && guild && read32(read32(mode) + kModeMsgSlot) == kModeMsg;
    if (sent)
        reinterpret_cast<ModeMsgFn>(uintptr_t(kModeMsg))(mode, kNoticeMsg, int(guild),
                                                          int(reinterpret_cast<uintptr_t>(subject.c_str())),
                                                          int(reinterpret_cast<uintptr_t>(body.c_str())));
    LOG_INFO("guild: the notice saved - %zu + %zu bytes %s", subject.size(), body.size(),
             sent ? "sent (0x83, the packet 0x016e)" : "not sent (no game mode or guild)");
    s.show.editing = false;
    s.key.clear();
}

// 3.0.11-beta.220: «Змінити» - the game's own tab window shown to edit the positions (3.0.22-beta.27: or the members:
// positions and expelling)
void editInGame(State& s) {
    int tab = -1;
    const uint32_t window = gameGuild(&tab);
    if (!window || !gc::gameDrawn(tab, true)) return;
    s.editing = window;
    LOG_INFO("guild: «Змінити» - the game's own tab %d shown to edit (back to the card on another tab)", tab);
    s.key.clear();
}

void closeIt(State& s) {
    const uint32_t window = gameGuild();
    if (!window) return;
    if (s.told++ < 30) LOG_INFO("guild: closed (the open tab's 0x87)");
    reinterpret_cast<SendMsgFn>(uintptr_t(read32(read32(window) + kSendMsgSlot)))(window, 0, kButtonMsg,
                                                                                   gc::kCloseButton, 0, 0);
    s.key.clear();
}

const gc::Member* chosenMember(const State& s) {
    const std::vector<size_t> order = gc::memberOrder(s.members, s.words, s.show);
    for (size_t i : order)
        if (s.show.member && s.members[i].character == s.show.member) return &s.members[i];
    return order.empty() ? nullptr : &s.members[order.front()];
}

// 3.0.22-beta.26: a member card's action - «Написати» (the chat's whisper line), «У друзі» (the game's 0x0202)
void act(State& s, int a) {
    const gc::Member* m = chosenMember(s);
    if (!m || m->name.empty() || !(kActsNow >> a & 1)) return;
    if (a == int(gc::Act::Whisper)) {
        const bool ok = chatwin::whisperTo(m->name);
        LOG_INFO("guild: «Написати» %s - %s", m->name.c_str(), ok ? "the chat's whisper line" : "no chat line");
    } else if (a == int(gc::Act::Party)) {  // 3.0.22-beta.27: to one in the game, by the account
        const bool sent = m->online && m->account && modeMsg(kPartyInviteMsg, int(m->account), 0, 0);
        LOG_INFO("guild: «Запросити в групу» %s - %s", m->name.c_str(),
                 sent ? "the game mode's 0x41 (the packet 0x00fc)" : m->online ? "no game mode" : "not in the game");
    } else if (a == int(gc::Act::Position)) {  // the master's: the game's own members' window
        if (gc::isMaster(s.show.rights)) editInGame(s);
        std::string name = narrowed(widened(m->name, CP_UTF8), s.codePage);
        if (name.empty() || name.size() > 23) return;
        char buf[24] = {};
        std::memcpy(buf, name.data(), name.size());
        reinterpret_cast<AddFriendFn>(uintptr_t(kAddFriend))(buf);
        LOG_INFO("guild: «У друзі» %s (the game's 0x0202)", m->name.c_str());
    }
    s.key.clear();
}

// The chosen row moved by `by` in the open tab's list (the controller's D-pad)
void step(State& s, int by) {
    gc::Show& sh = s.show;
    if (sh.tab == gc::Tab::Skills) {
        const std::vector<gs::Def>& t = gs::table();
        int i = std::max(0, gs::indexOf(sh.skill));
        i = std::clamp(i + by, 0, int(t.size()) - 1);
        sh.skill = t[size_t(i)].id;
    } else if (sh.tab == gc::Tab::Members) {
        const std::vector<size_t> order = gc::memberOrder(s.members, s.words, sh);
        if (order.empty()) return;
        int at = 0;
        for (size_t i = 0; i < order.size(); ++i)
            if (s.members[order[i]].character == sh.member) at = int(i);
        at = std::clamp(at + by, 0, int(order.size()) - 1);
        sh.member = s.members[order[size_t(at)]].character;
        int& first = sh.first[int(gc::Tab::Members)];
        if (at < first) first = at;
        if (at >= first + gc::kMemberRows) first = at - gc::kMemberRows + 1;
    } else if (sh.tab == gc::Tab::Positions) {
        const gc::PositionsView v = gc::positionsView(s.words, sh, texts());
        int at = 0;
        for (size_t i = 0; i < v.rows.size(); ++i)
            if (v.rows[i].focus) at = int(i);
        if (!v.rows.empty()) sh.position = v.rows[size_t(std::clamp(at + by, 0, int(v.rows.size()) - 1))].id;
    } else if (sh.tab == gc::Tab::Expelled) {
        const int n = int(s.words.expelled.size());
        sh.expelled = std::clamp(sh.expelled + by, 0, std::max(0, n - 1));
        int& first = sh.first[int(gc::Tab::Expelled)];
        if (sh.expelled < first) first = sh.expelled;
        if (sh.expelled >= first + gc::kExpelledRows) first = sh.expelled - gc::kExpelledRows + 1;
    }
    s.key.clear();
}

// The pad's X: the chosen war skill carried to the hotbar (win/pad_carry, «##AEGIS» as «Навички»'s)
void carry(State& s) {
    const gs::Row* r = rowOf(s, s.show.skill);
    const gs::Def* d = gs::def(s.show.skill);
    if (!r || !r->usable || !d) return;
    uagfx::padcarry::Thing t;
    t.from = uagfx::padcarry::From::Skill;
    t.value = std::string("##") + d->aegis;
    t.level = r->level;
    const int side = std::max(8, int(std::lround(24 * s.scale)));
    const panel::Picture* icon = clienttables::icon(itemwin::lowered(d->aegis), side, 4 * s.scale);
    float x = float(s.layout.x) + float(s.layout.w) * 0.3f, y = float(s.layout.y) + float(s.layout.h) * 0.5f;
    for (const gc::HitBox& b : s.layout.hits)
        if (b.part == gc::Part::Skill && b.index == r->id) x = float(s.layout.x) + b.box.x + 20 * s.scale,
                                                           y = float(s.layout.y) + b.box.y + b.box.h / 2;
    padcarrywin::begin(t, x, y, icon, d->name);
    LOG_INFO("guild: the controller's X - %s at level %d taken up", d->aegis, r->level);
}

std::string keyOf(const State& s) {
    const gc::Info& g = s.info;
    const gc::Show& sh = s.show;
    std::string key = g.name + ';' + g.master + ';' + g.territory + ';' + std::to_string(g.level) + ',' +
                      std::to_string(g.most) + ',' + std::to_string(g.average) + ',' + std::to_string(g.exp) + ',' +
                      std::to_string(g.nextExp) + ';' + std::to_string(int(sh.tab)) + ',' + std::to_string(sh.rights) +
                      ',' + std::to_string(int(sh.hover.part)) + ':' + std::to_string(sh.hover.index) + ',' +
                      std::to_string(sh.skill) + ',' + std::to_string(sh.member) + ',' + std::to_string(sh.position) +
                      ',' + std::to_string(sh.expelled) + ',' + (sh.pad ? (sh.sony ? "S" : "X") : "") + ',' +
                      std::to_string(int(sh.sort)) + '|' + sh.search + (sh.typing ? "|T" : "|") +
                      (sh.typing && sh.caret ? "c" : "") + ',' + std::to_string(sh.coolMs / 1000) + ',' + sh.me + ';' +
                      std::to_string(s.layout.x) + ',' + std::to_string(s.layout.y) + ',' + std::to_string(s.layout.w) +
                      '#' + std::to_string(uistyle::generation()) + '#' + s.emblemFor +
                      (s.emblemSmall.width > 0 ? "e" : "") + '#' + std::to_string(s.points);
    for (int i = 0; i < gc::kTabs; ++i) key += ',' + std::to_string(sh.first[i]);
    for (const gc::Member& m : s.members)
        key += ';' + m.name + ',' + m.title + ',' + m.job + ',' + std::to_string(m.level) + (m.online ? "o" : "");
    for (const gs::Row& r : s.rows)
        key += ';' + std::to_string(r.id) + ',' + std::to_string(r.level) + (r.plus ? "+" : "") + (r.usable ? "u" : "");
    const guildtabs::Data& d = s.words;
    for (const guildtabs::Position& p : d.positions)
        key += ";p" + std::to_string(p.id) + p.name + (p.invite ? "i" : "") + (p.expel ? "e" : "") +
               std::to_string(p.tax);
    for (const guildtabs::Expelled& e : d.expelled) key += ";x" + e.name + ',' + e.account + ',' + e.reason;
    for (const guildtabs::Alliance& a : d.alliances) key += ";a" + a.name + (a.enemy ? "!" : "");
    for (const guildtabs::Member& m : d.members)
        key += ";m" + std::to_string(m.character) + ',' + std::to_string(m.exp) + ',' + std::to_string(m.position);
    key += '|' + d.subject + '|' + d.body + (d.alliancesSeen ? "A" : "") + (d.membersSeen ? "M" : "") +
           (s.noGuild ? "|N" : "");
    if (sh.editing)  // .271
        key += "|E" + std::to_string(sh.field) + (sh.caret ? "|" : "") + '|' + sh.subject + '|' + sh.body;
    return key;
}

}  // namespace

static void frameInner(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    State& s = state();
    if (codePage != s.codePage) {
        s.codePage = codePage;
        wordsOf(s);
    }
    if (on != s.on) {
        s.on = on;
        s.key.clear();
        LOG_INFO("guild: [interface] guild = %d", on ? 1 : 0);
    }
    const bool world = inGameWorld();
    int tab = 0;
    const uint32_t window = world ? gameGuild(&tab) : 0;
    // 3.0.11-beta.220: all six tabs the card's; the game's own window while «Змінити» edits the positions there
    if (s.editing && s.editing != window) s.editing = 0;
    if (s.noGuild && (!world || !on || read32(kGuildId) || window)) s.noGuild = false;  // 3.0.22-beta.27
    const bool ours = (window && !gc::gameDrawn(tab, s.editing == window)) || s.noGuild;
    if (world) hold(s, window, on && ours);
    if (!on || !ours || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        hide(s);
        return;
    }
    if (s.show.tab != gc::Tab(tab)) s.show.typing = false;
    s.show.tab = gc::Tab(tab);
    s.show.rights = read32(kRights);
    if (!s.shown) LOG_INFO("guild: the game's guild window - the new card shows it");
    s.k = float(k);
    // 3.0.18-beta.151: times its own size (its corner pulled)
    s.scale = uistyle::readingScale(k) * windowstack::sizeOf(winstack::Win::Guild);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    const ULONGLONG now = GetTickCount64();
    s.show.coolMs = gs::coolLeft(s.usedAt, now);
    readGuild(s);
    readSkills(s);
    if (now - s.meAt > 2000 || s.show.me.empty()) {
        s.meAt = now;
        s.show.me = utf(s, readOwnName());
    }
    if (s.show.editing && s.show.tab != gc::Tab::Notice) s.show.editing = false;  // .271: another tab - let go
    const uapad::runtime::Look pad = uapad::runtime::navLook();
    s.show.pad = pad.shown;
    s.show.sony = pad.sony;
    const std::vector<gc::HitBox> hits = std::move(s.layout.hits);
    s.layout = gc::layout(targetWidth, targetHeight, s.scale);
    s.layout.hits = hits;
    readEmblem(s);  // 3.0.19-beta.1
    if (s.live.shown()) uikit::guild::refit(s.live.doc.doc(), s.layout);  // RmlUi's boxes
    // 3.0.18-beta.92 (the owner: «не все окна перетаскиваются»): moved by a press on its empty part
    s.move.apply(s.layout.x, s.layout.y, s.layout.w, s.layout.h, targetWidth, targetHeight, k, 48 * s.scale);
    s.show.caret = (now / 500) % 2 == 0;
    s.show.hover = gc::Hit{};
    int cx = 0, cy = 0;
    if (readHoverCursor(cx, cy))
        s.show.hover = gc::hitTest(s.layout, float(cx) * float(k) - float(s.layout.x),
                                   float(cy) * float(k) - float(s.layout.y));
    if (const std::string drawn = keyOf(s) + (uaui::ready() ? "u" : ""); drawn != s.key) {  // 3.0.18-beta.34
        s.key = drawn;
        render(s);
    }
    s.shown = true;
    block(s, game);
}

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    frameInner(game, on, codePage, k, targetWidth, targetHeight);
    State& s = state();
    namespace gl = uikit::guild;
    s.sheetShown = s.live.frame("guild", s.shown, s.layout.x, s.layout.y, s.sheetGen, gl::rml(), gl::bind,
                                [&](gl::Model& m) {
                                    m.ground = s.live.ground();
                                    const gc::Texts t = texts();
                                    const gc::Show& sh = s.show;
                                    const bool master = gc::isMaster(sh.rights);
                                    gl::Frame f;
                                    f.scale = s.scale;
                                    f.name = s.info.name.empty() ? t.title : s.info.name;
                                    int online = 0;
                                    for (const gc::Member& x : s.members) online += x.online ? 1 : 0;
                                    f.sub = gc::fmt(t.sub, {std::to_string(s.info.level), std::to_string(online),
                                                            std::to_string(s.members.size())});
                                    f.hotkey = t.hotkey;
                                    f.closeHot = sh.hover.part == gc::Part::Close;
                                    for (int i = 0; i < gc::kTabs; ++i) {
                                        const bool ok = gc::allowed(i, sh.rights);
                                        f.tabs.push_back(gl::TabV{
                                            t.tabs[i], i == 1 ? std::to_string(s.members.size()) : "",
                                            i == 3 && master && s.points > 0 ? std::to_string(s.points) : "",
                                            i == int(sh.tab), ok && sh.hover.part == gc::Part::Tab &&
                                                                  sh.hover.index == i && i != int(sh.tab),
                                            !ok});
                                    }
                                    f.tab = sh.tab;
                                    f.skills = gc::skillsView(s.rows, sh, t, master);
                                    f.info = gc::infoView(s.info, s.members, s.words, s.rows, sh, t);
                                    f.members = gc::membersView(s.info, s.members, s.words, sh, t, master, kActsNow);
                                    f.positions = gc::positionsView(s.words, sh, t);
                                    f.expelled = gc::expelledView(s.words, sh, t);
                                    f.notice = gc::noticeView(
                                        s.words, sh, t, gameBytes(sh.editing ? sh.subject : s.words.subject,
                                                                  s.codePage, 200).size(),
                                        gameBytes(sh.editing ? sh.body : s.words.body, s.codePage, 400).size());
                                    f.foot = gc::footOf(sh, t, s.points, master);
                                    f.typing = sh.typing;
                                    f.caret = sh.caret;
                                    if (s.noGuild) {  // 3.0.22-beta.27
                                        f.noGuild = true;
                                        f.name = t.title;
                                        f.sub.clear();
                                        f.ngHead = t.noGuild;
                                        f.ngSub = t.noGuildSub;
                                        f.ngJoin = t.join;
                                        f.ngJoinText = t.joinText;
                                        f.ngMake = t.make;
                                        f.ngMakeText = t.makeText;
                                    }
                                    f.searchHot = sh.hover.part == gc::Part::Search;
                                    f.allHot = sh.hover.part == gc::Part::AllMembers;
                                    gl::fill(
                                        m, f,
                                        [&](const std::string& aegis, int side, float& w) {
                                            const panel::Picture* p =
                                                clienttables::icon(aegis, side, std::max(2.0f, side / 6.0f));
                                            w = p ? float(p->width) : 0;
                                            return p ? s.live.icon(p) : std::string();
                                        },
                                        [&](int side) {
                                            if (s.noGuild) return std::string();
                                            const panel::Picture& p =
                                                side > int(std::lround(36 * s.scale)) ? s.emblemBig : s.emblemSmall;
                                            return p.width > 0 ? s.live.icon(&p) : std::string();
                                        },
                                        [&](int button, float h, float& w) {
                                            w = std::ceil(padglyph::widthOf(padglyph::Button(button), h));
                                            const int iw = std::max(1, int(w)), ih = std::max(1, int(h));
                                            const std::string key = "pad" + std::to_string(button) +
                                                                    (sh.sony ? "s" : "x") + std::to_string(iw) +
                                                                    'x' + std::to_string(ih);
                                            return s.live.made(key, iw, ih, [&](panel::Picture& p) {
                                                paintPadBadge(p, button, sh.sony, h);
                                            });
                                        });
                                });
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    s.live.raise();  // 3.0.18-beta.43: at its place in the windows' stack
    if (!s.shown || s.k <= 0 || !s.sheetShown) return;
    int cursor = -1, cx = 0, cy = 0;
    const bool known = readGameCursor(cx, cy);
    if (known) cursor = panel::cursorDraw(record, float(cx), float(cy));
    const panel::Box& g = s.layout.glass;
    // 3.0.18-beta.43: the shadow the driver's, under uaUI's window too
    s.shadow.draw(record, renderer, s.layout.x + int(g.x), s.layout.y + int(g.y), int(g.w), int(g.h),
                  12 * s.scale, s.scale, s.k);
    const gs::Def* d = s.press.dragging ? gs::def(s.pressSkill) : nullptr;
    const int side = std::max(8, int(std::lround(24 * s.scale)));
    const panel::Picture* icon = d ? clienttables::icon(itemwin::lowered(d->aegis), side, 4 * s.scale) : nullptr;
    if (icon && icon->width > 0 && known) {  // the dragged skill's icon under the cursor, over the windows
        if (icon != s.dragOf) {
            s.dragOf = icon;
            s.dragIcon = *icon;
            s.dragUpload = true;
        }
        const int x = int(std::lround(float(cx) * s.k)) - icon->width / 2;
        const int y = int(std::lround(float(cy) * s.k)) - icon->height / 2;
        carryicon::put(s.dragIcon, s.dragTexture, s.dragW, s.dragH, s.dragUpload, x, y, s.k);
    }
    if (cursor >= 0) record.moveLastBefore(record.commands().size() - size_t(cursor) - 1, size_t(cursor));
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    if (m == WM_LBUTTONUP) s.move.end();  // 3.0.18-beta.92
    if (!s.shown) return false;
    const bool button = mouseButton(m);
    if (!button && m != WM_MOUSEWHEEL && m != WM_MOUSEMOVE) return false;
    float tx = 0, ty = 0;
    if (!pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty, nullptr)) return false;
    if (s.press.index >= 0 && (m == WM_MOUSEMOVE || m == WM_LBUTTONUP)) {  // a skill pressed: dragged, or a click
        if (m == WM_MOUSEMOVE) {
            const bool was = s.press.dragging, now = windrag::moved(s.press, tx, ty, s.scale);
            if (now && !was) windrag::set(windrag::Thing{windrag::Kind::Skill, s.pressSkill, 0, 0});
            return now;
        }
        if (s.press.dragging) windrag::clear();
        const int dragged = windrag::released(s.press);
        if (dragged >= 0) {
            const char* did = dropSkill(s, dragged);
            if (s.told++ < 30) LOG_INFO("guild: its skill %d dragged and let go - %s", dragged, did);
        }
        s.pressSkill = 0;
        result = 0;
        return true;
    }
    if (m == WM_MOUSEMOVE) return false;
    const float lx = tx - float(s.layout.x), ly = ty - float(s.layout.y);
    if (!s.layout.glass.contains(lx, ly)) {
        if (m == WM_LBUTTONDOWN && s.show.typing) {
            s.show.typing = false;
            s.key.clear();
        }
        return false;
    }
    result = 0;
    gc::Show& sh = s.show;
    if (m == WM_MOUSEWHEEL) {  // the long lists, three rows a notch
        const int by = GET_WHEEL_DELTA_WPARAM(wp) > 0 ? -3 : 3;
        if (sh.tab == gc::Tab::Members) {
            const int n = int(gc::memberOrder(s.members, s.words, sh).size());
            sh.first[1] = std::clamp(sh.first[1] + by, 0, std::max(0, n - gc::kMemberRows));
        } else if (sh.tab == gc::Tab::Expelled) {
            const int n = int(s.words.expelled.size());
            sh.first[4] = std::clamp(sh.first[4] + by, 0, std::max(0, n - gc::kExpelledRows));
        }
        s.key.clear();
        return true;
    }
    if (m != WM_LBUTTONDOWN && m != WM_LBUTTONDBLCLK) return true;
    const gc::Hit h = gc::hitTest(s.layout, lx, ly);
    if (h.part != gc::Part::Search) sh.typing = false;
    // 3.0.18-beta.92; .102: its empty part moves it
    if ((h.part == gc::Part::None || h.part == gc::Part::Body || h.part == gc::Part::Title) && m == WM_LBUTTONDOWN)
        s.move.begin(tx, ty);
    switch (h.part) {
        case gc::Part::Tab: switchTab(s, h.index); break;  // 3.0.11-beta.210: the game's own tab with it
        case gc::Part::Close:
            if (s.noGuild) s.noGuild = false;
            else closeIt(s);
            break;
        case gc::Part::Skill:
            sh.skill = h.index;
            if (m == WM_LBUTTONDBLCLK) {
                useSkill(s, h.index, "a double click");
            } else if (const gs::Row* r = rowOf(s, h.index); r && r->usable) {
                windrag::pressed(s.press, h.index, tx, ty);
                s.pressSkill = h.index;
            }
            break;
        case gc::Part::Plus: raiseSkill(s, h.index, "its «+»"); break;
        case gc::Part::Use: useSkill(s, sh.skill, "«Використати»"); break;
        case gc::Part::Raise: raiseSkill(s, sh.skill, "«Підняти»"); break;
        case gc::Part::Member: sh.member = uint32_t(h.index); break;
        case gc::Part::Head: {
            const gc::Sort sorts[5] = {gc::Sort::Online, gc::Sort::Online, gc::Sort::Level, gc::Sort::Position,
                                       gc::Sort::Given};
            if (h.index >= 0 && h.index < 5) sh.sort = sorts[h.index];
            sh.first[1] = 0;
            break;
        }
        case gc::Part::Search: sh.typing = true; break;
        case gc::Part::Action: act(s, h.index); break;
        case gc::Part::Position: sh.position = h.index; break;
        case gc::Part::Expelled: sh.expelled = h.index + sh.first[4]; break;
        case gc::Part::AllMembers: switchTab(s, int(gc::Tab::Members)); break;
        case gc::Part::Edit:
            if (!gc::isMaster(sh.rights)) break;
            if (sh.tab == gc::Tab::Positions) editInGame(s);  // the game's own window still
            else if (sh.tab == gc::Tab::Notice && !sh.editing) startNotice(s);
            break;
        case gc::Part::Save:
            if (sh.editing) saveNotice(s);
            break;
        case gc::Part::Cancel: sh.editing = false; break;
        case gc::Part::Field:
            if (sh.editing) sh.field = h.index;
            break;
        default: break;
    }
    s.key.clear();
    return true;
}

bool shown() { return state().shown; }

bool close() {
    State& s = state();
    if (!s.shown) return false;
    if (s.show.typing) {  // Esc ends the search's typing first
        s.show.typing = false;
        s.key.clear();
        return true;
    }
    if (s.noGuild) {
        s.noGuild = false;
        s.key.clear();
        return true;
    }
    closeIt(s);
    return true;
}

// 3.0.22-beta.27: Alt+G with no guild - the game's key handler opens its guild window only with a guild id (0x74B6D4),
// so this card tells how to join and how to make one; again - gone
bool altG(UINT m, WPARAM wp, LPARAM lp) {
    State& s = state();
    if (m != WM_SYSKEYDOWN || wp != 'G' || !(lp & (1 << 29)) || !s.on || !inGameWorld() || read32(kGuildId))
        return false;
    if (lp & (1 << 30)) return true;  // held down: once
    s.noGuild = !s.noGuild;
    s.key.clear();
    LOG_INFO("guild: Alt+G with no guild - the card «%s» %s", "no guild", s.noGuild ? "shown" : "closed");
    return true;
}

// .271: the notice's fields take the keys while it is edited; 3.0.22-beta.26: «Учасники»'s search while typed into,
// Ctrl+F starts it
bool key(UINT m, WPARAM wp, LPARAM lp) {
    State& s = state();
    (void)lp;
    if (!s.shown) return false;
    gc::Show& sh = s.show;
    if (!sh.editing && !sh.typing) {
        if ((m == WM_KEYDOWN) && wp == 'F' && GetKeyState(VK_CONTROL) < 0 && sh.tab == gc::Tab::Members) {
            sh.typing = true;
            s.key.clear();
            return true;
        }
        return false;
    }
    std::string& f = sh.typing ? sh.search : sh.field ? sh.body : sh.subject;
    if (m == WM_KEYDOWN || m == WM_SYSKEYDOWN) {
        if (GetKeyState(VK_MENU) < 0) return false;
        switch (wp) {
            case VK_ESCAPE:
                if (sh.typing) sh.typing = false;
                else sh.editing = false;
                break;
            case VK_RETURN:
                if (sh.typing) sh.typing = false;
                else saveNotice(s);
                break;
            case VK_TAB:
                if (!sh.typing) sh.field = 1 - sh.field;
                break;
            case VK_BACK: {  // a whole letter off (UTF-8)
                while (!f.empty() && (static_cast<unsigned char>(f.back()) & 0xC0) == 0x80) f.pop_back();
                if (!f.empty()) f.pop_back();
                if (sh.typing) sh.first[1] = 0;
                break;
            }
            default: break;
        }
        s.key.clear();
        return true;
    }
    if (m == WM_CHAR) {  // the game's window is ANSI: a letter in the system's code page
        const unsigned char byte = static_cast<unsigned char>(wp);
        if (byte >= 0x20 && byte != 0x7F) {
            wchar_t w = 0;
            const char c = char(byte);
            if (MultiByteToWideChar(CP_ACP, 0, &c, 1, &w, 1) == 1) {
                const std::string add = utf8Of(std::wstring(1, w));
                if (sh.typing) {
                    if (sh.search.size() < 48) sh.search += add;
                    sh.first[1] = 0;
                } else {
                    const size_t most = sh.field ? gc::kBodyMost : gc::kSubjectMost;
                    if (gameBytes(f + add, s.codePage, most + 1).size() <= most) f += add;
                }
            }
        }
        s.key.clear();
        return true;
    }
    return m == WM_KEYUP || m == WM_SYSKEYUP || m == WM_DEADCHAR;
}

void beforeGameDraw() {
    State& s = state();
    if (!s.on || !inGameWorld()) return;
    int tab = 0;
    const uint32_t window = gameGuild(&tab);
    hold(s, window, !gc::gameDrawn(tab, s.editing && s.editing == window));  // 3.0.11-beta.220: unless editing
}

void padCommand(PadCommand c) {
    State& s = state();
    if (!s.shown) return;
    if (s.noGuild) {  // 3.0.22-beta.27: B or Start closes it
        if (c == PadCommand::Close) s.noGuild = false;
        s.key.clear();
        return;
    }
    gc::Show& sh = s.show;
    switch (c) {
        case PadCommand::Up: step(s, -1); break;
        case PadCommand::Down: step(s, 1); break;
        case PadCommand::TabBack:
        case PadCommand::TabOn: {  // 3.0.11-beta.210: the next allowed tab, as the game's own
            const int by = c == PadCommand::TabOn ? 1 : gc::kTabs - 1;
            int next = int(sh.tab);
            for (int n = 0; n < gc::kTabs; ++n) {
                next = (next + by) % gc::kTabs;
                if (gc::allowed(next, sh.rights)) break;
            }
            switchTab(s, next);
            break;
        }
        case PadCommand::Use:  // 3.0.22-beta.26: A - the skill used, a member written to
            if (sh.tab == gc::Tab::Skills) useSkill(s, sh.skill, "the controller's A");
            else if (sh.tab == gc::Tab::Members) act(s, int(gc::Act::Whisper));
            break;
        case PadCommand::Raise:
            if (sh.tab == gc::Tab::Skills) raiseSkill(s, sh.skill, "the controller's Y");
            break;
        case PadCommand::Assign:
            if (sh.tab == gc::Tab::Skills) carry(s);
            break;
        case PadCommand::Close: closeIt(s); break;
    }
    s.key.clear();
}

bool box(int& x, int& y, int& w, int& h) {
    const State& s = state();
    if (!s.shown) return false;
    x = s.layout.x;
    y = s.layout.y;
    w = s.layout.w;
    h = s.layout.h;
    return true;
}

void packet(uint16_t id, const uint8_t* data, size_t size) {
    State& s = state();
    if (id == 0x011A) {  // 3.0.22-beta.26: a skill on someone - the player's war skill begins the shared block
        if (!data || size < 15) return;
        const int skill = int(data[2] | data[3] << 8);
        uint32_t src = 0;
        std::memcpy(&src, data + 10, 4);
        if (gs::shared(skill) && src && src == read32(kOwnAccount)) {
            s.usedAt = GetTickCount64();
            LOG_INFO("guild: its skill %d used by the player - the four wait 5 minutes (guild_block_skill)", skill);
            s.key.clear();
        }
        return;
    }
    if (guildtabs::read(s.tabs, data, size)) {
        if (s.told++ < 40) LOG_INFO("guild: the server's 0x%04X taken for the card's tabs", unsigned(id));
        wordsOf(s);
        s.key.clear();
    }
}

bool mateLook(uint32_t account, MateLook& out) {
    if (!account) return false;
    const uint32_t head = read32(kMembers);
    if (!head) return false;
    uint32_t node = read32(head);
    for (size_t n = 0; node && node != head && n < kMostMembers; ++n, node = read32(node)) {
        uint32_t f[16] = {};  // the account, the character, the name (a string), hair +0x18, colour, class, sex
        if (!readGame(node + kMember, f, sizeof f)) break;
        if (f[0] != account) continue;
        out = MateLook{int(f[9]) ? 1 : 0, int(f[6]), int(f[7]), int(f[8]), int(f[15])};
        return out.hair > 0 && out.hair < 1000;
    }
    return false;
}

std::string mateName(uint32_t account) {
    if (!account) return {};
    const uint32_t head = read32(kMembers);
    if (!head) return {};
    uint32_t node = read32(head);
    for (size_t n = 0; node && node != head && n < kMostMembers; ++n, node = read32(node)) {
        uint32_t f[5] = {};
        if (!readGame(node + kMember, f, sizeof f)) break;
        if (f[0] != account && f[1] != account) continue;
        char buf[65] = {};
        const uint32_t len = std::min<uint32_t>(f[4], 64);
        if (!len || !readGame(f[3], buf, len)) return {};
        static bool told = false;
        if (!told) {
            told = true;
            LOG_INFO("guild: a mate's name on the map from the members' list (the account in its field %d)",
                     f[0] == account ? 0 : 1);
        }
        return utf8Of(widened(std::string(buf, len), state().codePage));
    }
    return {};
}

}  // namespace uagfx::win::guildcardwin

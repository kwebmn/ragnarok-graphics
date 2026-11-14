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
#include "core/guild_tabs.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
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
using uapad::gamewindow::Blocker;

// uaRO.exe's: the guild's six windows (their classes' vtables and SendMsgs, in the manager from + 0x170), a window's
// shown flag; the guild object: its name's, master's and territory's words (a string's pointer and count), its
// numbers (the level, the members of the most, the average level, the experience and the next, the tax points), its
// members' list (a node: next +0, the member +8 - its name's words +0xC / +0x10, job +0x20, title +0x30 / +0x34,
// level +0x3C, online +0x58)
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

struct State {
    CardDrag move;  // 3.0.18-beta.92: moved by its empty part
    // 3.0.18-beta.102 (docs/superpowers/plans/2026-11-10-uaui-live-drag.md): live - its head and tabs by RmlUi, its
    // body layered
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
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    int told = 0;
    guildtabs::Data tabs;     // 3.0.11-beta.220: tabs 2-5 as the server sent them
    gc::More more;
    uint32_t editing = 0;     // 3.0.11-beta.220: the game's own tab window «Змінити» showed (0 none)
    // 3.0.19-beta.1: the guild's emblem from the game's file (empty: none found), what it was read for, when
    panel::Picture emblem;
    std::string emblemFor;
    ULONGLONG emblemAt = 0;
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

// 3.0.19-beta.1 (the testers: «не видно значок ГІ твоеї»): the guild's emblem as the game keeps it -
// <client>\_tmpEmblem\<server>_<guild>_<emblem>.ebm (uaRO.exe FUN_0063aab0 writes it from the packet 0x0152; the
// server's name is the session's, so any) at the card's emblem size; looked for again every 3 s while not there
void readEmblem(State& s) {
    const uint32_t guild = read32(kGuildId), emblem = read32(kEmblemId);
    const int side = int(std::lround(s.layout.emblem.w));
    if (!guild || side <= 0) return;
    const std::string want = std::to_string(guild) + '_' + std::to_string(emblem) + '_' + std::to_string(side);
    const ULONGLONG now = GetTickCount64();
    if (want == s.emblemFor && (s.emblem.width > 0 || now - s.emblemAt < 3000)) return;
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
    std::vector<uint32_t> px = gc::emblemPixels(bytes, side);
    s.emblem = panel::Picture{};
    if (!px.empty()) {
        s.emblem.width = s.emblem.height = side;
        s.emblem.px = std::move(px);
    }
    if (!again || s.emblem.width > 0)
        LOG_INFO("guild: emblem %u of the guild %u - %s", emblem, guild,
                 s.emblem.width > 0 ? ("_tmpEmblem\\" + file + ", " + std::to_string(side) + " px").c_str()
                 : file.empty()     ? "no file in _tmpEmblem yet (looked for again every 3 s)"
                                    : ("_tmpEmblem\\" + file + " not an emblem (" + std::to_string(bytes.size()) +
                                       " bytes)").c_str());
}

gc::Texts texts() {
    gc::Texts t;
    t.title = say(settings::Text::GuildTitle);
    t.tabs[0] = say(settings::Text::GuildInfo);
    t.tabs[1] = say(settings::Text::GuildMembers);
    t.tabs[2] = say(settings::Text::GuildTabPositions);  // 3.0.11-beta.210
    t.tabs[3] = say(settings::Text::GuildTabSkills);
    t.tabs[4] = say(settings::Text::GuildTabExpelled);
    t.tabs[5] = say(settings::Text::GuildTabNotice);
    const settings::Text facts[7] = {settings::Text::GuildMaster,  settings::Text::GuildLevel,
                                     settings::Text::GuildMembers, settings::Text::GuildAverage,
                                     settings::Text::GuildExp,     settings::Text::GuildTax,
                                     settings::Text::GuildTerritory};
    for (int i = 0; i < 7; ++i) t.facts[i] = say(facts[i]);
    t.none = say(settings::Text::GuildNone);
    t.online = say(settings::Text::GuildOnline);
    t.edit = say(settings::Text::GuildEdit);  // 3.0.11-beta.220
    t.fieldTitle = say(settings::Text::GuildNoticeSubject);  // 3.0.11-beta.271
    t.fieldBody = say(settings::Text::GuildNoticeBody);
    t.save = say(settings::Text::GuildNoticeSave);
    t.cancel = say(settings::Text::MsgCancel);
    t.editHint = say(settings::Text::GuildNoticeHint);
    return t;
}

// 3.0.11-beta.220: tabs 2-5's rows from the server's packets
gc::More moreOf(const State& s) {
    gc::More m;
    const auto utf = [&](const std::string& game) { return utf8Of(widened(game, s.codePage)); };
    const guildtabs::Data& d = s.tabs;
    switch (s.show.tab) {
        case gc::Tab::Positions:
            m.head = settings::fmt(say(settings::Text::GuildPositionsHead), {std::to_string(d.positions.size())});
            for (const guildtabs::Position& p : d.positions) {
                std::string rights;
                if (p.invite) rights = say(settings::Text::GuildRightInvite);
                if (p.expel) rights += (rights.empty() ? "" : " \xC2\xB7 ") + say(settings::Text::GuildRightExpel);
                m.lines.push_back(gc::Line{utf(p.name), rights.empty() ? "\xE2\x80\x94" : rights,
                                           std::to_string(p.tax) + " %", false});
            }
            break;
        case gc::Tab::Skills:
            m.head = settings::fmt(say(settings::Text::GuildSkillPoints), {std::to_string(d.points)});
            for (const guildtabs::Skill& k : d.skills) {
                const std::string name = guildtabs::skillName(k.id);
                m.lines.push_back(gc::Line{name.empty() ? "#" + std::to_string(k.id) : name,
                                           settings::fmt(say(settings::Text::MercLevelShort), {std::to_string(k.lv)}),
                                           "", k.up && d.points > 0});
            }
            break;
        case gc::Tab::Expelled:
            m.head = say(settings::Text::GuildTabExpelled);
            for (const guildtabs::Expelled& e : d.expelled)
                m.lines.push_back(gc::Line{utf(e.name), utf(e.account), utf(e.reason), false});
            break;
        case gc::Tab::Notice:
            m.subject = utf(d.subject);
            m.body = utf(d.body);
            break;
        default:
            break;
    }
    return m;
}

void render(State& s) {
    ++s.sheetGen;
}

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
}

// 3.0.11-beta.220: a guild skill's «+» - the game mode's 0x4A as its own window's (the packet 0x0112)
void raiseSkill(State& s, int row) {
    if (s.show.tab != gc::Tab::Skills || row < 0 || size_t(row) >= s.tabs.skills.size()) return;
    const guildtabs::Skill& k = s.tabs.skills[size_t(row)];
    if (!k.up || s.tabs.points <= 0) return;
    const uint32_t mode = read32(kModeNow);
    const bool sent = mode && read32(read32(mode) + kModeMsgSlot) == kModeMsg;
    if (sent) reinterpret_cast<ModeMsgFn>(uintptr_t(kModeMsg))(mode, kRaiseMsg, k.id, 0, 0);
    if (s.told++ < 30)
        LOG_INFO("guild: its skill %d +1 (level %d) %s", k.id, k.lv,
                 sent ? "asked (0x4A, the packet 0x0112)" : "not asked");
    s.key.clear();
}

// 3.0.11-beta.271: the notice edited on the card - started from the notice there is; saved through the game mode's
// message 0x83 (the guild, the subject, the body: the packet 0x016e, as the game's own notice window sends it; the
// guild's id kGuildId)
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
    s.show.subject = s.more.subject;
    s.show.body = s.more.body;
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

// 3.0.11-beta.220: «Змінити» - the game's own tab window shown to edit the positions or the notice
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

}  // namespace

static void frameInner(HWND game, bool on, unsigned codePage, double k, int targetWidth,
                       int targetHeight) {
    State& s = state();
    s.codePage = codePage;
    if (on != s.on) {
        s.on = on;
        s.key.clear();
        LOG_INFO("guild: [interface] guild = %d", on ? 1 : 0);
    }
    const bool world = inGameWorld();
    int tab = 0;
    const uint32_t window = world ? gameGuild(&tab) : 0;
    // 3.0.11-beta.220: all six tabs the card's; the game's own window while «Змінити» edits there (till it goes)
    if (s.editing && s.editing != window) s.editing = 0;
    const bool ours = window && !gc::gameDrawn(tab, s.editing == window);
    if (world) hold(s, window, on && ours);
    if (!on || !ours || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        hide(s);
        return;
    }
    s.show.tab = gc::Tab(tab);
    s.show.rights = read32(kRights);
    if (!s.shown) LOG_INFO("guild: the game's guild window - the new card shows it");
    s.k = float(k);
    // 3.0.18-beta.151: times its own size (its corner pulled)
    s.scale = uistyle::readingScale(k) * windowstack::sizeOf(winstack::Win::Guild);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    readGuild(s);
    s.more = moreOf(s);
    const size_t rows = s.show.tab == gc::Tab::Members ? s.members.size() : s.more.lines.size();
    if (s.show.editing && s.show.tab != gc::Tab::Notice) s.show.editing = false;  // .271: another tab - let go
    s.layout = gc::layout(targetWidth, targetHeight, s.scale, s.show.tab, rows, s.show.editing);
    readEmblem(s);  // 3.0.19-beta.1
    if (s.live.shown()) uikit::guild::refit(s.live.doc.doc(), s.layout);  // 3.0.18-beta.102: its drawn boxes
    // 3.0.18-beta.92 (the owner: «не все окна перетаскиваются»): moved by a press on its empty part
    s.move.apply(s.layout.x, s.layout.y, s.layout.w, s.layout.h, targetWidth, targetHeight, k, 48 * s.scale);
    s.show.caret = (GetTickCount64() / 500) % 2 == 0;
    s.show.scroll = std::clamp(s.show.scroll, 0, gc::mostScroll(s.layout, rows));
    s.show.canEdit = gc::allowed(tab, s.show.rights);
    s.show.hoverTab = -1;
    s.show.hoverClose = false;
    s.show.hoverPlus = -1;
    s.show.hoverEdit = false;
    s.show.hoverField = -1;
    s.show.hoverCancel = false;
    int cx = 0, cy = 0;
    if (readHoverCursor(cx, cy)) {
        const gc::Hit h = gc::hitTest(s.layout, float(cx) * float(k) - float(s.layout.x),
                                      float(cy) * float(k) - float(s.layout.y));
        if (h.part == gc::Part::Tab) s.show.hoverTab = h.index;
        if (h.part == gc::Part::Plus) s.show.hoverPlus = h.index;
        s.show.hoverClose = h.part == gc::Part::Close;
        s.show.hoverEdit = h.part == gc::Part::Edit;
        if (h.part == gc::Part::Field) s.show.hoverField = h.index;  // .271
        s.show.hoverCancel = h.part == gc::Part::Cancel;
    }
    const gc::Info& g = s.info;
    std::string key = g.name + ';' + g.master + ';' + g.territory + ';' + std::to_string(g.level) + ',' +
                      std::to_string(g.most) + ',' + std::to_string(g.average) + ',' + std::to_string(g.exp) + ',' +
                      std::to_string(g.nextExp) + ',' + std::to_string(g.tax) + ';' + std::to_string(int(s.show.tab)) +
                      ',' + std::to_string(s.show.scroll) + ',' + std::to_string(s.show.hoverTab) + ',' +
                      std::to_string(s.show.rights) +
                      (s.show.hoverClose ? "x" : "") + ';' + std::to_string(s.layout.x) + ',' +
                      std::to_string(s.layout.y) + ',' + std::to_string(s.layout.wide) + '#' +
                      std::to_string(uistyle::generation()) + '#' + s.emblemFor +
                      (s.emblem.width > 0 ? "e" : "");  // 3.0.19-beta.1: its size and its emblem too
    for (const gc::Member& m : s.members)
        key += ';' + m.name + ',' + m.title + ',' + std::to_string(m.level) + (m.online ? "o" : "");
    key += '|' + s.more.head + '|' + s.more.subject + '|' + s.more.body + '|' + std::to_string(s.show.hoverPlus) +
           (s.show.hoverEdit ? "e" : "") + (s.show.canEdit ? "c" : "");
    if (s.show.editing)  // .271
        key += "|E" + std::to_string(s.show.field) + (s.show.caret ? "|" : "") + std::to_string(s.show.hoverField) +
               (s.show.hoverCancel ? "x" : "") + '|' + s.show.subject + '|' + s.show.body;
    for (const gc::Line& l : s.more.lines) key += ';' + l.left + ',' + l.middle + ',' + l.right + (l.plus ? "+" : "");
    if (const std::string drawn = key + (uaui::ready() ? "u" : ""); drawn != s.key) {  // 3.0.18-beta.34
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
                                    gl::fill(m, s.layout, s.info, s.members, s.more, s.show, texts(),
                                             s.emblem.width > 0 ? s.live.icon(&s.emblem) : std::string());
                                });
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    s.live.raise();  // 3.0.18-beta.43: at its place in the windows' stack
    if (!s.shown || s.k <= 0 || ((s.picture.width <= 0 || s.picture.height <= 0) && !s.sheetShown)) return;
    int cursor = -1, cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) cursor = panel::cursorDraw(record, float(cx), float(cy));
    const panel::Box& g = s.layout.glass;
    // 3.0.18-beta.43: the shadow the driver's, under uaUI's window too
    s.shadow.draw(record, renderer, s.layout.x + int(g.x), s.layout.y + int(g.y), int(g.w), int(g.h),
                  12 * s.scale, s.scale, s.k);
    if (cursor >= 0) record.moveLastBefore(record.commands().size() - size_t(cursor) - 1, size_t(cursor));
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    if (m == WM_LBUTTONUP) s.move.end();  // 3.0.18-beta.92
    if (!s.shown) return false;
    const bool button = mouseButton(m);
    if (!button && m != WM_MOUSEWHEEL) return false;
    float tx = 0, ty = 0;
    if (!pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty, nullptr)) return false;
    const float lx = tx - float(s.layout.x), ly = ty - float(s.layout.y);
    if (!s.layout.glass.contains(lx, ly)) return false;
    result = 0;
    if (m == WM_MOUSEWHEEL) {
        const int by = GET_WHEEL_DELTA_WPARAM(wp) > 0 ? -1 : 1;
        const size_t rows = s.show.tab == gc::Tab::Members ? s.members.size() : s.more.lines.size();
        s.show.scroll = std::clamp(s.show.scroll + by, 0, gc::mostScroll(s.layout, rows));
        s.key.clear();
        return true;
    }
    if (m != WM_LBUTTONDOWN && m != WM_LBUTTONDBLCLK) return true;
    const gc::Hit h = gc::hitTest(s.layout, lx, ly);
    // 3.0.18-beta.92; .102: its body (nothing pressable there) - the hit test never says None inside the card
    if ((h.part == gc::Part::None || h.part == gc::Part::Body) && m == WM_LBUTTONDOWN) s.move.begin(tx, ty);
    if (h.part == gc::Part::Tab) {
        s.show.scroll = 0;
        switchTab(s, h.index);  // 3.0.11-beta.210: the game's own tab with it (the card follows it)
    } else if (h.part == gc::Part::Close) {
        closeIt(s);
    } else if (h.part == gc::Part::Plus) {
        raiseSkill(s, h.index + s.show.scroll);
    } else if (h.part == gc::Part::Edit && s.show.canEdit) {
        if (s.show.tab != gc::Tab::Notice) editInGame(s);  // the positions: the game's own window still
        else if (s.show.editing) saveNotice(s);           // .271: the notice on the card
        else startNotice(s);
    } else if (h.part == gc::Part::Cancel && s.show.editing) {
        s.show.editing = false;
        s.key.clear();
    } else if (h.part == gc::Part::Field && s.show.editing) {
        s.show.field = h.index;
        s.key.clear();
    }
    return true;
}

bool shown() { return state().shown; }

bool close() {
    State& s = state();
    if (!s.shown) return false;
    closeIt(s);
    return true;
}

bool key(UINT m, WPARAM wp, LPARAM lp) {  // .271: the notice's fields take the keys while it is edited
    State& s = state();
    (void)lp;
    if (!s.shown || !s.show.editing) return false;
    std::string& f = s.show.field ? s.show.body : s.show.subject;
    if (m == WM_KEYDOWN || m == WM_SYSKEYDOWN) {
        if (GetKeyState(VK_MENU) < 0) return false;
        switch (wp) {
            case VK_ESCAPE: s.show.editing = false; break;
            case VK_RETURN: saveNotice(s); break;
            case VK_TAB: s.show.field = 1 - s.show.field; break;
            case VK_BACK: {  // a whole letter off (UTF-8)
                while (!f.empty() && (static_cast<unsigned char>(f.back()) & 0xC0) == 0x80) f.pop_back();
                if (!f.empty()) f.pop_back();
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
                const size_t most = s.show.field ? gc::kBodyMost : gc::kSubjectMost;
                if (gameBytes(f + add, s.codePage, most + 1).size() <= most) f += add;
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
    switch (c) {
        case PadCommand::Up: s.show.scroll = std::max(0, s.show.scroll - 1); break;
        case PadCommand::Down: {
            const size_t rows = s.show.tab == gc::Tab::Members ? s.members.size() : s.more.lines.size();
            s.show.scroll = std::min(gc::mostScroll(s.layout, rows), s.show.scroll + 1);
            break;
        }
        case PadCommand::TabBack:
        case PadCommand::TabOn: {  // 3.0.11-beta.210: the next allowed tab, as the game's own
            const int by = c == PadCommand::TabOn ? 1 : gc::kTabs - 1;
            int next = int(s.show.tab);
            for (int n = 0; n < gc::kTabs; ++n) {
                next = (next + by) % gc::kTabs;
                if (gc::allowed(next, s.show.rights)) break;
            }
            s.show.scroll = 0;
            switchTab(s, next);
            break;
        }
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
    if (guildtabs::read(s.tabs, data, size)) {
        if (s.told++ < 30) LOG_INFO("guild: the server's 0x%04X taken for the card's tabs", unsigned(id));
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

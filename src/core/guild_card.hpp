#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "core/guild_skills.hpp"
#include "core/guild_tabs.hpp"
#include "core/window_panel.hpp"

namespace uagfx::guildcard {

// 3.0.11-beta.165 (task 6 of the remaining windows: the guild, Alt+G): the game's guild window (each of its tabs a
// UI window of its own, 0x3C-0x41) drawn by the driver; ✕ the game's close (6 / 0x87). 3.0.22-beta.26
// (docs/superpowers/plans/2026-11-19-guild-window.md - the tester: «а ГІ можливо також картинки скілів»; the owner:
// «все делай»): rebuilt as «Навички» is - a head, six tabs underlined, a list with a side panel, a foot. The parts that
// need no Windows: the tabs' views (what each shows, from the game's guild object, its members' list and the server's
// packets), the window's size before RmlUi lays it out, the hit test over RmlUi's boxes.
constexpr int kWindows = 6, kCloseButton = 0x87;
constexpr uint32_t kFirstAt = 0x170;  // the manager's place of the first of them (4 apart)
enum class Tab : uint8_t { Info, Members, Positions, Skills, Expelled, Notice };
// 3.0.11-beta.210: a click switches the game's own tab (its message 0x45; the game checks the rights, 0x0168 bits)
constexpr int kTabs = 6, kTabMsg = 0x45;
// The game's own window shows only while «Змінити» edits the positions; 3.0.22-beta.27: or the members (positions
// and expelling, the master's)
bool gameDrawn(int tab, bool editing);
bool allowed(int tab, uint32_t rights);  // members 1, positions 2, expelled 0x10, the notice 0x80
// 3.0.22-beta.26: the master's rights (0xD7) hold the notice's 0x80, a member's (0x57) do not
constexpr uint32_t kMasterRight = 0x80;
inline bool isMaster(uint32_t rights) { return (rights & kMasterRight) != 0; }

struct Info {
    std::string name, master, territory;  // UTF-8
    int level = 0, members = 0, most = 0, average = 0, tax = 0;
    long long exp = 0, nextExp = 0;
};
struct Member {  // as the game keeps them (its list: +0 the account, +4 the character)
    std::string name, title, job;  // UTF-8
    int level = 0;
    bool online = false;
    uint32_t account = 0, character = 0;
};
// .271: the server's notice: its subject up to 59 bytes, its body up to 119 (the game's code page)
constexpr size_t kSubjectMost = 59, kBodyMost = 119;
// The castles in the server's words (msg 300 + N: «None Taken», «Two Castles», «Total Domination» - all 24); -1 other
int castles(const std::string& territory);

// The window's words (strings.cpp's, uk/ru/en; «{1}» filled)
struct Texts {
    std::string title, hotkey, tabs[kTabs], sub, none;
    // «Інформація»
    std::string infoSub, expLine, master, average, castles, yourTax, yourTaxValue, allies, enemies, notice, gifts;
    std::string nearMaster, warOnly, online, offline, allMembers;
    // «Учасники»
    std::string search, count, colName, colJob, colLevel, colPosition, colGiven, inGame, away, position, given;
    std::string givenValue, whisper, party, friendly, setPosition, expel, rightsNote, membersHints;
    // «Посади»
    std::string colRights, colTax, colPeople, invite, expelRight, noRights, folded, edit, editNote, peopleOn;
    // «Навички»
    std::string groups[guildskills::kGroups], groupsMore[guildskills::kGroups], kinds[3], sp, lines[14], about[14];
    std::string levelsLabel, youLabel, youNear, whereLabel, coolLabel, needsLabel, whereAura, whereWar, coolShared;
    std::string ready, wait, use, raiseTo, places, need, points, memberFoot, skillHints, whoRaises, whoRaisesText;
    std::string coolRow, levelOf;  // «перезарядка, ще {1}», «рівень {1} з {2}»
    // «Вигнані», «Оголошення»
    std::string expelledHead, account, reason, fieldTitle, fieldBody, save, cancel, editHint, counter, preview;
    std::string previewNote, noticeEmpty;
    // the controller's words
    std::string padTabs, padPick, padUse, padRaise, padAssign, padClose, padActions;
    // 3.0.22-beta.27: no guild
    std::string noGuild, noGuildSub, join, joinText, make, makeText;
};

enum class Sort : uint8_t { Online, Level, Position, Given };
enum class Part : uint8_t {
    None, Body, Title, Close, Tab, Skill, Plus, Use, Raise, Member, Head, Search, Action, Position, Edit, Expelled,
    Field, Save, Cancel, AllMembers,
    MembersBar  // 3.0.23-beta.15: the members' scroll bar (pressed and dragged)
};
struct Hit {
    Part part = Part::None;
    int index = -1;
};
// What the player changed and what is under the mouse
struct Show {
    Tab tab = Tab::Info;
    uint32_t rights = 0xFF;
    bool pad = false, sony = false;
    int skill = 10000;         // the chosen skill (its id)
    uint32_t member = 0;       // the chosen member (its character), 0 the first
    int position = -1, expelled = 0;
    int first[kTabs] = {};     // each tab's first row shown
    Hit hover;
    bool editing = false;      // .271: the notice edited - its fields (UTF-8), the one typed into, the caret
    std::string subject, body;
    int field = 0;
    bool caret = true;
    std::string search;        // «Учасники»: the search, typed into while `typing`
    bool typing = false;
    Sort sort = Sort::Online;
    int coolMs = 0;            // the four war skills' shared cooldown left
    std::string me;            // the player's name (UTF-8)
};

// At scale 1: 760 wide, the body 642 high; a member's row 28 (20 shown), a position's 34, an expelled one's 30
constexpr float kWide = 760, kBodyH = 642;
constexpr int kMemberRows = 20, kPositionRows = 16, kExpelledRows = 18;
struct HitBox {
    Part part = Part::None;
    int index = -1;
    panel::Box box;
};
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;
    panel::Box glass;
    std::vector<HitBox> hits;  // RmlUi's boxes of every pressable part (uikit guild's refit)
};
Layout layout(int targetW, int targetH, float scale);  // its size before RmlUi lays it out, placed right of the middle
// 3.0.23-beta.15 (a tester: «окно огромне і навіть кінця низу немає»): `scale` made smaller where the window would be
// taller (or wider) than the screen (a margin kept) - its bottom and its corner on the screen
float fitScale(int targetW, int targetH, float scale);
// The members' scroll: the thumb's top (percent of the bar) for the list from `first` of `total` with `rows` shown and
// the thumb `thumbH` percent tall; the first row for a press at `frac` (0-1) of the bar
float thumbTop(int first, int total, int rows, float thumbH);
int firstAt(float frac, int total, int rows);
Hit hitTest(const Layout& L, float x, float y);        // the smallest box under (x, y); Body inside it, None outside

// ---- the tabs' views (what uikit guild shows)
struct FootV {
    std::string left;
    bool gold = false;
    std::vector<std::string> hints;              // the mouse's
    std::vector<std::pair<int, std::string>> pad;  // a controller's: padglyph::Button, its words
};
struct SkillRowV {
    int id = 0;
    std::string group, groupMore;  // over the first of a group
    std::string aegis, name, kind, info, level;
    bool active = false, locked = false, learned = false, focus = false, hot = false, plus = false, plusHot = false;
    std::vector<int> dots;  // 1 learned, 0 not (none past 5)
};
struct LevelV {
    std::string n, what;
    bool now = false;
};
struct NeedV {
    std::string what;
    bool met = false;
};
struct SkillSideV {
    int id = 0;
    std::string aegis, name, kind, about, you, where, cool, wait, raiseWord, useWord, who;
    bool active = false, ready = false, use = false, useOff = false, raise = false, useHot = false, raiseHot = false;
    std::vector<LevelV> levels;
    std::vector<NeedV> needs;
    // its labels: «Вам зараз», «Рівні», «Де діє», «Перезарядка», «Потрібно», «Хто піднімає»
    std::string youLabel, levelsLabel, whereLabel, coolLabel, needsLabel, whoLabel;
};
struct SkillsView {
    std::vector<SkillRowV> rows;
    SkillSideV side;
};
SkillsView skillsView(const std::vector<guildskills::Row>& rows, const Show& sh, const Texts& t, bool master);

struct FactV {
    std::string label, value;
    bool dot = false;
};
struct GiftV {
    std::string aegis, what, from;
};
struct PersonV {
    std::string name, job;
};
struct InfoView {
    std::string name, sub, expLine;
    std::string alliesLabel, enemiesLabel, noticeLabel, giftsLabel, allWord, none;
    float expShare = 0;  // 0..1
    std::vector<FactV> facts;
    std::vector<std::string> allies, enemies;
    bool alliances = false;  // the server's list seen
    std::string subject, body;
    std::vector<GiftV> gifts;
    std::string onlineHead, offline;
    std::vector<PersonV> online;
};
InfoView infoView(const Info& g, const std::vector<Member>& members, const guildtabs::Data& d,
                  const std::vector<guildskills::Row>& skills, const Show& sh, const Texts& t);

struct MemberRowV {
    uint32_t character = 0;
    std::string name, job, level, position, given;
    bool online = false, focus = false, hot = false, me = false;
};
struct HeadV {
    std::string name;
    bool on = false, hot = false;
};
// A member's actions on its card (the hit's index): «Написати», «Запросити в групу», «У друзі», the master's
// «Посада» and «Вигнати…»
enum class Act : uint8_t { Whisper, Party, Friend, Position, Expel };
constexpr int kActs = 5;
constexpr uint32_t kAllActs = 0x1F;
struct CardV {
    bool shown = false, online = false, master = false;
    std::string name, sub, position, given;
    std::vector<std::pair<int, std::string>> actions;  // Act, its words
    int hotAction = -1;
};
struct MembersView {
    std::string count, searchWord, placeholder, positionLabel, givenLabel, rightsNote;
    std::vector<HeadV> heads;  // the columns: name, class, level, position, given (a click sorts)
    std::vector<MemberRowV> rows;
    int total = 0;  // the list's length (searched), for the scroll
    int first = 0;  // 3.0.23-beta.15: the first row shown (the thumb's place)
    CardV card;
};
// The members in the list's order (sorted, searched): their indices into `members`
std::vector<size_t> memberOrder(const std::vector<Member>& members, const guildtabs::Data& d, const Show& sh);
// `acts`: the actions the window can do now (1 << Act)
MembersView membersView(const Info& g, const std::vector<Member>& members, const guildtabs::Data& d, const Show& sh,
                        const Texts& t, bool master, uint32_t acts = kAllActs);

struct PositionRowV {
    int id = 0;
    std::string name, tax, people;
    std::vector<std::string> rights;
    bool focus = false, hot = false;
};
struct PositionsView {
    std::vector<HeadV> heads;  // «Посада», «Права», «Податок», «Людей»
    std::vector<PositionRowV> rows;
    std::string folded;
    std::string sideName, sideRights, sideTax, sidePeople, editWord, editNote, noRights, rightsLabel, taxLabel;
    bool canEdit = false, editHot = false;
};
PositionsView positionsView(const guildtabs::Data& d, const Show& sh, const Texts& t);

struct ExpelledRowV {
    std::string name, reason;
    bool focus = false, hot = false;
};
struct ExpelledView {
    std::string head, accountLabel, reasonLabel, nameLabel, none;
    std::vector<ExpelledRowV> rows;
    std::string sideName, sideAccount, sideReason;
};
ExpelledView expelledView(const guildtabs::Data& d, const Show& sh, const Texts& t);

struct NoticeView {
    std::string subject, body, subjectCount, bodyCount;  // the fields' words while edited; the notice's otherwise
    std::string fieldTitle, fieldBody, hint, previewLabel, previewNote, saveWord, cancelWord, editWord, label, none;
    bool editing = false, canEdit = false, empty = false;
    int field = 0;
    bool caret = false, saveHot = false, cancelHot = false, editHot = false;
};
NoticeView noticeView(const guildtabs::Data& d, const Show& sh, const Texts& t, size_t subjectBytes, size_t bodyBytes);

FootV footOf(const Show& sh, const Texts& t, int points, bool master);
std::string fmt(std::string pattern, const std::vector<std::string>& values);  // {1}, {2}... filled

// 3.0.19-beta.1 (the testers: «не видно значок ГІ твоеї»): the guild's emblem from the game's own file
// (<client>\_tmpEmblem\<server>_<guild id>_<emblem id>.ebm, written by uaRO.exe's FUN_0063aab0 from the packet
// 0x0152: the BMP zlib-packed as the server keeps it; a plain BMP taken too) - side x side ARGB, its magenta
// (255, 0, 255, the game's key) see-through, made bigger pixel for pixel; empty when it is not one
std::vector<uint32_t> emblemPixels(const std::string& file, int side);

}  // namespace uagfx::guildcard

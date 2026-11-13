#include "win/info_card.hpp"

#include "core/item_elements.hpp"
#include "core/vend_shop.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <map>
#include <string>
#include <vector>

#include "core/char_block.hpp"
#include "core/game_words.hpp"
#include "core/hotbar_panel.hpp"
#include "core/info_card.hpp"
#include "core/settings/strings.hpp"
#include "core/skill_costs.hpp"
#include "core/skill_info.hpp"
#include "win/card_drag.hpp"
#include "core/skill_tree.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "win/client_tables.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/game_memory.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/reference_window.hpp"
#include "win/ui_cost.hpp"
#include "win/uaui.hpp"
#include "win/ui_sheet.hpp"
#include "win/window_input.hpp"
#include "win/window_shadow.hpp"
#include "win/words.hpp"

namespace uagfx::win::infocardwin {
static bool g_elementColours = true;  // 3.0.18-beta.144: [interface] element_colours
namespace {

namespace ic = uagfx::infocard;
using uapad::gamewindow::Blocker;

// uaRO.exe's: the windows' manager's item card (UI window 0xC) and skill card (0x2E) - where each is kept, its class's
// vtable and its SendMsg (message 6 with 0x87: its own close - FUN_004d1e60 deletes it)
struct GameCard {
    uint32_t at, cls, send;
};
constexpr uint32_t kWindowMgr = 0x727ED8;
constexpr GameCard kGame[2] = {{kWindowMgr + 0xC8, 0x698D88, 0x478300}, {kWindowMgr + 0xD4, 0x698EB8, 0x479910}};
constexpr uint32_t kShown = 0x38, kSendMsgSlot = 0x94;
constexpr int kButtonMsg = 6, kCloseButton = 0x87;
// The item card's ITEM_INFO at +0x60: its count +0x10, its slots' four cards +0x1C, its number's words (an MSVC 6
// std::string: its text +0x30, its length +0x34), identified +0x3C, refined +0x40. The skill card's skill: its name
// as shown (FUN_00505d60's, «Coin Flip» - 3.0.11-beta.200: not its AEGIS name) - its text +0x78, its length +0x7C
constexpr uint32_t kInfo = 0x60, kCount = kInfo + 0x10, kSlots = kInfo + 0x1C, kNameText = kInfo + 0x30;
constexpr uint32_t kNameLen = kInfo + 0x34, kKnown = kInfo + 0x3C, kRefine = kInfo + 0x40;
constexpr uint32_t kAegisText = 0x78, kAegisLen = 0x7C;
// The session's skills (a list: a node's next +0, its level +0x14, its AEGIS name's char* +0x24)
constexpr uint32_t kSkills = 0x771148 + 0x2D8, kSkillLevel = 0x14, kSkillName = 0x24;
constexpr size_t kMostNodes = 1024;

struct Tables {
    bool read = false;
    std::map<std::string, std::vector<std::string>> items, unknown, skills;
    std::map<int, int> slots;
    std::map<std::string, std::string> unknownFiles;  // an unidentified item's picture's file
    std::map<std::string, std::string> cardArt;  // 3.0.18-beta.110: a card's illustration's file
};

struct One {
    uisheet::Sheet sheet;  // 3.0.18-beta.31: the card drawn by uaUI
    int sheetGen = 0;
    bool sheetShown = false;
    WindowShadow shadow;
    bool shown = false, hid = false;
    uint32_t window = 0;
    std::string what;  // the thing shown (its number, identified, refined, cards, count; a skill's AEGIS name)
    int itemId = 0;    // 3.0.18-beta.242: the item shown (a slot's card's too), 0 a skill - «У довіднику ›»
    ic::Card card;
    ic::Show show;
    ic::Layout layout;
    int atX = 0, atY = 0;  // its place in the target, where the cursor was when it opened
    CardDrag move;  // 3.0.18-beta.93 (the owner: «описание вещи ... не двигается»): moved by its title or body
    // 3.0.18-beta.109 (the owner: «как было в оригинале»): a slot's card opened in its place (its slot; -1 the thing),
    // the slots' cards' numbers
    int viewing = -1;
    int cardIds[4] = {};
    std::string thing;  // the thing the card shows (its viewing aside)
    std::string key;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    std::string bigOf;  // the big picture's file and size as fitted
    panel::Picture big;
};

struct State {
    bool on = false;
    unsigned codePage = 0;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    One cards[3];  // the item card, the skill card, 3.0.18-beta.139 a slot's card
    Tables tables;
    bool slotOpen = false;  // 3.0.18-beta.139: the slot's card's window
    int slotCard = 0;
    int told = 0;
};

State& state() {
    static State s;
    return s;
}

std::string text(uint32_t at, uint32_t len) {
    if (!at || !len || len > 64) return "";
    std::string out(len, '\0');
    return readGame(at, out.data(), len) ? out : "";
}

std::string utf8(const State& s, const std::string& game) { return utf8Of(widened(game, s.codePage)); }

const char* nameOf(Kind k) {
    return k == Kind::Item ? "the item card" : k == Kind::Skill ? "the skill card" : "a slot's card";
}

uint32_t gameCard(int i) {
    const uint32_t w = read32(kGame[i].at);
    return w && read32(w) == kGame[i].cls && read32(read32(w) + kSendMsgSlot) == kGame[i].send ? w : 0;
}

void hold(One& o, uint32_t window, bool on, Kind kind) {
    if (!window) {
        o.hid = false;
        return;
    }
    if (on) {
        if (read32(window + kShown) && write32(window + kShown, 0) && !o.hid) {
            LOG_INFO("cards: %s of the game hidden for the new one (its shown flag 0)", nameOf(kind));
            o.hid = true;
        }
    } else if (o.hid) {
        write32(window + kShown, 1);
        o.hid = false;
    }
}

// The game's tables, once: in the folder it reads its language's from (data\english\ for most), else data\'s own
void readTables(State& s) {
    Tables& t = s.tables;
    if (t.read) return;
    t.read = true;
    const std::string folder = "data\\" + ic::languageFolder(unsigned(GetUserDefaultLangID()));
    const auto first = [](std::initializer_list<std::string> names, std::string& from) {
        std::string body;
        for (const std::string& n : names)
            if (clienttables::readClientFile(n, body) && !body.empty()) {
                from = n;
                return body;
            }
        from = "none";
        return std::string();
    };
    std::string a, b, c, d, e;
    t.items = ic::descriptions(first({folder + "idnum2itemdesctable.txt", "data\\idnum2itemdesctable.txt"}, a));
    t.unknown = ic::descriptions(first({folder + "num2itemdesctable.txt", "data\\num2itemdesctable.txt"}, b));
    t.skills = ic::descriptions(first({folder + "skilldesctable2.txt", folder + "skilldesctable.txt",
                                       "data\\skilldesctable2.txt", "data\\skilldesctable.txt"},
                                      c));
    t.slots = ic::slotCounts(first({folder + "itemslotcounttable.txt", "data\\itemslotcounttable.txt"}, d));
    t.unknownFiles = hotbar::nameTable(first({"data\\num2itemresnametable.txt"}, e));
    std::string f;  // 3.0.18-beta.110: the cards' illustrations
    t.cardArt = hotbar::nameTable(first({"data\\num2cardillustnametable.txt"}, f));
    LOG_INFO("cards: the cards' illustrations %zu (%s)", t.cardArt.size(), f.c_str());
    LOG_INFO("cards: the game's tables - item descriptions %zu (%s), unidentified %zu (%s), skills %zu (%s), slots "
             "%zu (%s), unidentified pictures %zu",
             t.items.size(), a.c_str(), t.unknown.size(), b.c_str(), t.skills.size(), c.c_str(), t.slots.size(),
             d.c_str(), t.unknownFiles.size());
}

// 3.0.11-beta.270: the character's job (the session's, as the character block reads it) and a job's English name
int ownJob() {
    uint32_t v = 0;
    return readGame(0x771F8C, &v, 4) ? int(v) : -1;
}

std::string jobWords(int job) { return charblock::jobName(job); }

std::vector<std::string> utf8Lines(const State& s, const std::vector<std::string>& lines) {
    std::vector<std::string> out;
    for (const std::string& l : lines) out.push_back(utf8(s, l));
    return out;
}

// An item's big picture fitted into the card's room, kept while its file and the scale are the same
// 3.0.18-beta.110: a card's illustration fitted into its room (the card's own window, as the original)
const panel::Picture* cardArt(State& s, One& o, const std::string& file) {
    int rw = 0, rh = 0;
    ic::artRoom(s.scale, rw, rh);
    const std::string key = "art:" + file + '@' + std::to_string(rw) + 'x' + std::to_string(rh);
    if (key != o.bigOf) {
        o.bigOf = key;
        std::vector<uint32_t> argb;
        int w = 0, h = 0;
        o.big = clienttables::cardArtPixels(file, argb, w, h) ? ic::fitted(argb, w, h, rw, rh) : panel::Picture{};
    }
    return o.big.width > 0 ? &o.big : nullptr;
}

const panel::Picture* bigPicture(State& s, One& o, const std::string& file) {
    int rw = 0, rh = 0;
    ic::pictureRoom(s.scale, rw, rh);
    const std::string key = file + '@' + std::to_string(rw) + 'x' + std::to_string(rh);
    if (key != o.bigOf) {
        o.bigOf = key;
        std::vector<uint32_t> argb;
        int w = 0, h = 0;
        o.big = clienttables::collectionPixels(file, argb, w, h) ? ic::fitted(argb, w, h, rw, rh) : panel::Picture{};
    }
    return o.big.width > 0 ? &o.big : nullptr;
}

std::string itemName(const State& s, const std::string& number, bool known) {
    const auto& names = known ? clienttables::itemNames() : clienttables::unknownNames();
    const auto n = names.find(number);
    return n != names.end() ? utf8(s, hotbar::spaced(n->second)) : "#" + number;
}

// 3.0.18-beta.142: a forged thing's maker by the character's id, as the game names it (FUN_004fef70: the session's
// names, 0x74B578 +4 while +0x58 is 1, FUN_00560c30 - it asks the server once for one not known); asked again at
// most each 5 s, a name known kept
std::string makerName(int cid, unsigned codePage) {
    using NameFn = uint32_t(__attribute__((thiscall)) *)(uint32_t self, uint32_t cid);
    constexpr uint32_t kNames = 0x74B578, kNameOf = 0x560C30;
    static std::map<int, std::string> known;
    static std::map<int, ULONGLONG> asked;
    if (cid <= 0) return "";
    if (const auto k = known.find(cid); k != known.end()) return k->second;
    const ULONGLONG now = GetTickCount64();
    if (const auto a = asked.find(cid); a != asked.end() && now - a->second < 5000) return "";
    asked[cid] = now;
    const uint32_t self = read32(kNames + 0x58) == 1 ? read32(kNames + 4) : 0;
    if (!self) return "";
    const uint32_t entry = reinterpret_cast<NameFn>(uintptr_t(kNameOf))(self, uint32_t(cid));
    const uint32_t text = entry ? read32(entry + 4) : 0;
    char buf[32] = {};
    if (!text || !readGame(text, buf, sizeof buf - 1) || !buf[0]) return "";
    const std::string name = utf8Of(widened(buf, codePage));
    known[cid] = name;
    LOG_INFO("cards: a forged thing's maker #%d - %s", cid, name.c_str());
    return name;
}

std::string forgedLine(const int cards[4], unsigned codePage) {
    const std::string maker = makerName(cardnames::makerOf(cards), codePage);
    if (maker.empty()) return say(settings::Text::ThingForged);
    return settings::fmt(say(settings::Text::ThingForgedBy), {maker});
}

// The item card's thing: its title (+refined, [slots]), its count or «Не опізнано», its description, its slots' cards
void itemCard(State& s, One& o, uint32_t window) {
    const std::string number = text(read32(window + kNameText), read32(window + kNameLen));
    o.itemId = std::atoi(number.c_str());  // 3.0.18-beta.242
    uint8_t known = 0;
    readGame(window + kKnown, &known, 1);
    const int refine = int(read32(window + kRefine)), count = int(read32(window + kCount));
    int cards[4];
    for (int i = 0; i < 4; ++i) cards[i] = int(read32(window + kSlots + 4 * uint32_t(i)));
    std::string what = number + '|' + std::to_string(int(known)) + '|' + std::to_string(refine) + '|' +
                       std::to_string(count);
    for (const int c : cards) what += '|' + std::to_string(c);
    what += '@' + std::to_string(s.scale);
    if (what != o.thing) o.viewing = -1, o.thing = what;  // another thing: its own words again
    what += '#' + std::to_string(o.viewing);
    if (what == o.what) return;
    const bool fresh = o.what.rfind(number + '|', 0) != 0;
    o.what = what;
    if (fresh) o.show.scroll = 0;
    for (int i = 0; i < 4; ++i) o.cardIds[i] = cards[i];
    ic::Card c;
    const int id = std::atoi(number.c_str());
    const auto slots = s.tables.slots.find(id);
    const int slotCount = known && slots != s.tables.slots.end() ? std::clamp(slots->second, 0, 4) : 0;
    // 3.0.18-beta.112: its cards' words in its name as the game names it («Saharic Jur», «Clip of Counter»)
    const std::string base = itemName(s, number, known != 0);
    c.title = (known && refine > 0 ? "+" + std::to_string(refine) + " " : "") +
              (known ? cardnames::named(base, cards, clienttables::cardWords()) : base) +
              (slotCount > 0 ? " [" + std::to_string(slotCount) + "]" : "");
    c.note = count > 1 ? "\xC3\x97" + std::to_string(count) : known ? "" : say(settings::Text::CardUnknown);
    if (known && cardnames::forged(cards) && c.note.empty()) c.note = forgedLine(cards, s.codePage);  // .142
    const auto& table = known ? s.tables.items : s.tables.unknown;
    const auto d = table.find(number);
    if (d != table.end()) c.lines = utf8Lines(s, d->second);
    // 3.0.18-beta.146 (the owner: as the later clients - «Property: Wind» in a thing's words): its element first
    if (known && g_elementColours)
        if (const int e = itemelements::ofThing(id, cards))
            c.lines.insert(c.lines.begin(), std::string("Property: ") + itemelements::word(e));
    // a forged or a named thing keeps its maker in the cards' places (255, 254, 0xFF00): no cards shown then
    const bool special = cards[0] == 0xFF || cards[0] == 0xFE || cards[0] == 0xFF00 || cards[0] < 0;
    if (known && !special)
        for (int i = 0; i < std::max(slotCount, 0); ++i)
            c.cards.push_back(cards[i] > 0 ? itemName(s, std::to_string(cards[i]), true) : "");
    const auto& files = known ? clienttables::itemFiles() : s.tables.unknownFiles;
    const auto f = files.find(number);
    if (f != files.end()) {
        c.icon = clienttables::icon(f->second, ic::iconSide(s.scale), 4 * s.scale);
        if (known) c.picture = bigPicture(s, o, f->second);
    }
    // 3.0.18-beta.109: each slot's card picture (its item icon)
    const int cardSide = int(std::lround(26 * s.scale));
    for (size_t i = 0; i < c.cards.size(); ++i) {
        const panel::Picture* pic = nullptr;
        if (cards[i] > 0) {
            const auto cf = clienttables::itemFiles().find(std::to_string(cards[i]));
            if (cf != clienttables::itemFiles().end()) pic = clienttables::icon(cf->second, cardSide, 2 * s.scale);
        }
        c.cardIcons.push_back(pic);
    }
    o.card = c;
    if (s.told++ < 40)
        LOG_INFO("cards: the item card shows %s (#%s%s, refined %d, %d slot(s), %zu description line(s)%s)",
                 c.title.c_str(), number.c_str(), known ? "" : ", not identified", refine, slotCount, c.lines.size(),
                 c.picture ? ", its big picture" : "");
}

// 3.0.18-beta.109/.139: a card's own card - its name, words, its illustration big over them (3.0.18-beta.110, the
// owner: «окно карты как в оригинале»), else its item picture
void cardCard(State& s, One& o, int id) {
    o.itemId = id;  // 3.0.18-beta.242
    const std::string card = std::to_string(id);
    const std::string what = "card " + card + '@' + std::to_string(s.scale);
    if (what == o.what) return;
    o.what = what;
    o.show.scroll = 0;
    ic::Card v;
    v.title = itemName(s, card, true);
    const auto cd = s.tables.items.find(card);
    if (cd != s.tables.items.end()) v.lines = utf8Lines(s, cd->second);
    const auto cf = clienttables::itemFiles().find(card);
    if (cf != clienttables::itemFiles().end())
        v.icon = clienttables::icon(cf->second, ic::iconSide(s.scale), 4 * s.scale);
    const auto art = s.tables.cardArt.find(card);
    if (art != s.tables.cardArt.end()) v.picture = cardArt(s, o, art->second);
    v.art = v.picture != nullptr;
    if (!v.picture && cf != clienttables::itemFiles().end()) v.picture = bigPicture(s, o, cf->second);
    o.card = v;
    LOG_INFO("cards: a slot's card shows %s (#%d) in its own window", v.title.c_str(), id);
}

// The character's skills: their AEGIS names and levels (the session's list, as «Навички» reads it)
std::vector<ic::LearnedSkill> sessionSkills() {
    std::vector<ic::LearnedSkill> out;
    const uint32_t head = read32(kSkills);
    if (!head) return out;
    uint32_t node = read32(head);
    for (size_t n = 0; node && node != head && n < kMostNodes; ++n, node = read32(node)) {
        char name[48] = {};
        const uint32_t at = read32(node + kSkillName);
        if (!at || !readGame(at, name, sizeof name - 1)) continue;
        name[sizeof name - 1] = 0;
        out.push_back(ic::LearnedSkill{name, int(read32(node + kSkillLevel))});
    }
    return out;
}

// 3.0.11-beta.229 (R-EQDQWH: «на некоторых скилах нету описания» - Parry has none in the game's table): what the
// driver knows of a skill instead - what it is used on, its levels, SP by level, what it needs (the server's tables)
std::vector<std::string> skillFacts(State& s, const std::string& aegis) {
    std::vector<std::string> out;
    int id = 0;
    for (int i = 1; i < 0x3FD && !id; ++i)
        if (clienttables::skillAegis(i) == aegis) id = i;
    if (!id) return out;
    const std::vector<int> jobs = skilltree::chain(int(read32(0x771148 + 0xE44)));
    const int job = jobs.empty() ? 0 : jobs.back();
    out.push_back(say(settings::Text::CardNoDescription));
    const int type = skillinfo::type(id);
    if (type >= 0) {
        const settings::Text t = type == 0           ? settings::Text::SkillsPassive
                                 : (type & 32) != 0 ? settings::Text::SkillsTrap
                                 : (type & 16) != 0 ? settings::Text::SkillsAlly
                                 : (type & 2) != 0  ? settings::Text::SkillsGround
                                 : (type & 4) != 0  ? settings::Text::SkillsSelf
                                                    : settings::Text::SkillsEnemy;
        out.push_back(say(t));
    }
    const int most = skilltree::maxLevel(job, id);
    if (most > 0) out.push_back(settings::fmt(say(settings::Text::CardMostLevel), {std::to_string(most)}));
    std::string sp;
    for (int lv = 1; lv <= std::max(1, most) && lv <= 10; ++lv) {
        const int v = skillcost::sp(id, lv);
        if (v < 0) break;
        sp += (sp.empty() ? "" : " / ") + std::to_string(v);
    }
    if (!sp.empty()) out.push_back(settings::fmt(say(settings::Text::SkillsSp), {sp}));
    std::string needs;
    for (const auto& [need, lv] : skilltree::needs(job, id)) {
        const std::string a = clienttables::skillAegis(need);
        const auto& names = clienttables::skillNames();
        const auto n = names.find(a);
        const char* server = skillinfo::name(need);
        const std::string named = n != names.end() ? utf8(s, hotbar::spaced(n->second))
                                  : server && *server ? std::string(server)
                                                      : "#" + std::to_string(need);
        needs += (needs.empty() ? "" : ", ") + named + " " + std::to_string(lv);
    }
    if (!needs.empty()) out.push_back(settings::fmt(say(settings::Text::SkillsNeeds), {needs}));
    return out;
}

void skillCard(State& s, One& o, uint32_t window) {
    o.itemId = 0;  // 3.0.18-beta.242: a skill - no item card in «Довідник»
    // 3.0.11-beta.200: the game's card keeps the skill's name as shown (FUN_00505d60's) - its AEGIS name and level
    // the character's skill of that name (core/info_card skillOfCard)
    const std::string shown = text(read32(window + kAegisText), read32(window + kAegisLen));
    const ic::LearnedSkill skill = ic::skillOfCard(shown, sessionSkills(), clienttables::skillNames());
    const std::string& aegis = skill.aegis;
    const int level = skill.level;
    const std::string what = aegis + '|' + std::to_string(level) + '@' + std::to_string(s.scale);
    if (what == o.what) return;
    if (o.what.rfind(aegis + '|', 0) != 0) o.show.scroll = 0;
    o.what = what;
    ic::Card c;
    const auto& names = clienttables::skillNames();
    const auto n = names.find(aegis);
    c.title = n != names.end() ? utf8(s, hotbar::spaced(n->second)) : aegis;
    c.note = level > 0 ? settings::fmt(say(settings::Text::CardLevel), {std::to_string(level)})
                       : say(settings::Text::CardNotLearned);
    const auto d = s.tables.skills.find(aegis);
    if (d != s.tables.skills.end()) {
        c.lines = utf8Lines(s, ic::forJob(d->second, ownJob(), jobWords));  // .270: the character's job's
        if (!c.lines.empty() && c.lines.front() == utf8(s, n != names.end() ? hotbar::spaced(n->second) : aegis))
            c.lines.erase(c.lines.begin());  // its first line its name again
    }
    if (c.lines.empty()) c.lines = skillFacts(s, aegis);  // 3.0.11-beta.229
    c.icon = clienttables::icon(hotbar::lowered(aegis), ic::iconSide(s.scale), 4 * s.scale);
    o.card = c;
    if (s.told++ < 40)
        LOG_INFO("cards: the skill card shows %s (%s, level %d, %zu description line(s))", c.title.c_str(),
                 aegis.c_str(), level, c.lines.size());
}

float measure(const std::string& utf8Text, float px, bool bold) {
    return float(wordsWidth(utf8Wide(utf8Text), px, bold));
}

ic::Texts texts(const One& o) {
    ic::Texts t;
    t.slots = say(settings::Text::CardSlots);
    t.empty = say(settings::Text::CardEmpty);
    if (o.itemId > 0 && referencewin::on()) t.reference = say(settings::Text::RefInReference);  // 3.0.18-beta.242
    return t;
}

void render(One& o) {
    ++o.sheetGen;
}

Blocker blockerOf(Kind k) {
    return k == Kind::Item ? Blocker::ItemCard : k == Kind::Skill ? Blocker::SkillCard : Blocker::SlotCard;
}

void block(const State& s, const One& o, HWND game, Kind kind) {
    RECT client{};
    if (s.targetW <= 0 || s.targetH <= 0 || !GetClientRect(game, &client)) return;
    const panel::Box& b = o.layout.glass;
    const float x0 = float(o.layout.x) + b.x, y0 = float(o.layout.y) + b.y;
    const RECT r = clientBox(client, s.targetW, s.targetH, x0, y0, b.w, b.h);
    uapad::gamewindow::setBlocked(blockerOf(kind), game, &r, false);
}

void hide(One& o, Kind kind) {
    if (o.shown) uapad::gamewindow::setBlocked(blockerOf(kind), nullptr, nullptr, false);
    o.shown = false;
    o.what.clear();
}

One& one(Kind k) { return state().cards[k == Kind::Item ? 0 : k == Kind::Skill ? 1 : 2]; }

}  // namespace

static void frameInner(HWND game, bool on, unsigned codePage, double k, int targetWidth,
                       int targetHeight) {
    State& s = state();
    s.codePage = codePage;
    if (on != s.on) {
        s.on = on;
        for (One& o : s.cards) o.key.clear();
        LOG_INFO("cards: [interface] cards = %d", on ? 1 : 0);
    }
    const bool world = inGameWorld();
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    for (int i = 0; i < 2; ++i) {
        const Kind kind = i == 0 ? Kind::Item : Kind::Skill;
        One& o = s.cards[i];
        const uint32_t window = world ? gameCard(i) : 0;
        if (world) hold(o, window, on, kind);
        if (!on || !window || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
            hide(o, kind);
            o.window = 0;
            continue;
        }
        readTables(s);
        if (!o.shown || window != o.window) {  // opened: beside the cursor that asked for it
            int cx = 0, cy = 0;
            if (readGameCursor(cx, cy)) {
                o.atX = int(float(cx) * float(k) + 28 * s.scale);
                o.atY = int(float(cy) * float(k) - 60 * s.scale);
            } else {
                o.atX = targetWidth / 2 + int(40 * s.scale);
                o.atY = targetHeight / 4;
            }
            o.what.clear();
            o.move = CardDrag{};  // a new card opens beside the cursor again
            LOG_INFO("cards: %s opened - the new one shows it", nameOf(kind));
        }
        o.window = window;
        if (kind == Kind::Item) itemCard(s, o, window);
        else skillCard(s, o, window);
        o.layout = ic::layout(o.card, o.atX, o.atY, targetWidth, targetHeight, s.scale, o.show, texts(o), measure);
        o.move.apply(o.layout.x, o.layout.y, o.layout.w, o.layout.h, targetWidth, targetHeight, k, 48 * s.scale);
        o.show.scroll = std::clamp(o.show.scroll, 0, std::max(0, o.layout.rows - o.layout.shownRows));
        o.show.hoverClose = false;
        int cx = 0, cy = 0;
        if (readHoverCursor(cx, cy))
            o.show.hoverClose = ic::hitTest(o.layout, float(cx) * float(k) - float(o.layout.x),
                                            float(cy) * float(k) - float(o.layout.y)) == ic::Part::Close;
        const std::string key = o.what + ';' + std::to_string(o.layout.x) + ',' + std::to_string(o.layout.y) + ';' +
                                std::to_string(o.show.scroll) + (o.show.hoverClose ? "c" : "") + '#' +
                                std::to_string(uistyle::generation()) + (uaui::ready() ? "u" : "");
        if (key != o.key) {
            o.key = key;
            render(o);
        }
        o.shown = true;
        block(s, o, game, kind);
    }
    One& c = s.cards[2];  // 3.0.18-beta.139: a slot's card, its own window
    if (!on || !world || !s.slotOpen || s.slotCard <= 0 || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        if (!world) s.slotOpen = false;
        hide(c, Kind::Card);
        return;
    }
    readTables(s);
    cardCard(s, c, s.slotCard);
    c.layout = ic::layout(c.card, c.atX, c.atY, targetWidth, targetHeight, s.scale, c.show, texts(c), measure);
    c.move.apply(c.layout.x, c.layout.y, c.layout.w, c.layout.h, targetWidth, targetHeight, k, 48 * s.scale);
    c.show.scroll = std::clamp(c.show.scroll, 0, std::max(0, c.layout.rows - c.layout.shownRows));
    c.show.hoverClose = false;
    int cx = 0, cy = 0;
    if (readHoverCursor(cx, cy))
        c.show.hoverClose = ic::hitTest(c.layout, float(cx) * float(k) - float(c.layout.x),
                                        float(cy) * float(k) - float(c.layout.y)) == ic::Part::Close;
    const std::string key = c.what + ';' + std::to_string(c.layout.x) + ',' + std::to_string(c.layout.y) + ';' +
                            std::to_string(c.show.scroll) + (c.show.hoverClose ? "c" : "") + '#' +
                            std::to_string(uistyle::generation()) + (uaui::ready() ? "u" : "");
    if (key != c.key) {
        c.key = key;
        render(c);
    }
    c.shown = true;
    block(s, c, game, Kind::Card);
}

// 3.0.18-beta.31 (docs/superpowers/plans/2026-11-09-uaui-migration.md, stage 5 - by hand): a thing's or a skill's
// card in RmlUi's markup, line by line as core infocard paints it - its glass, the title's band, the icon, ✕, the
// card's own picture, the slots (gold where a card sits); the words where core infocard places them
static void fillSheet(One& o, uisheet::Content& c) {
    const ic::Layout& L = o.layout;
    const ic::Card& d = o.card;
    const float sc = L.s;
    constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xF3C653, kMuted = 0x9AA3B5;
    auto& u = c.under;
    const auto centred = [&](const panel::Picture* p, const panel::Box& b) {
        if (p && p->width > 0 && b.w > 0)
            c.icons.push_back(uisheet::Icon{std::round(b.x + (b.w - float(p->width)) / 2),
                                            std::round(b.y + (b.h - float(p->height)) / 2), float(p->width),
                                            float(p->height), 1, o.sheet.icon(p)});
    };
    c.w = float(L.w), c.h = float(L.h);
    c.glass = uisheet::Box{L.glass.x, L.glass.y, L.glass.w, L.glass.h, 12 * sc, 1.0f * sc, 1, "glass"};
    c.ground = uisheet::Sheet::ground(kCard, uistyle::window());
    c.rect(u, panel::Box{L.glass.x + 1 * sc, L.glass.y + 1 * sc, L.glass.w - 2 * sc, L.body.y - 6 * sc}, 11 * sc,
           kWhite, 0.035f);
    centred(d.icon, L.icon);
    if (o.show.hoverClose) c.rect(u, L.close, 6 * sc, kWhite, 0.10f);
    const uint32_t cross = o.show.hoverClose ? kGold : kMuted;
    const int side = std::max(4, int(std::ceil(L.close.w)));
    c.icons.push_back(uisheet::Icon{L.close.x, L.close.y, float(side), float(side), 1,
                                    o.sheet.made("x" + std::to_string(side) + ':' + std::to_string(cross), side, side,
                                                 [&](panel::Picture& p) {
                                                     uiicons::draw(p, uiicons::Icon::PadCross, 0, 0, float(side),
                                                                   cross, 1.0f, 2.0f);
                                                 })});
    centred(d.picture, L.picture);
    for (size_t i = 0; i < L.slots.size() && i < d.cardIcons.size(); ++i)  // 3.0.18-beta.109: each card's picture
        if (const panel::Picture* p = d.cardIcons[i]; p && p->width > 0)
            centred(p, panel::Box{L.slots[i].x + 3 * sc, L.slots[i].y, 28 * sc, L.slots[i].h});
    for (size_t i = 0; i < L.slots.size(); ++i) {
        const bool filled = i < d.cards.size() && !d.cards[i].empty();
        c.rect(u, L.slots[i], 6 * sc, filled ? kGold : kWhite, filled ? 0.10f : 0.04f, filled ? kGold : kWhite,
               filled ? 0.55f : 0.12f, 1.0f * sc);
    }
    c.say(ic::words(L, d, o.show, texts(o)));
}

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    frameInner(game, on, codePage, k, targetWidth, targetHeight);
    State& s = state();
    for (int i = 0; i < 3; ++i) {
        One& o = s.cards[i];
        const char* name = i == 0 ? "item card" : i == 1 ? "skill card" : "slot card";
        o.sheetShown = o.sheet.frame(name, o.shown, true, o.layout.x, o.layout.y,
                                     o.sheetGen, [&](uisheet::Content& c) { fillSheet(o, c); });
    }
}

void draw(Kind kind, FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    One& o = one(kind);
    o.sheet.raise();  // 3.0.18-beta.43: at its place in the windows' stack
    if (!o.shown || s.k <= 0 || ((o.picture.width <= 0 || o.picture.height <= 0) && !o.sheetShown)) return;
    int cursor = -1, cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) cursor = panel::cursorDraw(record, float(cx), float(cy));
    const panel::Box& g = o.layout.glass;
    // 3.0.18-beta.43: the shadow the driver's, under uaUI's card too
    o.shadow.draw(record, renderer, o.layout.x + int(g.x), o.layout.y + int(g.y), int(g.w), int(g.h), 12 * s.scale,
                  s.scale, s.k);
    if (o.sheetShown) {
        if (cursor >= 0) record.moveLastBefore(record.commands().size() - size_t(cursor) - 1, size_t(cursor));
        return;
    }
    drawPart(record, renderer, o.picture, o.texture, o.texW, o.texH, o.upload, o.layout.x, o.layout.y, s.k);
    if (cursor >= 0) record.moveLastBefore(record.commands().size() - size_t(cursor) - 1, size_t(cursor));
}

bool mouse(Kind kind, HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    One& o = one(kind);
    if (!o.shown) return false;
    if (m == WM_LBUTTONUP) o.move.end();  // 3.0.18-beta.93
    const bool button = mouseButton(m);
    if (!button && m != WM_MOUSEWHEEL) return false;
    float tx = 0, ty = 0;
    if (!pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty, nullptr)) return false;
    const ic::Part part = ic::hitTest(o.layout, tx - float(o.layout.x), ty - float(o.layout.y));
    if (part == ic::Part::None) return false;  // the world's
    result = 0;
    if (m == WM_MOUSEWHEEL) {
        o.show.scroll = ic::scrolled(o.layout, o.show.scroll, GET_WHEEL_DELTA_WPARAM(wp) > 0 ? -1 : 1);
        o.key.clear();
    } else if ((m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK) && part == ic::Part::Close) {
        close(kind);
    } else if (m == WM_LBUTTONDOWN && part == ic::Part::Reference && o.itemId > 0) {  // 3.0.18-beta.242
        referencewin::openItem(o.itemId);
        if (s.told++ < 40) LOG_INFO("cards: «У довіднику» - the item #%d's card opened there", o.itemId);
    } else if (const int slot = ic::slotAt(o.layout, tx - float(o.layout.x), ty - float(o.layout.y));
               (m == WM_LBUTTONDOWN || m == WM_RBUTTONDOWN) && kind == Kind::Item && slot >= 0 && slot < 4 &&
               o.cardIds[slot] > 0) {  // 3.0.18-beta.139: a slot's card in its own window (was in this one)
        One& c = s.cards[2];
        if (!s.slotOpen || s.slotCard != o.cardIds[slot]) {
            c.atX = o.layout.x + o.layout.w + int(12 * s.scale);  // beside the thing's card, else left of it
            if (c.atX + int(300 * s.scale) > s.targetW) c.atX = std::max(0, o.layout.x - int(312 * s.scale));
            c.atY = o.layout.y;
            c.move = CardDrag{};
            c.what.clear();
        }
        s.slotOpen = true;
        s.slotCard = o.cardIds[slot];
        if (s.told++ < 40) LOG_INFO("cards: the card in slot %d opened in its own window (#%d)", slot + 1, s.slotCard);
    } else if (m == WM_LBUTTONDOWN) {  // 3.0.18-beta.93: the title or the body - the card follows the cursor
        o.move.begin(tx, ty);
    }
    return true;
}

bool key(Kind kind, UINT m, WPARAM wp, LPARAM) {
    One& o = one(kind);
    if (!o.shown || m != WM_KEYDOWN) return false;
    if (wp == VK_UP || wp == VK_DOWN || wp == VK_PRIOR || wp == VK_NEXT) {
        const int notches = wp == VK_UP ? -1 : wp == VK_DOWN ? 1 : wp == VK_PRIOR ? -4 : 4;
        o.show.scroll = ic::scrolled(o.layout, o.show.scroll, notches);
        o.key.clear();
        return true;
    }
    return false;
}

bool shown(Kind kind) { return one(kind).shown; }

void close(Kind kind) {
    One& o = one(kind);
    if (kind == Kind::Card) {  // 3.0.18-beta.139: ours alone - no game window behind it
        state().slotOpen = false;
        hide(o, kind);
        return;
    }
    const uint32_t window = gameCard(kind == Kind::Item ? 0 : 1);
    if (!window) return;
    const uint32_t send = read32(read32(window) + kSendMsgSlot);
    LOG_INFO("cards: %s closed - the game's own close (its message 6, 0x87)", nameOf(kind));
    reinterpret_cast<SendMsgFn>(uintptr_t(send))(window, 0, kButtonMsg, kCloseButton, 0, 0);
    hide(o, kind);
}

void beforeGameDraw() {
    State& s = state();
    if (!s.on || !inGameWorld()) return;
    for (int i = 0; i < 2; ++i)
        if (const uint32_t window = gameCard(i)) hold(s.cards[i], window, true, i == 0 ? Kind::Item : Kind::Skill);
}

void padCommand(Kind kind, PadCommand c) {
    One& o = one(kind);
    if (!o.shown) return;
    switch (c) {
        case PadCommand::Up: o.show.scroll = ic::scrolled(o.layout, o.show.scroll, -1); break;
        case PadCommand::Down: o.show.scroll = ic::scrolled(o.layout, o.show.scroll, 1); break;
        case PadCommand::Close: close(kind); return;
    }
    o.key.clear();
}

bool box(Kind kind, int& x, int& y, int& w, int& h) {
    const One& o = one(kind);
    if (!o.shown) return false;
    x = o.layout.x;
    y = o.layout.y;
    w = o.layout.w;
    h = o.layout.h;
    return true;
}

std::vector<std::string> skillDescription(const std::string& aegis, const std::string& name) {
    State& s = state();
    readTables(s);
    std::vector<std::string> out;
    const auto d = s.tables.skills.find(aegis);
    if (d == s.tables.skills.end()) return out;
    for (const std::string& l : ic::forJob(d->second, ownJob(), jobWords))  // .270: the character's job's
        out.push_back(utf8(s, gamewords::withoutColours(l)));
    if (!out.empty() && (out.front() == name || out.front().empty())) out.erase(out.begin());
    return out;
}

std::vector<std::string> itemDescription(int id) {
    State& s = state();
    readTables(s);
    std::vector<std::string> out;
    const auto d = s.tables.items.find(std::to_string(id));
    if (d == s.tables.items.end()) return out;
    for (const std::string& l : d->second) out.push_back(utf8(s, gamewords::withoutColours(l)));
    return out;
}

int slotsOf(int id) {
    State& s = state();
    readTables(s);
    const auto it = s.tables.slots.find(id);
    return it == s.tables.slots.end() ? 0 : it->second;
}

bool showItem(uint32_t itemInfo) {
    using MakeFn = uint32_t(__attribute__((thiscall)) *)(uint32_t mgr, int id);
    constexpr uint32_t kMakeWindow = 0x4D42D0;
    constexpr int kItemCardWindow = 0xC, kShowItem = 0x18;
    if (!itemInfo) return false;
    const uint32_t window = reinterpret_cast<MakeFn>(uintptr_t(kMakeWindow))(kWindowMgr, kItemCardWindow);
    const uint32_t send = window ? read32(read32(window) + kSendMsgSlot) : 0;
    if (!send) return false;
    reinterpret_cast<SendMsgFn>(uintptr_t(send))(window, 0, kShowItem, int(itemInfo), 0, 0);
    return true;
}

void describe(const uint8_t* itemInfo, int id, unsigned codePage, std::string& name, std::string& inside) {
    inside.clear();
    if (!itemInfo || itemInfo[0x3C] == 0) return;
    {
        int cards[4] = {};
        cardnames::cardsOf(itemInfo, cards);
        if (cardnames::forged(cards)) {  // 3.0.18-beta.142: its maker under the name (the name has its words)
            inside = forgedLine(cards, codePage);
            return;
        }
    }
    const int slots = slotsOf(id);
    name = vendshop::withSlots(name, slots);
    int cards[4] = {};
    cardnames::cardsOf(itemInfo, cards);
    const auto& names = clienttables::itemNames();
    inside = vendshop::inside(cards, slots, [&](int card) {
        const auto w = names.find(std::to_string(card));
        return w == names.end() ? std::string() : utf8Of(widened(hotbar::spaced(w->second), codePage));
    }, say(settings::Text::VendFree));
}


void setElementColours(bool on) {
    if (on != g_elementColours) LOG_INFO("cards: [interface] element_colours = %d", on ? 1 : 0);
    g_elementColours = on;
}

int elementOf(int id, const int cards[4]) { return g_elementColours ? itemelements::ofThing(id, cards) : 0; }

uint32_t elementColour(int id, const uint8_t* itemInfo) {
    if (!g_elementColours || (itemInfo && itemInfo[0x3C] == 0)) return 0;
    int cards[4] = {};
    if (itemInfo) cardnames::cardsOf(itemInfo, cards);
    return itemelements::colour(itemelements::ofThing(id, cards));
}

std::string elementHex(int id, const uint8_t* itemInfo) {
    const uint32_t c = elementColour(id, itemInfo);
    if (!c) return "";
    char b[8];
    std::snprintf(b, sizeof b, "#%06x", unsigned(c));
    return b;
}

std::string withSlots(const std::string& name, int id, const uint8_t* itemInfo) {
    if (!itemInfo || itemInfo[0x3C] == 0) return name;
    int cards[4] = {};
    cardnames::cardsOf(itemInfo, cards);
    return cardnames::forged(cards) ? name : vendshop::withSlots(name, slotsOf(id));
}

}  // namespace uagfx::win::infocardwin

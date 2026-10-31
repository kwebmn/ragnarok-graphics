#include "win/skill_window.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "core/char_block.hpp"
#include "core/hotbar_panel.hpp"
#include "core/layout.hpp"
#include "core/settings/strings.hpp"
#include "core/skill_tree.hpp"
#include "core/skill_window.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "pad/win/runtime.hpp"
#include "win/client_tables.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/pad_assign.hpp"
#include "win/paths.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/window_shadow.hpp"
#include "win/window_stack.hpp"

namespace uagfx::win::skillswin {
namespace {

namespace kw = uagfx::skillwin;

// uaRO.exe's: the windows' manager's skill tree (UI window 0x25, its class's vtable, its SendMsg FUN_004bdd90 -
// message 6 with 0x87 its close button), a window's shown flag; the session's skills (a std::list: its head at +0x2D8,
// a node's record at +8), the levels they are used at (+0x534, an int each by id: FUN_006397c0), the job (+0xE44), the
// free skill points; the skills' AEGIS names by id (FUN_00505ce0); the game mode's SendMsg (FUN_005707d0, vtable +
// 0x14): 0x4A raises a skill (packet 0x0112), 0x7E uses one (its record, the level), 0x51 drags one (id, name, level)
constexpr uint32_t kWindowMgr = 0x727ED8, kSkillsAt = kWindowMgr + 0x118, kSkillsClass = 0x69B9D0;
constexpr uint32_t kShown = 0x38, kSendMsgSlot = 0x94, kSkillsSendMsg = 0x4BDD90;
constexpr int kButtonMsg = 6, kCloseButton = 0x87;
constexpr uint32_t kSession = 0x771148, kSkillList = kSession + 0x2D8, kUseLevels = kSession + 0x534;
constexpr uint32_t kJob = kSession + 0xE44, kSkillPoints = 0x771FA4, kSkillNames = 0x74B9AC;
constexpr int kMostSkill = 0x3FD;
constexpr uint32_t kModeNow = 0x74B57C, kModeMsgSlot = 0x14, kModeMsg = 0x5707D0;
constexpr int kRaiseMsg = 0x4A, kUseMsg = 0x7E, kDragMsg = 0x51;
// 3.0.11-beta.106: a drag let go - the game mode's 0x12 (the cursor's x, y), as its frame sends it when the button
// comes up (as «Речі»'s, item_window)
constexpr int kDropMsg = 0x12;
constexpr ULONGLONG kReadMs = 200;

using SendMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);
using ModeMsgFn = int(__attribute__((thiscall)) *)(uint32_t mode, int msg, int a, int b, int c);

struct Raw {  // a skill as the session keeps it
    uint32_t record = 0, name = 0;
    int id = 0, type = 0, level = 0, sp = 0, up = 0, range = 0;
    std::string aegis;
};

struct State {
    WindowShadow shadow;
    WindowEdge edge;  // 3.0.11-beta.108: the top of two or more windows (win/window_stack)
    bool on = false, shown = false, hid = false, stateRead = false, namesChecked = false, namesKnown = false;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    float fx = 0.3f, fy = 0.18f;  // its place: a share of the free room to its left and above it
    unsigned codePage = 0;
    std::vector<Raw> raw;
    std::vector<std::vector<int>> tabRaw;  // each tab's skills: their indices in `raw`
    kw::Data data;
    ULONGLONG readAt = 0;
    kw::Show show;
    kw::Layout layout;
    std::string key;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    bool moving = false;
    float grabX = 0, grabY = 0;
    int told = 0, pressesTold = 0;
    // 3.0.11-beta.106 (R-WCXUQP: «глюк с переносом скилов на панель, не переносятся»): the game's drag begun at the
    // press never saw its button come up (the press and the release are the window's) - a drag of its own now, its
    // skill's icon under the cursor, the game's drag begun and let go only when the button comes up (as «Речі»)
    kw::Press press;
    panel::Picture dragIcon;
    const panel::Picture* dragOf = nullptr;
    uint32_t dragTexture = 0;
    int dragW = 0, dragH = 0;
    bool dragUpload = false;
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

std::string readString(uint32_t at) {
    if (!at) return "";
    char buf[64] = {};
    if (!readGame(at, buf, sizeof buf - 1)) return "";
    return std::string(buf, strnlen(buf, sizeof buf - 1));
}

uint32_t gameWindow() {
    const uint32_t w = read32(kSkillsAt);
    return w && read32(w) == kSkillsClass ? w : 0;
}

void hold(State& s, uint32_t window, bool on) {
    if (!window) {
        s.hid = false;
        return;
    }
    if (on) {
        if (read32(window + kShown) && write32(window + kShown, 0) && !s.hid) {
            LOG_INFO("skills: the game's skill tree hidden for «Навички» (its shown flag 0)");
            s.hid = true;
        }
    } else if (s.hid) {
        write32(window + kShown, 1);
        s.hid = false;
        LOG_INFO("skills: the game's skill tree back");
    }
}

void readState(State& s) {
    if (s.stateRead) return;
    s.stateRead = true;
    std::string text;
    if (!readTextFile(layout::join(gameDir(), layout::kSkillsState), text)) return;
    float x = 0, y = 0;
    int tab = 0;
    if (const size_t at = text.find("at "); at != std::string::npos &&
        std::sscanf(text.c_str() + at, "at %f %f", &x, &y) == 2 && x >= 0 && x <= 1 && y >= 0 && y <= 1) {
        s.fx = x;
        s.fy = y;
    }
    if (const size_t at = text.find("tab "); at != std::string::npos && std::sscanf(text.c_str() + at, "tab %d", &tab))
        s.show.tab = std::max(0, tab);
}

void keepState(const State& s) {
    char text[64];
    std::snprintf(text, sizeof text, "at %.4f %.4f\ntab %d\n", s.fx, s.fy, s.show.tab);
    const std::string path = layout::join(gameDir(), layout::kSkillsState);
    if (!writeFileAtomic(path, std::string(text))) LOG_INFO("skills: %s cannot be written", path.c_str());
}

std::string aegisById(State& s, int id) {
    if (!s.namesChecked) {
        s.namesChecked = true;
        const uint32_t names = read32(kSkillNames);
        s.namesKnown = names && readString(read32(names + 4)) == "NV_BASIC";
    }
    if (!s.namesKnown || id <= 0 || id >= kMostSkill) return "";
    return readString(read32(read32(kSkillNames) + 4 * uint32_t(id)));
}

std::string nameOf(const State& s, const std::string& aegis) {
    const auto& names = clienttables::skillNames();
    const auto n = names.find(aegis);
    return n != names.end() ? utf8Of(widened(hotbar::spaced(n->second), s.codePage)) : aegis;
}

int useLevel(int id, int level) {
    const uint32_t levels = read32(kUseLevels);
    const int v = levels && id > 0 && id < kMostSkill ? int(read32(levels + 4 * uint32_t(id))) : 0;
    return v > 0 ? std::min(v, level) : level;
}

bool setUseLevel(int id, int level) {
    const uint32_t levels = read32(kUseLevels);
    return levels && id > 0 && id < kMostSkill && write32(levels + 4 * uint32_t(id), uint32_t(level));
}

void readSkills(std::vector<Raw>& out) {
    out.clear();
    const uint32_t head = read32(kSkillList);
    if (!head) return;
    uint32_t node = read32(head);
    for (int n = 0; node && node != head && n < 600; ++n) {
        int32_t r[8] = {};
        if (!readGame(node + 8, r, sizeof r)) break;
        if (r[1] > 0 && r[1] < 0x10000) {
            Raw k;
            k.record = node + 8;
            k.id = r[1];
            k.type = r[2];
            k.level = r[3];
            k.sp = r[4];
            k.up = r[5];
            k.range = r[6];
            k.name = uint32_t(r[7]);
            k.aegis = readString(k.name);
            out.push_back(k);
        }
        node = read32(node);
    }
}

void readData(State& s) {
    readSkills(s.raw);
    const int job = int(read32(kJob));
    const std::vector<int> chain = skilltree::chain(job);
    std::map<int, std::vector<int>> byJob;  // the tab's job to the skills' indices
    for (size_t i = 0; i < s.raw.size(); ++i) {
        int home = skilltree::homeJob(chain, s.raw[i].id);
        if (home < 0) home = chain.back();
        byJob[home].push_back(int(i));
    }
    s.data.tabs.clear();
    s.tabRaw.clear();
    const int last = chain.back();
    const float side = float(kw::iconSide(s.scale));
    for (int j : chain) {
        auto it = byJob.find(j);
        if (it == byJob.end()) continue;
        std::vector<int>& list = it->second;
        std::stable_sort(list.begin(), list.end(), [&](int a, int b) {
            return skilltree::order(j, s.raw[size_t(a)].id) < skilltree::order(j, s.raw[size_t(b)].id);
        });
        const char* named = charblock::jobName(j);
        kw::Tab tab{named && *named ? named : "#" + std::to_string(j), {}};
        for (int i : list) {
            const Raw& r = s.raw[size_t(i)];
            kw::Skill k;
            k.id = r.id;
            k.aegis = r.aegis;
            k.name = nameOf(s, r.aegis);
            k.type = r.type;
            k.level = r.level;
            k.most = skilltree::maxLevel(last, r.id);
            if (!k.most) k.most = skilltree::maxLevel(j, r.id);
            k.use = useLevel(r.id, r.level);
            k.sp = r.sp;
            k.range = r.range;
            k.up = r.up != 0;
            if (r.level <= 0) {
                std::string needs;
                for (const auto& [id, lv] : skilltree::needs(last, r.id)) {
                    const std::string aegis = aegisById(s, id);
                    const std::string named = aegis.empty() ? "#" + std::to_string(id) : nameOf(s, aegis);
                    needs += (needs.empty() ? "" : ", ") + named + " " + std::to_string(lv);
                }
                k.needs = needs;
            }
            k.icon = r.aegis.empty() ? nullptr : clienttables::icon(hotbar::lowered(r.aegis), int(side), 4 * s.scale);
            tab.skills.push_back(k);
        }
        s.data.tabs.push_back(tab);
        s.tabRaw.push_back(list);
    }
    s.data.points = int(read32(kSkillPoints));
    if (!s.data.tabs.empty()) s.show.tab = std::clamp(s.show.tab, 0, int(s.data.tabs.size()) - 1);
}

kw::Texts texts() {
    kw::Texts t;
    t.title = say(settings::Text::SkillsTitle);
    t.hotkey = "Alt+S";
    t.points = say(settings::Text::SkillsPoints);
    t.drag = say(settings::Text::SkillsDrag);
    t.passive = say(settings::Text::SkillsPassive);
    t.enemy = say(settings::Text::SkillsEnemy);
    t.ground = say(settings::Text::SkillsGround);
    t.self = say(settings::Text::SkillsSelf);
    t.ally = say(settings::Text::SkillsAlly);
    t.trap = say(settings::Text::SkillsTrap);
    t.sp = say(settings::Text::SkillsSp);
    t.range = say(settings::Text::SkillsRange);
    t.needs = say(settings::Text::SkillsNeeds);
    t.unlearned = say(settings::Text::SkillsUnlearned);
    t.use = say(settings::Text::SkillsPadUse);
    t.raise = say(settings::Text::SkillsPadRaise);
    t.assign = say(settings::Text::SkillsPadAssign);
    t.close = say(settings::Text::SkillsPadClose);
    return t;
}

float measure(const std::string& utf8Text, float px, bool bold) {
    return float(wordsWidth(utf8Wide(utf8Text), px, bold));
}

std::vector<Words> gdiWords(const std::vector<kw::Words>& in) {
    std::vector<Words> out;
    for (const kw::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == kw::Align::Right    ? DT_RIGHT
                                 : w.align == kw::Align::Center ? DT_CENTER
                                                                : DT_LEFT),
                            false});
    return out;
}

void render(State& s) {
    const uicost::Paint cost("«Навички»", s.layout.w, s.layout.h);
    if (!panel::blank(s.picture, s.layout.w, s.layout.h)) return;
    kw::paint(s.picture, s.layout, s.data, s.show);
    drawWords(s.picture, gdiWords(kw::words(s.layout, s.data, s.show, texts(), measure)));
    s.upload = true;
}

void block(const State& s, HWND game) {  // the mouse's clicks over it are its own, not the world's
    RECT client{};
    if (s.targetW <= 0 || s.targetH <= 0 || !GetClientRect(game, &client)) return;
    const panel::Box& b = s.layout.glass;
    const float x0 = float(s.layout.x) + b.x, y0 = float(s.layout.y) + b.y;
    const RECT r{LONG(int64_t(x0) * client.right / s.targetW), LONG(int64_t(y0) * client.bottom / s.targetH),
                 LONG(int64_t(x0 + b.w) * client.right / s.targetW + 1),
                 LONG(int64_t(y0 + b.h) * client.bottom / s.targetH + 1)};
    uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Skills, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Skills, nullptr, nullptr, false);
    s.shown = false;
    s.moving = false;
    if (s.press.dragging) windrag::clear();  // 3.0.11-beta.110
    s.press = kw::Press{};
}

bool modeMsg(int msg, int a, int b, int c) {
    const uint32_t mode = read32(kModeNow);
    if (!mode || !inGameWorld() || read32(read32(mode) + kModeMsgSlot) != kModeMsg) return false;
    reinterpret_cast<ModeMsgFn>(uintptr_t(kModeMsg))(mode, msg, a, b, c);
    return true;
}

void closeGameWindow() {
    const uint32_t window = gameWindow();
    if (window && read32(read32(window) + kSendMsgSlot) == kSkillsSendMsg)
        reinterpret_cast<SendMsgFn>(uintptr_t(kSkillsSendMsg))(window, 0, kButtonMsg, kCloseButton, 0, 0);
}

// The skill at `index` of the tab shown, as the session keeps it now; null: none
const Raw* rawAt(const State& s, int index) {
    const int tab = s.show.tab;
    if (tab < 0 || tab >= int(s.tabRaw.size()) || index < 0 || index >= int(s.tabRaw[size_t(tab)].size()))
        return nullptr;
    return &s.raw[size_t(s.tabRaw[size_t(tab)][size_t(index)])];
}

const kw::Skill* skillAt(const State& s, int index) {
    const int tab = s.show.tab;
    if (tab < 0 || tab >= int(s.data.tabs.size()) || index < 0 || index >= int(s.data.tabs[size_t(tab)].skills.size()))
        return nullptr;
    return &s.data.tabs[size_t(tab)].skills[size_t(index)];
}

void raise(State& s, int index, const char* by) {
    const kw::Skill* k = skillAt(s, index);
    if (!k || !kw::canRaise(*k, s.data.points)) return;
    const bool sent = modeMsg(kRaiseMsg, k->id, 0, 0);
    s.readAt = 0;
    if (s.told++ < 20)
        LOG_INFO("skills: %s %s +1 (%d -> %d) %s", by, k->aegis.c_str(), k->level, k->level + 1,
                 sent ? "asked as the game's own «+» (0x4A)" : "not asked");
}

void use(State& s, int index, const char* by) {
    const Raw* r = rawAt(s, index);
    const kw::Skill* k = skillAt(s, index);
    if (!r || !k || !kw::usable(*k)) return;
    const bool sent = modeMsg(kUseMsg, int(r->record), k->use, 0);
    if (s.told++ < 20)
        LOG_INFO("skills: %s uses %s at level %d %s", by, k->aegis.c_str(), k->use,
                 sent ? "as the game's own window (0x7E)" : "not sent");
}

void changeUse(State& s, int index, int by) {
    const kw::Skill* k = skillAt(s, index);
    if (!k || !kw::usable(*k)) return;
    const int to = std::clamp(k->use + by, 1, k->level);
    if (to == k->use || !setUseLevel(k->id, to)) return;
    s.readAt = 0;
    if (s.told++ < 20) LOG_INFO("skills: %s used at level %d now", k->aegis.c_str(), to);
}

// 3.0.11-beta.106: a skill dragged and let go - the game's drag of it begun (0x51, as its own tree's press) and let go
// at once where the game's cursor is (0x12): onto a window of the game's or the driver's hotbar (win/world_mouse)
const char* dropAt(State& s, int index) {
    const Raw* r = rawAt(s, index);
    const kw::Skill* k = skillAt(s, index);
    if (!r || !k || !kw::usable(*k) || !r->name) return "not dropped (not a skill to use)";
    int cx = 0, cy = 0;
    if (!readGameCursor(cx, cy)) return "not dropped (no cursor)";
    if (!modeMsg(kDragMsg, k->id, int(r->name), k->use)) return "not dropped (the game mode not found)";
    modeMsg(kDropMsg, cx, cy, 0);
    return "let go through the game's own drop";
}

void setTab(State& s, int tab) {
    if (s.data.tabs.empty()) return;
    tab = std::clamp(tab, 0, int(s.data.tabs.size()) - 1);
    if (tab == s.show.tab) return;
    s.show.tab = tab;
    s.show.scroll = 0;
    s.show.focus = 0;
    keepState(s);
}

}  // namespace

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    State& s = state();
    if (on != s.on) {
        s.on = on;
        s.key.clear();
        LOG_INFO("skills: [interface] skills = %d", on ? 1 : 0);
    }
    const bool world = inGameWorld();
    const uint32_t window = world ? gameWindow() : 0;
    if (world) hold(s, window, on);
    if (!on || !window || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        hide(s);
        return;
    }
    readState(s);
    const float scale = uistyle::readingScale(k);
    const bool fresh = scale != s.scale || codePage != s.codePage;
    s.k = float(k);
    s.scale = scale;
    s.codePage = codePage;
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    const ULONGLONG now = GetTickCount64();
    if (fresh || now - s.readAt >= kReadMs) {
        s.readAt = now;
        readData(s);
    }
    const uapad::runtime::Look pad = uapad::runtime::look();
    s.show.pad = pad.shown;
    s.show.sony = pad.sony;
    const int count = s.data.tabs.empty() ? 0 : int(s.data.tabs[size_t(s.show.tab)].skills.size());
    if (s.show.pad) {
        s.show.focus = std::clamp(s.show.focus, 0, std::max(0, count - 1));
        s.show.scroll = kw::keepInSight(s.show.focus, s.show.scroll, count);
    }
    const kw::Texts t = texts();
    s.layout = kw::layout(0, 0, scale, s.data, s.show, t, measure);
    s.layout.x = int(std::lround(s.fx * float(std::max(0, targetWidth - s.layout.w))));
    s.layout.y = int(std::lround(s.fy * float(std::max(0, targetHeight - s.layout.h))));
    s.show.hoverTab = s.show.hoverRow = s.show.hoverPlus = s.show.hoverLess = s.show.hoverMore = -1;
    s.show.hoverClose = false;
    int cx = 0, cy = 0;
    if (readGameCursor(cx, cy) && !s.moving) {
        const kw::Hit h = kw::hitTest(s.layout, float(cx) * float(k) - float(s.layout.x),
                                      float(cy) * float(k) - float(s.layout.y));
        switch (h.part) {
            case kw::Part::Tab: s.show.hoverTab = h.index; break;
            case kw::Part::Row: s.show.hoverRow = h.index; break;
            case kw::Part::Plus: s.show.hoverPlus = h.index; break;
            case kw::Part::Less: s.show.hoverLess = h.index; break;
            case kw::Part::More: s.show.hoverMore = h.index; break;
            case kw::Part::Close: s.show.hoverClose = true; break;
            default: break;
        }
    }
    std::string key = std::to_string(s.layout.x) + ',' + std::to_string(s.layout.y) + ';' + std::to_string(scale) +
                      ';' + std::to_string(s.show.tab) + ',' + std::to_string(s.layout.first) + ',' +
                      std::to_string(s.show.hoverTab) + ',' + std::to_string(s.show.hoverRow) + ',' +
                      std::to_string(s.show.hoverPlus) + ',' + std::to_string(s.show.hoverLess) + ',' +
                      std::to_string(s.show.hoverMore) + ',' + std::to_string(s.show.focus) +
                      (s.show.hoverClose ? "x" : "") + (s.show.pad ? (s.show.sony ? "S" : "X") : "") + ';' +
                      std::to_string(s.data.points) + ';';
    for (const kw::Tab& tab : s.data.tabs)
        for (const kw::Skill& sk : tab.skills)
            key += std::to_string(sk.id) + ':' + std::to_string(sk.level) + '/' + std::to_string(sk.use) +
                   (sk.up ? "u" : "") + (sk.icon ? "i" : "") + ',';
    if (const std::string drawn = key + '#' + std::to_string(uistyle::generation()); drawn != s.key) {
        s.key = drawn;
        render(s);
    }
    if (!s.shown && s.told < 20) {
        ++s.told;
        size_t skills = 0;
        for (const kw::Tab& tab : s.data.tabs) skills += tab.skills.size();
        LOG_INFO("skills: «Навички» open - %zu tab(s), %zu skill(s), %d free point(s), the job %u", s.data.tabs.size(),
                 skills, s.data.points, read32(kJob));
    }
    s.shown = true;
    block(s, game);
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.shown || s.k <= 0 || s.picture.width <= 0 || s.picture.height <= 0) return;
    int cursor = -1, cx = 0, cy = 0;  // under the game's own cursor, as the minimap
    const bool known = readGameCursor(cx, cy);
    if (known) cursor = panel::cursorDraw(record, float(cx), float(cy));
    const panel::Box& g = s.layout.glass;
    s.shadow.draw(record, renderer, s.layout.x + int(g.x), s.layout.y + int(g.y), int(g.w), int(g.h), 12 * s.scale,
                  s.scale, s.k);
    drawPart(record, renderer, s.picture, s.texture, s.texW, s.texH, s.upload, s.layout.x, s.layout.y, s.k);
    if (windowstack::marked(winstack::Win::Skills))  // 3.0.11-beta.108: its gold edge, the top of two or more
        s.edge.draw(record, renderer, s.layout.x + int(g.x), s.layout.y + int(g.y), int(g.w), int(g.h), 12 * s.scale,
                    s.scale, s.k);
    const kw::Skill* dragged = s.press.dragging ? skillAt(s, s.press.index) : nullptr;
    const panel::Picture* icon = dragged ? dragged->icon : nullptr;
    if (icon && icon->width > 0 && known) {  // 3.0.11-beta.106: the dragged skill's icon under the cursor
        if (icon != s.dragOf) {
            s.dragOf = icon;
            s.dragIcon = *icon;
            s.dragUpload = true;
        }
        const int x = int(std::lround(float(cx) * s.k)) - icon->width / 2;
        const int y = int(std::lround(float(cy) * s.k)) - icon->height / 2;
        drawPart(record, renderer, s.dragIcon, s.dragTexture, s.dragW, s.dragH, s.dragUpload, x, y, s.k);
    }
    if (cursor >= 0) record.moveLastBefore(record.commands().size() - size_t(cursor) - 1, size_t(cursor));
}

bool shown() { return state().shown; }

void padCommand(PadCommand c) {
    State& s = state();
    if (!s.shown || s.data.tabs.empty()) return;
    const int count = int(s.data.tabs[size_t(s.show.tab)].skills.size());
    s.show.focus = std::clamp(s.show.focus, 0, std::max(0, count - 1));
    switch (c) {
        case PadCommand::Up: s.show.focus = count ? (s.show.focus + count - 1) % count : 0; break;
        case PadCommand::Down: s.show.focus = count ? (s.show.focus + 1) % count : 0; break;
        case PadCommand::Less: changeUse(s, s.show.focus, -1); break;
        case PadCommand::More: changeUse(s, s.show.focus, +1); break;
        case PadCommand::TabBack: {
            const int tabs = int(s.data.tabs.size());
            setTab(s, (s.show.tab + tabs - 1) % tabs);
            break;
        }
        case PadCommand::TabOn: setTab(s, (s.show.tab + 1) % int(s.data.tabs.size())); break;
        case PadCommand::Use: use(s, s.show.focus, "the controller's A"); break;
        case PadCommand::Raise: raise(s, s.show.focus, "the controller's Y:"); break;
        case PadCommand::Assign:
            if (const kw::Skill* k = skillAt(s, s.show.focus); k && kw::usable(*k) && !k->aegis.empty()) {
                padassignwin::open("##" + k->aegis, k->use, k->name);
                LOG_INFO("skills: the controller's X - «Призначити» for %s at level %d", k->aegis.c_str(), k->use);
            }
            break;
        case PadCommand::Close: closeGameWindow(); break;
    }
    s.key.clear();
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    if (!s.shown) return false;
    const bool button = m == WM_LBUTTONDOWN || m == WM_LBUTTONUP || m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDOWN ||
                        m == WM_RBUTTONUP || m == WM_RBUTTONDBLCLK || m == WM_MBUTTONDOWN || m == WM_MBUTTONUP;
    if (!button && m != WM_MOUSEWHEEL && m != WM_MOUSEMOVE) return false;
    float tx = 0, ty = 0;
    bool fromGame = false;
    if (!pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty, &fromGame)) return false;
    if (s.moving && (m == WM_MOUSEMOVE || m == WM_LBUTTONUP)) {  // its title dragged
        const int roomX = std::max(1, s.targetW - s.layout.w), roomY = std::max(1, s.targetH - s.layout.h);
        s.fx = std::clamp((tx - s.grabX) / float(roomX), 0.0f, 1.0f);
        s.fy = std::clamp((ty - s.grabY) / float(roomY), 0.0f, 1.0f);
        if (m == WM_LBUTTONUP) {
            s.moving = false;
            keepState(s);
        }
        result = 0;
        return true;
    }
    if (s.press.index >= 0 && (m == WM_MOUSEMOVE || m == WM_LBUTTONUP)) {  // a skill pressed: dragged, or a click
        if (m == WM_MOUSEMOVE) {
            const bool was = s.press.dragging, now = kw::moved(s.press, tx, ty, s.scale);
            if (now && !was)  // 3.0.11-beta.110: told to the hotbar (its cells lit in gold)
                if (const kw::Skill* k = skillAt(s, s.press.index))
                    windrag::set(windrag::Thing{windrag::Kind::Skill, k->id, 0, 0});
            return now;
        }
        if (s.press.dragging) windrag::clear();
        const int dragged = kw::released(s.press);
        if (dragged >= 0) {
            const kw::Skill* k = skillAt(s, dragged);
            const char* did = dropAt(s, dragged);
            if (s.told++ < 20) LOG_INFO("skills: %s dragged and let go - %s", k ? k->aegis.c_str() : "?", did);
        }
        result = 0;
        return true;
    }
    const kw::Hit h = kw::hitTest(s.layout, tx - float(s.layout.x), ty - float(s.layout.y));
    if ((m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK) && s.pressesTold < 8) {
        ++s.pressesTold;
        LOG_INFO("skills: a press at the window's (%d, %d) - the target's (%.0f, %.0f) by the %s; «Навички» at %d, %d "
                 "%dx%d: part %d, index %d", GET_X_LPARAM(lp), GET_Y_LPARAM(lp), tx, ty,
                 fromGame ? "game's cursor" : "message's point", s.layout.x, s.layout.y, s.layout.w, s.layout.h,
                 int(h.part), h.index);
    }
    if (h.part == kw::Part::None || m == WM_MOUSEMOVE) return false;
    result = 0;
    if (m == WM_MOUSEWHEEL) {  // the list scrolled, three rows a notch
        const int count = s.data.tabs.empty() ? 0 : int(s.data.tabs[size_t(s.show.tab)].skills.size());
        const int by = GET_WHEEL_DELTA_WPARAM(wp) > 0 ? -3 : 3;
        s.show.scroll = std::clamp(s.layout.first + by, 0, std::max(0, count - kw::kRows));
        s.key.clear();
        return true;
    }
    if (m != WM_LBUTTONDOWN && m != WM_LBUTTONDBLCLK) return true;
    switch (h.part) {
        case kw::Part::Close:
            if (s.told++ < 20) LOG_INFO("skills: its close - the game's own skill tree asked to close (its 0x87)");
            closeGameWindow();
            break;
        case kw::Part::Tab: setTab(s, h.index); break;
        case kw::Part::Plus:
            s.show.focus = h.index;
            raise(s, h.index, "its «+»:");
            break;
        case kw::Part::Less:
        case kw::Part::More:
            s.show.focus = h.index;
            changeUse(s, h.index, h.part == kw::Part::Less ? -1 : 1);
            break;
        case kw::Part::Row:
            s.show.focus = h.index;
            if (m == WM_LBUTTONDBLCLK)
                use(s, h.index, "a double click");
            else
                kw::pressed(s.press, h.index, tx, ty);  // 3.0.11-beta.106: a drag once it moves
            break;
        case kw::Part::Title:
            s.moving = true;
            s.grabX = tx - float(s.layout.x);
            s.grabY = ty - float(s.layout.y);
            break;
        default:
            break;
    }
    s.key.clear();
    return true;
}

void beforeGameDraw() {
    State& s = state();
    if (!s.on || !inGameWorld()) return;
    if (const uint32_t window = gameWindow()) hold(s, window, true);
}


bool box(int& x, int& y, int& w, int& h) {  // 3.0.11-beta.109
    const State& s = state();
    if (!s.shown) return false;
    x = s.layout.x;
    y = s.layout.y;
    w = s.layout.w;
    h = s.layout.h;
    return true;
}

void moveTo(int x, int y) {  // 3.0.11-beta.109: question 4 - beside the window standing in its own place
    State& s = state();
    if (!s.shown) return;
    const int roomX = std::max(1, s.targetW - s.layout.w), roomY = std::max(1, s.targetH - s.layout.h);
    s.fx = std::clamp(float(x) / float(roomX), 0.0f, 1.0f);
    s.fy = std::clamp(float(y) / float(roomY), 0.0f, 1.0f);
    s.layout.x = x;
    s.layout.y = y;
    keepState(s);
}

}  // namespace uagfx::win::skillswin

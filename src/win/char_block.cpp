#include "win/char_block.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "core/char_block.hpp"
#include "core/char_status.hpp"
#include "core/exp_rate.hpp"
#include "core/game_packets.hpp"
#include "core/layout.hpp"
#include "core/settings/strings.hpp"
#include "core/sprite_file.hpp"
#include "core/ui_motion.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "win/ammo_watch.hpp"
#include "win/buff_overlay.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/head_picture.hpp"
#include "win/hud_edit.hpp"
#include "win/log.hpp"
#include "win/party_panel.hpp"
#include "win/paths.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/updates_hook.hpp"

namespace uagfx::win::charpanel {
namespace {

namespace cb = uagfx::charblock;

// uaRO.exe's: the windows' manager and its UI window 0 (Basic Info: its class's vtable, its shown flag the manager
// draws it and gives it the mouse by - FUN_004cd720, FUN_004cea60 -, its SendMsg FUN_0046c980: message 6 a button's)
constexpr uint32_t kWindowMgr = 0x727ED8, kBasicInfo = kWindowMgr + 0x90, kBasicInfoClass = 0x6984A0;
constexpr uint32_t kShown = 0x38, kSendMsgSlot = 0x94, kSendMsg = 0x46C980;
constexpr int kButtonMsg = 6;
using SendMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);
// The session's character: its class, its sex (+0x1CA4, which GetSex FUN_006425b0 keeps in +0x770), its head and
// hair colour (its head's sprite: win/head_picture)
constexpr uint32_t kJob = 0x771F8C, kSex = 0x772DEC, kHead = 0x7718F4, kHairColour = 0x771910;
// 3.0.11-beta.235 (the concept «Блок персонажа»): the guild's name (a std::string's text and length, as the guild
// window reads it - win/guild_card), the party's name (char*), the character's account (its hits and its death)
constexpr uint32_t kGuildName = 0x74B654, kPartyName = 0x772D88, kOwnAccount = 0x771F7C;
// the server's natural regen periods (uAthena conf/battle/player.conf natural_healhp_interval / sp)
constexpr int kHpEverySec = 6, kSpEverySec = 8;
constexpr ULONGLONG kWarnMs = 2000;  // the low HP warning's
constexpr ULONGLONG kTipMs = 400, kPressMs = 160;  // 3.0.11-beta.51: the concept's tip after 0.4 s; a press shown
// 3.0.11-beta.114 (the concept «Концепция окон и инвентаря», question 10 «можна»): the ammo worn running low on the
// info line - read by win/ammo_watch (3.0.11-beta.125: shared with the hotbar's pill)

struct State {
    CharacterLook look = CharacterLook::Off;
    bool shown = false, stateRead = false, hidOld = false, oldTold = false;
    cb::View view = cb::View::Full;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    cb::Data data;
    cb::Layout layout;
    cb::Show show;
    bool wasLow = false;
    ULONGLONG warnUntil = 0;
    std::string key;  // what the picture shows
    std::string portraitKey;
    panel::Picture portrait;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    int clicksLogged = 0;
    // 3.0.11-beta.235: this session (from the character's first frame): its start, its zeny then, the monsters it hit
    // (the recent ones), those of them that died, its deaths
    ULONGLONG sessionAt = 0;
    long long zenyAt = -1;
    std::set<uint32_t> hit;
    int kills = 0, deaths = 0;
    // 3.0.11-beta.46 (the concept's motion): a picture of its own with its texture - the view before a change (fading
    // out under the new one), the low HP warning (sliding in, fading out), a new level's gold edge (flaring once)
    struct Layer {
        panel::Picture picture;
        uint32_t texture = 0;
        int texW = 0, texH = 0, x = 0, y = 0;
        bool upload = false;
        std::string key;
    };
    Layer before, warning, flare;
    ULONGLONG changedAt = 0, warnAt = 0, levelAt = 0;
    int lastBase = 0, lastJob = 0;
    cb::View shownView = cb::View::Full;  // 3.0.11-beta.49: compact while the windows are open
    int hoverId = 0, pressedButton = -1;  // 3.0.11-beta.51: what is pointed at since when (its tip after 0.4 s)
    ULONGLONG hoverSince = 0, pressedUntil = 0;
    bool ammoArrows = true;  // 3.0.11-beta.114: the ammo worn arrows or other (win/ammo_watch)
    int overweightTold = -2;  // 3.0.11-beta.139: the overweight state last told the log
    // 3.0.11-beta.141 (the owner: «блок с персонажем должен работать от альт-V ... прогресбар с бейз опытом и джоб
    // опытом ... точное количество опыта. и побольше подробностей»): the small view Alt+V folds to (the last one used);
    // the experience gained (core/exp_rate) - whose, its pace once a second
    cb::View small = cb::View::Compact;
    exprate::Track baseTrack, jobTrack;
    std::string trackName;
    ULONGLONG paceAt = 0;
    double basePerHour = -1, jobPerHour = -1;
    int keysLogged = 0;
};

State& state() {
    static State s;
    return s;
}

std::string say(settings::Text t) { return settings::tr(language(), t); }

// 3.0.11-beta.139 (the owner: «пишет что 60% но хп воостанавливается»): the server's own overweight state - its status
// icons SI_WEIGHT90 (36: no attacks nor skills, 90 %) and SI_WEIGHT50 (35: no natural regen - uaRO's
// natural_heal_weight_rate is 70 %, not the classic 50)
int ownOverweight() {
    const buffs::Timers& t = buffoverlay::timers();
    return t.on(36) ? 2 : t.on(35) ? 1 : 0;
}

uint32_t read32(uint32_t at) {
    uint32_t v = 0;
    return readGame(at, &v, 4) ? v : 0;
}

// UI window 0 when it is the Basic Info window of this build
uint32_t basicInfo() {
    const uint32_t window = read32(kBasicInfo);
    return window && read32(window) == kBasicInfoClass ? window : 0;
}

bool write32(uint32_t at, uint32_t v) {
    SIZE_T n = 0;
    return WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(uintptr_t(at)), &v, 4, &n) && n == 4;
}

// Its shown flag held: 0 while the block shows (each frame - the game shows it again on a new map), 1 back
void holdGameWindow(State& s, bool hide) {
    const uint32_t window = basicInfo();
    if (!window) {
        if (hide && !s.oldTold) {
            s.oldTold = true;
            LOG_INFO("character: the game's Basic Info window is not where this build has it - it stays as it is");
        }
        return;
    }
    const uint32_t shown = read32(window + kShown);
    if (hide && shown != 0 && write32(window + kShown, 0)) {
        if (!s.hidOld) LOG_INFO("character: the game's Basic Info window hidden for the block (its shown flag 0)");
        s.hidOld = true;
    } else if (!hide && s.hidOld && shown == 0) {
        write32(window + kShown, 1);
        s.hidOld = false;
        LOG_INFO("character: the game's Basic Info window back");
    }
}

// One of the Basic Info window's buttons, as its own: its SendMsg (message 6) with the button's command
bool press(int command) {
    const uint32_t window = basicInfo();
    if (!window || read32(read32(window) + kSendMsgSlot) != kSendMsg) return false;
    reinterpret_cast<SendMsgFn>(uintptr_t(kSendMsg))(window, 0, kButtonMsg, command, 0, 0);
    return true;
}

void readState(State& s) {
    if (s.stateRead) return;
    s.stateRead = true;
    std::string text;
    if (!readTextFile(layout::join(gameDir(), layout::kCharacterState), text)) return;
    if (text.find("view compact") != std::string::npos) s.view = cb::View::Compact;
    if (text.find("view icon") != std::string::npos) s.view = cb::View::Icon;
    if (text.find("small icon") != std::string::npos) s.small = cb::View::Icon;  // 3.0.11-beta.141: Alt+V's
    if (s.view != cb::View::Full) s.small = s.view;
}

const char* viewToken(cb::View v) {
    return v == cb::View::Compact ? "compact" : v == cb::View::Icon ? "icon" : "full";
}

void keepState(const State& s) {
    const std::string path = layout::join(gameDir(), layout::kCharacterState);
    if (!writeFileAtomic(path, std::string("view ") + viewToken(s.view) + "\nsmall " + viewToken(s.small) + "\n"))
        LOG_INFO("character: %s cannot be written", path.c_str());
}

// Another view (a click on the portrait, Alt+V): the one before fades out under it (3.0.11-beta.46), kept for the
// next start
void changeView(State& s, cb::View v) {
    if (motion::on() && s.picture.width > 0) {
        s.before.picture = s.picture;
        s.before.x = s.layout.x;
        s.before.y = s.layout.y;
        s.before.upload = true;
        s.changedAt = GetTickCount64();
    }
    s.view = v;
    if (v != cb::View::Full) s.small = v;
    s.shownView = s.view;
    keepState(s);
    s.key.clear();
}

// The portrait: the head's first frame (standing, facing down) in its hair colour, fitted into `side` (3.0.11-beta.80:
// win/head_picture, the party's faces too)
void makePortrait(State& s, int side) {
    const int sex = int(read32(kSex)) != 0 ? 1 : 0;  // as FUN_0062e7b0: not 0 - a man
    const int head = int(read32(kHead));
    const int colour = int(read32(kHairColour));
    const std::string key = std::to_string(sex) + ':' + std::to_string(head) + ':' + std::to_string(colour) + ':' +
                            std::to_string(side);
    if (key == s.portraitKey) return;
    s.portraitKey = key;
    std::string from;
    if (headpicture::make(sex, head, colour, side, s.portrait, &from))
        LOG_INFO("character: the portrait - %s", from.c_str());
    else
        LOG_INFO("character: %s - the portrait the name's letter", from.c_str());
}

cb::Texts texts() {
    cb::Texts t;
    t.weight = say(settings::Text::CharWeight);
    t.points = say(settings::Text::CharPoints);
    t.lowHp = say(settings::Text::CharLowHp);
    t.ammo = say(state().ammoArrows ? settings::Text::CharArrows : settings::Text::CharAmmo);  // 3.0.11-beta.114
    t.overweight1 = say(settings::Text::CharOverweight1);  // 3.0.11-beta.139
    t.overweight2 = say(settings::Text::CharOverweight2);
    t.skillPoints = say(settings::Text::CharSkillPoints);  // 3.0.11-beta.141: the experience's words
    t.expMax = say(settings::Text::CharExpMax);
    t.hour = say(settings::Text::CharHour);
    t.minute = say(settings::Text::CharMinute);
    t.toLevel = say(settings::Text::CharToLevel);
    t.levelIn = say(settings::Text::CharLevelIn);
    t.perHour = say(settings::Text::CharPerHour);
    t.session = say(settings::Text::CharSession);
    t.lastGain = say(settings::Text::CharLastGain);
    t.fold = say(settings::Text::CharFold);
    t.unfold = say(settings::Text::CharUnfold);
    t.guild = say(settings::Text::CharGuild);  // 3.0.11-beta.235
    t.party = say(settings::Text::CharParty);
    t.regen = say(settings::Text::CharRegen);
    t.regenFull = say(settings::Text::CharRegenFull);
    t.regenToFull = say(settings::Text::CharRegenToFull);
    t.regenNone = say(settings::Text::CharRegenNone);
    t.sessionHead = say(settings::Text::CharSessionHead);
    t.killed = say(settings::Text::CharKilled);
    t.deaths = say(settings::Text::CharDeaths);
    t.tipWeight = say(settings::Text::CharTipWeight);
    t.tipPoints = say(settings::Text::CharTipPoints);
    t.tipSkillPoints = say(settings::Text::CharTipSkillPoints);
    t.tipRegen1 = say(settings::Text::CharTipRegen1);
    t.tipRegen2 = say(settings::Text::CharTipRegen2);
    t.tipAtk = say(settings::Text::CharTipAtk);
    t.tipDef = say(settings::Text::CharTipDef);
    t.tipFlee = say(settings::Text::CharTipFlee);
    t.tipAspd = say(settings::Text::CharTipAspd);
    t.tipOpen = say(settings::Text::CharTipOpen);
    t.tipTime = say(settings::Text::CharTipTime);
    t.tipZeny = say(settings::Text::CharTipZeny);
    t.tipKills = say(settings::Text::CharTipKills);
    t.tipDeaths = say(settings::Text::CharTipDeaths);
    t.tipOnline = say(settings::Text::CharTipOnline);
    const settings::Text names[cb::kButtons] = {settings::Text::CharStats,  settings::Text::CharEquip,
                                                settings::Text::CharItems,  settings::Text::CharSkills,
                                                settings::Text::CharFriends, settings::Text::CharOptions};
    for (int i = 0; i < cb::kButtons; ++i) t.buttons[i] = say(names[i]);
    t.decimal = language() == settings::Lang::En ? '.' : ',';
    return t;
}

std::vector<Words> gdiWords(const std::vector<cb::Words>& in) {
    std::vector<Words> out;
    for (const cb::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == cb::Align::Right    ? DT_RIGHT
                                 : w.align == cb::Align::Center ? DT_CENTER
                                                                : DT_LEFT),
                            false});
    return out;
}

// The warning's picture and the flare's: the block's size, its place (made again when what they show changes)
void renderWarning(State& s) {
    const uicost::Paint cost("character block's warning", s.layout.w, s.layout.h);  // 3.0.11-beta.55
    const cb::Texts t = texts();
    const std::string key = std::to_string(s.layout.w) + 'x' + std::to_string(s.layout.h) + ';' +
                            std::to_string(int(s.layout.view)) + ';' + std::to_string(s.data.hp) + '/' +
                            std::to_string(s.data.hpMost);
    if (key == s.warning.key) return;
    s.warning.key = key;
    panel::Picture& p = s.warning.picture;
    p = panel::Picture{};
    p.width = s.layout.w;
    p.height = s.layout.h;
    p.px.assign(size_t(p.width) * size_t(p.height), 0u);
    cb::paintWarning(p, s.layout, s.data, t);
    drawWords(p, gdiWords(cb::warningWords(s.layout, s.data, t)));
    s.warning.upload = true;
}

void renderFlare(State& s) {
    const uicost::Paint cost("character block's flare", s.layout.w, s.layout.h);  // 3.0.11-beta.55
    const std::string key = std::to_string(s.layout.w) + 'x' + std::to_string(s.layout.h) + ';' +
                            std::to_string(int(s.layout.view)) + ',' + std::to_string(int(s.layout.look));
    if (key == s.flare.key) return;
    s.flare.key = key;
    panel::Picture& p = s.flare.picture;
    p = panel::Picture{};
    p.width = s.layout.w;
    p.height = s.layout.h;
    p.px.assign(size_t(p.width) * size_t(p.height), 0u);
    cb::paintFlare(p, s.layout);
    s.flare.upload = true;
}

void render(State& s) {
    const uicost::Paint cost("character block", s.layout.w, s.layout.h);  // 3.0.11-beta.55
    if (!panel::blank(s.picture, s.layout.w, s.layout.h)) return;  // 3.0.11-beta.57: in the memory it had
    const cb::Texts t = texts();
    cb::paint(s.picture, s.layout, s.data, t, s.show);
    drawWords(s.picture, gdiWords(cb::words(s.layout, s.data, t, s.show)));
    s.upload = true;
}

void block(const State& s, HWND game) {  // the mouse's clicks over it are its own, not the game's
    RECT client{};
    if (s.targetW <= 0 || s.targetH <= 0 || !GetClientRect(game, &client)) return;
    const panel::Box& b = s.layout.block;
    const float x0 = float(s.layout.x) + b.x, y0 = float(s.layout.y) + b.y;
    const RECT r{LONG(int64_t(x0) * client.right / s.targetW), LONG(int64_t(y0) * client.bottom / s.targetH),
                 LONG(int64_t(x0 + b.w) * client.right / s.targetW + 1),
                 LONG(int64_t(y0 + b.h) * client.bottom / s.targetH + 1)};
    uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Character, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Character, nullptr, nullptr, false);
    s.shown = false;
}

float share(int value, int next, int level) {
    if (next > 0) return std::clamp(float(value) / float(next), 0.0f, 1.0f);
    return level > 0 ? 1.0f : -1.0f;  // next 0: the last level
}

}  // namespace

void frame(HWND game, CharacterLook look, unsigned codePage, double k, int targetWidth, int targetHeight) {
    State& s = state();
    if (look != s.look) {
        LOG_INFO("character: [interface] character = %s", characterLookToken(look));
        s.look = look;
        s.key.clear();
    }
    const bool on = look != CharacterLook::Off;
    const bool world = inGameWorld();
    if (world) holdGameWindow(s, on);
    if (!on || !world || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        hide(s);
        s.lastBase = s.lastJob = 0;  // another character next: no flare for its levels
        return;
    }
    readState(s);
    charstatus::Status st;
    if (!readOwnStatus(st)) {
        hide(s);
        return;
    }
    // 3.0.11-beta.75: its size in «Розміщення інтерфейсу»
    const float scale = uistyle::partScale(k) * hudedit::sizeOf(hudlayout::Part::Character);
    s.k = float(k);
    s.scale = scale;
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    const cb::Look panelLook = look == CharacterLook::Rings ? cb::Look::Rings : cb::Look::Bars;
    // 3.0.11-beta.186 (the tester: «кнопка "статуси" згортає разом з собою і меню персонажа, інші так не роблять»):
    // the block as the player set it while windows are open too - beta.49 folded it to compact under «Речі»,
    // «Характеристики» and «Довідник» alone, its buttons gone with it (the one that opened the window among them)
    const cb::View view = s.view;
    if (view != s.shownView) {
        if (motion::on() && s.picture.width > 0) {  // the look before fades out, as after a click
            s.before.picture = s.picture;
            s.before.x = s.layout.x;
            s.before.y = s.layout.y;
            s.before.upload = true;
            s.changedAt = GetTickCount64();
        }
        s.shownView = view;
    }
    // 3.0.11-beta.235: the session's rows at the last levels (as the last frame's numbers had them)
    s.layout = cb::layout(view, panelLook, int(std::lround(10 * scale)), int(std::lround(10 * scale)), scale,
                          cb::lastLevels(s.data));
    hudedit::place(hudlayout::Part::Character, float(k), targetWidth, targetHeight, s.layout.w, s.layout.h, s.layout.x,
                   s.layout.y);  // 3.0.11-beta.75: where the player put it
    makePortrait(s, int(std::lround(s.layout.portrait.w * 0.86f)));
    cb::Data& d = s.data;
    const std::string own = readOwnName();  // 3.0.11-beta.117: the game's code page's letters, as UTF-8
    d.name = own.empty() ? std::string() : utf8Of(widened(own, codePage));
    d.job = cb::jobName(int(read32(kJob)));
    d.baseLevel = st.baseLevel;
    d.jobLevel = st.jobLevel;
    d.hp = st.hp;
    d.hpMost = st.hpMost;
    d.sp = st.sp;
    d.spMost = st.spMost;
    d.baseExp = share(st.baseExp, st.baseNext, st.baseLevel);
    d.jobExp = share(st.jobExp, st.jobNext, st.jobLevel);
    d.weight = st.weightMost > 0 ? int(int64_t(st.weight) * 100 / st.weightMost) : -1;
    d.weightNow = st.weight;  // 3.0.11-beta.139: the numbers and the server's own overweight state
    d.weightMost = st.weightMost;
    d.overweight = ownOverweight();
    if (d.overweight != s.overweightTold && d.weight >= 0) {
        s.overweightTold = d.overweight;
        LOG_INFO("character: weight %d / %d (%d %%) - the server's overweight state %d (%s)", st.weight,
                 st.weightMost, d.weight, d.overweight,
                 d.overweight == 2 ? "90 %: no attacks nor skills" : d.overweight == 1 ? "no natural regen" : "none");
    }
    d.zeny = st.zeny;
    d.statusPoints = st.statusPoints;
    d.skillPoints = st.skillPoints;  // 3.0.11-beta.141
    d.portrait = s.portrait.width > 0 ? &s.portrait : nullptr;
    const ULONGLONG now = GetTickCount64();
    // 3.0.11-beta.141: the experience in the game's numbers, gained as it plays (another character: counted anew), its
    // pace an hour counted once a second
    if (d.name != s.trackName) {
        s.trackName = d.name;
        s.baseTrack.reset();
        s.jobTrack.reset();
        s.paceAt = 0;
        s.sessionAt = now;  // 3.0.11-beta.235: another character - its own session
        s.zenyAt = st.zeny;
        s.hit.clear();
        s.kills = s.deaths = 0;
    }
    s.baseTrack.feed(now, st.baseLevel, st.baseExp, st.baseNext);
    s.jobTrack.feed(now, st.jobLevel, st.jobExp, st.jobNext);
    if (now - s.paceAt >= 1000) {
        s.paceAt = now;
        s.basePerHour = s.baseTrack.perHour(now);
        s.jobPerHour = s.jobTrack.perHour(now);
    }
    d.baseNow = st.baseLevel > 0 ? st.baseExp : -1;
    d.baseNext = st.baseLevel > 0 ? st.baseNext : -1;
    d.jobNow = st.jobLevel > 0 ? st.jobExp : -1;
    d.jobNext = st.jobLevel > 0 ? st.jobNext : -1;
    d.basePerHour = s.basePerHour;
    d.jobPerHour = s.jobPerHour;
    d.baseSession = s.baseTrack.session();
    d.jobSession = s.jobTrack.session();
    d.baseLast = s.baseTrack.last();
    d.jobLast = s.jobTrack.last();
    // 3.0.11-beta.235: the guild and the party, the natural regen (uAthena status_calc_regen: HP 1 + VIT/5 + MaxHP/200
    // each 6 s, SP 1 + INT/6 + MaxSP/100 each 8 s, more past INT 120), the battle's numbers, this session
    const auto text = [&](uint32_t ptr, uint32_t len) -> std::string {
        if (!ptr || !len || len > 40) return "";
        char buf[41] = {};
        return readGame(ptr, buf, len) ? utf8Of(widened(std::string(buf, len), codePage)) : "";
    };
    d.guild = text(read32(kGuildName), read32(kGuildName + 4));
    {
        char party[25] = {};
        const uint32_t at = read32(kPartyName);
        d.party =
            at && readGame(at, party, 24) ? utf8Of(widened(std::string(party, strnlen(party, 24)), codePage)) : "";
        d.partyOnline = d.partyTotal = 0;
        if (!d.party.empty()) partywin::counts(d.partyOnline, d.partyTotal);
    }
    const int vit = st.statBase[2] + st.statBonus[2], intel = st.statBase[3] + st.statBonus[3];
    d.hpRegen = st.hpMost > 0 ? 1 + vit / 5 + st.hpMost / 200 : -1;
    d.spRegen = st.spMost > 0 ? 1 + intel / 6 + st.spMost / 100 + (intel >= 120 ? (intel - 120) / 2 + 4 : 0) : -1;
    d.hpEverySec = kHpEverySec;
    d.spEverySec = kSpEverySec;
    d.atk = st.atk;
    d.atk2 = st.atk2;
    d.def = st.def;
    d.def2 = st.def2;
    d.flee = st.flee;
    d.flee2 = st.flee2;
    d.aspd = st.aspd;
    d.sessionMinutes = s.sessionAt ? (long long)((now - s.sessionAt) / 60000) : -1;
    d.zenyGain = s.zenyAt >= 0 ? (long long)st.zeny - s.zenyAt : 0;
    d.kills = s.kills;
    d.deaths = s.deaths;
    // low HP: a short warning as it falls under a quarter
    const ammowatch::Ammo& ammo = ammowatch::read(now);  // 3.0.11-beta.114 (.125: win/ammo_watch)
    d.ammo = ammo.count;
    s.ammoArrows = ammo.arrows;
    const bool low = cb::lowHp(d);
    if (low && !s.wasLow) {
        s.warnAt = now;
        s.warnUntil = now + kWarnMs;
        LOG_INFO("character: low HP - %d of %d", d.hp, d.hpMost);
    }
    s.wasLow = low;
    if (s.lastBase > 0 && (d.baseLevel > s.lastBase || d.jobLevel > s.lastJob)) {  // a new level: the gold edge
        s.levelAt = now;
        LOG_INFO("character: a new level - Base %d, Job %d%s", d.baseLevel, d.jobLevel,
                 motion::on() ? ": the block's edge flares" : "");
    }
    s.lastBase = d.baseLevel;
    s.lastJob = d.jobLevel;
    cb::Show show;
    show.warning = now < s.warnUntil;
    show.warningApart = true;  // 3.0.11-beta.46: its own picture
    int cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) {
        const cb::Hit h = cb::hitTest(s.layout, float(cx) * float(k) - float(s.layout.x),
                                      float(cy) * float(k) - float(s.layout.y));
        if (h.part == cb::Part::Button) show.hoverButton = h.index;
        if (h.part == cb::Part::Exp) show.hoverExp = h.index;  // 3.0.11-beta.141: an experience row's tip
        if (h.part == cb::Part::Zone) show.hoverZone = h.index;  // 3.0.11-beta.235: a part's tip
        show.hoverPortrait = h.part == cb::Part::Portrait;     // 3.0.11-beta.141: Alt+V in its tip
        show.hoverBlock = h.part != cb::Part::None;
    }
    const int hoverId = show.hoverButton >= 0 ? 100 + show.hoverButton
                        : show.hoverExp >= 0  ? 200 + show.hoverExp
                        : show.hoverZone >= 0 ? 300 + show.hoverZone
                        : show.hoverPortrait  ? 2
                        : show.hoverBlock     ? 1
                                              : 0;
    if (hoverId != s.hoverId) {
        s.hoverId = hoverId;
        s.hoverSince = now;
    }
    show.tip = hoverId != 0 && now - s.hoverSince >= kTipMs;
    show.pressedButton = now < s.pressedUntil ? s.pressedButton : -1;
    s.show = show;
    const std::string key =
        std::to_string(int(view)) + ';' + std::to_string(int(panelLook)) + ';' + std::to_string(scale) + ';' +
        d.name + ';' + d.job + ';' + std::to_string(d.baseLevel) + ',' + std::to_string(d.jobLevel) + ';' +
        std::to_string(d.hp) + '/' + std::to_string(d.hpMost) + ',' + std::to_string(d.sp) + '/' +
        std::to_string(d.spMost) + ';' + std::to_string(int(d.baseExp * 1000)) + ',' +
        std::to_string(int(d.jobExp * 1000)) + ';' + std::to_string(d.weight) + ';' + std::to_string(d.zeny) + ';' +
        std::to_string(d.statusPoints) + ';' + std::to_string(d.ammo) + (s.ammoArrows ? "a" : "b") + ';' +
        std::to_string(d.weightNow) + '/' + std::to_string(d.weightMost) + 'o' + std::to_string(d.overweight) + ';' +
        s.portraitKey + (d.portrait ? "p" : "") + ';' +
        std::to_string(show.hoverButton) + (show.hoverBlock ? "h" : "") + (show.warning ? "w" : "") +
        (show.tip ? "t" : "") + std::to_string(show.pressedButton) + ';' +
        // 3.0.11-beta.141: the experience's numbers, its pace (once a second), the gains, the rows and the portrait
        std::to_string(d.baseNow) + '/' + std::to_string(d.baseNext) + ',' + std::to_string(d.jobNow) + '/' +
        std::to_string(d.jobNext) + ';' + std::to_string(std::llround(d.basePerHour)) + ',' +
        std::to_string(std::llround(d.jobPerHour)) + ';' + std::to_string(d.baseSession) + ',' +
        std::to_string(d.jobSession) + ',' + std::to_string(d.baseLast) + ',' + std::to_string(d.jobLast) + ';' +
        std::to_string(d.skillPoints) + 'e' + std::to_string(show.hoverExp) + (show.hoverPortrait ? "f" : "") + ';' +
        // 3.0.11-beta.235: the social line, the regen, the battle, the session, the part pointed at
        d.guild + '|' + d.party + std::to_string(d.partyOnline) + '/' + std::to_string(d.partyTotal) + ';' +
        std::to_string(d.hpRegen) + ',' + std::to_string(d.spRegen) + ';' + std::to_string(d.atk) + ',' +
        std::to_string(d.atk2) + ',' + std::to_string(d.def) + ',' + std::to_string(d.def2) + ',' +
        std::to_string(d.flee) + ',' + std::to_string(d.flee2) + ',' + std::to_string(d.aspd) + ';' +
        std::to_string(d.sessionMinutes) + ',' + std::to_string(d.zenyGain) + ',' + std::to_string(d.kills) + ',' +
        std::to_string(d.deaths) + 'z' + std::to_string(show.hoverZone);
    if (const std::string drawn = key + '#' + std::to_string(uistyle::generation()); drawn != s.key) {  // its grounds
        s.key = drawn;
        render(s);
    }
    if (show.warning) renderWarning(s);
    if (now - s.levelAt < motion::kLevelMs) renderFlare(s);
    s.shown = true;
    block(s, game);
}

bool panelBox(int& x, int& y, int& w, int& h) {
    const State& s = state();
    if (!s.shown) return false;
    x = s.layout.x + int(s.layout.block.x);
    y = s.layout.y + int(s.layout.block.y);
    w = int(std::ceil(s.layout.block.w));
    h = int(std::ceil(s.layout.block.h));
    return true;
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.shown || s.k <= 0 || s.picture.width <= 0 || s.picture.height <= 0) return;
    int cursor = -1, cx = 0, cy = 0;  // under the game's own cursor, as the minimap
    if (readGameCursor(cx, cy)) cursor = panel::cursorDraw(record, float(cx), float(cy));
    const ULONGLONG now = GetTickCount64();
    const auto layer = [&](State::Layer& l, int x, int y, float a) {
        drawPart(record, renderer, l.picture, l.texture, l.texW, l.texH, l.upload, x, y, s.k, a);
    };
    // 3.0.11-beta.46: a view changed - the one before fading out under the new one coming in (160 ms)
    const float a = motion::fadeIn(now, s.changedAt, motion::kBlockMs);
    if (a < 1 && s.before.picture.width > 0) layer(s.before, s.before.x, s.before.y, 1 - a);
    drawPart(record, renderer, s.picture, s.texture, s.texW, s.texH, s.upload, s.layout.x, s.layout.y, s.k, a);
    if (now < s.warnUntil && s.warning.picture.width > 0) {  // from 8 px over its place in 180 ms; its last 240 out
        float w = 1, shift = 0;
        const ULONGLONG outAt = s.warnUntil - motion::kToastOutMs;
        if (motion::on() && now < outAt) {
            w = motion::fadeIn(now, s.warnAt, motion::kToastInMs);
            shift = -(1 - w) * 8 * s.scale;
        } else if (motion::on()) {
            w = motion::fadeOut(now, outAt, motion::kToastOutMs);
        }
        if (w > 0) layer(s.warning, s.layout.x, s.layout.y + int(std::lround(shift)), w);
    }
    const float f = motion::flare(now, s.levelAt, motion::kLevelMs);  // a new level: once, 600 ms
    if (f > 0 && s.flare.picture.width > 0) layer(s.flare, s.layout.x, s.layout.y, f);
    if (cursor >= 0) record.moveLastBefore(record.commands().size() - size_t(cursor) - 1, size_t(cursor));
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    if (!s.shown) return false;
    const bool button = m == WM_LBUTTONDOWN || m == WM_LBUTTONUP || m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDOWN ||
                        m == WM_RBUTTONUP || m == WM_RBUTTONDBLCLK || m == WM_MBUTTONDOWN || m == WM_MBUTTONUP;
    if (!button && m != WM_MOUSEWHEEL) return false;
    (void)wp;
    // 3.0.11-beta.235: the press where the game's own cursor stands, as the hover (game_camera pointerInTarget)
    float tx = 0, ty = 0;
    if (!pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty)) return false;
    const cb::Hit h = cb::hitTest(s.layout, tx - float(s.layout.x), ty - float(s.layout.y));
    if (h.part == cb::Part::None) return false;
    if (m == WM_LBUTTONDOWN) {
        if (h.part == cb::Part::Portrait) {  // full, compact, the icon, full again
            changeView(s, cb::nextView(s.view));
            LOG_INFO("character: the block %s", s.view == cb::View::Full      ? "full"
                                                : s.view == cb::View::Compact ? "compact"
                                                                              : "folded to its icon");
        } else if (h.part == cb::Part::Zone && h.index >= 0 &&
                   (cb::opensStats(cb::Zone(h.index)) || h.index == int(cb::Zone::Points))) {
            // 3.0.11-beta.235: the battle's numbers - «Характеристики»
            const bool sent = press(cb::kButton[0].command);
            if (s.clicksLogged++ < 10)
                LOG_INFO("character: the battle's line - «Характеристики» %s", sent ? "opened" : "not opened");
        } else if (h.part == cb::Part::Button && h.index >= 0 && h.index < cb::kButtons) {
            s.pressedButton = h.index;  // 3.0.11-beta.51: shown pressed a moment
            s.pressedUntil = GetTickCount64() + kPressMs;
            const bool sent = press(cb::kButton[h.index].command);
            if (s.clicksLogged++ < 10)
                LOG_INFO("character: the Basic Info window's button 0x%X %s", cb::kButton[h.index].command,
                         sent ? "pressed through the window"
                              : "not pressed - the window is not where this build has it");
        }
    }
    result = 0;
    return true;
}

bool key(UINT m, WPARAM wp, LPARAM lp) {
    State& s = state();
    if (!s.shown || m != WM_SYSKEYDOWN || wp != 'V' || !(lp & (1 << 29))) return false;  // Alt+V (Alt down: bit 29)
    if (lp & (1 << 30)) return true;  // held down: once
    changeView(s, cb::altV(s.shownView, s.small));
    if (s.keysLogged++ < 20)
        LOG_INFO("character: Alt+V - the block %s (the game's own Basic Info key, not given the game)",
                 s.view == cb::View::Full ? "full" : s.view == cb::View::Compact ? "compact" : "folded to its icon");
    return true;
}

void beforeGameDraw() {
    State& s = state();
    if (s.look != CharacterLook::Off && inGameWorld()) holdGameWindow(s, true);
}

bool pressGame(int command) { return press(command); }

bool gameOpen(int command) {
    const uint32_t at = command == 0x81   ? kWindowMgr + 0x94
                        : command == 0x82 ? kWindowMgr + 0x118
                        : command == 0x7F ? kWindowMgr + 0x98
                        : command == 0x85 ? kWindowMgr + 0x11C
                                          : 0;
    return at && inGameWorld() && read32(at) != 0;
}

// 3.0.11-beta.235: this session's kills and deaths - a monster the character hit (0x008a, its skills) that died
// (0x0080 type 1), the character's own death
void packet(const packets::Event& e) {
    State& s = state();
    const uint32_t me = read32(kOwnAccount);
    if (!me) return;
    if (const auto* h = std::get_if<packets::Hit>(&e)) {
        if (h->source == me && h->target != me && h->target) {
            if (s.hit.size() > 256) s.hit.clear();
            s.hit.insert(h->target);
        }
    } else if (const auto* v = std::get_if<packets::Vanish>(&e)) {
        if (v->type != 1) return;
        if (v->id == me) {
            ++s.deaths;
        } else if (s.hit.erase(v->id)) {
            ++s.kills;
        }
    }
}

bool expInfo(charblock::Data& d, charblock::Texts& t) {
    const State& s = state();
    if (!s.shown || s.data.baseLevel <= 0) return false;
    d = s.data;
    t = texts();
    return true;
}

}  // namespace uagfx::win::charpanel

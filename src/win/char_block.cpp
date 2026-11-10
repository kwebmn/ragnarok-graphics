#include "win/upkeep_watch.hpp"
#include "win/char_block.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <map>
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
#include "win/exp_gain.hpp"
#include "win/game_camera.hpp"
#include "win/head_picture.hpp"
#include "win/hud_edit.hpp"
#include "win/log.hpp"
#include "win/net_predict.hpp"
#include "win/party_panel.hpp"
#include "win/paths.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/updates_hook.hpp"
#include "win/uaui.hpp"
#include "core/ui_icons.hpp"

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
// 3.0.11-beta.238: the session's skills (a std::list: its head at +0x2D8, a node's record at +8 - its id the second
// int, its level the fourth, as «Навички» reads them); the regen skills' ids (uAthena skill_db); the last fight's cost
// shown this long, its last second fading
constexpr uint32_t kSkillList = 0x771148 + 0x2D8;
constexpr int kHpRecovery = 4, kSpRecovery = 9, kSpiritualCadence = 260, kMeditatio = 363, kNinpou = 533;
constexpr ULONGLONG kCostMs = 4000, kCostFadeMs = 1000, kFightIdleMs = 8000;
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
    // 3.0.11-beta.238: the regen skills' levels (read once a second), the fight under way (HP and SP at its first hit,
    // the last hit), the last fight's cost and when
    std::map<int, int> skills;
    ULONGLONG skillsAt = 0;
    bool fight = false;
    int fightHp = 0, fightSp = 0;
    ULONGLONG lastHitAt = 0, costAt = 0;
    float hpCost = -1, spCost = -1;
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
    bool uiShown = false;  // 3.0.18-beta.17: drawn by uaUI this frame
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
    t.broken = say(settings::Text::CharBroken);  // 3.0.16-beta.13
    t.pet = say(settings::Text::CharPet);
    t.homun = say(settings::Text::CharHomun);
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
    t.regenPerMin = say(settings::Text::CharRegenPerMin);  // 3.0.11-beta.238
    t.standing = say(settings::Text::CharStanding);
    t.sitting = say(settings::Text::CharSitting);
    t.regenNatural = say(settings::Text::CharRegenNatural);
    t.regenSkill = say(settings::Text::CharRegenSkill);
    t.regenSitSkill = say(settings::Text::CharRegenSitSkill);
    t.fightCost = say(settings::Text::CharFightCost);
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
    panel::blank(p, s.layout.w, s.layout.h);  // 3.0.18-beta.14: recorded for uaUI while it is on
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
    panel::blank(p, s.layout.w, s.layout.h);  // 3.0.18-beta.14
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


// ---- 3.0.18-beta.17 (the owner: «Переписывать 73 части в разметку RmlUi вручную»): the block as an RmlUi document
// while [interface] uaui is on - the same words (core/char_block model), its places the layout's (concept px = dp),
// the clicks through RmlUi's events into press() and changeView(); its picture not painted then ----

const char* kDoc = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="cb" data-model="charblock" data-style-width="w + 'dp'" data-style-height="h + 'dp'">
<div class="card" data-style-width="w + 'dp'" data-style-height="h + 'dp'" data-if="view != 2 && !(view == 1 && look == 1)"></div>
<div class="zone" style="left: 0; top: 0;" data-style-width="w + 'dp'" data-style-height="h + 'dp'"
     data-event-mouseover="hover(3, 0)" data-event-mouseout="unhover()"></div>
<div data-if="view == 0">
 <div class="portrait" style="left: 12dp; top: 12dp; width: 54dp; height: 54dp;" data-event-click="portrait()"
      data-event-mouseover="hover(4, 0)" data-event-mouseout="unhover()">
  <img data-if="portrait != ''" data-attr-src="portrait"/>
  <span class="letter" style="font-size: 22dp; line-height: 54dp;" data-if="portrait == ''">{{letter}}</span></div>
 <div class="line" style="left: 78dp; top: 14dp; height: 20dp;"><span class="name">{{name}}</span>
  <span class="job">{{job_name}}</span></div>
 <span class="levels">{{levels}}</span>
 <div data-if="weight_shown">
  <span class="line muted" style="left: 78dp; top: 36dp;">{{weight_label}}</span>
  <div class="track" style="left: 118dp; top: 42dp; width: 60dp; height: 5dp;"><div class="fill weightfill" data-style-width="weight_w + '%'"></div></div>
  <span class="line" style="left: 184dp; top: 36dp; font-weight: bold;" data-attr-class="'line ' + weight_tone">{{weight_pct}}</span>
 </div>
 <span class="line" style="left: 234dp; top: 36dp; font-weight: bold;" data-if="zeny_shown">{{zeny}} <span class="muted" style="font-weight: normal;">z</span></span>
 <div class="line" style="left: 78dp; top: 54dp;"><span class="chip" data-for="c : social" data-attr-class="'chip ' + c.tone">{{c.text}}</span></div>
 <div class="line" style="left: 78dp; top: 71dp;"><span class="chip" data-for="c : points" data-attr-class="'chip ' + c.tone">{{c.text}}</span></div>
 <span class="label" style="left: 12dp; top: 94dp;">HP</span>
 <div class="track" style="left: 48dp; top: 97dp; width: 180dp; height: 10dp;"><div class="fill hp" data-class-low="low" data-style-width="hp_w + '%'"></div></div>
 <span class="num" style="left: 236dp; top: 94dp; width: 124dp;" data-class-low="low" data-if="hp_shown">{{hp}}</span>
 <span class="small" style="left: 48dp; top: 109dp;" data-attr-class="'small ' + hp_regen.tone">{{hp_regen.text}}</span>
 <span class="small" style="right: 12dp; top: 109dp;" data-attr-class="'small ' + hp_regen_right.tone">{{hp_regen_right.text}}</span>
 <span class="label" style="left: 12dp; top: 126dp;">SP</span>
 <div class="track" style="left: 48dp; top: 129dp; width: 180dp; height: 10dp;"><div class="fill sp" data-style-width="sp_w + '%'"></div></div>
 <span class="num" style="left: 236dp; top: 126dp; width: 124dp;" data-if="sp_shown">{{sp}}</span>
 <span class="small" style="left: 48dp; top: 141dp;" data-attr-class="'small ' + sp_regen.tone">{{sp_regen.text}}</span>
 <span class="small" style="right: 12dp; top: 141dp;" data-attr-class="'small ' + sp_regen_right.tone">{{sp_regen_right.text}}</span>
 <div class="cell" style="top: 160dp; width: 82dp; height: 32dp;" data-for="c, i : battle" data-style-left="12 + i * 88.5 + 'dp'"
      data-event-mouseover="hover(0, 6 + i)" data-event-mouseout="unhover()" data-event-click="zone(6 + i)">
  <span class="k">{{c.key}}</span><span class="v" data-attr-class="'v ' + c.tone">{{c.value}}</span></div>
 <div data-if="!session_shown">
  <div data-if="base.shown" data-event-mouseover="hover(1, 0)" data-event-mouseout="unhover()">
   <span class="label" style="left: 12dp; top: 200dp;">{{base.label}}</span>
   <div class="track" style="left: 48dp; top: 205dp; width: 180dp; height: 6dp;"><div class="fill base" data-style-width="base.w + '%'"></div></div>
   <span class="num muted" style="left: 236dp; top: 200dp; width: 124dp; font-size: 12dp;">{{base.pct}}</span>
   <span class="small muted" style="left: 48dp; top: 216dp;">{{base.numbers}}</span>
   <span class="small" style="right: 12dp; top: 216dp;" data-attr-class="'small ' + base.right_tone">{{base.right}}</span>
  </div>
  <div data-if="job.shown" data-event-mouseover="hover(1, 1)" data-event-mouseout="unhover()">
   <span class="label" style="left: 12dp; top: 234dp;">{{job.label}}</span>
   <div class="track" style="left: 48dp; top: 239dp; width: 180dp; height: 6dp;"><div class="fill jobx" data-style-width="job.w + '%'"></div></div>
   <span class="num muted" style="left: 236dp; top: 234dp; width: 124dp; font-size: 12dp;">{{job.pct}}</span>
   <span class="small muted" style="left: 48dp; top: 250dp;">{{job.numbers}}</span>
   <span class="small" style="right: 12dp; top: 250dp;" data-attr-class="'small ' + job.right_tone">{{job.right}}</span>
  </div>
 </div>
 <div data-if="session_shown">
  <span class="small muted" style="left: 12dp; top: 200dp; font-weight: bold; font-size: 10.5dp;"
        data-event-mouseover="hover(0, 10)" data-event-mouseout="unhover()">{{session_head}}</span>
  <div class="cell" style="top: 218dp; width: 112dp; height: 32dp;" data-for="c, i : session"
       data-style-left="12 + i * 118 + 'dp'" data-event-mouseover="hover(0, 11 + i)" data-event-mouseout="unhover()">
   <span class="k">{{c.key}}</span><span class="v" data-attr-class="'v ' + c.tone">{{c.value}}</span></div>
 </div>
 <div class="zone" style="left: 78dp; top: 36dp; width: 150dp; height: 17dp;" data-event-mouseover="hover(0, 0)" data-event-mouseout="unhover()"></div>
 <div class="zone" style="left: 234dp; top: 36dp; width: 126dp; height: 17dp;" data-event-mouseover="hover(0, 1)" data-event-mouseout="unhover()"></div>
 <div class="zone" style="left: 78dp; top: 54dp; width: 282dp; height: 16dp;" data-event-mouseover="hover(0, 2)" data-event-mouseout="unhover()"></div>
 <div class="zone" style="left: 78dp; top: 71dp; width: 282dp; height: 16dp;" data-event-mouseover="hover(0, 3)" data-event-mouseout="unhover()" data-event-click="zone(3)"></div>
 <div class="zone" style="left: 48dp; top: 109dp; width: 312dp; height: 13dp;" data-event-mouseover="hover(0, 4)" data-event-mouseout="unhover()"></div>
 <div class="zone" style="left: 48dp; top: 141dp; width: 312dp; height: 13dp;" data-event-mouseover="hover(0, 5)" data-event-mouseout="unhover()"></div>
 <div class="button" style="width: 32dp; height: 26dp;" data-for="b, i : buttons" data-style-left="12 + i * 38 + 'dp'"
      data-style-top="h - 38 + 'dp'" data-event-click="press(i)" data-event-mouseover="hover(2, i)" data-event-mouseout="unhover()">
  <img data-attr-src="b"/></div>
</div>
<div data-if="view == 1 && look == 0">
 <div class="portrait" style="left: 10dp; top: 10dp; width: 46dp; height: 46dp;" data-event-click="portrait()"
      data-event-mouseover="hover(4, 0)" data-event-mouseout="unhover()">
  <img data-if="portrait != ''" data-attr-src="portrait"/>
  <span class="letter" style="font-size: 19dp; line-height: 46dp;" data-if="portrait == ''">{{letter}}</span></div>
 <div class="track" style="left: 66dp; top: 14dp; width: 222dp; height: 10dp;"><div class="fill hp" data-class-low="low" data-style-width="hp_w + '%'"></div></div>
 <div class="track" style="left: 66dp; top: 29dp; width: 222dp; height: 8dp;"><div class="fill sp" data-style-width="sp_w + '%'"></div></div>
 <span class="num" style="left: 66dp; top: 42dp; text-align: left;" data-class-low="low">{{hp_compact}}</span>
 <span class="num" style="left: 158dp; top: 42dp; width: 130dp;">{{sp_compact}}</span>
 <div class="track" style="left: 10dp; top: 62dp; width: 280dp; height: 3dp;"><div class="fill base" data-style-width="base.w + '%'"></div></div>
 <div class="track" style="left: 10dp; top: 67dp; width: 280dp; height: 3dp;"><div class="fill jobx" data-style-width="job.w + '%'"></div></div>
</div>
<div data-if="view == 2 || (view == 1 && look == 1)">
 <div class="abs" style="left: 0; top: 0; border-radius: 999px; background-color: #00000080;"
      data-style-width="ring_d + 'dp'" data-style-height="ring_d + 'dp'"></div>
 <ua-ring class="abs" style="left: 0; top: 0;" data-style-width="ring_d + 'dp'" data-style-height="ring_d + 'dp'"
          data-attr-width="ring_hp_w" data-attr-value="hp_w / 100" data-attr-fill="low ? '#ff5a4e' : '#5bd36c'" track="#ffffff1f"></ua-ring>
 <ua-ring class="abs" data-style-left="ring_gap + 'dp'" data-style-top="ring_gap + 'dp'"
          data-style-width="ring_d - 2 * ring_gap + 'dp'" data-style-height="ring_d - 2 * ring_gap + 'dp'"
          data-attr-width="ring_sp_w" data-attr-value="sp_w / 100" fill="#4f8bff" track="#ffffff14"></ua-ring>
 <div class="portrait" data-style-left="ring_d / 2 - ring_pic + 'dp'" data-style-top="ring_d / 2 - ring_pic + 'dp'"
      data-style-width="2 * ring_pic + 'dp'" data-style-height="2 * ring_pic + 'dp'" data-event-click="portrait()"
      data-event-mouseover="hover(3, 0)" data-event-mouseout="unhover()">
  <img data-if="portrait != ''" data-attr-src="portrait"/>
  <span class="letter" data-if="portrait == ''" data-style-line-height="2 * ring_pic + 'dp'">{{letter}}</span></div>
 <span class="badge" data-style-left="ring_d / 2 - ring_badge_w / 2 + 'dp'" data-style-top="ring_d - ring_badge_h * 0.6 - ring_gap + 'dp'"
       data-style-width="ring_badge_w + 'dp'" data-style-height="ring_badge_h + 'dp'" data-style-line-height="ring_badge_h + 'dp'">{{badge}}</span>
</div>
<span class="warning away" id="warning" data-style-top="h + 8 + 'dp'" style="left: 0;">{{low_text}}</span>
<div class="tip" data-if="tip.size > 0" data-style-left="tip_left + 'dp'" data-style-top="tip_top + 'dp'">
 <p data-for="l, i : tip" data-class-first="i == 0">{{l}}</p></div>
</body></rml>)RML";

struct Ui {
    uaui::Doc doc;
    cb::Model m;
    int view = 0, look = 0;
    float w = 372, h = 312, hpW = 0, spW = 0, weightW = 0;
    struct Exp {
        bool shown = false;
        Rml::String label, pct, numbers, right, rightTone;
        float w = 0;
    } base, job;
    std::vector<Rml::String> buttons, tip;
    Rml::String portrait, portraitSrc;
    float tipLeft = 0, tipTop = 0;
    float ringD = 98, ringGap = 7, ringPic = 31, ringBadgeW = 32, ringBadgeH = 18, ringHpW = 5, ringSpW = 4;
    std::string key;
    int portraitGen = 0;
    std::string portraitKey;
    panel::Picture icons[cb::kButtons];
    float iconsScale = 0;
    int iconsGen = 0;
    std::string iconSrc[cb::kButtons];
    bool warningShown = false;
    int hoverKind = -1, hoverIndex = -1;
};

Ui& ui() {
    static Ui u;
    return u;
}

void tipFor(int kind, int index) {  // the tip of what the mouse is over (kind: 0 zone, 1 exp row, 2 button, 3 body,
    Ui& u = ui();                    // 4 portrait)
    u.hoverKind = kind, u.hoverIndex = index;
    std::vector<std::string> lines;
    if (kind == 0 && index >= 0 && index < cb::kZones) lines = u.m.zoneTips[index];
    else if (kind == 1) lines = index == 0 ? u.m.baseExp.tip : u.m.jobExp.tip;
    else if (kind == 2 && index >= 0 && index < cb::kButtons) lines = {texts().buttons[index]};
    else if (kind == 3 && u.view != 0) lines = u.m.bodyTip;
    else if (kind == 4 && !u.m.portraitTip.empty()) lines = {u.m.portraitTip};
    u.tip.assign(lines.begin(), lines.end());
    u.tipLeft = kind == 2 ? 12 + float(index) * 38 : 0;
    u.tipTop = u.h + 6 + (u.warningShown ? 36 : 0);
    u.doc.dirty("tip");
    u.doc.dirty("tip_left");
    u.doc.dirty("tip_top");
}

bool uiMake() {
    Ui& u = ui();
    if (u.doc.made()) return true;
    return u.doc.make("charblock", kDoc, [&](Rml::DataModelConstructor& m) {
        if (auto c = m.RegisterStruct<cb::Chip>()) {
            c.RegisterMember("text", &cb::Chip::text);
            c.RegisterMember("tone", &cb::Chip::tone);
        }
        if (auto c = m.RegisterStruct<cb::Cell>()) {
            c.RegisterMember("key", &cb::Cell::key);
            c.RegisterMember("value", &cb::Cell::value);
            c.RegisterMember("tone", &cb::Cell::tone);
        }
        if (auto e = m.RegisterStruct<Ui::Exp>()) {
            e.RegisterMember("shown", &Ui::Exp::shown);
            e.RegisterMember("label", &Ui::Exp::label);
            e.RegisterMember("pct", &Ui::Exp::pct);
            e.RegisterMember("numbers", &Ui::Exp::numbers);
            e.RegisterMember("right", &Ui::Exp::right);
            e.RegisterMember("right_tone", &Ui::Exp::rightTone);
            e.RegisterMember("w", &Ui::Exp::w);
        }
        m.RegisterArray<std::vector<cb::Chip>>();
        m.RegisterArray<std::vector<cb::Cell>>();
        m.RegisterArray<std::vector<Rml::String>>();
        m.Bind("view", &u.view);
        m.Bind("look", &u.look);
        m.Bind("w", &u.w);
        m.Bind("h", &u.h);
        m.Bind("name", &u.m.name);
        m.Bind("job_name", &u.m.job);  // 3.0.18-beta.46: «job» is the job bar's
        m.Bind("levels", &u.m.levels);
        m.Bind("letter", &u.m.letter);
        m.Bind("badge", &u.m.badge);
        m.Bind("portrait", &u.portrait);
        m.Bind("low", &u.m.low);
        m.Bind("low_text", &u.m.lowText);
        m.Bind("weight_shown", &u.m.weightShown);
        m.Bind("weight_label", &u.m.weightLabel);
        m.Bind("weight_pct", &u.m.weightPct);
        m.Bind("weight_tone", &u.m.weightTone);
        m.Bind("weight_w", &u.weightW);
        m.Bind("zeny_shown", &u.m.zenyShown);
        m.Bind("zeny", &u.m.zeny);
        m.Bind("social", &u.m.social);
        m.Bind("points", &u.m.points);
        m.Bind("hp_shown", &u.m.hpShown);
        m.Bind("sp_shown", &u.m.spShown);
        m.Bind("hp", &u.m.hp);
        m.Bind("sp", &u.m.sp);
        m.Bind("hp_compact", &u.m.hpCompact);
        m.Bind("sp_compact", &u.m.spCompact);
        m.Bind("hp_w", &u.hpW);
        m.Bind("sp_w", &u.spW);
        m.Bind("hp_regen", &u.m.hpRegen);
        m.Bind("hp_regen_right", &u.m.hpRegenRight);
        m.Bind("sp_regen", &u.m.spRegen);
        m.Bind("sp_regen_right", &u.m.spRegenRight);
        m.Bind("battle", &u.m.battle);
        m.Bind("session", &u.m.session);
        m.Bind("session_shown", &u.m.sessionShown);
        m.Bind("session_head", &u.m.sessionHead);
        m.Bind("base", &u.base);
        m.Bind("job", &u.job);
        m.Bind("buttons", &u.buttons);
        m.Bind("tip", &u.tip);
        m.Bind("tip_left", &u.tipLeft);
        m.Bind("tip_top", &u.tipTop);
        m.Bind("ring_d", &u.ringD);
        m.Bind("ring_gap", &u.ringGap);
        m.Bind("ring_pic", &u.ringPic);
        m.Bind("ring_badge_w", &u.ringBadgeW);
        m.Bind("ring_badge_h", &u.ringBadgeH);
        m.Bind("ring_hp_w", &u.ringHpW);
        m.Bind("ring_sp_w", &u.ringSpW);
        m.BindEventCallback("portrait", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            State& s = state();
            changeView(s, cb::nextView(s.view));
            LOG_INFO("character: the block %s (uaUI)", s.view == cb::View::Full      ? "full"
                                                       : s.view == cb::View::Compact ? "compact"
                                                                                     : "folded to its icon");
        });
        m.BindEventCallback("press", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
            const int i = a.empty() ? -1 : a[0].Get<int>();
            if (i < 0 || i >= cb::kButtons) return;
            const bool sent = press(cb::kButton[i].command);
            State& s = state();
            if (s.clicksLogged++ < 10)
                LOG_INFO("character: the Basic Info window's button 0x%X %s (uaUI)", cb::kButton[i].command,
                         sent ? "pressed through the window" : "not pressed - the window is not where this build has it");
        });
        m.BindEventCallback("zone", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
            const int z = a.empty() ? -1 : a[0].Get<int>();
            if (z >= 0 && z < cb::kZones && (cb::opensStats(cb::Zone(z)) || z == int(cb::Zone::Points)))
                press(cb::kButton[0].command);
        });
        m.BindEventCallback("hover", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
            if (a.size() >= 2) tipFor(a[0].Get<int>(), a[1].Get<int>());
        });
        m.BindEventCallback("unhover", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            Ui& u = ui();
            u.hoverKind = u.hoverIndex = -1;
            if (!u.tip.empty()) u.tip.clear(), u.doc.dirty("tip");
        });
    });
}

// The block by uaUI this frame: true when it is shown so (the picture not painted)
bool uiFrame(State& s, const cb::View view, const cb::Look look, const std::string& key) {
    Ui& u = ui();
    if (!uaui::ready() || !uiMake()) {
        u.doc.show(false);
        return false;
    }
    const float dp = uaui::dp();
    const float size = hudedit::sizeOf(hudlayout::Part::Character);  // its size in «Розміщення інтерфейсу»
    if (key != u.key) {  // the words and shares anew, all dirty (a second's change at most, as the picture's)
        u.key = key;
        u.m = cb::model(s.data, texts(), view, look, cb::lastLevels(s.data));
        u.view = int(view);
        u.look = int(look);
        const bool session = u.m.sessionShown;
        u.w = view == cb::View::Full ? 372 : view == cb::View::Compact && look == cb::Look::Bars ? 300 : 0;
        u.h = view == cb::View::Full ? (session ? 250.0f : 264.0f) + 10 + 26 + 12
              : view == cb::View::Compact && look == cb::Look::Bars ? 74
                                                                    : 0;
        const bool icon = view == cb::View::Icon;
        const float hpR = icon ? 28 : 46;
        u.ringD = 2 * (hpR + 3);
        u.ringGap = 3 + (hpR - (icon ? 23.0f : 39.0f));
        u.ringPic = icon ? 18 : 31;
        u.ringBadgeW = icon ? 26 : 32;
        u.ringBadgeH = icon ? 15 : 18;
        u.ringHpW = icon ? 4 : 5;
        u.ringSpW = icon ? 3 : 4;
        if (u.w == 0) u.w = u.ringD, u.h = u.ringD - u.ringGap + u.ringBadgeH * 0.4f + 3;
        u.hpW = u.m.hpShare * 100, u.spW = u.m.spShare * 100, u.weightW = u.m.weightShare * 100;
        const auto exp = [](const cb::ExpRow& r, Ui::Exp& e) {
            e.shown = r.shown, e.label = r.label, e.pct = r.pct, e.numbers = r.numbers, e.right = r.right;
            e.rightTone = r.rightTone, e.w = r.share * 100;
        };
        exp(u.m.baseExp, u.base);
        exp(u.m.jobExp, u.job);
        if (u.hoverKind >= 0) tipFor(u.hoverKind, u.hoverIndex);
        u.doc.dirtyAll();
    }
    // the portrait and the buttons' icons as pictures of the driver's
    const std::string pkey = s.portraitKey + (s.data.portrait ? "p" : "");
    if (pkey != u.portraitKey) u.portraitKey = pkey, ++u.portraitGen;
    const Rml::String portrait = uaui::picture(s.data.portrait, u.portraitGen, u.portraitSrc);
    if (portrait != u.portrait) u.portrait = portrait, u.doc.dirty("portrait");
    const float iconPx = 18 * dp * size;
    if (iconPx != u.iconsScale) {
        u.iconsScale = iconPx;
        ++u.iconsGen;
        u.buttons.clear();
        for (int i = 0; i < cb::kButtons; ++i) {
            panel::Picture& p = u.icons[i];
            const int side = std::max(8, int(std::ceil(iconPx)));
            p.width = p.height = side;
            p.px.assign(size_t(side) * size_t(side), 0u);
            uiicons::draw(p, cb::kButton[i].icon, 0, 0, float(side), 0xECEFF5, 0.9f);
            u.buttons.push_back(uaui::picture(&p, u.iconsGen, u.iconSrc[i]));
        }
        u.doc.dirty("buttons");
    }
    // the low HP warning slides in and out by the theme
    const bool warn = s.show.warning && u.m.low;
    if (warn != u.warningShown) {
        u.warningShown = warn;
        if (Rml::Element* w = u.doc.byId("warning")) w->SetClass("away", !warn);
    }
    if (Rml::ElementDocument* d = u.doc.doc()) {
        const ULONGLONG now = GetTickCount64();
        d->SetClass("flare", now - s.levelAt < motion::kLevelMs);  // a new level: its edge gold a moment
        const int a = int(std::lround(uistyle::panel() * 255));  // [interface] panel_opacity
        char ground[32];
        std::snprintf(ground, sizeof ground, "#10141c%02x", a);
        if (Rml::Element* card = d->GetFirstChild())
            if (card->IsClassSet("card")) card->SetProperty("background-color", ground);
        d->SetProperty("transform", size != 1.0f ? "scale(" + std::to_string(size) + ")" : "none");
    }
    u.doc.place(s.layout.x, s.layout.y);
    u.doc.show(true);
    return true;
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
        ui().doc.show(false);  // 3.0.18-beta.17
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
    // 3.0.11-beta.238 (the owner: «когда сидишь, реген совсем другой»): sitting, and the skills' regen (uAthena
    // status_calc_regen: Increase HP Recovery lv*5 + MaxHP*lv/500, Increase SP Recovery / Spirit of the Blade lv*3 +
    // MaxSP*lv/500 each 10 s; Meditatio the natural SP +3 % a level; Spiritual Cadence sitting only)
    if (now - s.skillsAt >= 1000) {
        s.skillsAt = now;
        s.skills.clear();
        const uint32_t head = read32(kSkillList);
        uint32_t node = head ? read32(head) : 0;
        for (int n = 0; node && node != head && n < 600; ++n, node = read32(node)) {
            int32_t r[4] = {};
            if (!readGame(node + 8, r, sizeof r)) break;
            if (r[1] > 0 && r[1] < 0x10000 && r[3] > 0) s.skills[r[1]] = r[3];
        }
    }
    const auto lv = [&](int id) {
        const auto it = s.skills.find(id);
        return it == s.skills.end() ? 0 : it->second;
    };
    d.sitting = netpredictwin::sitting();
    if (d.spRegen > 0 && lv(kMeditatio) > 0) d.spRegen = d.spRegen * (100 + 3 * lv(kMeditatio)) / 100;
    d.hpSkill = lv(kHpRecovery) * 5 + lv(kHpRecovery) * st.hpMost / 500;
    d.spSkill = lv(kSpRecovery) * 3 + lv(kSpRecovery) * st.spMost / 500;
    d.spSkill += lv(kNinpou) * 3 + lv(kNinpou) * st.spMost / 500;
    d.hpSitSkill = lv(kSpiritualCadence) * 4 + lv(kSpiritualCadence) * st.hpMost / 500;
    d.spSitSkill = lv(kSpiritualCadence) * 2 + lv(kSpiritualCadence) * st.spMost / 500;
    // 3.0.11-beta.238: the last fight's cost, a while after its kill
    d.hpCost = s.hpCost;
    d.spCost = s.spCost;
    // 3.0.11-beta.239 (the owner: «в самом диалоге по опыту ничего не видно»): the last gain on the experience rows
    const expgain::Label gain = expgainwin::label();
    d.gainBase = gain.base;
    d.gainJob = gain.job;
    d.gainShown = gain.ms ? expgain::opacity(gain.ms, now, kCostMs, kCostFadeMs) : 0.0f;
    d.costShown = 0;
    if (s.costAt && now - s.costAt < kCostMs)
        d.costShown = now - s.costAt + kCostFadeMs <= kCostMs ? 1.0f : float(kCostMs - (now - s.costAt)) / kCostFadeMs;
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
    d.broken = upkeepwatch::broken(codePage);  // 3.0.16-beta.13
    d.pet = upkeepwatch::pet();
    d.homun = upkeepwatch::homun();
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
        d.broken + '|' + std::to_string(d.pet) + ',' + std::to_string(d.homun) + ';' +  // 3.0.16-beta.13
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
        std::to_string(d.deaths) + 'z' + std::to_string(show.hoverZone) + ';' + (d.sitting ? "s" : "u") +
        std::to_string(d.hpSkill) + ',' + std::to_string(d.spSkill) + ',' + std::to_string(d.hpSitSkill) + ',' +
        std::to_string(d.spSitSkill) + ',' + std::to_string(int(d.hpCost * 1000)) + ',' +
        std::to_string(int(d.spCost * 1000)) + ',' + std::to_string(int(d.costShown * 20)) + ';' +
        std::to_string(d.gainBase) + ',' + std::to_string(d.gainJob) + ',' + std::to_string(int(d.gainShown * 20));
    s.uiShown = uiFrame(s, view, panelLook, key);  // 3.0.18-beta.17: by uaUI while [interface] uaui is on
    if (s.uiShown) {
        s.shown = true;
        block(s, game);
        return;
    }
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
    if (s.uiShown) return;  // 3.0.18-beta.17: uaUI draws it
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
    if (!s.shown || s.uiShown) return false;  // 3.0.18-beta.17: uaUI's events take its clicks
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
    const ULONGLONG now = GetTickCount64();
    if (const auto* h = std::get_if<packets::Hit>(&e)) {
        if (h->source == me && h->target != me && h->target) {
            if (s.hit.size() > 256) s.hit.clear();
            s.hit.insert(h->target);
            if (!s.fight || now - s.lastHitAt > kFightIdleMs) {  // 3.0.11-beta.238: a fight begins - HP and SP then
                s.fight = true;
                s.fightHp = s.data.hp;
                s.fightSp = s.data.sp;
            }
            s.lastHitAt = now;
        }
    } else if (const auto* v = std::get_if<packets::Vanish>(&e)) {
        if (v->type != 1) return;
        if (v->id == me) {
            ++s.deaths;
        } else if (s.hit.erase(v->id)) {
            ++s.kills;
            // 3.0.11-beta.238: what it cost; the next one's from here
            if (s.fight && s.data.hpMost > 0 && s.data.spMost > 0) {
                s.hpCost = float(std::max(0, s.fightHp - s.data.hp)) / float(s.data.hpMost);
                s.spCost = float(std::max(0, s.fightSp - s.data.sp)) / float(s.data.spMost);
                s.costAt = now;
                s.fightHp = s.data.hp;
                s.fightSp = s.data.sp;
            }
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

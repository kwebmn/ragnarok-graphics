#include "win/notifier.hpp"

#include <shellapi.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <variant>
#include <vector>

#include "core/buff_timers.hpp"
#include "core/notify.hpp"
#include "core/party_watch.hpp"
#include "win/buff_overlay.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/packet_tap.hpp"
#include "win/real_ddraw.hpp"
#include "win/window_switch.hpp"

namespace uagfx::win::notifier {
namespace {

constexpr uint32_t kOwnAccount = 0x771F7C;  // CSession's account id (the 0x196 handler compares with it)
constexpr uint32_t kSession = 0x771148, kIgnored = 0x639F60;  // CSession::IsIgnored(name) - the whisper's own check
const uint8_t kIgnoredCode[] = {0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x08, 0x8B, 0x81, 0xF8, 0x18, 0x00, 0x00};
constexpr UINT kIconId = 1;

using IgnoredFn = int(__attribute__((thiscall)) *)(void* session, const char* name);
using NotifyFn = BOOL(WINAPI*)(DWORD message, NOTIFYICONDATAW* data);
using QuietFn = HRESULT(WINAPI*)(int* state);  // SHQueryUserNotificationState
constexpr uint64_t kWhyEveryMs = 60000;  // 3.0.7-beta.8: a kind seen but not told - in the log once a minute

struct State {
    HWND window = nullptr;
    bool front = true, logged = false, icon = false;
    bool main = true;  // 3.0.7-beta.6: the main game window (what the party and the guild hear told by it alone)
    NotifySettings settings;
    unsigned codePage = CP_ACP;
    notify::Gate gate;
    NotifyFn shell = nullptr;
    bool shellTried = false;
    notify::Attacks attacks;  // 3.0.7-beta.6
    party::Watch party;
    buffs::Ending ending;
    notify::Recovery recovery;  // 3.0.7-beta.7
    // 3.0.7-beta.8 (the user: «мне кажется что уведомления не выводятся»): what the log tells of it - each kind seen
    // and not told (when last), Windows' own state of notifications
    uint64_t whyAt[notify::kKinds] = {};
    QuietFn quiet = nullptr;
};

const char* kindName(notify::Kind k) {
    static const char* const kNames[] = {"a whisper",  "a party invitation", "a trade",       "a guild invitation",
                                         "a friend",   "the death",          "an attack",     "a buff ending",
                                         "a death in the party", "low HP in the party", "the party's chat",
                                         "the guild's chat", "the name said", "the server gone", "HP and SP back",
                                         "a report's answer"};
    static_assert(sizeof kNames / sizeof kNames[0] == notify::kKinds, "a name a kind");
    return kNames[int(k)];
}

// Windows' notifications now (SHQueryUserNotificationState): whether it shows them at all
const char* quietState(State& s) {
    int q = 0;
    if (!s.quiet || FAILED(s.quiet(&q))) return "its state not known";
    switch (q) {
        case 1: return "the screen saver or a locked screen: held";
        case 2: return "a full-screen program busy: held";
        case 3: return "a Direct3D full-screen program: held";
        case 4: return "the presentation mode: held";
        case 5: return "accepting notifications";
        case 6: return "the quiet time of a new user: held";
        case 7: return "a Windows app in full screen: held";
        default: return "an unknown state";
    }
}

State& state() {
    static State s;
    return s;
}

std::string fromGame(const std::string& text) { return utf8Of(widened(text, state().codePage)); }

void copy(wchar_t* to, size_t room, const std::wstring& from) {
    const size_t n = from.size() < room - 1 ? from.size() : room - 1;
    std::memcpy(to, from.data(), n * sizeof(wchar_t));
    to[n] = L'\0';
}

bool ignored(const std::string& name) {
    static const bool known =
        std::memcmp(reinterpret_cast<const void*>(uintptr_t(kIgnored)), kIgnoredCode, sizeof kIgnoredCode) == 0;
    if (!known) return false;
    char padded[32] = {};
    std::memcpy(padded, name.data(), name.size() < 24 ? name.size() : 24);
    const auto fn = reinterpret_cast<IgnoredFn>(uintptr_t(kIgnored));
    return fn(reinterpret_cast<void*>(uintptr_t(kSession)), padded) != 0;
}

// The balloon: the character's name above, the words below; the game's own icon. 3.0.7-beta.8: whether Windows took it
bool show(State& s, const std::string& words) {
    if (!s.shellTried) {
        s.shellTried = true;
        const HMODULE shell = LoadLibraryA("shell32.dll");
        const FARPROC fn = shell ? GetProcAddress(shell, "Shell_NotifyIconW") : nullptr;
        s.shell = reinterpret_cast<NotifyFn>(reinterpret_cast<void*>(fn));
        const FARPROC q = shell ? GetProcAddress(shell, "SHQueryUserNotificationState") : nullptr;
        s.quiet = reinterpret_cast<QuietFn>(reinterpret_cast<void*>(q));
    }
    if (!s.shell || !s.window) return false;
    NOTIFYICONDATAW d{};
    d.cbSize = sizeof d;
    d.hWnd = s.window;
    d.uID = kIconId;
    d.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE | NIF_INFO;
    d.uCallbackMessage = kMessage;
    HICON icon = reinterpret_cast<HICON>(GetClassLongPtrW(s.window, GCLP_HICON));
    d.hIcon = icon ? icon : LoadIconW(nullptr, reinterpret_cast<LPCWSTR>(IDI_APPLICATION));
    const std::string name = readOwnName();
    const std::wstring title = widened(name.empty() ? std::string("uaRO") : fromGame(name), CP_UTF8);
    copy(d.szTip, sizeof d.szTip / sizeof d.szTip[0], title);
    copy(d.szInfoTitle, sizeof d.szInfoTitle / sizeof d.szInfoTitle[0], title);
    copy(d.szInfo, sizeof d.szInfo / sizeof d.szInfo[0], widened(words, CP_UTF8));
    d.dwInfoFlags = NIIF_INFO | (s.settings.sound ? 0u : DWORD(NIIF_NOSOUND));  // 3.0.7-beta.6: Windows' own sound
    const bool took = s.shell(s.icon ? NIM_MODIFY : NIM_ADD, &d) != FALSE;
    s.icon = took || s.icon;
    return took;
}

void hide(State& s) {
    if (!s.icon || !s.shell || !s.window) return;
    NOTIFYICONDATAW d{};
    d.cbSize = sizeof d;
    d.hWnd = s.window;
    d.uID = kIconId;
    s.shell(NIM_DELETE, &d);
    s.icon = false;
}

// 3.0.9-beta.4: the words made already (a report's answer: UTF-8, not the game's code page)
void tellWords(notify::Kind kind, const std::string& words) {
    State& s = state();
    if (!s.window) return;
    if (!notify::wanted(s.settings, kind, s.front, s.main)) {
        // 3.0.7-beta.8 (the user: «мне кажется что уведомления не выводятся»): why not, a kind once a minute
        const uint64_t now = GetTickCount64();
        uint64_t& at = s.whyAt[int(kind)];
        if (!at || now - at >= kWhyEveryMs) {
            at = now;
            const char* why = !s.settings.on                                        ? "the notifications off"
                              : s.front                                             ? "the game in front"
                              : notify::wanted(s.settings, kind, false, true) ? "not the main game window"
                                                                                    : "its box off";
            LOG_INFO("notify: %s seen - not told: %s", kindName(kind), why);
        }
        return;
    }
    FLASHWINFO f{sizeof f, s.window, FLASHW_TRAY | FLASHW_TIMERNOFG, 0, 0};  // until it is in front again
    FlashWindowEx(&f);
    if (!s.gate.pass(kind, GetTickCount64())) return;  // a burst: the flashing says it
    const bool took = show(s, words);
    LOG_INFO("notify: %s told from behind - Windows %s it (error %lu), Windows' notifications: %s", kindName(kind),
             took ? "took" : "refused", took ? 0ul : (unsigned long)GetLastError(), quietState(s));
}

void tell(notify::Kind kind, const std::string& who, const std::string& what) {
    tellWords(kind, notify::text(startup().lang, kind, fromGame(who), fromGame(what)));
}

// 3.0.7-beta.6 (the user's notification 6): what the party watch saw - a death, a fall under the threshold
void tellParty(const std::vector<party::News>& news) {
    for (const party::News& n : news)
        if (n.kind == party::News::Dead)
            tell(notify::Kind::PartyDeath, n.name, "");
        else
            tell(notify::Kind::PartyLowHp, n.name, std::to_string(n.percent));
}

uint32_t ownAccount() { return knownGameBuild() ? *reinterpret_cast<const uint32_t*>(uintptr_t(kOwnAccount)) : 0; }

}  // namespace

void frame(HWND window, bool front, bool main, const Config& config) {
    State& s = state();
    if (!s.logged) {
        s.logged = true;
        LOG_INFO("notify: %s", packetTapOn() ? "the game's packets read - whispers, invitations and deaths told from "
                                               "behind ([notify])"
                                             : "not this uaRO.exe's packet loop - no notifications from the game");
    }
    if (front && !s.front) hide(s);  // back: the player sees the game itself
    if (!front && s.front && s.window)  // 3.0.7-beta.8: the log tells when the game went behind
        LOG_INFO("notify: the game behind - told from now (%s, the main game window %s)",
                 config.notify.on ? "notifications on" : "notifications off", main ? "yes" : "no");
    s.window = window;
    s.front = front;
    s.main = main;
    s.settings = config.notify;
    s.codePage = gameCodePage(config);
    // 3.0.7-beta.6 (the user's notification 5): a buff of the character's about to end - each once, in front too (the
    // player sees it there), so a window put behind later does not tell of it
    const uint64_t now = GetTickCount64();
    const buffs::Timers& timers = buffoverlay::timers();
    for (uint16_t icon : s.ending.due(timers, now, uint32_t(s.settings.buffSeconds) * 1000u)) {
        const std::string name = buffs::name(icon);
        if (!name.empty())
            tell(notify::Kind::BuffEnding, name, std::to_string((timers.left(icon, now) + 999u) / 1000u));
    }
}

void answer(const std::string& code, const std::string& status) {
    tellWords(notify::Kind::Answer, notify::text(startup().lang, notify::Kind::Answer, code, status));
}

void disconnected() {
    LOG_INFO("notify: the server closed the connection");
    tell(notify::Kind::Disconnect, "", "");
}

void packet(const packets::Event& e) {
    const auto own = [](uint32_t id) {
        return knownGameBuild() && id == *reinterpret_cast<const uint32_t*>(uintptr_t(kOwnAccount));
    };
    if (const auto* w = std::get_if<packets::Whisper>(&e)) {
        if (ignored(w->from)) return;
        windowmarks::whispered();  // 3.0.4-beta.4: on its card in the other windows' panels
        tell(notify::Kind::Whisper, w->from, w->text);
    } else if (const auto* h = std::get_if<packets::Hit>(&e)) {
        if (!own(h->target)) return;
        if (h->damage > 0) windowmarks::hit();  // a miss (3.0.7-beta.6) no mark: nothing hurt
        // 3.0.7-beta.6 (the user's notification 1): once, again after a calm
        if (state().attacks.hit(GetTickCount64()))
            tell(notify::Kind::Attack, "", h->damage > 0 ? std::to_string(h->damage) : std::string());
    } else if (const auto* p = std::get_if<packets::PartyInvite>(&e)) {
        tell(notify::Kind::Party, p->party, "");
    } else if (const auto* t = std::get_if<packets::TradeRequest>(&e)) {
        tell(notify::Kind::Trade, t->from, "");
    } else if (const auto* g = std::get_if<packets::GuildInvite>(&e)) {
        tell(notify::Kind::Guild, g->guild, "");
    } else if (const auto* f = std::get_if<packets::FriendRequest>(&e)) {
        tell(notify::Kind::Friend, f->from, "");
    } else if (const auto* v = std::get_if<packets::Vanish>(&e)) {
        if (v->type == 1 && own(v->id)) {
            windowmarks::died();
            tell(notify::Kind::Death, "", "");
        } else if (v->type == 1) {  // 3.0.7-beta.6: one of the party seen dying
            std::vector<party::News> news;
            state().party.died(v->id, news);
            tellParty(news);
        }
    } else if (const auto* l = std::get_if<packets::PartyList>(&e)) {  // 3.0.7-beta.6 (the user's notification 6)
        std::vector<std::pair<uint32_t, std::string>> members;  // 3.0.11-beta.39: the seats' accounts and names
        for (const packets::PartySeat& seat : l->seats) members.emplace_back(seat.account, seat.name);
        state().party.members(members, ownAccount());
    } else if (const auto* m = std::get_if<packets::PartyMember>(&e)) {
        state().party.joined(m->account, m->name);
    } else if (const auto* gone = std::get_if<packets::PartyLeft>(&e)) {
        state().party.left(gone->account);
    } else if (const auto* hp = std::get_if<packets::PartyHp>(&e)) {
        std::vector<party::News> news;
        state().party.hp(hp->account, hp->hp, hp->most, state().settings.partyHp, news);
        tellParty(news);
    } else if (const auto* pc = std::get_if<packets::PartyChat>(&e)) {  // the user's notification 7
        if (own(pc->account)) return;  // the party hears its own words back
        std::string who, what;
        notify::speaker(pc->text, who, what);
        tell(notify::Kind::PartyChat, who, what);
    } else if (const auto* gc = std::get_if<packets::GuildChat>(&e)) {
        std::string who, what;
        notify::speaker(gc->text, who, what);
        if (who != readOwnName()) tell(notify::Kind::GuildChat, who, what);
    } else if (const auto* st = std::get_if<packets::Status>(&e)) {  // 3.0.7-beta.7: HP and SP back
        State& s = state();
        if (s.recovery.status(st->type, st->value, s.settings.recoveredPercent))
            tell(notify::Kind::Recovered, "", std::to_string(s.settings.recoveredPercent));
    } else if (const auto* c = std::get_if<packets::Chat>(&e)) {  // the user's notification 8: the name said near
        if (own(c->unit)) return;
        std::string who, what;
        notify::speaker(c->text, who, what);
        const std::string name = readOwnName();
        if (!name.empty() && notify::mentions(fromGame(what), fromGame(name))) tell(notify::Kind::Mention, who, what);
    }
}

bool message(HWND window, UINT m, WPARAM, LPARAM lp) {
    if (m != kMessage) return false;
    const UINT what = LOWORD(lp);
    if (what == NIN_BALLOONUSERCLICK || what == WM_LBUTTONUP) {  // the notification or the icon clicked
        if (IsIconic(window)) ShowWindow(window, SW_RESTORE);
        SetForegroundWindow(window);
    }
    return true;
}

void gone(HWND window) {
    State& s = state();
    if (s.window == window) hide(s);
}

}  // namespace uagfx::win::notifier

#include "win/cast_bar.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "core/cast_bar.hpp"
#include "core/hotbar_panel.hpp"
#include "core/settings/strings.hpp"
#include "win/client_tables.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"

namespace uagfx::win::castwin {
namespace {

namespace cb = uagfx::castbar;

// uaRO.exe's: the character's account; the game mode's world (+0xC4) and its player (+0x3C - FUN_0058b560's caster
// when 0x013e names the character); the player's cast gauge (UI window +0x220: FUN_0054cfc0 makes it at the actor's
// message 0x52, FUN_0054ac00 moves it each frame and takes it away at the cast's end + 0x22C, its start + 0x230); the
// gauge's class (FUN_0044a110), a window's place (+0x14 x, y, w, h) and shown flag (+0x38); the skills' names
// (FUN_00505ce0: [0x74B9AC][id], 0 < id < 0x3FD)
constexpr uint32_t kOwnAccount = 0x771F7C, kWorld = 0xC4, kPlayer = 0x3C, kGauge = 0x220, kCastEnd = 0x22C;
constexpr uint32_t kCastStart = 0x230, kGaugeClass = 0x696C18, kRect = 0x14, kShown = 0x38, kSkillNames = 0x74B9AC;
constexpr int kMostSkill = 0x3FD;
using TimeFn = DWORD(WINAPI*)();

struct State {
    bool on = false, shown = false, namesChecked = false, namesKnown = false;
    float k = 0, scale = 1;
    uint32_t hidGauge = 0;  // the game's gauge this one hides
    uint16_t skill = 0;     // the character's last cast
    std::string aegis;
    cb::Data data;
    cb::Layout layout;
    std::string key;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    int told = 0;
};

State& state() {
    static State s;
    return s;
}

uint32_t read32(uint32_t at) {
    uint32_t v = 0;
    return readGame(at, &v, 4) ? v : 0;
}

bool write32(uint32_t at, uint32_t v) {
    SIZE_T n = 0;
    return WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(uintptr_t(at)), &v, 4, &n) && n == 4;
}

std::string readString(uint32_t at) {
    char buf[48] = {};
    if (!at || !readGame(at, buf, sizeof buf - 1)) return "";
    return buf;
}

// The game's clock of the cast's times (its own winmm, already loaded; GetTickCount's near enough without it)
DWORD gameTime() {
    static TimeFn fn = [] {
        const HMODULE winmm = GetModuleHandleA("winmm.dll");
        return winmm ? reinterpret_cast<TimeFn>(reinterpret_cast<void*>(GetProcAddress(winmm, "timeGetTime")))
                     : nullptr;
    }();
    return fn ? fn() : GetTickCount();
}

// The skill's name in the game's table ("MG_FIREBOLT"); "" none - the table checked once by its first name
std::string aegisOf(State& s, int id) {
    if (!s.namesChecked) {
        s.namesChecked = true;
        const uint32_t names = read32(kSkillNames);
        s.namesKnown = names && readString(read32(names + 4)) == "NV_BASIC";
        if (!s.namesKnown) LOG_INFO("cast: the game's skills' names are not where this build has them - no names");
    }
    if (!s.namesKnown || id <= 0 || id >= kMostSkill) return "";
    return readString(read32(read32(kSkillNames) + 4 * uint32_t(id)));
}

// The game's gauge back as it was - only while it is still the player's own: one the game took away at the cast's end
// is freed memory, never written
void restore(State& s, uint32_t live) {
    if (s.hidGauge && s.hidGauge == live && read32(live) == kGaugeClass) write32(live + kShown, 1);
    s.hidGauge = 0;
}

void hide(State& s, uint32_t live) {
    restore(s, live);
    s.shown = false;
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

void render(State& s) {
    s.picture = panel::Picture{};
    s.picture.width = s.layout.w;
    s.picture.height = s.layout.h;
    if (s.layout.w <= 0 || s.layout.h <= 0) return;
    s.picture.px.assign(size_t(s.layout.w) * size_t(s.layout.h), 0u);
    cb::paint(s.picture, s.layout, s.data);
    drawWords(s.picture, gdiWords(cb::words(s.layout, s.data)));
    s.upload = true;
}

}  // namespace

void packet(const packets::Event& e) {
    const auto* c = std::get_if<packets::Casting>(&e);
    if (!c || c->source != read32(kOwnAccount)) return;
    State& s = state();
    s.skill = c->skill;
    s.aegis.clear();
    if (s.told < 20) {
        ++s.told;
        LOG_INFO("cast: the character casts the skill %u for %u ms", unsigned(c->skill), unsigned(c->ms));
    }
}

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    State& s = state();
    if (on != s.on) {
        s.on = on;
        LOG_INFO("cast: [interface] cast = %d", on ? 1 : 0);
    }
    const uint32_t mode = readGameMode();
    const uint32_t player = mode ? read32(read32(mode + kWorld) + kPlayer) : 0;
    const uint32_t gauge = player ? read32(player + kGauge) : 0;
    if (!on || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0 || !gauge ||
        read32(gauge) != kGaugeClass) {
        hide(s, gauge);
        if (!gauge) s.skill = 0;  // the cast over: its skill forgotten
        return;
    }
    if (s.hidGauge != gauge) s.hidGauge = 0;  // another cast's gauge: the old one is the game's to free
    int32_t r[4] = {0, 0, 0, 0};  // the gauge's place: the game moves it over the hero each frame
    const DWORD end = read32(player + kCastEnd), start = read32(player + kCastStart), now = gameTime();
    if (!readGame(gauge + kRect, r, sizeof r) || r[2] <= 0 || r[2] > 400 || end <= start) {
        hide(s, gauge);
        return;
    }
    if (read32(gauge + kShown) && write32(gauge + kShown, 0) && s.told < 20) {
        ++s.told;
        LOG_INFO("cast: the game's gauge at %d,%d (%dx%d) hidden for the label (%u ms)", r[0], r[1], r[2], r[3],
                 unsigned(end - start));
    }
    s.hidGauge = gauge;
    const float scale = std::clamp(float(k), 0.75f, 6.0f);
    s.k = float(k);
    s.scale = scale;
    if (s.skill && s.aegis.empty()) s.aegis = aegisOf(s, s.skill);
    cb::Data& d = s.data;
    d.name.clear();
    d.icon = nullptr;
    if (!s.aegis.empty()) {
        const auto& names = clienttables::skillNames();
        const auto n = names.find(s.aegis);
        d.name = n != names.end() ? utf8Of(widened(hotbar::spaced(n->second), codePage)) : s.aegis;
        d.icon = clienttables::icon(hotbar::lowered(s.aegis), cb::iconSide(scale), 4 * scale);
    }
    const int total = int(end - start), past = std::clamp(int(now - start), 0, total);
    d.done = float(past) / float(total);
    const settings::Lang lang = startup().lang;
    d.left = cb::seconds(total - past, lang == settings::Lang::En ? '.' : ',',
                         settings::tr(lang, settings::Text::CastSeconds));
    const int centre = int(std::lround((float(r[0]) + float(r[2]) / 2) * float(k)));
    const int top = int(std::lround(float(r[1]) * float(k)));
    s.layout = cb::layout(d, centre, top, scale);
    const std::string key = d.name + ';' + d.left + ';' + std::to_string(int(d.done * s.layout.bar.w)) + ';' +
                            std::to_string(s.layout.x) + ',' + std::to_string(s.layout.y) + ';' +
                            std::to_string(scale) + (d.icon ? "i" : "");
    if (key != s.key) {
        s.key = key;
        render(s);
    }
    s.shown = true;
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.shown || s.k <= 0 || s.picture.width <= 0 || s.picture.height <= 0) return;
    int cursor = -1, cx = 0, cy = 0;  // under the game's own cursor, as the panels
    if (readGameCursor(cx, cy)) cursor = panel::cursorDraw(record, float(cx), float(cy));
    drawPicture(record, renderer, s.picture, s.texture, s.texW, s.texH, s.upload, s.layout.x, s.layout.y, s.k);
    if (cursor >= 0) record.moveLastBefore(record.commands().size() - size_t(cursor) - 1, size_t(cursor));
}

}  // namespace uagfx::win::castwin

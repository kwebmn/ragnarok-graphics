#include "win/npc_tag.hpp"

#include <cmath>
#include <cstring>
#include <map>
#include <string>
#include <variant>
#include <vector>

#include "core/pad_glyphs.hpp"
#include "core/pad_hint.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "pad/win/runtime.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/npc_talk.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"

namespace uagfx::win::npctagwin {
namespace {

namespace ph = uagfx::padhint;

// The game's name request (mob_hp's): the game mode's own, which sends 0x0094 for an actor
constexpr uint32_t kModeNow = 0x74B57C, kModeMsgSlot = 0x14, kModeMsg = 0x5707D0, kAskName = 0x560650;
const uint8_t kAskNameCode[] = {0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x24, 0x8B, 0x45,
                                0x08, 0x53, 0x56, 0x57, 0x85, 0xC0, 0x8B, 0xF9};
using AskNameFn = void(__attribute__((thiscall)) *)(uint32_t mode, uint32_t id);
constexpr DWORD kAskAgainMs = 5000;
constexpr uint32_t kDot = 0x99E8C36Au;  // a faint gold dot over the others

struct State {
    std::map<uint32_t, std::string> names;  // the game's code page
    std::map<uint32_t, DWORD> askedAt;
    std::vector<uint32_t> toAsk;
    uint32_t tag = 0;
    std::vector<ph::Part> parts;
    ph::Layout layout;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false, shown = false;
    std::string key;
    float k = 0, scale = 1;
    struct Dot {
        float x = 0, y = 0;
    };
    std::vector<Dot> dots;
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

std::vector<Words> gdiWords(const std::vector<ph::Words>& in) {
    std::vector<Words> out;
    for (const ph::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == ph::Align::Right    ? DT_RIGHT
                                 : w.align == ph::Align::Center ? DT_CENTER
                                                                : DT_LEFT),
                            false});
    return out;
}

void want(State& s, uint32_t id) {
    if (s.names.count(id)) return;
    const DWORD now = GetTickCount();
    if (const auto at = s.askedAt.find(id); at != s.askedAt.end() && now - at->second < kAskAgainMs) return;
    s.askedAt[id] = now;
    if (s.toAsk.size() < 8) s.toAsk.push_back(id);
}

}  // namespace

void packet(const packets::Event& e) {
    State& s = state();
    if (const auto* n = std::get_if<packets::ActorName>(&e)) {
        if (s.names.size() > 512) s.names.clear();
        s.names[n->id] = n->name;
    }
}

void frame(HWND game, unsigned codePage, double k, int targetWidth, int targetHeight) {
    State& s = state();
    s.shown = false;
    s.dots.clear();
    const uapad::runtime::Look look = uapad::runtime::look();
    if (!game || k <= 0 || targetWidth <= 0 || targetHeight <= 0 || !inGameWorld() || !look.shown || npcwin::shown())
        return;
    const uint32_t tag = uapad::runtime::npcTag();
    s.k = float(k);
    const float scale = uistyle::partScale(k);
    s.scale = scale;
    for (const uint32_t id : uapad::runtime::npcNear()) {  // the others: a dot over each
        float box[4] = {};
        if (id == tag || !readActorBox(id, box)) continue;
        s.dots.push_back(State::Dot{(box[0] + box[2]) / 2 * float(k), box[1] * float(k) - 6 * scale});
    }
    float box[4] = {};
    if (!tag || !readActorBox(tag, box)) return;
    if (tag != s.tag) {
        s.tag = tag;
        if (s.told++ < 10) LOG_INFO("npc tag: over the NPC %u (the Talk button takes it)", tag);
    }
    want(s, tag);
    const auto named = s.names.find(tag);
    const std::string name = named != s.names.end() && !named->second.empty()
                                 ? utf8Of(widened(named->second, codePage))
                                 : settings::tr(language(), settings::Text::NpcTalkTag);
    ph::Part part{"", {}, name};
    const padglyph::Button g = padglyph::buttonOf(uapad::runtime::talkButtonBit());
    if (g != padglyph::Button::kCount) part.buttons.push_back(int(g));
    s.parts = {part};
    const int centre = int(std::lround((box[0] + box[2]) / 2 * float(k)));
    const int bottom = int(std::lround(box[1] * float(k) - 4 * scale));
    s.layout = ph::layout(s.parts, centre, bottom, scale);
    std::string key = std::to_string(s.layout.w) + (look.sony ? "S" : "X") + name + '|' +
                      std::to_string(part.buttons.empty() ? -1 : part.buttons[0]) + '#' +
                      std::to_string(uistyle::generation());
    if (key != s.key) {
        s.key = key;
        if (panel::blank(s.picture, s.layout.w, s.layout.h)) {
            ph::paint(s.picture, s.layout, s.parts, look.sony);
            drawWords(s.picture, gdiWords(ph::words(s.layout, s.parts, look.sony)));
            s.upload = true;
        }
    }
    s.shown = true;
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (s.k <= 0 || (!s.shown && s.dots.empty())) return;
    const size_t world = record.firstInterface(), before = record.commands().size();
    const float r = std::max(2.0f, 2.5f * s.scale);  // the target's pixels, as the dots are
    for (const State::Dot& d : s.dots) drawFill(record, d.x - r, d.y - r, 2 * r, 2 * r, s.k, kDot);
    if (s.shown && s.picture.width > 0)
        drawPart(record, renderer, s.picture, s.texture, s.texW, s.texH, s.upload, s.layout.x, s.layout.y, s.k);
    const size_t added = record.commands().size() - before;  // under the game's own windows, as the world is
    if (added) record.moveLastBefore(added, world);
}

void beforeGameDraw() {
    State& s = state();
    if (s.toAsk.empty()) return;
    std::vector<uint32_t> ids;
    ids.swap(s.toAsk);
    static int known = -1;
    if (known < 0) {
        uint8_t have[sizeof kAskNameCode] = {};
        known = readGame(kAskName, have, sizeof have) && std::memcmp(have, kAskNameCode, sizeof have) == 0 ? 1 : 0;
        if (!known) LOG_INFO("npc tag: the game's name request is not where this build has it - no name asked");
    }
    const uint32_t mode = read32(kModeNow);
    if (!known || !mode || !inGameWorld() || read32(read32(mode) + kModeMsgSlot) != kModeMsg) return;
    for (const uint32_t id : ids) reinterpret_cast<AskNameFn>(uintptr_t(kAskName))(mode, id);
}

}  // namespace uagfx::win::npctagwin

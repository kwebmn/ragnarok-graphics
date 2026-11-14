#include "win/threat_arc.hpp"

#include <windows.h>

#include <cmath>
#include <vector>

#include "core/d3d7.hpp"
#include "core/effect_exe.hpp"
#include "core/game_camera.hpp"
#include "core/threat_arc.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/real_ddraw.hpp"
#include "win/uaui.hpp"
#include "win/view3d.hpp"

namespace uagfx::win::threatarcwin {
namespace {

namespace ta = ::uagfx::threatarc;

// The character's account; the game mode's world (+0xC4), its player (+0x3C) and its actors (a list at +0x18, each
// node's actor at +8, its id at +0x208 - FUN_004ebd10); an actor's place +4 (x), +0xC (z)
constexpr uint32_t kOwnAccount = 0x771F7C, kWorld = 0xC4, kPlayer = 0x3C, kActors = 0x18, kActorId = 0x208;
constexpr uint32_t kPlaceX = 4, kPlaceZ = 0xC, kGameW = 0x6EBE1C, kGameH = 0x6EBE20;
constexpr size_t kMostNodes = 4096;
constexpr float kPi = 3.14159265f;

struct Label {  // 3.0.19-beta.37: an arc's distance, in the game's pixels
    float x = 0, y = 0, alpha = 0;
    int cells = 0;
};

struct State {
    bool on = true;
    bool drawn = false;
    ta::Hits hits;
    int told = 0;
    std::vector<own_ground::Tl1> verts;
    std::vector<Label> labels;
};

// 3.0.19-beta.37: the distances by the arcs in RmlUi's markup - each a red word with its outline, its middle there
const char* kDoc = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="ta" data-model="threat" data-style-width="w + 'px'" data-style-height="h + 'px'">
<span class="far" data-for="f : dist" data-style-left="f.x - 60 * s + 'px'" data-style-top="f.y - 11 * s + 'px'"
  data-style-opacity="f.op">{{f.text}}</span>
</body></rml>)RML";

struct FarV {
    float x = 0, y = 0, op = 0;
    Rml::String text;
};

struct Ui {
    uaui::Doc doc;
    float w = 0, h = 0, s = 1;
    std::vector<FarV> dist;  // not "far": windef.h makes it nothing
};

Ui& ui() {
    static Ui u;
    return u;
}

State& state() {
    static State s;
    return s;
}

uint32_t read32(uint32_t a) {
    uint32_t v = 0;
    return readGame(a, &v, 4) ? v : 0;
}

bool placeOf(uint32_t actor, float& x, float& z) {
    return readGame(actor + kPlaceX, &x, 4) && readGame(actor + kPlaceZ, &z, 4) && std::isfinite(x) &&
           std::isfinite(z);
}

uint32_t actorOf(uint32_t world, uint32_t id) {
    const uint32_t head = read32(world + kActors);
    if (!head) return 0;
    uint32_t node = read32(head);
    for (size_t n = 0; node && node != head && n < kMostNodes; ++n, node = read32(node)) {
        const uint32_t actor = read32(node + 8);
        if (actor && read32(actor + kActorId) == id) return actor;
    }
    return 0;
}

DrawSetup setup() {  // blended, untextured, its colour its own, over the world
    DrawSetup s;
    s.zEnable = false;
    s.zWrite = false;
    s.cull = d3d7::CULL_NONE;
    s.blend = true;
    s.srcBlend = d3d7::BLEND_SRCALPHA;
    s.dstBlend = d3d7::BLEND_INVSRCALPHA;
    s.stage[0].colorOp = s.stage[0].alphaOp = d3d7::TOP_SELECTARG1;
    s.stage[0].colorArg1 = s.stage[0].alphaArg1 = d3d7::TA_DIFFUSE;
    s.stage[1].colorOp = s.stage[1].alphaOp = d3d7::TOP_DISABLE;
    return s;
}

}  // namespace

void setOn(bool on) {
    State& s = state();
    if (!on) s.hits.last.clear();
    s.on = on;
}

void packet(const packets::Event& e) {
    State& s = state();
    if (!s.on) return;
    if (const auto* h = std::get_if<packets::Hit>(&e)) {
        const uint32_t own = read32(kOwnAccount);
        if (own && h->target == own && h->source && h->source != own) s.hits.hit(h->source, GetTickCount());
    } else if (const auto* v = std::get_if<packets::Vanish>(&e)) {
        s.hits.gone(v->id);
    }
}

void newFrame() {
    State& s = state();
    s.drawn = false;
    s.labels.clear();
}

void draw(FrameRecord& record, int width, int height) {
    State& s = state();
    if (!s.on || s.drawn || s.hits.last.empty() || !view3dwin::active()) return;
    s.drawn = true;
    const uint32_t now = GetTickCount();
    s.hits.forget(now);
    float right[2] = {0, 0}, forward[2] = {0, 0};
    if (s.hits.last.empty() || !view3dwin::viewAxes(right, forward)) return;
    const uint32_t mode = readGameMode(), world = mode ? read32(mode + kWorld) : 0;
    const uint32_t player = world ? read32(world + kPlayer) : 0;
    float px = 0, pz = 0;
    if (!player || !placeOf(player, px, pz)) return;
    int gw = 0, gh = 0;
    const bool sized = readGame(kGameW, &gw, 4) && readGame(kGameH, &gh, 4) && gw > 0 && gh > 0;
    const float half = sized ? std::atan(std::tan(camera::fovNow() * 0.5f * kPi / 180.0f) * float(gw) / float(gh))
                             : 0.9f;  // the view's half across
    s.verts.clear();
    for (const auto& [id, at] : s.hits.last) {
        const uint32_t actor = actorOf(world, id);
        float ax = 0, az = 0;
        if (!actor || !placeOf(actor, ax, az)) continue;
        const float bearing = ta::bearingOf(px, pz, ax, az, forward[0], forward[1], right[0], right[1]);
        if (std::fabs(bearing) < half * 0.95f) continue;  // in sight: its sprite and its bar say it
        const float alpha = s.hits.alphaOf(id, now);
        ta::arc(float(width), float(height), bearing, alpha, s.verts);
        Label l;  // 3.0.19-beta.37: its distance by it
        ta::labelAt(float(width), float(height), bearing, l.x, l.y);
        l.alpha = alpha;
        l.cells = ta::cellsOf(std::hypot(ax - px, az - pz));
        s.labels.push_back(l);
        if (s.told < 10) {
            ++s.told;
            LOG_INFO("3D view: %u hits the character from %.0f degrees off the view's way - the threat's arc", id,
                     double(bearing * 180.0f / kPi));
        }
    }
    if (s.verts.empty()) return;
    DrawCmd cmd;
    cmd.state = record.addState(setup());
    cmd.primitive = d3d7::PT_TRIANGLELIST;
    cmd.fvf = d3d7::FVF_TL1;
    cmd.effect = uint8_t(effect_exe::Kind::None);
    record.draw(cmd, s.verts.data(), uint32_t(s.verts.size()), nullptr, 0);
}

void labels(double k, int targetWidth, int targetHeight) {
    State& s = state();
    Ui& u = ui();
    std::vector<FarV> now;
    const bool latin = language() == settings::Lang::En;
    for (const Label& l : s.labels)
        if (l.alpha > 0.0f && l.cells > 0)
            now.push_back(FarV{l.x * float(k), l.y * float(k), l.alpha, ta::metres(l.cells, latin)});
    if (!uaui::ready() || !s.on || k <= 0.0 || now.empty()) {
        u.doc.show(false);
        return;
    }
    if (!u.doc.made()) {
        u.doc.passive = true;
        if (!u.doc.make("threat", kDoc, [&](Rml::DataModelConstructor& m) {
                if (auto f = m.RegisterStruct<FarV>()) {
                    f.RegisterMember("x", &FarV::x);
                    f.RegisterMember("y", &FarV::y);
                    f.RegisterMember("op", &FarV::op);
                    f.RegisterMember("text", &FarV::text);
                }
                m.RegisterArray<std::vector<FarV>>();
                m.Bind("w", &u.w);
                m.Bind("h", &u.h);
                m.Bind("s", &u.s);
                m.Bind("dist", &u.dist);
            }))
            return;
    }
    if (u.w != float(targetWidth)) u.w = float(targetWidth), u.doc.dirty("w");
    if (u.h != float(targetHeight)) u.h = float(targetHeight), u.doc.dirty("h");
    if (u.s != float(k)) u.s = float(k), u.doc.dirty("s");
    u.dist = std::move(now);
    u.doc.dirty("dist");  // they fade each frame
    u.doc.place(0, 0);
    u.doc.show(true);
}

}  // namespace uagfx::win::threatarcwin

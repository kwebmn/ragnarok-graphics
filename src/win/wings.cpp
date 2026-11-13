#include "win/wings.hpp"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <string>

#include "core/frame.hpp"
#include "core/grf/grf.hpp"
#include "core/layout.hpp"
#include "core/sprite_exe.hpp"
#include "core/sprite_file.hpp"
#include "core/wings.hpp"
#include "win/context.hpp"
#include "win/entry_hook.hpp"
#include "win/game_files.hpp"
#include "win/game_memory.hpp"
#include "win/gpu/renderer.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"

namespace uagfx::win::wings {
namespace {

namespace cw = uagfx::wings;

// uaRO.exe's (the spec's «Facts»): the players' layered render (thiscall: the actor, the view matrix, a flag) and its
// first 9 bytes (push ebp; mov ebp, esp; sub esp, 0x6F4)
constexpr uint32_t kRender = 0x5AECC0;
constexpr uint8_t kRenderBytes[9] = {0x55, 0x8B, 0xEC, 0x81, 0xEC, 0xF4, 0x06, 0x00, 0x00};
constexpr uint32_t kViews[3] = {0x36C, 0x370, 0x374};  // its headgears' views
// its layers' resources (8, the body first; .236: the act named by the sprite), a resource's name, its action
// and frame, its size
constexpr uint32_t kSprites = 0x3C4, kResName = 0x14, kAction = 0x24, kFrame = 0x28, kSize = 0x3C;
// the view: a point's screen place and depth (FUN_00412C30), a depth's scale (FUN_00412D60), as the render asks them
constexpr uint32_t kViewObject = 0x6EE3D8, kProject = 0x412C30, kDepthScale = 0x412D60;
using RenderFn = void(__attribute__((thiscall)) *)(uint32_t actor, uint32_t matrix, int flag);
using ProjectFn = void(__attribute__((thiscall)) *)(uint32_t view, uint32_t pos, uint32_t matrix, float* x, float* y,
                                                     float* depth);
using DepthScaleFn = float(__attribute__((thiscall)) *)(uint32_t view, float depth);
// 3.0.18-beta.231: a headgear's sprite and act names from its view (thiscall on the session: two words, the sex, the
// view, the name's buffer; ret 0x14), their first 7 bytes (push ebp; mov ebp, esp; push ebx; mov ebx, [ebp+0x14]);
// the session's table of the names (a vector of pointers: first +0x880, last +0x884)
constexpr uint32_t kSprName = 0x62E970, kActName = 0x62EA20, kNamesFirst = 0x880, kNamesLast = 0x884;
constexpr uint8_t kNameBytes[7] = {0x55, 0x8B, 0xEC, 0x53, 0x8B, 0x5D, 0x14};
using NameFn = char*(__attribute__((thiscall)) *)(uint32_t session, uint32_t a, uint32_t b, int sex, int view,
                                                  char* out);

struct Garment {
    bool ok = false;
    spritefile::Sprite sprite;
    spritefile::Act act;
    std::vector<uint32_t> textures;  // per picture (the palette ones, then the true colour ones); 0 not made yet
};
struct Job {
    Garment* garment = nullptr;
    std::vector<cw::Quad> quads;
    std::vector<Quad> ready;  // with their textures, made at its first draw
    bool drawn = false;       // .235: its wings went to a draw this frame
};
struct State {
    uintptr_t render = 0;
    uintptr_t sprName = 0, actName = 0;  // 3.0.18-beta.231
    bool packTried = false, packOk = false;
    grf::Archive pack;
    std::unique_ptr<gamefiles::Reader> files;
    std::map<std::string, Garment> garments;                         // by look and the pack's name
    std::map<std::string, std::unique_ptr<spritefile::Act>> bodies;  // by the body act's name; null: not read
    cw::Frame frame;
    std::vector<Job> jobs;  // as the frame's jobs
    int current = -1;       // the wearer whose render runs now
    std::set<std::string> told;
    bool renderSeen = false, firstDrawn = false;  // .235: the log's proofs - the hook runs, the wings reach a draw
};

State& state() {
    static State s;
    return s;
}

std::string cString(uint32_t at) {  // a name in the game's memory, 256 bytes at most
    std::string out;
    char chunk[32];
    for (uint32_t n = 0; at && n < 8; ++n) {
        if (!readGame(at + n * 32, chunk, sizeof chunk)) break;
        if (const void* end = std::memchr(chunk, 0, sizeof chunk)) {
            out.append(chunk, size_t(static_cast<const char*>(end) - chunk));
            return out;
        }
        out.append(chunk, sizeof chunk);
    }
    return {};
}

std::string resName(uint32_t actor, uint32_t list) {
    const uint32_t res = read32(read32(actor + list));
    return res ? cString(res + kResName) : std::string();
}

bool packRead(State& s, const std::string& name, std::string& out) {
    if (!s.packTried) {
        s.packTried = true;
        std::string error;
        s.packOk = s.pack.openToRead(layout::join(gameDir(), layout::kWings), error);
        if (!s.packOk) LOG_INFO("wings: %s not read - %s; no wings drawn", layout::kWings, error.c_str());
    }
    std::vector<uint8_t> bytes;
    std::string error;
    if (!s.packOk || !s.pack.read(name, bytes, error)) return false;
    out.assign(bytes.begin(), bytes.end());
    return true;
}

Garment* garmentFor(State& s, cw::Look look, const std::string& body) {
    const cw::Set set = cw::setOf(look);
    const std::string spr = cw::garmentEntry(set, body, "spr"), act = cw::garmentEntry(set, body, "act");
    if (spr.empty()) {  // .235: told - R-TT95RZ had no line of why
        if (s.told.insert("not a body " + body).second)
            LOG_INFO("wings: %s worn on the body «%s» - not a human body's sprite, none drawn", cw::lookName(look),
                     body.c_str());
        return nullptr;
    }
    auto [it, fresh] = s.garments.try_emplace(std::string(cw::lookName(look)) + "|" + spr);
    Garment& g = it->second;
    if (fresh) {
        std::string sprBytes, actBytes;
        g.ok = packRead(s, spr, sprBytes) && packRead(s, act, actBytes) && spritefile::readSpr(sprBytes, g.sprite) &&
               spritefile::readAct(actBytes, g.act);
        if (g.ok) {
            cw::recolour(g.sprite, look);
            g.textures.assign(g.sprite.palette.size() + g.sprite.truecolour.size(), 0);
        } else if (s.packOk && s.told.insert("no " + spr).second) {
            LOG_INFO("wings: no garment for the body %s (%s) - none drawn on it", body.c_str(), spr.c_str());
        }
    }
    return g.ok ? &g : nullptr;
}

const spritefile::Act* bodyAct(State& s, const std::string& name) {
    auto [it, fresh] = s.bodies.try_emplace(name);
    if (fresh) {
        if (!s.files) s.files = std::make_unique<gamefiles::Reader>(gameDir());
        std::vector<uint8_t> bytes;
        auto act = std::make_unique<spritefile::Act>();
        if (!name.empty() && s.files->read(cw::dataPath(name), bytes) &&  // .235: the game keeps "sprite\..."
            spritefile::readAct(std::string(bytes.begin(), bytes.end()), *act))
            it->second = std::move(act);
        else if (s.told.insert("body " + name).second)
            LOG_INFO("wings: the body's %s not read - no wings on it", name.c_str());
    }
    return it->second.get();
}

// A wearer's wings at the start of its render: where the game puts it now, its garment's frame at its action and frame
void startJob(State& s, uint32_t actor, uint32_t matrix) {
    if (!Context::get().enhanced()) return;
    int views[3];
    for (int i = 0; i < 3; ++i) views[i] = int(read32(actor + kViews[i]));
    if (!s.renderSeen) {  // .235: the hook's proof (FUN_005AECC0 draws the players alone: no vtable asked any more)
        s.renderSeen = true;
        LOG_INFO("wings: the players' render runs (the first: vtable 0x%X, headgear views %d, %d, %d)",
                 read32(actor), views[0], views[1], views[2]);
    }
    const cw::Look look = cw::lookOfViews(views);
    if (look == cw::Look::None) return;
    const std::string body = resName(actor, kSprites);
    Garment* g = garmentFor(s, look, body);
    // .236 (R-4U3RD7): the body's act by its sprite's name - the list taken for the acts held the sprites
    const spritefile::Act* bact = g ? bodyAct(s, cw::actOf(body)) : nullptr;
    const uint32_t view = read32(kViewObject);
    float size = 0;
    if (!bact) return;
    if (!view || !readGame(actor + kSize, &size, 4) || !(size > 0.0f && size < 100.0f)) {
        if (s.told.insert("no size").second)
            LOG_INFO("wings: %s on «%s» - no size (%.3f) or view (0x%X) to place it, none drawn", cw::lookName(look),
                     body.c_str(), double(size), view);
        return;
    }
    float x = 0, y = 0, depth = 0;
    reinterpret_cast<ProjectFn>(uintptr_t(kProject))(view, actor + 4, matrix, &x, &y, &depth);
    const float scale = size * reinterpret_cast<DepthScaleFn>(uintptr_t(kDepthScale))(view, depth);
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(scale) || !(scale > 0.0f)) {
        if (s.told.insert("no place").second)
            LOG_INFO("wings: %s on «%s» - the game gave no screen place (%.1f, %.1f, scale %.3f), none drawn",
                     cw::lookName(look), body.c_str(), double(x), double(y), double(scale));
        return;
    }
    const uint32_t action = read32(actor + kAction), frame = read32(actor + kFrame);
    Job job;
    job.garment = g;
    job.quads = cw::quads(g->act, *bact, g->sprite, action, frame, cw::Transform{x, y, scale});
    if (job.quads.empty()) {
        if (s.told.insert("no frame " + std::to_string(action)).second)
            LOG_INFO("wings: %s on «%s» - its garment has no frame for action %u frame %u, none drawn",
                     cw::lookName(look), body.c_str(), action, frame);
        return;
    }
    s.current = s.frame.begin(actor, cw::facesAway(action), y);  // .237: its faces' depths compared at its feet
    s.jobs.push_back(std::move(job));
    if (s.told.insert(std::string("on ") + cw::lookName(look) + "|" + body).second)
        LOG_INFO("wings: %s on %s (views %d, %d, %d) at %.0f, %.0f scale %.2f, %u pictures - waiting for its faces",
                 cw::lookName(look), body.c_str(), views[0], views[1], views[2], double(x), double(y), double(scale),
                 unsigned(s.jobs.back().quads.size()));
}

void __attribute__((thiscall)) playerRender(uint32_t actor, uint32_t matrix, int flag) {
    State& s = state();
    s.current = -1;
    startJob(s, actor, matrix);
    reinterpret_cast<RenderFn>(s.render)(actor, matrix, flag);
    s.current = -1;  // its faces end with its render
}

// 3.0.18-beta.231: a view past the game's names asks for no sprite (the game's way for none) - the 2007 client read
// another table's word for it and showed «Cannot find File» for each of its files
char* namedOrNone(uintptr_t original, uint32_t session, uint32_t a, uint32_t b, int sex, int view, char* out) {
    const uint32_t first = read32(session + kNamesFirst), last = read32(session + kNamesLast);
    if (!cw::pastTable(view, first, last)) return reinterpret_cast<NameFn>(original)(session, a, b, sex, view, out);
    if (out) *out = 0;
    State& s = state();
    if (s.told.insert("past " + std::to_string(view)).second)
        LOG_INFO("wings: the game's headgear view %d is past its %u names - no sprite asked for it (no «Cannot find "
                 "File»)", view, first && last >= first ? unsigned((last - first) / 4) : 0u);
    return out;
}

char* __attribute__((thiscall)) sprName(uint32_t session, uint32_t a, uint32_t b, int sex, int view, char* out) {
    return namedOrNone(state().sprName, session, a, b, sex, view, out);
}

char* __attribute__((thiscall)) actName(uint32_t session, uint32_t a, uint32_t b, int sex, int view, char* out) {
    return namedOrNone(state().actName, session, a, b, sex, view, out);
}

uint32_t textureOf(Garment& g, const cw::Quad& q) {
    const size_t at = q.type == 1 ? g.sprite.palette.size() + size_t(q.image) : size_t(q.image);
    if (q.image < 0 || at >= g.textures.size()) return 0;
    if (g.textures[at]) return g.textures[at];
    const spritefile::Image& im =
        q.type == 1 ? g.sprite.truecolour[size_t(q.image)] : g.sprite.palette[size_t(q.image)];
    Context& c = Context::get();
    if (!c.renderer || im.w <= 0 || im.h <= 0) return 0;
    const uint32_t id = c.renderer->createTexture(im.w, im.h, 1);
    if (!id) return 0;
    // a sprite's frame for the renderer (its light as a player's sprite)
    const std::vector<SpriteCell> slots{SpriteCell{Rect{0, 0, im.w, im.h}, uint8_t(sprite_exe::Source::Player)}};
    c.record.uploadOwn(id, 0, Rect{0, 0, im.w, im.h}, im.argb.data(), slots);
    g.textures[at] = id;
    return id;
}

const std::vector<Quad>& ready(Job& j) {
    j.drawn = true;
    State& s = state();
    if (!s.firstDrawn) {
        s.firstDrawn = true;
        LOG_INFO("wings: the first wings drawn with a wearer's faces");
    }
    if (j.ready.empty())
        for (const cw::Quad& q : j.quads)
            if (const uint32_t tex = textureOf(*j.garment, q)) j.ready.push_back(Quad{q.x0, q.y0, q.x1, q.y1, tex,
                                                                                       q.mirror, q.tint});
    return j.ready;
}

}  // namespace

bool install() {
    State& s = state();
    if (s.render) return true;
    // 3.0.18-beta.231: the headgears' views past the game's names - no sprite asked for
    if (hookEntryBytes(kSprName, kNameBytes, sizeof kNameBytes, reinterpret_cast<const void*>(&sprName), s.sprName) &&
        hookEntryBytes(kActName, kNameBytes, sizeof kNameBytes, reinterpret_cast<const void*>(&actName), s.actName))
        LOG_INFO("wings: the headgears' names hooked (uaRO.exe 0x%X, 0x%X) - a view past the game's names asks for no "
                 "sprite", kSprName, kActName);
    else
        LOG_INFO("wings: the headgears' names are not where this build of uaRO.exe has them - left as the game has "
                 "them");
    if (!hookEntryBytes(kRender, kRenderBytes, sizeof kRenderBytes, reinterpret_cast<const void*>(&playerRender),
                        s.render)) {
        LOG_INFO("wings: the players' render is not where this build of uaRO.exe has it - no wings");
        return false;
    }
    LOG_INFO("wings: the players' render hooked (uaRO.exe 0x%X) - the nine headgears' wings drawn", kRender);
    return true;
}

void noteFace(uint32_t face, bool blended) {
    State& s = state();
    if (s.current < 0 || !face) return;
    // .237 (R-FAFJWS «мигает тело персонажа»): its depth from its vertices, as the game made them before the sort
    const uint32_t vertices = read32(face), count = std::min<uint32_t>(read32(face + 4), 4);
    uint8_t bytes[4 * 32] = {};
    const bool read = vertices && count && readGame(vertices, bytes, size_t(count) * 32);
    s.frame.face(s.current, vertices, read ? cw::faceDepth(bytes, count) : cw::Depth{}, blended);
}

void frameStart() {
    State& s = state();
    for (const Job& j : s.jobs)  // .235: worked out but never drawn - its faces unseen (the face sort hook's)
        if (!j.drawn && s.told.insert("undrawn").second)
            LOG_INFO("wings: a wearer's wings were worked out but none of its faces came to a draw - none drawn");
    s.frame.clear();
    s.jobs.clear();
    s.current = -1;
}

Emit drawn(uint32_t vertices) {
    State& s = state();
    Emit e;
    if (s.jobs.empty()) return e;
    const cw::Frame::Emit at = s.frame.drawn(vertices);
    if (at.before >= 0 && size_t(at.before) < s.jobs.size()) e.before = &ready(s.jobs[size_t(at.before)]);
    if (at.after >= 0 && size_t(at.after) < s.jobs.size()) e.after = &ready(s.jobs[size_t(at.after)]);
    e.depth = at.depth;  // .237
    if ((e.before || e.after) && s.told.insert(e.before ? "depth behind" : "depth over").second) {
        // .237 (R-FAFJWS): the proof - the wings' depth against the face they go to (view units, top - bottom)
        uint8_t bytes[4 * 32] = {};
        const bool read = readGame(vertices, bytes, sizeof bytes);
        const cw::Depth face = read ? cw::faceDepth(bytes, 4) : cw::Depth{};
        const auto w = [](float rhw) { return rhw > 0.0f ? 1.0f / rhw : 0.0f; };
        LOG_INFO("wings: %s - their depth %.1f - %.1f, the face they go %s %.1f - %.1f%s",
                 e.before ? "behind every face of the wearer" : "over every face of the wearer",
                 double(w(e.depth.rhwTop)), double(w(e.depth.rhwBottom)), e.before ? "before" : "after",
                 double(w(face.rhwTop)), double(w(face.rhwBottom)), e.depth.known() ? "" : " (none known: its own)");
    }
    return e;
}

}  // namespace uagfx::win::wings

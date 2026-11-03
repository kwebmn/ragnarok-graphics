#include "win/cursor_look.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include "core/cursor_art.hpp"
#include "core/ui_style.hpp"
#include "core/window_panel.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"

namespace uagfx::win::cursorlook {
namespace {

namespace ca = uagfx::cursorart;

// uaRO.exe's: the mode now (CModeMgr's), its cursor action (cursors.act's; FUN_004f5a50 sets it)
constexpr uint32_t kModeNow = 0x74B57C, kCursorAction = 0x38;
constexpr float kSize = 34;      // the shapes' 24-unit box at the interface's size 1: the arrow as tall as the game's
constexpr int kLookBack = 24;    // the cursor's faces among the frame's last draws
constexpr float kMostSide = 96;  // a face of it no bigger
constexpr float kNear = 110;     // nor farther off its point (cursors.act's gem falls from 80 over it)

struct Slot {
    ca::Art art;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    bool made = false;
};

struct State {
    CursorLook look = CursorLook::Game;
    float k = 0, size = 0;
    Slot slots[int(ca::Shape::kCount)];
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

// The game's cursor's faces near its point (x, y): small interface faces all of its colour, the last ones first
std::vector<size_t> cursorFaces(const FrameRecord& r, float x, float y) {
    std::vector<size_t> out;
    int seen = 0;
    for (size_t c = r.commands().size(); c-- > 0 && seen < kLookBack;) {
        if (r.commands()[c].type != Cmd::Draw) continue;
        ++seen;
        const DrawCmd& d = r.draws()[r.commands()[c].index];
        const uint32_t stride = FrameRecord::stride(d.fvf);
        if (!stride || d.vertexCount < 3 || d.vertexCount > 6 || d.state >= r.states().size() ||
            !r.states()[d.state].ui || d.vertexOffset + size_t(d.vertexCount) * stride > r.vertexBytes().size())
            continue;
        float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
        bool coloured = true;
        for (uint32_t v = 0; v < d.vertexCount && coloured; ++v) {
            const uint8_t* at = r.vertexBytes().data() + d.vertexOffset + size_t(v) * stride;
            float p[2];
            uint32_t colour = 0;
            std::memcpy(p, at, sizeof p);
            std::memcpy(&colour, at + 16, 4);  // FVF_TL1 and TL2: x y z rhw, then the diffuse colour
            coloured = colour == panel::kCursorColour;
            x0 = std::min(x0, p[0]), y0 = std::min(y0, p[1]), x1 = std::max(x1, p[0]), y1 = std::max(y1, p[1]);
        }
        if (coloured && x1 - x0 <= kMostSide && y1 - y0 <= kMostSide && x >= x0 - kNear && x <= x1 + kNear &&
            y >= y0 - kNear && y <= y1 + kNear)
            out.push_back(c);
    }
    return out;  // the last first: dropped from the end
}

}  // namespace

void frame(CursorLook look, double k) {
    State& s = state();
    const float size = float(std::lround(kSize * uistyle::partScale(k)));
    if (look != s.look || size != s.size) {
        if (look != s.look) LOG_INFO("cursor: [interface] cursor = %s", cursorLookToken(look));
        s.look = look;
        s.size = size;
        for (Slot& slot : s.slots) slot.made = false;  // made again at their next showing
    }
    s.k = float(k);
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (s.look == CursorLook::Game || s.k <= 0 || s.size <= 0) return;
    int cx = 0, cy = 0;
    if (!readGameCursor(cx, cy)) return;
    const std::vector<size_t> faces = cursorFaces(record, float(cx), float(cy));
    if (faces.empty()) return;  // the game drew none: none of ours either
    for (const size_t c : faces) record.dropCommand(c);
    const uint32_t mode = read32(kModeNow);
    const int action = mode ? int(read32(mode + kCursorAction)) : 0;
    const ca::Shape shape = ca::shapeOf(action);
    Slot& slot = s.slots[int(shape)];
    if (!slot.made) {
        slot.art = ca::make(shape, s.look, s.size);
        slot.made = slot.upload = true;
    }
    if (slot.art.picture.width <= 0) return;
    drawPart(record, renderer, slot.art.picture, slot.texture, slot.texW, slot.texH, slot.upload,
             int(std::lround(float(cx) * s.k - slot.art.hotX)), int(std::lround(float(cy) * s.k - slot.art.hotY)),
             s.k);
    if (s.told < 3) {
        ++s.told;
        LOG_INFO("cursor: the game's cursor (%zu face(s), its action %d) drawn as the interface's %s", faces.size(),
                 action, cursorLookToken(s.look));
    }
}

}  // namespace uagfx::win::cursorlook

#include "win/sharp_frames.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <unordered_map>

#include "core/d3d7.hpp"
#include "core/hash64.hpp"
#include "core/sharp_frames.hpp"
#include "core/sprite_exe.hpp"
#include "win/context.hpp"
#include "win/gpu/renderer.hpp"
#include "win/log.hpp"

namespace uagfx::win::sharpframes {
namespace {

namespace sf = ::uagfx::sharpframes;

constexpr size_t kMostPictures = 256;  // the three files hold 22 frames; a bound against the unforeseen

struct Entry {
    int w = 0, h = 0;
    std::vector<uint32_t> argb;  // the frame as the game wrote it, until its copy is made
    uint32_t texture = 0;
    int levels = 0, copyW = 0, copyH = 0;  // 3.0.18-beta.185: the copy's size (16 or 4 times the frame's)
    bool failed = false;  // no copy or no texture: drawn as the game's
};

// The game's thread alone: it writes its sprites and draws them itself (sprite_hook.hpp)
std::unordered_map<uint64_t, Entry, Hash64> g_entries;
unsigned g_noted = 0, g_drawn = 0, g_missed = 0;  // since the last stats line
bool g_toldNote = false, g_toldDraw = false, g_toldMiss = false;

// The copy made and sent to the card (once): false when it cannot be
bool send(Context& c, Entry& e) {
    const sf::Copy copy = sf::makeCopy(e.argb.data(), e.w, e.h);
    if (copy.levels.empty()) return false;
    e.texture = c.renderer->createTexture(copy.width, copy.height, int(copy.levels.size()));
    if (!e.texture) {
        LOG_INFO("sharp numbers: no texture for a %dx%d copy - that frame drawn as the game's", copy.width,
                 copy.height);
        return false;
    }
    int lw = copy.width, lh = copy.height;
    for (size_t level = 0; level < copy.levels.size(); ++level) {
        // level 0 tells the renderer it is a sprite's frame, as the game's cell did (its light, no leaves' sway)
        std::vector<SpriteCell> slots;
        if (level == 0) slots.push_back(SpriteCell{Rect{0, 0, lw, lh}, uint8_t(sprite_exe::Source::Effect)});
        c.record.uploadOwn(e.texture, uint32_t(level), Rect{0, 0, lw, lh}, copy.levels[level].data(), slots);
        lw = std::max(1, lw / 2);
        lh = std::max(1, lh / 2);
    }
    e.levels = int(copy.levels.size());
    e.copyW = copy.width, e.copyH = copy.height;
    e.argb.clear();
    e.argb.shrink_to_fit();
    return true;
}

}  // namespace

uint64_t note(const uint32_t* argb, int w, int h) {
    if (!argb || w <= 0 || h <= 0 || w > sf::kMostSide || h > sf::kMostSide) return 0;
    const uint64_t key = sf::keyOf(argb, w, h);
    ++g_noted;
    if (g_entries.count(key)) return key;
    if (g_entries.size() >= kMostPictures) return 0;
    Entry& e = g_entries[key];
    e.w = w;
    e.h = h;
    e.argb.assign(argb, argb + size_t(w) * size_t(h));
    if (!g_toldNote) {
        g_toldNote = true;
        LOG_INFO("sharp numbers: the first number's frame the game wrote into its sprite cache - %dx%d", w, h);
    }
    return key;
}

bool swap(const sprite_cells::Cell& cell, int texW, int texH, DrawCmd& cmd, DrawSetup& setup, const void*& vertices,
          uint32_t count, uint32_t fvf, std::vector<uint8_t>& scratch) {
    Context& c = Context::get();
    if (!cell.sharp || !c.config.crispNumbers || !c.enhanced() || !c.renderer || !vertices || !count) return false;
    if ((fvf != d3d7::FVF_TL1 && fvf != d3d7::FVF_TL2) || setup.stage[0].texCoord != 0) return false;
    const auto it = g_entries.find(cell.sharp);
    if (it == g_entries.end() || it->second.failed) return false;
    Entry& e = it->second;
    const Rect frame{cell.rect.left, cell.rect.top, cell.rect.left + cell.frameW, cell.rect.top + cell.frameH};
    const uint32_t stride = uint32_t(FrameRecord::stride(fvf));
    const auto* bytes = static_cast<const uint8_t*>(vertices);
    scratch.assign(bytes, bytes + size_t(count) * stride);
    if (!sf::remap(scratch.data(), count, stride, texW, texH, frame)) {
        ++g_missed;
        if (!g_toldMiss)
            LOG_INFO("sharp numbers: a draw at a number's cell reaches past its %dx%d frame - drawn as the game's",
                     int(cell.frameW), int(cell.frameH));
        g_toldMiss = true;
        return false;
    }
    if (!e.texture && !send(c, e)) {
        e.failed = true;
        return false;
    }
    cmd.texture[0] = e.texture;
    cmd.sharpCopy = true;
    StageSetup& st = setup.stage[0];  // smooth through the copy's levels, its edges not wrapped round
    st.magFilter = d3d7::TFG_LINEAR;
    st.minFilter = d3d7::TFN_LINEAR;
    st.mipFilter = d3d7::TFP_LINEAR;
    st.addressU = st.addressV = d3d7::TADDRESS_CLAMP;
    vertices = scratch.data();
    ++g_drawn;
    if (!g_toldDraw) {  // the proof: the game's quad, how large it shows, and in what part of the frame
        g_toldDraw = true;
        float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
        for (uint32_t i = 0; i < count; ++i) {
            float xy[2];
            std::memcpy(xy, scratch.data() + size_t(i) * stride, sizeof xy);
            x0 = std::min(x0, xy[0]), x1 = std::max(x1, xy[0]), y0 = std::min(y0, xy[1]), y1 = std::max(y1, xy[1]);
        }
        LOG_INFO("sharp numbers: the first drawn from its copy - the game's %dx%d frame, its copy %dx%d in %d "
                 "levels, the quad %.0fx%.0f of the game's pixels (%s)",
                 e.w, e.h, e.copyW, e.copyH, e.levels, x1 - x0, y1 - y0,
                 setup.ui ? "with the interface" : "with the world");
    }
    return true;
}

std::string stats() {
    if (g_entries.empty()) return "";
    char line[160];
    std::snprintf(line, sizeof line, "; sharp numbers: %u pictures, %u draws from their copies, %u frames written",
                  unsigned(g_entries.size()), g_drawn, g_noted);
    std::string s = line;
    if (g_missed) {
        std::snprintf(line, sizeof line, ", %u past a frame", g_missed);
        s += line;
    }
    g_noted = g_drawn = g_missed = 0;
    return s;
}

}  // namespace uagfx::win::sharpframes

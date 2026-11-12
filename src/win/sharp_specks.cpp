#include "win/sharp_specks.hpp"

#include <algorithm>
#include <cstdio>
#include <unordered_map>
#include <vector>

#include "core/d3d7.hpp"
#include "core/hash64.hpp"
#include "core/sharp_frames.hpp"
#include "core/sharp_specks.hpp"
#include "core/sprite_exe.hpp"
#include "win/context.hpp"
#include "win/gpu/renderer.hpp"
#include "win/log.hpp"

namespace uagfx::win::sharpspecks {
namespace {

namespace ss = ::uagfx::sharpspecks;

constexpr size_t kMostTextures = 512, kMostPictures = 64;  // Storm Gust has eleven; a bound against the unforeseen

struct Entry {
    uint32_t texture = 0;  // the copy's (0: none - not a sheet of snow, or no texture for it)
    int w = 0, h = 0, copyW = 0, copyH = 0, levels = 0, specks = 0;
};

// The game's thread alone
std::unordered_map<uint32_t, uint64_t> g_byTexture;  // the game's texture -> its picture's key
std::unordered_map<uint64_t, Entry, Hash64> g_pictures;
unsigned g_sheets = 0, g_drawn = 0;
int g_toldSheet = 0;
bool g_toldDraw = false;

// The copy made and sent to the card: false when it cannot be
bool send(Context& c, Entry& e, const ss::Copy& copy) {
    e.texture = c.renderer->createTexture(copy.width, copy.height, int(copy.levels.size()));
    if (!e.texture) {
        LOG_INFO("storm gust: no texture for a %dx%d copy of its snow - drawn as the game's", copy.width,
                 copy.height);
        return false;
    }
    int lw = copy.width, lh = copy.height;
    for (size_t level = 0; level < copy.levels.size(); ++level) {
        // level 0 tells the renderer it is an effect's picture, as the game's texture was
        std::vector<SpriteCell> slots;
        if (level == 0) slots.push_back(SpriteCell{Rect{0, 0, lw, lh}, uint8_t(sprite_exe::Source::Effect)});
        c.record.uploadOwn(e.texture, uint32_t(level), Rect{0, 0, lw, lh}, copy.levels[level].data(), slots);
        lw = std::max(1, lw / 2);
        lh = std::max(1, lh / 2);
    }
    e.copyW = copy.width, e.copyH = copy.height, e.levels = int(copy.levels.size());
    return true;
}

}  // namespace

bool wanted(uint32_t texture, bool changed) { return texture && (changed || !g_byTexture.count(texture)); }

void note(uint32_t texture, const uint32_t* argb, int w, int h) {
    if (!texture || !argb || w <= 0 || h <= 0) return;
    Context& c = Context::get();
    if (g_byTexture.size() >= kMostTextures) g_byTexture.clear();
    const uint64_t key = ::uagfx::sharpframes::keyOf(argb, w, h);
    g_byTexture[texture] = key;
    if (g_pictures.count(key) || g_pictures.size() >= kMostPictures) return;
    Entry& e = g_pictures[key];
    e.w = w, e.h = h;
    const ss::Copy copy = ss::makeCopy(argb, w, h);
    if (copy.levels.empty() || !c.renderer || !send(c, e, copy)) return;  // not snow: as the game draws it
    e.specks = copy.specks;
    ++g_sheets;
    if (g_toldSheet < 4) {
        ++g_toldSheet;
        LOG_INFO("storm gust: a sheet of its snow (%dx%d, %d flakes) drawn anew at real resolution - its copy %dx%d "
                 "in %d levels",
                 w, h, copy.specks, copy.width, copy.height, e.levels);
    }
}

bool swap(DrawCmd& cmd, DrawSetup& setup, const char* look) {
    Context& c = Context::get();
    if (!cmd.texture[0] || !c.renderer || !c.enhanced()) return false;
    // the snow is light added on (SRCALPHA, ONE); anything else of it as the game draws it
    if (!setup.blend || setup.dstBlend != d3d7::BLEND_ONE || setup.stage[0].texCoord != 0) return false;
    const auto at = g_byTexture.find(cmd.texture[0]);
    if (at == g_byTexture.end()) return false;
    const auto it = g_pictures.find(at->second);
    if (it == g_pictures.end() || !it->second.texture) return false;
    const Entry& e = it->second;
    cmd.texture[0] = e.texture;
    cmd.sharpCopy = true;
    StageSetup& st = setup.stage[0];  // smooth through the copy's levels
    st.magFilter = d3d7::TFG_LINEAR;
    st.minFilter = d3d7::TFN_LINEAR;
    st.mipFilter = d3d7::TFP_LINEAR;
    ++g_drawn;
    if (!g_toldDraw) {  // the proof
        g_toldDraw = true;
        LOG_INFO("storm gust: the first snow drawn from its copy - the game's %dx%d sheet, %d flakes, its copy %dx%d "
                 "(%s)",
                 e.w, e.h, e.specks, e.copyW, e.copyH, look ? look : "");
    }
    return true;
}

std::string stats() {
    if (!g_sheets) return "";
    char line[120];
    std::snprintf(line, sizeof line, "; storm gust's snow: %u sheets, %u draws from their copies", g_sheets, g_drawn);
    g_drawn = 0;
    return line;
}

}  // namespace uagfx::win::sharpspecks

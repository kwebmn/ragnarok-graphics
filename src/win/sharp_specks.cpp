#include "win/sharp_specks.hpp"

#include <windows.h>

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

// The skills' textures looked at (a key each), the sheets' copies made (a few dozen at most: Storm Gust's snow, the
// sparkles 192/204/205/219 share; ~1.4 MB of the card's each); a bound against the unforeseen
constexpr size_t kMostTextures = 2048, kMostPictures = 64;
constexpr ULONGLONG kNoteEveryMs = 16;  // one new texture a frame: a skill of many new ones spread over a few

struct Entry {
    uint32_t texture = 0;  // the copy's (0: none - not a sheet of specks, or no texture for it)
    int w = 0, h = 0, copyW = 0, copyH = 0, levels = 0, specks = 0;
    unsigned effect = 0;
};

// The game's thread alone
std::unordered_map<uint32_t, uint64_t> g_byTexture;  // the game's texture -> its picture's key
std::unordered_map<uint64_t, Entry, Hash64> g_pictures;
unsigned g_sheets = 0, g_drawn = 0;
int g_toldSheet = 0;
bool g_toldDraw = false;
ULONGLONG g_lastNote = 0;

// The copy made and sent to the card: false when it cannot be
bool send(Context& c, Entry& e, const ss::Copy& copy) {
    e.texture = c.renderer->createTexture(copy.width, copy.height, int(copy.levels.size()));
    if (!e.texture) {
        LOG_INFO("skills' specks: no texture for a %dx%d copy - drawn as the game's", copy.width, copy.height);
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

bool wanted(uint32_t texture, bool changed) {
    if (!texture || (!changed && g_byTexture.count(texture))) return false;
    const ULONGLONG now = GetTickCount64();
    if (now - g_lastNote < kNoteEveryMs) return false;  // the next frame's
    g_lastNote = now;
    return true;
}

void note(uint32_t texture, const uint32_t* argb, int w, int h, unsigned effect) {
    if (!texture || !argb || w <= 0 || h <= 0) return;
    Context& c = Context::get();
    if (g_byTexture.size() >= kMostTextures) g_byTexture.clear();
    const uint64_t key = ::uagfx::sharpframes::keyOf(argb, w, h);
    g_byTexture[texture] = key;
    if (g_pictures.count(key) || g_pictures.size() >= kMostPictures) return;
    Entry& e = g_pictures[key];
    e.w = w, e.h = h, e.effect = effect;
    const ss::Copy copy = ss::makeCopy(argb, w, h);
    if (copy.levels.empty() || !c.renderer || !send(c, e, copy)) return;  // no specks: as the game draws it
    e.specks = copy.specks;
    ++g_sheets;
    if (g_toldSheet < 8) {
        ++g_toldSheet;
        LOG_INFO("skills' specks: effect %u's sheet (%dx%d, %d specks) drawn anew at real resolution - its copy %dx%d "
                 "in %d levels",
                 effect, w, h, copy.specks, copy.width, copy.height, e.levels);
    }
}

bool swap(DrawCmd& cmd, DrawSetup& setup, const char* look) {
    Context& c = Context::get();
    if (!cmd.texture[0] || !c.renderer || !c.enhanced()) return false;
    // the specks are light added on (SRCALPHA, ONE); anything else as the game draws it
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
        LOG_INFO("skills' specks: the first drawn from a copy - effect %u's %dx%d sheet, %d specks, its copy %dx%d "
                 "(the skills' look: %s)",
                 e.effect, e.w, e.h, e.specks, e.copyW, e.copyH, look ? look : "");
    }
    return true;
}

std::string stats() {
    if (!g_sheets) return "";
    char line[120];
    std::snprintf(line, sizeof line, "; the skills' specks: %u sheets, %u draws from their copies", g_sheets, g_drawn);
    g_drawn = 0;
    return line;
}

}  // namespace uagfx::win::sharpspecks

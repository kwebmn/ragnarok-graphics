#include "win/sharp_specks.hpp"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <deque>
#include <unordered_map>
#include <vector>

#include "core/d3d7.hpp"
#include "core/hash64.hpp"
#include "core/sharp_art.hpp"
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
// sparkles 192/204/205/219 share; ~1.4 MB of the card's each); a bound on the copies against the unforeseen
constexpr size_t kMostTextures = 2048, kMostPictures = 64;
constexpr size_t kMostJudged = 8192;  // pictures remembered as neither (their keys alone)
// 3.0.20-beta.68: the painted pictures' copies - 240 such pictures in the whole game's data (a 64 x 64 one's copy with
// its levels 0.35 MB of the card's, a 128 x 128 one's 1.4 MB): this many texels of copies at most, the rest as the
// game draws them
constexpr size_t kMostArtPictures = 384;
constexpr uint64_t kMostArtBytes = 64ull << 20;
// a copy in the making: this many of the picture's texels a frame (about a millisecond), a frame apart; the pictures
// waiting their turn (a buff's first show brings up to sixteen at once - Kyrie's)
constexpr int kStepTexels = 768;
constexpr ULONGLONG kWorkEveryMs = 12;
constexpr size_t kMostWaiting = 32;
constexpr ULONGLONG kNoteEveryMs = 16;  // one new texture a frame: a skill of many new ones spread over a few

struct Entry {
    uint32_t texture = 0;  // the copy's (0: none - not a sheet of specks nor a painted picture, or no texture for it)
    int w = 0, h = 0, copyW = 0, copyH = 0, levels = 0, specks = 0;
    unsigned effect = 0;
    bool art = false;  // 3.0.20-beta.68: a painted picture's (core/sharp_art), not a sheet of specks
};

// The game's thread alone
std::unordered_map<uint32_t, uint64_t> g_byTexture;  // the game's texture -> its picture's key
std::unordered_map<uint64_t, Entry, Hash64> g_pictures;
unsigned g_sheets = 0, g_drawn = 0;
int g_toldSheet = 0;
bool g_toldDraw = false;
unsigned g_arts = 0, g_artDrawn = 0, g_artLeft = 0;  // 3.0.20-beta.68: the painted pictures' copies, their draws
uint64_t g_artBytes = 0;
int g_toldArt = 0;
bool g_toldArtDraw = false, g_toldArtFull = false;
struct Waiting {
    uint64_t key = 0;
    ::uagfx::sharpart::CopyJob job;
    ::uagfx::sharpart::Facts facts;
    ULONGLONG since = 0;
    int steps = 0;
};
std::deque<Waiting> g_waiting;  // the copies in the making, the first in hand
ULONGLONG g_lastWork = 0;
ULONGLONG g_lastNote = 0;

// The copy made and sent to the card: false when it cannot be
template <typename Copy>
bool send(Context& c, Entry& e, const Copy& copy) {
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
    if (g_pictures.count(key)) return;
    // 3.0.20-beta.68: a painted picture (the specks' sheets are light on black - never both)
    if (::uagfx::sharpart::painted(argb, w, h)) {
        if (g_arts >= kMostArtPictures || g_artBytes >= kMostArtBytes) {
            ++g_artLeft;
            if (!g_toldArtFull) {
                g_toldArtFull = true;
                LOG_INFO("skills' painted pictures: %u copies, %.0f MB of the card's - the next ones as the game draws "
                         "them", g_arts, double(g_artBytes) / 1048576.0);
            }
            return;
        }
        if (g_waiting.size() >= kMostWaiting) {  // looked at again once the ones in the making are done
            g_byTexture.erase(texture);
            return;
        }
        Entry& e = g_pictures[key];  // no texture yet: as the game draws it till its copy is made (work)
        e.w = w, e.h = h, e.effect = effect, e.art = true;
        Waiting job;
        job.key = key;
        job.job = ::uagfx::sharpart::CopyJob(argb, w, h);
        job.facts = ::uagfx::sharpart::factsOf(argb, w, h);
        job.since = GetTickCount64();
        g_waiting.push_back(std::move(job));
        return;
    }
    // 3.0.20-beta.68: the bound is on the sheets' copies. It had counted every picture looked at, a sheet or not:
    // the sixty-fifth picture of the skills in a game was never looked at - Storm Gust's snow stayed the game's after
    // an hour of other skills (found reading this for the painted pictures)
    if (g_sheets >= kMostPictures) return;
    const ss::Copy copy = ss::makeCopy(argb, w, h);
    if (copy.levels.empty()) {  // no specks: as the game draws it, and not judged again
        if (g_pictures.size() < kMostJudged) g_pictures[key] = Entry{};
        return;
    }
    if (!c.renderer) return;
    Entry& e = g_pictures[key];
    e.w = w, e.h = h, e.effect = effect;
    if (!send(c, e, copy)) return;
    e.specks = copy.specks;
    ++g_sheets;
    if (g_toldSheet < 8) {
        ++g_toldSheet;
        LOG_INFO("skills' specks: effect %u's sheet (%dx%d, %d specks) drawn anew at real resolution - its copy %dx%d "
                 "in %d levels",
                 effect, w, h, copy.specks, copy.width, copy.height, e.levels);
    }
}

void work() {
    if (g_waiting.empty()) return;
    const ULONGLONG now = GetTickCount64();
    if (now - g_lastWork < kWorkEveryMs) return;
    g_lastWork = now;
    Waiting& first = g_waiting.front();
    ++first.steps;
    if (!first.job.step(kStepTexels)) return;
    const ::uagfx::sharpframes::Copy copy = first.job.take();
    const auto it = g_pictures.find(first.key);
    Context& c = Context::get();
    if (it != g_pictures.end() && !copy.levels.empty() && c.renderer && send(c, it->second, copy)) {
        Entry& e = it->second;
        ++g_arts;
        for (const auto& level : copy.levels) g_artBytes += level.size() * 4;
        if (g_toldArt < 16) {
            ++g_toldArt;
            LOG_INFO("skills' painted pictures: effect %u's %dx%d picture (pixel art on a key: %d %% keyed out, "
                     "%d %% of the rest in flat colour, %d %% of its edge dark, steps of one level %d %%) four times "
                     "larger by xBR - its copy %dx%d in %d levels, made in %d frames' parts over %llu ms",
                     e.effect, e.w, e.h, first.facts.clear, first.facts.flat, first.facts.rimDark, first.facts.smooth,
                     copy.width, copy.height, e.levels, first.steps,
                     static_cast<unsigned long long>(now - first.since));
        }
    }
    g_waiting.pop_front();
}

bool swap(DrawCmd& cmd, DrawSetup& setup, const char* look) {
    Context& c = Context::get();
    if (!cmd.texture[0] || !c.renderer || !c.enhanced()) return false;
    if (setup.stage[0].texCoord != 0) return false;
    const auto at = g_byTexture.find(cmd.texture[0]);
    if (at == g_byTexture.end()) return false;
    const auto it = g_pictures.find(at->second);
    if (it == g_pictures.end() || !it->second.texture) return false;
    const Entry& e = it->second;
    if (e.art) {  // 3.0.20-beta.68: however it blends - the copy beside the game's own, the renderer picks
        cmd.sharpArt = e.texture;
        ++g_artDrawn;
        if (!g_toldArtDraw) {  // the proof
            g_toldArtDraw = true;
            LOG_INFO("skills' painted pictures: the first draw with its copy - effect %u's %dx%d picture, its copy "
                     "%dx%d, blend %u/%u (the skills' look: %s); the renderer draws the pack's AI picture of it when "
                     "it has one",
                     e.effect, e.w, e.h, e.copyW, e.copyH, setup.blend ? unsigned(setup.srcBlend) : 0u,
                     setup.blend ? unsigned(setup.dstBlend) : 0u, look ? look : "");
        }
        return true;
    }
    // the specks are light added on (SRCALPHA, ONE); anything else as the game draws it
    if (!setup.blend || setup.dstBlend != d3d7::BLEND_ONE) return false;
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
    std::string out;
    char line[160];
    if (g_sheets) {
        std::snprintf(line, sizeof line, "; the skills' specks: %u sheets, %u draws from their copies", g_sheets,
                      g_drawn);
        out += line;
    }
    if (g_arts || g_artLeft) {  // 3.0.20-beta.68
        std::snprintf(line, sizeof line, "; the skills' painted pictures: %u copies (%.1f MB), %u draws with them",
                      g_arts, double(g_artBytes) / 1048576.0, g_artDrawn);
        out += line;
    }
    g_drawn = 0;
    g_artDrawn = 0;
    return out;
}

}  // namespace uagfx::win::sharpspecks

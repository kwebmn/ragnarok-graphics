#include "win/sprite_hook.hpp"

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <unordered_map>

#include "core/crisp_exe.hpp"
#include "core/sprite_exe.hpp"
#include "core/map_objects.hpp"
#include "win/context.hpp"
#include "win/dx7/surface7.hpp"
#include "win/gpu/renderer.hpp"
#include "win/log.hpp"

namespace uagfx::win {
namespace {

// CTexture::UpdateSprite's own convention: the texture in ECX, six arguments the callee takes off the stack.
using WriteFn = void(__attribute__((thiscall)) *)(uint32_t texture, int x, int y, unsigned width, int height,
                                                   void* image, void* palette);

bool g_writing = false;  // the game's thread only: it draws and writes its sprites itself
Rect g_cell;
uint8_t g_source = 0;
// 0.16: every frame the sprite loader read (SprImg*) -> what its file is (sprite_exe::Source). A frame freed and its
// memory taken by another file's is written again by that file's load.
std::unordered_map<uint32_t, uint8_t> g_frames;
std::unordered_map<uint32_t, bool> g_cards;  // 0.95: the frames of a card lying on the ground
bool g_card = false;
// 2.5.23: every palette frame -> its sprite (CSprRes, whose own palette is at sprite_exe::kPalette); the frame being
// written in a dye, in the sprite's own colours
std::unordered_map<uint32_t, uint32_t> g_owners;
std::vector<uint16_t> g_own;
bool g_ownValid = false;
std::atomic<bool> g_dyes{true};
bool g_dyeLogged = false;
uintptr_t g_load = 0;  // the loader the vtable held

using LoadFn = uint32_t(__attribute__((thiscall)) *)(uint32_t sprite, const char* name);

// The frames of one of the sprite's lists (std::vector<SprImg*>: begin, end), each noted with `source`.
void noteFrames(uint32_t list, uint8_t source) {
    const uint32_t* begin = *reinterpret_cast<uint32_t* const*>(uintptr_t(list));
    const uint32_t* end = *reinterpret_cast<uint32_t* const*>(uintptr_t(list) + 4);
    if (!begin || end < begin || end - begin > 65536) return;
    for (const uint32_t* f = begin; f != end; ++f)
        if (*f) g_frames[*f] = source;
}

// The bytes readable from at on, to the end of its committed region (0: none).
size_t readableFrom(uintptr_t at) {
    MEMORY_BASIC_INFORMATION m{};
    if (!at || !VirtualQuery(reinterpret_cast<const void*>(at), &m, sizeof m) || m.State != MEM_COMMIT ||
        (m.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
        return 0;
    return reinterpret_cast<uintptr_t>(m.BaseAddress) + m.RegionSize - at;
}

// [at, at + n) lies in committed, readable memory: the game's objects are read on its word.
bool readable(uintptr_t at, size_t n) { return n <= readableFrom(at); }

// 2.5.23: the frame (a SprImg) going into a cell of cellW x cellH with `palette` - when that is not its sprite's own,
// the cell as the sprite's own colours would make it, into g_own.
void ownColours(const void* image, const void* palette, int cellW, int cellH) {
    const auto o = g_owners.find(uint32_t(reinterpret_cast<uintptr_t>(image)));
    if (o == g_owners.end()) return;
    const uintptr_t own = uintptr_t(o->second) + sprite_exe::kPalette;
    if (own == reinterpret_cast<uintptr_t>(palette) || !readable(own, 1024) ||
        !readable(reinterpret_cast<uintptr_t>(palette), 1024) ||
        !readable(reinterpret_cast<uintptr_t>(image), sprite_exe::kImageData + 4))
        return;
    const auto* ownPalette = reinterpret_cast<const uint32_t*>(own);
    if (sprite_exe::sameColours(ownPalette, static_cast<const uint32_t*>(palette))) return;
    const auto* img = static_cast<const uint8_t*>(image);
    int16_t w = 0, h = 0;
    std::memcpy(&w, img, 2);
    std::memcpy(&h, img + 2, 2);
    uint32_t data = 0;
    std::memcpy(&data, img + sprite_exe::kImageData, 4);
    const size_t available = readableFrom(data);
    if (!available || w <= 0 || h <= 0) return;
    g_ownValid = sprite_exe::cellTexels(reinterpret_cast<const uint8_t*>(uintptr_t(data)), available, w, h, ownPalette,
                                        cellW, cellH, g_own);
    if (g_ownValid && !g_dyeLogged) {
        g_dyeLogged = true;
        LOG_INFO("AI textures: a frame in a dye (clothes, hair) - its picture in its own colours, coloured as the dye");
    }
}

// 0.23.1, 0.24, the maps' objects drawn anew (core/map_objects.hpp): the loader gives each 32-bit frame a texture of
// its own (SprImg + 8, a CTexture whose surface is one of ours) and the effect draws from it, not from the sprite
// cache - the renderer learns which texture is which frame.
void noteObjectTextures(uint32_t sprite, objects::File file) {
    gpu::Renderer* renderer = Context::get().renderer;
    const uint32_t list = sprite + sprite_exe::kRgbaImages;
    if (!renderer || !readable(list, 8)) return;
    const uint32_t* begin = *reinterpret_cast<uint32_t* const*>(uintptr_t(list));
    const uint32_t* end = *reinterpret_cast<uint32_t* const*>(uintptr_t(list) + 4);
    if (!begin || end <= begin || end - begin > 64 || !readable(uintptr_t(begin), size_t(end - begin) * 4)) return;
    int found = 0;
    for (const uint32_t* f = begin; f != end; ++f) {
        if (!*f || !readable(*f, 12)) continue;
        const uint32_t texture = *reinterpret_cast<const uint32_t*>(uintptr_t(*f) + 8);
        if (!texture || !readable(texture, crisp_exe::kTextureSurface + 4)) continue;
        IDirectDrawSurface7* surface =
            *reinterpret_cast<IDirectDrawSurface7* const*>(uintptr_t(texture) + crisp_exe::kTextureSurface);
        if (!surface || !readable(reinterpret_cast<uintptr_t>(surface), sizeof(dx7::Surface7))) continue;
        const dx7::Surface7* s = dx7::Surface7::from(surface);
        if (!s || !s->gpuTexture()) continue;
        renderer->noteObjectTexture(s->gpuTexture(), uint8_t(objects::kindOf(file, size_t(f - begin))));
        ++found;
    }
    static bool logged[3] = {};
    const size_t at = size_t(file) % 3;
    if (!logged[at])
        LOG_INFO("map objects: the %s's frames, %d textures of their own, known",
                 file == objects::File::Torch ? "torch" : "chimney smoke", found);
    logged[at] = true;
}

uint32_t __attribute__((thiscall)) spriteLoad(uint32_t sprite, const char* name) {
    const uint32_t result = reinterpret_cast<LoadFn>(g_load)(sprite, name);
    const uint8_t source = uint8_t(sprite_exe::sourceOf(name));
    noteFrames(sprite + sprite_exe::kImages, source);
    noteFrames(sprite + sprite_exe::kRgbaImages, source);
    {  // 2.5.23: each palette frame's sprite - its own palette
        const uint32_t list = sprite + sprite_exe::kImages;
        const uint32_t* begin = *reinterpret_cast<uint32_t* const*>(uintptr_t(list));
        const uint32_t* end = *reinterpret_cast<uint32_t* const*>(uintptr_t(list) + 4);
        if (begin && end >= begin && end - begin <= 65536)
            for (const uint32_t* f = begin; f != end; ++f)
                if (*f) g_owners[*f] = sprite;
    }
    if (source == uint8_t(sprite_exe::Source::Item)) {  // 0.95: whether it is a card's, every frame of it
        const bool card = sprite_exe::cardOnGround(name);
        const uint32_t list = sprite + sprite_exe::kImages;
        const uint32_t* begin = *reinterpret_cast<uint32_t* const*>(uintptr_t(list));
        const uint32_t* end = *reinterpret_cast<uint32_t* const*>(uintptr_t(list) + 4);
        if (begin && end >= begin && end - begin <= 65536)
            for (const uint32_t* f = begin; f != end; ++f)
                if (*f) g_cards[*f] = card;
    }
    const objects::File object = sprite_exe::objectFile(name);  // 0.23, 0.24
    if (object != objects::File::None) noteObjectTextures(sprite, object);
    return result;
}

void __attribute__((thiscall)) spriteWrite(uint32_t texture, int x, int y, unsigned width, int height, void* image,
                                           void* palette) {
    g_cell = Rect{x, y, x + int(width), y + height};
    const auto f = g_frames.find(uint32_t(reinterpret_cast<uintptr_t>(image)));
    g_source = f == g_frames.end() ? 0 : f->second;
    const auto c = g_cards.find(uint32_t(reinterpret_cast<uintptr_t>(image)));
    g_card = c != g_cards.end() && c->second;
    g_ownValid = false;
    if (g_dyes.load() && image && palette) ownColours(image, palette, int(width), height);
    g_writing = true;
    reinterpret_cast<WriteFn>(uintptr_t(sprite_exe::kWrite))(texture, x, y, width, height, image, palette);
    g_writing = false;
}

}  // namespace

bool installSpriteHook() {
    static int done = -1;  // once: 1 hooked, 0 not
    if (done >= 0) return done == 1;
    done = 0;
    auto* at = reinterpret_cast<uint8_t*>(uintptr_t(sprite_exe::kWriteCall));
    uint8_t bytes[5] = {};
    SIZE_T got = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), at, bytes, sizeof bytes, &got) || got != sizeof bytes ||
        !sprite_exe::isWriteCall(bytes)) {
        LOG_INFO("AI textures: sprites off - the game's sprite cache is not where this build of uaRO.exe has it");
        return false;
    }
    DWORD old = 0;
    if (!VirtualProtect(at, sizeof bytes, PAGE_EXECUTE_READWRITE, &old)) {
        LOG_INFO("AI textures: sprites off - the game's code cannot be changed (error %lu)", GetLastError());
        return false;
    }
    const int32_t rel = int32_t(reinterpret_cast<uintptr_t>(&spriteWrite) - (uintptr_t(sprite_exe::kWriteCall) + 5));
    std::memcpy(at + 1, &rel, sizeof rel);
    DWORD unused = 0;
    VirtualProtect(at, sizeof bytes, old, &unused);
    FlushInstructionCache(GetCurrentProcess(), at, sizeof bytes);
    done = 1;
    LOG_INFO("AI textures: the game's sprite frames are seen one by one (uaRO.exe 0x%X)", sprite_exe::kWriteCall);
    // 0.16: the loader's vtable entry, so that every frame is known by its file's folder.
    auto* slot = reinterpret_cast<uintptr_t*>(uintptr_t(sprite_exe::kLoadSlot));
    uintptr_t now = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), slot, &now, sizeof now, &got) || got != sizeof now ||
        now != sprite_exe::kLoad || !VirtualProtect(slot, sizeof now, PAGE_EXECUTE_READWRITE, &old)) {
        LOG_INFO("AI textures: the sprites' files are not known - every sprite counts as a monster");
        return true;
    }
    g_load = now;
    *slot = reinterpret_cast<uintptr_t>(&spriteLoad);
    VirtualProtect(slot, sizeof now, old, &unused);
    LOG_INFO("AI textures: sprites known by their files (monsters, players, NPCs, items)");
    return true;
}

bool spriteCell(Rect& cell, uint8_t& source) {
    if (!g_writing) return false;
    cell = g_cell;
    source = g_source;
    return true;
}

bool spriteCellCard() { return g_writing && g_card; }

const std::vector<uint16_t>* spriteCellOwn() { return g_writing && g_ownValid ? &g_own : nullptr; }

void setSpriteDyes(bool on) { g_dyes = on; }

}  // namespace uagfx::win

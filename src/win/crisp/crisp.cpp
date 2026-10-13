#include "win/crisp/crisp.hpp"

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <iterator>
#include <new>
#include <vector>

#include "win/context.hpp"
#include "win/crisp/state.hpp"
#include "win/log.hpp"

namespace uagfx::win::crisp {

namespace ce = crisp_exe;

State& state() {
    static State s;
    return s;
}

bool active() { return state().installed && state().K > 1.0; }

namespace {

constexpr size_t kBudget = size_t(256) << 20;  // every twin and copy together: a 32-bit game has 2 GB

uintptr_t self(const void* p) { return reinterpret_cast<uintptr_t>(p); }

template <typename Fn>
Fn original(int slot) {
    return reinterpret_cast<Fn>(state().original[slot]);
}

Rect whole(const Picture& p) { return Rect{0, 0, p.twin->width(), p.twin->height()}; }

// [at, at + n) lies in committed, readable memory: the game's window lists are walked on its word.
bool readable(uintptr_t at, size_t n) {
    MEMORY_BASIC_INFORMATION m{};
    if (!at || !VirtualQuery(reinterpret_cast<const void*>(at), &m, sizeof m) || m.State != MEM_COMMIT ||
        (m.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
        return false;
    return at + n <= reinterpret_cast<uintptr_t>(m.BaseAddress) + m.RegionSize;
}

// Writes a pointer into the exe's read-only data: a vtable entry or an import.
bool write(uintptr_t at, uintptr_t value) {
    DWORD old = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(at), sizeof value, PAGE_EXECUTE_READWRITE, &old)) return false;
    *reinterpret_cast<uintptr_t*>(at) = value;
    DWORD unused = 0;
    VirtualProtect(reinterpret_cast<void*>(at), sizeof value, old, &unused);
    return true;
}

void forgetDc(uintptr_t bitmap) {
    State& s = state();
    const auto it = s.dcs.find(field<HDC>(bitmap, ce::kDc));
    if (it != s.dcs.end() && it->second == bitmap) s.dcs.erase(it);
}

void dropAll() {
    State& s = state();
    while (!s.pictures.empty()) drop(s.pictures.begin()->first);
    s.tiles.clear();
}

// The game's windows from a list's head node down through their children: each gets the flag Invalidate sets, so
// the game redraws it at its next frame (spec §5). Returns how many.
int markWindows(uintptr_t head, int depth) {
    if (depth > 32 || !readable(head, 12)) return 0;
    int n = 0;
    int guard = 0;
    for (uintptr_t node = field<uintptr_t>(head, ce::kNodeNext); node != head && ++guard < 100000;
         node = field<uintptr_t>(node, ce::kNodeNext)) {
        if (!readable(node, 12)) break;
        const uintptr_t window = field<uintptr_t>(node, ce::kNodeValue);
        if (!readable(window, ce::kWindowDirty + 4)) continue;
        field<uint32_t>(window, ce::kWindowDirty) = 1;
        n += 1 + markWindows(field<uintptr_t>(window, ce::kWindowChildren), depth + 1);
    }
    return n;
}

// --- The window picture's methods (docs/research/text.md §1.1): the game's first, then the twin (spec §4). ---

using DestroyFn = void*(UAGFX_THISCALL*)(void*, unsigned);
using GetDcFn = void(UAGFX_THISCALL*)(void*, HDC*);
using CopyPixelsFn = void(UAGFX_THISCALL*)(void*, int, int, int, int, const uint32_t*, int);
using CopyFromFn = void(UAGFX_THISCALL*)(void*, int, int, int, int, void*);
using SpriteFn = void(UAGFX_THISCALL*)(void*, int, int, int, int, int);
using TextureCopyFn = void(UAGFX_THISCALL*)(void*, int, int, int, int, int, int, int, int, int, int);
using ClearFn = void(UAGFX_THISCALL*)(void*, const RECT*, uint32_t);
using CreateDibFn = HBITMAP(WINAPI*)(HDC, const BITMAPINFO*, UINT, void**, HANDLE, DWORD);

void* UAGFX_THISCALL onDestroy(void* p, unsigned flags) {
    Guard guard;
    drop(self(p));
    forgetDc(self(p));
    return original<DestroyFn>(ce::kDestroy)(p, flags);
}

void UAGFX_THISCALL onGetDc(void* p, HDC* out) {
    Guard guard;
    original<GetDcFn>(ce::kGetDc)(p, out);
    if (active() && out && *out) state().dcs[*out] = self(p);  // TextOutW and FillRgn find the picture by its DC
}

void UAGFX_THISCALL onCopyPixels(void* p, int x, int y, int w, int h, const uint32_t* src, int key) {
    Guard guard;
    original<CopyPixelsFn>(ce::kCopyPixels)(p, x, y, w, h, src, key);
    if (Picture* pic = picture(self(p))) pic->twin->sync(pic->bits, Rect{x, y, x + w, y + h});
}

void UAGFX_THISCALL onCopyFrom(void* p, int x, int y, int w, int h, void* child) {
    Guard guard;
    Picture* pic = picture(self(p));
    Picture* kid = pic && child ? picture(self(child)) : nullptr;
    Rect r;
    const bool copies = pic && copyFromRegion(x, y, w, h, pic->twin->width(), pic->twin->height(), r);
    if (copies) {  // what both twins missed so far, before the copy mixes them
        pic->twin->sync(pic->bits, r);
        if (kid) kid->twin->sync(kid->bits, whole(*kid));
    }
    original<CopyFromFn>(ce::kCopyFrom)(p, x, y, w, h, child);
    if (!copies) return;
    if (kid)
        pic->twin->copyFrom(pic->bits, *kid->twin, r.left, r.top, r.width(), r.height());
    else
        pic->twin->sync(pic->bits, r);
}

void UAGFX_THISCALL onSprite(void* p, int a, int b, int c, int d, int e) {
    Guard guard;
    original<SpriteFn>(ce::kSprite)(p, a, b, c, d, e);
    if (Picture* pic = picture(self(p))) pic->twin->sync(pic->bits, whole(*pic));
}

void UAGFX_THISCALL onTextureCopy(void* p, int a, int b, int c, int d, int e, int f, int g, int h, int i, int j) {
    Guard guard;
    original<TextureCopyFn>(ce::kTextureCopy)(p, a, b, c, d, e, f, g, h, i, j);
    if (Picture* pic = picture(self(p))) pic->twin->sync(pic->bits, whole(*pic));
}

void UAGFX_THISCALL onClear(void* p, const RECT* r, uint32_t color) {
    Guard guard;
    original<ClearFn>(ce::kClear)(p, r, color);
    if (Picture* pic = picture(self(p))) {
        const Rect all = whole(*pic);
        pic->twin->overwrite(pic->bits, r ? Rect{int(r->left), int(r->top), int(r->right), int(r->bottom)} : all);
    }
}

HBITMAP WINAPI onCreateDibSection(HDC dc, const BITMAPINFO* info, UINT usage, void** bits, HANDLE section,
                                  DWORD offset) {
    Guard guard;
    const auto real = reinterpret_cast<CreateDibFn>(state().createDibSection);
    const HBITMAP made = real(dc, info, usage, bits, section, offset);
    // The game's only call (FUN_00403960) passes &bitmap->bits: a new DIB for that window picture, so whatever twin
    // the address had is gone (spec §5).
    if (bits && reinterpret_cast<uintptr_t>(__builtin_return_address(0)) == ce::kCreateDibReturn)
        drop(self(bits) - ce::kBits);
    return made;
}

// Where a hook goes and where the game's own function is kept.
struct Hook {
    uintptr_t at;
    uintptr_t hook;
    uintptr_t* original;
};

std::vector<Hook> hooks() {
    State& s = state();
    const auto slot = [&s](int i, uintptr_t fn) {
        return Hook{ce::kBitmapVtable + 4u * uint32_t(i), fn, &s.original[i]};
    };
    return {
        slot(ce::kDestroy, reinterpret_cast<uintptr_t>(&onDestroy)),
        slot(ce::kGetDc, reinterpret_cast<uintptr_t>(&onGetDc)),
        slot(ce::kCopyPixels, reinterpret_cast<uintptr_t>(&onCopyPixels)),
        slot(ce::kCopyFrom, reinterpret_cast<uintptr_t>(&onCopyFrom)),
        slot(ce::kSprite, reinterpret_cast<uintptr_t>(&onSprite)),
        slot(ce::kTextureCopy, reinterpret_cast<uintptr_t>(&onTextureCopy)),
        slot(ce::kClear, reinterpret_cast<uintptr_t>(&onClear)),
        Hook{ce::kImportCreateDibSection, reinterpret_cast<uintptr_t>(&onCreateDibSection), &s.createDibSection},
    };
}

// "" when this is the uaRO.exe the hooks know (spec §5).
std::string check() {
    if (reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)) != ce::kImageBase) return "the game is not at 0x400000";
    if (!readable(ce::kBitmapVtable, 4 * ce::kBitmapSlots)) return "no window picture methods at 0x6956ec";
    uint32_t entries[ce::kBitmapSlots];
    std::memcpy(entries, reinterpret_cast<const void*>(uintptr_t(ce::kBitmapVtable)), sizeof entries);
    const std::string methods = ce::checkBitmapMethods(entries);
    if (!methods.empty()) return methods;
    const HMODULE gdi = GetModuleHandleW(L"gdi32.dll");
    const struct {
        uint32_t at;
        const char* name;
    } imports[] = {{ce::kImportTextOutW, "TextOutW"},
                   {ce::kImportFillRgn, "FillRgn"},
                   {ce::kImportCreateDibSection, "CreateDIBSection"}};
    for (const auto& i : imports)
        if (!gdi || !readable(i.at, 4) ||
            field<uintptr_t>(i.at, 0) != reinterpret_cast<uintptr_t>(GetProcAddress(gdi, i.name)))
            return std::string("the game's ") + i.name + " does not lead to gdi32";
    return "";
}

}  // namespace

Picture* picture(uintptr_t bitmap) {
    State& s = state();
    if (!active() || !bitmap) return nullptr;
    GdiFlush();  // GDI may still hold drawing for these pixels
    const uint32_t* bits = field<const uint32_t*>(bitmap, ce::kBits);
    const int w = field<int>(bitmap, ce::kWidth);
    const int h = field<int>(bitmap, ce::kHeight);
    const auto it = s.pictures.find(bitmap);
    if (it != s.pictures.end()) {
        const Picture& p = *it->second;
        if (p.bits == bits && p.twin->width() == w && p.twin->height() == h) return it->second.get();
        drop(bitmap);  // another picture at that address: its twin starts again
    }
    if (!bits || w <= 0 || h <= 0 || w > 8192 || h > 8192) return nullptr;
    const size_t bytes = TwinPicture::bytesFor(w, h, s.K);
    if (s.bytes + bytes > kBudget) {
        if (!s.budgetLogged)
            LOG_INFO("crisp text: twins reached %u MB, more windows stay as in 0.4", unsigned(kBudget >> 20));
        s.budgetLogged = true;
        return nullptr;
    }
    std::unique_ptr<Picture> p;
    try {  // an exception must not reach the game's frames
        p = std::make_unique<Picture>();
        p->twin = std::make_unique<TwinPicture>(bits, w, h, s.K);
    } catch (const std::bad_alloc&) {
        return nullptr;  // no memory for this twin: the window stays as in 0.4
    }
    p->self = bitmap;
    p->dc = field<HDC>(bitmap, ce::kDc);
    p->bits = bits;
    Picture* made = p.get();
    if (p->dc) s.dcs[p->dc] = bitmap;
    s.bytes += bytes;
    s.pictures[bitmap] = std::move(p);
    return made;
}

void drop(uintptr_t bitmap) {
    State& s = state();
    const auto it = s.pictures.find(bitmap);
    if (it == s.pictures.end()) return;
    gpu::Renderer* r = Context::get().renderer;
    for (const Picture::Tile& t : it->second->tiles)
        if (t.texture && r) r->releaseTexture(t.texture);
    for (auto t = s.tiles.begin(); t != s.tiles.end();)
        t = t->second.picture == bitmap ? s.tiles.erase(t) : std::next(t);
    s.bytes -= it->second->twin->bytes();
    s.pictures.erase(it);
}

void install(bool enabled) {
    State& s = state();
    if (s.installed) return;
    if (!enabled) {
        LOG_INFO("crisp text: off (crisp_text = 0)");
        return;
    }
    const std::string why = check();
    if (!why.empty()) {
        LOG_INFO("crisp text: off (%s)", why.c_str());
        return;
    }
    const std::vector<Hook> list = hooks();
    size_t done = 0;
    for (; done < list.size(); ++done) {
        *list[done].original = field<uintptr_t>(list[done].at, 0);
        if (!write(list[done].at, list[done].hook)) break;
    }
    if (done < list.size()) {
        const unsigned long error = GetLastError();
        while (done > 0) {
            --done;
            write(list[done].at, *list[done].original);
        }
        LOG_INFO("crisp text: off (the game's methods cannot be changed: error %lu)", error);
        return;
    }
    s.installed = true;
    s.K = 0.0;
    LOG_INFO("crisp text: on");
}

void uninstall() {
    State& s = state();
    if (!s.installed) return;
    for (const Hook& h : hooks()) write(h.at, *h.original);
    dropAll();
    s.dcs.clear();
    s.loggedSizes.clear();
    s.budgetLogged = false;
    s.K = 0.0;
    s.installed = false;
}

void setScale(double K) {
    State& s = state();
    if (!s.installed || K == s.K) return;
    dropAll();
    s.K = K;
    if (K <= 1.0) {
        LOG_INFO("crisp text: k %.3f, waiting for the game to be enlarged", K);
        return;
    }
    const uintptr_t head = readable(ce::kWindowList, 4) ? field<uintptr_t>(ce::kWindowList, 0) : 0;
    LOG_INFO("crisp text: k %.3f, %d windows redrawn", K, markWindows(head, 0));
}

std::string stats() {
    const State& s = state();
    if (!active()) return "";
    char line[128];
    std::snprintf(line, sizeof line, "; crisp text: %u windows, %.1f MB, %u letters",
                  static_cast<unsigned>(s.pictures.size()), double(s.bytes) / double(1 << 20),
                  static_cast<unsigned>(s.glyphs.size()));
    return line;
}

}  // namespace uagfx::win::crisp

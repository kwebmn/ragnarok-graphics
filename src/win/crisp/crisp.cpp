#include "win/crisp/crisp.hpp"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <new>
#include <vector>

#include "core/ai/texture_cache.hpp"
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
    dropFonts();  // the big fonts and their letters were for the old K
    s.glyphs.clear();
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

// --- The window picture's methods (docs/research/text.md §1.1): the game's first, then the twin (spec §4). Every
// hook returns what the game's method left in eax: the painter tests GetDC's true before it makes the text
// background transparent (0x406478), so a lost result paints a box behind every string; the rest pass theirs on too.

using DestroyFn = void*(UAGFX_THISCALL*)(void*, unsigned);
using GetDcFn = uint32_t(UAGFX_THISCALL*)(void*, HDC*);
using CopyPixelsFn = uint32_t(UAGFX_THISCALL*)(void*, int, int, int, int, const uint32_t*, int);
using CopyFromFn = uint32_t(UAGFX_THISCALL*)(void*, int, int, int, int, void*);
using SpriteFn = uint32_t(UAGFX_THISCALL*)(void*, int, int, int, int, int);
using TextureCopyFn = uint32_t(UAGFX_THISCALL*)(void*, int, int, int, int, int, int, int, int, int, int);
using ClearFn = uint32_t(UAGFX_THISCALL*)(void*, const RECT*, uint32_t);
using CreateDibFn = HBITMAP(WINAPI*)(HDC, const BITMAPINFO*, UINT, void**, HANDLE, DWORD);

void* UAGFX_THISCALL onDestroy(void* p, unsigned flags) {
    Guard guard;
    guarded(
        [p] {
            forgetDc(self(p));  // first: it cannot fail, drop() can (0.5.2 review 2b)
            drop(self(p));
        },
        [] {});
    return original<DestroyFn>(ce::kDestroy)(p, flags);
}

uint32_t UAGFX_THISCALL onGetDc(void* p, HDC* out) {
    Guard guard;
    const uint32_t result = original<GetDcFn>(ce::kGetDc)(p, out);
    guarded(  // TextOutW and FillRgn find the picture by its DC
        [p, out] {
            if (active() && out && *out) state().dcs[*out] = self(p);
        },
        [p] { lost(self(p)); });
    return result;
}

// 0.14, AI icons: a picture copied whole into the window - an icon (a square of 16 to 64: items, skills) or (0.14.1)
// a picture (32 to 512 a side, at most 3 times as long as wide: an item's picture in its description, a card's, an
// NPC's illustration; not a strip of the window's frame). The AI's larger one goes into the twin when it has come
// back - the twin keeps what is under it, the copy takes the game's pixels; until then it is asked for once and the
// game's pixels go in as blocks. true: the twin has it.
bool aiPictureSize(int w, int h) {
    if (w == h && w >= 16 && w <= 64) return true;
    return w >= 32 && h >= 32 && w <= 512 && h <= 512 && std::max(w, h) <= 3 * std::min(w, h);
}

bool aiIcon(Picture& pic, int x, int y, int w, int h, int rows, const uint32_t* src, bool key) {
    State& s = state();
    if (!s.aiIcons || !aiPictureSize(w, h) || rows != h || x < 0 || y < 0 || x + w > pic.twin->width() ||
        y + h > pic.twin->height())
        return false;
    std::vector<uint32_t> argb(size_t(w) * size_t(h));
    for (size_t i = 0; i < argb.size(); ++i)
        argb[i] = key && src[i] == 0xFFFF00FFu ? 0u : (src[i] | 0xFF000000u);  // the key: transparent
    const uint64_t k = ai::textureKey(argb.data(), w, h).hash;
    const auto it = s.icons.find(k);
    if (it != s.icons.end()) {
        const Rect r{x, y, x + w, y + h};
        pic.twin->absorb(pic.bits, r);
        pic.twin->paint(r, it->second.argb.data(), it->second.width, it->second.height);
        return true;
    }
    if (s.asked.insert(k).second)
        if (gpu::Renderer* renderer = Context::get().renderer) renderer->requestIcon(k, w, h, argb.data());
    return false;
}

// The game writes every pixel of the rectangle but what its key skips, so the twin takes them all (review 0.5.0 I1):
// a skin repainted under a label erases the edges of its crisp letters. Which rows it writes depends on its pixels
// before the call.
uint32_t UAGFX_THISCALL onCopyPixels(void* p, int x, int y, int w, int h, const uint32_t* src, int key) {
    Guard guard;
    // The mode fade's logo: no twin for a picture drawn once and deleted (0.5.0 review M6).
    if (reinterpret_cast<uintptr_t>(__builtin_return_address(0)) == ce::kLogoCopyReturn) {
        guarded([p] { drop(self(p)); }, [] {});
        return original<CopyPixelsFn>(ce::kCopyPixels)(p, x, y, w, h, src, key);
    }
    Picture* pic = nullptr;
    int rows = 0;
    guarded(
        [&] {
            pic = src ? picture(self(p)) : nullptr;
            if (pic) rows = copyPixelsRows(pic->bits, pic->twin->width(), pic->twin->height(), x, y, h);
        },
        [&] {
            pic = nullptr;
            lost(self(p));
        });
    const uint32_t result = original<CopyPixelsFn>(ce::kCopyPixels)(p, x, y, w, h, src, key);
    if (pic)  // the key is a byte
        guarded(
            [&] {
                if (!aiIcon(*pic, x, y, w, h, rows, src, (key & 0xFF) != 0))
                    pic->twin->copyPixels(pic->bits, x, y, w, rows, src, (key & 0xFF) != 0);
            },
            [p] { lost(self(p)); });
    return result;
}

uint32_t UAGFX_THISCALL onCopyFrom(void* p, int x, int y, int w, int h, void* child) {
    Guard guard;
    Picture* pic = nullptr;
    Picture* kid = nullptr;
    Rect r;
    bool copies = false;
    guarded(
        [&] {
            pic = picture(self(p));
            kid = pic && child ? picture(self(child)) : nullptr;
            copies = pic && copyFromRegion(x, y, w, h, pic->twin->width(), pic->twin->height(), r);
            if (copies) {  // what both twins missed so far, before the copy mixes them
                pic->twin->sync(pic->bits, r);
                if (kid) kid->twin->sync(kid->bits, whole(*kid));
            }
        },
        [&] {
            copies = false;
            lost(self(p));
        });
    const uint32_t result = original<CopyFromFn>(ce::kCopyFrom)(p, x, y, w, h, child);
    if (copies)
        guarded(
            [&] {
                if (kid)
                    pic->twin->copyFrom(pic->bits, *kid->twin, r.left, r.top, r.width(), r.height());
                else
                    pic->twin->sync(pic->bits, r);
            },
            [p] { lost(self(p)); });
    return result;
}

uint32_t UAGFX_THISCALL onSprite(void* p, int a, int b, int c, int d, int e) {
    Guard guard;
    const uint32_t result = original<SpriteFn>(ce::kSprite)(p, a, b, c, d, e);
    guarded(
        [p] {
            if (Picture* pic = picture(self(p))) pic->twin->sync(pic->bits, whole(*pic));
        },
        [p] { lost(self(p)); });
    return result;
}

uint32_t UAGFX_THISCALL onTextureCopy(void* p, int a, int b, int c, int d, int e, int f, int g, int h, int i, int j) {
    Guard guard;
    const uint32_t result = original<TextureCopyFn>(ce::kTextureCopy)(p, a, b, c, d, e, f, g, h, i, j);
    guarded(
        [p] {
            if (Picture* pic = picture(self(p))) pic->twin->sync(pic->bits, whole(*pic));
        },
        [p] { lost(self(p)); });
    return result;
}

uint32_t UAGFX_THISCALL onClear(void* p, const RECT* r, uint32_t color) {
    Guard guard;
    const uint32_t result = original<ClearFn>(ce::kClear)(p, r, color);
    guarded(
        [p, r] {
            // Only into a twin there is already: the twin a picture's next write makes from its DIB has the fill, so a
            // fill makes none - and the mode fade's logo, cleared by the factory (0x404DAC), gets none (0.5.2
            // review 1).
            if (Picture* pic = existing(self(p))) {
                const Rect area = r ? Rect{int(r->left), int(r->top), int(r->right), int(r->bottom)} : whole(*pic);
                pic->twin->overwrite(pic->bits, area);
            }
        },
        [p] { lost(self(p)); });
    return result;
}

using DrawFn = uint32_t(UAGFX_THISCALL*)(void*, int, int, int, int, uint32_t);

// The twin textures of a picture's tiles (spec §6): made, or made again when their size changes, and loaded with
// what changed in the twin since the last time - for the chat window, which the game redraws every frame, its edit
// line. Every tile's surface is noted, so its quad - drawn later, from the game's interface list - finds the twin.
void uploadTiles(Picture& pic) {
    State& s = state();
    Context& c = Context::get();
    const int count = field<int>(pic.self, ce::kTileCount);
    const uintptr_t head = field<uintptr_t>(pic.self, ce::kTiles);
    if (!c.renderer || count <= 0 || count > 4096 || !head) return;
    for (size_t i = size_t(count); i < pic.tiles.size(); ++i)
        if (pic.tiles[i].texture) c.renderer->releaseTexture(pic.tiles[i].texture);
    pic.tiles.resize(size_t(count));
    const Rect changed = pic.twin->takeChanged();
    std::vector<uint32_t> texels;
    int i = 0;
    for (uintptr_t node = field<uintptr_t>(head, ce::kNodeNext); node != head && i < count;
         node = field<uintptr_t>(node, ce::kNodeNext), ++i) {
        const uintptr_t texture = field<uintptr_t>(node, ce::kNodeValue);
        if (!texture) continue;
        const void* surface = reinterpret_cast<const void*>(field<uintptr_t>(texture, ce::kTextureSurface));
        if (!surface) continue;
        s.tiles[surface] = TileRef{pic.self, i};
        const TilePlace place = tilePlace(i, pic.twin->width(), pic.twin->height());
        const TwinSpan sx = tileSpan(place.x, place.width, pic.twin->twinWidth(), s.K);
        const TwinSpan sy = tileSpan(place.y, place.height, pic.twin->twinHeight(), s.K);
        const Rect span{sx.first, sy.first, sx.end, sy.end};  // the tile's texture, in twin pixels
        Picture::Tile& t = pic.tiles[size_t(i)];
        const bool fresh = t.width != span.width() || t.height != span.height();
        if (fresh) {  // a texture that cannot be made is not tried again at this size
            if (t.texture) c.renderer->releaseTexture(t.texture);
            t.width = span.width();
            t.height = span.height();
            t.texture = t.width > 0 && t.height > 0 ? c.renderer->createTexture(t.width, t.height, 1) : 0;
        }
        Rect part = span;
        if (!t.texture || (!fresh && !clipRect(changed, part))) continue;  // none, or nothing of it changed
        pic.twin->texels(part, texels);
        const Rect at{part.left - span.left, part.top - span.top, part.right - span.left, part.bottom - span.top};
        c.record.upload(t.texture, 0, at, texels.data());
        c.flushIfFull();
    }
}

// The game shows a window picture (spec §4, §6): first the safety net - what reached its pixels outside the known
// calls goes into the twin enlarged - then the game's own upload and quads, then the twin textures.
uint32_t UAGFX_THISCALL onDraw(void* p, int x, int y, int w, int h, uint32_t argb) {
    Guard guard;
    if (reinterpret_cast<uintptr_t>(__builtin_return_address(0)) == ce::kLogoDrawReturn)
        return original<DrawFn>(ce::kDraw)(p, x, y, w, h, argb);  // the fade's logo: no twin (0.5.0 review M6)
    Picture* pic = nullptr;
    guarded(
        [&] {
            State& s = state();
            pic = picture(self(p));
            const bool dirty = field<uint8_t>(self(p), ce::kDirty) != 0;
            if (pic && dirty && pic->twin->sync(pic->bits, whole(*pic)) > 0 &&
                s.loggedSizes.insert(uint32_t(pic->twin->width()) << 16 | uint32_t(pic->twin->height())).second)
                LOG_INFO("crisp text: a %dx%d window changed outside the known calls", pic->twin->width(),
                         pic->twin->height());
        },
        [&] {
            pic = nullptr;
            lost(self(p));
        });
    const uint32_t result = original<DrawFn>(ce::kDraw)(p, x, y, w, h, argb);
    if (pic) guarded([&] { uploadTiles(*pic); }, [p] { lost(self(p)); });
    return result;
}

HBITMAP WINAPI onCreateDibSection(HDC dc, const BITMAPINFO* info, UINT usage, void** bits, HANDLE section,
                                  DWORD offset) {
    Guard guard;
    const auto real = reinterpret_cast<CreateDibFn>(state().createDibSection);
    const HBITMAP made = real(dc, info, usage, bits, section, offset);
    // The game's only call (FUN_00403960) passes &bitmap->bits: a new DIB for that window picture, so whatever twin
    // the address had is gone (spec §5).
    if (bits && reinterpret_cast<uintptr_t>(__builtin_return_address(0)) == ce::kCreateDibReturn)
        guarded([bits] { drop(self(bits) - ce::kBits); }, [] {});
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
        slot(ce::kDraw, reinterpret_cast<uintptr_t>(&onDraw)),
        Hook{ce::kImportCreateDibSection, reinterpret_cast<uintptr_t>(&onCreateDibSection), &s.createDibSection},
        Hook{ce::kImportTextOutW, reinterpret_cast<uintptr_t>(&hookTextOutW), &s.textOutW},
        Hook{ce::kImportFillRgn, reinterpret_cast<uintptr_t>(&hookFillRgn), &s.fillRgn},
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
    s.pictures[bitmap] = std::move(p);  // first: should an insert below throw, drop() finds the picture whole
    s.bytes += bytes;
    if (made->dc) s.dcs[made->dc] = bitmap;
    return made;
}

Picture* existing(uintptr_t bitmap) { return state().pictures.count(bitmap) ? picture(bitmap) : nullptr; }

void lost(uintptr_t bitmap) noexcept {
    guarded([bitmap] { drop(bitmap); }, [] {});
    static bool logged = false;
    if (!logged)
        LOG_INFO("crisp text: a window's twin was dropped after an error; it is made again at its next change");
    logged = true;
}

void drop(uintptr_t bitmap) {
    State& s = state();
    const auto it = s.pictures.find(bitmap);
    if (it == s.pictures.end()) return;
    // Unlinked first: should releasing a texture throw, nothing of the picture stays behind (0.5.2 review 2b).
    const std::unique_ptr<Picture> pic = std::move(it->second);
    s.pictures.erase(it);
    s.bytes -= pic->twin->bytes();
    for (auto t = s.tiles.begin(); t != s.tiles.end();)
        t = t->second.picture == bitmap ? s.tiles.erase(t) : std::next(t);
    if (gpu::Renderer* r = Context::get().renderer)
        for (const Picture::Tile& t : pic->tiles)
            if (t.texture) r->releaseTexture(t.texture);
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
    s.scale = Settled(kSettleMs);
    s.installed = false;
    s.icons.clear();  // their renderer is gone; a new one is asked again
    s.iconOrder.clear();
    s.iconBytes = 0;
    s.asked.clear();
    s.iconsArrived = false;
}

void setScale(double K) {
    State& s = state();
    // AI icons came back: every window draws again, the new ones in its twin - at most five times a second while a
    // full inventory's icons arrive one after another.
    const ULONGLONG now = GetTickCount64();
    if (s.iconsArrived && s.installed && s.K > 1.0 && now - s.iconsMarked >= 200) {
        s.iconsArrived = false;
        s.iconsMarked = now;
        markWindows(readable(ce::kWindowList, 4) ? field<uintptr_t>(ce::kWindowList, 0) : 0, 0);
    }
    // While the window's edge is dragged K changes on every frame: it is taken once it holds for kSettleMs; until
    // then the twins are not shown - their K is not the renderer's k (0.5.0 review M2).
    if (!s.installed || !s.scale.take(K, GetTickCount64()) || K == s.K) return;
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

void tileDraw(const void* surface, DrawCmd& cmd) {
    const State& s = state();
    if (!active() || !surface) return;
    const auto t = s.tiles.find(surface);
    if (t == s.tiles.end()) return;
    const auto p = s.pictures.find(t->second.picture);
    if (p == s.pictures.end() || size_t(t->second.index) >= p->second->tiles.size()) return;
    const Picture& pic = *p->second;
    const Picture::Tile& tile = pic.tiles[size_t(t->second.index)];
    if (!tile.texture) return;
    const TilePlace place = tilePlace(t->second.index, pic.twin->width(), pic.twin->height());
    cmd.twin = tile.texture;
    cmd.twinTile[0] = place.x;
    cmd.twinTile[1] = place.y;
    cmd.twinScale = s.K;
}

void forgetSurface(const void* surface) { state().tiles.erase(surface); }

void setAiIcons(bool on) {
    state().aiIcons = on;
    if (on) LOG_INFO("AI textures: icons of the windows go to the network (crisp text draws them)");
}

void iconReady(void*, uint64_t key, int width, int height, const uint32_t* argb) {
    State& s = state();
    s.asked.erase(key);
    if (width <= 0 || height <= 0 || s.icons.count(key)) return;
    // The game's address space is 32-bit: 96 MB of them at most - an NPC's illustration made 4 times larger is some
    // ten; the oldest go first and are asked for again when drawn (upscaled.grf has them by then).
    const size_t bytes = size_t(width) * size_t(height) * 4, budget = size_t(96) << 20;
    while (!s.iconOrder.empty() && s.iconBytes + bytes > budget) {
        const auto old = s.icons.find(s.iconOrder.front());
        if (old != s.icons.end()) {
            s.iconBytes -= old->second.argb.size() * 4;
            s.icons.erase(old);
        }
        s.iconOrder.pop_front();
    }
    State::Icon& icon = s.icons[key];
    icon.width = width;
    icon.height = height;
    icon.argb.assign(argb, argb + size_t(width) * size_t(height));
    s.iconOrder.push_back(key);
    s.iconBytes += bytes;
    s.iconsArrived = true;
}

}  // namespace uagfx::win::crisp

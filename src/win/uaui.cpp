#include "win/uaui.hpp"

#include <RmlUi/Core.h>

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <cctype>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/char_block.hpp"
#include "core/char_status.hpp"
#include "core/d3d7.hpp"
#include "core/effect_exe.hpp"
#include "core/glint.hpp"
#include "core/states.hpp"
#include "core/ui_style.hpp"
#include "core/uaui_mesh.hpp"
#include "core/ui_icons.hpp"
#include "core/ui_scene.hpp"
#include "core/ui_theme.hpp"
#include "core/ui_motion.hpp"
#include <map>
#include "core/layout.hpp"
#include "core/window_panel.hpp"
#include "win/paths.hpp"
#include "pad/win/game_window.hpp"
#include "win/game_camera.hpp"
#include "win/gpu/renderer.hpp"
#include "win/code_page.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/ui_font.hpp"

namespace uagfx::win::uaui {
namespace {

constexpr int kAtlas = 2048;  // the glyphs' atlas, one texture (3.0.18-beta.48: 1024 filled up at 4K, R-QTH95E)
constexpr uintptr_t kAtlasHandle = 1;      // its RmlUi texture handle
constexpr int kGlyphPad = 1;               // clear pixels round a glyph in the atlas

double clockNow() {
    static LARGE_INTEGER f{};
    if (!f.QuadPart) QueryPerformanceFrequency(&f);
    LARGE_INTEGER c{};
    QueryPerformanceCounter(&c);
    return double(c.QuadPart) / double(f.QuadPart);
}

std::wstring wide(const std::u32string& s) {
    std::wstring w;
    for (char32_t c : s) {
        if (c >= 0x10000) {
            const char32_t v = c - 0x10000;
            w.push_back(wchar_t(0xD800 + (v >> 10)));
            w.push_back(wchar_t(0xDC00 + (v & 0x3FF)));
        } else {
            w.push_back(wchar_t(c));
        }
    }
    return w;
}

std::string utf8From(const std::string& text, unsigned codePage) {
    if (text.empty()) return {};
    const int n = MultiByteToWideChar(codePage, 0, text.data(), int(text.size()), nullptr, 0);
    std::wstring w(size_t(std::max(n, 0)), L'\0');
    if (n > 0) MultiByteToWideChar(codePage, 0, text.data(), int(text.size()), w.data(), n);
    const int m = WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), nullptr, 0, nullptr, nullptr);
    std::string out(size_t(std::max(m, 0)), '\0');
    if (m > 0) WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), out.data(), m, nullptr, nullptr);
    return out;
}

// ---- the glyphs: GDI into one atlas (white, its coverage as alpha, premultiplied) ----

struct Glyph {
    int x = 0, y = 0, w = 0, h = 0;  // in the atlas
    int advance = 0;
};

struct Face {
    HFONT plex = nullptr, segoe = nullptr;
    Rml::FontMetrics metrics{};
    int ascent = 0, height = 0;
    std::unordered_map<char32_t, Glyph> glyphs;
    std::unordered_map<char32_t, Glyph> halos;  // 3.0.18-beta.20: each glyph's outline, grown by 2 pixels
};

struct Atlas {
    std::vector<uint32_t> px = std::vector<uint32_t>(size_t(kAtlas) * kAtlas, 0u);
    int shelfX = 0, shelfY = 0, shelfH = 0;
    bool dirty = false, full = false;
    int glyphs = 0, uploads = 0;
    int dirtyY0 = 0, dirtyY1 = kAtlas;  // 3.0.18-beta.48: the rows changed since the last upload (sent alone)
    void touch(int y0, int y1) {
        if (!dirty) dirtyY0 = y0, dirtyY1 = y1;
        else dirtyY0 = std::min(dirtyY0, y0), dirtyY1 = std::max(dirtyY1, y1);
        dirty = true;
    }
};

struct Stats {
    int frames = 0, draws = 0, vertices = 0;
    double update = 0, render = 0, emit = 0, worst = 0;
    double since = 0;
};

struct Texture {
    int w = 0, h = 0;
    std::vector<uint32_t> px;
    uint32_t id = 0;  // the renderer's
    bool upload = true;
};

struct Batch {
    uintptr_t geometry = 0;
    float dx = 0, dy = 0;
    uintptr_t texture = 0;
    bool clipped = false;
    ::uagfx::uaui::ClipBox clip{};
    bool transformed = false;
    float m[16] = {};
    bool glass = false;  // 3.0.18-beta.64: the frosted glass's shape (drawn by the renderer's Backdrop shader)
};

struct Geometry {
    std::vector<::uagfx::uaui::Point> points;
    std::vector<int> indices;
};


struct State {
    bool started = false, failed = false, on = false, inWorld = false, dragging = false;
    bool leftDown = false, rightDown = false;  // 3.0.18-beta.22: a press taken by uaUI, its release ours too
    Rml::Context* context = nullptr;
    HWND game = nullptr;
    unsigned codePage = 1251;
    double k = 1;
    int targetW = 0, targetH = 0;
    std::vector<Doc*> docs;  // every part's, made in turn
    ULONGLONG themeAt = 0;
    FILETIME themeStamp{};
    bool themeFile = false;
    double frameStart = 0;
    // the renderer's side
    std::unordered_map<uintptr_t, Geometry> geometries;
    uintptr_t nextGeometry = 1, nextTexture = 16;
    std::unordered_map<uintptr_t, Texture> textures;
    std::vector<uint32_t> released;
    std::vector<Batch> batches;
    // 3.0.18-beta.27: the windows' documents (z-index 10) after the parts' (auto) - the batches from `split` on
    // (the marker's, z-index 5) drawn where the windows are drawn
    size_t split = 0;
    bool splitSeen = false;
    Rml::ElementDocument* marker = nullptr;
    // 3.0.18-beta.38: the world's documents (z-index 1: the bars over the monsters, the plates) before the parts'
    // (3) - the batches up to `worldSplit` (the marker's at z-index 2) drawn where the world's overlays are
    size_t worldSplit = 0;
    unsigned frameNo = 1;  // 3.0.18-beta.40: begin()'s count - a document not asked to show in one hidden at its end
    bool worldSeen = false;
    Rml::ElementDocument* worldMarker = nullptr;
    bool clipOn = false;
    ::uagfx::uaui::ClipBox clip{};
    // 3.0.18-beta.61 ([interface] uaui_effects): the rounded clip's outline (overflow hidden + border-radius), the
    // gradients RmlUi compiled, the geometries shaded by them (the gradient's, its geometry's) - made once
    bool maskOn = false;
    std::vector<float> mask;
    std::unordered_map<uintptr_t, ::uagfx::uaui::Gradient> shaders;
    uintptr_t nextShader = 1;
    std::map<std::pair<uintptr_t, uintptr_t>, uintptr_t> shaded;
    bool effects = false;
    bool glassOn = false, glassNext = false;  // 3.0.18-beta.64: [interface] uaui_glass (Direct3D 12); a glass draw
    bool transformOn = false;
    float transform[16] = {};
    // the words
    std::unordered_map<int, Face> faces;
    int fontVersion = 1;  // 3.0.18-beta.48: a new one each time the atlas starts again - RmlUi makes its words anew
    Atlas atlas;
    uint32_t atlasId = 0;
    HDC dc = nullptr;
    HBITMAP bitmap = nullptr;
    uint32_t* bits = nullptr;
    int bitsW = 0, bitsH = 0;
    Stats stats;
    int logged = 0;
    // 3.0.18-beta.12: the recorded pictures' side (sceneReady, drawScene)
    struct Cached {
        int generation = 0, x = 0, y = 0, w = 0, h = 0, epoch = 0, theme = 0;
        float grow = 1;
        ULONGLONG used = 0;
        float k = 0, opacity = -1;
        bool residual = false;
        std::vector<std::pair<uint32_t, std::vector<glint::Vertex>>> runs;  // by texture (0 none)
    };
    std::unordered_map<const void*, Cached> scenes;  // by the picture's scene
    struct Tex {
        uint32_t id = 0;
        int w = 0, h = 0;
        uint32_t sum = 0;
        ULONGLONG used = 0;
    };
    std::unordered_map<std::string, Tex> icons;
    std::unordered_map<uint32_t, Tex> images;  // by their pixels' sum
    // the words' atlas: each word's GDI coverage as it is
    struct Slot {
        int x = 0, y = 0;
    };
    std::vector<uint32_t> wordsPx;
    std::unordered_map<std::string, Slot> words;
    int wordsX = 0, wordsY = 0, wordsRow = 0, wordsEpoch = 1;
    uint32_t wordsId = 0;
    bool wordsDirty = false;
    int scenesDrawn = 0, sceneVertices = 0;
    bool recordingTold = false;
    // 3.0.18-beta.13: the theme ([interface] theme, uaro-graphics\ui\palette.txt over it); its serial in the meshes
    uitheme::Theme theme;
    std::string themeName;
    int themeSerial = 1;
    FILETIME paletteStamp{};
    bool paletteThere = false;
    // 3.0.18-beta.13: a picture coming into sight (not drawn for a while) grows from 96 % and fades in, 120 ms
    struct Seen {
        ULONGLONG last = 0, since = 0;
    };
    std::unordered_map<const void*, Seen> seen;
    std::map<std::pair<int, int>, int> unrecorded;
};

State& state() {
    static State s;
    return s;
}

// ---- RmlUi's interfaces ----

// ---- RmlUi's interfaces ----

class System final : public Rml::SystemInterface {
public:
    double GetElapsedTime() override { return clockNow(); }
    // our own names as they are: «uaui:» the built-in files (Files), «icon:» a picture of the driver's
    void JoinPath(Rml::String& out, const Rml::String& document, const Rml::String& path) override {
        if (path.rfind("uaui:", 0) == 0 || path.rfind("icon:", 0) == 0 || path.rfind("pic:", 0) == 0) {
            out = path;
            return;
        }
        Rml::SystemInterface::JoinPath(out, document, path);
    }
    bool LogMessage(Rml::Log::Type type, const Rml::String& message) override {
        State& s = state();
        // 3.0.18-beta.46 (R-4YDWHF: the 20 first ones were the documents' addresses - the chat's never told): each
        // message once, 300 of them
        static std::set<std::string> told;
        std::string kind = message.substr(0, 200);  // 3.0.18-beta.53: «words[13]» and «words[14]» one kind
        for (char& c : kind)
            if (c >= '0' && c <= '9') c = '#';
        if (type <= Rml::Log::LT_WARNING && s.logged < 300 && told.insert(kind).second) {
            ++s.logged;
            LOG_INFO("uaui: RmlUi %s: %s", type == Rml::Log::LT_WARNING ? "warns" : "says an error", message.c_str());
        }
        return true;
    }
};

class Render final : public Rml::RenderInterface {
public:
    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> v, Rml::Span<const int> ix) override {
        State& s = state();
        Geometry g;
        g.points.reserve(v.size());
        for (const Rml::Vertex& p : v) {
            const uint32_t argb = uint32_t(p.colour.alpha) << 24 | uint32_t(p.colour.red) << 16 |
                                  uint32_t(p.colour.green) << 8 | uint32_t(p.colour.blue);
            g.points.push_back({p.position.x, p.position.y, argb, p.tex_coord.x, p.tex_coord.y});
        }
        g.indices.assign(ix.begin(), ix.end());
        const uintptr_t h = s.nextGeometry++;
        s.geometries.emplace(h, std::move(g));
        return Rml::CompiledGeometryHandle(h);
    }
    void RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f t, Rml::TextureHandle texture) override {
        State& s = state();
        Batch b;
        b.geometry = uintptr_t(geometry);
        b.dx = t.x;
        b.dy = t.y;
        b.texture = uintptr_t(texture);
        b.clipped = s.clipOn;
        b.clip = s.clip;
        if (s.maskOn && s.effects && s.mask.size() >= 6) {  // 3.0.18-beta.61: cut by the rounded outline too
            if (!b.clipped) b.clip = {-1e6f, -1e6f, 1e6f, 1e6f, {}};
            b.clipped = true;
            b.clip.hull = s.mask;
        }
        b.transformed = s.transformOn;
        b.glass = s.glassNext;
        if (s.transformOn) std::copy(s.transform, s.transform + 16, b.m);
        s.batches.push_back(b);
    }
    void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override {
        State& s = state();
        s.geometries.erase(uintptr_t(geometry));
        for (auto it = s.shaded.begin(); it != s.shaded.end();)  // 3.0.18-beta.61: its shaded copies with it
            if (it->first.second == uintptr_t(geometry)) s.geometries.erase(it->second), it = s.shaded.erase(it);
            else ++it;
    }
    // 3.0.18-beta.61 (the possibilities' item 13): a linear gradient as the vertices' colours (core/uaui_mesh
    // shade); the others (radial, conic) not drawn - told once
    Rml::CompiledShaderHandle CompileShader(const Rml::String& name, const Rml::Dictionary& p) override {
        State& s = state();
        const bool radial = name == "radial-gradient";  // 3.0.18-beta.63
        if ((name != "linear-gradient" && !radial) || !s.effects) {
            static std::set<std::string> told;
            if (told.insert(name).second)
                LOG_INFO("uaui: no «%s» drawn (linear and radial gradients, with uaui_effects)", name.c_str());
            return 0;
        }
        ::uagfx::uaui::Gradient g;
        const Rml::Vector2f p0 = radial ? Rml::Get(p, "center", Rml::Vector2f(0, 0))
                                        : Rml::Get(p, "p0", Rml::Vector2f(0, 0));
        const Rml::Vector2f p1 = radial ? Rml::Get(p, "radius", Rml::Vector2f(1, 1))
                                        : Rml::Get(p, "p1", Rml::Vector2f(0, 1));
        g.radial = radial;
        g.x0 = p0.x, g.y0 = p0.y, g.x1 = p1.x, g.y1 = p1.y;
        g.repeating = Rml::Get(p, "repeating", false);
        const auto it = p.find("color_stop_list");
        if (it != p.end() && it->second.GetType() == Rml::Variant::COLORSTOPLIST)
            for (const Rml::ColorStop& c : it->second.GetReference<Rml::ColorStopList>())
                g.stops.push_back({c.position.number, uint32_t(c.color.alpha) << 24 | uint32_t(c.color.red) << 16 |
                                                          uint32_t(c.color.green) << 8 | uint32_t(c.color.blue)});
        const uintptr_t h = s.nextShader++;
        s.shaders.emplace(h, std::move(g));
        return Rml::CompiledShaderHandle(h);
    }
    void RenderShader(Rml::CompiledShaderHandle shader, Rml::CompiledGeometryHandle geometry, Rml::Vector2f t,
                      Rml::TextureHandle texture) override {
        State& s = state();
        const auto sh = s.shaders.find(uintptr_t(shader));
        const auto g = s.geometries.find(uintptr_t(geometry));
        if (sh == s.shaders.end() || g == s.geometries.end()) return;
        const std::pair<uintptr_t, uintptr_t> key{uintptr_t(shader), uintptr_t(geometry)};
        auto it = s.shaded.find(key);
        if (it == s.shaded.end()) {
            Geometry copy = g->second;
            ::uagfx::uaui::shade(copy.points, copy.indices, sh->second);
            const uintptr_t h = s.nextGeometry++;
            s.geometries.emplace(h, std::move(copy));
            it = s.shaded.emplace(key, h).first;
        }
        RenderGeometry(Rml::CompiledGeometryHandle(it->second), t, texture);
    }
    void ReleaseShader(Rml::CompiledShaderHandle shader) override {
        State& s = state();
        s.shaders.erase(uintptr_t(shader));
        for (auto it = s.shaded.begin(); it != s.shaded.end();)
            if (it->first.first == uintptr_t(shader)) s.geometries.erase(it->second), it = s.shaded.erase(it);
            else ++it;
    }
    // 3.0.18-beta.61 (item 16): a rounded clip - the mask's convex outline (set or intersected: the last one;
    // an inverted one not kept)
    void EnableClipMask(bool enable) override { state().maskOn = enable; }
    void RenderToClipMask(Rml::ClipMaskOperation op, Rml::CompiledGeometryHandle geometry, Rml::Vector2f t) override {
        State& s = state();
        if (op == Rml::ClipMaskOperation::SetInverse) return;
        const auto g = s.geometries.find(uintptr_t(geometry));
        if (g == s.geometries.end()) return;
        s.mask = ::uagfx::uaui::hull(g->second.points, t.x, t.y, s.transformOn ? s.transform : nullptr);
    }
    Rml::TextureHandle LoadTexture(Rml::Vector2i& dimensions, const Rml::String& source) override {
        State& s = state();
        // 3.0.18-beta.9: a picture the driver keeps (an icon of client_tables, a portrait), as it is -
        // «pic:<address>:<generation>» (uaui::picture), «icon:<address>»
        if (source.rfind("icon:", 0) == 0 || source.rfind("pic:", 0) == 0) {
            const char* at = source.c_str() + (source[0] == 'i' ? 5 : 4);
            const auto* pic = reinterpret_cast<const panel::Picture*>(uintptr_t(std::strtoull(at, nullptr, 16)));
            if (pic && pic->width > 0 && pic->height > 0 && pic->px.size() >= size_t(pic->width) * pic->height) {
                Texture t;
                t.w = pic->width;
                t.h = pic->height;
                t.px.assign(pic->px.begin(), pic->px.begin() + ptrdiff_t(size_t(t.w) * t.h));
                dimensions = {t.w, t.h};
                const uintptr_t h = s.nextTexture++;
                s.textures.emplace(h, std::move(t));
                return Rml::TextureHandle(h);
            }
        }
        if (s.logged < 20) {
            ++s.logged;
            LOG_INFO("uaui: a picture asked by name (%s) - not in the spike", source.c_str());
        }
        dimensions = {0, 0};
        return 0;
    }
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i d) override {
        State& s = state();
        if (d.x <= 0 || d.y <= 0 || source.size() < size_t(d.x) * size_t(d.y) * 4) return 0;
        Texture t;
        t.w = d.x;
        t.h = d.y;
        t.px.resize(size_t(d.x) * size_t(d.y));
        for (size_t i = 0; i < t.px.size(); ++i)  // RGBA (premultiplied) bytes as our ARGB
            t.px[i] = uint32_t(source[i * 4 + 3]) << 24 | uint32_t(source[i * 4]) << 16 |
                      uint32_t(source[i * 4 + 1]) << 8 | uint32_t(source[i * 4 + 2]);
        const uintptr_t h = s.nextTexture++;
        s.textures.emplace(h, std::move(t));
        return Rml::TextureHandle(h);
    }
    void ReleaseTexture(Rml::TextureHandle texture) override {
        State& s = state();
        const auto it = s.textures.find(uintptr_t(texture));
        if (it == s.textures.end()) return;
        if (it->second.id) s.released.push_back(it->second.id);
        s.textures.erase(it);
    }
    void EnableScissorRegion(bool enable) override { state().clipOn = enable; }
    void SetScissorRegion(Rml::Rectanglei r) override {
        State& s = state();
        s.clip = {float(r.Left()), float(r.Top()), float(r.Right()), float(r.Bottom()), {}};
    }
    void SetTransform(const Rml::Matrix4f* m) override {
        State& s = state();
        s.transformOn = m != nullptr;
        if (m) std::copy(m->data(), m->data() + 16, s.transform);
    }
};

bool ensureBits(State& s, int w, int h) {
    if (s.bits && w <= s.bitsW && h <= s.bitsH) return true;
    if (s.bitmap) DeleteObject(s.bitmap);
    s.bitsW = std::max(w, 64);
    s.bitsH = std::max(h, 64);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = s.bitsW;
    bi.bmiHeader.biHeight = -s.bitsH;  // top down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    s.bitmap = CreateDIBSection(s.dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    s.bits = static_cast<uint32_t*>(bits);
    if (!s.bitmap || !s.bits) return false;
    SelectObject(s.dc, s.bitmap);
    return true;
}

const Glyph& glyphOf(State& s, Face& f, char32_t c) {
    const auto it = f.glyphs.find(c);
    if (it != f.glyphs.end()) return it->second;
    Glyph g;
    const std::wstring w = wide(std::u32string(1, c));
    // Plex where it has the letter, else Segoe UI (★ ● ✕ ...), as win/ui_font's words
    HFONT font = f.plex;
    WORD index[2] = {};
    SelectObject(s.dc, f.plex);
    if (GetGlyphIndicesW(s.dc, w.c_str(), int(w.size()), index, GGI_MARK_NONEXISTING_GLYPHS) == GDI_ERROR ||
        index[0] == 0xFFFF)
        font = f.segoe ? f.segoe : f.plex;
    SelectObject(s.dc, font);
    SIZE size{};
    GetTextExtentPoint32W(s.dc, w.c_str(), int(w.size()), &size);
    g.advance = size.cx;
    g.w = size.cx + 2 * kGlyphPad + 2;  // italics' overhang a little
    g.h = f.height + 2 * kGlyphPad;
    Atlas& a = s.atlas;
    if (c > U' ' && g.w > 0 && ensureBits(s, g.w, g.h)) {
        if (a.shelfX + g.w > kAtlas) a.shelfX = 0, a.shelfY += a.shelfH, a.shelfH = 0;
        if (a.shelfY + g.h > kAtlas) {
            a.full = true;  // no room: the letter drawn as nothing
            g.w = g.h = 0;
        } else {
            std::fill(s.bits, s.bits + size_t(s.bitsW) * s.bitsH, 0u);
            SetBkMode(s.dc, TRANSPARENT);
            SetTextColor(s.dc, RGB(255, 255, 255));
            TextOutW(s.dc, kGlyphPad, kGlyphPad, w.c_str(), int(w.size()));
            GdiFlush();
            g.x = a.shelfX;
            g.y = a.shelfY;
            for (int y = 0; y < g.h; ++y)
                for (int x = 0; x < g.w; ++x) {
                    const uint32_t p = s.bits[size_t(y) * s.bitsW + x];
                    const uint32_t cover = std::max({p & 0xFF, (p >> 8) & 0xFF, (p >> 16) & 0xFF});
                    a.px[size_t(g.y + y) * kAtlas + size_t(g.x + x)] = cover << 24 | cover << 16 | cover << 8 | cover;
                }
            a.shelfX += g.w;
            a.shelfH = std::max(a.shelfH, g.h);
            a.touch(g.y, g.y + g.h);
            ++a.glyphs;
        }
    } else {
        g.w = g.h = 0;  // a space: its advance alone
    }
    return f.glyphs.emplace(c, g).first->second;
}

// 3.0.18-beta.20: a glyph's outline (its coverage grown by 2 pixels, a disc of 5 - win/picture_draw's halo) in the
// atlas beside it, white; drawn under the letters in the effect's colour
const Glyph& haloOf(State& s, Face& f, char32_t c) {
    const auto it = f.halos.find(c);
    if (it != f.halos.end()) return it->second;
    const Glyph g = glyphOf(s, f, c);
    Glyph h;
    h.advance = g.advance;
    Atlas& a = s.atlas;
    if (g.w > 0 && g.h > 0) {
        h.w = g.w + 4, h.h = g.h + 4;
        if (a.shelfX + h.w > kAtlas) a.shelfX = 0, a.shelfY += a.shelfH, a.shelfH = 0;
        if (a.shelfY + h.h > kAtlas) {
            a.full = true;
            h.w = h.h = 0;
        } else {
            h.x = a.shelfX, h.y = a.shelfY;
            const auto cover = [&](int x, int y) -> uint32_t {
                return x < 0 || y < 0 || x >= g.w || y >= g.h ? 0u
                                                                : a.px[size_t(g.y + y) * kAtlas + size_t(g.x + x)] >> 24;
            };
            for (int y = 0; y < h.h; ++y)
                for (int x = 0; x < h.w; ++x) {
                    uint32_t v = 0;
                    for (int dy = -2; dy <= 2 && v < 255; ++dy)
                        for (int dx = -2; dx <= 2; ++dx)
                            if (dx * dx + dy * dy <= 5) v = std::max(v, cover(x - 2 + dx, y - 2 + dy));
                    a.px[size_t(h.y + y) * kAtlas + size_t(h.x + x)] = v << 24 | v << 16 | v << 8 | v;
                }
            a.shelfX += h.w;
            a.shelfH = std::max(a.shelfH, h.h);
            a.touch(h.y, h.y + h.h);
        }
    }
    return f.halos.emplace(c, h).first->second;
}

// 3.0.18-beta.61 (the possibilities' item 14): decorator: ua-shadow(<blur> <colour>) - a soft shadow or glow round
// the element's border box (its top left corner's radius), the colour at its edge fading to nothing `blur` out;
// with [interface] uaui_effects only
class Shadow final : public Rml::Decorator {
public:
    float blur = 8;
    Rml::Colourb colour{0, 0, 0, 96};
    Rml::NumericValue blurValue{8, Rml::Unit::DP};
    struct Data {
        Rml::Geometry geometry;
    };
    Rml::DecoratorDataHandle GenerateElementData(Rml::Element* e, Rml::BoxArea) const override {
        auto* d = new Data;
        const Rml::Vector2f size = e->GetBox().GetSize(Rml::BoxArea::Border);
        const float b = e->ResolveLength(blurValue);
        const float r = e->GetComputedValues().border_top_left_radius();
        const Rml::ColourbPremultiplied c = colour.ToPremultiplied(e->GetComputedValues().opacity());
        const uint32_t argb = uint32_t(c.alpha) << 24 | uint32_t(c.red) << 16 | uint32_t(c.green) << 8 | c.blue;
        std::vector<::uagfx::uaui::Point> pts;
        std::vector<int> ix;
        ::uagfx::uaui::softRing(0, 0, size.x, size.y, r, b, argb, pts, ix);
        Rml::Mesh mesh;
        for (const auto& p : pts) {
            Rml::Vertex v;
            v.position = {p.x, p.y};
            v.colour = Rml::ColourbPremultiplied(Rml::byte(p.argb >> 16), Rml::byte(p.argb >> 8), Rml::byte(p.argb),
                                                 Rml::byte(p.argb >> 24));
            v.tex_coord = {0, 0};
            mesh.vertices.push_back(v);
        }
        mesh.indices = ix;
        if (Rml::Context* ctx = e->GetContext()) d->geometry = ctx->GetRenderManager().MakeGeometry(std::move(mesh));
        return reinterpret_cast<Rml::DecoratorDataHandle>(d);
    }
    void ReleaseElementData(Rml::DecoratorDataHandle h) const override { delete reinterpret_cast<Data*>(h); }
    void RenderElement(Rml::Element* e, Rml::DecoratorDataHandle h) const override {
        auto* d = reinterpret_cast<Data*>(h);
        if (d && state().effects) d->geometry.Render(e->GetAbsoluteOffset(Rml::BoxArea::Border));
    }
};

class ShadowInstancer final : public Rml::DecoratorInstancer {
public:
    ShadowInstancer() {
        blur_ = RegisterProperty("blur", "8dp").AddParser("length").GetId();
        colour_ = RegisterProperty("color", "#00000060").AddParser("color").GetId();
        RegisterShorthand("decorator", "blur, color", Rml::ShorthandType::FallThrough);
    }
    Rml::SharedPtr<Rml::Decorator> InstanceDecorator(const Rml::String&, const Rml::PropertyDictionary& p,
                                                     const Rml::DecoratorInstancerInterface&) override {
        auto d = Rml::MakeShared<Shadow>();
        if (const Rml::Property* b = p.GetProperty(blur_)) d->blurValue = b->GetNumericValue();
        if (const Rml::Property* c = p.GetProperty(colour_)) d->colour = c->Get<Rml::Colourb>();
        return d;
    }

private:
    Rml::PropertyId blur_{}, colour_{};
};
ShadowInstancer* g_shadow = nullptr;

// 3.0.18-beta.64 (the possibilities' item 15): decorator: ua-glass(<share>) - the world under the element blurred
// (the renderer's Backdrop draw, Direct3D 12), its own ground laid over it at 3/4 of its alpha; with
// [interface] uaui_glass only
class Glass final : public Rml::Decorator {
public:
    float share = 1;
    struct Data {
        Rml::Geometry shape, tint;
    };
    static Rml::Geometry box(Rml::Element* e, Rml::Vector2f size, float r, uint32_t argb) {
        std::vector<::uagfx::uaui::Point> pts;
        std::vector<int> ix;
        ::uagfx::uaui::roundedBox(0, 0, size.x, size.y, r, argb, pts, ix);
        Rml::Mesh mesh;
        for (const auto& p : pts) {
            Rml::Vertex v;
            v.position = {p.x, p.y};
            v.colour = Rml::ColourbPremultiplied(Rml::byte(p.argb >> 16), Rml::byte(p.argb >> 8), Rml::byte(p.argb),
                                                 Rml::byte(p.argb >> 24));
            v.tex_coord = {0, 0};
            mesh.vertices.push_back(v);
        }
        mesh.indices = ix;
        Rml::Context* ctx = e->GetContext();
        return ctx ? ctx->GetRenderManager().MakeGeometry(std::move(mesh)) : Rml::Geometry();
    }
    Rml::DecoratorDataHandle GenerateElementData(Rml::Element* e, Rml::BoxArea) const override {
        auto* d = new Data;
        const Rml::ComputedValues& cv = e->GetComputedValues();
        const Rml::Vector2f size = e->GetBox().GetSize(Rml::BoxArea::Border);
        const float r = cv.border_top_left_radius(), op = cv.opacity();
        const uint32_t a = uint32_t(std::lround(std::clamp(share * op, 0.0f, 1.0f) * 255));
        d->shape = box(e, size, r, a << 24 | 0xFFFFFFu);
        Rml::Colourb bg = cv.background_color();
        bg.alpha = Rml::byte(std::lround(bg.alpha * 0.75f));
        const Rml::ColourbPremultiplied t = bg.ToPremultiplied(op);
        d->tint = box(e, size, r, uint32_t(t.alpha) << 24 | uint32_t(t.red) << 16 | uint32_t(t.green) << 8 | t.blue);
        return reinterpret_cast<Rml::DecoratorDataHandle>(d);
    }
    void ReleaseElementData(Rml::DecoratorDataHandle h) const override { delete reinterpret_cast<Data*>(h); }
    void RenderElement(Rml::Element* e, Rml::DecoratorDataHandle h) const override {
        auto* d = reinterpret_cast<Data*>(h);
        State& s = state();
        if (!d || !s.glassOn) return;
        const Rml::Vector2f at = e->GetAbsoluteOffset(Rml::BoxArea::Border);
        s.glassNext = true;
        d->shape.Render(at);
        s.glassNext = false;
        d->tint.Render(at);
    }
};

class GlassInstancer final : public Rml::DecoratorInstancer {
public:
    GlassInstancer() {
        share_ = RegisterProperty("share", "1").AddParser("number").GetId();
        RegisterShorthand("decorator", "share", Rml::ShorthandType::FallThrough);
    }
    Rml::SharedPtr<Rml::Decorator> InstanceDecorator(const Rml::String&, const Rml::PropertyDictionary& p,
                                                     const Rml::DecoratorInstancerInterface&) override {
        auto d = Rml::MakeShared<Glass>();
        if (const Rml::Property* v = p.GetProperty(share_)) d->share = v->Get<float>();
        return d;
    }

private:
    Rml::PropertyId share_{};
};

class Fonts final : public Rml::FontEngineInterface {
public:
    Rml::FontFaceHandle GetFontFaceHandle(const Rml::String&, Rml::Style::FontStyle style, Rml::Style::FontWeight weight,
                                          int size) override {
        State& s = state();
        const bool bold = int(weight) >= 600;
        const bool italic = style == Rml::Style::FontStyle::Italic;
        const int key = size * 4 + (bold ? 1 : 0) + (italic ? 2 : 0);
        auto it = s.faces.find(key);
        if (it == s.faces.end()) {
            Face f;
            const uifont::Face plex = uifont::faceFor(L"A", bold);
            f.plex = CreateFontW(-size, 0, 0, 0, plex.weight, italic, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, plex.name);
            f.segoe = CreateFontW(-size, 0, 0, 0, bold ? FW_SEMIBOLD : FW_NORMAL, italic, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                                  DEFAULT_PITCH, L"Segoe UI");
            TEXTMETRICW tm{};
            SelectObject(s.dc, f.plex);
            GetTextMetricsW(s.dc, &tm);
            f.ascent = tm.tmAscent;
            f.height = tm.tmHeight;
            f.metrics.size = size;
            f.metrics.ascent = float(tm.tmAscent);
            f.metrics.descent = float(tm.tmDescent);
            f.metrics.line_spacing = float(tm.tmHeight + tm.tmExternalLeading);
            f.metrics.x_height = float(tm.tmAscent) * 0.55f;
            f.metrics.underline_position = float(tm.tmDescent) * 0.5f;
            f.metrics.underline_thickness = 1;
            f.metrics.has_ellipsis = true;
            it = s.faces.emplace(key, std::move(f)).first;
        }
        return Rml::FontFaceHandle(key);
    }
    const Rml::FontMetrics& GetFontMetrics(Rml::FontFaceHandle h) override { return faceOf(h).metrics; }
    int GetVersion(Rml::FontFaceHandle) override { return state().fontVersion; }
    // 3.0.18-beta.20: an effect behind the letters (font-effect: outline(2dp #000000)) - their outline in its colour
    Rml::FontEffectsHandle PrepareFontEffects(Rml::FontFaceHandle, const Rml::FontEffectList& effects) override {
        for (const auto& e : effects)
            if (e && e->GetLayer() == Rml::FontEffect::Layer::Back) {
                const Rml::Colourb c = e->GetColour();
                return Rml::FontEffectsHandle(1 + (uint32_t(c.alpha) << 24 | uint32_t(c.red) << 16 |
                                                   uint32_t(c.green) << 8 | uint32_t(c.blue)));
            }
        return 0;
    }
    int GetStringWidth(Rml::FontFaceHandle h, Rml::StringView text, const Rml::TextShapingContext&,
                       Rml::Character) override {
        State& s = state();
        Face& f = faceOf(h);
        int w = 0;
        for (char32_t c : ::uagfx::uaui::codepoints(std::string_view(text.begin(), text.size())))
            w += glyphOf(s, f, c).advance;
        return w;
    }
    int GenerateString(Rml::RenderManager& rm, Rml::FontFaceHandle h, Rml::FontEffectsHandle fx, Rml::StringView text,
                       Rml::Vector2f at, Rml::ColourbPremultiplied colour, float opacity, const Rml::TextShapingContext&,
                       Rml::TexturedMeshList& list) override {
        State& s = state();
        Face& f = faceOf(h);
        if (fx) {  // 3.0.18-beta.20: the outline first, under the letters
            const uint32_t c = uint32_t(uintptr_t(fx) - 1);
            const Rml::ColourbPremultiplied hc = Rml::Colourb(Rml::byte(c >> 16), Rml::byte(c >> 8), Rml::byte(c),
                                                              Rml::byte(c >> 24))
                                                     .ToPremultiplied(opacity);
            Rml::Mesh halo;
            float pen = 0;
            const float inv = 1.0f / float(kAtlas);
            for (char32_t ch : ::uagfx::uaui::codepoints(std::string_view(text.begin(), text.size()))) {
                const Glyph& g = haloOf(s, f, ch);
                if (g.w > 0 && g.h > 0) {
                    const float x0 = std::round(at.x + pen) - kGlyphPad - 2,
                                y0 = std::round(at.y) - float(f.ascent) - kGlyphPad - 2;
                    const int base = int(halo.vertices.size());
                    for (int q = 0; q < 4; ++q) {
                        Rml::Vertex v;
                        v.position = {x0 + float((q & 1) * g.w), y0 + float((q >> 1) * g.h)};
                        v.colour = hc;
                        v.tex_coord = {float(g.x + (q & 1) * g.w) * inv, float(g.y + (q >> 1) * g.h) * inv};
                        halo.vertices.push_back(v);
                    }
                    for (int q : {0, 1, 2, 1, 3, 2}) halo.indices.push_back(base + q);
                }
                pen += float(g.advance);
            }
            if (!halo.vertices.empty()) list.push_back({std::move(halo), atlasSource_.GetTexture(rm)});
        }
        Rml::Mesh mesh;
        float pen = 0;
        const float inv = 1.0f / float(kAtlas);
        for (char32_t c : ::uagfx::uaui::codepoints(std::string_view(text.begin(), text.size()))) {
            const Glyph& g = glyphOf(s, f, c);
            if (g.w > 0 && g.h > 0) {
                const float x0 = std::round(at.x + pen) - kGlyphPad, y0 = std::round(at.y) - float(f.ascent) - kGlyphPad;
                const int base = int(mesh.vertices.size());
                for (int q = 0; q < 4; ++q) {
                    Rml::Vertex v;
                    v.position = {x0 + float((q & 1) * g.w), y0 + float((q >> 1) * g.h)};
                    v.colour = colour;
                    v.tex_coord = {float(g.x + (q & 1) * g.w) * inv, float(g.y + (q >> 1) * g.h) * inv};
                    mesh.vertices.push_back(v);
                }
                for (int q : {0, 1, 2, 1, 3, 2}) mesh.indices.push_back(base + q);
            }
            pen += float(g.advance);
        }
        if (!mesh.vertices.empty()) list.push_back({std::move(mesh), atlasSource_.GetTexture(rm)});
        return int(pen);
    }

private:
    static Face& faceOf(Rml::FontFaceHandle h) {
        State& s = state();
        const auto it = s.faces.find(int(h));
        if (it != s.faces.end()) return it->second;
        static Face none;
        return none;
    }
    // the atlas as RmlUi's texture: its handle ours (kAtlasHandle), uploaded by draw() when new glyphs came
    Rml::CallbackTextureSource atlasSource_{[](const Rml::CallbackTextureInterface& t) {
        t.SetTextureHandle(Rml::TextureHandle(kAtlasHandle), {kAtlas, kAtlas});
        return true;
    }};
};

System g_system;
Render g_render;
Fonts* g_fonts = nullptr;  // made at the start (its texture source needs RmlUi's types alive)

}  // namespace

// 3.0.18-beta.57 (docs/superpowers/plans/2026-11-10-uaui-possibilities.md, stage 1): the words' width as this engine
// draws them - its GDI face at the size RmlUi asks for (truncated to whole pixels); < 0 before the start
float textWidth(const std::string& utf8, float px, bool bold) {
    State& s = state();
    const int size = int(px);
    if (!g_fonts || !s.started || size <= 0) return -1;
    const Rml::FontFaceHandle h =
        g_fonts->GetFontFaceHandle("", Rml::Style::FontStyle::Normal,
                                   bold ? Rml::Style::FontWeight::Bold : Rml::Style::FontWeight::Normal, size);
    const Rml::String language;
    return float(g_fonts->GetStringWidth(h, utf8, Rml::TextShapingContext{language}, Rml::Character::Null));
}

namespace {

// ---- the theme and the components: every colour, size and motion in one style sheet ----

// ---- the theme and the components (stage 2): every colour, size and motion in one style sheet ----

// The built-in theme: the new interface's look (the concept's colours, 8dp corners, 0.12-0.16 s eased out).
// uaro-graphics\ui\theme.rcss in its place when there - read again within a second of a change.
const char* kTheme = R"(
body { font-family: Plex; font-size: 14dp; color: #e9edf2; }
.ua-screen { width: 100%; height: 100%; }
.ua-card { position: absolute; left: 50%; top: 96dp; margin-left: -160dp; width: 320dp; padding: 12dp 14dp;
           border-radius: 8dp; border: 1dp #ffffff22; background-color: #161b22e0; drag: drag; cursor: move;
           transition: opacity transform background-color border-color 0.16s quadratic-out; }
.ua-card.away { opacity: 0; transform: scale(0.96); }
.ua-card:hover { background-color: #1e2633f0; border-color: #3d7fd8aa; }
.ua-name { font-weight: bold; font-size: 15dp; }
.ua-sub { color: #9aa4b2; font-size: 12dp; margin-left: 8dp; }
.ua-row { display: block; margin-top: 8dp; }
.ua-num { font-size: 12dp; color: #c9d1d9; }
.ua-bar { display: block; height: 8dp; border-radius: 4dp; background-color: #2a313c; margin-top: 4dp; }
.ua-fill { display: block; height: 8dp; border-radius: 4dp; background-color: #3fb950; }
.ua-fill.sp { background-color: #388bfd; }
.ua-tag { display: block; margin-top: 10dp; font-size: 11dp; color: #7d8590; }
.ua-window { position: absolute; width: 440dp; border-radius: 8dp; border: 1dp #ffffff22;
             background-color: #161b22eb; transition: opacity transform 0.12s quadratic-out; }
.ua-window.away { opacity: 0; transform: scale(0.96); }
.ua-title { display: block; height: 40dp; padding-left: 16dp; border-bottom: 1dp #ffffff14; cursor: move; }
.ua-title-name { font-size: 15dp; font-weight: bold; line-height: 40dp; }
.ua-close { position: absolute; right: 8dp; top: 6dp; width: 28dp; height: 28dp; line-height: 28dp;
            text-align: center; border-radius: 6dp; color: #8b949e; transition: background-color color 0.1s; }
.ua-close:hover { background-color: #ffffff18; color: #e9edf2; }
.ua-content { display: block; padding: 12dp 16dp; }
.ua-hint { display: block; padding: 8dp 16dp 10dp 16dp; border-top: 1dp #ffffff14; font-size: 11dp; color: #8b949e; }
tabset { display: block; }
tabset tabs { display: block; margin-bottom: 12dp; }
tab { display: inline-block; margin-right: 18dp; padding-bottom: 4dp; color: #8b949e; border-bottom: 2dp #00000000;
      transition: color border-color 0.12s; cursor: pointer; }
tab:hover { color: #c9d1d9; }
tab:selected { color: #e9edf2; font-weight: bold; border-bottom-color: #3d7fd8; }
panels { display: block; }
.ua-grid { display: block; }
.ua-slot { display: inline-block; position: relative; width: 52dp; height: 52dp; margin: 0 6dp 6dp 0;
           border-radius: 8dp; border: 1dp #ffffff1a; background-color: #ffffff06;
           transition: background-color border-color 0.1s; }
.ua-slot:hover { background-color: #ffffff14; border-color: #3d7fd8aa; }
.ua-slot img { position: absolute; left: 6dp; top: 6dp; width: 40dp; height: 40dp; }
.ua-count { position: absolute; right: 4dp; bottom: 2dp; font-size: 11dp; font-weight: bold; }
.ua-list { display: block; height: 220dp; overflow-y: auto; }
.ua-list p { display: block; padding: 3dp 6dp; border-radius: 4dp; }
.ua-list p:hover { background-color: #ffffff0e; }
.ua-label { display: block; color: #8b949e; font-size: 12dp; margin: 10dp 0 6dp 0; }
.ua-button { display: inline-block; padding: 6dp 14dp; margin: 0 8dp 8dp 0; border-radius: 6dp;
             background-color: #2a313c; transition: background-color 0.1s; cursor: pointer; }
.ua-button:hover { background-color: #34404f; }
.ua-button:active { background-color: #3d7fd8; }
input.checkbox { display: inline-block; width: 16dp; height: 16dp; margin-right: 8dp; border-radius: 4dp;
                 border: 1dp #ffffff50; vertical-align: middle; transition: background-color border-color 0.1s; }
input.checkbox:checked { background-color: #3d7fd8; border-color: #3d7fd8; }
/* 3.0.18-beta.59 (the possibilities' item 9): the form's parts in the game's look */
input.radio { display: inline-block; width: 16dp; height: 16dp; margin-right: 8dp; border-radius: 8dp;
              background-color: #0000004d; border: 1dp #ffffff40; vertical-align: middle; }
input.radio:checked { background-color: #c9a55b; border-color: #e6cb8a; }
input.text, input.password, textarea { display: inline-block; box-sizing: border-box; width: 100%; height: 30dp;
    padding: 6dp 10dp; border-radius: 6dp; background-color: #00000059; border: 1dp #ffffff1a; color: #eceff5;
    cursor: text; }
input.text:focus, input.password:focus, textarea:focus { border-color: #c9a55b99; }
textarea { height: 80dp; }
input.range { display: inline-block; width: 100%; height: 18dp; }
input.range slidertrack { margin-top: 7dp; height: 4dp; border-radius: 2dp; background-color: #ffffff1f; }
input.range sliderbar { width: 16dp; height: 16dp; margin-top: 1dp; border-radius: 8dp; background-color: #e6cb8a;
                        border: 1dp #00000066; }
input.range sliderbar:hover { background-color: #f3d998; }
input.range sliderarrowdec, input.range sliderarrowinc { width: 0; height: 0; }
select { display: inline-block; box-sizing: border-box; width: 100%; height: 30dp; border-radius: 6dp;
         background-color: #00000059; border: 1dp #ffffff1a; color: #eceff5; }
select selectvalue { margin: 6dp 30dp 0 10dp; height: 18dp; }
select selectarrow { width: 24dp; height: 28dp; }
select selectbox { margin-top: 2dp; padding: 4dp 0; border-radius: 6dp; background-color: #10141cf5;
                   border: 1dp #ffffff24; }
select selectbox option { display: block; padding: 4dp 10dp; }
select selectbox option:hover { background-color: #ffffff14; }
select selectbox option:checked { color: #e6cb8a; }
.ua-row2 { display: block; margin-bottom: 10dp; }
/* 3.0.18-beta.17: the character block (win/char_block) - the concept's v17_char, in concept pixels (dp) */
.cb { position: absolute; font-family: Plex; color: #eceff5; transform-origin: 0 0; }
.cb .card { position: absolute; left: 0; top: 0; border-radius: 10dp; border: 1dp #ffffff17;
            background-color: #10141ccc; transition: border-color 0.3s quadratic-out; }
.cb.flare .card { border-color: #e6cb8a; }
.cb .abs { position: absolute; }
.cb .portrait { position: absolute; border-radius: 999px; background-color: #1b1d24; overflow: hidden;
                cursor: pointer; }
.cb .portrait img { position: absolute; left: 7%; top: 7%; width: 86%; height: 86%; }
.cb .letter { display: block; width: 100%; text-align: center; font-weight: bold; color: #eceff5; }
.cb .name { font-size: 16dp; font-weight: bold; }
.cb .job { font-size: 13dp; color: #9aa3b5; margin-left: 8dp; }
.cb .levels { position: absolute; right: 12dp; top: 14dp; font-size: 13dp; font-weight: bold; color: #e6cb8a; }
.cb .line { position: absolute; height: 17dp; font-size: 12.5dp; white-space: nowrap; }
.cb .muted { color: #9aa3b5; }
.cb .gold { color: #e6cb8a; }
.cb .warn { color: #f0a050; }
.cb .bad { color: #ff5a4e; }
.cb .fight { color: #f3d27a; font-weight: bold; }
.cb .chip { margin-right: 12dp; }
.cb .label { position: absolute; font-size: 11dp; font-weight: bold; color: #9aa3b5; }
.cb .track { position: absolute; border-radius: 5dp; background-color: #00000073; }
.cb .fill { position: absolute; left: 0; top: 0; height: 100%; border-radius: 5dp; }
.cb .hp { background-color: #5bd36c; }
.cb .hp.low { background-color: #ff5a4e; }
.cb .sp { background-color: #4f8bff; }
.cb .base { background-color: #b7a6ff; }
.cb .jobx { background-color: #7fd3c3; }
.cb .weightfill { background-color: #c9a55b; }
.cb .num { position: absolute; font-size: 13dp; font-weight: bold; text-align: right; }
.cb .num.low { color: #ff9a8f; }
.cb .small { position: absolute; font-size: 11dp; white-space: nowrap; }
.cb .cell { position: absolute; border-radius: 6dp; background-color: #1b1d24b3; }
.cb .cell .k { position: absolute; left: 7dp; top: 2dp; font-size: 9.5dp; font-weight: bold; color: #9aa3b5; }
.cb .cell .v { position: absolute; left: 7dp; top: 14dp; font-size: 12.5dp; font-weight: bold; }
.cb .button { position: absolute; border-radius: 7dp; background-color: #1b1d24b3; cursor: pointer;
              transition: background-color 0.1s; }
.cb .button:hover { background-color: #2a2f3a; }
.cb .button:active { background-color: #3a3f4c; }
.cb .button img { position: absolute; left: 7dp; top: 4dp; width: 18dp; height: 18dp; }
.cb .badge { position: absolute; border-radius: 9dp; background-color: #10141c; border: 1dp #c9a55b;
             text-align: center; font-weight: bold; font-size: 12dp; }
.cb .zone { position: absolute; }
.cb .tip { position: absolute; padding: 4dp 10dp 4dp 10dp; border-radius: 8dp; background-color: #10141cee;
           border: 1dp #ffffff17; white-space: nowrap; font-size: 13dp;
           margin-right: -2000dp; }  /* 3.0.18-beta.55 (R-PWJXJ3): as wide as its longest line, past the block */
.cb .tip p { display: block; color: #9aa3b5; line-height: 17dp; }
.cb .tip p.first { color: #eceff5; font-weight: bold; }
.cb .warning { position: absolute; padding: 0 12dp; height: 30dp; line-height: 30dp; border-radius: 8dp;
               background-color: #3a1414; font-size: 13dp; font-weight: bold; white-space: nowrap;
               transition: opacity transform 0.18s quadratic-out; }
.cb .warning.away { opacity: 0; transform: translateY(-8dp); }
/* 3.0.18-beta.18: the target's card (win/target_card) - the concept's target16 */
.tc { position: absolute; font-family: Plex; color: #eceff5; text-align: center; transform-origin: 50% 0;
      transition: opacity transform 0.12s quadratic-out; }
.tc.away { opacity: 0; transform: translateY(-4dp); }
.tc .card { display: inline-block; position: relative; height: 48dp; padding: 0 8dp; border-radius: 10dp;
            border: 1dp #ffffff17; background-color: #10141ccc; text-align: left; white-space: nowrap; cursor: pointer; }
.tc .corner { position: absolute; width: 11dp; height: 11dp; border-color: #c9a55b; }
.tc .tl { left: 0; top: 0; border-left-width: 2dp; border-top-width: 2dp; }
.tc .tr { right: 0; top: 0; border-right-width: 2dp; border-top-width: 2dp; }
.tc .bl { left: 0; bottom: 0; border-left-width: 2dp; border-bottom-width: 2dp; }
.tc .br { right: 0; bottom: 0; border-right-width: 2dp; border-bottom-width: 2dp; }
.tc .round { display: inline-block; vertical-align: top; margin-top: 8dp; width: 32dp; height: 32dp;
             border-radius: 16dp; background-color: #1b1d24; }
.tc .round img { width: 32dp; height: 32dp; }
.tc .mid { display: inline-block; vertical-align: top; margin-left: 10dp; }
.tc .name { font-size: 16dp; font-weight: bold; }
.tc .lv { font-size: 13dp; color: #9aa3b5; margin-left: 6dp; }
.tc .chip { display: inline-block; margin-left: 8dp; padding: 0 8dp; height: 22dp; line-height: 22dp;
            border-radius: 6dp; border-width: 1dp; font-size: 12.5dp; font-weight: bold; }
.tc .top { display: block; height: 22dp; line-height: 22dp; margin-top: 13dp; }
.tc .top.withhp { margin-top: 4dp; }
.tc .hpline { display: block; height: 16dp; }
.tc .hptrack { display: inline-block; vertical-align: middle; width: 80dp; height: 5dp; border-radius: 3dp;
               background-color: #0000008c; }
.tc .hpfill { display: block; height: 5dp; border-radius: 3dp; background-color: #5bd36c; }
.tc .hpfill.low { background-color: #e5534b; }
.tc .pct { margin-left: 6dp; font-size: 12dp; font-weight: bold; }
.tc .book { display: inline-block; vertical-align: top; margin: 9dp 0 0 12dp; width: 30dp; height: 30dp;
            border-radius: 7dp; background-color: #ffffff0f; border: 1dp #c9a55b00;
            transition: background-color border-color 0.1s; }
.tc .book:hover { background-color: #ffffff24; border-color: #c9a55bcc; }
.tc .book img { margin: 5dp 0 0 5dp; width: 18dp; height: 18dp; }
.tc .book .lit { display: none; }
.tc .book:hover .lit { display: inline-block; }
.tc .book:hover .dim { display: none; }
.tc .hints { display: inline-block; vertical-align: top; margin: 6dp 0 0 13dp; padding-left: 7dp;
             border-left: 1dp #ffffff1a; font-size: 13dp; }
.tc .hints div { display: block; height: 19dp; }
.tc .hints img { width: 17dp; height: 17dp; margin-right: 7dp; vertical-align: middle; }
.tc .details { display: none; margin-top: 6dp; padding: 5dp 12dp 5dp 12dp; border-radius: 8dp;
               border: 1dp #ffffff1f; background-color: #10141cf0; text-align: left; white-space: nowrap; }
.tc:hover .details { display: inline-block; }
.tc .details p { display: block; height: 18dp; line-height: 18dp; font-size: 13dp; }
.tc .details p.first { font-weight: bold; }
/* 3.0.18-beta.19: the party's panel (win/party_panel) - the concept's v19_cave */
.pp { position: absolute; font-family: Plex; color: #eceff5; transform-origin: 0 0; }
.pp .glass { display: block; width: 260dp; padding: 0 8dp 8dp 8dp; border-radius: 10dp; border: 1dp #ffffff17;
             background-color: #10141ccc; }
.pp .head { display: block; position: relative; height: 26dp; line-height: 26dp; border-bottom: 1dp #ffffff1a;
            font-size: 12dp; color: #9aa3b5; }
.pp .head.folded { border-bottom-width: 0; }
.pp .title { position: absolute; left: 4dp; font-weight: bold; }
.pp .count { position: absolute; right: 56dp; }
.pp .hbtn { position: absolute; top: 3dp; width: 22dp; height: 20dp; line-height: 20dp; text-align: center;
            border-radius: 5dp; color: #eceff5; cursor: pointer; }
.pp .hbtn:hover { background-color: #ffffff18; }
.pp .viewb { right: 26dp; }
.pp .foldb { right: 2dp; }
.pp .row { display: block; position: relative; height: 40dp; border-radius: 7dp; border: 1dp #00000000; cursor: pointer; }
.pp .row.buffs { height: 58dp; }
.pp .row.low { background-color: #e5534b1a; border-color: #e5534bd9; }
.pp .row.target { background-color: #c9a55b14; border-color: #c9a55be6; }
.pp .row.chosen { background-color: #c9a55b38; border-color: #c9a55b; border-width: 2dp; }
.pp .row.dead { background-color: #3a14148c; border-color: #e5534b99; }
.pp .row.alarm { background-color: #e5534b4d; border-color: #e5534b; border-width: 2dp; }
.pp .buf.glow { border: 1.5dp #c9a55b; }
.pp .face { position: absolute; left: 2dp; top: 6dp; width: 28dp; height: 28dp; border-radius: 14dp; background-color: #1b1d24; }
.pp .face img { width: 28dp; height: 28dp; }
.pp .face.away img { opacity: 0.45; }
.pp .letter { display: block; text-align: center; line-height: 28dp; font-size: 10.5dp; font-weight: bold; color: #9aa3b5; }
.pp .name { position: absolute; left: 38dp; top: 5dp; height: 18dp; line-height: 18dp; font-size: 13dp;
            font-weight: bold; white-space: nowrap; }
.pp .name.bad { color: #ff9a8f; }
.pp .name.away { color: #9aa3b5; }
.pp .after { font-size: 11.5dp; font-weight: normal; color: #9aa3b5; margin-left: 6dp; }
.pp .after.cast { color: #7fc4ff; }
.pp .crown { width: 13dp; height: 13dp; margin-left: 4dp; vertical-align: middle; }
.pp .value { position: absolute; right: 4dp; top: 5dp; height: 18dp; line-height: 18dp; font-size: 13dp;
             font-weight: bold; white-space: nowrap; }
.pp .value.muted { color: #9aa3b5; font-size: 12dp; font-weight: normal; }
.pp .value.bad { color: #ff9a8f; }
.pp .value img { width: 12dp; height: 12dp; margin-right: 4dp; vertical-align: middle; }
.pp .bar { position: absolute; left: 38dp; top: 27dp; right: 48dp; height: 5dp; border-radius: 3dp;
           background-color: #00000073; }
.pp .bar .fill { display: block; height: 5dp; border-radius: 3dp; background-color: #5bd36c; }
.pp .bar .fill.bad { background-color: #e5534b; }
.pp .castline { position: absolute; left: 38dp; top: 33.5dp; right: 48dp; height: 2.5dp; border-radius: 1dp;
                background-color: #00000073; }
.pp .castline .fill { display: block; height: 2.5dp; background-color: #7fc4ff; }
.pp .where { position: absolute; right: 4dp; top: 22dp; width: 40dp; height: 14dp; font-size: 11dp; color: #9aa3b5;
             text-align: right; }
.pp .where img { position: absolute; left: 0; top: 0; width: 14dp; height: 14dp; }
.pp .bufrow { position: absolute; left: 38dp; top: 37dp; white-space: nowrap; }
.pp .buf { display: inline-block; position: relative; width: 16dp; height: 16dp; margin-right: 3dp;
           border-radius: 3dp; }
.pp .buf img { width: 16dp; height: 16dp; }
.pp .buf .time { position: absolute; left: 0; top: 17dp; width: 16dp; height: 2dp; background-color: #00000080; }
.pp .buf .time .fill { display: block; height: 2dp; background-color: #c9a55b; }
.pp .buf .time .fill.end { background-color: #e5534b; }
.pp .cells { display: block; }
.pp .cell { display: inline-block; position: relative; width: 125dp; height: 30dp; margin-right: 6dp; }
.pp .cell .face { left: 2dp; top: 6dp; width: 16dp; height: 16dp; border-radius: 8dp; }
.pp .cell .face img { width: 16dp; height: 16dp; }
.pp .cell .letter { line-height: 16dp; font-size: 9dp; }
.pp .cell .name { left: 23dp; top: 3dp; height: 15dp; line-height: 15dp; font-size: 11.5dp; }
.pp .cell .bar { left: 23dp; right: 3dp; top: 20dp; height: 4dp; }
.pp .cell .bar .fill { height: 4dp; }
/* 3.0.18-beta.20: the buffs' column (win/buff_column) - the concept's v19_vbuffs_ab; places in the target's pixels */
.bc { position: absolute; font-family: Plex; color: #eceff5; font-size: 1px; }
.bc .slot { position: absolute; cursor: pointer; }
.bc .shadow { position: absolute; background-color: #00000038; }
.bc .outline { position: absolute; background-color: #000000b3; }
.bc .icon { position: absolute; background-color: #1b1d24; }
.bc .icon img { width: 100%; height: 100%; }
.bc .bad { position: absolute; border: 1.6rem #ff5a4ef2; }
.bc .hover { position: absolute; border: 1.5rem #ffffffd9; }
.bc .time { position: absolute; text-align: center; font-size: 11.5rem; font-weight: bold;
            font-effect: outline(2px #000000d9); }
.bc .time.warn { color: #f0a050; }
.bc .time.bad { color: #ff9a8f; }
.bc .name { position: absolute; text-align: right; font-size: 13rem; font-weight: bold; white-space: nowrap;
            font-effect: outline(2px #000000d9); }
.bc .name.bad { color: #ff9a8f; }
.bc .more { position: absolute; text-align: center; font-size: 12rem; font-weight: bold; cursor: pointer;
            font-effect: outline(2px #000000d9); }
.bc .toggle { position: absolute; background-color: #00000073; border: 1rem #ffffff47; cursor: pointer;
              transition: background-color 0.1s; }
.bc .toggle:hover { background-color: #0000009e; }
.bc .toggle.on { border-color: #e6cb8ad9; }
.bc .toggle .ln { position: absolute; left: 50%; margin-left: -5rem; width: 10rem; height: 1.5rem; border-radius: 1rem;
                  background-color: #ffffffe6; }
.bc .toggle.on .ln { background-color: #e6cb8ae6; }
.bc .tip { position: absolute; padding: 4rem 10rem 0 10rem; border-radius: 8rem; border: 1rem #ffffff1f;
           background-color: #10141cf0; white-space: nowrap; }
.bc .tip .a { display: block; font-size: 13rem; font-weight: bold; height: 18rem; line-height: 18rem; }
.bc .tip .b { display: block; font-size: 11.5rem; color: #9aa3b5; height: 15rem; line-height: 15rem; }
/* 3.0.18-beta.21: the hotbar (win/hotbar) - the concept's cells; places in the target's pixels */
.xb { position: absolute; font-family: Plex; color: #eceff5; }
.xg { position: absolute; font-family: Plex; }
.it { position: absolute; font-family: Plex; }
.hp { position: absolute; font-family: Plex; border-color: #ffffff17; }
.mp { position: absolute; font-family: Plex; border-color: #ffffff17; }
.tr { position: absolute; font-family: Plex; }
.mm { position: absolute; font-family: Plex; }
/* 3.0.18-beta.52 (R-W9WZ4G: the signs off the round buttons' middles): an edge inside the box, as roundRect's */
.mm div, .sh div, .ig div, .tr div, .hb div, .hp, .mp { box-sizing: border-box; }
.ig { position: absolute; font-family: Plex; }
.sh { position: absolute; font-family: Plex; }
.sh div, .sh span, .sh .a { position: absolute; }
.sh .a { left: 0; top: 0; }
.sh .clip { overflow: hidden; }
.sh .glass { border-color: #ffffff17; }
.sh .mark { display: none; left: 0; top: 0; width: 100%; height: 100%; border: 1.5dp #e6cb8ad9; border-radius: 12dp; }
.sh.marked .mark { display: block; }
.sh .hot { background-color: #ffffff1f; }
.sh .rule { background-color: #ffffff14; }
.sh .cell { background-color: #00000059; border-color: #ffffff14; }
.sh .cell.hot { border-color: #c9a55bf2; }
.sh .cell.late { border-color: #ff7d6ef2; }
.sh .plate { background-color: #000000b3; }
.sh .field { background-color: #00000059; border-color: #ffffff1a; }
.sh .field.typing { border-color: #c9a55b99; }
.sh .caret { background-color: #eef2f8f2; }
.sh .warn { background-color: #ff7d6e24; border-color: #ff7d6e80; }
.sh .ready { background-color: #5fd08a38; border-color: #5fd08ab3; }
.sh .dropping { background-color: #c9a55b0f; border-color: #f3c653e6; }
.sh .btn { background-color: #ffffff14; border-color: #ffffff24; }
.sh .btn.hot { background-color: #ffffff29; }
.sh .btn.off { border-color: #ffffff0d; }
.sh .btn.main { background-color: #f3c653d9; }
.sh .btn.main.hot { background-color: #f3c653f2; }
.sh .thumb { background-color: #ffffff40; }
.sh .tip { background-color: #10141cf5; border-color: #ffffff24; }
.sh .slothot { background-color: #ffffff0f; }
.sh .focus { background-color: #c9a55b14; border-color: #f3c653e6; }
.sh .fits { background-color: #c9a55b1f; border-color: #f3c653f2; }
.sh .tile { background-color: #1b1d24e6; border-color: #ffffff14; }
.sh .tile.round { background-color: #1b1d24; }
.sh .dot { background-color: #ffffff29; }
.sh .dot.card { background-color: #f3c653; }
.sh .picks { background-color: #1b1d24f2; border-color: #c9a55b80; }
.sh .pickhot { background-color: #ffffff12; }
.sh .word { white-space: nowrap; overflow: hidden; }
.sh .padfocus { position: absolute; border-color: #e6cb8a; background-color: #e6cb8a14; }  /* 3.0.18-beta.60 */
.motion.sh .padfocus { transition: left top width height 0.1s cubic-out; }
.sh .word.bold { font-weight: bold; }
.sh .word.right { text-align: right; }
.sh .word.centre { text-align: center; }
.sh .word.wrap { white-space: normal; }
.sh .word.halo { font-effect: outline(2px #000000d9); }
.ig div, .ig span, .ig .a { position: absolute; }
.ig .glass { border-color: #ffffff17; }
.ig .mark { display: none; position: absolute; left: 0; top: 0; width: 100%; height: 100%; border: 1.5dp #e6cb8ad9;
            border-radius: 12dp; }
.ig.marked .mark { display: block; }
.ig .hot { background-color: #ffffff1f; }
.ig .rule { background-color: #ffffff14; }
.ig .tab { background-color: #f3c653f2; }
.ig .field { background-color: #00000059; border-color: #ffffff1a; }
.ig .field.typing { border-color: #c9a55b99; }
.ig .caret { background-color: #eef2f8f2; }
.ig .cell { background-color: #00000059; border-color: #ffffff14; }
.ig .cell.hot { border-color: #c9a55bf2; }
.ig .plate { background-color: #000000b3; }
.ig .thumb { background-color: #ffffff40; }
.ig .bar { background-color: #ffffff1a; }
.ig .fill.green { background-color: #5fd08af2; }
.ig .fill.gold { background-color: #f3c653f2; }
.ig .fill.red { background-color: #ff7d6ef2; }
.ig .fresh { background-color: #f3c653f2; }
.ig .dropping { background-color: #c9a55b0f; border-color: #f3c653e6; }
.ig .flash { background-color: #f3c6534d; }
.ig .ring { border-color: #f3c653f2; }
.ig .tip { background-color: #10141cf5; border-color: #ffffff24; }
.ig .word { white-space: nowrap; overflow: hidden; }
.ig .word.bold { font-weight: bold; }
.ig .word.right { text-align: right; }
.ig .word.centre { text-align: center; }
.mm div, .mm span, .mm .a { position: absolute; }
.mm .edge { border-color: #ffffff29; }
.mm .gold { background-color: #e6cb8af2; }
.mm .btn { background-color: #0b0e14b8; border-color: #ffffff2e; }
.mm .btn.hot { background-color: #c9a55b8c; }
.mm .btn.scheme { background-color: #c9a55bbf; border-color: #e6cb8acc; }
.mm .card { border-color: #ffffff1f; }
.mm .card.list { border-color: #ffffff1a; }
.mm .card.route { border-width: 0; }
.mm .route { background-color: #c9a55b29; border-color: #c9a55b8c; }
.mm .hot { background-color: #ffffff1a; }
.mm .clear { background-color: #ffffff14; }
.mm .base { background-color: #ffffff1a; }
.mm .word { white-space: nowrap; overflow: hidden; }
.mm .word.bold { font-weight: bold; }
.mm .word.right { text-align: right; }
.mm .word.centre { text-align: center; }
.tr div, .tr span { position: absolute; }
.tr .glass { border-color: #ffffff17; }
.tr .rule { background-color: #ffffff14; }
.tr .hot { background-color: #ffffff1f; }
.tr .entry { background-color: #ffffff0a; }
.tr .entry.over { background-color: #ffffff13; }
.tr .more { background-color: #ffffff0f; }
.tr .word { white-space: nowrap; overflow: hidden; }
.tr .word.bold { font-weight: bold; }
.tr .word.right { text-align: right; }
.tr .word.centre { text-align: center; }
.mp .title, .mp .name, .mp .left, .mp .lit { position: absolute; white-space: nowrap; }
.mp .title { font-weight: bold; color: #e6cb8a; }
.mp .lit { background-color: #ffffff14; }
.mp .name { color: #9aa3b5; }
.mp .name.hot { color: #eef2f8; }
.mp .left { text-align: right; font-weight: bold; color: #eef2f8; }
.mp .left.back { color: #8fd19e; }
.hp .title, .hp .line, .hp .reset { position: absolute; white-space: nowrap; }
.hp .title { font-weight: bold; color: #e6cb8a; }
.hp .line { color: #9aa3b5; }
.hp .line.first { font-weight: bold; color: #eef2f8; }
.hp .reset { text-align: center; font-weight: bold; color: #eef2f8; background-color: #ffffff0f; }
.hp .reset.hot { background-color: #ffffff29; }
.it .toast { position: absolute; box-sizing: border-box; text-align: center; font-weight: bold; color: #f3c653;
             white-space: nowrap; background-color: #10141ceb; border-color: #f3c653b3; }
.xg .gain { position: absolute; width: 600dp; height: 22dp; line-height: 22dp; text-align: center; font-size: 15dp;
            font-weight: bold; color: #f3d27a; white-space: nowrap; font-effect: outline(2px #000000d9); }
.xb .band { position: absolute; left: 0; width: 100%; background-color: #0000008c; }
.xb .row { position: absolute; left: 0; width: 100%; }
.xb .fill { position: absolute; left: 0; top: 0; height: 100%; }
.xb .fill.base { background-color: #b7a6ff; }
.xb .fill.job { background-color: #7fd3c3; }
.xb .tick { position: absolute; top: 0; width: 1dp; height: 100%; background-color: #00000099; }
.xb .gain { position: absolute; padding: 0 6dp; height: 20dp; line-height: 20dp; font-size: 13dp; font-weight: bold;
            color: #f3d27a; white-space: nowrap; font-effect: outline(2px #000000d9); }
.xb .tip { position: absolute; box-sizing: border-box; padding: 4dp 10dp; background-color: #10141cf0;
           border: 1dp #ffffff1f; border-radius: 8dp; }
.xb .tip .line { display: block; height: 17dp; line-height: 17dp; font-size: 13dp; font-weight: bold; color: #9aa3b5;
                 white-space: nowrap; }
.xb .tip .line:first-child { color: #eceff5; }
.hb { position: absolute; font-family: Plex; color: #eceff5; font-size: 1px; }
.hb .a { position: absolute; }
.hb .glass { position: absolute; border: 1rem #ffffff17; background-color: #10141ccc; }
.hb .cell { position: absolute; background-color: #ffffff0d; border: 1rem #ffffff1a; }
.hb .cell.hot { background-color: #ffffff1a; border-color: #ffffff33; }
.hb .cell.down { background-color: #0000004d; }
.hb .cell.lit { border-color: #c9a55bd9; border-width: 2rem; }
.hb .cell.chosen, .hb .cell.under { border-color: #c9a55b; border-width: 2rem; }
.hb .cell.combo { background-color: #e6cb8a1f; border-color: #c9a55b; border-width: 2rem; }
.hb .cell.ready { background-color: #4da3ff24; border-color: #4da3fff2; border-width: 2rem; }
.hb .flash { position: absolute; border: 2rem #f0a050; }
.hb .shade { position: absolute; background-color: #00000080; }
.hb .plate { position: absolute; background-color: #0000009e; text-align: center; font-size: 11rem; font-weight: bold;
             white-space: nowrap; }
.hb .plate.sp { background-color: #3d7eebf2; color: #ffffff; }
.hb .plate.lack { background-color: #c8443af2; color: #ffffff; }
.hb .plate.timer { background-color: #000000b8; font-size: 12rem; color: #c9a55b; }
.hb .bar { position: absolute; height: 3rem; }
.hb .bar.combo { background-color: #e6cb8a; }
.hb .bar.timer { background-color: #c9a55b; }
.hb .chip { position: absolute; background-color: #c0392be6; text-align: center; font-size: 12rem; font-weight: bold; }
.hb .hint { position: absolute; font-size: 11.5rem; color: #9aa3b5; white-space: nowrap; }
.hb .rowbox { position: absolute; background-color: #ffffff0a; }
.hb .rownum { position: absolute; text-align: center; font-size: 15rem; font-weight: bold; }
.hb .f12 { position: absolute; text-align: center; font-size: 10.5rem; color: #9aa3b5; }
.hb .label { position: absolute; text-align: center; font-size: 13rem; font-weight: bold; }
.hb .toggle { position: absolute; background-color: #ffffff0f; border: 1rem #c9a55b00; }
.hb .toggle.hot { background-color: #ffffff24; border-color: #c9a55bcc; }
.hb .toggle.battle { background-color: #c0392be6; }
.hb .toggle.battle.hot { background-color: #c0392b; }
.hb .profile { position: absolute; text-align: center; font-size: 10rem; font-weight: bold; color: #e6cb8a; }
.hb .pill { position: absolute; background-color: #10141cd9; border: 1rem #ffffff1f; }
.hb .pilltext { position: absolute; font-size: 12.5rem; font-weight: bold; white-space: nowrap; }
.hb .dot { position: absolute; background-color: #ffffff14; border: 1rem #ffffff4d; }
.hb .dot.on { background-color: #e6cb8a; border-width: 0; }
.hb .tip { position: absolute; border: 1rem #ffffff1f; background-color: #10141cf0; }
.hb .tipline { position: absolute; font-size: 11.5rem; white-space: nowrap; }
.hb .tipline.first { font-size: 13rem; font-weight: bold; }
.hb .ttip { position: absolute; border: 1rem #ffffff1f; background-color: #10141cf0; text-align: center;
            font-size: 12rem; white-space: nowrap; }
/* 3.0.18-beta.61: [interface] uaui_effects - gradients, soft shadows, glows (the body's class «fx»); off: none */
.fx.sh .glass, .fx.ig .glass, .fx.hb .glass, .fx .ua-window { decorator: ua-shadow(16dp #00000059); }
.fx.sh .padfocus { decorator: ua-shadow(8dp #e6cb8a59); }
.fx.hb .cell.ready { decorator: ua-shadow(6dp #4da3ff59); }
.fx.hb .cell.lit, .fx.hb .cell.combo { decorator: ua-shadow(6dp #e6cb8a4d); }
.fx.cb .hp, .fx.cb .sp, .fx.tc .hpfill, .fx.xb .fill, .fx.ig .fill, .fx .ua-fill {
    decorator: linear-gradient(180deg, #ffffff38 0%, #ffffff00 55%, #00000026 100%); }
.fx.sh .btn, .fx.ig .tab { decorator: linear-gradient(180deg, #ffffff14 0%, #ffffff00 100%); }
/* 3.0.18-beta.64: [interface] uaui_glass - the windows' frosted glass (the body's class «glassy», Direct3D 12) */
.glassy.win.sh .glass, .glassy.win.ig .glass, .glassy .ua-window { decorator: ua-glass(1); }
.fx.glassy.win.sh .glass, .fx.glassy.win.ig .glass, .fx.glassy .ua-window {
    decorator: ua-shadow(16dp #00000059), ua-glass(1); }
/* 3.0.18-beta.58: [interface] uaui_motion - the soft motion (the body's class «motion»); off: none of it */
.motion.win { transition: transform 0.16s cubic-out; }
.motion.win.away { transform: translateY(8dp); }
.motion.it { transition: opacity transform 0.2s cubic-out; }
.motion.it.away { opacity: 0; transform: translateY(-10dp); }
.motion.hb .cell, .motion.ig .cell, .motion.ig .hot, .motion.sh .cell, .motion.sh .btn, .motion.sh .tile,
.motion.sh .hot { transition: background-color border-color 0.12s cubic-out; }
.motion.hb .a { transition: transform 0.09s cubic-out; }
.motion.hb .a.hot { transform: scale(1.05); }
.motion.hb .a.down { transform: scale(0.92); }
@keyframes ua-pulse { from { opacity: 1; } to { opacity: 0.55; } }
.motion.cb .hp.low, .motion.tc .hpfill.low { animation: 0.7s sine-in-out infinite alternate ua-pulse; }
@keyframes ua-rise { from { opacity: 0; transform: translateY(10dp); } to { opacity: 1; transform: translateY(0dp); } }
.motion.xg .gain, .motion.xb .gain { animation: 0.35s cubic-out ua-rise; }
scrollbarvertical { width: 8dp; margin-left: 4dp; }
scrollbarvertical slidertrack { background-color: #ffffff0c; border-radius: 4dp; }
scrollbarvertical sliderbar { background-color: #ffffff40; border-radius: 4dp; min-height: 24dp; }
scrollbarvertical sliderbar:hover { background-color: #ffffff70; }
scrollbarvertical sliderarrowdec, scrollbarvertical sliderarrowinc { width: 0; height: 0; }
)";

// The window: its title (dragged by RmlUi's handle), a close button, the content, the controls' hint line
const char* kWindowTemplate = R"(<template name="ua-window" content="content">
<head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="ua-window away">
<handle move_target="#document" class="ua-title"><span class="ua-title-name" id="ua-title"></span></handle>
<div class="ua-close" id="ua-close">×</div>
<div class="ua-content" id="content"></div>
<div class="ua-hint" id="ua-hint"></div>
</body></template>)";


std::string themeFile() { return layout::join(gameDir(), layout::kUiTheme); }

// 3.0.18-beta.59: what ui\builtin is for (UTF-8, as the samples)
const char* kSamplesReadme =
    "uaRO Graphics - the new interface's markup (uaUI, RmlUi)\r\n\r\n"
    "UK: Тут лежить вбудована розмітка кожної частини нового інтерфейсу (<частина>.rml) і його стиль (theme.rcss).\r\n"
    "Щоб змінити частину, скопіюйте її файл на рівень вище (uaro-graphics\\ui\\<частина>.rml) і правте копію - гра\r\n"
    "підхопить зміни за секунду. Видаліть копію - повернеться вбудована. Свої правила стилю пишіть у\r\n"
    "uaro-graphics\\ui\\theme.rcss (вони йдуть поверх вбудованих). Файли тут переписуються з кожною версією.\r\n\r\n"
    "RU: Здесь встроенная разметка каждой части нового интерфейса (<часть>.rml) и его стиль (theme.rcss).\r\n"
    "Чтобы изменить часть, скопируйте её файл уровнем выше (uaro-graphics\\ui\\<часть>.rml) и правьте копию - игра\r\n"
    "подхватит изменения за секунду. Удалите копию - вернётся встроенная. Свои правила стиля пишите в\r\n"
    "uaro-graphics\\ui\\theme.rcss (они идут поверх встроенных). Файлы здесь переписываются с каждой версией.\r\n\r\n"
    "EN: The built-in markup of every new interface part (<part>.rml) and its style (theme.rcss). To change a part,\r\n"
    "copy its file one level up (uaro-graphics\\ui\\<part>.rml) and edit the copy - the game picks it up within a\r\n"
    "second; delete the copy for the built-in one. Your own style rules go to uaro-graphics\\ui\\theme.rcss.\r\n";

// 3.0.18-beta.59: a small text file whole ("" none or over 1 MB); `there` whether it is
std::string readSmall(const std::string& path, bool* there = nullptr) {
    std::string text;
    if (there) *there = false;
    HANDLE h = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0,
                           nullptr);
    if (h == INVALID_HANDLE_VALUE) return text;
    LARGE_INTEGER size{};
    if (GetFileSizeEx(h, &size) && size.QuadPart >= 0 && size.QuadPart < (1 << 20)) {
        text.assign(size_t(size.QuadPart), '\0');
        DWORD got = 0;
        if (!ReadFile(h, text.data(), DWORD(text.size()), &got, nullptr) || got != text.size()) text.clear();
        else if (there) *there = true;
    }
    CloseHandle(h);
    return text;
}
// 3.0.18-beta.59 (the possibilities' item 12): uaro-graphics\ui - a part's own markup over the built-in one
std::string uiDir() { return layout::join(gameDir(), "uaro-graphics\\ui"); }
std::string safeName(const std::string& name) {
    std::string out;
    for (char c : name) out += std::isalnum(static_cast<unsigned char>(c)) || c == '-' ? c : '_';
    return out;
}
// The built-in markup as a sample next to it (ui\builtin\<part>.rml), written when missing or other
void writeSample(const std::string& file, const std::string& text) {
    CreateDirectoryA(uiDir().c_str(), nullptr);
    const std::string dir = layout::join(uiDir(), "builtin");
    CreateDirectoryA(dir.c_str(), nullptr);
    const std::string path = layout::join(dir, file);
    if (readSmall(path) == text) return;
    HANDLE h = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD put = 0;
    WriteFile(h, text.data(), DWORD(text.size()), &put, nullptr);
    CloseHandle(h);
}

// RmlUi's files: «uaui:» ours from memory (the theme from its file when there), else none
class Files final : public Rml::FileInterface {
public:
    Rml::FileHandle Open(const Rml::String& path) override {
        auto* f = new Open_;
        if (path == "uaui:theme.rcss") {  // 3.0.18-beta.17: the file's rules after the built-in ones (over them)
            f->data = uitheme::themedCss(state().theme, kTheme);  // 3.0.18-beta.54: in the theme chosen
            State& s = state();
            s.themeFile = false;
            HANDLE h = CreateFileA(themeFile().c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                   OPEN_EXISTING, 0, nullptr);
            if (h != INVALID_HANDLE_VALUE) {
                LARGE_INTEGER size{};
                if (GetFileSizeEx(h, &size) && size.QuadPart > 0 && size.QuadPart < (1 << 20)) {
                    std::string text(size_t(size.QuadPart), '\0');
                    DWORD got = 0;
                    if (ReadFile(h, text.data(), DWORD(text.size()), &got, nullptr) && got == text.size()) {
                        f->data += "\n" + text;
                        s.themeFile = true;
                    }
                }
                CloseHandle(h);
            }
        } else if (path == "uaui:window.rml") {
            f->data = kWindowTemplate;
        } else {
            delete f;
            return 0;
        }
        return Rml::FileHandle(f);
    }
    void Close(Rml::FileHandle file) override { delete reinterpret_cast<Open_*>(file); }
    size_t Read(void* buffer, size_t size, Rml::FileHandle file) override {
        auto* f = reinterpret_cast<Open_*>(file);
        const size_t n = std::min(size, f->data.size() - f->at);
        std::copy(f->data.data() + f->at, f->data.data() + f->at + n, static_cast<char*>(buffer));
        f->at += n;
        return n;
    }
    bool Seek(Rml::FileHandle file, long offset, int origin) override {
        auto* f = reinterpret_cast<Open_*>(file);
        const long base = origin == SEEK_SET ? 0 : origin == SEEK_CUR ? long(f->at) : long(f->data.size());
        const long at = base + offset;
        if (at < 0 || size_t(at) > f->data.size()) return false;
        f->at = size_t(at);
        return true;
    }
    size_t Tell(Rml::FileHandle file) override { return reinterpret_cast<Open_*>(file)->at; }

private:
    struct Open_ {
        std::string data;
        size_t at = 0;
    };
};
Files g_files;

// 3.0.18-beta.17: <ua-ring value="0.7" width="5" fill="#5bd36cff" track="#ffffff1f" start="-90"> - an arc round the
// middle of the element's box, `width` dp wide at its edge, `value` of the circle from `start` degrees clockwise
// (with the track's colour for the rest); sector="1" a pie to the middle (a cooldown's sweep). Its colours at the
// element's opacity. The rings of the character block, the hotbar's and the buffs' sweeps.
bool hexColour(const Rml::String& s, Rml::Colourb& out) {
    if (s.size() != 7 && s.size() != 9) return false;
    if (s[0] != '#') return false;
    char* end = nullptr;
    const unsigned long v = std::strtoul(s.c_str() + 1, &end, 16);
    if (!end || *end) return false;
    if (s.size() == 7) out = Rml::Colourb(Rml::byte(v >> 16), Rml::byte(v >> 8), Rml::byte(v), 255);
    else out = Rml::Colourb(Rml::byte(v >> 24), Rml::byte(v >> 16), Rml::byte(v >> 8), Rml::byte(v));
    return true;
}

class ElementRing final : public Rml::Element {
public:
    explicit ElementRing(const Rml::String& tag) : Rml::Element(tag) {}

protected:
    void OnAttributeChange(const Rml::ElementAttributes& changed) override {
        Rml::Element::OnAttributeChange(changed);
        dirty_ = true;
    }
    void OnResize() override { dirty_ = true; }
    void OnRender() override {
        const float opacity = GetComputedValues().opacity();
        if (dirty_ || opacity != opacity_) build(opacity);
        geometry_.Render(GetAbsoluteOffset(Rml::BoxArea::Border));
    }

private:
    void build(float opacity) {
        dirty_ = false;
        opacity_ = opacity;
        Rml::RenderManager* rm = GetRenderManager();
        if (!rm) return;
        const Rml::Vector2f size = GetBox().GetSize(Rml::BoxArea::Border);
        const float r = std::min(size.x, size.y) / 2;
        if (r <= 1) {
            geometry_ = {};
            return;
        }
        const float dpr = GetContext() ? GetContext()->GetDensityIndependentPixelRatio() : 1.0f;
        const bool sector = GetAttribute<int>("sector", 0) != 0;
        const bool square = sector && GetAttribute<int>("square", 0) != 0;  // 3.0.18-beta.20: a pie over the whole box
        const float w = sector ? r : std::min(r, GetAttribute<float>("width", 4.0f) * dpr);
        const float value = std::clamp(GetAttribute<float>("value", 1.0f), 0.0f, 1.0f);
        const float start = GetAttribute<float>("start", -90.0f) * 3.14159265f / 180;
        Rml::Colourb fill(255, 255, 255, 255), track(0, 0, 0, 0);
        hexColour(GetAttribute<Rml::String>("fill", ""), fill);
        hexColour(GetAttribute<Rml::String>("track", ""), track);
        Rml::Mesh mesh;
        const Rml::Vector2f c(size.x / 2, size.y / 2);
        const auto arc = [&](float from, float to, Rml::Colourb col) {
            if (to <= from || col.alpha == 0) return;
            const Rml::ColourbPremultiplied in = col.ToPremultiplied(opacity), out = col.ToPremultiplied(0.0f);
            const int n = std::max(2, int(std::ceil((to - from) / (2 * 3.14159265f) * std::clamp(r, 24.0f, 160.0f))));
            float ro = r - 0.5f;
            const float ri = std::max(0.0f, r - w + 0.5f);
            for (int i = 0; i <= n; ++i) {
                const float a = from + (to - from) * float(i) / float(n), ca = std::cos(a), sa = std::sin(a);
                if (square)  // as far as the box's edge that way
                    ro = std::min(std::fabs(ca) > 1e-4f ? size.x / 2 / std::fabs(ca) : 1e9f,
                                  std::fabs(sa) > 1e-4f ? size.y / 2 / std::fabs(sa) : 1e9f) - 1;
                const int b = int(mesh.vertices.size());
                // the fringe out, the band, the fringe in (none for a pie)
                mesh.vertices.push_back({{c.x + (ro + 1) * ca, c.y + (ro + 1) * sa}, out, {0, 0}});
                mesh.vertices.push_back({{c.x + ro * ca, c.y + ro * sa}, in, {0, 0}});
                mesh.vertices.push_back({{c.x + ri * ca, c.y + ri * sa}, in, {0, 0}});
                mesh.vertices.push_back({{c.x + std::max(0.0f, ri - 1) * ca, c.y + std::max(0.0f, ri - 1) * sa},
                                         sector ? in : out, {0, 0}});
                if (i == 0) continue;
                for (int k = 0; k < 3; ++k)
                    for (int q : {b - 4 + k, b + k, b - 3 + k, b - 3 + k, b + k, b + 1 + k}) mesh.indices.push_back(q);
            }
        };
        const float turn = 2 * 3.14159265f;
        arc(start + turn * value, start + turn, track);
        arc(start, start + turn * value, fill);
        geometry_ = rm->MakeGeometry(std::move(mesh));
    }
    Rml::Geometry geometry_;
    bool dirty_ = true;
    float opacity_ = -1;
};
Rml::ElementInstancerGeneric<ElementRing> g_ringInstancer;

// 3.0.18-beta.27: where the windows' documents begin - the batches rendered so far are the parts'
class ElementSplit final : public Rml::Element {
public:
    explicit ElementSplit(const Rml::String& tag) : Rml::Element(tag) {}

protected:
    void OnRender() override;
};
Rml::ElementInstancerGeneric<ElementSplit> g_splitInstancer;
void ElementSplit::OnRender() {
    State& s = state();
    if (GetAttribute<int>("at", 2) == 1) {  // 3.0.18-beta.38: the world's end
        s.worldSplit = s.batches.size();
        s.worldSeen = true;
        return;
    }
    s.split = s.batches.size();
    s.splitSeen = true;
}

// The window template's ×: its document hidden, the part told
class Close final : public Rml::EventListener {
public:
    void ProcessEvent(Rml::Event& e) override {
        Rml::ElementDocument* d = e.GetCurrentElement()->GetOwnerDocument();
        for (Doc* doc : state().docs)
            if (doc->doc() == d) {
                doc->show(false);
                if (doc->onClose) doc->onClose();
            }
    }
};
Close g_close;

bool themeChanged(State& s) {
    WIN32_FILE_ATTRIBUTE_DATA a{};
    const bool there = GetFileAttributesExA(themeFile().c_str(), GetFileExInfoStandard, &a) != 0;
    const FILETIME t = there ? a.ftLastWriteTime : FILETIME{};
    const bool changed = there != s.themeFile || CompareFileTime(&t, &s.themeStamp) != 0;
    s.themeStamp = t;
    return changed;
}

bool start(State& s) {
    if (s.started) return true;
    if (s.failed) return false;
    s.dc = CreateCompatibleDC(nullptr);
    if (!s.dc || !ensureBits(s, 64, 64)) {
        s.failed = true;
        LOG_ERROR("uaui: no drawing context for the words - uaUI stays off");
        return false;
    }
    Rml::SetSystemInterface(&g_system);
    Rml::SetRenderInterface(&g_render);
    static Fonts fonts;
    g_fonts = &fonts;
    Rml::SetFontEngineInterface(g_fonts);
    Rml::SetFileInterface(&g_files);
    if (!Rml::Initialise()) {
        s.failed = true;
        LOG_ERROR("uaui: RmlUi did not start - uaUI stays off");
        return false;
    }
    s.context = Rml::CreateContext("uaui", {std::max(s.targetW, 1), std::max(s.targetH, 1)});
    if (!s.context) {
        s.failed = true;
        LOG_ERROR("uaui: RmlUi made no context - uaUI stays off");
        return false;
    }
    Rml::Factory::RegisterElementInstancer("ua-ring", &g_ringInstancer);  // 3.0.18-beta.17
    static ShadowInstancer shadow;  // 3.0.18-beta.61
    g_shadow = &shadow;
    Rml::Factory::RegisterDecoratorInstancer("ua-shadow", g_shadow);
    static GlassInstancer glass;  // 3.0.18-beta.64
    Rml::Factory::RegisterDecoratorInstancer("ua-glass", &glass);
    Rml::Factory::RegisterElementInstancer("ua-split", &g_splitInstancer);  // 3.0.18-beta.27
    // the numbers as the interface writes them - «5 157» (core/char_block grouped); for every model of the context
    Rml::DataModelConstructor m = s.context->CreateDataModel("ua");
    m.RegisterTransformFunc("num", [](const Rml::VariantList& a) -> Rml::Variant {
        return a.empty() ? Rml::Variant() : Rml::Variant(charblock::grouped(a[0].Get<int64_t>()));
    });
    themeChanged(s);  // its stamp now: read again only when it changes
    s.marker = s.context->LoadDocumentFromMemory(
        "<rml><body style=\"position: absolute; left: 0; top: 0; width: 1px; height: 1px; z-index: 5;\">"
        "<ua-split style=\"display: block; width: 1px; height: 1px;\"/></body></rml>",
        "uaui://split");
    if (s.marker) s.marker->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
    s.worldMarker = s.context->LoadDocumentFromMemory(
        "<rml><body style=\"position: absolute; left: 0; top: 0; width: 1px; height: 1px; z-index: 2;\">"
        "<ua-split at=\"1\" style=\"display: block; width: 1px; height: 1px;\"/></body></rml>",
        "uaui://split-world");
    if (s.worldMarker) s.worldMarker->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
    s.started = true;
    LOG_INFO("uaui: RmlUi %s started - its context %dx%d, %.2f target pixels a concept pixel; the theme %s",
             Rml::GetVersion().c_str(), s.targetW, s.targetH, double(uistyle::partScale(s.k)),
             s.themeFile ? themeFile().c_str() : "built in");
    return true;
}

// The document of ours under the game's cursor, its box: the parts' blocker (win/world_mouse) - the wheel, the clicks
// and the world's hover stay the window's
Doc* docUnder(State& s) {
    Rml::Element* hover = s.context ? s.context->GetHoverElement() : nullptr;
    const Rml::ElementDocument* owner = hover ? hover->GetOwnerDocument() : nullptr;
    if (!owner) return nullptr;
    for (Doc* d : s.docs)
        if (d->shown() && !d->passive && d->doc() == owner) return d;
    return nullptr;
}

}  // namespace

namespace {

// ---- 3.0.18-beta.12: the recorded pictures on the card ----

constexpr int kWords = 2048;  // the words' atlas, one texture

uint32_t premul(uint32_t rgb, float a) {
    const float k = std::clamp(a, 0.0f, 1.0f);
    const auto ch = [&](int sh) { return uint32_t(std::lround(float((rgb >> sh) & 0xFF) * k)); };
    return uint32_t(std::lround(k * 255)) << 24 | ch(16) << 16 | ch(8) << 8 | ch(0);
}


// A word's slot in the atlas (its halo's beside it, grown by 2): false when it has no room even in an empty one
bool wordSlot(State& s, const ::uagfx::uiscene::Prim& w, bool halo, State::Slot& out) {
    const std::string key = halo ? w.key + "#h" : w.key;
    const auto it = s.words.find(key);
    if (it != s.words.end()) {
        out = it->second;
        return true;
    }
    const int cw = int(w.w), ch = int(w.h), pad = halo ? 2 : 0, sw = cw + 2 * pad + 1, sh = ch + 2 * pad + 1;
    if (sw > kWords || sh > kWords || w.cover.size() < size_t(cw) * size_t(ch)) return false;
    if (s.wordsPx.empty()) s.wordsPx.assign(size_t(kWords) * kWords, 0u);
    if (s.wordsX + sw > kWords) s.wordsX = 0, s.wordsY += s.wordsRow, s.wordsRow = 0;
    if (s.wordsY + sh > kWords) {  // full: all of it anew (the scenes' meshes made again by the epoch)
        std::fill(s.wordsPx.begin(), s.wordsPx.end(), 0u);
        s.words.clear();
        s.wordsX = s.wordsY = s.wordsRow = 0;
        ++s.wordsEpoch;
    }
    const State::Slot slot{s.wordsX, s.wordsY};
    const auto cover = [&](int x, int y) -> int {
        return x < 0 || y < 0 || x >= cw || y >= ch ? 0 : w.cover[size_t(y) * size_t(cw) + size_t(x)];
    };
    for (int y = 0; y < sh - 1; ++y)
        for (int x = 0; x < sw - 1; ++x) {
            int v = 0;
            if (!halo) {
                v = cover(x, y);
            } else {  // as win/picture_draw: the coverage grown by 2 pixels (a disc of 5)
                for (int dy = -2; dy <= 2 && v < 255; ++dy)
                    for (int dx = -2; dx <= 2; ++dx)
                        if (dx * dx + dy * dy <= 5) v = std::max(v, cover(x - pad + dx, y - pad + dy));
            }
            const uint32_t c = uint32_t(v);
            s.wordsPx[size_t(slot.y + y) * kWords + size_t(slot.x + x)] = c << 24 | c << 16 | c << 8 | c;
        }
    s.wordsX += sw;
    s.wordsRow = std::max(s.wordsRow, sh);
    s.wordsDirty = true;
    s.words.emplace(key, slot);
    out = slot;
    return true;
}

// An icon of uiicons drawn once at its size, colour and place's fraction - its texture
const State::Tex* iconTex(State& s, gpu::Renderer& r, FrameRecord& record, const ::uagfx::uiscene::Prim& p,
                          float& ox, float& oy) {
    const float fx = std::floor(p.x), fy = std::floor(p.y);
    const int qx = int(std::lround((p.x - fx) * 4)), qy = int(std::lround((p.y - fy) * 4));
    char key[96];
    std::snprintf(key, sizeof key, "%d|%.2f|%06x|%.2f|%d|%d", p.icon, double(p.w), unsigned(p.fill),
                  double(p.stroke), qx, qy);
    auto it = s.icons.find(key);
    if (it == s.icons.end()) {
        panel::Picture pic;  // painted, not recorded (its pixels its own)
        pic.width = int(std::ceil(p.w)) + 3;
        pic.height = pic.width;
        pic.px.assign(size_t(pic.width) * size_t(pic.height), 0u);
        uiicons::draw(pic, uiicons::Icon(p.icon), 1 + float(qx) / 4, 1 + float(qy) / 4, p.w, p.fill, 1.0f, p.stroke);
        State::Tex t;
        t.w = pic.width, t.h = pic.height;
        t.id = r.createTexture(t.w, t.h, 1);
        if (!t.id) return nullptr;
        record.upload(t.id, 0, Rect{0, 0, t.w, t.h}, pic.px.data());
        it = s.icons.emplace(key, t).first;
    }
    it->second.used = GetTickCount64();
    ox = fx - 1, oy = fy - 1;
    return &it->second;
}

// A recorded picture (an icon, a portrait) as a texture - one for each content (its pixels' sum)
const State::Tex* imageTex(State& s, gpu::Renderer& r, FrameRecord& record, const ::uagfx::uiscene::Prim& p) {
    if (!p.pixels || p.iw <= 0 || p.ih <= 0 || p.pixels->size() < size_t(p.iw) * size_t(p.ih)) return nullptr;
    auto it = s.images.find(p.sum);
    if (it == s.images.end()) {
        State::Tex t;
        t.w = p.iw, t.h = p.ih;
        t.id = r.createTexture(t.w, t.h, 1);
        if (!t.id) return nullptr;
        record.upload(t.id, 0, Rect{0, 0, t.w, t.h}, p.pixels->data());
        it = s.images.emplace(p.sum, t).first;
    }
    it->second.used = GetTickCount64();
    return &it->second;
}

using ::uagfx::uaui::Point;

void quad(std::vector<Point>& pts, std::vector<int>& ix, float x, float y, float w, float h, uint32_t c, float u0,
          float v0, float u1, float v1) {
    const int b = int(pts.size());
    pts.push_back({x, y, c, u0, v0});
    pts.push_back({x + w, y, c, u1, v0});
    pts.push_back({x, y + h, c, u0, v1});
    pts.push_back({x + w, y + h, c, u1, v1});
    for (int q : {0, 1, 2, 1, 3, 2}) ix.push_back(b + q);
}

// A picture within the circle of radius r round its box's middle: a disc of its texture, its border a pixel smooth
void disc(std::vector<Point>& pts, std::vector<int>& ix, float x, float y, float w, float h, float r, uint32_t c) {
    const float cx = x + w / 2, cy = y + h / 2;
    const int n = std::clamp(int(r), 16, 96);
    const auto uv = [&](float px, float py, float a) {
        return Point{px, py, a >= 1 ? c : premul(0, 0), (px - x) / w, (py - y) / h};
    };
    const int center = int(pts.size());
    pts.push_back(uv(cx, cy, 1));
    for (int i = 0; i < n; ++i) {
        const float a = 6.2831853f * float(i) / float(n);
        pts.push_back(uv(cx + (r - 0.5f) * std::cos(a), cy + (r - 0.5f) * std::sin(a), 1));
        pts.push_back(uv(cx + (r + 0.5f) * std::cos(a), cy + (r + 0.5f) * std::sin(a), 0));
    }
    for (int i = 0; i < n; ++i) {
        const int a0 = center + 1 + 2 * i, a1 = center + 1 + 2 * ((i + 1) % n);
        for (int q : {center, a0, a1, a0, a0 + 1, a1, a1, a0 + 1, a1 + 1}) ix.push_back(q);
    }
}

}  // namespace

bool sceneReady() {
    const State& s = state();
    return s.started && s.on;
}

bool residual(const panel::Picture& pic) {
    State& s = state();
    if (!pic.scene) return true;
    State::Cached& c = s.scenes[pic.scene];
    if (c.generation != pic.scene->generation) {  // its pixels looked at once a generation
        c.residual = std::any_of(pic.px.begin(), pic.px.end(), [](uint32_t v) { return (v >> 24) != 0; });
        c.generation = pic.scene->generation;
        c.opacity = -1;  // its meshes made again
    }
    return c.residual;
}

void drawScene(FrameRecord& record, gpu::Renderer& renderer, const panel::Picture& pic, int x, int y, int w, int h,
               float k, float opacity, int stretch) {
    State& s = state();
    if (!pic.scene || pic.width <= 0 || pic.height <= 0 || k <= 0) return;
    residual(pic);  // its generation noted
    State::Cached& c = s.scenes[pic.scene];
    const ULONGLONG nowMs = GetTickCount64();
    c.used = nowMs;
    // 3.0.18-beta.13: a window or a card coming into sight - from 96 % of its size and nothing, 120 ms (the theme's
    // motion; [interface] animations); the small and the screen-wide ones as they are
    float grow = 1, fadeIn = 1;
    State::Seen& seen = s.seen[&pic];
    if (nowMs - seen.last > 250) seen.since = nowMs;
    seen.last = nowMs;
    if (motion::on() && pic.width >= 140 && pic.height >= 60 && w < s.targetW * 9 / 10) {
        const float t = std::min(1.0f, float(nowMs - seen.since) / float(motion::kCardMs));
        const float e = 1 - (1 - t) * (1 - t);  // eased out
        grow = 0.96f + 0.04f * e;
        fadeIn = e;
    }
    const float alpha = opacity * pic.scene->opacity * fadeIn;
    if (c.opacity != alpha || c.x != x || c.y != y || c.w != w || c.h != h || c.k != k || c.epoch != s.wordsEpoch ||
        c.theme != s.themeSerial || c.grow != grow) {
        c.x = x, c.y = y, c.w = w, c.h = h, c.k = k, c.opacity = alpha, c.theme = s.themeSerial, c.grow = grow;
        c.runs.clear();
        const float sx = float(w) / float(pic.width), sy = float(h) / float(pic.height);
        const float half = 0.5f / float(std::max(stretch, 1));
        std::vector<Point> pts;
        std::vector<int> ix;
        std::vector<glint::Vertex> shape;
        uint32_t runTex = 0;
        bool haveRun = false;
        const auto emit = [&](uint32_t tex, const ::uagfx::uiscene::Prim& p) {
            if (!haveRun || tex != runTex) c.runs.emplace_back(tex, std::vector<glint::Vertex>()), runTex = tex;
            haveRun = true;
            const float cx = float(w) / 2, cy = float(h) / 2;
            for (Point& q : pts) q.x = (q.x * sx - cx) * grow + cx, q.y = (q.y * sy - cy) * grow + cy;
            ::uagfx::uaui::ClipBox clip{float(p.cx0) * sx, float(p.cy0) * sy, float(p.cx1) * sx, float(p.cy1) * sy, {}};
            ::uagfx::uaui::triangles(pts, ix, float(x), float(y), nullptr, p.clipped ? &clip : nullptr, k, half,
                                     c.runs.back().second);
            pts.clear();
            ix.clear();
        };
        for (const ::uagfx::uiscene::Prim& p : pic.scene->prims) {
            switch (p.kind) {
                case ::uagfx::uiscene::Kind::Rect: {
                    shape.clear();
                    ::uagfx::uiscene::Prim themed = p;  // 3.0.18-beta.13: in the theme's colours and corners
                    themed.fill = uitheme::colour(s.theme, p.fill);
                    themed.edge = uitheme::colour(s.theme, p.edge);
                    if (p.r < std::min(p.w, p.h) / 2 - 0.01f) themed.r = p.r * s.theme.radius;  // a circle stays one
                    themed.edgeW = p.edgeW * s.theme.edge;
                    ::uagfx::uiscene::mesh(themed, 0, 0, 1.0f, 0.0f, alpha, shape);
                    for (const glint::Vertex& v : shape) {
                        ix.push_back(int(pts.size()));
                        pts.push_back({v.x, v.y, v.diffuse, 0, 0});
                    }
                    emit(0, p);
                    break;
                }
                case ::uagfx::uiscene::Kind::Icon: {
                    float ox = 0, oy = 0;
                    ::uagfx::uiscene::Prim themed = p;
                    themed.fill = uitheme::colour(s.theme, p.fill);
                    const State::Tex* t = iconTex(s, renderer, record, themed, ox, oy);
                    if (!t) break;
                    const uint32_t col = premul(0xFFFFFF, p.opacity * alpha);
                    quad(pts, ix, ox, oy, float(t->w), float(t->h), col, 0, 0, 1, 1);
                    emit(t->id, p);
                    break;
                }
                case ::uagfx::uiscene::Kind::Image: {
                    const State::Tex* t = imageTex(s, renderer, record, p);
                    if (!t) break;
                    const uint32_t col = premul(0xFFFFFF, p.opacity * alpha);
                    if (p.r > 0) disc(pts, ix, p.x, p.y, p.w, p.h, p.r, col);
                    else quad(pts, ix, p.x, p.y, p.w, p.h, col, 0, 0, 1, 1);
                    emit(t->id, p);
                    break;
                }
                case ::uagfx::uiscene::Kind::Words: {
                    State::Slot slot;
                    const float inv = 1.0f / float(kWords);
                    if (p.halo && wordSlot(s, p, true, slot))
                        quad(pts, ix, p.x - 2, p.y - 2, p.w + 4, p.h + 4, premul(0x000000, 0.85f * alpha * p.opacity),
                             float(slot.x) * inv, float(slot.y) * inv, float(slot.x + p.w + 4) * inv,
                             float(slot.y + p.h + 4) * inv);
                    if (wordSlot(s, p, false, slot))
                        quad(pts, ix, p.x, p.y, p.w, p.h, premul(uitheme::colour(s.theme, p.fill), alpha * p.opacity),
                             float(slot.x) * inv,
                             float(slot.y) * inv, float(slot.x + p.w) * inv, float(slot.y + p.h) * inv);
                    if (!s.wordsId) s.wordsId = renderer.createTexture(kWords, kWords, 1);
                    emit(s.wordsId, p);
                    break;
                }
            }
        }
        c.epoch = s.wordsEpoch;
    }
    if (s.wordsDirty && s.wordsId) {  // the new words up before they are drawn
        record.upload(s.wordsId, 0, Rect{0, 0, kWords, kWords}, s.wordsPx.data());
        s.wordsDirty = false;
    }
    for (const auto& [tex, verts] : c.runs) {
        if (verts.empty()) continue;
        DrawSetup st;  // premultiplied over what is there, no depth (as win/picture_draw's)
        st.ui = true;
        st.blend = true;
        st.srcBlend = d3d7::BLEND_ONE;
        st.dstBlend = d3d7::BLEND_INVSRCALPHA;
        if (tex) {
            st.stage[0].colorOp = st.stage[0].alphaOp = d3d7::TOP_MODULATE;
            st.stage[0].colorArg1 = st.stage[0].alphaArg1 = d3d7::TA_TEXTURE;
            st.stage[0].colorArg2 = st.stage[0].alphaArg2 = d3d7::TA_DIFFUSE;
            st.stage[0].addressU = st.stage[0].addressV = d3d7::TADDRESS_CLAMP;
            const bool exact = w == pic.width && h == pic.height;  // a texel a pixel: sharp
            st.stage[0].magFilter = exact ? d3d7::TFG_POINT : d3d7::TFG_LINEAR;
            st.stage[0].minFilter = exact ? d3d7::TFN_POINT : d3d7::TFN_LINEAR;
        } else {
            st.stage[0].colorOp = st.stage[0].alphaOp = d3d7::TOP_SELECTARG1;
            st.stage[0].colorArg1 = st.stage[0].alphaArg1 = d3d7::TA_DIFFUSE;
        }
        st.stage[1].colorOp = st.stage[1].alphaOp = d3d7::TOP_DISABLE;
        DrawCmd cmd;
        cmd.state = record.addState(st);
        cmd.texture[0] = tex;
        cmd.effect = uint8_t(effect_exe::Kind::Panel);
        cmd.primitive = d3d7::PT_TRIANGLELIST;
        cmd.fvf = d3d7::FVF_TL1;
        record.draw(cmd, verts.data(), uint32_t(verts.size()), nullptr, 0);
        s.sceneVertices += int(verts.size());
    }
    ++s.scenesDrawn;
}

bool ready() {
    const State& s = state();
    return s.started && s.on && s.inWorld;
}

namespace {
int g_windowRank = 0;
bool g_windowMarked = false;
}
void setWindowRank(int rank, bool marked) { g_windowRank = rank, g_windowMarked = marked; }
bool g_motion = false;
int windowRank() { return g_windowRank; }
bool windowMarked() { return g_windowMarked; }

// 3.0.18-beta.58 ([interface] uaui_motion): every document's body class «motion» - the theme's soft motion under it
void setMotion(bool on) {
    if (on == g_motion) return;
    g_motion = on;
    LOG_INFO("uaui: [interface] uaui_motion = %d", on ? 1 : 0);
    for (Doc* d : state().docs)
        if (d->doc()) d->doc()->SetClass("motion", on);
}
bool motion() { return g_motion; }

// 3.0.18-beta.64 ([interface] uaui_glass, Direct3D 12 only): the windows' frosted glass - the body class «glassy»
void setGlass(bool on) {
    State& s = state();
    if (on == s.glassOn) return;
    s.glassOn = on;
    LOG_INFO("uaui: frosted glass %s", on ? "on" : "off");
    for (Doc* d : s.docs)
        if (d->doc()) d->doc()->SetClass("glassy", on);
}

// 3.0.18-beta.61 ([interface] uaui_effects): gradients, soft shadows and glows, rounded clips - the theme's under the
// body class «fx»
void setEffects(bool on) {
    State& s = state();
    if (on == s.effects) return;
    s.effects = on;
    LOG_INFO("uaui: [interface] uaui_effects = %d", on ? 1 : 0);
    for (Doc* d : s.docs)
        if (d->doc()) d->doc()->SetClass("fx", on);
}
HWND gameWindow() { return state().game; }
double targetScale() { return state().k; }

// 3.0.18-beta.59 (the possibilities' item 9): the keys while a field of uaUI's has the focus - its letters, its
// arrows, Backspace, Delete, Home, End, Ctrl+A/C/V/X; Esc or Enter lets it go
bool key(UINT m, WPARAM wp, LPARAM) {
    State& s = state();
    if (!s.started || !s.context || !s.on) return false;
    Rml::Element* f = s.context->GetFocusElement();
    if (!f) return false;
    const Rml::String tag = f->GetTagName();
    const Rml::String type = f->GetAttribute<Rml::String>("type", "text");
    if (!(tag == "textarea" || (tag == "input" && (type == "text" || type == "password")))) return false;
    if (m == WM_CHAR) {
        if (wp >= 32 && wp != 127) s.context->ProcessTextInput(typedUtf8(unsigned(wp)));
        return true;
    }
    if (m != WM_KEYDOWN && m != WM_KEYUP) return false;
    using K = Rml::Input::KeyIdentifier;
    K k = K::KI_UNKNOWN;
    switch (wp) {
        case VK_BACK: k = K::KI_BACK; break;
        case VK_DELETE: k = K::KI_DELETE; break;
        case VK_LEFT: k = K::KI_LEFT; break;
        case VK_RIGHT: k = K::KI_RIGHT; break;
        case VK_UP: k = K::KI_UP; break;
        case VK_DOWN: k = K::KI_DOWN; break;
        case VK_HOME: k = K::KI_HOME; break;
        case VK_END: k = K::KI_END; break;
        case 'A': k = K::KI_A; break;
        case 'C': k = K::KI_C; break;
        case 'V': k = K::KI_V; break;
        case 'X': k = K::KI_X; break;
        case VK_RETURN:
        case VK_ESCAPE:
            if (m == WM_KEYDOWN) f->Blur();
            return true;
        default: break;
    }
    int mods = 0;
    if (GetKeyState(VK_CONTROL) & 0x8000) mods |= Rml::Input::KM_CTRL;
    if (GetKeyState(VK_SHIFT) & 0x8000) mods |= Rml::Input::KM_SHIFT;
    if (k != K::KI_UNKNOWN) {
        if (m == WM_KEYDOWN) s.context->ProcessKeyDown(k, mods);
        else s.context->ProcessKeyUp(k, mods);
    }
    return true;  // the game types nothing while the field has it
}

void Doc::mark(bool on) {
    if (!doc_ || int(on) == marked_) return;
    marked_ = int(on);
    doc_->SetClass("marked", on);
}

void Doc::raise(int z) {
    if (!doc_ || z == z_) return;
    z_ = z;
    doc_->SetProperty("z-index", std::to_string(z));
}

uint32_t themed(uint32_t rgb) { return uitheme::colour(state().theme, rgb); }
float themeRadius() { return state().theme.radius; }
float themeEdge() { return state().theme.edge; }

bool readyAnywhere() {
    const State& s = state();
    return s.started && s.on && s.k > 0 && s.targetW > 0 && s.targetH > 0;
}

float dp() { return uistyle::partScale(state().k); }

std::string utf8(const std::string& gameText) { return utf8From(gameText, state().codePage); }

std::string picture(const panel::Picture* p, int generation, std::string& previous) {
    if (!p) {
        if (!previous.empty()) Rml::ReleaseTexture(previous), previous.clear();
        return {};
    }
    char src[48];
    std::snprintf(src, sizeof src, "pic:%llx:%d", (unsigned long long)uintptr_t(p), generation);
    if (previous != src) {
        if (!previous.empty()) Rml::ReleaseTexture(previous);
        previous = src;
    }
    return previous;
}

int stamp(const panel::Picture* p) {
    if (!p) return 0;
    uint32_t h = uint32_t(p->width) * 2654435761u ^ uint32_t(p->height);
    for (size_t i = 0; i < p->px.size(); i += 97) h = (h ^ p->px[i]) * 16777619u;
    return int(h & 0x7FFFFFFF);
}

bool Doc::make(const char* name, const char* rml, const std::function<void(Rml::DataModelConstructor&)>& bind) {
    State& s = state();
    if (doc_ || !s.started || !s.context) return doc_ != nullptr;
    // 3.0.18-beta.47 (R-27NCJE: the sheets 0x0, the experience strip «Could not locate data model 'expbars'»): the
    // model's name made here - letters and digits, one of its own (the whispers' sheets share a part's name) - and put
    // on the document's body in place of any the markup has
    static int made = 0;
    std::string model;
    for (const char* c = name; *c; ++c) model += std::isalnum(static_cast<unsigned char>(*c)) ? *c : '_';
    model += "_" + std::to_string(++made);
    Rml::DataModelConstructor m = s.context->CreateDataModel(model);
    if (!m) {
        LOG_ERROR("uaui: no model «%s»", model.c_str());
        return false;
    }
    if (bind) bind(m);
    model_ = m.GetModelHandle();
    model_name_ = model;
    builtin_ = rml;
    this->name = name;
    // 3.0.18-beta.59 (the possibilities' item 12): the built-in markup as a sample (ui\builtin\<part>.rml); the
    // player's ui\<part>.rml in its place when there
    static std::set<std::string> written;
    const std::string file = safeName(name) + ".rml";
    if (written.insert(file).second) writeSample(file, builtin_);
    bool there = false;
    const std::string own = readSmall(layout::join(uiDir(), file), &there);
    if (there && load(own)) {
        WIN32_FILE_ATTRIBUTE_DATA a{};
        if (GetFileAttributesExA(layout::join(uiDir(), file).c_str(), GetFileExInfoStandard, &a))
            fileStamp_ = (unsigned long long)a.ftLastWriteTime.dwHighDateTime << 32 | a.ftLastWriteTime.dwLowDateTime;
        LOG_INFO("uaui: «%s» from ui\\%s", name, file.c_str());
    } else if (!load(builtin_)) {
        return false;
    }
    s.docs.push_back(this);
    LOG_INFO("uaui: «%s» made", name);
    return true;
}

// Its markup (the model's name put on its body) loaded in place of the one shown: false - kept as it was
bool Doc::load(const std::string& rml) {
    State& s = state();
    std::string text = rml;
    const size_t body = text.find("<body");
    if (body == std::string::npos) {
        LOG_ERROR("uaui: «%s» has no body", name.c_str());
        return false;
    }
    const size_t bodyEnd = text.find('>', body);
    const size_t had = text.find("data-model=\"", body);
    if (had != std::string::npos && had < bodyEnd) {
        const size_t from = had + 12, to = text.find('"', from);
        text.replace(from, to - from, model_name_);
    } else {
        text.insert(body + 5, " data-model=\"" + model_name_ + "\"");
    }
    Rml::ElementDocument* d = s.context->LoadDocumentFromMemory(text, std::string("uaui://") + model_name_);
    if (!d) {
        LOG_ERROR("uaui: the document «%s» did not load", name.c_str());
        return false;
    }
    Rml::ElementDocument* was = doc_;
    doc_ = d;
    // 3.0.18-beta.27: drawn with the windows; .38 the world's under the parts (theirs 3, between the markers)
    doc_->SetProperty("z-index", z_ >= 0 ? std::to_string(z_) : window ? "10" : world ? "1" : "3");
    // 3.0.18-beta.46 (R-4YDWHF: nothing dragged): a document drawn only never takes the pointer - a screen-wide one
    // (the experience strip, the interface editor) held the hover over the windows of uaUI under it
    if (passive) doc_->SetProperty("pointer-events", "none");
    if (Rml::Element* t = doc_->GetElementById("ua-title")) t->SetInnerRML(doc_->GetTitle());
    if (Rml::Element* c = doc_->GetElementById("ua-close")) c->AddEventListener(Rml::EventId::Click, &g_close);
    doc_->SetClass("win", window);  // 3.0.18-beta.58: the theme's motion slides windows only
    doc_->SetClass("motion", g_motion);
    doc_->SetClass("fx", s.effects);  // 3.0.18-beta.61
    doc_->SetClass("glassy", s.glassOn);  // 3.0.18-beta.64
    if (marked_ > 0) doc_->SetClass("marked", true);
    if (was) {
        if (shown_) doc_->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
        was->Close();
        model_.DirtyAllVariables();
    }
    return true;
}

void Doc::lookAtFile() {
    if (!doc_) return;
    const std::string path = layout::join(uiDir(), safeName(name) + ".rml");
    WIN32_FILE_ATTRIBUTE_DATA a{};
    const unsigned long long stamp =
        GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &a)
            ? (unsigned long long)a.ftLastWriteTime.dwHighDateTime << 32 | a.ftLastWriteTime.dwLowDateTime
            : 0;
    if (stamp == fileStamp_) return;
    fileStamp_ = stamp;
    if (stamp == 0) {
        LOG_INFO("uaui: «%s» - ui\\%s.rml gone, the built-in markup again", name.c_str(), safeName(name).c_str());
        load(builtin_);
        return;
    }
    bool there = false;
    const std::string own = readSmall(path, &there);
    if (there && load(own)) LOG_INFO("uaui: «%s» - ui\\%s.rml loaded again", name.c_str(), safeName(name).c_str());
}

void Doc::show(bool on) {
    if (on) wantedAt = state().frameNo;  // 3.0.18-beta.40: asked for this frame
    if (!doc_ || on == shown_) return;
    shown_ = on;
    if (on) {
        doc_->SetClass("away", true);
        doc_->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
        fadePending = true;
    } else {
        doc_->Hide();
        doc_->SetClass("away", true);
        fadePending = false;
    }
}

void Doc::place(int x, int y) {
    if (!doc_) return;
    doc_->SetProperty("left", std::to_string(x) + "px");
    doc_->SetProperty("top", std::to_string(y) + "px");
}

bool Doc::box(int& x, int& y, int& w, int& h) const {
    if (!doc_ || !shown_) return false;
    const Rml::Vector2f at = doc_->GetAbsoluteOffset(Rml::BoxArea::Border);
    const Rml::Vector2f size = doc_->GetBox().GetSize(Rml::BoxArea::Border);
    x = int(at.x), y = int(at.y), w = int(size.x), h = int(size.y);
    return w > 0 && h > 0;
}

namespace {

std::string paletteFile() { return layout::join(gameDir(), layout::kUiPalette); }

// The theme chosen and the palette's file over it - made again when either changes (the file looked at each second)
void themeFrame(State& s, const std::string& name, bool look) {
    bool changed = name != s.themeName;
    if (look) {
        WIN32_FILE_ATTRIBUTE_DATA a{};
        const bool there = GetFileAttributesExA(paletteFile().c_str(), GetFileExInfoStandard, &a) != 0;
        const FILETIME t = there ? a.ftLastWriteTime : FILETIME{};
        if (there != s.paletteThere || CompareFileTime(&t, &s.paletteStamp) != 0) changed = true;
        s.paletteThere = there;
        s.paletteStamp = t;
    }
    if (!changed) return;
    s.themeName = name;
    s.theme = uitheme::named(name);
    std::string text, bad;
    HANDLE h = CreateFileA(paletteFile().c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_EXISTING, 0, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER size{};
        if (GetFileSizeEx(h, &size) && size.QuadPart > 0 && size.QuadPart < (1 << 20)) {
            text.resize(size_t(size.QuadPart));
            DWORD got = 0;
            if (!ReadFile(h, text.data(), DWORD(text.size()), &got, nullptr)) text.clear();
            text.resize(got);
        }
        CloseHandle(h);
    }
    if (!text.empty()) uitheme::read(text, s.theme, bad);
    ++s.themeSerial;
    if (s.started) {  // 3.0.18-beta.54: the documents' style sheets again, the parts' own colours said again
        Rml::Factory::ClearStyleSheetCache();
        for (Doc* d : s.docs)
            if (d->doc()) d->doc()->ReloadStyleSheet();
        uistyle::bump();
    }
    LOG_INFO("uaui: the theme «%s»%s%s%s", s.theme.name.c_str(), text.empty() ? "" : ", the palette's file over it",
             bad.empty() ? "" : " - a line not understood: ", bad.c_str());
}

}  // namespace

void noteUnrecorded(int w, int h) { ++state().unrecorded[{w, h}]; }


void begin(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight,
           const std::string& theme) {
    State& s = state();
    s.game = game;
    s.codePage = codePage;
    s.k = k;
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    const bool world = inGameWorld() && k > 0 && targetWidth > 0 && targetHeight > 0;
    if (on != s.on) {
        LOG_INFO("uaui: [interface] uaui = %d", on ? 1 : 0);
        uistyle::bump();  // 3.0.18-beta.12: every part paints again - recorded, or painted as before
    }
    s.on = on;
    s.inWorld = world;
    if (on && k > 0 && targetWidth > 0 && targetHeight > 0) start(s);
    if (on && s.started) {
        static ULONGLONG lookedAt = 0;
        const ULONGLONG t = GetTickCount64();
        themeFrame(s, theme, t - lookedAt >= 1000);
        if (t - lookedAt >= 1000) {
            lookedAt = t;
            for (Doc* d : s.docs) d->lookAtFile();  // 3.0.18-beta.59: a part's own markup changed
            static bool sampled = false;
            if (!sampled) {
                sampled = true;
                writeSample("theme.rcss", kTheme);
                writeSample("README.txt", kSamplesReadme);
            }
        }
    }
    // 3.0.18-beta.57: the parts' plates measured by the letters drawn - every part lays out again when that changes
    const panel::Measure measure = on && s.started ? panel::Measure(textWidth) : nullptr;
    static panel::Measure measuring = nullptr;
    if (measure != measuring) {
        measuring = measure;
        panel::setMeasure(measure);
        uistyle::bump();
    }
    // 3.0.18-beta.16 (the owner: «надо вообщето Переписывать 73 части в разметку RmlUi вручную»; R-ZK9ZK9, the game
    // gone after «Довідник» recorded): the parts' pictures no longer recorded - each part moves to RmlUi by hand
    panel::setRecording(false);
    if (!on || !s.started || !readyAnywhere()) {
        for (Doc* d : s.docs) d->show(false);
        uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Uaui, nullptr, nullptr, false);
        return;
    }
    if (!world)  // 3.0.18-beta.33: outside the world only the login's windows
        for (Doc* d : s.docs)
            if (!d->anywhere) d->show(false);
    s.frameStart = clockNow();
    s.context->SetDimensions({targetWidth, targetHeight});
    s.context->SetDensityIndependentPixelRatio(uistyle::partScale(k));
    const ULONGLONG nowMs = GetTickCount64();
    if (nowMs - s.themeAt >= 1000) {  // the theme's file read again within a second of a change
        s.themeAt = nowMs;
        if (themeChanged(s)) {
            Rml::Factory::ClearStyleSheetCache();
            Rml::Factory::ClearTemplateCache();
            for (Doc* d : s.docs) d->doc()->ReloadStyleSheet();
            LOG_INFO("uaui: the theme read again - %s", s.themeFile ? themeFile().c_str() : "built in");
        }
    }
    int cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) s.context->ProcessMouseMove(int(float(cx) * float(k)), int(float(cy) * float(k)), 0);
}

void end() {
    State& s = state();
    // 3.0.18-beta.40: a document its part did not ask to show this frame (the part no longer runs: «Продовжити» after
    // the way in, a window's part gone quiet) - hidden, never left on the screen
    for (Doc* d : s.docs)
        if (d->shown() && d->wantedAt != s.frameNo) d->show(false);
    ++s.frameNo;
    if (!readyAnywhere()) return;
    // 3.0.18-beta.48 (R-QTH95E: «Довідник» with letters missing - the atlas full at 4K): a full atlas starts again -
    // every face's letters let go, the words made anew with the letters in sight now
    if (s.atlas.full) {
        LOG_INFO("uaui: the letters' atlas full (%d letters) - started again", s.atlas.glyphs);
        std::fill(s.atlas.px.begin(), s.atlas.px.end(), 0u);
        s.atlas.shelfX = s.atlas.shelfY = s.atlas.shelfH = 0;
        s.atlas.full = false;
        s.atlas.glyphs = 0;
        s.atlas.touch(0, kAtlas);
        for (auto& [key, face] : s.faces) face.glyphs.clear(), face.halos.clear();
        ++s.fontVersion;
    }
    for (Doc* d : s.docs)  // a frame after showing: the fade in runs (the theme's transition)
        if (d->fadePending && d->shown()) d->doc()->SetClass("away", false), d->fadePending = false;
    s.context->Update();
    // 3.0.18-beta.10 (the owner: «когда мышкой колесиком скролю мир увеличивается и уменьшается»): the document under
    // the game's cursor a window for the game (win/world_mouse)
    RECT client{};
    int x = 0, y = 0, w = 0, h = 0;
    Doc* under = docUnder(s);
    if (under && under->box(x, y, w, h) && GetClientRect(s.game, &client)) {
        const RECT r{LONG(int64_t(x) * client.right / s.targetW), LONG(int64_t(y) * client.bottom / s.targetH),
                     LONG(int64_t(x + w) * client.right / s.targetW + 1),
                     LONG(int64_t(y + h) * client.bottom / s.targetH + 1)};
        uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Uaui, s.game, &r, false);
    } else {
        uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Uaui, nullptr, nullptr, false);
    }
    const double t1 = clockNow();
    s.batches.clear();
    s.splitSeen = s.worldSeen = false;
    s.context->Render();
    if (!s.worldSeen) s.worldSplit = 0;
    if (!s.splitSeen) s.split = s.batches.size();
    const double t2 = clockNow();
    s.stats.update += t1 - s.frameStart;
    s.stats.render += t2 - t1;
}

void draw(FrameRecord& record, gpu::Renderer& renderer, Layer layer) {
    State& s = state();
    // 3.0.18-beta.38: the world's layer where the world ends - under the game's own windows, names and cursor (as
    // the monsters' bars), its uploads too
    const size_t worldEnd = record.firstInterface(), commandsBefore = record.commands().size();
    struct UnderGame {
        FrameRecord& r;
        size_t at, before;
        bool on;
        ~UnderGame() {
            const size_t added = r.commands().size() - before;
            if (on && added) r.moveLastBefore(added, at);
        }
    } underGame{record, worldEnd, commandsBefore, layer == Layer::World};
    for (uint32_t id : s.released) renderer.releaseTexture(id);
    s.released.clear();
    if (!readyAnywhere() || s.batches.empty()) return;
    const size_t n = s.batches.size(), world = std::min(s.worldSplit, n), split = std::max(world, std::min(s.split, n));
    const size_t from = layer == Layer::Windows ? split : layer == Layer::Hud ? world : 0;
    const size_t to = layer == Layer::Windows ? n : layer == Layer::Hud ? split : world;
    if (from >= to) return;
    const double t0 = clockNow();
    // the glyphs' atlas, once made and again when new letters came
    if (!s.atlasId) s.atlasId = renderer.createTexture(kAtlas, kAtlas, 1), s.atlas.touch(0, kAtlas);
    if (s.atlasId && s.atlas.dirty) {  // 3.0.18-beta.48: the rows changed alone
        const int y0 = std::clamp(s.atlas.dirtyY0, 0, kAtlas), y1 = std::clamp(s.atlas.dirtyY1, y0 + 1, kAtlas);
        record.upload(s.atlasId, 0, Rect{0, y0, kAtlas, y1}, s.atlas.px.data() + size_t(y0) * kAtlas);
        s.atlas.dirty = false;
        ++s.atlas.uploads;
    }
    const float k = float(s.k), half = 0.5f / float(partStretch());
    std::vector<glint::Vertex> verts;
    uint32_t runTexture = 0;
    bool runSmooth = false, runGlass = false;
    int draws = 0, vertices = 0;
    const auto flush = [&]() {
        if (verts.empty()) return;
        DrawSetup st;  // premultiplied over the world, no depth (win/picture_draw's)
        st.ui = true;
        st.blend = true;
        st.srcBlend = d3d7::BLEND_ONE;
        st.dstBlend = d3d7::BLEND_INVSRCALPHA;
        if (runGlass) {  // 3.0.18-beta.64: the picture's copy read smoothly, its edges clamped
            st.stage[0].colorOp = st.stage[0].alphaOp = d3d7::TOP_SELECTARG1;
            st.stage[0].colorArg1 = st.stage[0].alphaArg1 = d3d7::TA_DIFFUSE;
            st.stage[0].addressU = st.stage[0].addressV = d3d7::TADDRESS_CLAMP;
            st.stage[0].magFilter = d3d7::TFG_LINEAR;
            st.stage[0].minFilter = d3d7::TFN_LINEAR;
        } else if (runTexture) {
            st.stage[0].colorOp = st.stage[0].alphaOp = d3d7::TOP_MODULATE;
            st.stage[0].colorArg1 = st.stage[0].alphaArg1 = d3d7::TA_TEXTURE;
            st.stage[0].colorArg2 = st.stage[0].alphaArg2 = d3d7::TA_DIFFUSE;
            st.stage[0].addressU = st.stage[0].addressV = d3d7::TADDRESS_CLAMP;
            st.stage[0].magFilter = runSmooth ? d3d7::TFG_LINEAR : d3d7::TFG_POINT;
            st.stage[0].minFilter = runSmooth ? d3d7::TFN_LINEAR : d3d7::TFN_POINT;
        } else {
            st.stage[0].colorOp = st.stage[0].alphaOp = d3d7::TOP_SELECTARG1;
            st.stage[0].colorArg1 = st.stage[0].alphaArg1 = d3d7::TA_DIFFUSE;
        }
        st.stage[1].colorOp = st.stage[1].alphaOp = d3d7::TOP_DISABLE;
        DrawCmd cmd;
        cmd.state = record.addState(st);
        cmd.texture[0] = runTexture;
        cmd.effect = uint8_t(runGlass ? effect_exe::Kind::Backdrop : effect_exe::Kind::Panel);
        cmd.primitive = d3d7::PT_TRIANGLELIST;
        cmd.fvf = d3d7::FVF_TL1;
        record.draw(cmd, verts.data(), uint32_t(verts.size()), nullptr, 0);
        ++draws;
        vertices += int(verts.size());
        verts.clear();
    };
    for (size_t bi = from; bi < to; ++bi) {
        const Batch& b = s.batches[bi];
        const auto g = s.geometries.find(b.geometry);
        if (g == s.geometries.end()) continue;
        uint32_t texture = 0;
        if (b.texture == kAtlasHandle) {
            texture = s.atlasId;
        } else if (b.texture) {
            const auto t = s.textures.find(b.texture);
            if (t != s.textures.end()) {
                if (!t->second.id) t->second.id = renderer.createTexture(t->second.w, t->second.h, 1);
                if (t->second.id && t->second.upload) {
                    record.upload(t->second.id, 0, Rect{0, 0, t->second.w, t->second.h}, t->second.px.data());
                    t->second.upload = false;
                }
                texture = t->second.id;
            }
        }
        const bool smooth = b.transformed || (b.texture && b.texture != kAtlasHandle);
        if (texture != runTexture || smooth != runSmooth || b.glass != runGlass) {
            flush();
            runTexture = texture;
            runSmooth = smooth;
            runGlass = b.glass;
        }
        ::uagfx::uaui::triangles(g->second.points, g->second.indices, b.dx, b.dy, b.transformed ? b.m : nullptr,
                                 b.clipped ? &b.clip : nullptr, k, half, verts);
    }
    flush();
    const double spent = clockNow() - t0;
    Stats& st = s.stats;
    st.emit += spent;
    st.draws += draws;
    st.vertices += vertices;
    ++st.frames;
    const double now = clockNow();
    if (st.since == 0) st.since = now;
    st.worst = std::max(st.worst, spent);
    if (now - st.since >= 10.0 && st.frames > 0) {
        // 3.0.18-beta.46 (R-4YDWHF: the chat and the hotbar not seen): each document shown - its place, size, opacity,
        // z-index, whether it fades in still, the elements in it
        std::string docs;
        for (Doc* d : s.docs) {
            if (!d->shown() || !d->doc() || docs.size() > 1500) continue;
            Rml::ElementDocument* e = d->doc();
            const Rml::Vector2f at = e->GetAbsoluteOffset(Rml::BoxArea::Border);
            const Rml::Vector2f size = e->GetBox().GetSize(Rml::BoxArea::Border);
            char line[200];
            std::snprintf(line, sizeof line, "%s%s at %.0f,%.0f %.0fx%.0f op %.2f z %d%s%s, %d children",
                          docs.empty() ? "" : "; ", d->name.c_str(), at.x, at.y, size.x, size.y,
                          e->GetComputedValues().opacity(), int(e->GetComputedValues().z_index().value),
                          e->IsClassSet("away") ? " away" : "", e->IsVisible() ? "" : " invisible",
                          e->GetNumChildren());
            docs += line;
        }
        LOG_INFO("uaui: shown: %s (batches %zu: world %zu, parts to %zu)", docs.empty() ? "none" : docs.c_str(),
                 s.batches.size(), s.worldSplit, s.split);
        const double n = st.frames;
        if (s.scenesDrawn)
            LOG_INFO("uaui: in %.0f s %d pictures of the parts drawn by their scenes, %.0f vertices a frame; %zu words "
                     "in the %dx%d atlas, %zu icons, %zu pictures", now - st.since, s.scenesDrawn,
                     double(s.sceneVertices) / double(std::max(st.frames, 1)), s.words.size(), kWords, kWords,
                     s.icons.size(), s.images.size());
        s.scenesDrawn = s.sceneVertices = 0;
        if (!s.unrecorded.empty()) {  // 3.0.18-beta.13: the parts still painted, by their pictures' sizes
            std::string list;
            for (const auto& [size, n] : s.unrecorded) {
                if (list.size() > 300) break;
                list += (list.empty() ? "" : ", ") + std::to_string(size.first) + "x" + std::to_string(size.second) +
                        " ×" + std::to_string(n);
            }
            LOG_INFO("uaui: pictures drawn as painted, not recorded (no panel::blank): %s", list.c_str());
            s.unrecorded.clear();
        }
        for (auto it = s.seen.begin(); it != s.seen.end();)
            if (it->second.last + 60000 < GetTickCount64()) it = s.seen.erase(it);
            else ++it;
        const ULONGLONG old = GetTickCount64() - 60000;  // unused for a minute: let go
        for (auto it = s.images.begin(); it != s.images.end();)
            if (it->second.used < old) renderer.releaseTexture(it->second.id), it = s.images.erase(it);
            else ++it;
        for (auto it = s.scenes.begin(); it != s.scenes.end();)  // the scenes of pictures gone
            if (it->second.used < old) it = s.scenes.erase(it);
            else ++it;
        for (auto it = s.icons.begin(); it != s.icons.end();)
            if (it->second.used < old) renderer.releaseTexture(it->second.id), it = s.icons.erase(it);
            else ++it;
        LOG_INFO("uaui: in %.0f s %d frames - RmlUi's update %.3f ms, its render %.3f ms, ours into the bridge %.3f ms "
                 "(at most %.3f) a frame; %.1f draws, %.0f vertices a frame; %d letters in the %dx%d atlas%s, sent %d "
                 "times", now - st.since, st.frames, st.update * 1000 / n, st.render * 1000 / n, st.emit * 1000 / n,
                 st.worst * 1000, st.draws / n, st.vertices / n, s.atlas.glyphs, kAtlas, kAtlas,
                 s.atlas.full ? " (full)" : "", s.atlas.uploads);
        st = Stats{};
        st.since = now;
    }
}


bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    if (!ready() || !s.context) return false;
    const bool down = m == WM_LBUTTONDOWN, up = m == WM_LBUTTONUP, wheel = m == WM_MOUSEWHEEL;
    const bool right = m == WM_RBUTTONDOWN || m == WM_RBUTTONUP;
    if (!down && !up && !wheel && !right) return false;
    float tx = 0, ty = 0;
    if (!pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty)) return false;
    s.context->ProcessMouseMove(int(tx), int(ty), 0);
    const bool over = docUnder(s) != nullptr;
    // 3.0.18-beta.22 (the owner: «окна не перетаскиваются»): a button let go is ours only when its press was - a drag
    // of the driver's other windows ends where they are, its release theirs
    const bool ours = (up && s.leftDown) || (m == WM_RBUTTONUP && s.rightDown);
    if (!over && !ours) return false;
    if (down) s.leftDown = true;
    if (up) s.leftDown = false;
    if (m == WM_RBUTTONDOWN) s.rightDown = true;
    if (m == WM_RBUTTONUP) s.rightDown = false;
    if (wheel) s.context->ProcessMouseWheel(-float(GET_WHEEL_DELTA_WPARAM(wp)) / float(WHEEL_DELTA), 0);
    else if (down) s.context->ProcessMouseButtonDown(0, 0);
    else if (up) s.context->ProcessMouseButtonUp(0, 0);
    else if (m == WM_RBUTTONDOWN) s.context->ProcessMouseButtonDown(1, 0);
    else s.context->ProcessMouseButtonUp(1, 0);
    result = 0;
    return true;
}

}  // namespace uagfx::win::uaui

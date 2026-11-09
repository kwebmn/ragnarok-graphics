#include "win/uaui.hpp"

#include <RmlUi/Core.h>

#include <algorithm>
#include <cstdlib>
#include <cmath>
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
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/ui_font.hpp"

namespace uagfx::win::uaui {
namespace {

constexpr int kAtlas = 1024;               // the glyphs' atlas, one texture
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
};

struct Atlas {
    std::vector<uint32_t> px = std::vector<uint32_t>(size_t(kAtlas) * kAtlas, 0u);
    int shelfX = 0, shelfY = 0, shelfH = 0;
    bool dirty = false, full = false;
    int glyphs = 0, uploads = 0;
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
};

struct Geometry {
    std::vector<::uagfx::uaui::Point> points;
    std::vector<int> indices;
};


struct State {
    bool started = false, failed = false, on = false, inWorld = false, dragging = false;
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
    bool clipOn = false;
    ::uagfx::uaui::ClipBox clip{};
    bool transformOn = false;
    float transform[16] = {};
    // the words
    std::unordered_map<int, Face> faces;
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
        if (type <= Rml::Log::LT_WARNING && s.logged < 20) {
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
        b.transformed = s.transformOn;
        if (s.transformOn) std::copy(s.transform, s.transform + 16, b.m);
        s.batches.push_back(b);
    }
    void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override { state().geometries.erase(uintptr_t(geometry)); }
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
        s.clip = {float(r.Left()), float(r.Top()), float(r.Right()), float(r.Bottom())};
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
            a.dirty = true;
            ++a.glyphs;
        }
    } else {
        g.w = g.h = 0;  // a space: its advance alone
    }
    return f.glyphs.emplace(c, g).first->second;
}

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
    int GetStringWidth(Rml::FontFaceHandle h, Rml::StringView text, const Rml::TextShapingContext&,
                       Rml::Character) override {
        State& s = state();
        Face& f = faceOf(h);
        int w = 0;
        for (char32_t c : ::uagfx::uaui::codepoints(std::string_view(text.begin(), text.size())))
            w += glyphOf(s, f, c).advance;
        return w;
    }
    int GenerateString(Rml::RenderManager& rm, Rml::FontFaceHandle h, Rml::FontEffectsHandle, Rml::StringView text,
                       Rml::Vector2f at, Rml::ColourbPremultiplied colour, float, const Rml::TextShapingContext&,
                       Rml::TexturedMeshList& list) override {
        State& s = state();
        Face& f = faceOf(h);
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

// RmlUi's files: «uaui:» ours from memory (the theme from its file when there), else none
class Files final : public Rml::FileInterface {
public:
    Rml::FileHandle Open(const Rml::String& path) override {
        auto* f = new Open_;
        if (path == "uaui:theme.rcss") {
            f->data = kTheme;
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
                        f->data = std::move(text);
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
    // the numbers as the interface writes them - «5 157» (core/char_block grouped); for every model of the context
    Rml::DataModelConstructor m = s.context->CreateDataModel("ua");
    m.RegisterTransformFunc("num", [](const Rml::VariantList& a) -> Rml::Variant {
        return a.empty() ? Rml::Variant() : Rml::Variant(charblock::grouped(a[0].Get<int64_t>()));
    });
    themeChanged(s);  // its stamp now: read again only when it changes
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
        if (d->shown() && d->doc() == owner) return d;
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
            ::uagfx::uaui::ClipBox clip{float(p.cx0) * sx, float(p.cy0) * sy, float(p.cx1) * sx, float(p.cy1) * sy};
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
                        quad(pts, ix, p.x - 2, p.y - 2, p.w + 4, p.h + 4, premul(0x000000, 0.85f * alpha),
                             float(slot.x) * inv, float(slot.y) * inv, float(slot.x + p.w + 4) * inv,
                             float(slot.y + p.h + 4) * inv);
                    if (wordSlot(s, p, false, slot))
                        quad(pts, ix, p.x, p.y, p.w, p.h, premul(uitheme::colour(s.theme, p.fill), alpha),
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

bool Doc::make(const char* name, const char* rml, const std::function<void(Rml::DataModelConstructor&)>& bind) {
    State& s = state();
    if (doc_ || !s.started || !s.context) return doc_ != nullptr;
    Rml::DataModelConstructor m = s.context->CreateDataModel(name);
    if (!m) {
        LOG_ERROR("uaui: no model «%s»", name);
        return false;
    }
    if (bind) bind(m);
    model_ = m.GetModelHandle();
    doc_ = s.context->LoadDocumentFromMemory(rml, std::string("uaui:") + name);
    if (!doc_) {
        LOG_ERROR("uaui: the document «%s» did not load", name);
        return false;
    }
    if (Rml::Element* t = doc_->GetElementById("ua-title")) t->SetInnerRML(doc_->GetTitle());
    if (Rml::Element* c = doc_->GetElementById("ua-close")) c->AddEventListener(Rml::EventId::Click, &g_close);
    s.docs.push_back(this);
    LOG_INFO("uaui: «%s» made", name);
    return true;
}

void Doc::show(bool on) {
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
        if (t - lookedAt >= 1000) lookedAt = t;
    }
    // 3.0.18-beta.12: the parts' pictures recorded for the card while on (in the world and out of it)
    panel::setRecording(on && s.started);
    if (on && s.started && !s.recordingTold) {
        s.recordingTold = true;
        LOG_INFO("uaui: the parts' pictures recorded and drawn on the card (their shapes, icons, pictures, words)");
    }
    if (!on || !world || !s.started) {
        for (Doc* d : s.docs) d->show(false);
        uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Uaui, nullptr, nullptr, false);
        return;
    }
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
    if (!ready()) return;
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
    s.context->Render();
    const double t2 = clockNow();
    s.stats.update += t1 - s.frameStart;
    s.stats.render += t2 - t1;
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    for (uint32_t id : s.released) renderer.releaseTexture(id);
    s.released.clear();
    if (!ready() || s.batches.empty()) return;
    const double t0 = clockNow();
    // the glyphs' atlas, once made and again when new letters came
    if (!s.atlasId) s.atlasId = renderer.createTexture(kAtlas, kAtlas, 1), s.atlas.dirty = true;
    if (s.atlasId && s.atlas.dirty) {
        record.upload(s.atlasId, 0, Rect{0, 0, kAtlas, kAtlas}, s.atlas.px.data());
        s.atlas.dirty = false;
        ++s.atlas.uploads;
    }
    const float k = float(s.k), half = 0.5f / float(partStretch());
    std::vector<glint::Vertex> verts;
    uint32_t runTexture = 0;
    bool runSmooth = false;
    int draws = 0, vertices = 0;
    const auto flush = [&]() {
        if (verts.empty()) return;
        DrawSetup st;  // premultiplied over the world, no depth (win/picture_draw's)
        st.ui = true;
        st.blend = true;
        st.srcBlend = d3d7::BLEND_ONE;
        st.dstBlend = d3d7::BLEND_INVSRCALPHA;
        if (runTexture) {
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
        cmd.effect = uint8_t(effect_exe::Kind::Panel);
        cmd.primitive = d3d7::PT_TRIANGLELIST;
        cmd.fvf = d3d7::FVF_TL1;
        record.draw(cmd, verts.data(), uint32_t(verts.size()), nullptr, 0);
        ++draws;
        vertices += int(verts.size());
        verts.clear();
    };
    for (const Batch& b : s.batches) {
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
        if (texture != runTexture || smooth != runSmooth) {
            flush();
            runTexture = texture;
            runSmooth = smooth;
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
    if (!over && !(up && s.context->IsMouseInteracting())) return false;
    if (wheel) s.context->ProcessMouseWheel(-float(GET_WHEEL_DELTA_WPARAM(wp)) / float(WHEEL_DELTA), 0);
    else if (down) s.context->ProcessMouseButtonDown(0, 0);
    else if (up) s.context->ProcessMouseButtonUp(0, 0);
    else if (m == WM_RBUTTONDOWN) s.context->ProcessMouseButtonDown(1, 0);
    else s.context->ProcessMouseButtonUp(1, 0);
    result = 0;
    return true;
}

}  // namespace uagfx::win::uaui

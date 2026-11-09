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
#include "core/layout.hpp"
#include "core/window_panel.hpp"
#include "win/item_window.hpp"
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

// 3.0.18-beta.9: an item in the showcase - its icon (a texture by its picture, «icon:<address>») and its count
struct ItemView {
    Rml::String icon;
    int count = 0;
};

struct State {
    bool started = false, failed = false, on = false, shown = false, dragging = false;
    // 3.0.18-beta.9 (stage 2): the components' showcase - a window of the theme's template with tabs
    Rml::ElementDocument* demo = nullptr;
    Rml::DataModelHandle demoModel;
    std::vector<ItemView> items;
    std::vector<Rml::String> lines;
    float demoHp = 0;
    bool demoClosed = false;
    ULONGLONG itemsAt = 0, themeAt = 0;
    FILETIME themeStamp{};
    bool themeFile = false;
    Rml::Context* context = nullptr;
    Rml::ElementDocument* doc = nullptr;
    Rml::Element* card = nullptr;
    Rml::DataModelHandle model;
    Rml::String name;
    int base = 0, job = 0, hp = 0, hpMost = 0, sp = 0, spMost = 0;
    float hpPct = 0, spPct = 0;
    double k = 1;
    int targetW = 0, targetH = 0;
    float dragX = 0, dragY = 0;
    bool fadeAsked = false;
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
};

State& state() {
    static State s;
    return s;
}

// ---- RmlUi's interfaces ----

class System final : public Rml::SystemInterface {
public:
    double GetElapsedTime() override { return clockNow(); }
    // our own names as they are: «uaui:» the built-in files (Files), «icon:» a picture of the driver's
    void JoinPath(Rml::String& out, const Rml::String& document, const Rml::String& path) override {
        if (path.rfind("uaui:", 0) == 0 || path.rfind("icon:", 0) == 0) {
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
        if (source.rfind("icon:", 0) == 0) {  // 3.0.18-beta.9: an icon the driver keeps (client_tables), as it is
            const auto* pic =
                reinterpret_cast<const panel::Picture*>(uintptr_t(std::strtoull(source.c_str() + 5, nullptr, 16)));
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

// The card: dragged by the mouse
class Drag final : public Rml::EventListener {
public:
    void ProcessEvent(Rml::Event& e) override {
        State& s = state();
        Rml::Element* card = e.GetCurrentElement();
        const float mx = e.GetParameter<float>("mouse_x", 0), my = e.GetParameter<float>("mouse_y", 0);
        if (e.GetId() == Rml::EventId::Dragstart) {
            // 3.0.18-beta.8 (the owner: «плашка кудато прыгает влево и потом уже нормально двигается»): the card
            // placed by its own pixels before it moves - its centring (left 50 %, margin-left -160dp) left the margin
            // under the dragged left, a jump of 160dp (365 pixels at 4K)
            const Rml::Vector2f at = card->GetAbsoluteOffset(Rml::BoxArea::Border);
            card->SetProperty("margin-left", "0px");
            card->SetProperty("left", std::to_string(int(at.x)) + "px");
            card->SetProperty("top", std::to_string(int(at.y)) + "px");
            s.dragX = mx - at.x;
            s.dragY = my - at.y;
            s.dragging = true;
        } else if (e.GetId() == Rml::EventId::Drag) {
            card->SetProperty("left", std::to_string(int(mx - s.dragX)) + "px");
            card->SetProperty("top", std::to_string(int(my - s.dragY)) + "px");
        } else if (e.GetId() == Rml::EventId::Dragend) {
            s.dragging = false;
        }
    }
};
Drag g_drag;

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

const char* kCard = R"(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="ua-screen" data-model="uaui">
<div id="card" class="ua-card away">
<div><span class="ua-name">{{name}}</span><span class="ua-sub">Base {{base}} · Job {{job}}</span></div>
<div class="ua-row"><span class="ua-num">HP {{hp | num}} / {{hp_most | num}}</span><div class="ua-bar"><div class="ua-fill" data-style-width="hp_pct + '%'"></div></div></div>
<div class="ua-row"><span class="ua-num">SP {{sp | num}} / {{sp_most | num}}</span><div class="ua-bar"><div class="ua-fill sp" data-style-width="sp_pct + '%'"></div></div></div>
<span class="ua-tag">uaUI · RmlUi 6.3</span>
</div></body></rml>)";

// The showcase: the theme's window with tabs - the bag's items (their icons), a list that scrolls, buttons, switches
const char* kDemo = R"(<rml><head><link type="text/template" href="uaui:window.rml"/><title>uaUI · компоненти</title></head>
<body template="ua-window" data-model="demo" style="left: 120dp; top: 200dp;">
<tabset>
<tab>Речі</tab><panel>
<div class="ua-grid" data-if="items.size > 0"><div class="ua-slot" data-for="it : items"><img data-attr-src="it.icon"/><span class="ua-count" data-if="it.count > 1">{{it.count | num}}</span></div></div>
<span class="ua-label" data-if="items.size == 0">Відкрийте «Речі» (Alt+E) — тут з'являться предмети.</span>
</panel>
<tab>Список</tab><panel><div class="ua-list"><p data-for="line : lines">{{line}}</p></div></panel>
<tab>Кнопки</tab><panel>
<span class="ua-label">Кнопки</span><span class="ua-button">Звичайна</span><span class="ua-button">Ще одна</span>
<span class="ua-label">Перемикачі</span>
<div><input type="checkbox" checked="checked"/>Увімкнено</div>
<div><input type="checkbox"/>Вимкнено</div>
<span class="ua-label">Смуга (HP персонажа)</span><div class="ua-bar"><div class="ua-fill" data-style-width="hp_pct + '%'"></div></div>
</panel>
</tabset>
</body></rml>)";

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

// The window's close button: the window hidden until the switch is turned again
class Close final : public Rml::EventListener {
public:
    void ProcessEvent(Rml::Event& e) override {
        State& s = state();
        if (Rml::ElementDocument* d = e.GetCurrentElement()->GetOwnerDocument()) {
            d->Hide();
            if (d == s.demo) s.demoClosed = true;
            LOG_INFO("uaui: the showcase closed by its ×");
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
        LOG_ERROR("uaui: no drawing context for the words - the card stays off");
        return false;
    }
    Rml::SetSystemInterface(&g_system);
    Rml::SetRenderInterface(&g_render);
    static Fonts fonts;
    g_fonts = &fonts;
    Rml::SetFontEngineInterface(g_fonts);
    Rml::SetFileInterface(&g_files);  // 3.0.18-beta.9: the theme and the window's template
    if (!Rml::Initialise()) {
        s.failed = true;
        LOG_ERROR("uaui: RmlUi did not start - the card stays off");
        return false;
    }
    s.context = Rml::CreateContext("uaui", {std::max(s.targetW, 1), std::max(s.targetH, 1)});
    if (!s.context) {
        s.failed = true;
        LOG_ERROR("uaui: RmlUi made no context - the card stays off");
        return false;
    }
    Rml::DataModelConstructor m = s.context->CreateDataModel("uaui");
    // 3.0.18-beta.8: the numbers as the interface writes them - «5 157» (core/char_block grouped)
    m.RegisterTransformFunc("num", [](const Rml::VariantList& a) -> Rml::Variant {
        return a.empty() ? Rml::Variant() : Rml::Variant(charblock::grouped(a[0].Get<int64_t>()));
    });
    m.Bind("name", &s.name);
    m.Bind("base", &s.base);
    m.Bind("job", &s.job);
    m.Bind("hp", &s.hp);
    m.Bind("hp_most", &s.hpMost);
    m.Bind("sp", &s.sp);
    m.Bind("sp_most", &s.spMost);
    m.Bind("hp_pct", &s.hpPct);
    m.Bind("sp_pct", &s.spPct);
    s.model = m.GetModelHandle();
    s.doc = s.context->LoadDocumentFromMemory(kCard);
    if (!s.doc) {
        s.failed = true;
        LOG_ERROR("uaui: the card's document did not load - the card stays off");
        return false;
    }
    // 3.0.18-beta.9 (stage 2): the components' showcase
    Rml::DataModelConstructor d = s.context->CreateDataModel("demo");
    if (auto item = d.RegisterStruct<ItemView>()) {
        item.RegisterMember("icon", &ItemView::icon);
        item.RegisterMember("count", &ItemView::count);
    }
    d.RegisterArray<std::vector<ItemView>>();
    d.RegisterArray<std::vector<Rml::String>>();
    d.Bind("items", &s.items);
    d.Bind("lines", &s.lines);
    d.Bind("hp_pct", &s.demoHp);
    s.demoModel = d.GetModelHandle();
    for (int i = 1; i <= 60; ++i)
        s.lines.push_back("Рядок " + std::to_string(i) + " — коліщатко миші гортає список, смуга праворуч тягнеться");
    s.demo = s.context->LoadDocumentFromMemory(kDemo);
    if (s.demo) {
        if (Rml::Element* t = s.demo->GetElementById("ua-title")) t->SetInnerRML(s.demo->GetTitle());
        if (Rml::Element* h = s.demo->GetElementById("ua-hint"))
            h->SetInnerRML("Заголовок тягне вікно · × ховає · вигляд — uaro-graphics\\ui\\theme.rcss");
        if (Rml::Element* c = s.demo->GetElementById("ua-close")) c->AddEventListener(Rml::EventId::Click, &g_close);
    } else {
        LOG_INFO("uaui: the showcase's document did not load");
    }
    themeChanged(s);  // its stamp now: read again only when it changes
    s.card = s.doc->GetElementById("card");
    if (s.card) {
        s.card->AddEventListener(Rml::EventId::Dragstart, &g_drag);
        s.card->AddEventListener(Rml::EventId::Drag, &g_drag);
        s.card->AddEventListener(Rml::EventId::Dragend, &g_drag);
    }
    s.started = true;
    LOG_INFO("uaui: RmlUi %s started - its context %dx%d, %.2f target pixels a concept pixel; the theme %s",
             Rml::GetVersion().c_str(), s.targetW, s.targetH, double(uistyle::partScale(s.k)),
             s.themeFile ? themeFile().c_str() : "built in");
    return true;
}

}  // namespace

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    State& s = state();
    s.k = k;
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    const bool want = on && inGameWorld() && k > 0 && targetWidth > 0 && targetHeight > 0;
    if (!want) {
        if (s.shown) uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Uaui, nullptr, nullptr, false);
        if (s.shown && s.doc) {
            s.doc->Hide();
            if (s.card) s.card->SetClass("away", true);
            if (s.demo) s.demo->Hide(), s.demo->SetClass("away", true);
            LOG_INFO("uaui: the card hidden");
        }
        if (!on) s.demoClosed = false;  // the switch turned again: the showcase back
        s.shown = false;
        return;
    }
    if (!start(s)) return;
    const double t0 = clockNow();
    s.context->SetDimensions({targetWidth, targetHeight});
    s.context->SetDensityIndependentPixelRatio(uistyle::partScale(k));
    if (!s.shown) {
        s.doc->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
        if (s.demo && !s.demoClosed) s.demo->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
        s.shown = true;
        s.fadeAsked = false;
        LOG_INFO("uaui: the card shown");
    } else if (!s.fadeAsked && s.card) {  // a frame after showing: the fade in runs (RCSS transition)
        s.card->SetClass("away", false);
        if (s.demo) s.demo->SetClass("away", false);
        s.fadeAsked = true;
    }
    const ULONGLONG nowMs = GetTickCount64();
    // 3.0.18-beta.9: the theme's file read again within a second of a change - every document's style sheets anew
    if (nowMs - s.themeAt >= 1000) {
        s.themeAt = nowMs;
        if (themeChanged(s)) {
            Rml::Factory::ClearStyleSheetCache();
            Rml::Factory::ClearTemplateCache();
            s.doc->ReloadStyleSheet();
            if (s.demo) s.demo->ReloadStyleSheet();
            LOG_INFO("uaui: the theme read again - %s", s.themeFile ? themeFile().c_str() : "built in");
        }
    }
    // 3.0.18-beta.9: the bag as «Речі» last read it, its icons as textures - twice a second
    if (s.demo && nowMs - s.itemsAt >= 500) {
        s.itemsAt = nowMs;
        std::vector<ItemView> now;
        for (const auto& it : itemswin::lastBag()) {
            if (now.size() >= 60) break;
            char src[32];
            std::snprintf(src, sizeof src, "icon:%llx", (unsigned long long)uintptr_t(it.icon));
            now.push_back({it.icon ? src : "", it.count});
        }
        bool same = now.size() == s.items.size();
        for (size_t i = 0; same && i < now.size(); ++i)
            same = now[i].icon == s.items[i].icon && now[i].count == s.items[i].count;
        if (!same) s.items = std::move(now), s.demoModel.DirtyVariable("items");
    }
    // the character's numbers: only what changed is laid out again
    charstatus::Status st;
    if (readOwnStatus(st)) {
        const auto set = [&](int& field, int value, const char* name) {
            if (field != value) field = value, s.model.DirtyVariable(name);
        };
        set(s.base, st.baseLevel, "base");
        set(s.job, st.jobLevel, "job");
        set(s.hp, st.hp, "hp");
        set(s.hpMost, st.hpMost, "hp_most");
        set(s.sp, st.sp, "sp");
        set(s.spMost, st.spMost, "sp_most");
        const float hp = st.hpMost > 0 ? 100.0f * float(st.hp) / float(st.hpMost) : 0.0f;
        const float sp = st.spMost > 0 ? 100.0f * float(st.sp) / float(st.spMost) : 0.0f;
        if (hp != s.hpPct) s.hpPct = hp, s.model.DirtyVariable("hp_pct");
        if (hp != s.demoHp) s.demoHp = hp, s.demoModel.DirtyVariable("hp_pct");
        if (sp != s.spPct) s.spPct = sp, s.model.DirtyVariable("sp_pct");
    }
    const std::string name = utf8From(readOwnName(), codePage);
    if (name != s.name) s.name = name, s.model.DirtyVariable("name");
    int cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) s.context->ProcessMouseMove(int(float(cx) * float(k)), int(float(cy) * float(k)), 0);
    s.context->Update();
    // 3.0.18-beta.10 (the owner: «когда мышкой колесиком скролю мир увеличивается и уменьшается»): the window under
    // the game's cursor is a window for the game (win/world_mouse) - its wheel, clicks and the world's hover stay ours
    {
        Rml::Element* hover = s.context->GetHoverElement();
        Rml::ElementDocument* owner = hover ? hover->GetOwnerDocument() : nullptr;
        Rml::Element* box = !hover                                    ? nullptr
                            : owner == s.doc && hover != s.doc        ? s.card
                            : s.demo && owner == s.demo               ? static_cast<Rml::Element*>(s.demo)
                                                                      : nullptr;
        RECT client{};
        if (box && GetClientRect(game, &client)) {
            const Rml::Vector2f at = box->GetAbsoluteOffset(Rml::BoxArea::Border);
            const Rml::Vector2f size = box->GetBox().GetSize(Rml::BoxArea::Border);
            const RECT r{LONG(int64_t(at.x) * client.right / targetWidth),
                         LONG(int64_t(at.y) * client.bottom / targetHeight),
                         LONG(int64_t(at.x + size.x) * client.right / targetWidth + 1),
                         LONG(int64_t(at.y + size.y) * client.bottom / targetHeight + 1)};
            uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Uaui, game, &r, false);
        } else {
            uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Uaui, nullptr, nullptr, false);
        }
    }
    const double t1 = clockNow();
    s.batches.clear();
    s.context->Render();
    const double t2 = clockNow();
    s.stats.update += t1 - t0;
    s.stats.render += t2 - t1;
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    for (uint32_t id : s.released) renderer.releaseTexture(id);
    s.released.clear();
    if (!s.shown || s.batches.empty()) return;
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
    if (!s.shown || !s.context) return false;
    const bool down = m == WM_LBUTTONDOWN, up = m == WM_LBUTTONUP, wheel = m == WM_MOUSEWHEEL;
    if (!down && !up && !wheel) return false;
    float tx = 0, ty = 0;
    if (!pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty)) return false;
    s.context->ProcessMouseMove(int(tx), int(ty), 0);
    Rml::Element* hover = s.context->GetHoverElement();
    // over the card (its screen-wide page itself is not), or anywhere on the showcase's window
    const Rml::ElementDocument* owner = hover ? hover->GetOwnerDocument() : nullptr;
    const bool over = hover && ((owner == s.doc && hover != s.doc) || (s.demo && owner == s.demo));
    if (!over && !(up && s.dragging) && !(up && s.context->IsMouseInteracting())) return false;
    if (wheel) s.context->ProcessMouseWheel(-float(GET_WHEEL_DELTA_WPARAM(wp)) / float(WHEEL_DELTA), 0);
    else if (down) s.context->ProcessMouseButtonDown(0, 0);
    else s.context->ProcessMouseButtonUp(0, 0);
    result = 0;
    return true;
}

}  // namespace uagfx::win::uaui

#include "win/uaui.hpp"

#include <RmlUi/Core.h>

#include <algorithm>
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
    bool started = false, failed = false, on = false, shown = false, dragging = false;
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

const char* kCard = R"(<rml><head><style>
body { width: 100%; height: 100%; font-family: Plex; font-size: 14dp; color: #e9edf2; }
#card { position: absolute; left: 50%; top: 96dp; margin-left: -160dp; width: 320dp; padding: 12dp 14dp;
        border-radius: 8dp; border: 1dp #ffffff22; background-color: #161b22e0; drag: drag; cursor: move;
        transition: opacity transform background-color border-color 0.16s quadratic-out; }
#card.away { opacity: 0; transform: scale(0.96); }
#card:hover { background-color: #1e2633f0; border-color: #3d7fd8aa; }
.name { font-weight: bold; font-size: 15dp; }
.sub { color: #9aa4b2; font-size: 12dp; margin-left: 8dp; }
.row { display: block; margin-top: 8dp; }
.num { font-size: 12dp; color: #c9d1d9; }
.bar { display: block; height: 8dp; border-radius: 4dp; background-color: #2a313c; margin-top: 4dp; }
.fill { display: block; height: 8dp; border-radius: 4dp; }
.hp { background-color: #3fb950; }
.sp { background-color: #388bfd; }
.tag { display: block; margin-top: 10dp; font-size: 11dp; color: #7d8590; }
</style></head><body data-model="uaui">
<div id="card" class="away">
<div><span class="name">{{name}}</span><span class="sub">Base {{base}} · Job {{job}}</span></div>
<div class="row"><span class="num">HP {{hp | num}} / {{hp_most | num}}</span><div class="bar"><div class="fill hp" data-style-width="hp_pct + '%'"></div></div></div>
<div class="row"><span class="num">SP {{sp | num}} / {{sp_most | num}}</span><div class="bar"><div class="fill sp" data-style-width="sp_pct + '%'"></div></div></div>
<span class="tag">uaUI · RmlUi 6.3</span>
</div></body></rml>)";

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
    s.card = s.doc->GetElementById("card");
    if (s.card) {
        s.card->AddEventListener(Rml::EventId::Dragstart, &g_drag);
        s.card->AddEventListener(Rml::EventId::Drag, &g_drag);
        s.card->AddEventListener(Rml::EventId::Dragend, &g_drag);
    }
    s.started = true;
    LOG_INFO("uaui: RmlUi %s started - its context %dx%d, %.2f target pixels a concept pixel", Rml::GetVersion().c_str(),
             s.targetW, s.targetH, double(uistyle::partScale(s.k)));
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
        if (s.shown && s.doc) {
            s.doc->Hide();
            if (s.card) s.card->SetClass("away", true);
            LOG_INFO("uaui: the card hidden");
        }
        s.shown = false;
        return;
    }
    if (!start(s)) return;
    (void)game;
    const double t0 = clockNow();
    s.context->SetDimensions({targetWidth, targetHeight});
    s.context->SetDensityIndependentPixelRatio(uistyle::partScale(k));
    if (!s.shown) {
        s.doc->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
        s.shown = true;
        s.fadeAsked = false;
        LOG_INFO("uaui: the card shown");
    } else if (!s.fadeAsked && s.card) {  // a frame after showing: the fade in runs (RCSS transition)
        s.card->SetClass("away", false);
        s.fadeAsked = true;
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
        if (sp != s.spPct) s.spPct = sp, s.model.DirtyVariable("sp_pct");
    }
    const std::string name = utf8From(readOwnName(), codePage);
    if (name != s.name) s.name = name, s.model.DirtyVariable("name");
    int cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) s.context->ProcessMouseMove(int(float(cx) * float(k)), int(float(cy) * float(k)), 0);
    s.context->Update();
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

bool mouse(HWND game, UINT m, WPARAM, LPARAM lp, LRESULT& result) {
    State& s = state();
    if (!s.shown || !s.context) return false;
    const bool down = m == WM_LBUTTONDOWN, up = m == WM_LBUTTONUP;
    if (!down && !up) return false;
    float tx = 0, ty = 0;
    if (!pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty)) return false;
    s.context->ProcessMouseMove(int(tx), int(ty), 0);
    Rml::Element* hover = s.context->GetHoverElement();
    const bool over = hover && hover != s.doc && hover->GetOwnerDocument() == s.doc;
    if (!over && !(up && s.dragging)) return false;
    if (down) s.context->ProcessMouseButtonDown(0, 0);
    else s.context->ProcessMouseButtonUp(0, 0);
    result = 0;
    return true;
}

}  // namespace uagfx::win::uaui

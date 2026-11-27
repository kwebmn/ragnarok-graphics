#include "rml_host.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <map>
#include <set>
#include <vector>
#include <cstring>

#include "core/ui_theme_css.hpp"
#include "uikit/doc_sources.hpp"
#include "uikit/applying.hpp"

namespace rmlhost {
namespace {

std::string g_log;
Work g_work;  // 3.0.21-beta.8: what the frame under way cost
std::map<std::string, int> g_loads;  // 3.0.21-beta.14: the textures' loads by source
std::set<std::string> g_failing;     // and the sources whose loads fail

class System final : public Rml::SystemInterface {
public:
    double GetElapsedTime() override { return 0; }
    void JoinPath(Rml::String& out, const Rml::String& document, const Rml::String& path) override {
        if (path.rfind("uaui:", 0) == 0 || path.rfind("icon:", 0) == 0 || path.rfind("pic:", 0) == 0) {
            out = path;
            return;
        }
        Rml::SystemInterface::JoinPath(out, document, path);
    }
    bool LogMessage(Rml::Log::Type type, const Rml::String& message) override {
        // the driver's own decorators (win/uaui registers them) are not made here
        const bool ours = message.find("ua-shadow") != std::string::npos;
        if (type <= Rml::Log::LT_WARNING && !ours) g_log += message + "\n";
        return true;
    }
};

class Render final : public Rml::RenderInterface {
    struct Placed {
        float x = 0, y = 0;
        bool cut = false;
        Rml::Rectanglei by;
    };
    std::map<Rml::CompiledGeometryHandle, Placed> placed_;  // each geometry as the frame before drew it
    Rml::CompiledGeometryHandle next_ = 1;
    bool cut_ = false;
    Rml::Rectanglei by_;

public:
    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex>, Rml::Span<const int>) override {
        ++g_work.compiled;
        return next_++;
    }
    void RenderGeometry(Rml::CompiledGeometryHandle g, Rml::Vector2f t, Rml::TextureHandle) override {
        ++g_work.drawn;
        const auto it = placed_.find(g);
        const bool same = it != placed_.end() && it->second.x == t.x && it->second.y == t.y &&
                          it->second.cut == cut_ && (!cut_ || it->second.by == by_);
        if (!same) ++g_work.moved;
        placed_[g] = Placed{t.x, t.y, cut_, by_};
    }
    void ReleaseGeometry(Rml::CompiledGeometryHandle g) override { placed_.erase(g); }
    Rml::TextureHandle LoadTexture(Rml::Vector2i& dimensions, const Rml::String& source) override {
        ++g_loads[source];  // 3.0.21-beta.14: each source's loads counted, the failing ones refused
        if (g_failing.count(source)) {
            dimensions = {0, 0};
            return 0;
        }
        dimensions = {32, 32};
        return 1;
    }
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte>, Rml::Vector2i) override { return 1; }
    void ReleaseTexture(Rml::TextureHandle) override {}
    void EnableScissorRegion(bool on) override { cut_ = on; }
    void SetScissorRegion(Rml::Rectanglei r) override { by_ = r; }
};

const uagfx::uitheme::Theme* g_theme = nullptr;  // 3.0.24-beta.33

class Files final : public Rml::FileInterface {
    struct Open {
        std::string data;
        size_t at = 0;
    };

public:
    Rml::FileHandle Open(const Rml::String& path) override {
        if (path.rfind("uaui://", 0) == 0) {  // 3.0.24-beta.33: a document's own markup (its style sheet's reload)
            std::string rml;
            if (!uagfx::uikit::docsources::find(path, rml)) return 0;
            auto* f = new struct Open;
            f->data = rml;
            return Rml::FileHandle(f);
        }
        if (path != "uaui:theme.rcss") return 0;
        auto* f = new struct Open;
        f->data = g_theme ? uagfx::uitheme::themedCss(*g_theme, uagfx::uitheme::builtInCss())
                          : uagfx::uitheme::builtInCss();
        return Rml::FileHandle(f);
    }
    void Close(Rml::FileHandle h) override { delete reinterpret_cast<struct Open*>(h); }
    size_t Read(void* buffer, size_t size, Rml::FileHandle h) override {
        auto* f = reinterpret_cast<struct Open*>(h);
        const size_t n = std::min(size, f->data.size() - f->at);
        std::memcpy(buffer, f->data.data() + f->at, n);
        f->at += n;
        return n;
    }
    bool Seek(Rml::FileHandle h, long offset, int origin) override {
        auto* f = reinterpret_cast<struct Open*>(h);
        const long base = origin == SEEK_SET ? 0 : origin == SEEK_CUR ? long(f->at) : long(f->data.size());
        if (base + offset < 0 || size_t(base + offset) > f->data.size()) return false;
        f->at = size_t(base + offset);
        return true;
    }
    size_t Tell(Rml::FileHandle h) override { return reinterpret_cast<struct Open*>(h)->at; }
};

class Fonts final : public Rml::FontEngineInterface {
public:
    Rml::FontFaceHandle GetFontFaceHandle(const Rml::String&, Rml::Style::FontStyle, Rml::Style::FontWeight weight,
                                          int size) override {
        const int key = size * 2 + (int(weight) >= 600 ? 1 : 0);
        Rml::FontMetrics& m = metrics_[key];
        m.size = size;
        m.ascent = 0.8f * float(size);
        m.descent = 0.2f * float(size);
        m.line_spacing = 1.2f * float(size);
        m.x_height = 0.45f * float(size);
        m.underline_position = 0.1f * float(size);
        m.underline_thickness = 1;
        return Rml::FontFaceHandle(key);
    }
    const Rml::FontMetrics& GetFontMetrics(Rml::FontFaceHandle h) override { return metrics_[int(h)]; }
    int GetStringWidth(Rml::FontFaceHandle h, Rml::StringView text, const Rml::TextShapingContext&,
                       Rml::Character) override {
        ++g_work.measured;
        const int key = int(h);
        int letters = 0;
        for (const char* c = text.begin(); c != text.end(); ++c)
            if ((static_cast<unsigned char>(*c) & 0xC0) != 0x80) ++letters;
        return int(float(letters) * float(key / 2) * (key % 2 ? 0.6f : 0.55f) + 0.5f);
    }
    int GenerateString(Rml::RenderManager&, Rml::FontFaceHandle h, Rml::FontEffectsHandle, Rml::StringView text,
                       Rml::Vector2f, Rml::ColourbPremultiplied, float, const Rml::TextShapingContext& c,
                       Rml::TexturedMeshList&) override {
        ++g_work.words;
        return GetStringWidth(h, text, c, Rml::Character::Null);
    }
    int GetVersion(Rml::FontFaceHandle) override { return 1; }

private:
    std::map<int, Rml::FontMetrics> metrics_;
};

System g_system;
Render g_render;
Files g_files;
Fonts g_fonts;
Rml::Context* g_context = nullptr;
int g_models = 0;
std::vector<Rml::DataModelHandle> g_handles;  // dirtied whole by update(), as win/uaui's parts do

}  // namespace

Rml::Context* context(int w, int h) {
    if (!g_context) {
        Rml::SetSystemInterface(&g_system);
        Rml::SetFileInterface(&g_files);
        Rml::SetFontEngineInterface(&g_fonts);
        Rml::SetRenderInterface(&g_render);
        Rml::Initialise();
        g_context = Rml::CreateContext("host", Rml::Vector2i(w, h));
    }
    g_context->SetDimensions(Rml::Vector2i(w, h));
    return g_context;
}

Rml::ElementDocument* load(const std::string& rml, const std::function<void(Rml::DataModelConstructor&)>& bind,
                           float x, float y, std::string* error) {
    Rml::Context* c = context();
    const std::string model = "m" + std::to_string(++g_models);
    Rml::DataModelConstructor m = c->CreateDataModel(model);
    if (bind) bind(m);
    g_handles.push_back(m.GetModelHandle());
    std::string text = rml;
    const size_t body = text.find("<body");
    if (body == std::string::npos) return nullptr;
    text.insert(body + 5, " data-model=\"" + model + "\"");
    const size_t before = g_log.size();
    uagfx::uikit::docsources::keep("uaui://" + model, text);  // 3.0.24-beta.33: as win/uaui - its style sheet's reload
    Rml::ElementDocument* d = c->LoadDocumentFromMemory(text, "uaui://" + model);
    if (!d) {
        if (error) *error = g_log.substr(before);
        return nullptr;
    }
    d->SetProperty(Rml::PropertyId::Left, Rml::Property(x, Rml::Unit::PX));
    d->SetProperty(Rml::PropertyId::Top, Rml::Property(y, Rml::Unit::PX));
    d->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
    {
        const uagfx::uikit::Applying applying;  // as win/uaui: the models' values reach the elements here
        for (int i = 0; i < 4; ++i) c->Update();  // the data views, then the layout of what they showed
    }
    if (error) *error = g_log.substr(before);
    return d;
}

void update() {
    for (Rml::DataModelHandle& h : g_handles) h.DirtyAllVariables();
    const uagfx::uikit::Applying applying;
    for (int i = 0; i < 4; ++i) context()->Update();
}

Work frame() {
    g_work = Work{};
    {
        const uagfx::uikit::Applying applying;
        context()->Update();
    }
    context()->Render();
    return g_work;
}

void failTextures(const std::set<std::string>& sources) { g_failing = sources; }
int textureLoads(const std::string& source) {
    const auto it = g_loads.find(source);
    return it == g_loads.end() ? 0 : it->second;
}

void close(Rml::ElementDocument* doc) {
    if (doc) uagfx::uikit::docsources::forget(doc->GetSourceURL());
    if (doc) doc->Close();
    context()->Update();
}

const std::string& log() { return g_log; }

void theme(const uagfx::uitheme::Theme* t) {
    g_theme = t;
    Rml::Factory::ClearStyleSheetCache();
}

}  // namespace rmlhost

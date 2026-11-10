#include "rml_host.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <map>
#include <cstring>

#include "core/ui_theme_css.hpp"

namespace rmlhost {
namespace {

std::string g_log;

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
        const bool ours =
            message.find("ua-glass") != std::string::npos || message.find("ua-shadow") != std::string::npos;
        if (type <= Rml::Log::LT_WARNING && !ours) g_log += message + "\n";
        return true;
    }
};

class Render final : public Rml::RenderInterface {
public:
    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex>, Rml::Span<const int>) override {
        return 1;
    }
    void RenderGeometry(Rml::CompiledGeometryHandle, Rml::Vector2f, Rml::TextureHandle) override {}
    void ReleaseGeometry(Rml::CompiledGeometryHandle) override {}
    Rml::TextureHandle LoadTexture(Rml::Vector2i& dimensions, const Rml::String&) override {
        dimensions = {32, 32};
        return 1;
    }
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte>, Rml::Vector2i) override { return 1; }
    void ReleaseTexture(Rml::TextureHandle) override {}
    void EnableScissorRegion(bool) override {}
    void SetScissorRegion(Rml::Rectanglei) override {}
};

class Files final : public Rml::FileInterface {
    struct Open {
        std::string data;
        size_t at = 0;
    };

public:
    Rml::FileHandle Open(const Rml::String& path) override {
        if (path != "uaui:theme.rcss") return 0;
        auto* f = new struct Open;
        f->data = uagfx::uitheme::builtInCss();
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
        const int key = int(h);
        int letters = 0;
        for (const char* c = text.begin(); c != text.end(); ++c)
            if ((static_cast<unsigned char>(*c) & 0xC0) != 0x80) ++letters;
        return int(float(letters) * float(key / 2) * (key % 2 ? 0.6f : 0.55f) + 0.5f);
    }
    int GenerateString(Rml::RenderManager&, Rml::FontFaceHandle h, Rml::FontEffectsHandle, Rml::StringView text,
                       Rml::Vector2f, Rml::ColourbPremultiplied, float, const Rml::TextShapingContext& c,
                       Rml::TexturedMeshList&) override {
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
    std::string text = rml;
    const size_t body = text.find("<body");
    if (body == std::string::npos) return nullptr;
    text.insert(body + 5, " data-model=\"" + model + "\"");
    const size_t before = g_log.size();
    Rml::ElementDocument* d = c->LoadDocumentFromMemory(text, "uaui://" + model);
    if (!d) {
        if (error) *error = g_log.substr(before);
        return nullptr;
    }
    d->SetProperty(Rml::PropertyId::Left, Rml::Property(x, Rml::Unit::PX));
    d->SetProperty(Rml::PropertyId::Top, Rml::Property(y, Rml::Unit::PX));
    d->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
    c->Update();
    c->Update();
    if (error) *error = g_log.substr(before);
    return d;
}

void update() {
    context()->Update();
    context()->Update();
}

void close(Rml::ElementDocument* doc) {
    if (doc) doc->Close();
    context()->Update();
}

const std::string& log() { return g_log; }

}  // namespace rmlhost

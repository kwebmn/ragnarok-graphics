#pragma once
#include <windows.h>

// windowsx.h's macros under RmlUi's names (Element::GetNextSibling ...), where a file has it before this
#undef GetFirstChild
#undef GetFirstSibling
#undef GetLastSibling
#undef GetNextSibling
#undef GetPrevSibling
#include <RmlUi/Core.h>

#include <functional>
#include <string>

#include "core/frame.hpp"
#include "core/window_panel.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::uaui {

// 3.0.18-beta.7 (the owner: «давай сделаем проверку RmlUi»; Claude Doc «Концепция: единая библиотека интерфейса
// uaRO Graphics»): RmlUi 6.3 (third_party/rmlui, MIT) in the driver - its geometry as the game's own vertices through
// the bridge (core/uaui_mesh), its words from a GDI glyph atlas in IBM Plex Sans (win/ui_font), its clock, files and
// log ours, one theme (built in, or uaro-graphics\ui\theme.rcss read again within a second of a change).
// 3.0.18-beta.11 (the owner: «реализовывай полностью всю концепцию»; docs/superpowers/plans/2026-11-09-uaui-
// migration.md): the runtime the parts draw by - [interface] uaui. Each frame: begin() before the parts' frames,
// the parts fill their documents' models, end() lays out and renders; draw() puts it into the frame. On the game's
// thread.
void begin(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight,
           const std::string& theme);
void end();
void draw(FrameRecord& record, gpu::Renderer& renderer);
// The mouse's buttons and wheel over a document of uaUI (and the end of a drag wherever it is): true taken
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);

bool ready();  // [interface] uaui on, in the world, started: the parts draw by uaUI this frame
// 3.0.18-beta.12 (the ruling of docs/superpowers/plans/2026-11-09-uaui-migration.md): the parts' recorded pictures
// (panel::Picture::scene, core/ui_scene) drawn on the card - [interface] uaui on and started (in the world or not)
bool sceneReady();
// Whether a recorded picture has pixels painted by hand as well (a glow, a mask) - kept by its scene's generation
bool residual(const panel::Picture& pic);
// 3.0.18-beta.13: a picture drawn while recording but not recorded (a part painting without panel::blank) - in the
// log every 10 s, by its size, to find the parts still painted
void noteUnrecorded(int w, int h);
// Its shapes, icons, pictures and words at (x, y) w x h of the target (stretched from its own size), at `opacity`
void drawScene(FrameRecord& record, gpu::Renderer& renderer, const panel::Picture& pic, int x, int y, int w, int h,
               float k, float opacity, int stretch);
float dp();    // the target's pixels a concept pixel (the theme's 1dp)
std::string utf8(const std::string& gameText);  // the game's words (its code page) as UTF-8
// A picture of the driver's as an <img> source - «pic:<address>:<generation>»; another generation is another texture
// (the one of `previous` let go when it changes)
std::string picture(const panel::Picture* p, int generation, std::string& previous);
// 3.0.18-beta.19: a picture's content, roughly (its size and every 97th pixel) - a generation for picture() when its
// maker draws it again in place
int stamp(const panel::Picture* p);

// One document of a part: its body the part's box, its model (named as the document) bound by the part
class Doc {
public:
    // Made once, while ready(): false if RmlUi refused it (told in the log)
    bool make(const char* name, const char* rml, const std::function<void(Rml::DataModelConstructor&)>& bind);
    bool made() const { return doc_ != nullptr; }
    void show(bool on);  // shown with the theme's fade in (its body's class «away» taken off the next frame)
    bool shown() const { return shown_; }
    void dirty(const char* variable) { model_.DirtyVariable(variable); }
    void dirtyAll() { model_.DirtyAllVariables(); }
    void place(int x, int y);  // its box's top left in the target's pixels
    bool box(int& x, int& y, int& w, int& h) const;  // false while not shown
    Rml::ElementDocument* doc() const { return doc_; }
    Rml::Element* byId(const char* id) const { return doc_ ? doc_->GetElementById(id) : nullptr; }
    std::function<void()> onClose;  // its × (the window template's), besides hiding it

    // the runtime's
    bool fadePending = false;

private:
    Rml::ElementDocument* doc_ = nullptr;
    Rml::DataModelHandle model_;
    bool shown_ = false;
};

}  // namespace uagfx::win::uaui

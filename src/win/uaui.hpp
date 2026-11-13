#pragma once
#include <climits>
#include <map>
#include <vector>
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
// 3.0.18-beta.27: the parts' documents (Hud) where the parts are drawn, the windows' (Doc::window) where the windows
// are - over the panels, the chat and the hotbar
// .38: World - under the parts, over the world; .69: Top - over everything of the driver's (the veil, its notes),
// Cursor - the interface's cursor
enum class Layer : uint8_t { Hud, Windows, World, Top, Cursor };
void draw(FrameRecord& record, gpu::Renderer& renderer, Layer layer = Layer::Hud);
// The mouse's buttons and wheel over a document of uaUI (and the end of a drag wherever it is): true taken
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);

bool ready();  // [interface] uaui on, in the world, started: the parts draw by uaUI this frame
// 3.0.18-beta.33: on and started, in the world or not - the login's, the servers' and the characters' windows
// (their documents Doc::anywhere; the rest hidden outside the world)
bool readyAnywhere();
// 3.0.18-beta.54: the theme ([interface] theme, ui\palette.txt) for the colours and corners the parts give in code
uint32_t themed(uint32_t rgb);  // 0xRRGGBB
float themeRadius();
float themeEdge();
// 3.0.18-beta.43: the windows' stack drawing its windows bottom up - the place of the one being drawn now (its
// document raised to it: the last opened or clicked on top, as the driver's windows)
void setWindowRank(int rank, bool marked = false);
int windowRank();
bool windowMarked();
void setMotion(bool on);  // 3.0.18-beta.58: [interface] uaui_motion - the theme's soft motion (body class «motion»)
bool motion();
void setEffects(bool on);
void setGlass(bool on);  // 3.0.18-beta.64: [interface] uaui_glass (body class «glassy»; Direct3D 12 only)  // 3.0.18-beta.61: [interface] uaui_effects (body class «fx»)
HWND gameWindow();  // 3.0.18-beta.60
double targetScale();  // the target's pixels a game pixel
void targetSize(int& w, int& h);  // 3.0.18-beta.92: the target's pixels
bool key(UINT m, WPARAM wp, LPARAM lp);  // 3.0.18-beta.59: true - a field of uaUI's took it  // the one drawn now has the stack's gold edge (the top of two or more)
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
// 3.0.18-beta.57: the words' width as uaUI draws them at `px` (< 0: not started)
float textWidth(const std::string& utf8, float px, bool bold);
float dp();    // the target's pixels a concept pixel (the theme's 1dp)
  // the game's words (its code page) as UTF-8
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
    // 3.0.18-beta.172: each one counted - the log's uaUI line tells the parts that change their data the most.
    // 3.0.18-beta.242 (R-F8FGR4): nothing before make() - a part that sets its model's words before its document is
    // made (the reference's first open: its search's words emptied) wrote through RmlUi's empty handle (a read at 0x28);
    // the document reads the bound model as it is when made
    void dirty(const char* variable) {
        if (!model_) return;
        ++dirties;
        model_.DirtyVariable(variable);
    }
    void dirtyAll() {
        if (!model_) return;
        ++dirtyAlls;
        model_.DirtyAllVariables();
    }
    void place(int x, int y);  // its box's top left in the target's pixels (3.0.18-beta.70: set only when it moves)
    // 3.0.18-beta.70 (the big six, C): a property of its body set only when its value changes
    void prop(const std::string& name, const std::string& value);
    void raise(int z);  // 3.0.18-beta.43: its z-index (a window's: 10 + its place in the stack)
    void mark(bool on);  // 3.0.18-beta.44: its body's class «marked» (the theme's gold edge)
    bool box(int& x, int& y, int& w, int& h) const;  // false while not shown
    // 3.0.18-beta.152 (the owner: every window sized by its corner): the whole document scaled from its top left
    // (RCSS transform) - box() and boxesOf() in the target's pixels as drawn; 1 as laid out
    void zoom(float z);
    float zoomed() const { return zoom_; }
    // 3.0.18-beta.73 (the big six, F: input through the library): a window's mouse message given to RmlUi when the
    // pointer is over this document - false otherwise (another's then). `onControl`: over an element of class «ctl»
    // (a button, a cell: its own events) - else over its ground (the part drags the window by it)
    bool pointer(HWND game, UINT m, WPARAM wp, LPARAM lp, bool& onControl);
    // The boxes of its elements matching `selector` (CSS), in its own pixels - the pad's targets
    std::vector<panel::Box> boxesOf(const char* selector) const;
    Rml::ElementDocument* doc() const { return doc_; }
    Rml::Element* byId(const char* id) const { return doc_ ? doc_->GetElementById(id) : nullptr; }
    std::function<void()> onClose;  // its × (the window template's), besides hiding it
    // 3.0.18-beta.21: drawn only - its part keeps its own mouse, keys and blocker (the hotbar's drag and wheel)
    bool passive = false;
    bool window = false;  // 3.0.18-beta.27: set before make() - drawn with the windows (uaui::Layer::Windows)
    bool anywhere = false;  // 3.0.18-beta.33: shown outside the world too (the login's windows)
    bool world = false;     // 3.0.18-beta.38: set before make() - drawn with the world's overlays (uaui::Layer::World)
    bool top = false;       // 3.0.18-beta.69: set before make() - over everything (uaui::Layer::Top)
    bool cursor = false;    // 3.0.18-beta.69: set before make() - the cursor's own (uaui::Layer::Cursor)

    // the runtime's
    void lookAtFile();  // 3.0.18-beta.59: uaro-graphics\ui\<name>.rml changed - loaded over the built-in markup
    bool fadePending = false;
    unsigned wantedAt = 0;  // 3.0.18-beta.40: the frame show(true) was last asked in
    unsigned dirties = 0, dirtyAlls = 0;  // 3.0.18-beta.172: since the uaUI line's last count

private:
    Rml::ElementDocument* doc_ = nullptr;
    Rml::DataModelHandle model_;
    bool shown_ = false;
    int z_ = -1;
    int marked_ = -1;
    int px_ = INT_MIN, py_ = INT_MIN;  // 3.0.18-beta.70: where it was placed
    bool pressedIn_ = false;           // 3.0.18-beta.82: its left button's press taken (its release is its own)
    float zoom_ = 1;                   // 3.0.18-beta.152
    std::map<std::string, std::string> props_;
    // 3.0.18-beta.59: its markup - the built-in one, its model's name; the player's file's time (0 none)
    std::string builtin_, model_name_;
    std::string shipped_;  // 3.0.18-beta.71: the admin's markup from the updates ("" none)
    unsigned long long fileStamp_ = 0;
    bool load(const std::string& rml);
public:
    std::string name;  // 3.0.18-beta.46: its part's (the log's)
};

}  // namespace uagfx::win::uaui

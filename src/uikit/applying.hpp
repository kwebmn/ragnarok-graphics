#pragma once

namespace uagfx::uikit {

// 3.0.21-beta.9 (R-MSAM96: «Налаштування» wrote sliders into uaro-graphics.ini that no hand had moved): RmlUi sends
// an element's «change» not only when a person changes it - also while a model's values reach the element in
// Context::Update (a slider's new least or most cuts the value it still holds from the row that stood there before).
// Whoever calls Context::Update holds an Applying for the call (win/uaui, the host's tests); a window's handler asks
// applying() and takes no such event for the person's.
struct Applying {
    Applying();
    ~Applying();
    Applying(const Applying&) = delete;
    Applying& operator=(const Applying&) = delete;
};
bool applying();

}  // namespace uagfx::uikit

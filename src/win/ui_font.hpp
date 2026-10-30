#pragma once
#include <windows.h>

#include <string>

namespace uagfx::win::uifont {

// 3.0.11-beta.66 (the concept: «Шрифт — встроенный IBM Plex Sans в обоих клиентах (им нарисованы все макеты, лицензия
// SIL OFL). Segoe UI, которым драйвер пишет сейчас, — запасной для Windows»): the new interface's words in IBM Plex
// Sans - IBM's own TTFs, unchanged, in ddraw.dll (src/win/ui_font.rc, third_party/plex; their license in
// uaro-graphics\licenses) - added for the game's process alone (AddFontMemResourceEx) at the first word. A word with a
// letter Plex has not (★ ● ▾ ✕ ⚠ ...) in Segoe UI as before; all of them when Plex cannot be added.
struct Face {
    const wchar_t* name;  // CreateFontW's face
    int weight;
};
// The face for a word: Plex Regular, its SemiBold for the bold (GDI names it «IBM Plex Sans SmBld»), else Segoe UI
Face faceFor(const std::wstring& text, bool bold);

}  // namespace uagfx::win::uifont

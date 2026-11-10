#include "win/ui_field.hpp"

#include <string>

#include "win/code_page.hpp"

namespace uagfx::win::uifield {

std::string clipboardText() {
    std::string out;
    if (!OpenClipboard(nullptr)) return out;
    if (HANDLE h = GetClipboardData(CF_UNICODETEXT)) {
        if (const auto* w = static_cast<const wchar_t*>(GlobalLock(h))) {
            out = utf8Of(std::wstring(w));
            GlobalUnlock(h);
        }
    }
    CloseClipboard();
    return out;
}

void setClipboard(const std::string& utf8) {
    const std::wstring w = widened(utf8, CP_UTF8);
    if (!OpenClipboard(nullptr)) return;
    EmptyClipboard();
    if (HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, (w.size() + 1) * sizeof(wchar_t))) {
        if (auto* p = static_cast<wchar_t*>(GlobalLock(g))) {
            std::copy(w.begin(), w.end(), p);
            p[w.size()] = 0;
            GlobalUnlock(g);
            if (!SetClipboardData(CF_UNICODETEXT, g)) GlobalFree(g);
        } else {
            GlobalFree(g);
        }
    }
    CloseClipboard();
}

bool key(textfield::Field& f, UINT m, WPARAM wp, size_t most, Result& result) {
    result = Result::None;
    const bool ctrl = GetKeyState(VK_CONTROL) < 0, alt = GetKeyState(VK_MENU) < 0;
    if (m == WM_KEYDOWN || m == WM_SYSKEYDOWN) {
        if (alt) return false;  // Alt+E and the rest: the game's
        switch (wp) {
            case VK_ESCAPE:
            case VK_RETURN: result = Result::Done; return true;
            case VK_BACK:
                if (textfield::backspace(f)) result = Result::Changed;
                return true;
            case VK_DELETE:
                if (textfield::clear(f)) result = Result::Changed;
                return true;
            default: break;
        }
        if (ctrl && wp == 'A') f.all = true;
        if (ctrl && (wp == 'C' || wp == 'X') && !f.text.empty()) {
            setClipboard(f.text);
            if (wp == 'X' && textfield::clear(f)) result = Result::Changed;
        }
        if (ctrl && wp == 'V' && textfield::type(f, textfield::oneLine(clipboardText()), most))
            result = Result::Changed;
        return true;
    }
    if (m == WM_CHAR) {
        if (!ctrl && wp >= 32 && wp != 127 && textfield::type(f, typedUtf8(unsigned(wp)), most))
            result = Result::Changed;
        return true;
    }
    return m == WM_KEYUP || m == WM_DEADCHAR || m == WM_SYSKEYUP;
}

}  // namespace uagfx::win::uifield

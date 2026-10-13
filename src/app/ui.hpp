#pragma once
#include <windows.h>

#include <commctrl.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace uagfx::app {

int windowDpi(HWND hwnd);  // GetDpiForWindow on Windows 10, else the screen's
inline int px(int at96, int dpi) { return MulDiv(at96, dpi, 96); }
HFONT makeFont(int dpi, bool bold, int percent = 100);  // Windows' message font at that DPI

enum class Tone { Normal, Muted, Good, Warn, Bad };
COLORREF toneColor(Tone t);

// The controls of one page (stage 2 spec §6), laid out from the top in rows: a label on the left, the control on the
// right, notes under it across the page. They are children of the main window, so Tab goes through all of them.
class Panel {
public:
    Panel(HWND owner, HFONT font, HFONT heading, HWND tooltip, int dpi, RECT area);
    ~Panel();
    Panel(const Panel&) = delete;
    Panel& operator=(const Panel&) = delete;

    HWND heading(const std::wstring& text);
    HWND check(int id, const std::wstring& text, bool on, const std::wstring& hint, bool enabled = true);
    // A radio button in the control column; `label` (the group's name) goes to the label column of its row.
    HWND radio(int id, const std::wstring& label, const std::wstring& text, bool on, bool first, const std::wstring& hint,
               bool enabled = true);
    HWND choice(int id, const std::wstring& label, const std::vector<std::wstring>& items, int selected,
                const std::wstring& hint, bool enabled = true);
    // A trackbar with its number on the right; the number is control id + 1.
    HWND slider(int id, const std::wstring& label, int lo, int hi, int step, int value, const std::wstring& valueText,
                const std::wstring& hint, bool enabled = true);
    HWND number(int id, const std::wstring& label, const std::wstring& value, const std::wstring& hint,
                bool enabled = true);
    // Two numbers with "×" between them: ids id and id + 1.
    HWND pair(int id, const std::wstring& label, int a, int b, const std::wstring& hint, bool enabled = true);
    HWND note(const std::wstring& text, Tone tone = Tone::Muted);
    HWND buttons(const std::vector<std::pair<int, std::wstring>>& list, bool enabled = true);
    HWND listBox(int id, int rows);
    HWND listView(int id, const std::vector<std::pair<std::wstring, int>>& columns, int rows);
    void gap(int at96 = 8);
    bool toneOf(HWND ctl, Tone& tone) const;

private:
    HWND make(const wchar_t* cls, const std::wstring& text, DWORD style, int x, int y, int w, int h, int id,
              DWORD ex = 0);
    HWND label(const std::wstring& text, int height);
    void tip(HWND ctl, const std::wstring& hint);
    int textHeight(const std::wstring& text, int width) const;
    int at(int v) const { return px(v, dpi_); }

    HWND owner_;
    HFONT font_;
    HFONT heading_;
    HWND tooltip_;
    int dpi_;
    RECT area_;
    int y_;
    std::vector<HWND> made_;
    std::map<HWND, Tone> tones_;
};

}  // namespace uagfx::app

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

bool copyToClipboard(HWND owner, const std::wstring& text);

enum class Tone { Normal, Muted, Good, Warn, Bad, Accent };  // 2.6: Accent - the "changed" dots
COLORREF toneColor(Tone t);

struct CheckItem {  // a check box of Panel::checks
    int id;
    std::wstring text;
    bool on;
    std::wstring hint;
    bool enabled;
};

struct ButtonItem {  // a button of Panel::buttons
    int id;
    std::wstring text;
    bool enabled = true;
};

struct LineItem {  // a text field of Panel::lines
    int id;
    std::wstring label;
    std::wstring value;
    std::wstring cue;  // shown grey while it is empty
};

// The controls of one page (stage 2 spec §6), laid out from the top in rows: a label on the left, the control on the
// right, notes under it across the page. They are children of the page view (0.22.1: it scrolls them), a control
// parent of the main window, so Tab goes through all of them.
class Panel {
public:
    // taken: the letters of the window's own buttons (Alt+letter): the page's controls take others.
    Panel(HWND owner, HFONT font, HFONT heading, HWND tooltip, int dpi, RECT area, std::wstring taken);
    ~Panel();
    Panel(const Panel&) = delete;
    Panel& operator=(const Panel&) = delete;

    HWND heading(const std::wstring& text);
    // nested (0.23.2): in the control column, under the box it belongs to.
    HWND check(int id, const std::wstring& text, bool on, const std::wstring& hint, bool enabled = true,
               bool nested = false);
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
    // Two numbers with "×" (or `between`) between them: ids id and id + 1.
    HWND pair(int id, const std::wstring& label, int a, int b, const std::wstring& hint, bool enabled = true,
              const std::wstring& between = L"×");
    void checks(const std::vector<CheckItem>& items, int perRow = 2);  // two (or perRow) to a row
    // Text fields two to a row, a short label before each (the chat macros); at most maxChars letters.
    void lines(const std::vector<LineItem>& items, int maxChars, const std::wstring& hint, bool enabled);
    // 0.51: one text field the width of the page under its label (the report's comment)
    HWND line(int id, const std::wstring& label, const std::wstring& cue, int maxChars, const std::wstring& hint);
    HWND note(const std::wstring& text, Tone tone = Tone::Muted);
    HWND buttons(const std::vector<ButtonItem>& list);
    HWND listBox(int id, int rows);
    HWND progress(int id);  // 0.43: a bar the width of the page, 0-1000
    // 1.6: pictures side by side across the page (BMP files, each scaled into a square), a caption under each;
    // 1.6.1: zoom 2, 4, 8 - each picture's middle that much larger, pixel by pixel, perRow to a row.
    void images(const std::vector<std::pair<std::wstring, std::wstring>>& files, int zoom = 1, int perRow = 0);
    // checkboxes: a check box before every row (LVS_EX_CHECKBOXES).
    HWND listView(int id, const std::vector<std::pair<std::wstring, int>>& columns, int rows, bool checkboxes = false);
    void gap(int at96 = 8);
    // 2.6: two columns when the page is wide - rows go into the left one until nextColumn(), then into the right one
    // from the same top; join() goes on under the taller one, across the page. False and one column when either
    // would be narrower than minColumn96: nextColumn() and join() change nothing then.
    bool split(int minColumn96 = 400);
    void nextColumn();
    void join();
    // 2.6: tabs - push-like radio buttons in a row (ids firstId + n), the selected one pressed.
    void tabs(int firstId, const std::vector<std::wstring>& names, int selected);
    // 2.6: a dot in the accent colour in the margin left of a row's control: its value is not the recommended one.
    HWND dot(HWND row, const std::wstring& hint);
    size_t madeCount() const { return made_.size(); }
    HWND madeAt(size_t i) const { return made_[i]; }
    bool toneOf(HWND ctl, Tone& tone) const;
    // How tall the page is: from the area's top to below its last row (the scrolled page view's range).
    int height() const { return (column_ == 1 && leftBottom_ > y_ ? leftBottom_ : y_) - area_.top; }
    // The hints over disabled controls follow their controls after the page view scrolled them.
    void moveTips();

private:
    HWND make(const wchar_t* cls, const std::wstring& text, DWORD style, int x, int y, int w, int h, int id,
              DWORD ex = 0, bool group = true);
    HWND label(const std::wstring& text, int height, bool letter = true);
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
    std::vector<HBITMAP> bitmaps_;  // 1.6: images'; freed with the panel
    std::vector<HWND> tipped_;  // the controls with a hint over their rectangle too (tip())
    std::map<HWND, Tone> tones_;
    std::wstring taken_;  // the letters for Alt+letter taken so far
    RECT full_{};         // 2.6: the area across the page while it is split
    int splitTop_ = 0;
    int leftBottom_ = 0;  // under the left column's last row
    int column_ = -1;     // -1 not split, 0 the left column, 1 the right one
};

}  // namespace uagfx::app

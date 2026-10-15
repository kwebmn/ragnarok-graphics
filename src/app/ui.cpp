#include "app/ui.hpp"

#include <algorithm>
#include <cstring>
#include <utility>

#include "app/mnemonic.hpp"

namespace uagfx::app {
namespace {

constexpr int kLabel = 200;  // the label column, at 96 DPI
constexpr int kRow = 26;
constexpr int kGap = 6;

// A grey control's text: no letter of its own (Alt+one would send the focus elsewhere), its '&' shown as it is.
std::wstring literal(const std::wstring& text) {
    std::wstring out;
    for (wchar_t ch : text) {
        out += ch;
        if (ch == L'&') out += L'&';
    }
    return out;
}

}  // namespace

int windowDpi(HWND hwnd) {
    using Fn = UINT(WINAPI*)(HWND);
    static const Fn fn =
        reinterpret_cast<Fn>(reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow")));
    if (fn && hwnd) return int(fn(hwnd));
    HDC dc = GetDC(nullptr);
    const int dpi = GetDeviceCaps(dc, LOGPIXELSY);
    ReleaseDC(nullptr, dc);
    return dpi;
}

HFONT makeFont(int dpi, bool bold, int percent) {
    NONCLIENTMETRICSW m{};
    m.cbSize = sizeof m;
    using Fn = BOOL(WINAPI*)(UINT, UINT, PVOID, UINT, UINT);
    static const Fn forDpi = reinterpret_cast<Fn>(
        reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "SystemParametersInfoForDpi")));
    LOGFONTW lf{};
    if (forDpi && forDpi(SPI_GETNONCLIENTMETRICS, sizeof m, &m, 0, UINT(dpi))) {
        lf = m.lfMessageFont;
    } else if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof m, &m, 0)) {
        lf = m.lfMessageFont;
        HDC dc = GetDC(nullptr);
        const int screen = GetDeviceCaps(dc, LOGPIXELSY);
        ReleaseDC(nullptr, dc);
        lf.lfHeight = MulDiv(lf.lfHeight, dpi, screen);
    } else {
        lf.lfHeight = -MulDiv(9, dpi, 72);
        lstrcpyW(lf.lfFaceName, L"Segoe UI");
    }
    lf.lfHeight = MulDiv(lf.lfHeight, percent, 100);
    if (bold) lf.lfWeight = FW_SEMIBOLD;
    return CreateFontIndirectW(&lf);
}

COLORREF toneColor(Tone t) {
    switch (t) {
        case Tone::Muted: return RGB(96, 96, 96);
        case Tone::Good: return RGB(16, 124, 16);
        case Tone::Warn: return RGB(157, 93, 0);
        case Tone::Bad: return RGB(196, 43, 28);
        case Tone::Normal: break;
    }
    return GetSysColor(COLOR_WINDOWTEXT);
}

Panel::Panel(HWND owner, HFONT font, HFONT heading, HWND tooltip, int dpi, RECT area, std::wstring taken)
    : owner_(owner), font_(font), heading_(heading), tooltip_(tooltip), dpi_(dpi), area_(area), y_(area.top),
      taken_(std::move(taken)) {}

Panel::~Panel() {
    for (HWND h : made_) {
        if (tooltip_) {  // both of tip()'s tools
            TOOLINFOW ti{};
            ti.cbSize = sizeof ti;
            ti.hwnd = owner_;
            ti.uId = reinterpret_cast<UINT_PTR>(h);
            SendMessageW(tooltip_, TTM_DELTOOLW, 0, reinterpret_cast<LPARAM>(&ti));
            ti.uId = UINT_PTR(GetDlgCtrlID(h));
            SendMessageW(tooltip_, TTM_DELTOOLW, 0, reinterpret_cast<LPARAM>(&ti));
        }
        DestroyWindow(h);
    }
}

// Every control starts a group of its own, but a radio button after its group's first: the arrows go through a
// group, and a group of radio buttons ends at the next control (0.6.0 review M6).
HWND Panel::make(const wchar_t* cls, const std::wstring& text, DWORD style, int x, int y, int w, int h, int id,
                 DWORD ex, bool group) {
    HWND c = CreateWindowExW(ex, cls, text.c_str(), WS_CHILD | WS_VISIBLE | style | (group ? WS_GROUP : 0), x, y, w,
                             h, owner_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr),
                             nullptr);
    SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    made_.push_back(c);
    return c;
}

void Panel::tip(HWND ctl, const std::wstring& hint) {
    if (!tooltip_ || hint.empty()) return;
    TOOLINFOW ti{};
    ti.cbSize = sizeof ti;
    ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;  // the control's own mouse, while it is enabled
    ti.hwnd = owner_;
    ti.uId = reinterpret_cast<UINT_PTR>(ctl);
    ti.lpszText = const_cast<wchar_t*>(hint.c_str());
    SendMessageW(tooltip_, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&ti));
    // A disabled control gets no mouse - its parent does, over its rectangle: the same hint there (0.6.0 review M6).
    TOOLINFOW area{};
    area.cbSize = sizeof area;
    area.uFlags = TTF_SUBCLASS;
    area.hwnd = owner_;
    area.uId = UINT_PTR(GetDlgCtrlID(ctl));
    GetWindowRect(ctl, &area.rect);
    MapWindowPoints(nullptr, owner_, reinterpret_cast<POINT*>(&area.rect), 2);
    area.lpszText = const_cast<wchar_t*>(hint.c_str());
    SendMessageW(tooltip_, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&area));
    tipped_.push_back(ctl);
}

void Panel::moveTips() {
    if (!tooltip_) return;
    for (HWND ctl : tipped_) {
        TOOLINFOW area{};
        area.cbSize = sizeof area;
        area.hwnd = owner_;
        area.uId = UINT_PTR(GetDlgCtrlID(ctl));
        GetWindowRect(ctl, &area.rect);
        MapWindowPoints(nullptr, owner_, reinterpret_cast<POINT*>(&area.rect), 2);
        SendMessageW(tooltip_, TTM_NEWTOOLRECTW, 0, reinterpret_cast<LPARAM>(&area));
    }
}

int Panel::textHeight(const std::wstring& text, int width) const {
    HDC dc = GetDC(owner_);
    HGDIOBJ old = SelectObject(dc, font_);
    RECT r{0, 0, width, 0};
    DrawTextW(dc, text.c_str(), int(text.size()), &r, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    SelectObject(dc, old);
    ReleaseDC(owner_, dc);
    return r.bottom - r.top;
}

// A row's name. With a letter (Alt+it) it sends the focus to the control after it (stage 2 spec §6).
HWND Panel::label(const std::wstring& text, int height, bool letter) {
    return make(L"STATIC", letter ? withMnemonic(text, taken_) : text,
                SS_LEFT | SS_CENTERIMAGE | (letter ? 0 : SS_NOPREFIX), area_.left, y_, at(kLabel) - at(8), height, 0);
}

HWND Panel::heading(const std::wstring& text) {
    const int h = at(28);
    HWND c = make(L"STATIC", text, SS_LEFT | SS_NOPREFIX, area_.left, y_, area_.right - area_.left, h, 0);
    SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(heading_), TRUE);
    y_ += h + at(kGap);
    return c;
}

HWND Panel::check(int id, const std::wstring& text, bool on, const std::wstring& hint, bool enabled, bool nested) {
    const int x = area_.left + (nested ? at(kLabel) : 0);
    HWND c = make(L"BUTTON", enabled ? withMnemonic(text, taken_) : literal(text), BS_AUTOCHECKBOX | WS_TABSTOP, x,
                  y_, area_.right - x, at(kRow), id);
    SendMessageW(c, BM_SETCHECK, on ? BST_CHECKED : BST_UNCHECKED, 0);
    EnableWindow(c, enabled);
    tip(c, hint);
    y_ += at(kRow) + at(kGap);
    return c;
}

HWND Panel::radio(int id, const std::wstring& text0, const std::wstring& text, bool on, bool first,
                  const std::wstring& hint, bool enabled) {
    if (!text0.empty()) label(text0, at(kRow), false);  // the group's name: the buttons have letters of their own
    HWND c = make(L"BUTTON", enabled ? withMnemonic(text, taken_) : literal(text), BS_AUTORADIOBUTTON | WS_TABSTOP,
                  area_.left + at(kLabel), y_, area_.right - area_.left - at(kLabel), at(kRow), id, 0, first);
    SendMessageW(c, BM_SETCHECK, on ? BST_CHECKED : BST_UNCHECKED, 0);
    EnableWindow(c, enabled);
    tip(c, hint);
    y_ += at(kRow);
    return c;
}

HWND Panel::choice(int id, const std::wstring& text, const std::vector<std::wstring>& items, int selected,
                   const std::wstring& hint, bool enabled) {
    label(text, at(kRow), enabled);
    HWND c = make(WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, area_.left + at(kLabel), y_,
                  area_.right - area_.left - at(kLabel), at(300), id);
    for (const std::wstring& item : items) SendMessageW(c, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item.c_str()));
    SendMessageW(c, CB_SETCURSEL, WPARAM(selected), 0);
    EnableWindow(c, enabled);
    tip(c, hint);
    y_ += at(kRow) + at(kGap);
    return c;
}

HWND Panel::slider(int id, const std::wstring& text, int lo, int hi, int step, int value, const std::wstring& valueText,
                   const std::wstring& hint, bool enabled) {
    label(text, at(kRow + 4), enabled);
    const int width = area_.right - area_.left - at(kLabel) - at(70);
    HWND c = make(TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_AUTOTICKS | WS_TABSTOP, area_.left + at(kLabel), y_, width,
                  at(kRow + 4), id);
    SendMessageW(c, TBM_SETRANGE, TRUE, MAKELPARAM(lo, hi));
    const int tenth = std::max(step, (hi - lo) / 10);  // ticks and a page: a tenth of the range, at least a step
    SendMessageW(c, TBM_SETTICFREQ, WPARAM(tenth), 0);
    SendMessageW(c, TBM_SETLINESIZE, 0, LPARAM(step));
    SendMessageW(c, TBM_SETPAGESIZE, 0, LPARAM(std::max(step * 2, tenth)));
    SendMessageW(c, TBM_SETPOS, TRUE, LPARAM(value));
    EnableWindow(c, enabled);
    tip(c, hint);
    make(L"STATIC", valueText, SS_LEFT | SS_CENTERIMAGE | SS_NOPREFIX, area_.left + at(kLabel) + width + at(8), y_,
         at(62), at(kRow + 4), id + 1);
    y_ += at(kRow + 4) + at(kGap);
    return c;
}

HWND Panel::number(int id, const std::wstring& text, const std::wstring& value, const std::wstring& hint, bool enabled) {
    label(text, at(kRow), enabled);
    HWND c = make(L"EDIT", value, ES_NUMBER | ES_AUTOHSCROLL | WS_TABSTOP, area_.left + at(kLabel), y_, at(90), at(kRow),
                  id, WS_EX_CLIENTEDGE);
    EnableWindow(c, enabled);
    tip(c, hint);
    y_ += at(kRow) + at(kGap);
    return c;
}

HWND Panel::pair(int id, const std::wstring& text, int a, int b, const std::wstring& hint, bool enabled,
                 const std::wstring& between) {
    label(text, at(kRow), enabled);
    const int x = area_.left + at(kLabel);
    HWND c = make(L"EDIT", std::to_wstring(a), ES_NUMBER | ES_AUTOHSCROLL | WS_TABSTOP, x, y_, at(80), at(kRow), id,
                  WS_EX_CLIENTEDGE);
    make(L"STATIC", between, SS_CENTER | SS_CENTERIMAGE | SS_NOPREFIX, x + at(80), y_, at(24), at(kRow), 0);
    HWND d = make(L"EDIT", std::to_wstring(b), ES_NUMBER | ES_AUTOHSCROLL | WS_TABSTOP, x + at(104), y_, at(80),
                  at(kRow), id + 1, WS_EX_CLIENTEDGE);
    EnableWindow(c, enabled);
    EnableWindow(d, enabled);
    tip(c, hint);
    tip(d, hint);
    y_ += at(kRow) + at(kGap);
    return c;
}

void Panel::checks(const std::vector<CheckItem>& items, int perRow) {
    const size_t n = size_t(std::max(perRow, 1));
    const int width = (area_.right - area_.left - at(12) * int(n - 1)) / int(n);
    for (size_t i = 0; i < items.size(); ++i) {
        const CheckItem& c = items[i];
        HWND h = make(L"BUTTON", c.enabled ? withMnemonic(c.text, taken_) : literal(c.text),
                      BS_AUTOCHECKBOX | WS_TABSTOP, area_.left + int(i % n) * (width + at(12)), y_, width, at(kRow),
                      c.id);
        SendMessageW(h, BM_SETCHECK, c.on ? BST_CHECKED : BST_UNCHECKED, 0);
        EnableWindow(h, c.enabled);
        tip(h, c.hint);
        if (i % n == n - 1 || i + 1 == items.size()) y_ += at(kRow) + at(kGap);
    }
}

void Panel::lines(const std::vector<LineItem>& items, int maxChars, const std::wstring& hint, bool enabled) {
    const int half = (area_.right - area_.left - at(12)) / 2;
    const int name = at(48);
    for (size_t i = 0; i < items.size(); ++i) {
        const LineItem& l = items[i];
        const int x = area_.left + (i % 2 ? half + at(12) : 0);
        make(L"STATIC", l.label, SS_LEFT | SS_CENTERIMAGE | SS_NOPREFIX, x, y_, name, at(kRow), 0);
        HWND e = make(L"EDIT", l.value, ES_AUTOHSCROLL | WS_TABSTOP, x + name, y_, half - name, at(kRow), l.id,
                      WS_EX_CLIENTEDGE);
        SendMessageW(e, EM_LIMITTEXT, WPARAM(maxChars), 0);
        SendMessageW(e, EM_SETCUEBANNER, FALSE, reinterpret_cast<LPARAM>(l.cue.c_str()));
        EnableWindow(e, enabled);
        tip(e, hint);
        if (i % 2 == 1 || i + 1 == items.size()) y_ += at(kRow) + at(kGap);
    }
}

HWND Panel::note(const std::wstring& text, Tone tone) {
    const int width = area_.right - area_.left;
    const int h = textHeight(text, width);
    HWND c = make(L"STATIC", text, SS_LEFT | SS_NOPREFIX, area_.left, y_, width, h, 0);
    tones_[c] = tone;
    y_ += h + at(kGap);
    return c;
}

HWND Panel::buttons(const std::vector<ButtonItem>& list) {
    HWND first = nullptr;
    int x = area_.left;
    HDC dc = GetDC(owner_);
    HGDIOBJ old = SelectObject(dc, font_);
    for (const auto& b : list) {
        SIZE s{};
        GetTextExtentPoint32W(dc, b.text.c_str(), int(b.text.size()), &s);
        const int w = s.cx + at(28);
        HWND c = make(L"BUTTON", b.enabled ? withMnemonic(b.text, taken_) : literal(b.text),
                      BS_PUSHBUTTON | WS_TABSTOP, x, y_, w, at(kRow + 4), b.id);
        EnableWindow(c, b.enabled);
        if (!first) first = c;
        x += w + at(8);
    }
    SelectObject(dc, old);
    ReleaseDC(owner_, dc);
    y_ += at(kRow + 4) + at(kGap);
    return first;
}

HWND Panel::progress(int id) {
    const int h = at(16);
    HWND c = make(PROGRESS_CLASSW, L"", 0, area_.left, y_, area_.right - area_.left, h, id, 0, false);
    SendMessageW(c, PBM_SETRANGE32, 0, 1000);
    y_ += h + at(kGap);
    return c;
}

HWND Panel::listBox(int id, int rows) {
    const int h = at(rows * 20 + 8);
    HWND c = make(L"LISTBOX", L"", LBS_NOTIFY | WS_VSCROLL | WS_TABSTOP | LBS_NOINTEGRALHEIGHT, area_.left, y_,
                  area_.right - area_.left, h, id, WS_EX_CLIENTEDGE);
    y_ += h + at(kGap);
    return c;
}

HWND Panel::listView(int id, const std::vector<std::pair<std::wstring, int>>& columns, int rows, bool checkboxes) {
    const int h = at(rows * 22 + 28);
    HWND c = make(WC_LISTVIEWW, L"", LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER | WS_TABSTOP,
                  area_.left, y_, area_.right - area_.left, h, id, WS_EX_CLIENTEDGE);
    SendMessageW(c, LVM_SETEXTENDEDLISTVIEWSTYLE, 0,
                 LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP | (checkboxes ? LVS_EX_CHECKBOXES : 0));
    for (size_t i = 0; i < columns.size(); ++i) {
        LVCOLUMNW col{};
        col.mask = LVCF_TEXT | LVCF_WIDTH;
        col.pszText = const_cast<wchar_t*>(columns[i].first.c_str());
        col.cx = at(columns[i].second);
        SendMessageW(c, LVM_INSERTCOLUMNW, WPARAM(i), reinterpret_cast<LPARAM>(&col));
    }
    y_ += h + at(kGap);
    return c;
}

void Panel::gap(int at96) { y_ += at(at96); }

bool Panel::toneOf(HWND ctl, Tone& tone) const {
    const auto it = tones_.find(ctl);
    if (it == tones_.end()) return false;
    tone = it->second;
    return true;
}

bool copyToClipboard(HWND owner, const std::wstring& text) {
    if (!OpenClipboard(owner)) return false;
    EmptyClipboard();
    const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, bytes);
    bool ok = false;
    if (h) {
        if (void* p = GlobalLock(h)) {
            std::memcpy(p, text.c_str(), bytes);
            GlobalUnlock(h);
            ok = SetClipboardData(CF_UNICODETEXT, h) != nullptr;
        }
        if (!ok) GlobalFree(h);
    }
    CloseClipboard();
    return ok;
}

}  // namespace uagfx::app

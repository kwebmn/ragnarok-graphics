#include "studio/make_dialog.hpp"

#include <commctrl.h>

#include <algorithm>

#include "app/text.hpp"

namespace uagfx::studio {
namespace {

constexpr const wchar_t* kClass = L"uaroStudioMake";
const char* const kModels[] = {"general", "anime", "anime6b"};

enum Id : int {
    kModel = 500, kScale, kOwnDenoise, kDenoise, kDeblock, kKey, kTone, kGrain, kSharpen, kAlpha, kBlend, kOriginal,
    kTiles, kGo, kCancelButton
};

struct Dialog {
    settings::Lang lang;
    HFONT font;
    int dpi;
    atlas_tiles::Grid grid;
    ai::Chain chain;
    HWND hwnd = nullptr;
    bool done = false, made = false;
    HWND get(int id) const { return GetDlgItem(hwnd, id); }
    bool checked(int id) const { return SendMessageW(get(id), BM_GETCHECK, 0, 0) == BST_CHECKED; }
    int pos(int id) const { return int(SendMessageW(get(id), TBM_GETPOS, 0, 0)); }
    void take() {  // the controls into the chain
        chain.model = kModels[std::clamp(int(SendMessageW(get(kModel), CB_GETCURSEL, 0, 0)), 0, 2)];
        chain.scale = SendMessageW(get(kScale), CB_GETCURSEL, 0, 0) == 1 ? 2 : 4;
        chain.denoise = checked(kOwnDenoise) ? pos(kDenoise) : -1;
        chain.deblock = checked(kDeblock);
        chain.repairKey = checked(kKey);
        chain.matchTone = checked(kTone);
        chain.grain = float(pos(kGrain)) / 100.0f;
        chain.sharpen = float(pos(kSharpen)) / 100.0f;
        chain.cleanAlpha = checked(kAlpha);
        const int blend = int(SendMessageW(get(kBlend), CB_GETCURSEL, 0, 0));
        chain.blendModel = blend >= 1 && blend <= 3 ? kModels[blend - 1] : "";
        chain.original = float(pos(kOriginal)) / 100.0f;
        chain.tiles = (grid.cols > 1 || grid.rows > 1) && checked(kTiles);
    }
    void enable() { EnableWindow(get(kDenoise), checked(kOwnDenoise)); }
};

LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* d = reinterpret_cast<Dialog*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
        case WM_NCCREATE:
            SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                              reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams));
            break;
        case WM_COMMAND:
            if (!d) break;
            if (LOWORD(wp) == kGo || LOWORD(wp) == IDOK) {
                d->take();
                d->made = true;
                d->done = true;
            } else if (LOWORD(wp) == kCancelButton || LOWORD(wp) == IDCANCEL) {
                d->done = true;
            } else if (LOWORD(wp) == kOwnDenoise) {
                d->enable();
            }
            return 0;
        case WM_CLOSE:
            if (d) d->done = true;
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace

bool askChain(HWND owner, settings::Lang lang, HFONT font, int dpi, const std::wstring& what, atlas_tiles::Grid grid,
              ai::Chain& c) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = kClass;
    RegisterClassExW(&wc);
    Dialog d{lang, font, dpi, grid, c};
    const auto at = [dpi](int v) { return MulDiv(v, dpi, 96); };
    const int width = at(460), rowH = at(24), gap = at(6);
    RECT owned{};
    GetWindowRect(owner, &owned);
    const std::wstring title = app::wtrf(lang, settings::Text::StudioMakeTitle, {app::utf8(what)});
    d.hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME, kClass, title.c_str(), WS_POPUP | WS_CAPTION | WS_SYSMENU,
                             (owned.left + owned.right) / 2 - width / 2, owned.top + at(80), width, at(560), owner,
                             nullptr, wc.hInstance, &d);
    if (!d.hwnd) return false;
    int y = at(12);
    const int x = at(14), labelW = at(150), fieldX = x + labelW, fieldW = width - fieldX - at(24);
    const auto make = [&](const wchar_t* cls, const std::wstring& text, DWORD style, int id, int cx, int cy, int w,
                          int h) {
        HWND h0 = CreateWindowExW(0, cls, text.c_str(), WS_CHILD | WS_VISIBLE | style, cx, cy, w, h, d.hwnd,
                                  reinterpret_cast<HMENU>(INT_PTR(id)), nullptr, nullptr);
        SendMessageW(h0, WM_SETFONT, WPARAM(font), TRUE);
        return h0;
    };
    const auto t = [lang](settings::Text x0) { return app::wtr(lang, x0); };
    const auto label = [&](settings::Text x0) { make(L"STATIC", t(x0), SS_LEFT, -1, x, y + at(4), labelW, rowH); };
    const auto combo = [&](int id, const std::vector<std::wstring>& items, int sel) {
        HWND h = make(WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, id, fieldX, y, fieldW, at(200));
        for (const std::wstring& s : items) SendMessageW(h, CB_ADDSTRING, 0, LPARAM(s.c_str()));
        SendMessageW(h, CB_SETCURSEL, WPARAM(sel), 0);
        y += rowH + gap;
    };
    const auto check = [&](int id, const std::wstring& text, bool on, int indent = 0) {
        HWND h = make(L"BUTTON", text, BS_AUTOCHECKBOX | WS_TABSTOP, id, x + indent, y, width - x * 2 - indent, rowH);
        SendMessageW(h, BM_SETCHECK, on ? BST_CHECKED : BST_UNCHECKED, 0);
        y += rowH;
    };
    const auto slider = [&](int id, settings::Text name, int most, int value) {
        label(name);
        HWND h = make(TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_AUTOTICKS | WS_TABSTOP, id, fieldX, y, fieldW, rowH);
        SendMessageW(h, TBM_SETRANGE, TRUE, MAKELPARAM(0, most));
        SendMessageW(h, TBM_SETTICFREQ, most / 10, 0);
        SendMessageW(h, TBM_SETPOS, TRUE, value);
        y += rowH + gap;
    };
    label(settings::Text::StudioMakeModel);
    combo(kModel, {L"general", L"anime", L"anime6b"},
          c.model == "anime" ? 1 : c.model == "anime6b" ? 2 : 0);
    label(settings::Text::StudioMakeScale);
    combo(kScale, {L"×4", L"×2"}, c.scale == 2 ? 1 : 0);
    check(kOwnDenoise, t(settings::Text::StudioMakeOwnDenoise), c.denoise >= 0);
    slider(kDenoise, settings::Text::StudioMakeDenoise, 100, c.denoise >= 0 ? c.denoise : 50);
    make(L"STATIC", t(settings::Text::StudioMakeBefore), SS_LEFT, -1, x, y, width - 2 * x, rowH);
    y += rowH;
    check(kDeblock, t(settings::Text::StudioMakeDeblock), c.deblock, at(14));
    check(kKey, t(settings::Text::StudioMakeKey), c.repairKey, at(14));
    y += gap;
    make(L"STATIC", t(settings::Text::StudioMakeAfter), SS_LEFT, -1, x, y, width - 2 * x, rowH);
    y += rowH;
    check(kTone, t(settings::Text::StudioMakeTone), c.matchTone, at(14));
    check(kAlpha, t(settings::Text::StudioMakeAlpha), c.cleanAlpha, at(14));
    slider(kGrain, settings::Text::StudioMakeGrain, 100, int(c.grain * 100.0f + 0.5f));
    slider(kSharpen, settings::Text::StudioMakeSharpen, 100, int(c.sharpen * 100.0f + 0.5f));
    slider(kOriginal, settings::Text::StudioMakeOriginal, 50, int(c.original * 100.0f + 0.5f));
    label(settings::Text::StudioMakeBlend);
    combo(kBlend, {t(settings::Text::StudioNone), L"general", L"anime", L"anime6b"},
          c.blendModel == "general" ? 1 : c.blendModel == "anime" ? 2 : c.blendModel == "anime6b" ? 3 : 0);
    if (grid.cols > 1 || grid.rows > 1)
        check(kTiles,
              app::wtrf(lang, settings::Text::StudioMakeTiles, {std::to_string(grid.cols), std::to_string(grid.rows)}),
              true);
    y += gap;
    make(L"BUTTON", t(settings::Text::StudioMakeGo), BS_DEFPUSHBUTTON | WS_TABSTOP, kGo, width - at(230), y, at(100),
         rowH + at(4));
    make(L"BUTTON", t(settings::Text::StudioCancel), BS_PUSHBUTTON | WS_TABSTOP, kCancelButton, width - at(124), y,
         at(100), rowH + at(4));
    y += rowH + at(4) + at(14);
    RECT frame{0, 0, width, y};
    AdjustWindowRectEx(&frame, WS_POPUP | WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_DLGMODALFRAME);
    SetWindowPos(d.hwnd, nullptr, 0, 0, frame.right - frame.left, frame.bottom - frame.top, SWP_NOMOVE | SWP_NOZORDER);
    d.enable();
    EnableWindow(owner, FALSE);
    ShowWindow(d.hwnd, SW_SHOW);
    MSG msg;
    while (!d.done && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (IsDialogMessageW(d.hwnd, &msg)) continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    EnableWindow(owner, TRUE);
    DestroyWindow(d.hwnd);
    SetActiveWindow(owner);
    if (d.made) c = d.chain;
    return d.made;
}

}  // namespace uagfx::studio

#include "app/chat_view.hpp"

#include <windowsx.h>

#include <algorithm>

#include "app/ui.hpp"

namespace uagfx::app {
namespace {

const wchar_t kChatClass[] = L"uaroGraphicsChat";
const wchar_t kHeadClass[] = L"uaroGraphicsTalkHead";
constexpr UINT kThumb = WM_APP + 22;  // a thumbnail fetched (lParam: a Thumb, the view's to delete)

constexpr COLORREF kGround = RGB(247, 248, 251), kMine = RGB(47, 111, 209), kTheirs = RGB(255, 255, 255);
constexpr COLORREF kEdge = RGB(221, 226, 234), kText = RGB(27, 35, 51), kMuted = RGB(93, 104, 128);
constexpr COLORREF kWaiting = RGB(226, 231, 239), kDone = RGB(29, 107, 58), kDoneLine = RGB(156, 201, 173);
constexpr COLORREF kLater = RGB(107, 119, 144);

struct Thumb {
    unsigned generation = 0;
    size_t item = 0, shot = 0;
    HBITMAP bmp = nullptr;
};

struct Placed {
    RECT head{}, bubble{}, text{};
    std::vector<RECT> shots;
};

struct Chat {
    HFONT font = nullptr, bold = nullptr;
    int dpi = 96;
    std::vector<ChatItem> items;
    std::vector<std::vector<HBITMAP>> thumbs;
    std::vector<Placed> placed;
    unsigned generation = 0;
    int scroll = 0, total = 0;
    ~Chat() { clear(); }
    void clear() {
        for (auto& row : thumbs)
            for (HBITMAP b : row)
                if (b) DeleteObject(b);
        thumbs.clear();
    }
    int at(int v) const { return px(v, dpi); }
};

Chat* chatOf(HWND w) { return reinterpret_cast<Chat*>(GetWindowLongPtrW(w, GWLP_USERDATA)); }

COLORREF mix(COLORREF a, COLORREF b) {
    return RGB((GetRValue(a) + GetRValue(b)) / 2, (GetGValue(a) + GetGValue(b)) / 2, (GetBValue(a) + GetBValue(b)) / 2);
}

void roundBox(HDC dc, const RECT& r, int radius, COLORREF fill, COLORREF edge) {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, edge);
    HGDIOBJ oldBrush = SelectObject(dc, brush), oldPen = SelectObject(dc, pen);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, radius, radius);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void rule(HDC dc, int x0, int x1, int y, COLORREF colour) {
    if (x1 <= x0) return;
    HPEN pen = CreatePen(PS_SOLID, 1, colour);
    HGDIOBJ old = SelectObject(dc, pen);
    MoveToEx(dc, x0, y, nullptr);
    LineTo(dc, x1, y);
    SelectObject(dc, old);
    DeleteObject(pen);
}

void layout(HWND w, Chat& c) {
    RECT client{};
    GetClientRect(w, &client);
    const int width = client.right;
    c.placed.assign(c.items.size(), Placed{});
    HDC dc = GetDC(w);
    HGDIOBJ old = SelectObject(dc, c.font);
    const int pad = c.at(16), gap = c.at(12), padX = c.at(12), padY = c.at(8), headH = c.at(18);
    const SIZE thumb = chatThumbSize(c.dpi);
    const int most = std::max(c.at(160), std::min(width * 72 / 100, c.at(560)));
    int y = pad;
    for (size_t i = 0; i < c.items.size(); ++i) {
        const ChatItem& it = c.items[i];
        Placed& p = c.placed[i];
        if (it.kind == ChatItem::Line) {
            p.text = RECT{pad, y, width - pad, y + c.at(20)};
            y += c.at(20) + gap;
            continue;
        }
        const bool mine = it.kind == ChatItem::Mine;
        p.head = RECT{pad, y, width - pad, y + headH};
        y += headH + c.at(4);
        if (!it.text.empty()) {
            RECT t{0, 0, most - 2 * padX, 0};
            DrawTextW(dc, it.text.c_str(), int(it.text.size()), &t,
                      DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX | DT_EDITCONTROL);
            const int bw = int(t.right - t.left) + 2 * padX, bh = int(t.bottom - t.top) + 2 * padY;
            const int x = mine ? width - pad - bw : pad;
            p.bubble = RECT{x, y, x + bw, y + bh};
            p.text = RECT{x + padX, y + padY, x + bw - padX, y + bh - padY};
            y += bh + c.at(6);
        }
        for (size_t k = 0; k < it.shots; ++k) {
            const int x = mine ? width - pad - thumb.cx : pad;
            p.shots.push_back(RECT{x, y, x + thumb.cx, y + thumb.cy});
            y += thumb.cy + c.at(6);
        }
        y += gap - c.at(6);
    }
    SelectObject(dc, old);
    ReleaseDC(w, dc);
    c.total = y + pad - gap;
}

void setScroll(HWND w, Chat& c) {
    RECT client{};
    GetClientRect(w, &client);
    c.scroll = std::clamp(c.scroll, 0, std::max(0, c.total - int(client.bottom)));
    SCROLLINFO si{};
    si.cbSize = sizeof si;
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMax = std::max(0, c.total - 1);
    si.nPage = UINT(std::max(0L, client.bottom));
    si.nPos = c.scroll;
    SetScrollInfo(w, SB_VERT, &si, TRUE);
}

void paintChat(HWND w, Chat& c) {
    PAINTSTRUCT ps;
    HDC target = BeginPaint(w, &ps);
    RECT client{};
    GetClientRect(w, &client);
    HDC dc = CreateCompatibleDC(target);
    HBITMAP buffer = CreateCompatibleBitmap(target, std::max(1L, client.right), std::max(1L, client.bottom));
    HGDIOBJ oldBuffer = SelectObject(dc, buffer);
    HBRUSH ground = CreateSolidBrush(kGround);
    FillRect(dc, &client, ground);
    DeleteObject(ground);
    SetBkMode(dc, TRANSPARENT);
    HGDIOBJ oldFont = SelectObject(dc, c.font);
    const auto moved = [&](RECT r) {
        OffsetRect(&r, 0, -c.scroll);
        return r;
    };
    for (size_t i = 0; i < c.items.size() && i < c.placed.size(); ++i) {
        const ChatItem& it = c.items[i];
        const Placed& p = c.placed[i];
        if (it.kind == ChatItem::Line) {
            RECT r = moved(p.text);
            if (r.bottom < 0 || r.top > client.bottom) continue;
            SIZE size{};
            GetTextExtentPoint32W(dc, it.text.c_str(), int(it.text.size()), &size);
            const int middle = (r.left + r.right) / 2, half = int(size.cx) / 2, y = (r.top + r.bottom) / 2;
            const COLORREF light = mix(it.tone, kGround);  // the rules beside the words
            rule(dc, std::max(int(r.left), middle - half - c.at(56)), middle - half - c.at(10), y, light);
            rule(dc, middle + half + c.at(10), std::min(int(r.right), middle + half + c.at(56)), y, light);
            SetTextColor(dc, it.tone);
            DrawTextW(dc, it.text.c_str(), int(it.text.size()), &r,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
            continue;
        }
        const bool mine = it.kind == ChatItem::Mine;
        RECT head = moved(p.head);
        SetTextColor(dc, kMuted);
        DrawTextW(dc, it.head.c_str(), int(it.head.size()), &head,
                  (mine ? DT_RIGHT : DT_LEFT) | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
        if (!it.text.empty()) {
            roundBox(dc, moved(p.bubble), c.at(16), mine ? kMine : kTheirs, mine ? kMine : kEdge);
            RECT t = moved(p.text);
            SetTextColor(dc, mine ? RGB(255, 255, 255) : kText);
            DrawTextW(dc, it.text.c_str(), int(it.text.size()), &t, DT_WORDBREAK | DT_NOPREFIX | DT_EDITCONTROL);
        }
        for (size_t k = 0; k < p.shots.size(); ++k) {
            const RECT s = moved(p.shots[k]);
            HBITMAP bmp = i < c.thumbs.size() && k < c.thumbs[i].size() ? c.thumbs[i][k] : nullptr;
            if (bmp) {
                HDC from = CreateCompatibleDC(dc);
                HGDIOBJ old = SelectObject(from, bmp);
                BitBlt(dc, s.left, s.top, s.right - s.left, s.bottom - s.top, from, 0, 0, SRCCOPY);
                SelectObject(from, old);
                DeleteDC(from);
            } else {  // still coming
                HBRUSH waiting = CreateSolidBrush(kWaiting);
                FillRect(dc, &s, waiting);
                DeleteObject(waiting);
            }
            HBRUSH edge = CreateSolidBrush(kEdge);
            FrameRect(dc, &s, edge);
            DeleteObject(edge);
        }
    }
    SelectObject(dc, oldFont);
    BitBlt(target, 0, 0, client.right, client.bottom, dc, 0, 0, SRCCOPY);
    SelectObject(dc, oldBuffer);
    DeleteObject(buffer);
    DeleteDC(dc);
    EndPaint(w, &ps);
}

bool shotAt(const Chat& c, int x, int y, size_t& item, size_t& shot) {
    const POINT p{x, y + c.scroll};
    for (size_t i = 0; i < c.placed.size(); ++i)
        for (size_t k = 0; k < c.placed[i].shots.size(); ++k)
            if (PtInRect(&c.placed[i].shots[k], p)) {
                item = i;
                shot = k;
                return true;
            }
    return false;
}

LRESULT CALLBACK chatProc(HWND w, UINT m, WPARAM wp, LPARAM lp) {
    Chat* c = chatOf(w);
    switch (m) {
        case WM_NCDESTROY:
            delete c;
            SetWindowLongPtrW(w, GWLP_USERDATA, 0);
            break;
        case WM_ERASEBKGND: return 1;
        case WM_PAINT:
            if (!c) break;
            paintChat(w, *c);
            return 0;
        case WM_SIZE: {
            if (!c) break;
            RECT client{};
            GetClientRect(w, &client);
            const bool atBottom = c->scroll + int(client.bottom) >= c->total - c->at(4);
            layout(w, *c);
            if (atBottom) c->scroll = c->total;  // the newest stays in view
            setScroll(w, *c);
            InvalidateRect(w, nullptr, FALSE);
            return 0;
        }
        case WM_VSCROLL: {
            if (!c) break;
            SCROLLINFO si{};
            si.cbSize = sizeof si;
            si.fMask = SIF_ALL;
            GetScrollInfo(w, SB_VERT, &si);
            switch (LOWORD(wp)) {
                case SB_LINEUP: c->scroll -= c->at(40); break;
                case SB_LINEDOWN: c->scroll += c->at(40); break;
                case SB_PAGEUP: c->scroll -= int(si.nPage); break;
                case SB_PAGEDOWN: c->scroll += int(si.nPage); break;
                case SB_THUMBTRACK:
                case SB_THUMBPOSITION: c->scroll = si.nTrackPos; break;
                case SB_TOP: c->scroll = 0; break;
                case SB_BOTTOM: c->scroll = c->total; break;
                default: break;
            }
            setScroll(w, *c);
            InvalidateRect(w, nullptr, FALSE);
            return 0;
        }
        case WM_MOUSEWHEEL:
            if (!c) break;
            c->scroll -= GET_WHEEL_DELTA_WPARAM(wp) * c->at(60) / WHEEL_DELTA;
            setScroll(w, *c);
            InvalidateRect(w, nullptr, FALSE);
            return 0;
        case WM_SETCURSOR: {
            POINT p{};
            size_t item = 0, shot = 0;
            if (c && LOWORD(lp) == HTCLIENT && GetCursorPos(&p) && ScreenToClient(w, &p) &&
                shotAt(*c, p.x, p.y, item, shot)) {
                SetCursor(LoadCursorW(nullptr, MAKEINTRESOURCEW(32649)));  // IDC_HAND: it opens
                return TRUE;
            }
            break;
        }
        case WM_LBUTTONUP: {
            size_t item = 0, shot = 0;
            if (c && shotAt(*c, GET_X_LPARAM(lp), GET_Y_LPARAM(lp), item, shot))
                PostMessageW(GetParent(w), kChatShot, WPARAM(item), LPARAM(shot));
            return 0;
        }
        case kThumb: {
            Thumb* t = reinterpret_cast<Thumb*>(lp);
            if (!t) return 0;
            if (c && t->generation == c->generation && t->item < c->thumbs.size() &&
                t->shot < c->thumbs[t->item].size()) {
                if (c->thumbs[t->item][t->shot]) DeleteObject(c->thumbs[t->item][t->shot]);
                c->thumbs[t->item][t->shot] = t->bmp;
                InvalidateRect(w, nullptr, FALSE);
            } else if (t->bmp) {
                DeleteObject(t->bmp);  // another conversation's by now
            }
            delete t;
            return 0;
        }
        default: break;
    }
    return DefWindowProcW(w, m, wp, lp);
}

struct Head {
    HFONT font = nullptr, bold = nullptr;
    int dpi = 96;
    TalkHead data;
    int at(int v) const { return px(v, dpi); }
};

Head* headOf(HWND w) { return reinterpret_cast<Head*>(GetWindowLongPtrW(w, GWLP_USERDATA)); }

int widthOf(HDC dc, HFONT font, const std::wstring& text) {
    HGDIOBJ old = SelectObject(dc, font);
    SIZE size{};
    GetTextExtentPoint32W(dc, text.c_str(), int(text.size()), &size);
    SelectObject(dc, old);
    return int(size.cx);
}

void paintHead(HWND w, Head& h) {
    PAINTSTRUCT ps;
    HDC target = BeginPaint(w, &ps);
    RECT client{};
    GetClientRect(w, &client);
    HDC dc = CreateCompatibleDC(target);
    HBITMAP buffer = CreateCompatibleBitmap(target, std::max(1L, client.right), std::max(1L, client.bottom));
    HGDIOBJ oldBuffer = SelectObject(dc, buffer);
    FillRect(dc, &client, reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
    SetBkMode(dc, TRANSPARENT);
    const TalkHead& d = h.data;
    // the title, the status's chip after it
    const int chipW = d.chip.empty() ? 0 : widthOf(dc, h.bold, d.chip) + h.at(18);
    const int titleRoom = std::max(0, int(client.right) - chipW - h.at(12));
    RECT title{0, h.at(2), std::min(titleRoom, widthOf(dc, h.bold, d.title) + h.at(2)), h.at(28)};
    HGDIOBJ oldFont = SelectObject(dc, h.bold);
    SetTextColor(dc, kText);
    DrawTextW(dc, d.title.c_str(), int(d.title.size()), &title,
              DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
    if (chipW > 0) {
        RECT chip{title.right + h.at(10), h.at(5), title.right + h.at(10) + chipW, h.at(25)};
        roundBox(dc, chip, h.at(10), d.chipGround, d.chipGround);
        SetTextColor(dc, d.chipWords);
        DrawTextW(dc, d.chip.c_str(), int(d.chip.size()), &chip, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    }
    // the facts
    SelectObject(dc, h.font);
    SetTextColor(dc, kMuted);
    RECT facts{0, h.at(30), client.right, h.at(50)};
    DrawTextW(dc, d.facts.c_str(), int(d.facts.size()), &facts,
              DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
    // the path: done ones green with a tick, the current one in its chip, the rest grey; short lines between
    int x = 0;
    const int top = h.at(56), bottom = h.at(80), middle = (top + bottom) / 2;
    for (size_t i = 0; i < d.steps.size(); ++i) {
        const TalkStep& s = d.steps[i];
        if (i > 0) {
            rule(dc, x + h.at(6), x + h.at(24), middle, s.done || s.current ? kDoneLine : kEdge);
            x += h.at(30);
        }
        const std::wstring text = s.done ? L"✓ " + s.text : s.text;
        const int tw = widthOf(dc, s.current ? h.bold : h.font, text);
        if (s.current) {
            RECT pill{x, top, x + tw + h.at(18), bottom};
            roundBox(dc, pill, bottom - top, d.chipGround, d.chipGround);
            SelectObject(dc, h.bold);
            SetTextColor(dc, d.chipWords);
            DrawTextW(dc, text.c_str(), int(text.size()), &pill, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
            x = pill.right;
        } else {
            RECT r{x, top, x + tw + h.at(2), bottom};
            SelectObject(dc, h.font);
            SetTextColor(dc, s.done ? kDone : kLater);
            DrawTextW(dc, text.c_str(), int(text.size()), &r, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
            x = r.right;
        }
    }
    SelectObject(dc, oldFont);
    BitBlt(target, 0, 0, client.right, client.bottom, dc, 0, 0, SRCCOPY);
    SelectObject(dc, oldBuffer);
    DeleteObject(buffer);
    DeleteDC(dc);
    EndPaint(w, &ps);
}

LRESULT CALLBACK headProc(HWND w, UINT m, WPARAM wp, LPARAM lp) {
    Head* h = headOf(w);
    switch (m) {
        case WM_NCDESTROY:
            delete h;
            SetWindowLongPtrW(w, GWLP_USERDATA, 0);
            break;
        case WM_ERASEBKGND: return 1;
        case WM_PAINT:
            if (!h) break;
            paintHead(w, *h);
            return 0;
        case WM_SIZE: InvalidateRect(w, nullptr, FALSE); return 0;
        default: break;
    }
    return DefWindowProcW(w, m, wp, lp);
}

void registerOnce(const wchar_t* name, WNDPROC proc) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof wc;
    if (GetClassInfoExW(GetModuleHandleW(nullptr), name, &wc)) return;
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));  // IDC_ARROW
    wc.lpszClassName = name;
    RegisterClassExW(&wc);
}

}  // namespace

SIZE chatThumbSize(int dpi) { return SIZE{px(240, dpi), px(135, dpi)}; }

HWND makeChatView(HWND parent, int id, HFONT font, HFONT bold, int dpi) {
    registerOnce(kChatClass, chatProc);
    HWND w = CreateWindowExW(0, kChatClass, L"", WS_CHILD | WS_VSCROLL | WS_CLIPSIBLINGS, 0, 0, 10, 10, parent,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
    if (!w) return nullptr;
    Chat* c = new Chat;
    c->font = font;
    c->bold = bold;
    c->dpi = dpi;
    SetWindowLongPtrW(w, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(c));
    return w;
}

void setChat(HWND view, const std::vector<ChatItem>& items, unsigned generation) {
    Chat* c = chatOf(view);
    if (!c) return;
    c->clear();
    c->items = items;
    for (const ChatItem& it : items) c->thumbs.emplace_back(it.shots, nullptr);
    c->generation = generation;
    layout(view, *c);
    c->scroll = c->total;  // the newest in view
    setScroll(view, *c);
    InvalidateRect(view, nullptr, FALSE);
}

void postChatThumb(HWND view, unsigned generation, size_t item, size_t shot, HBITMAP bmp) {
    Thumb* t = new Thumb{generation, item, shot, bmp};
    if (!PostMessageW(view, kThumb, 0, reinterpret_cast<LPARAM>(t))) {
        if (bmp) DeleteObject(bmp);
        delete t;
    }
}

HWND makeTalkHead(HWND parent, int id, HFONT font, HFONT bold, int dpi) {
    registerOnce(kHeadClass, headProc);
    HWND w = CreateWindowExW(0, kHeadClass, L"", WS_CHILD | WS_CLIPSIBLINGS, 0, 0, 10, 10, parent,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
    if (!w) return nullptr;
    Head* h = new Head;
    h->font = font;
    h->bold = bold;
    h->dpi = dpi;
    SetWindowLongPtrW(w, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(h));
    return w;
}

void setTalkHead(HWND head, const TalkHead& data) {
    Head* h = headOf(head);
    if (!h) return;
    h->data = data;
    InvalidateRect(head, nullptr, FALSE);
}

}  // namespace uagfx::app

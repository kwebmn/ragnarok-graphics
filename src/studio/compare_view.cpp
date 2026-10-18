#include "studio/compare_view.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cmath>

namespace uagfx::studio {
namespace {

constexpr const wchar_t* kClass = L"uaroStudioCompare";
constexpr uint32_t kGround = 0xFF2B2E33u;  // about the texture
constexpr float kCellTexels = 64.0f;       // a map's cell, in the original's texels, in the map's view

uint32_t checker(int x, int y) { return ((x / 8 + y / 8) & 1) ? 0xFF5A5E66u : 0xFF464A52u; }

uint32_t over(uint32_t c, int x, int y) {  // a texel over the checker by its alpha
    const uint32_t a = c >> 24;
    if (a >= 255) return c;
    const uint32_t g = checker(x, y);
    uint32_t out = 0xFF000000u;
    for (int i = 0; i < 3; ++i) {
        const uint32_t s = (c >> (8 * i)) & 0xFF, d = (g >> (8 * i)) & 0xFF;
        out |= ((s * a + d * (255 - a)) / 255) << (8 * i);
    }
    return out;
}

}  // namespace

bool CompareView::registerClass(HINSTANCE instance) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = &CompareView::proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_CROSS);
    wc.lpszClassName = kClass;
    return RegisterClassExW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

HWND CompareView::create(HWND parent, int id) {
    hwnd_ = CreateWindowExW(WS_EX_CLIENTEDGE, kClass, L"", WS_CHILD, 0, 0, 10, 10, parent,
                            reinterpret_cast<HMENU>(INT_PTR(id)), nullptr, this);
    return hwnd_;
}

void CompareView::set(std::shared_ptr<const Image> original, std::shared_ptr<const Image> other) {
    a_ = std::move(original);
    b_ = std::move(other);
    diff_.clear();
    if (a_ && b_ && !a_->empty() && !b_->empty())
        diff_ = differenceImage(a_->argb.data(), a_->width, a_->height, b_->argb.data(), b_->width, b_->height,
                                b_->width, b_->height);
    fit();
}

void CompareView::setMode(Mode m) {
    mode_ = m;
    fit();
}

void CompareView::setSmooth(bool smooth) {
    smooth_ = smooth;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void CompareView::setPatch(std::vector<map_textures::PatchCell> cells) {
    patch_ = std::move(cells);
    patchW_ = patchH_ = 0;
    for (const map_textures::PatchCell& c : patch_) {
        patchW_ = std::max(patchW_, c.x + 1);
        patchH_ = std::max(patchH_, c.y + 1);
    }
    patchAt_.assign(size_t(patchW_) * size_t(patchH_), -1);
    for (size_t i = 0; i < patch_.size(); ++i)
        patchAt_[size_t(patch_[i].y) * size_t(patchW_) + size_t(patch_[i].x)] = int(i);
    if (mode_ == Mode::Map) fit();
}

void CompareView::fit() {
    RECT r{};
    GetClientRect(hwnd_, &r);
    const bool two = mode_ == Mode::Pair || mode_ == Mode::Map;
    const float paneW = float(two ? r.right / 2 : r.right), paneH = float(r.bottom);
    float w = a_ && !a_->empty() ? float(a_->width) : 64.0f, h = a_ && !a_->empty() ? float(a_->height) : 64.0f;
    if (mode_ == Mode::Tiled) w *= 3, h *= 3;
    if (mode_ == Mode::Map) {
        w = float(std::max(patchW_, 1)) * kCellTexels;
        h = float(std::max(patchH_, 1)) * kCellTexels;
    }
    view_.zoom = std::clamp(std::min(paneW / w, paneH / h), kLeastZoom, kMostZoom);
    view_.x = -(paneW / view_.zoom - w) / 2.0f;
    view_.y = -(paneH / view_.zoom - h) / 2.0f;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

bool CompareView::sample(const Image& img, float ox, float oy, bool wrap, uint32_t& out) const {
    if (img.empty() || !a_ || a_->empty()) return false;
    const float w = float(a_->width), h = float(a_->height);  // the original's texel space
    if (wrap) {
        ox = std::fmod(std::fmod(ox, w) + w, w);
        oy = std::fmod(std::fmod(oy, h) + h, h);
    } else if (ox < 0 || oy < 0 || ox >= w || oy >= h) {
        return false;
    }
    const float px = ox * float(img.width) / w, py = oy * float(img.height) / h;
    if (!smooth_) {
        const int x = std::min(img.width - 1, int(px)), y = std::min(img.height - 1, int(py));
        out = img.argb[size_t(y) * size_t(img.width) + size_t(x)];
        return true;
    }
    const float fx = std::max(0.0f, px - 0.5f), fy = std::max(0.0f, py - 0.5f);
    const int x0 = std::min(img.width - 1, int(fx)), y0 = std::min(img.height - 1, int(fy));
    const int x1 = wrap ? (x0 + 1) % img.width : std::min(img.width - 1, x0 + 1);
    const int y1 = wrap ? (y0 + 1) % img.height : std::min(img.height - 1, y0 + 1);
    const float ax = fx - float(x0), ay = fy - float(y0);
    const auto at = [&](int x, int y) { return img.argb[size_t(y) * size_t(img.width) + size_t(x)]; };
    out = 0;
    for (int c = 0; c < 4; ++c) {
        const auto ch = [&](uint32_t v) { return float((v >> (8 * c)) & 0xFF); };
        const float v = (ch(at(x0, y0)) * (1 - ax) + ch(at(x1, y0)) * ax) * (1 - ay) +
                        (ch(at(x0, y1)) * (1 - ax) + ch(at(x1, y1)) * ax) * ay;
        out |= uint32_t(std::clamp(int(v + 0.5f), 0, 255)) << (8 * c);
    }
    return true;
}

bool CompareView::sampleMap(const Image& img, float px, float py, uint32_t& out) const {
    if (px < 0 || py < 0) return false;
    const int cx = int(px), cy = int(py);
    if (cx >= patchW_ || cy >= patchH_) return false;
    const int at = patchAt_[size_t(cy) * size_t(patchW_) + size_t(cx)];
    if (at < 0 || !a_ || a_->empty()) return false;
    const map_textures::PatchCell& c = patch_[size_t(at)];
    const float fx = px - float(cx), fy = py - float(cy);
    const float u = (c.u[0] * (1 - fx) + c.u[1] * fx) * (1 - fy) + (c.u[2] * (1 - fx) + c.u[3] * fx) * fy;
    const float v = (c.v[0] * (1 - fx) + c.v[1] * fx) * (1 - fy) + (c.v[2] * (1 - fx) + c.v[3] * fx) * fy;
    return sample(img, u * float(a_->width), v * float(a_->height), true, out);
}

void CompareView::paint(HDC dc) {
    RECT r{};
    GetClientRect(hwnd_, &r);
    const int W = r.right, H = r.bottom;
    if (W <= 0 || H <= 0) return;
    std::vector<uint32_t> px(size_t(W) * size_t(H), kGround);
    const bool two = mode_ == Mode::Pair || mode_ == Mode::Map;
    const int paneW = two ? W / 2 : W;
    Image diff;
    if (mode_ == Mode::Difference && b_) {
        diff.width = b_->width;
        diff.height = b_->height;
        diff.argb = diff_;
    }
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const int pane = two && x >= paneW ? 1 : 0, lx = x - pane * paneW;
            const float ox = view_.x + (float(lx) + 0.5f) / view_.zoom, oy = view_.y + (float(y) + 0.5f) / view_.zoom;
            const Image* img = pane ? b_.get() : a_.get();
            if (mode_ == Mode::Wipe) img = float(x) < wipe_ * float(W) ? a_.get() : b_.get();
            if (mode_ == Mode::Tiled) img = b_ ? b_.get() : a_.get();
            if (mode_ == Mode::Difference) img = &diff;
            if (mode_ == Mode::Single) img = a_.get();
            if (!img) continue;
            uint32_t c = 0;
            const bool got = mode_ == Mode::Map ? sampleMap(*img, ox / kCellTexels, oy / kCellTexels, c)
                                                : sample(*img, ox, oy, mode_ == Mode::Tiled, c);
            if (got) px[size_t(y) * size_t(W) + size_t(x)] = over(c, x, y);
        }
    if (mode_ == Mode::Wipe)  // the wipe's line
        for (int y = 0; y < H; ++y) px[size_t(y) * size_t(W) + size_t(std::clamp(int(wipe_ * float(W)), 0, W - 1))] =
            0xFFFFD040u;
    if (two)
        for (int y = 0; y < H; ++y) px[size_t(y) * size_t(W) + size_t(paneW)] = 0xFF101214u;
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = W;
    bi.bmiHeader.biHeight = -H;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    SetDIBitsToDevice(dc, 0, 0, UINT(W), UINT(H), 0, 0, 0, UINT(H), px.data(), &bi, DIB_RGB_COLORS);
}

LRESULT CALLBACK CompareView::proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        auto* self = static_cast<CompareView*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    auto* self = reinterpret_cast<CompareView*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    return self ? self->handle(msg, wp, lp) : DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT CompareView::handle(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd_, &ps);
            paint(dc);
            EndPaint(hwnd_, &ps);
            return 0;
        }
        case WM_SIZE:
            fit();
            return 0;
        case WM_MOUSEWHEEL: {
            POINT p{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            ScreenToClient(hwnd_, &p);
            RECT r{};
            GetClientRect(hwnd_, &r);
            const bool two = mode_ == Mode::Pair || mode_ == Mode::Map;
            if (two && p.x >= r.right / 2) p.x -= r.right / 2;  // either pane zooms about its own cursor
            const float factor = GET_WHEEL_DELTA_WPARAM(wp) > 0 ? 2.0f : 0.5f;
            view_ = zoomAt(view_, factor, float(p.x), float(p.y));
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
            SetFocus(hwnd_);
            SetCapture(hwnd_);
            dragging_ = true;
            wiping_ = mode_ == Mode::Wipe && (msg == WM_RBUTTONDOWN || (wp & MK_SHIFT));
            last_ = POINT{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            return 0;
        case WM_MOUSEMOVE:
            if (dragging_) {
                const POINT p{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
                RECT r{};
                GetClientRect(hwnd_, &r);
                if (wiping_) {
                    wipe_ = std::clamp(float(p.x) / float(std::max<LONG>(1, r.right)), 0.0f, 1.0f);
                } else {
                    view_.x -= float(p.x - last_.x) / view_.zoom;
                    view_.y -= float(p.y - last_.y) / view_.zoom;
                }
                last_ = p;
                InvalidateRect(hwnd_, nullptr, FALSE);
            }
            return 0;
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
            dragging_ = false;
            ReleaseCapture();
            return 0;
        case WM_LBUTTONDBLCLK:
            fit();
            return 0;
    }
    return DefWindowProcW(hwnd_, msg, wp, lp);
}

}  // namespace uagfx::studio

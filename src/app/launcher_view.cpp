#include "app/launcher_view.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <utility>

#include "app/files.hpp"
#include "app/text.hpp"
#include "app/ui.hpp"
#include "core/version.hpp"

namespace uagfx::app {
namespace {

using launcherview::Box;
using launcherview::Hit;
using launcherview::Part;
using launcherview::PlayState;
using settings::Text;

constexpr wchar_t kClass[] = L"uagfxLauncher";
constexpr UINT_PTR kArtTimer = 1;
constexpr int kFadeTickMs = 40, kHoldTickMs = 1000;
constexpr UINT kDpiChangedAfterParent = 0x02E3;  // WM_DPICHANGED_AFTERPARENT (Windows 10 1703)

// The game's own palette (uaUI style A, the concept page E86nmjnrRPw7s2iqnwW2VF)
constexpr uint32_t kShadeRgb = 0x080A0F;
constexpr uint32_t kCard = 0x10141C, kRaise = 0x1B2232, kLine = 0x273041;
constexpr uint32_t kText = 0xEEF2F8, kMuted = 0x9AA3B5, kPointRgb = 0xD6DBE6, kIconRgb = 0xCFD6E4;
constexpr uint32_t kGold = 0xC9A55B, kGoldHi = 0xECC97C, kOk = 0x4CC38A, kRed = 0xE0564F, kWarn = 0xE0A84F;
constexpr uint32_t kOnGold = 0x2A1D08, kOnGoldArrow = 0x3A2A0E, kOnBlue = 0x0D1730, kWhite = 0xFFFFFF;

constexpr int kArtCount = 4;
const wchar_t* const kArtFiles[kArtCount] = {L"art-dawn.jpg", L"art-day.jpg", L"art-sunset.jpg", L"art-winter.jpg"};

// The fonts in the app's resources (src/app/app.rc, third_party/launcher_fonts), each loaded once for the process
void loadFonts() {
    static bool done = false;
    if (done) return;
    done = true;
    static const wchar_t* const kNames[] = {L"LAUNCHER_EXO2_MEDIUM", L"LAUNCHER_EXO2_SEMIBOLD", L"LAUNCHER_EXO2_BOLD",
                                            L"LAUNCHER_EXO2_EXTRABOLD", L"LAUNCHER_CINZEL_BOLD"};
    for (const wchar_t* name : kNames) {
        HRSRC r = FindResourceW(nullptr, name, MAKEINTRESOURCEW(10));  // RT_RCDATA
        HGLOBAL g = r ? LoadResource(nullptr, r) : nullptr;
        void* data = g ? LockResource(g) : nullptr;
        DWORD added = 0;
        if (data) AddFontMemResourceEx(data, SizeofResource(nullptr, r), nullptr, &added);
    }
}

// The faces as GDI names them (the fonts' name ID 1: Exo 2's Bold is the family's own bold), and Segoe UI's weight
// where one is not there
struct FaceName {
    const wchar_t* name;
    int weight, fallbackWeight;
};
const FaceName kFaceNames[] = {
    {L"Exo 2 Medium", FW_NORMAL, FW_NORMAL}, {L"Exo 2 SemiBold", FW_NORMAL, FW_SEMIBOLD}, {L"Exo 2", FW_BOLD, FW_BOLD},
    {L"Exo 2 ExtraBold", FW_NORMAL, FW_BOLD}, {L"Cinzel", FW_BOLD, FW_BOLD},
    {L"Segoe MDL2 Assets", FW_NORMAL, FW_NORMAL}, {L"Segoe UI", FW_SEMIBOLD, FW_SEMIBOLD}};

HFONT makeFont(const wchar_t* name, int weight, int px) {
    return CreateFontW(-px, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH, name);
}

bool isFace(HFONT f, const wchar_t* name) {
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) return false;
    const HGDIOBJ old = SelectObject(dc, f);
    wchar_t got[LF_FACESIZE] = {};
    GetTextFaceW(dc, LF_FACESIZE, got);
    SelectObject(dc, old);
    DeleteDC(dc);
    return lstrcmpiW(got, name) == 0;
}

COLORREF colourOf(uint32_t rgb) { return RGB((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255); }

uint32_t mix(uint32_t a, uint32_t b, float t) {
    uint32_t out = 0;
    for (int shift = 0; shift <= 16; shift += 8) {
        const float x = float((a >> shift) & 255u), y = float((b >> shift) & 255u);
        out |= uint32_t(std::clamp(std::lround(x + (y - x) * t), 0L, 255L)) << shift;
    }
    return out;
}

panel::Box pb(const Box& b) { return panel::Box{b.x, b.y, b.w, b.h}; }
Box grown(const Box& b, float by) { return Box{b.x - by, b.y - by, b.w + 2 * by, b.h + 2 * by}; }

// A rounded box's signed distance (< 0 inside)
float roundDistance(float x, float y, const Box& b, float r) {
    r = std::min(r, std::min(b.w, b.h) / 2);
    const float hx = b.w / 2, hy = b.h / 2;
    const float qx = std::fabs(x - (b.x + hx)) - (hx - r), qy = std::fabs(y - (b.y + hy)) - (hy - r);
    const float ox = std::max(qx, 0.0f), oy = std::max(qy, 0.0f);
    return std::sqrt(ox * ox + oy * oy) + std::min(std::max(qx, qy), 0.0f) - r;
}

// A rounded box filled with a gradient: from `from` through `mid` (halfway) to `to`, top to bottom (or left to right)
void gradient(panel::Picture& p, const Box& b, float r, uint32_t from, uint32_t mid, uint32_t to, float a,
              bool across = false) {
    const int x0 = std::max({0, p.clipX0, int(std::floor(b.x))});
    const int x1 = std::min({p.width, p.clipX1, int(std::ceil(b.x + b.w))});
    const int y0 = std::max({0, p.clipY0, int(std::floor(b.y))});
    const int y1 = std::min({p.height, p.clipY1, int(std::ceil(b.y + b.h))});
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) {
            const float inside = std::clamp(0.5f - roundDistance(float(x) + 0.5f, float(y) + 0.5f, b, r), 0.0f, 1.0f);
            if (inside <= 0) continue;
            const float t = across ? std::clamp((float(x) + 0.5f - b.x) / b.w, 0.0f, 1.0f)
                                   : std::clamp((float(y) + 0.5f - b.y) / b.h, 0.0f, 1.0f);
            const uint32_t c = t < 0.5f ? mix(from, mid, t * 2) : mix(mid, to, t * 2 - 1);
            panel::over(p.px[size_t(y) * size_t(p.width) + size_t(x)], c, a * inside);
        }
}

// ▾: a small triangle pointing down, its middle at (cx, cy), `half` wide each way
void triangleDown(panel::Picture& p, float cx, float cy, float half, uint32_t rgb, float a) {
    const float top = cy - half * 0.55f, bottom = cy + half * 0.55f;
    for (int y = int(std::floor(top)); y <= int(std::ceil(bottom)); ++y)
        for (int x = int(std::floor(cx - half)); x <= int(std::ceil(cx + half)); ++x) {
            if (x < 0 || y < 0 || x >= p.width || y >= p.height) continue;
            int hits = 0;
            for (int sy = 0; sy < 4; ++sy)
                for (int sx = 0; sx < 4; ++sx) {
                    const float px = float(x) + (float(sx) + 0.5f) / 4, py = float(y) + (float(sy) + 0.5f) / 4;
                    if (py < top || py > bottom) continue;
                    hits += std::fabs(px - cx) <= half * (bottom - py) / (bottom - top) ? 1 : 0;
                }
            if (hits) panel::over(p.px[size_t(y) * size_t(p.width) + size_t(x)], rgb, a * float(hits) / 16.0f);
        }
}

// A line from (ax, ay) to (bx, by), r thick each side, its ends round - the picker's ✕ and ✓
void stroke(panel::Picture& p, float ax, float ay, float bx, float by, float r, uint32_t rgb, float a) {
    const int x0 = std::max(0, int(std::floor(std::min(ax, bx) - r - 1)));
    const int x1 = std::min(p.width, int(std::ceil(std::max(ax, bx) + r + 1)));
    const int y0 = std::max(0, int(std::floor(std::min(ay, by) - r - 1)));
    const int y1 = std::min(p.height, int(std::ceil(std::max(ay, by) + r + 1)));
    const float dx = bx - ax, dy = by - ay, len2 = std::max(dx * dx + dy * dy, 1e-6f);
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) {
            const float px = float(x) + 0.5f, py = float(y) + 0.5f;
            const float t = std::clamp(((px - ax) * dx + (py - ay) * dy) / len2, 0.0f, 1.0f);
            const float ex = px - (ax + t * dx), ey = py - (ay + t * dy);
            const float inside = std::clamp(0.5f - (std::sqrt(ex * ex + ey * ey) - r), 0.0f, 1.0f);
            if (inside > 0) panel::over(p.px[size_t(y) * size_t(p.width) + size_t(x)], rgb, a * inside);
        }
}

// A picture made w x h by the mean of the pixels each target pixel covers (a small one from a big one stays smooth),
// cut to w:h at its middle first
void shrink(const launcherart::Image& src, int w, int h, std::vector<uint32_t>& out) {
    out.assign(size_t(std::max(w, 0)) * size_t(std::max(h, 0)), launcherart::kGround);
    if (src.w <= 0 || src.h <= 0 || w <= 0 || h <= 0 || src.argb.size() < size_t(src.w) * size_t(src.h)) return;
    const double k = std::min(double(src.w) / w, double(src.h) / h);  // source pixels a target pixel
    const double ox = (src.w - k * w) / 2, oy = (src.h - k * h) / 2;
    for (int y = 0; y < h; ++y) {
        const int sy0 = std::clamp(int(oy + y * k), 0, src.h - 1);
        const int sy1 = std::clamp(int(oy + (y + 1) * k), sy0 + 1, src.h);
        for (int x = 0; x < w; ++x) {
            const int sx0 = std::clamp(int(ox + x * k), 0, src.w - 1);
            const int sx1 = std::clamp(int(ox + (x + 1) * k), sx0 + 1, src.w);
            uint32_t r = 0, g = 0, b = 0, n = 0;
            for (int sy = sy0; sy < sy1; ++sy)
                for (int sx = sx0; sx < sx1; ++sx) {
                    const uint32_t c = src.argb[size_t(sy) * size_t(src.w) + size_t(sx)];
                    r += (c >> 16) & 255u, g += (c >> 8) & 255u, b += c & 255u, ++n;
                }
            out[size_t(y) * size_t(w) + size_t(x)] = 0xFF000000u | (r / n) << 16 | (g / n) << 8 | (b / n);
        }
    }
}

// pixels (w x h) into the picture at (x, y), their corners rounded r; `light` < 1 darkens them
void blitRounded(panel::Picture& p, float x, float y, float r, const std::vector<uint32_t>& px, int w, int h,
                 float light) {
    if (px.size() < size_t(w) * size_t(h)) return;
    const int ix = int(std::lround(x)), iy = int(std::lround(y));
    const Box b{float(ix), float(iy), float(w), float(h)};
    for (int j = 0; j < h; ++j) {
        const int ty = iy + j;
        if (ty < 0 || ty >= p.height) continue;
        for (int i = 0; i < w; ++i) {
            const int tx = ix + i;
            if (tx < 0 || tx >= p.width) continue;
            const float inside =
                std::clamp(0.5f - roundDistance(float(tx) + 0.5f, float(ty) + 0.5f, b, r), 0.0f, 1.0f);
            if (inside <= 0) continue;
            uint32_t c = px[size_t(j) * size_t(w) + size_t(i)] & 0xFFFFFFu;
            if (light < 1) c = mix(c, 0x000000, 1 - light);
            panel::over(p.px[size_t(ty) * size_t(p.width) + size_t(tx)], c, inside);
        }
    }
}

// The picture of the time of day (the season first: winter in December to February), the next ones after it
int firstArt() {
    SYSTEMTIME t{};
    GetLocalTime(&t);
    if (t.wMonth == 12 || t.wMonth <= 2) return 3;
    if (t.wHour >= 5 && t.wHour < 10) return 0;
    if (t.wHour >= 10 && t.wHour < 17) return 1;
    return 2;
}

std::string oneDecimal(double v) {
    char text[32];
    std::snprintf(text, sizeof text, "%.1f", v);
    return text;
}

std::string megabytes(uint64_t bytes) {
    return bytes ? oneDecimal(std::max(0.1, double(bytes) / (1024.0 * 1024.0))) : "0";
}

// 2026-11-12 as 12.11.2026; anything else as it is
std::string dateText(const std::string& iso) {
    if (iso.size() != 10 || iso[4] != '-' || iso[7] != '-') return iso;
    return iso.substr(8, 2) + "." + iso.substr(5, 2) + "." + iso.substr(0, 4);
}

std::wstring upper(std::wstring s) {
    if (!s.empty()) CharUpperBuffW(s.data(), DWORD(s.size()));
    return s;
}

const Part kDockParts[launcherview::kDockIcons] = {Part::Settings, Part::Reports, Part::Site, Part::Forum};
const Text kDockWords[launcherview::kDockIcons] = {Text::LauncherSettings, Text::LauncherReports, Text::LauncherSite,
                                                   Text::LauncherForum};
const wchar_t* const kDockGlyphs[launcherview::kDockIcons] = {L"\xE713", L"\xE8BD", L"\xE774", L"\xE716"};

}  // namespace

LauncherView::~LauncherView() {
    if (hwnd_) {
        KillTimer(hwnd_, kArtTimer);
        SetWindowLongPtrW(hwnd_, GWLP_USERDATA, 0);
        DestroyWindow(hwnd_);
    }
    dropFonts();
    if (memDc_) {
        if (firstBitmap_) SelectObject(memDc_, firstBitmap_);
        DeleteDC(memDc_);
    }
    if (dib_) DeleteObject(dib_);
}

bool LauncherView::create(HWND parent, LauncherHost& host, const std::wstring& artDir) {
    host_ = &host;
    artDir_ = artDir;
    loadFonts();
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof wc;
        wc.lpfnWndProc = proc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = kClass;
        registered = RegisterClassExW(&wc) != 0;
    }
    RECT r{};
    GetClientRect(parent, &r);
    hwnd_ = CreateWindowExW(0, kClass, L"", WS_CHILD | WS_CLIPSIBLINGS | WS_TABSTOP, 0, 0, r.right, r.bottom, parent,
                            nullptr, GetModuleHandleW(nullptr), this);
    if (!hwnd_) return false;
    dpi_ = windowDpi(hwnd_);
    artIndex_ = firstArt();
    std::string bytes;
    for (int k = 0; k < kArtCount && art_[0].w <= 0; ++k) {  // the first that reads
        const int i = (artIndex_ + k) % kArtCount;
        bytes.clear();
        if (readFile(artDir_ + L"\\" + kArtFiles[i], bytes)) art_[0] = launcherart::decode(bytes);
        if (art_[0].w > 0) artIndex_ = i;
    }
    relayout();
    return true;
}

void LauncherView::show(bool on) {
    shown_ = on;
    if (!hwnd_) return;
    ShowWindow(hwnd_, on ? SW_SHOW : SW_HIDE);
    if (on) SetFocus(hwnd_);
    updateTimer();
}

void LauncherView::setData(const LauncherData& d) {
    data_ = d;
    if (data_.gameRunning && fading_) finishFade();  // still while the game runs: the coming picture at once
    relayout();
    const int n = int(focusOrder().size());
    if (focus_ >= n) focus_ = -1;
    updateTimer();
    if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);
}

void LauncherView::setProgress(bool downloading, float share, double bytesPerSecond) {
    downloading_ = downloading;
    share_ = std::clamp(share, 0.0f, 1.0f);
    speed_ = std::max(0.0, bytesPerSecond);
    if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);
}

RECT LauncherView::screenRect(Part part, int index) const {
    const Box b = boxOf(Hit{part, index});
    RECT r{LONG(std::floor(b.x)), LONG(std::floor(b.y)), LONG(std::ceil(b.x + b.w)), LONG(std::ceil(b.y + b.h))};
    if (hwnd_) MapWindowPoints(hwnd_, nullptr, reinterpret_cast<POINT*>(&r), 2);
    return r;
}

std::wstring LauncherView::t(Text x) const { return wtr(data_.lang, x); }
std::wstring LauncherView::tf(Text x, const std::vector<std::string>& args) const {
    return wtrf(data_.lang, x, args);
}

PlayState LauncherView::playState() const {
    launcherview::PlayInputs in;
    in.offered = data_.update.kind == UpdateState::Done && !data_.update.offer.empty();
    in.downloading = downloading_;
    in.gameRunning = data_.gameRunning;
    return launcherview::playState(in);
}

void LauncherView::relayout() {
    if (!hwnd_) return;
    RECT r{};
    GetClientRect(hwnd_, &r);
    const int news = int(std::min<size_t>(data_.info.news.size(), size_t(launcherview::kMostNews)));
    layout_ = launcherview::layoutFor(r.right, r.bottom, float(dpi_) / 96.0f, news, !data_.plateText.empty());
    picker_ = launcherpick::layoutFor(layout_);  // 3.0.18-beta.194
}

// --- the words ---

HFONT LauncherView::font(Face f, float px) {
    const int size = std::max(6, int(std::lround(px * layout_.s)));
    const int key = int(f) * 4096 + size;
    const auto it = fonts_.find(key);
    if (it != fonts_.end()) return it->second;
    const FaceName& n = kFaceNames[int(f)];
    HFONT made = makeFont(n.name, n.weight, size);
    if (made && !isFace(made, n.name) && f != Face::Icons) {  // the private font not there: Segoe UI
        DeleteObject(made);
        made = makeFont(L"Segoe UI", n.fallbackWeight, size);
    }
    fonts_[key] = made;
    return made;
}

bool LauncherView::hasIcons() {
    if (icons_ < 0) icons_ = isFace(font(Face::Icons, 20), kFaceNames[int(Face::Icons)].name) ? 1 : 0;
    return icons_ > 0;
}

void LauncherView::dropFonts() {
    if (memDc_) SelectObject(memDc_, GetStockObject(DEFAULT_GUI_FONT));
    for (auto& [key, f] : fonts_)
        if (f) DeleteObject(f);
    fonts_.clear();
}

namespace {

// A line's runs: the face's own glyphs, and those it has none for (in Segoe UI)
struct Run {
    std::wstring text;
    bool other = false;
};
std::vector<Run> runsOf(HDC dc, const std::wstring& s) {
    std::vector<Run> runs;
    if (s.empty()) return runs;
    std::vector<WORD> glyphs(s.size());
    if (GetGlyphIndicesW(dc, s.c_str(), int(s.size()), glyphs.data(), GGI_MARK_NONEXISTING_GLYPHS) == GDI_ERROR)
        return {Run{s, false}};
    for (size_t i = 0; i < s.size(); ++i) {
        const bool other = glyphs[i] == 0xFFFF && s[i] != L' ';
        if (runs.empty() || runs.back().other != other) runs.push_back(Run{L"", other});
        runs.back().text += s[i];
    }
    return runs;
}

}  // namespace

int LauncherView::widthOf(HDC dc, const Style& st, const std::wstring& s) {
    HFONT own = font(st.face, st.px), other = font(Face::Other, st.px);
    SelectObject(dc, own);
    SetTextCharacterExtra(dc, int(std::lround(st.spacing * layout_.s)));
    int w = 0;
    for (const Run& r : runsOf(dc, s)) {
        SelectObject(dc, r.other ? other : own);
        SIZE z{};
        GetTextExtentPoint32W(dc, r.text.c_str(), int(r.text.size()), &z);
        w += z.cx;
    }
    SelectObject(dc, own);
    return w;
}

std::wstring LauncherView::fit(HDC dc, const Style& st, const std::wstring& s, int width) {
    if (widthOf(dc, st, s) <= width) return s;
    size_t lo = 0, hi = s.size();  // the longest start that fits with «…»
    while (lo < hi) {
        const size_t mid = (lo + hi + 1) / 2;
        std::wstring cut = s.substr(0, mid);
        while (!cut.empty() && cut.back() == L' ') cut.pop_back();
        if (widthOf(dc, st, cut + L"\x2026") <= width)
            lo = mid;
        else
            hi = mid - 1;
    }
    std::wstring cut = s.substr(0, lo);
    while (!cut.empty() && cut.back() == L' ') cut.pop_back();
    return cut + L"\x2026";
}

std::vector<std::wstring> LauncherView::wrap(HDC dc, const Style& st, const std::wstring& s, int width, int most) {
    std::vector<std::wstring> lines;
    std::vector<std::wstring> parts;
    size_t at = 0;
    while (at < s.size()) {
        const size_t space = s.find(L' ', at);
        const size_t end = space == std::wstring::npos ? s.size() : space;
        if (end > at) parts.push_back(s.substr(at, end - at));
        at = end + 1;
    }
    std::wstring cur;
    size_t i = 0;
    for (; i < parts.size(); ++i) {
        const std::wstring tried = cur.empty() ? parts[i] : cur + L" " + parts[i];
        if (cur.empty() || widthOf(dc, st, tried) <= width) {
            cur = tried;
            continue;
        }
        if (int(lines.size()) + 1 >= most) break;  // the last line: the rest is cut below
        lines.push_back(fit(dc, st, cur, width));
        cur = parts[i];
    }
    if (!cur.empty() || i < parts.size()) {
        std::wstring last = cur;
        for (; i < parts.size(); ++i) last += L" " + parts[i];
        lines.push_back(fit(dc, st, last, width));
    }
    return lines;
}

float LauncherView::ascentOf(HDC dc, const Style& st) {
    SelectObject(dc, font(st.face, st.px));
    TEXTMETRICW tm{};
    GetTextMetricsW(dc, &tm);
    return float(tm.tmAscent);
}

void LauncherView::line(HDC dc, const Style& st, const std::wstring& s, float x, float baseline, int align) {
    if (s.empty()) return;
    const int w = widthOf(dc, st, s);
    int left = int(std::lround(x));
    if (align == 0) left = int(std::lround(x - float(w) / 2));
    if (align > 0) left = int(std::lround(x)) - w;
    HFONT own = font(st.face, st.px), other = font(Face::Other, st.px);
    SetTextColor(dc, colourOf(st.rgb));
    SetTextAlign(dc, TA_BASELINE | TA_LEFT | TA_NOUPDATECP);
    SetTextCharacterExtra(dc, int(std::lround(st.spacing * layout_.s)));
    SelectObject(dc, own);
    const int y = int(std::lround(baseline));
    for (const Run& r : runsOf(dc, s)) {
        SelectObject(dc, r.other ? other : own);
        ExtTextOutW(dc, left, y, 0, nullptr, r.text.c_str(), UINT(r.text.size()), nullptr);
        SIZE z{};
        GetTextExtentPoint32W(dc, r.text.c_str(), int(r.text.size()), &z);
        left += z.cx;
    }
    SelectObject(dc, own);
}

// --- the frame ---

bool LauncherView::ensureFrame(int w, int h) {
    if (w <= 0 || h <= 0) return false;
    if (dib_ && w == frameW_ && h == frameH_) return true;
    if (!memDc_) memDc_ = CreateCompatibleDC(nullptr);
    if (!memDc_) return false;
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;  // rows top down, as panel::Picture's
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP dib = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!dib || !bits) {
        if (dib) DeleteObject(dib);
        return false;
    }
    const HGDIOBJ was = SelectObject(memDc_, dib);
    if (!firstBitmap_) firstBitmap_ = was;
    if (dib_) DeleteObject(dib_);
    dib_ = dib;
    bits_ = bits;
    frameW_ = w;
    frameH_ = h;
    SetBkMode(memDc_, TRANSPARENT);
    coverArt();
    return true;
}

void LauncherView::coverArt() {
    launcherart::cover(art_[0], frameW_, frameH_, artPx_);
    if (fading_) launcherart::cover(art_[1], frameW_, frameH_, nextPx_);
    baseValid_ = false;
}

// The concept's three shades over the art in (8, 10, 15): from the left (.94, .82 at 27 %, .25 at 50 %, none from
// 62 %), from the bottom (.88, .35 at 22 %, none from 40 %), from the top (.7, none from 13 %) - the words' side dark,
// the art clear at the top right. All of one colour: the pixel keeps (1 - a) of each, c + (p - c) * k(x) * k(y).
void LauncherView::shade(std::vector<uint32_t>& px) const {
    const int w = frameW_, h = frameH_;
    if (w <= 0 || h <= 0 || px.size() < size_t(w) * size_t(h)) return;
    const auto ramp = [](float t, std::initializer_list<std::pair<float, float>> stops) {
        auto prev = *stops.begin();
        if (t <= prev.first) return prev.second;
        for (const auto& s : stops) {
            if (t <= s.first) return prev.second + (s.second - prev.second) * (t - prev.first) / (s.first - prev.first);
            prev = s;
        }
        return prev.second;
    };
    std::vector<int> kx(size_t(w), 0), ky(size_t(h), 0);
    for (int x = 0; x < w; ++x) {
        const float a = ramp((float(x) + 0.5f) / float(w), {{0.0f, 0.94f}, {0.27f, 0.82f}, {0.5f, 0.25f}, {0.62f, 0}});
        kx[size_t(x)] = int(std::lround((1 - a) * 256));
    }
    for (int y = 0; y < h; ++y) {
        const float fromBottom = (float(h - y) - 0.5f) / float(h), fromTop = (float(y) + 0.5f) / float(h);
        const float a = ramp(fromBottom, {{0.0f, 0.88f}, {0.22f, 0.35f}, {0.4f, 0}});
        const float b = ramp(fromTop, {{0.0f, 0.7f}, {0.13f, 0}});
        ky[size_t(y)] = int(std::lround((1 - a) * (1 - b) * 256));
    }
    const int cr = int((kShadeRgb >> 16) & 255), cg = int((kShadeRgb >> 8) & 255), cb = int(kShadeRgb & 255);
    for (int y = 0; y < h; ++y) {
        uint32_t* row = px.data() + size_t(y) * size_t(w);
        const int fy = ky[size_t(y)];
        for (int x = 0; x < w; ++x) {
            const int k = (kx[size_t(x)] * fy) >> 8;  // 0..256
            const uint32_t p = row[x];
            const int r = cr + (((int((p >> 16) & 255) - cr) * k) >> 8);
            const int g = cg + (((int((p >> 8) & 255) - cg) * k) >> 8);
            const int b = cb + (((int(p & 255) - cb) * k) >> 8);
            row[x] = 0xFF000000u | uint32_t(r) << 16 | uint32_t(g) << 8 | uint32_t(b);
        }
    }
}

bool LauncherView::hovered(Part p, int index) const {
    return hover_.part == p && (index < 0 || hover_.index == index);
}
bool LauncherView::pressedOn(Part p) const { return pressed_.part == p && hover_.part == p; }

void LauncherView::compose() {
    const int w = frameW_, h = frameH_;
    if (!baseValid_) {
        if (artPx_.size() != size_t(w) * size_t(h)) coverArt();
        base_ = artPx_;
        if (fading_ && nextPx_.size() == base_.size()) {
            const float t = std::clamp(float(fadeMs_) / float(launcherart::kCrossfadeMs), 0.0f, 1.0f);
            launcherart::blend(base_, nextPx_, t * t * (3 - 2 * t));  // eased in and out
        }
        shade(base_);
        baseValid_ = true;
    }
    pic_.width = w;
    pic_.height = h;
    pic_.px = base_;
    pic_.clipX0 = pic_.clipY0 = 0;
    pic_.clipX1 = pic_.clipY1 = 1 << 30;
    const launcherview::Layout& L = layout_;
    const float s = L.s, edge = std::max(1.0f, s);
    HDC dc = memDc_;
    // the server's pill: its dot and words, at the right of its place
    {
        const Style st{Face::SemiBold, 13, kText};
        const std::wstring said = data_.siteFailed ? t(Text::LauncherServerDown)
                                   : data_.info.online >= 0
                                       ? tf(Text::LauncherServer, {std::to_string(data_.info.online)})
                                       : t(Text::LauncherServerAsking);
        const float pw = std::min(L.server.w, float(widthOf(dc, st, said)) + 40 * s), ph = 30 * s;
        pill_ = Box{L.server.x + L.server.w - pw, L.server.y + (L.server.h - ph) / 2, pw, ph};
        panel::roundRect(pic_, pb(pill_), ph / 2, kCard, 0.72f, kGold, 0.35f, edge);
        const uint32_t dot = data_.siteFailed ? kRed : data_.info.online >= 0 ? kOk : kMuted;
        const float cx = pill_.x + 15 * s, cy = pill_.y + ph / 2;
        panel::circle(pic_, cx, cy, 8 * s, dot, 0.18f);  // its glow
        panel::circle(pic_, cx, cy, 4 * s, dot, 1.0f);
    }
    // the language: «UA» and ▾
    {
        if (hovered(Part::Language)) panel::roundRect(pic_, pb(L.language), 6 * s, kCard, 0.6f, kLine, 1.0f, edge);
        const Style st{Face::SemiBold, 13, kMuted};
        const float tw = float(widthOf(dc, st, L"UA"));
        const float x = L.language.x + (L.language.w - tw - 14 * s) / 2 + tw + 8 * s;
        triangleDown(pic_, x, L.language.y + L.language.h / 2, 4 * s, hovered(Part::Language) ? kText : kMuted, 1.0f);
    }
    // the update's card: a gold edge and a gold strip at its left; the news' cards
    if (L.update.w > 0) {
        const bool on = hovered(Part::Changelog);
        panel::roundRect(pic_, pb(L.update), 9 * s, kCard, 0.82f, kGold, on ? 0.9f : 0.55f, edge);
        pic_.clipX1 = int(std::lround(L.update.x + 3 * s));
        panel::roundRect(pic_, pb(L.update), 9 * s, kGold, 1.0f);
        pic_.clipX1 = 1 << 30;
    }
    for (int i = 0; i < L.newsShown; ++i) {
        const bool on = hovered(Part::News, i);
        panel::roundRect(pic_, pb(L.news[i]), 9 * s, kCard, 0.82f, on ? kGold : kLine, on ? 0.6f : 1.0f, edge);
    }
    // the icons: a box each, the reports' red count
    for (int i = 0; i < launcherview::kDockIcons; ++i) {
        const Box& d = L.dock[i];
        const Box icon{d.x + (d.w - 44 * s) / 2, d.y, 44 * s, 44 * s};
        const bool on = hovered(kDockParts[i]);
        panel::roundRect(pic_, pb(icon), 9 * s, on ? kRaise : kCard, 0.86f, on ? kGold : kLine, on ? 0.7f : 1.0f,
                         edge);
        if (kDockParts[i] == Part::Reports && data_.unread > 0)
            panel::circle(pic_, icon.x + icon.w - 3 * s, icon.y + 3 * s, 9 * s, kRed, 1.0f);
    }
    // the look line
    if (L.look.w > 0) {
        const bool on = hovered(Part::Look);
        panel::roundRect(pic_, pb(L.look), 8 * s, kCard, 0.86f, on ? kGold : kLine, on ? 0.6f : 1.0f, edge);
    }
    // the button: gold to play (its glow under it), blue to update, dark while it comes or the game runs
    {
        const Box btn{L.play.x, L.play.y, L.play.w + L.playMore.w, L.play.h};
        const float r = 10 * s;
        const PlayState state = playState();
        if (state == PlayState::Play) {
            for (int i = 6; i >= 1; --i) {
                const float g = float(i) * 3 * s;
                panel::roundRect(pic_, pb(Box{btn.x - g, btn.y + 7 * s - g / 2, btn.w + 2 * g, btn.h + g}), r + g,
                                 kGold, 0.035f);
            }
            gradient(pic_, btn, r, 0xF0CF83, 0xD7AC58, 0xB88A3A, 1.0f);
        } else if (state == PlayState::Update) {
            for (int i = 5; i >= 1; --i) {
                const float g = float(i) * 3 * s;
                panel::roundRect(pic_, pb(Box{btn.x - g, btn.y + 7 * s - g / 2, btn.w + 2 * g, btn.h + g}), r + g,
                                 0x7A9CF0, 0.035f);
            }
            gradient(pic_, btn, r, 0x86A7F3, 0x6C90E4, 0x5279D6, 1.0f);
        } else {
            panel::roundRect(pic_, pb(btn), r, kRaise, 0.96f, kLine, 1.0f, edge);
            if (state == PlayState::Updating && share_ > 0) {
                pic_.clipX1 = int(std::lround(L.play.x + L.play.w * share_));
                gradient(pic_, btn, r, 0x4A5F96, 0x5C76B8, 0x7A9CF0, 0.75f, true);
                pic_.clipX1 = 1 << 30;
            }
        }
        const bool bright = state == PlayState::Play || state == PlayState::Update;
        if (bright) {  // the light along its top edge
            pic_.clipY1 = int(std::lround(btn.y + 2 * s));
            panel::roundRect(pic_, pb(btn), r, kWhite, 0.5f);
            pic_.clipY1 = 1 << 30;
        }
        // the hovered half lighter, the pressed one darker
        for (const auto& [part, box] : {std::pair{Part::Play, L.play}, std::pair{Part::PlayMore, L.playMore}}) {
            if (!hovered(part)) continue;
            pic_.clipX0 = int(std::lround(box.x));
            pic_.clipX1 = int(std::lround(box.x + box.w));
            panel::roundRect(pic_, pb(btn), r, pressedOn(part) ? 0x000000 : kWhite, pressedOn(part) ? 0.14f : 0.1f);
            pic_.clipX0 = 0;
            pic_.clipX1 = 1 << 30;
        }
        const uint32_t split = bright ? 0x3C280A : kLine;
        panel::roundRect(pic_, pb(Box{L.playMore.x, btn.y + 14 * s, edge, btn.h - 28 * s}), 0, split,
                         bright ? 0.35f : 1.0f);
        const uint32_t arrow = state == PlayState::Play ? kOnGoldArrow : state == PlayState::Update ? kOnBlue : kMuted;
        triangleDown(pic_, L.playMore.x + L.playMore.w / 2, btn.y + btn.h / 2, 7 * s, arrow, 1.0f);
    }
    // the plate over the icons, its button at its right
    plateButton_ = Box{};
    if (L.plate.w > 0) {
        panel::roundRect(pic_, pb(L.plate), 9 * s, kCard, 0.94f, kWarn, 0.6f, edge);
        const Style st{Face::SemiBold, 13, kGoldHi};
        const float bw = std::min(L.plate.w * 0.5f, float(widthOf(dc, st, data_.plateButton)) + 28 * s);
        plateButton_ = Box{L.plate.x + L.plate.w - 8 * s - bw, L.plate.y + 7 * s, bw, L.plate.h - 14 * s};
        const bool on = hovered(Part::Plate);
        panel::roundRect(pic_, pb(plateButton_), 7 * s, kGold, on ? 0.3f : 0.14f, kGold, 0.8f, edge);
    }
    // the keyboard's ring
    const std::vector<Hit> order = focusOrder();
    if (!picking_ && focus_ >= 0 && focus_ < int(order.size()) && GetFocus() == hwnd_) {
        const Box b = boxOf(order[size_t(focus_)]);
        if (b.w > 0) panel::roundRect(pic_, pb(grown(b, 3 * s)), 12 * s, 0, 0, kGoldHi, 0.95f, 2 * s);
    }
}

void LauncherView::words(HDC dc) {
    const launcherview::Layout& L = layout_;
    const float s = L.s;
    // the wordmark and its line
    line(dc, Style{Face::Logo, 30, kGoldHi, 1}, L"uaRO", L.logo.x, L.logo.y + 26 * s, -1);
    line(dc, Style{Face::SemiBold, 9.5f, kMuted, 2.2f}, upper(t(Text::LauncherTagline)), L.logo.x + 1 * s,
         L.logo.y + 40 * s, -1);
    // the server's pill
    {
        const Style st{Face::SemiBold, 13, kText};
        const std::wstring said = data_.siteFailed ? t(Text::LauncherServerDown)
                                   : data_.info.online >= 0
                                       ? tf(Text::LauncherServer, {std::to_string(data_.info.online)})
                                       : t(Text::LauncherServerAsking);
        const float base = pill_.y + pill_.h / 2 + ascentOf(dc, st) * 0.36f;
        line(dc, st, fit(dc, st, said, int(pill_.w - 40 * s)), pill_.x + 27 * s, base, -1);
    }
    // the language
    {
        const std::string token = launcher::langToken(data_.lang);
        const std::wstring code = token == "uk" ? L"UA" : token == "ru" ? L"RU" : L"EN";
        const Style st{Face::SemiBold, 13, hovered(Part::Language) ? kText : kMuted};
        const float tw = float(widthOf(dc, st, code));
        const float x = L.language.x + (L.language.w - tw - 14 * s) / 2;
        line(dc, st, code, x, L.language.y + L.language.h / 2 + ascentOf(dc, st) * 0.36f, -1);
    }
    // «Що нового»
    if (L.update.w > 0) {
        line(dc, Style{Face::Bold, 11.5f, kGold, 2.2f}, upper(t(Text::LauncherWhatsNew)), L.feed.x + 1 * s,
             L.feed.y + 16 * s, -1);
        const Box& c = L.update;
        const float x = c.x + 18 * s, width = c.w - 18 * s - 16 * s;
        const settings::changelog::Version* v = data_.news.empty() ? nullptr : &data_.news.front();
        const std::wstring title = !v ? wide(std::string("uaRO Graphics ") + kVersion)
                                   : data_.newFor > 0 ? tf(Text::LauncherUpdated, {v->version})
                                                      : wide(v->version);
        const Style head{Face::Bold, 17, kText};
        float y = c.y + 15 * s + ascentOf(dc, head);
        line(dc, head, fit(dc, head, title, int(width)), x, y, -1);
        if (v) {
            std::wstring sub = wide(dateText(v->date));
            const std::wstring channel = t(v->beta ? Text::LauncherBeta : Text::LauncherRelease);
            sub = sub.empty() ? channel : sub + L" \x00B7 " + channel;
            if (data_.newFor > 1) sub += L" \x00B7 " + tf(Text::LauncherNewSince, {std::to_string(data_.newFor)});
            const Style small{Face::Medium, 12, kMuted};
            y += 19 * s;
            line(dc, small, fit(dc, small, sub, int(width)), x, y, -1);
            // its first points, two lines each, while they fit above the link
            const Style point{Face::Medium, 13, kPointRgb};
            const Style bullet{Face::Bold, 13, kGold};
            const float bottom = c.y + c.h - 36 * s;
            y += 23 * s;
            int shown = 0;
            for (const settings::changelog::Point& p : v->points) {
                if (p.section || p.text.empty()) continue;
                if (shown == 2 || y > bottom) break;
                const int room = std::max(1, std::min(2, int((bottom - y) / (18 * s)) + 1));
                const std::vector<std::wstring> lines = wrap(dc, point, wide(p.text), int(width - 13 * s), room);
                line(dc, bullet, L"\x2022", x, y, -1);
                for (const std::wstring& l : lines) {
                    line(dc, point, l, x + 13 * s, y, -1);
                    y += 18 * s;
                }
                y += 4 * s;
                ++shown;
            }
        }
        line(dc, Style{Face::SemiBold, 13, hovered(Part::Changelog) ? kWhite : kGoldHi}, t(Text::LauncherLog), x,
             c.y + c.h - 14 * s, -1);
    }
    // the site's news: the date (gold when pinned) and the title, two lines
    for (int i = 0; i < L.newsShown && size_t(i) < data_.info.news.size(); ++i) {
        const launcher::News& n = data_.info.news[size_t(i)];
        const Box& c = L.news[i];
        const float x = c.x + 15 * s, width = c.w - 30 * s;
        const Style date{Face::Medium, 12, n.pinned ? kGold : kMuted};
        float y = c.y + 13 * s + ascentOf(dc, date);
        line(dc, date, fit(dc, date, n.pinned ? tf(Text::PlayPinned, {n.date}) : wide(n.date), int(width)), x, y, -1);
        const Style title{Face::SemiBold, 14, hovered(Part::News, i) ? kWhite : kText};
        y += 20 * s;
        for (const std::wstring& l : wrap(dc, title, wide(n.title), int(width), 2)) {
            line(dc, title, l, x, y, -1);
            y += 19 * s;
        }
    }
    // the icons' glyphs and words, the reports' count
    const bool glyphs = hasIcons();
    for (int i = 0; i < launcherview::kDockIcons; ++i) {
        const Box& d = L.dock[i];
        const bool on = hovered(kDockParts[i]);
        const float cx = d.x + d.w / 2;
        const std::wstring word = t(kDockWords[i]);
        if (glyphs) {
            const Style st{Face::Icons, 20, on ? kGoldHi : kIconRgb};
            line(dc, st, kDockGlyphs[i], cx, d.y + 22 * s + ascentOf(dc, st) * 0.5f, 0);
        } else {  // Windows 7 and 8: the word's first letter
            const Style st{Face::Bold, 18, on ? kGoldHi : kIconRgb};
            line(dc, st, word.substr(0, 1), cx, d.y + 22 * s + ascentOf(dc, st) * 0.36f, 0);
        }
        line(dc, Style{Face::SemiBold, 11, on ? kText : kMuted}, word, cx, d.y + 60 * s, 0);
        if (kDockParts[i] == Part::Reports && data_.unread > 0) {
            const Style st{Face::Bold, 10.5f, kWhite};
            const float bx = d.x + (d.w + 44 * s) / 2 - 3 * s, by = d.y + 3 * s;
            line(dc, st, std::to_wstring(std::min(data_.unread, 99)), bx, by + ascentOf(dc, st) * 0.36f, 0);
        }
    }
    // the look line: the look, its note under it, «змінити ›» at its right
    if (L.look.w > 0) {
        const Style change{Face::SemiBold, 13, hovered(Part::Look) ? kWhite : kGoldHi};
        const std::wstring changeWords = t(Text::LauncherChange);
        const float cw = float(widthOf(dc, change, changeWords));
        const float x = L.look.x + 15 * s, width = L.look.w - 30 * s - cw - 12 * s;
        const Style look{Face::SemiBold, 13.5f, kText}, note{Face::Medium, 11.5f, kMuted};
        if (data_.lookNote.empty()) {
            line(dc, look, fit(dc, look, data_.look, int(width)), x, L.look.y + L.look.h / 2 + 5 * s, -1);
        } else {
            line(dc, look, fit(dc, look, data_.look, int(width)), x, L.look.y + 22 * s, -1);
            line(dc, note, fit(dc, note, data_.lookNote, int(width)), x, L.look.y + 39 * s, -1);
        }
        line(dc, change, changeWords, L.look.x + L.look.w - 15 * s, L.look.y + L.look.h / 2 + 5 * s, 1);
    }
    // the button's words
    {
        const PlayState state = playState();
        Style st;
        std::wstring said;
        if (state == PlayState::Play) {
            st = Style{Face::ExtraBold, 28, kOnGold, 5};
            said = t(Text::LauncherPlay);
        } else if (state == PlayState::Update) {
            st = Style{Face::ExtraBold, 19, kOnBlue, 2};
            said = tf(Text::LauncherUpdate, {megabytes(data_.update.branch.package.size)});
        } else if (state == PlayState::Updating) {
            st = Style{Face::SemiBold, 16, kText, 0.5f};
            said = tf(Text::LauncherUpdating, {std::to_string(int(share_ * 100)), oneDecimal(speed_ / 1048576.0)});
        } else {
            st = Style{Face::SemiBold, 15, kMuted, 1};
            said = t(Text::LauncherRunning);
        }
        const std::wstring shown = fit(dc, st, said, int(L.play.w - 24 * s));
        const float base = L.play.y + L.play.h / 2 + ascentOf(dc, st) * 0.36f;
        line(dc, st, shown, L.play.x + L.play.w / 2 + st.spacing * s / 2, base, 0);
    }
    // the version, at the right under the button
    {
        const Style st{Face::Medium, 12, hovered(Part::Version) ? kText : kMuted};
        line(dc, st, fit(dc, st, data_.version, int(L.version.w)), L.version.x + L.version.w, L.version.y + 13 * s, 1);
    }
    // the plate's words (two lines at most) and its button's
    if (L.plate.w > 0) {
        const Style st{Face::Medium, 12.5f, kText};
        const std::vector<std::wstring> lines =
            wrap(dc, st, data_.plateText, int(plateButton_.x - L.plate.x - 28 * s), 2);
        const float lineH = 16 * s, middle = L.plate.y + L.plate.h / 2;
        float y = middle - lineH * float(lines.empty() ? 0 : lines.size() - 1) / 2 + ascentOf(dc, st) * 0.36f;
        for (const std::wstring& l : lines) {
            line(dc, st, l, L.plate.x + 15 * s, y, -1);
            y += lineH;
        }
        const float base = middle + ascentOf(dc, Style{Face::SemiBold, 13, kText}) * 0.36f;
        const Style button{Face::SemiBold, 13, hovered(Part::Plate) ? kWhite : kGoldHi};
        line(dc, button, fit(dc, button, data_.plateButton, int(plateButton_.w - 16 * s)),
             plateButton_.x + plateButton_.w / 2, base, 0);
    }
}

void LauncherView::paint(HDC target) {
    RECT r{};
    GetClientRect(hwnd_, &r);
    if (!ensureFrame(r.right, r.bottom)) return;
    if (layout_.w != r.right || layout_.h != r.bottom) relayout();
    compose();
    GdiFlush();
    std::memcpy(bits_, pic_.px.data(), size_t(frameW_) * size_t(frameH_) * sizeof(uint32_t));
    words(memDc_);
    if (picking_) {  // 3.0.18-beta.194: the picker over the frame with its words - back into pic_, its card, its words
        GdiFlush();
        std::memcpy(pic_.px.data(), bits_, size_t(frameW_) * size_t(frameH_) * sizeof(uint32_t));
        composePicker();
        std::memcpy(bits_, pic_.px.data(), size_t(frameW_) * size_t(frameH_) * sizeof(uint32_t));
        wordsPicker(memDc_);
    }
    GdiFlush();
    BitBlt(target, 0, 0, frameW_, frameH_, memDc_, 0, 0, SRCCOPY);
}

// --- the art's timer: a second's tick while a picture is held, 25 a second while the next one fades in ---

bool LauncherView::inFront() const {
    const HWND root = GetAncestor(hwnd_, GA_ROOT);
    return root && GetForegroundWindow() == root && !IsIconic(root);
}

void LauncherView::updateTimer() {
    if (!hwnd_) return;
    const bool run = shown_ && !data_.gameRunning && art_[0].w > 0;
    const int want = !run ? 0 : fading_ ? kFadeTickMs : kHoldTickMs;
    if (want == timerMs_) return;
    KillTimer(hwnd_, kArtTimer);
    timerMs_ = want;
    if (want) {
        lastTick_ = GetTickCount64();
        SetTimer(hwnd_, kArtTimer, UINT(want), nullptr);
    }
}

void LauncherView::finishFade() {
    if (!fading_) return;
    art_[0] = std::move(art_[1]);
    art_[1] = launcherart::Image{};
    artPx_.swap(nextPx_);
    std::vector<uint32_t>().swap(nextPx_);
    artIndex_ = nextIndex_;
    fading_ = false;
    heldMs_ = fadeMs_ = 0;
    baseValid_ = false;
    if (artPx_.size() != size_t(frameW_) * size_t(frameH_)) coverArt();
}

void LauncherView::tick() {
    const unsigned long long now = GetTickCount64();
    const int dt = int(std::min<unsigned long long>(now - lastTick_, 250));
    lastTick_ = now;
    if (!inFront()) return;  // held still behind other windows
    if (!fading_) {
        heldMs_ += dt;
        if (heldMs_ < launcherart::kHoldMs) return;
        heldMs_ = 0;
        std::string bytes;
        for (int k = 1; k < kArtCount; ++k) {  // the next picture that reads
            const int i = (artIndex_ + k) % kArtCount;
            bytes.clear();
            art_[1] = readFile(artDir_ + L"\\" + kArtFiles[i], bytes) ? launcherart::decode(bytes)
                                                                       : launcherart::Image{};
            if (art_[1].w > 0) {
                nextIndex_ = i;
                break;
            }
        }
        if (art_[1].w <= 0) return;
        launcherart::cover(art_[1], frameW_, frameH_, nextPx_);
        fading_ = true;
        fadeMs_ = 0;
        updateTimer();
        return;
    }
    fadeMs_ += dt;
    if (fadeMs_ >= launcherart::kCrossfadeMs) {
        finishFade();
        updateTimer();
    }
    baseValid_ = false;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

// --- 3.0.18-beta.194: the look picker ---

void LauncherView::openPicker() {
    if (!looksRead_) {  // «Як у грі», «Покращена», «Ремастер»'s pictures; «Авто» shows the improved one darker
        looksRead_ = true;
        static const wchar_t* const kFiles[3] = {L"look-classic.jpg", L"look-better.jpg", L"look-remaster.jpg"};
        std::string bytes;
        for (int i = 0; i < 3; ++i) {
            bytes.clear();
            looks_[i] =
                readFile(artDir_ + L"\\" + kFiles[i], bytes) ? launcherart::decode(bytes) : launcherart::Image{};
        }
        lookW_ = lookH_ = 0;
    }
    picking_ = true;
    hover_ = pressed_ = Hit{};
    pickHover_ = pickPressed_ = launcherpick::PickHit{};
    pickFocus_ = -1;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void LauncherView::closePicker() {
    if (!picking_) return;
    picking_ = false;
    pickHover_ = pickPressed_ = launcherpick::PickHit{};
    pickFocus_ = -1;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

std::vector<launcherpick::PickHit> LauncherView::pickerOrder() const {
    using launcherpick::PickHit;
    using launcherpick::PickPart;
    std::vector<PickHit> order;
    for (size_t i = 0; i < picker_.options.size(); ++i) order.push_back(PickHit{PickPart::Option, int(i)});
    order.push_back(PickHit{PickPart::Auto});
    order.push_back(PickHit{PickPart::Test});
    if (data_.undoable) order.push_back(PickHit{PickPart::Undo});
    order.push_back(PickHit{PickPart::All});
    order.push_back(PickHit{PickPart::Close});
    return order;
}

Box LauncherView::pickerBox(launcherpick::PickHit hit) const {
    using launcherpick::PickPart;
    switch (hit.part) {
    case PickPart::Option:
        return hit.option >= 0 && size_t(hit.option) < picker_.options.size() ? picker_.options[size_t(hit.option)].box
                                                                               : Box{};
    case PickPart::Auto: return picker_.autoRow;
    case PickPart::Test: return picker_.test;
    case PickPart::Undo: return picker_.undo;
    case PickPart::All: return picker_.all;
    case PickPart::Close: return picker_.close;
    default: break;
    }
    return Box{};
}

void LauncherView::pickerPress(launcherpick::PickHit hit) {
    using launcherpick::PickPart;
    namespace presets = settings::presets;
    switch (hit.part) {
    case PickPart::None:
    case PickPart::Close: closePicker(); return;
    case PickPart::Card: return;
    case PickPart::Option:
        if (host_ && hit.option >= 0 && size_t(hit.option) < picker_.options.size()) {
            const launcherpick::Option& o = picker_.options[size_t(hit.option)];
            host_->lookAction(PickPart::Option, o.choice, o.variant);
        }
        return;
    case PickPart::Undo:
        if (!data_.undoable) return;
        if (host_) host_->lookAction(hit.part, presets::Choice::Picture, 0);
        return;
    case PickPart::Auto:
        if (host_) host_->lookAction(hit.part, presets::Choice::Picture, 0);
        return;
    case PickPart::Test:
    case PickPart::All:
        closePicker();
        if (host_) host_->lookAction(hit.part, presets::Choice::Picture, 0);
        return;
    }
}

void LauncherView::composePicker() {
    namespace presets = settings::presets;
    using launcherpick::PickPart;
    const launcherpick::Picker& P = picker_;
    const float s = P.s, edge = std::max(1.0f, s);
    pic_.clipX0 = pic_.clipY0 = 0;
    pic_.clipX1 = pic_.clipY1 = 1 << 30;
    // its shadow, the card
    for (int i = 6; i >= 1; --i) {
        const float g = float(i) * 3 * s;
        panel::roundRect(pic_, pb(Box{P.card.x - g, P.card.y + 6 * s - g / 2, P.card.w + 2 * g, P.card.h + g}),
                         14 * s + g, 0x000000, 0.06f);
    }
    panel::roundRect(pic_, pb(P.card), 14 * s, kCard, 0.98f, kLine, 1.0f, edge);
    const auto on = [&](PickPart part, int option = -1) {
        return pickHover_.part == part && (option < 0 || pickHover_.option == option);
    };
    // «Авто для вашої відеокарти»: gold while the look is the card's, a tick at its right
    const bool autoOn = on(PickPart::Auto);
    panel::roundRect(pic_, pb(P.autoRow), 9 * s, kRaise, 0.9f, data_.fitsCard || autoOn ? kGold : kLine,
                     data_.fitsCard ? 0.95f : autoOn ? 0.6f : 1.0f, data_.fitsCard ? 2 * s : edge);
    if (data_.fitsCard) {
        const float cx = P.autoRow.x + P.autoRow.w - 22 * s, cy = P.autoRow.y + P.autoRow.h / 2;
        stroke(pic_, cx - 7 * s, cy, cx - 2 * s, cy + 5 * s, 1.3f * s, kGoldHi, 1.0f);
        stroke(pic_, cx - 2 * s, cy + 5 * s, cx + 8 * s, cy - 6 * s, 1.3f * s, kGoldHi, 1.0f);
    }
    // the options: the current one gold; «Картинка»'s with their pictures
    for (size_t i = 0; i < P.options.size(); ++i) {
        const launcherpick::Option& o = P.options[i];
        const bool current = data_.looks[int(o.choice)].variant == o.variant;
        const bool hot = on(PickPart::Option, int(i));
        panel::roundRect(pic_, pb(o.box), 8 * s, kRaise, hot ? 1.0f : 0.86f, current || hot ? kGold : kLine,
                         current ? 1.0f : hot ? 0.6f : 1.0f, current ? 2 * s : edge);
        if (o.choice != presets::Choice::Picture) continue;
        const int w = int(o.box.w - 10 * s), h = int(float(w) * 9.0f / 16.0f);
        if (w <= 0 || h <= 0) continue;
        if (w != lookW_ || h != lookH_) {
            for (int k = 0; k < 3; ++k) shrink(looks_[k], w, h, lookPx_[k]);
            lookW_ = w;
            lookH_ = h;
        }
        // «Авто» (0) the improved picture darker, «Як у грі» (1), «Покращена» (2), «Ремастер» (3)
        const int k = o.variant == 0 ? 1 : std::clamp(o.variant - 1, 0, 2);
        blitRounded(pic_, o.box.x + 5 * s, o.box.y + 5 * s, 6 * s, lookPx_[k], w, h, o.variant == 0 ? 0.45f : 1.0f);
    }
    // its bottom row: the test, undo (while a pick may be undone), all the settings
    panel::roundRect(pic_, pb(P.test), 8 * s, kRaise, 0.9f, kGold, on(PickPart::Test) ? 0.9f : 0.45f, edge);
    if (data_.undoable)
        panel::roundRect(pic_, pb(P.undo), 8 * s, kGold, on(PickPart::Undo) ? 0.34f : 0.18f, kGold, 0.9f, edge);
    // ✕
    {
        const uint32_t c = on(PickPart::Close) ? kText : kMuted;
        const float cx = P.close.x + P.close.w / 2, cy = P.close.y + P.close.h / 2, r = 6 * s;
        stroke(pic_, cx - r, cy - r, cx + r, cy + r, 1.1f * s, c, 1.0f);
        stroke(pic_, cx - r, cy + r, cx + r, cy - r, 1.1f * s, c, 1.0f);
    }
    // the keyboard's ring
    const std::vector<launcherpick::PickHit> order = pickerOrder();
    if (pickFocus_ >= 0 && pickFocus_ < int(order.size()) && GetFocus() == hwnd_) {
        const Box b = pickerBox(order[size_t(pickFocus_)]);
        if (b.w > 0) panel::roundRect(pic_, pb(grown(b, 3 * s)), 11 * s, 0, 0, kGoldHi, 0.95f, 2 * s);
    }
}

void LauncherView::wordsPicker(HDC dc) {
    namespace presets = settings::presets;
    using launcherpick::PickPart;
    const launcherpick::Picker& P = picker_;
    const float s = P.s;
    const auto on = [&](PickPart part, int option = -1) {
        return pickHover_.part == part && (option < 0 || pickHover_.option == option);
    };
    const Style title{Face::Bold, 16, kText};
    line(dc, title, fit(dc, title, t(Text::LookHead), int(P.title.w)), P.title.x, P.title.y + 17 * s, -1);
    // the card's set
    {
        const float x = P.autoRow.x + 14 * s, width = P.autoRow.w - 28 * s - (data_.fitsCard ? 24 * s : 0);
        const Style head{Face::SemiBold, 14, data_.fitsCard ? kGoldHi : kText};
        line(dc, head, fit(dc, head, t(Text::LauncherPickAuto), int(width)), x, P.autoRow.y + 22 * s, -1);
        const std::wstring note =
            data_.cardName.empty()
                ? t(Text::LauncherPickNoCard)
                : data_.cardName + L": " + t(presets::nameOf(presets::Choice::Picture, data_.card.picture)) +
                      L" \x00B7 " + t(presets::nameOf(presets::Choice::Textures, data_.card.textures));
        const Style small{Face::Medium, 12, kMuted};
        line(dc, small, fit(dc, small, note, int(width)), x, P.autoRow.y + 40 * s, -1);
    }
    // the rows' heads, «змінено N» at their right
    for (int c = 0; c < presets::kChoices; ++c) {
        const Box& h = P.heads[c];
        const float base = h.y + 15 * s;
        line(dc, Style{Face::Bold, 11, kGold, 2}, upper(t(presets::headOf(presets::Choice(c)))), h.x, base, -1);
        const int changed = data_.looks[c].changed;
        if (changed <= 0) continue;
        std::wstring said = tf(Text::LookChanged, {"", std::to_string(changed)});  // «{1} · змінено {2}»
        const size_t at = said.find_first_not_of(L" \x00B7");
        said = at == std::wstring::npos ? said : said.substr(at);
        line(dc, Style{Face::Medium, 11.5f, kMuted}, said, h.x + h.w, base, 1);
    }
    // the options' names: under the pictures (the words before «(»; «Авто»'s «(…)» on its darker picture), in the
    // middle of the others in two lines at most
    for (size_t i = 0; i < P.options.size(); ++i) {
        const launcherpick::Option& o = P.options[i];
        const bool current = data_.looks[int(o.choice)].variant == o.variant;
        const Style st{Face::SemiBold, 12.5f, current ? kGoldHi : on(PickPart::Option, int(i)) ? kWhite : kText};
        const std::wstring full = t(presets::nameOf(o.choice, o.variant));
        const float cx = o.box.x + o.box.w / 2, width = o.box.w - 10 * s;
        if (o.choice == presets::Choice::Picture) {
            const size_t open = full.find(L" (");
            const std::wstring name = open == std::wstring::npos ? full : full.substr(0, open);
            line(dc, st, fit(dc, st, name, int(width)), cx, o.box.y + o.box.h - 9 * s, 0);
            if (open != std::wstring::npos) {  // what it means, on the picture
                std::wstring said = full.substr(open + 2);
                if (!said.empty() && said.back() == L')') said.pop_back();
                const Style small{Face::SemiBold, 11, kWhite};
                const std::vector<std::wstring> lines = wrap(dc, small, said, int(width - 8 * s), 2);
                const float imageH = width * 9.0f / 16.0f, lineH = 14 * s;
                float y = o.box.y + 5 * s + imageH / 2 - lineH * float(lines.empty() ? 0 : lines.size() - 1) / 2 +
                          ascentOf(dc, small) * 0.36f;
                for (const std::wstring& l : lines) {
                    line(dc, small, l, cx, y, 0);
                    y += lineH;
                }
            }
        } else {
            const std::vector<std::wstring> lines = wrap(dc, st, full, int(width), 2);
            const float lineH = 15 * s;
            float y = o.box.y + o.box.h / 2 - lineH * float(lines.empty() ? 0 : lines.size() - 1) / 2 +
                      ascentOf(dc, st) * 0.36f;
            for (const std::wstring& l : lines) {
                line(dc, st, l, cx, y, 0);
                y += lineH;
            }
        }
    }
    // the bottom row
    const Style link{Face::SemiBold, 12.5f, kGoldHi};
    const float base = P.test.y + P.test.h / 2 + ascentOf(dc, link) * 0.36f;
    const Style test{Face::SemiBold, 12.5f, on(PickPart::Test) ? kWhite : kGoldHi};
    line(dc, test, fit(dc, test, t(Text::LauncherPickTest), int(P.test.w - 12 * s)), P.test.x + P.test.w / 2, base, 0);
    if (data_.undoable) {
        const Style undo{Face::SemiBold, 12.5f, on(PickPart::Undo) ? kWhite : kGoldHi};
        line(dc, undo, fit(dc, undo, t(Text::LauncherPickUndo), int(P.undo.w - 12 * s)), P.undo.x + P.undo.w / 2,
             base, 0);
    }
    const Style all{Face::SemiBold, 12.5f, on(PickPart::All) ? kWhite : kGoldHi};
    line(dc, all, fit(dc, all, t(Text::LauncherPickAll), int(P.all.w)), P.all.x + P.all.w, base, 1);
}

// --- the mouse and the keys ---

Hit LauncherView::hitAt(int x, int y) const {
    Hit h = launcherview::hitAt(layout_, float(x), float(y));
    if (h.part == Part::Plate && !plateButton_.contains(float(x), float(y))) h = Hit{};  // the plate's button alone
    return h;
}

std::vector<Hit> LauncherView::focusOrder() const {
    std::vector<Hit> order{Hit{Part::Play}, Hit{Part::PlayMore}, Hit{Part::Look}};
    if (layout_.update.w > 0) order.push_back(Hit{Part::Changelog});
    for (int i = 0; i < layout_.newsShown; ++i) order.push_back(Hit{Part::News, i});
    for (Part p : kDockParts) order.push_back(Hit{p});
    order.push_back(Hit{Part::Version});
    if (layout_.plate.w > 0) order.push_back(Hit{Part::Plate});
    order.push_back(Hit{Part::Language});
    return order;
}

Box LauncherView::boxOf(Hit hit) const {
    const launcherview::Layout& L = layout_;
    switch (hit.part) {
    case Part::Play: return L.play;
    case Part::PlayMore: return L.playMore;
    case Part::Look: return L.look;
    case Part::Changelog: return L.update;
    case Part::News: return hit.index >= 0 && hit.index < L.newsShown ? L.news[hit.index] : Box{};
    case Part::Version: return L.version;
    case Part::Language: return L.language;
    case Part::Plate: return plateButton_;
    default: break;
    }
    for (int i = 0; i < launcherview::kDockIcons; ++i)
        if (kDockParts[i] == hit.part) {
            const Box& d = L.dock[i];
            return Box{d.x + (d.w - 44 * L.s) / 2, d.y, 44 * L.s, 44 * L.s};
        }
    return Box{};
}

void LauncherView::press(Hit hit) {
    if (hit.part == Part::None || !host_) return;
    if (hit.part == Part::Plate && layout_.plate.w <= 0) return;
    if (hit.part == Part::Look) {  // 3.0.18-beta.194: the look picker over the launcher
        openPicker();
        return;
    }
    host_->launcherAction(hit.part, hit.index);
}

LRESULT CALLBACK LauncherView::proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        auto* self = static_cast<LauncherView*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    auto* self = reinterpret_cast<LauncherView*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCDESTROY && self) {
        self->hwnd_ = nullptr;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    return self ? self->handle(msg, wp, lp) : DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT LauncherView::handle(UINT msg, WPARAM wp, LPARAM lp) {
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
    case WM_SIZE: {
        const int dpi = windowDpi(hwnd_);
        if (dpi != dpi_) {
            dpi_ = dpi;
            dropFonts();
        }
        relayout();
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
    }
    case kDpiChangedAfterParent:
        dpi_ = windowDpi(hwnd_);
        dropFonts();
        relayout();
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
    case WM_TIMER:
        if (wp == kArtTimer) tick();
        return 0;
    case WM_MOUSEMOVE: {
        if (!tracking_) {
            TRACKMOUSEEVENT tme{};
            tme.cbSize = sizeof tme;
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd_;
            tracking_ = TrackMouseEvent(&tme) != FALSE;
        }
        if (picking_) {
            const launcherpick::PickHit h =
                launcherpick::pickAt(picker_, float(GET_X_LPARAM(lp)), float(GET_Y_LPARAM(lp)));
            if (h.part != pickHover_.part || h.option != pickHover_.option) {
                pickHover_ = h;
                InvalidateRect(hwnd_, nullptr, FALSE);
            }
            return 0;
        }
        const Hit h = hitAt(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        if (h.part != hover_.part || h.index != hover_.index) {
            hover_ = h;
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        tracking_ = false;
        pickHover_ = launcherpick::PickHit{};
        if (hover_.part != Part::None || picking_) {
            hover_ = Hit{};
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        return 0;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
        SetFocus(hwnd_);
        if (picking_) {
            pickPressed_ = launcherpick::pickAt(picker_, float(GET_X_LPARAM(lp)), float(GET_Y_LPARAM(lp)));
            SetCapture(hwnd_);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        pressed_ = hitAt(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        if (pressed_.part != Part::None) SetCapture(hwnd_);
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
    case WM_LBUTTONUP: {
        if (picking_) {
            const launcherpick::PickHit was = pickPressed_;
            pickPressed_ = launcherpick::PickHit{};
            if (GetCapture() == hwnd_) ReleaseCapture();
            const launcherpick::PickHit h =
                launcherpick::pickAt(picker_, float(GET_X_LPARAM(lp)), float(GET_Y_LPARAM(lp)));
            InvalidateRect(hwnd_, nullptr, FALSE);
            if (h.part == was.part && h.option == was.option) pickerPress(h);
            return 0;
        }
        const Hit was = pressed_;
        pressed_ = Hit{};
        if (GetCapture() == hwnd_) ReleaseCapture();
        InvalidateRect(hwnd_, nullptr, FALSE);
        const Hit h = hitAt(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        if (was.part != Part::None && h.part == was.part && h.index == was.index) press(was);
        return 0;
    }
    case WM_CAPTURECHANGED:
        if (pressed_.part != Part::None) {
            pressed_ = Hit{};
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        return 0;
    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT) {
            const launcherpick::PickPart pp = pickHover_.part;
            const bool hand = picking_ ? pp != launcherpick::PickPart::None && pp != launcherpick::PickPart::Card &&
                                             (pp != launcherpick::PickPart::Undo || data_.undoable)
                                       : hover_.part != Part::None;
            SetCursor(LoadCursorW(nullptr, hand ? IDC_HAND : IDC_ARROW));
            return TRUE;
        }
        break;
    case WM_GETDLGCODE:
        return DLGC_WANTALLKEYS | DLGC_WANTARROWS | DLGC_WANTTAB | DLGC_WANTCHARS;
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
    case WM_KEYDOWN: {
        if (picking_) {  // the picker's keys: Esc closes it, Tab and the arrows walk it, Enter presses
            const std::vector<launcherpick::PickHit> order = pickerOrder();
            const int n = int(order.size());
            if (wp == VK_ESCAPE) {
                closePicker();
            } else if (wp == VK_TAB || wp == VK_LEFT || wp == VK_RIGHT || wp == VK_UP || wp == VK_DOWN) {
                const bool previous = wp == VK_TAB ? GetKeyState(VK_SHIFT) < 0 : (wp == VK_LEFT || wp == VK_UP);
                pickFocus_ = pickFocus_ < 0 ? (previous ? n - 1 : 0) : (pickFocus_ + (previous ? n - 1 : 1)) % n;
                InvalidateRect(hwnd_, nullptr, FALSE);
            } else if ((wp == VK_RETURN || wp == VK_SPACE) && pickFocus_ >= 0 && pickFocus_ < n) {
                pickerPress(order[size_t(pickFocus_)]);
            }
            return 0;
        }
        const std::vector<Hit> order = focusOrder();
        const int n = int(order.size());
        const bool back = GetKeyState(VK_SHIFT) < 0;
        if (wp == VK_TAB || wp == VK_LEFT || wp == VK_RIGHT || wp == VK_UP || wp == VK_DOWN) {
            const bool previous = wp == VK_TAB ? back : (wp == VK_LEFT || wp == VK_UP);
            if (focus_ < 0)
                focus_ = previous ? n - 1 : 0;
            else
                focus_ = (focus_ + (previous ? n - 1 : 1)) % n;
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (wp == VK_RETURN || wp == VK_SPACE) {  // Enter: the ringed part, else the button
            if (focus_ >= 0 && focus_ < n)
                press(order[size_t(focus_)]);
            else if (wp == VK_RETURN)
                press(Hit{Part::Play});
            return 0;
        }
        break;
    }
    case WM_CHAR:
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd_, msg, wp, lp);
}

}  // namespace uagfx::app

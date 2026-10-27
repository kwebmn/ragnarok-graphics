#include "win/answer_card.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

#include "core/answer_card.hpp"
#include "core/layout.hpp"
#include "core/report_answers.hpp"
#include "core/report_texts.hpp"
#include "core/settings/strings.hpp"
#include "pad/win/game_window.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/notifier.hpp"
#include "win/paths.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"

namespace uagfx::win::answercard {
namespace {

using settings::Text;

constexpr ULONGLONG kReadEveryMs = 2000;  // the file looked at (its time) - the watcher writes it every 10 minutes
constexpr float kLine = 18;               // a line of the developer's words at scale 1 (core/answer_card's)

struct State {
    std::vector<reports::Answer> list;  // the file's, newest first
    FILETIME stamp{};
    bool stamped = false;
    ULONGLONG readAt = 0;
    std::set<std::string> handled;  // opened or put off in this game (answers::key)
    std::set<std::string> told;     // shown, or told from behind
    reports::Answer current;
    bool shown = false, dirty = true, upload = true;
    card::Geometry geometry;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0, x = 0, y = 0, targetW = 0, targetH = 0;
    float k = 0, scale = 1;
    card::Part hover = card::Part::None, pressed = card::Part::None;
};

State& state() {
    static State s;
    return s;
}

std::string file() { return layout::join(gameDir(), layout::kAnswers); }

void readFile(State& s) {
    const ULONGLONG now = GetTickCount64();
    if (s.readAt && now - s.readAt < kReadEveryMs) return;
    s.readAt = now;
    WIN32_FILE_ATTRIBUTE_DATA a{};
    if (!GetFileAttributesExA(file().c_str(), GetFileExInfoStandard, &a)) {
        if (s.stamped) s.list.clear();  // taken away: none
        s.stamped = false;
        return;
    }
    if (s.stamped && CompareFileTime(&a.ftLastWriteTime, &s.stamp) == 0) return;
    std::string text;
    std::vector<reports::Answer> list;
    if (!readTextFile(file(), text) || !answers::read(text, list)) return;  // half written: the next look
    s.stamp = a.ftLastWriteTime;
    s.stamped = true;
    s.list = list;
    if (!list.empty())
        LOG_INFO("answers: %zu not seen yet - the newest %s (%s)", list.size(), list.front().code.c_str(),
                 reports::statusToken(list.front().status));
}

const reports::Answer* next(const State& s) {
    for (const reports::Answer& a : s.list)
        if (!s.handled.count(answers::key(a))) return &a;
    return nullptr;
}

// Behind other windows: each new answer told once by a Windows notification (when [notify] wants it)
void tellBehind(State& s) {
    for (const reports::Answer& a : s.list) {
        const std::string key = answers::key(a);
        if (s.handled.count(key) || s.told.count(key)) continue;
        s.told.insert(key);
        notifier::answer(a.code, reporttexts::status(startup().lang, a.status, a.fixedIn));
    }
}

void hide(State& s) {
    if (!s.shown) return;
    s.shown = false;
    s.hover = s.pressed = card::Part::None;
    uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Card, nullptr, false);
}

uint32_t chipWords(reports::Status st) {  // the status's name, lighter than its chip
    if (st == reports::Status::NeedInfo) return 0xFFC58A;
    if (st == reports::Status::Fixed) return 0x8FE3A9;
    return 0xA9C4FF;
}

// The answer's time in this computer's clock: "00:31" today, else "26.10 18:44"; "" unknown
std::string clock(const std::string& iso) {
    reports::Civil u;
    if (!reports::utcOf(iso, u)) return "";
    SYSTEMTIME t{};
    t.wYear = WORD(u.year);
    t.wMonth = WORD(u.month);
    t.wDay = WORD(u.day);
    t.wHour = WORD(u.hour);
    t.wMinute = WORD(u.minute);
    t.wSecond = WORD(u.second);
    SYSTEMTIME local{}, now{};
    if (!SystemTimeToTzSpecificLocalTime(nullptr, &t, &local)) local = t;
    GetLocalTime(&now);
    char text[32];
    if (now.wYear == local.wYear && now.wMonth == local.wMonth && now.wDay == local.wDay)
        std::snprintf(text, sizeof text, "%02u:%02u", unsigned(local.wHour), unsigned(local.wMinute));
    else
        std::snprintf(text, sizeof text, "%02u.%02u %02u:%02u", unsigned(local.wDay), unsigned(local.wMonth),
                      unsigned(local.wHour), unsigned(local.wMinute));
    return text;
}

void render(State& s) {
    const settings::Lang lang = startup().lang;
    const reports::Answer& a = s.current;
    const float sc = s.scale;
    const float titlePx = 13 * sc, chipPx = 11.5f * sc, quotePx = 13 * sc, buttonPx = 12.5f * sc, timePx = 11.5f * sc;
    const std::wstring title = utf8Wide(settings::tr(lang, Text::CardTitle));
    const std::wstring status = utf8Wide(reporttexts::status(lang, a.status, a.fixedIn));
    const std::wstring openShort = utf8Wide(settings::tr(lang, Text::AnswerOpen));
    const std::wstring openFull = openShort + L" · Ctrl+Shift+F10";
    const std::wstring later = utf8Wide(settings::tr(lang, Text::CardLater));
    card::Widths w{float(wordsWidth(status, chipPx, true) + 2), float(wordsWidth(openFull, buttonPx, true) + 2),
                   float(wordsWidth(later, buttonPx, false) + 2)};
    card::Geometry g = card::geometry(sc, s.targetW, 2, w);
    bool full = true;
    if (g.later.x + g.later.w > float(g.width) - 14 * sc) {  // a narrow picture: the key's name off the button
        full = false;
        w.open = float(wordsWidth(openShort, buttonPx, true) + 2);
        g = card::geometry(sc, s.targetW, 2, w);
    }
    const float quoteW = g.quote.w;
    const auto fits = [&](const std::string& line) {
        return float(wordsWidth(utf8Wide(line), quotePx, false)) <= quoteW;
    };
    std::vector<std::string> lines = card::fitLines(a.text.empty() ? reporttexts::hint(lang, a) : a.text, 2, fits);
    if (lines.empty()) lines.push_back(reporttexts::hint(lang, a));
    g = card::geometry(sc, s.targetW, int(lines.size()), w);
    card::paint(s.picture, g, a.status, s.hover);
    std::vector<Words> words;
    const float titleW = std::min(float(wordsWidth(title, titlePx, true) + 2), g.title.w);
    words.push_back(Words{panel::Box{g.title.x, g.title.y, titleW, g.title.h}, title, panel::kText, titlePx, true,
                          DT_LEFT});
    const float codeX = g.title.x + titleW + 6 * sc, codeW = g.title.x + g.title.w - codeX;
    if (codeW > 0)
        words.push_back(Words{panel::Box{codeX, g.title.y, codeW, g.title.h}, utf8Wide(a.code), panel::kMuted, titlePx,
                              false, DT_LEFT});
    words.push_back(Words{g.chip, status, chipWords(a.status), chipPx, true, DT_CENTER});
    for (size_t i = 0; i < lines.size(); ++i)
        words.push_back(Words{panel::Box{g.quote.x, g.quote.y + float(i) * kLine * sc, g.quote.w, kLine * sc},
                              utf8Wide(lines[i]), 0xD6DDE9, quotePx, false, DT_LEFT});
    words.push_back(Words{g.open, full ? openFull : openShort, 0xFFFFFF, buttonPx, true, DT_CENTER});
    words.push_back(Words{g.later, later, 0xD6DDE9, buttonPx, false, DT_CENTER});
    const std::string when = clock(a.at);
    if (!when.empty() && g.time.w > 0)
        words.push_back(Words{g.time, utf8Wide(settings::fmt(settings::tr(lang, Text::CardFrom), {when})),
                              panel::kMuted, timePx, false, DT_RIGHT});
    drawWords(s.picture, words);
    s.geometry = g;
    s.dirty = false;
    s.upload = true;
}

bool toTarget(const State& s, HWND game, int x, int y, float& tx, float& ty) {
    RECT client{};
    if (!GetClientRect(game, &client) || client.right <= 0 || client.bottom <= 0) return false;
    tx = float(x) * float(s.targetW) / float(client.right);  // Windows' size of the window (uaRO.exe is not DPI-aware)
    ty = float(y) * float(s.targetH) / float(client.bottom);
    return true;
}

void block(const State& s, HWND game) {  // the pad's clicks there are the card's
    RECT client{};
    if (!GetClientRect(game, &client) || s.targetW <= 0 || s.targetH <= 0) return;
    const RECT r{LONG(int64_t(s.x) * client.right / s.targetW), LONG(int64_t(s.y) * client.bottom / s.targetH),
                 LONG(int64_t(s.x + s.geometry.width) * client.right / s.targetW + 1),
                 LONG(int64_t(s.y + s.geometry.height) * client.bottom / s.targetH + 1)};
    uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Card, &r, false);
}

void open(State& s, const char* by) {
    const reports::Answer a = s.current;
    s.handled.insert(answers::key(a));
    hide(s);
    LOG_INFO("answers: %s opened by %s - «Звернення» on it", a.code.c_str(), by);
    startAnswer(a.code);
}

void later(State& s) {
    s.handled.insert(answers::key(s.current));
    hide(s);
    LOG_INFO("answers: %s put off until the next game («Пізніше»)", s.current.code.c_str());
}

}  // namespace

void frame(HWND game, double k, int targetWidth, int targetHeight) {
    State& s = state();
    readFile(s);
    const bool front = game && GetForegroundWindow() == game;
    if (game && !front) tellBehind(s);
    const reports::Answer* a = front && k > 0 && targetWidth > 0 && targetHeight > 0 ? next(s) : nullptr;
    if (!a) {
        hide(s);
        return;
    }
    const std::string key = answers::key(*a);
    if (!s.shown || key != answers::key(s.current)) {
        if (key != answers::key(s.current) || !s.told.count(key))
            LOG_INFO("answers: the card shown - %s (%s)", a->code.c_str(), reports::statusToken(a->status));
        s.current = *a;
        s.dirty = true;
        s.hover = s.pressed = card::Part::None;
    }
    s.told.insert(key);  // seen in the game: no notification of it later
    const float scale = std::clamp(float(k), 0.75f, 6.0f);
    if (float(k) != s.k || scale != s.scale || targetWidth != s.targetW || targetHeight != s.targetH) {
        s.k = float(k);
        s.scale = scale;
        s.targetW = targetWidth;
        s.targetH = targetHeight;
        s.dirty = true;
    }
    if (s.dirty) render(s);
    const panel::Point p = card::position(s.geometry, s.targetW);
    s.x = p.x;
    s.y = p.y;
    s.shown = true;
    block(s, game);
}

void behind(HWND game) {
    State& s = state();
    readFile(s);
    if (game) tellBehind(s);
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.shown || s.picture.width <= 0 || s.picture.height <= 0 || s.k <= 0) return;
    // under the game's own cursor, as the windows' panel: the cursor's draw moved to the frame's end
    int cursor = -1, cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) cursor = panel::cursorDraw(record, float(cx), float(cy));
    drawPicture(record, renderer, s.picture, s.texture, s.texW, s.texH, s.upload, s.x, s.y, s.k);
    if (cursor >= 0) record.moveLastBefore(record.commands().size() - size_t(cursor) - 1, size_t(cursor));
}

bool mouse(HWND game, UINT m, LPARAM lp, LRESULT& result) {
    State& s = state();
    if (!s.shown) return false;
    const bool left = m == WM_LBUTTONDOWN || m == WM_LBUTTONUP || m == WM_LBUTTONDBLCLK;
    const bool other = m == WM_RBUTTONDOWN || m == WM_RBUTTONUP || m == WM_RBUTTONDBLCLK || m == WM_MBUTTONDOWN ||
                       m == WM_MBUTTONUP;
    if (!left && !other && m != WM_MOUSEMOVE) return false;
    float tx = 0, ty = 0;
    if (!toTarget(s, game, GET_X_LPARAM(lp), GET_Y_LPARAM(lp), tx, ty)) return false;
    const card::Part part = card::hitTest(s.geometry, tx - float(s.x), ty - float(s.y));
    if (m == WM_MOUSEMOVE) {  // the hover drawn; the game's own cursor goes on moving
        const card::Part look = part == card::Part::Body ? card::Part::None : part;
        if (look != s.hover) {
            s.hover = look;
            s.dirty = true;
        }
        return false;
    }
    if (part == card::Part::None) {
        if (m == WM_LBUTTONUP) s.pressed = card::Part::None;
        return false;
    }
    if (m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK) s.pressed = part;
    if (m == WM_LBUTTONUP) {
        const card::Part was = s.pressed;
        s.pressed = card::Part::None;
        if (was == part && part == card::Part::Later)
            later(s);
        else if (was == part)
            open(s, "a click");
    }
    result = 0;
    return true;
}

bool openByKey() {
    State& s = state();
    if (!s.shown) return false;
    open(s, "Ctrl+Shift+F10");
    return true;
}

}  // namespace uagfx::win::answercard

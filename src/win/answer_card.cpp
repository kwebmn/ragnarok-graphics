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
#include "win/uaui.hpp"
#include "win/ui_sheet.hpp"
#include "win/window_input.hpp"

namespace uagfx::win::answercard {
namespace {

using settings::Text;

constexpr ULONGLONG kReadEveryMs = 2000;  // the file looked at (its time) - the watcher writes it every 10 minutes
constexpr float kLine = 18;               // a line of the developer's words at scale 1 (core/answer_card's)

struct State {
    uisheet::Sheet sheet;  // 3.0.18-beta.31: the card drawn by uaUI
    int sheetGen = 0;
    bool sheetShown = false;
    std::vector<Words> words;
    std::vector<reports::Answer> list;  // the file's, newest first
    FILETIME stamp{};
    bool stamped = false;
    ULONGLONG readAt = 0;
    // 3.0.20-beta.17: the newest answer put off - opened, «Пізніше» or the card gone by itself (answers-later.txt)
    long long putOff = 0;
    bool putOffRead = false;
    std::set<std::string> told;  // shown, or told from behind
    reports::Answer current;
    size_t more = 0;  // 3.0.20-beta.17: the card's other new answers
    long long newestId = 0;
    ULONGLONG shownAt = 0;
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
std::string laterFile() { return layout::join(gameDir(), layout::kAnswersLater); }

void readPutOff(State& s) {
    if (s.putOffRead) return;
    s.putOffRead = true;
    std::string text;
    if (readTextFile(laterFile(), text)) s.putOff = answers::putOffOf(text);
}

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

// the answers the card stands for (not put off) marked told: no notification of them later
void markTold(State& s) {
    for (const reports::Answer& a : s.list)
        if (a.id > 0 ? a.id > s.putOff : s.putOff == 0) s.told.insert(answers::key(a));
}

// Behind other windows: the new answers told by one Windows notification - the card's (when [notify] wants it)
void tellBehind(State& s) {
    const answers::Card c = answers::cardOf(s.list, s.putOff);
    if (!c.shown || s.told.count(answers::key(*c.shown))) return;
    markTold(s);
    notifier::answer(c.shown->code, reporttexts::status(language(), c.shown->status, c.shown->fixedIn));
}

void hide(State& s) {
    if (!s.shown) return;
    s.shown = false;
    s.hover = s.pressed = card::Part::None;
    uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Card, nullptr, nullptr, false);
}

// 3.0.20-beta.17: the card's answers put off up to the newest of them - kept between games
void putOffNow(State& s) {
    if (s.newestId > s.putOff) {
        s.putOff = s.newestId;
        if (!writeFileAtomic(laterFile(), answers::putOffText(s.putOff)))
            LOG_INFO("answers: %s could not be written - the card may come again next game", layout::kAnswersLater);
    }
    hide(s);
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
    const settings::Lang lang = language();
    const reports::Answer& a = s.current;
    const float sc = s.scale;
    const float titlePx = 13 * sc, chipPx = 11.5f * sc, quotePx = 13 * sc, buttonPx = 12.5f * sc, timePx = 11.5f * sc;
    // 3.0.20-beta.17: «Відповіді на звернення: N» for several; «Відкрити» by a click alone (Ctrl+Shift+F10 is always
    // «Нове звернення»)
    const std::wstring title =
        utf8Wide(s.more ? settings::fmt(settings::tr(lang, Text::CardTitleMany), {std::to_string(s.more + 1)})
                        : std::string(settings::tr(lang, Text::CardTitle)));
    const std::wstring status = utf8Wide(reporttexts::status(lang, a.status, a.fixedIn));
    const std::wstring openShort = utf8Wide(settings::tr(lang, Text::AnswerOpen));
    const std::wstring later = utf8Wide(settings::tr(lang, Text::CardLater));
    card::Widths w{float(wordsWidth(status, chipPx, true) + 2), float(wordsWidth(openShort, buttonPx, true) + 2),
                   float(wordsWidth(later, buttonPx, false) + 2)};
    card::Geometry g = card::geometry(sc, s.targetW, 2, w);
    const float quoteW = g.quote.w;
    const auto fits = [&](const std::string& line) {
        return float(wordsWidth(utf8Wide(line), quotePx, false)) <= quoteW;
    };
    std::vector<std::string> lines = card::fitLines(a.text.empty() ? reporttexts::hint(lang, a) : a.text, 2, fits);
    if (lines.empty()) lines.push_back(reporttexts::hint(lang, a));
    g = card::geometry(sc, s.targetW, int(lines.size()), w);
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
    words.push_back(Words{g.open, openShort, 0xFFFFFF, buttonPx, true, DT_CENTER});
    words.push_back(Words{g.later, later, 0xD6DDE9, buttonPx, false, DT_CENTER});
    const std::string when = clock(a.at);
    if (!when.empty() && g.time.w > 0)
        words.push_back(Words{g.time, utf8Wide(settings::fmt(settings::tr(lang, Text::CardFrom), {when})),
                              panel::kMuted, timePx, false, DT_RIGHT});
    s.geometry = g;
    s.dirty = false;
    s.words = std::move(words);  // uaUI's card (.158: no picture)
    ++s.sheetGen;
}

// 3.0.18-beta.31 (docs/superpowers/plans/2026-11-09-uaui-migration.md, stage 5 - by hand): the answer's card in
// RmlUi's markup, line by line as core answercard paints it - its ground, the status' accent, dot and chip, «Відкрити»
// blue, «Пізніше»; the words as the card lays them out
void fillSheet(State& s, uisheet::Content& c) {
    const card::Geometry& g = s.geometry;
    const float sc = g.scale, edge = std::max(1.0f, sc);
    const uint32_t colour = card::statusColour(s.current.status);
    auto& u = c.under;
    c.w = float(g.width), c.h = float(g.height);
    constexpr uint32_t kGround = 0x0E121A, kBlue = 0x2F6FD1, kBlueHover = 0x3D7FE0;  // core card's
    c.rect(u, panel::Box{0, 0, float(g.width), float(g.height)}, g.radius, kGround, 0.90f, 0xFFFFFF, 0.12f, edge);
    c.rect(u, g.accent, 1.5f * sc, colour, 1.0f);
    c.rect(u, g.dot, g.dot.w / 2, colour, 1.0f);
    c.rect(u, g.chip, 6 * sc, colour, 0.20f);
    c.rect(u, g.open, 7 * sc, s.hover == card::Part::Open ? kBlueHover : kBlue, 1.0f);
    c.rect(u, g.later, 7 * sc, 0xFFFFFF, s.hover == card::Part::Later ? 0.10f : 0.0f, 0xFFFFFF, 0.24f, edge);
    uisheet::sayGdi(c, s.words);
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
    const RECT r = clientBox(client, s.targetW, s.targetH, s.x, s.y, s.geometry.width, s.geometry.height);
    uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Card, game, &r, false);
}

void open(State& s, const char* by) {
    const reports::Answer a = s.current;
    putOffNow(s);  // all of the card's: «Звернення» shows the others with their dots
    LOG_INFO("answers: %s opened by %s - «Звернення» on it", a.code.c_str(), by);
    startAnswer(a.code);
}

void later(State& s) {
    LOG_INFO("answers: %s and %zu more put off («Пізніше») until a newer one", s.current.code.c_str(), s.more);
    putOffNow(s);
}

}  // namespace

static void frameInner(HWND game, double k, int targetWidth, int targetHeight) {
    State& s = state();
    readFile(s);
    readPutOff(s);
    const bool front = game && GetForegroundWindow() == game;
    if (game && !front) tellBehind(s);
    const answers::Card c = front && k > 0 && targetWidth > 0 && targetHeight > 0 ? answers::cardOf(s.list, s.putOff)
                                                                                  : answers::Card{};
    if (!c.shown) {
        hide(s);
        return;
    }
    const reports::Answer* a = c.shown;
    const std::string key = answers::key(*a);
    if (!s.shown || key != answers::key(s.current) || c.more != s.more) {
        if (key != answers::key(s.current) || !s.told.count(key))
            LOG_INFO("answers: the card shown - %s (%s), %zu more%s", a->code.c_str(), reports::statusToken(a->status),
                     c.more, c.waits ? " - it waits for the player" : "");
        s.current = *a;
        s.more = c.more;
        s.dirty = true;
        s.hover = s.pressed = card::Part::None;
        s.shownAt = GetTickCount64();
    }
    s.newestId = c.newestId;
    markTold(s);  // seen in the game: no notification of them later
    // 3.0.20-beta.17: nothing the developer waits on - the card goes by itself (not under the pointer)
    if (!c.waits && s.hover == card::Part::None && s.pressed == card::Part::None &&
        GetTickCount64() - s.shownAt >= answers::kCardShowsMs) {
        LOG_INFO("answers: the card went by itself - %s and %zu more put off", a->code.c_str(), c.more);
        putOffNow(s);
        return;
    }
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

void frame(HWND game, double k, int targetWidth, int targetHeight) {
    frameInner(game, k, targetWidth, targetHeight);
    State& s = state();
    if (uaui::readyAnywhere() && s.shown && s.words.empty()) s.dirty = true;  // painted for the picture before: again
    if (uaui::readyAnywhere() && s.dirty && s.shown) render(s);
    s.sheet.anywhere = true;  // 3.0.18-beta.78: the developer's answer on the login's screens too
    s.sheetShown = s.sheet.frame("answer", s.shown, true, s.x, s.y, s.sheetGen,
                                 [&](uisheet::Content& c) { fillSheet(s, c); });
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.shown || s.sheetShown || s.picture.width <= 0 || s.picture.height <= 0 || s.k <= 0) return;
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

}  // namespace uagfx::win::answercard

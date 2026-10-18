#include "studio/window.hpp"

#include <commctrl.h>

#include <cstdio>

#include "app/text.hpp"
#include "app/ui.hpp"
#include "studio/messages.hpp"
#include "studio/places.hpp"

namespace uagfx::studio {
namespace {

constexpr UINT_PTR kBarTimer = 1;  // the engine looked at every two seconds

bool readWhole(const std::wstring& path, std::string& out) {
    std::FILE* f = _wfopen(path.c_str(), L"rb");
    if (!f) return false;
    out.clear();
    char buf[1 << 14];
    size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
    std::fclose(f);
    return true;
}

}  // namespace

std::wstring StudioWindow::t(settings::Text x) const { return app::wtr(lang_, x); }

bool StudioWindow::create(HINSTANCE instance, int show) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = &StudioWindow::proc;
    wc.hInstance = instance;
    wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
    wc.hIconSm = wc.hIcon;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = kWindowClass;
    if (!RegisterClassExW(&wc)) return false;
    loadSession();
    const bool placed = session_.x != -1 && session_.w > 200 && session_.h > 200;
    hwnd_ = CreateWindowExW(0, kWindowClass, t(settings::Text::StudioTitle).c_str(),
                            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, placed ? session_.x : CW_USEDEFAULT,
                            placed ? session_.y : CW_USEDEFAULT, placed ? session_.w : 1400, placed ? session_.h : 900,
                            nullptr, nullptr, instance, this);
    if (!hwnd_) return false;
    ShowWindow(hwnd_, show);
    return true;
}

void StudioWindow::loadSession() {
    std::string text, error;
    json::Value v;
    if (readWhole(studioRoot() + L"\\session.json", text) && json::parse(text, v, error)) session_ = sessionOf(v);
}

void StudioWindow::saveSession() {
    RECT r{};
    if (hwnd_ && !IsIconic(hwnd_) && !IsZoomed(hwnd_) && GetWindowRect(hwnd_, &r)) {
        session_.x = r.left;
        session_.y = r.top;
        session_.w = r.right - r.left;
        session_.h = r.bottom - r.top;
    }
    session_.paused = jobs_ && jobs_->paused();
    const std::wstring path = studioRoot() + L"\\session.json";
    makeFolder(studioRoot());
    if (std::FILE* f = _wfopen((path + L".part").c_str(), L"wb")) {
        const std::string text = json::dump(toJson(session_));
        const bool ok = std::fwrite(text.data(), 1, text.size(), f) == text.size();
        if (std::fclose(f) == 0 && ok) MoveFileExW((path + L".part").c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING);
    }
}

void StudioWindow::build() {
    dpi_ = app::windowDpi(hwnd_);
    font_ = app::makeFont(dpi_, false);
    const auto make = [&](const wchar_t* cls, const std::wstring& text, DWORD style, int id) {
        HWND h = CreateWindowExW(0, cls, text.c_str(), WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10, hwnd_,
                                 reinterpret_cast<HMENU>(INT_PTR(id)), nullptr, nullptr);
        SendMessageW(h, WM_SETFONT, WPARAM(font_), TRUE);
        return h;
    };
    tabs_ = make(WC_TABCONTROLW, L"", WS_CLIPSIBLINGS, kTabs);
    const settings::Text names[3] = {settings::Text::StudioTabTextures, settings::Text::StudioTabPack,
                                     settings::Text::StudioTabLog};
    for (int i = 0; i < 3; ++i) {
        std::wstring name = t(names[i]);
        TCITEMW item{};
        item.mask = TCIF_TEXT;
        item.pszText = &name[0];
        SendMessageW(tabs_, TCM_INSERTITEMW, WPARAM(i), LPARAM(&item));
    }
    pause_ = make(L"BUTTON", t(settings::Text::StudioPause), BS_PUSHBUTTON, kPauseButton);
    engineLabel_ = make(L"STATIC", L"", SS_LEFT | SS_NOPREFIX | SS_ENDELLIPSIS, kEngineLabel);
    jobLabel_ = make(L"STATIC", L"", SS_LEFT | SS_NOPREFIX | SS_ENDELLIPSIS, kJobLabel);
    jobBar_ = make(PROGRESS_CLASSW, L"", 0, kJobBar);
    SendMessageW(jobBar_, PBM_SETRANGE32, 0, 1000);
    stop_ = make(L"BUTTON", t(settings::Text::StudioStop), BS_PUSHBUTTON, kStopButton);
    jobs_ = std::make_unique<Jobs>(hwnd_, gameDir_);
    if (session_.paused) jobs_->pause();
    SetTimer(hwnd_, kBarTimer, 2000, nullptr);
    showBar();
}

void StudioWindow::layout() {
    RECT r{};
    GetClientRect(hwnd_, &r);
    const auto at = [&](int v) { return app::px(v, dpi_); };
    const int barH = at(40), w = r.right, h = r.bottom, y = h - barH + at(8), row = at(24);
    MoveWindow(tabs_, at(4), at(4), w - at(8), h - barH - at(8), TRUE);
    MoveWindow(pause_, at(8), y, at(110), row, TRUE);
    MoveWindow(engineLabel_, at(126), y + at(4), at(300), row, TRUE);
    MoveWindow(jobBar_, at(436), y + at(4), at(200), at(16), TRUE);
    MoveWindow(jobLabel_, at(646), y + at(4), std::max(at(50), w - at(646) - at(100)), row, TRUE);
    MoveWindow(stop_, w - at(90), y, at(80), row, TRUE);
}

void StudioWindow::showBar() {
    if (!jobs_) return;
    SetWindowTextW(pause_, t(jobs_->paused() ? settings::Text::StudioResume : settings::Text::StudioPause).c_str());
    std::wstring engine;
    const std::string name = jobs_->engineName();
    if (jobs_->paused()) engine = t(settings::Text::StudioPaused);
    else if (name.empty()) engine = t(settings::Text::StudioEngineNone);
    else engine = app::wtrf(lang_, settings::Text::StudioEngine, {name});
    if (!jobs_->paused() && jobs_->engineWaiting()) engine += L" - " + t(settings::Text::StudioEngineNewer);
    SetWindowTextW(engineLabel_, engine.c_str());
    const JobProgress p = jobs_->progress();
    size_t waiting = 0;
    for (const Job& j : jobs_->snapshot()) waiting += j.state == Job::Queued;
    std::wstring job;
    if (p.id) {
        std::string what = p.stage.empty() ? p.current : p.stage + (p.current.empty() ? "" : " - " + p.current);
        if (p.total) what += " (" + std::to_string(p.done) + "/" + std::to_string(p.total) + ")";
        job = app::wtrf(lang_, settings::Text::StudioJobRunning, {p.label.empty() ? p.op : p.label, what});
    } else {
        job = t(settings::Text::StudioJobsIdle);
    }
    if (waiting) job += L"; " + app::wtrf(lang_, settings::Text::StudioJobsWaiting, {std::to_string(waiting)});
    SetWindowTextW(jobLabel_, job.c_str());
    SendMessageW(jobBar_, PBM_SETPOS, WPARAM(p.total ? p.done * 1000 / p.total : 0), 0);
    EnableWindow(stop_, p.id != 0);
}

void StudioWindow::togglePause() {
    if (jobs_->paused()) jobs_->resume();
    else jobs_->pause();
    saveSession();
    showBar();
}

LRESULT CALLBACK StudioWindow::proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        auto* self = static_cast<StudioWindow*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    auto* self = reinterpret_cast<StudioWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    return self ? self->handle(msg, wp, lp) : DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT StudioWindow::handle(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE:
            build();
            layout();
            return 0;
        case WM_SIZE:
            if (tabs_) layout();
            return 0;
        case WM_COMMAND:
            if (LOWORD(wp) == kPauseButton) togglePause();
            if (LOWORD(wp) == kStopButton && jobs_) jobs_->stop(jobs_->progress().id);
            return 0;
        case WM_TIMER:
            if (wp == kBarTimer) showBar();
            return 0;
        case kJobsChanged:
        case kEngineGone:
            showBar();
            return 0;
        case kEngineLog:
            delete reinterpret_cast<std::string*>(lp);  // the Log tab takes it (Task 18)
            return 0;
        case WM_CLOSE:
            saveSession();
            DestroyWindow(hwnd_);
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd_, kBarTimer);
            jobs_.reset();  // the engine ends, the queue is written
            if (font_) DeleteObject(font_);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd_, msg, wp, lp);
}

}  // namespace uagfx::studio

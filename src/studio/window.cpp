#include "studio/window.hpp"

#include <commctrl.h>

#include <algorithm>
#include <cstdio>

#include "app/text.hpp"
#include "app/ui.hpp"
#include "core/ai/chain.hpp"
#include "core/studio_link.hpp"
#include "studio/make_dialog.hpp"
#include "studio/making.hpp"
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
    catalog_ = std::make_unique<CatalogWork>(gameDir_);
    thumbs_ = std::make_unique<Thumbs>(hwnd_, *catalog_);
    textures_ = std::make_unique<TexturesTab>(hwnd_, lang_, font_, dpi_, gameDir_, *catalog_, *thumbs_);
    inspector_ = std::make_unique<Inspector>(hwnd_, lang_, font_, dpi_, gameDir_, *catalog_);
    textures_->onSelect = [this](const Row& row) { inspector_->showRow(row, textures_->orphans()); };
    addPictureWork(*jobs_, *catalog_, lang_);
    pack_ = std::make_unique<PackTab>(hwnd_, lang_, font_, dpi_, *catalog_, *jobs_);
    log_ = std::make_unique<LogTab>(hwnd_, font_);
    inspector_->onMake = [this] { makeAgain(); };
    inspector_->onKeep = [this] { keep(); };
    inspector_->onDiscard = [this] { discard(); };
    inspector_->onTakeOut = [this] { takeOut(); };
    inspector_->onOwn = [this] { toggleOwn(); };
    inspector_->onMaterial = [this](const pack::MaterialScales& s) {  // T7: the shown texture's material
        const Shown* shown = inspector_->shown();
        if (!shown || shown->key.empty()) return;
        json::Value args = json::Value::object();
        args.set("key", shown->key);
        args.set("gloss", double(s.gloss));
        args.set("relief", double(s.relief));
        args.set("height", double(s.height));
        jobs_->add("material", args, true,
                   app::utf8(app::wtrf(lang_, settings::Text::StudioJobMaterial,
                                       {utf8OfGame(shown->file.substr(shown->file.rfind('\\') + 1))})));
    };
    tab_ = 0;  // the textures at the start (the Pack tab counts the pack: it waits until it is opened)
    TabCtrl_SetCurSel(tabs_, tab_);
    showTab(tab_);
    if (session_.paused) jobs_->pause();
    else catalog_->update(hwnd_);
    SetTimer(hwnd_, kBarTimer, 2000, nullptr);
    // L2: the game's Ctrl+Shift+F8 sets this event after writing link\pick.txt
    pickEvent_ = CreateEventW(nullptr, FALSE, FALSE, app::wide(kPickEvent).c_str());
    if (pickEvent_) pickThread_ = CreateThread(nullptr, 0, &StudioWindow::pickMain, this, 0, nullptr);
    showBar();
}

void StudioWindow::layout() {
    RECT r{};
    GetClientRect(hwnd_, &r);
    const auto at = [&](int v) { return app::px(v, dpi_); };
    const int barH = at(40), w = r.right, h = r.bottom, y = h - barH + at(8), row = at(24);
    MoveWindow(tabs_, at(4), at(4), w - at(8), h - barH - at(8), TRUE);
    RECT page{at(4), at(4), w - at(4), h - barH - at(4)};  // the tab's page, in the window's pixels
    TabCtrl_AdjustRect(tabs_, FALSE, &page);
    if (pack_) pack_->layout(page);
    if (log_) log_->layout(page);
    if (textures_ && inspector_) {  // the list on the left, the inspector on the right
        RECT left = page, right = page;
        left.right = page.left + (page.right - page.left) * 56 / 100;
        right.left = left.right + at(8);
        textures_->layout(left);
        inspector_->layout(right);
    }
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
    uint64_t catalogDone = 0, catalogTotal = 0;
    if (catalog_) catalog_->count(catalogDone, catalogTotal);
    const std::string catalogStage = catalog_ ? catalog_->stage() : std::string();
    if (!p.id && !catalogStage.empty()) {  // the catalog's update, when no job runs
        std::string what = catalogStage;
        if (catalogTotal) what += " (" + std::to_string(catalogDone) + "/" + std::to_string(catalogTotal) + ")";
        job = app::wtrf(lang_, settings::Text::StudioJobRunning, {"catalog", what});
        SendMessageW(jobBar_, PBM_SETPOS, WPARAM(catalogTotal ? catalogDone * 1000 / catalogTotal : 0), 0);
    } else if (p.id) {
        std::string what = p.stage.empty() ? p.current : p.stage + (p.current.empty() ? "" : " - " + p.current);
        if (p.total) what += " (" + std::to_string(p.done) + "/" + std::to_string(p.total) + ")";
        job = app::wtrf(lang_, settings::Text::StudioJobRunning, {p.label.empty() ? p.op : p.label, what});
    } else {
        job = lastError_.empty() ? t(settings::Text::StudioJobsIdle) : lastError_;
    }
    if (waiting) job += L"; " + app::wtrf(lang_, settings::Text::StudioJobsWaiting, {std::to_string(waiting)});
    SetWindowTextW(jobLabel_, job.c_str());
    if (p.id || catalogStage.empty())
        SendMessageW(jobBar_, PBM_SETPOS, WPARAM(p.total ? p.done * 1000 / p.total : 0), 0);
    EnableWindow(stop_, p.id != 0);
}

void StudioWindow::showTab(int tab) {
    tab_ = tab;
    session_.tab = tab == 1 ? "pack" : tab == 2 ? "log" : "textures";
    if (textures_) textures_->show(tab == 0);
    if (inspector_) inspector_->show(tab == 0);
    if (pack_) pack_->show(tab == 1);
    if (log_) log_->show(tab == 2);
}

void StudioWindow::makeAgain() {
    const auto cat = catalog_->catalog();
    if (!cat) return;
    std::vector<Row> rows = textures_->selection();
    if (rows.empty() && inspector_->shown()) rows.push_back(inspector_->shown()->row);
    rows.erase(std::remove_if(rows.begin(), rows.end(),
                              [&](const Row& r) { return r.file == Row::kNoFile || r.file >= cat->files.size(); }),
               rows.end());
    if (rows.empty()) return;
    ai::Chain chain;
    if (session_.chain.empty() || !ai::parseChain(session_.chain, chain)) chain = ai::Chain();
    const catalog::FileEntry& first = cat->files[rows[0].file];
    const std::wstring what = rows.size() == 1
                                  ? wideOfGame(first.name.substr(first.name.rfind('\\') + 1))
                                  : app::wtrf(lang_, settings::Text::StudioMakeMany, {std::to_string(rows.size())});
    if (!askChain(hwnd_, lang_, font_, dpi_, what, first.grid, chain)) return;
    session_.chain = ai::chainText(chain);
    for (const Row& r : rows) {
        const catalog::FileEntry& f = cat->files[r.file];
        json::Value args = json::Value::object();
        args.set("file", f.name);
        args.set("key", f.pictures[r.picture].key);
        args.set("chain", session_.chain);
        jobs_->add("make", args, false,
                   app::utf8(app::wtrf(lang_, settings::Text::StudioJobMake,
                                       {utf8OfGame(f.name.substr(f.name.rfind('\\') + 1))})));
    }
    lastError_.clear();
}

void StudioWindow::keep() {
    const Shown* s = inspector_->shown();
    if (!s || !inspector_->made() || s->key.empty()) return;
    json::Value args = json::Value::object();
    args.set("file", s->file);
    args.set("key", s->key);
    jobs_->add("keep", args, true,
               app::utf8(app::wtrf(lang_, settings::Text::StudioJobKeep,
                                   {utf8OfGame(s->file.substr(s->file.rfind('\\') + 1))})));
}

void StudioWindow::discard() {
    const Shown* s = inspector_->shown();
    if (!s || s->key.empty()) return;
    const std::wstring made = madeFile(s->key);
    DeleteFileW(made.c_str());
    DeleteFileW((made.substr(0, made.size() - 4) + L".txt").c_str());
    inspector_->setMade(nullptr);
}

void StudioWindow::takeOut() {
    const auto cat = catalog_->catalog();
    std::vector<Row> rows = textures_->selection();
    if (rows.empty() && inspector_->shown()) rows.push_back(inspector_->shown()->row);
    json::Value names = json::Value::array(), keys = json::Value::array();
    for (const Row& r : rows) {
        if (r.file == Row::kNoFile) {
            if (r.picture < textures_->orphans().size()) names.push(textures_->orphans()[r.picture]);
        } else if (cat && r.file < cat->files.size()) {
            const catalog::PictureOf& p = cat->files[r.file].pictures[r.picture];
            names.push(pack::pictureName(pack::Group(p.group), p.key));
            keys.push(p.key);
        }
    }
    if (names.items().empty()) return;
    json::Value args = json::Value::object();
    args.set("names", names);
    args.set("keys", keys);
    jobs_->add("takeout", args, true,
               app::utf8(app::wtrf(lang_, settings::Text::StudioJobTakeOut, {std::to_string(names.items().size())})));
}

void StudioWindow::toggleOwn() {
    const Shown* s = inspector_->shown();
    if (!s || s->key.empty()) return;
    json::Value args = json::Value::object();
    args.set("key", s->key);
    args.set("on", !s->own);
    jobs_->add("own", args, true,
               app::utf8(app::wtrf(lang_, settings::Text::StudioJobOwn,
                                   {utf8OfGame(s->file.substr(s->file.rfind('\\') + 1))})));
}

void StudioWindow::jobDone(int64_t id) {
    json::Value result;
    std::string why;
    std::string label;
    for (const Job& j : jobs_->snapshot())
        if (j.id == id) label = j.op;
    if (!jobs_->resultOf(id, result, why)) {
        lastError_ = app::wtrf(lang_, settings::Text::StudioJobFailed, {label, why});
        if (log_) log_->add(lastError_);
        showBar();
        return;
    }
    lastError_.clear();
    if (log_) log_->add(app::wtrf(lang_, settings::Text::StudioLogDone, {label}));
    if (pack_ && result["op"].asString() != "make") pack_->refresh();
    const std::string op = result["op"].asString();
    if (op != "make") {  // the pack changed: the thumbnails, the flags and the states again
        thumbs_->forget();
        textures_->catalogChanged();
    }
    const Shown* s = inspector_->shown();
    if (s && (op != "make" || result["key"].asString() == s->key)) inspector_->refresh(textures_->orphans());
    showBar();
}

DWORD WINAPI StudioWindow::pickMain(void* self) {
    auto* w = static_cast<StudioWindow*>(self);
    while (!w->closing_ && WaitForSingleObject(w->pickEvent_, INFINITE) == WAIT_OBJECT_0)
        if (!w->closing_) PostMessageW(w->hwnd_, kPicked, 0, 0);
    return 0;
}

void StudioWindow::picked() {
    std::string text;
    Pick p;
    if (!readWhole(linkFolder() + L"\\pick.txt", text) || !parsePick(text, p)) return;
    if (log_) log_->add(L"pick: " + app::wide(p.key) + L" (" + app::wide(p.kind) + L", " + app::wide(p.how) + L")");
    showTab(0);
    TabCtrl_SetCurSel(tabs_, 0);
    textures_->showKey(p.key);
    // in front of the game: Windows lets a window that was not active ask for it, or flash
    if (IsIconic(hwnd_)) ShowWindow(hwnd_, SW_RESTORE);
    if (!SetForegroundWindow(hwnd_)) {
        FLASHWINFO f{sizeof f, hwnd_, FLASHW_ALL | FLASHW_TIMERNOFG, 0, 0};
        FlashWindowEx(&f);
    }
}

void StudioWindow::togglePause() {
    if (jobs_->paused()) {  // G4: the newest engine at the next job; the catalog up to date with the archives
        jobs_->resume();
        catalog_->update(hwnd_);
    } else {  // G4: everything let go - the engine, the archives, the pack
        jobs_->pause();
        catalog_->stop();
    }
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
            if (textures_) textures_->command(LOWORD(wp), HIWORD(wp));
            if (inspector_) inspector_->command(LOWORD(wp), HIWORD(wp));
            if (pack_) pack_->command(LOWORD(wp), HIWORD(wp));
            return 0;
        case WM_HSCROLL:
            if (inspector_ && lp) inspector_->scrolled(reinterpret_cast<HWND>(lp));
            return 0;
        case WM_NOTIFY: {
            const auto* n = reinterpret_cast<const NMHDR*>(lp);
            if (n->hwndFrom == tabs_ && n->code == TCN_SELCHANGE) {
                showTab(TabCtrl_GetCurSel(tabs_));
                return 0;
            }
            LRESULT result = 0;
            if (textures_ && textures_->notify(n, result)) return result;
            break;
        }
        case WM_TIMER:
            if (wp == kBarTimer) {
                showBar();
                if (catalog_) catalog_->closeIdle();
            }
            if (textures_) textures_->timer(wp);
            return 0;
        case kCatalogReady:
            showBar();
            if (textures_) textures_->catalogChanged();
            return 0;
        case kThumbReady:
            if (textures_) textures_->thumbsReady();
            return 0;
        case kShownReady:
            if (inspector_) inspector_->shownReady(uint64_t(wp), reinterpret_cast<Shown*>(lp));
            else delete reinterpret_cast<Shown*>(lp);
            return 0;
        case kRowsReady:
            if (textures_) textures_->rowsReady(uint64_t(wp), reinterpret_cast<RowsResult*>(lp));
            else delete reinterpret_cast<RowsResult*>(lp);
            return 0;
        case kJobsChanged:
        case kEngineGone:
            showBar();
            return 0;
        case kJobDone:
            jobDone(int64_t(wp));
            return 0;
        case kPicked:
            picked();
            return 0;
        case kEngineLog: {
            std::unique_ptr<std::string> line(reinterpret_cast<std::string*>(lp));
            if (log_) log_->add(L"engine: " + app::wide(*line));
            return 0;
        }
        case kPackReady:
            if (pack_) pack_->ready(reinterpret_cast<std::wstring*>(lp));
            else delete reinterpret_cast<std::wstring*>(lp);
            return 0;
        case WM_CLOSE:
            saveSession();
            DestroyWindow(hwnd_);
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd_, kBarTimer);
            closing_ = true;
            if (pickThread_) {
                SetEvent(pickEvent_);
                WaitForSingleObject(pickThread_, 2000);
                CloseHandle(pickThread_);
            }
            if (pickEvent_) CloseHandle(pickEvent_);
            jobs_.reset();  // the engine ends, the queue is written
            inspector_.reset();
            textures_.reset();
            pack_.reset();
            log_.reset();
            thumbs_.reset();
            catalog_.reset();
            if (font_) DeleteObject(font_);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd_, msg, wp, lp);
}

}  // namespace uagfx::studio

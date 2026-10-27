#include "app/reports_window.hpp"

#include <commctrl.h>
#include <commdlg.h>
#include <objbase.h>

#include <algorithm>
#include <cstdio>
#include <memory>
#include <tuple>

#include "app/chat_view.hpp"
#include "app/files.hpp"
#include "app/report_send.hpp"
#include "app/reports.hpp"
#include "app/shots.hpp"
#include "app/text.hpp"
#include "app/ui.hpp"
#include "core/report_talk.hpp"
#include "core/report_texts.hpp"
#include "core/version.hpp"

namespace uagfx::app {
namespace {

using settings::Lang;
using settings::Text;

const wchar_t kClass[] = L"uaroGraphicsReports";
constexpr UINT kLoad = WM_APP + 1;  // the site asked once the window is on the screen
constexpr size_t kMostPicked = 6;
constexpr size_t kGameShots = 12;

// 3.0.9-beta.5: the conversation as a chat (app/chat_view: kHead, kChat), the list as cards with two tabs
enum Id {
    kNew = 100, kRefresh, kList, kHint, kHead, kNote, kConfirm, kStill, kChat, kReplyLabel, kReply, kAddShot,
    kPasteReply, kAttached, kSend, kNewHeading, kWhatLabel, kWhat, kKindLabel, kKind0, kKind1, kKind2, kKind3,
    kPickLabel, kPick, kAddFile, kPaste, kGoes, kCancel, kSendNew, kStatus, kClose, kTabOpen, kTabClosed,
};

// A status's chip: its ground and its words (the support canvas' colours)
struct Tone {
    COLORREF ground, words;
};
Tone toneOf(reports::Status s) {
    switch (s) {
        case reports::Status::Read: return {RGB(227, 236, 251), RGB(29, 79, 168)};
        case reports::Status::NeedInfo: return {RGB(253, 235, 214), RGB(138, 75, 0)};
        case reports::Status::Fixing: return {RGB(236, 230, 251), RGB(91, 63, 168)};
        case reports::Status::Fixed: return {RGB(223, 243, 230), RGB(29, 107, 58)};
        case reports::Status::Closed: return {RGB(236, 239, 244), RGB(93, 104, 128)};
        case reports::Status::New: break;
    }
    return {RGB(236, 239, 244), RGB(74, 85, 104)};
}

// The chat's screenshots fetched and made small on a thread of their own, each posted to the chat as it comes
struct ThumbJob {
    HWND view = nullptr;
    unsigned generation = 0;
    std::string key, code;
    SIZE size{};
    std::vector<std::tuple<size_t, size_t, std::string>> shots;  // the chat's item, the shot in it, its name
};

DWORD WINAPI fetchThumbs(void* p) {
    std::unique_ptr<ThumbJob> job(static_cast<ThumbJob*>(p));
    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);  // WIC
    for (const auto& s : job->shots) {
        if (!IsWindow(job->view)) break;
        std::string bytes, error;
        if (!fetchShot(job->key, job->code, std::get<2>(s), bytes, error)) continue;
        if (HBITMAP bmp = jpegThumb(bytes, int(job->size.cx), int(job->size.cy)))
            postChatThumb(job->view, job->generation, std::get<0>(s), std::get<1>(s), bmp);
    }
    if (SUCCEEDED(com)) CoUninitialize();
    return 0;
}

const reports::Kind kKinds[] = {reports::Kind::Graphics, reports::Kind::Crash, reports::Kind::Slow,
                                reports::Kind::Other};

std::wstring fileNameOf(const std::wstring& path) {
    const size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? path : path.substr(slash + 1);
}

std::wstring textOf(HWND ctl) {
    std::wstring s(size_t(GetWindowTextLengthW(ctl)) + 1, L'\0');
    s.resize(size_t(GetWindowTextW(ctl, &s[0], int(s.size()))));
    return s;
}

// The site's UTC time in this computer's clock: "22.10.2026 21:57"
std::string localTime(const std::string& iso) {
    reports::Civil c;
    if (!reports::utcOf(iso, c)) return iso;
    SYSTEMTIME utc{}, local{};
    utc.wYear = WORD(c.year);
    utc.wMonth = WORD(c.month);
    utc.wDay = WORD(c.day);
    utc.wHour = WORD(c.hour);
    utc.wMinute = WORD(c.minute);
    if (!SystemTimeToTzSpecificLocalTime(nullptr, &utc, &local)) return reports::shown(c);
    reports::Civil l;
    l.year = local.wYear;
    l.month = local.wMonth;
    l.day = local.wDay;
    l.hour = local.wHour;
    l.minute = local.wMinute;
    return reports::shown(l);
}

// One screenshot of the new report's list: a game's file or a pasted picture
struct Pick {
    std::wstring path;  // "" for a pasted one
    std::string jpeg;   // a pasted one's bytes
    std::wstring name;
    std::string png;  // 3.0.5-beta.5: a pasted one in full (clipboardPng), sent after the report
};

class ReportsWindow {
public:
    ReportsWindow(const Model& m, Lang l, int dpi, const ReportStart& start)
        : model_(m), lang_(l), dpi_(dpi), start_(start) {}
    ~ReportsWindow() {
        if (thumbs_) ImageList_Destroy(thumbs_);
        if (font_) DeleteObject(font_);
        if (bold_) DeleteObject(bold_);
        if (strong_) DeleteObject(strong_);
        if (barBrush_) DeleteObject(barBrush_);
        if (cardsBrush_) DeleteObject(cardsBrush_);
    }

    HWND create(HWND owner);
    void attach(HWND h) { hwnd_ = h; }  // at WM_NCCREATE: the messages of the creation have the window
    LRESULT proc(UINT msg, WPARAM wp, LPARAM lp);
    // Ctrl+V with a picture in the clipboard, 3.0.9-beta.5: Ctrl+Enter in the answer (it goes) - taken here
    bool keys(const MSG& msg);
    int unread() const { return loaded_ ? reports::unreadOf(list_) : -1; }
    std::vector<reports::Answer> answers() const { return reports::answersOf(list_); }  // 3.0.9-beta.4

private:
    enum class Pane { Hint, Talk, New };

    std::wstring t(Text x) const { return wtr(lang_, x); }
    std::wstring tf(Text x, const std::vector<std::string>& args) const { return wtrf(lang_, x, args); }
    int at(int v) const { return px(v, dpi_); }
    HWND make(const wchar_t* cls, const std::wstring& text, DWORD style, int id, DWORD ex = 0);
    void status(const std::wstring& text) { SetWindowTextW(GetDlgItem(hwnd_, kStatus), text.c_str()); }
    // shown by show()/fillTalk() - its own style, whether the window is on the screen yet or not
    bool shown(int id) const { return (GetWindowLongPtrW(GetDlgItem(hwnd_, id), GWL_STYLE) & WS_VISIBLE) != 0; }

    void layout();
    void show(Pane p);
    void load();
    void fillList();
    void openSelected();
    void openCode(const std::string& code);
    void fillTalk();
    void sendReply();
    void verdict(bool fixed);
    void openShot(size_t item, size_t shot);  // 3.0.9-beta.5: a thumbnail of the chat clicked
    void drawCard(const DRAWITEMSTRUCT* d);
    std::wstring shortTime(const std::string& iso) const;
    void startNew();
    void addPick(const Pick& p, bool ticked);
    void addFile(bool toReply);
    void paste(bool toReply);
    void attach(const std::string& jpeg);  // to the reply
    void sendNewReport();
    bool picksTicked(std::vector<size_t>& ticked) const;
    bool newTouched() const;

    std::wstring statusText(reports::Status s, const std::string& fixedIn) const;
    std::wstring kindText(reports::Kind k) const;
    std::wstring titleOf(const reports::Summary& s) const;
    std::wstring titleOfReason(const std::string& reason) const;

    const Model& model_;
    Lang lang_;
    int dpi_;
    ReportStart start_;
    HWND hwnd_ = nullptr;
    HFONT font_ = nullptr, bold_ = nullptr, strong_ = nullptr;  // strong_: bold at the text's size (the cards)
    HIMAGELIST thumbs_ = nullptr;
    Pane pane_ = Pane::Hint;
    std::string key_;
    bool loaded_ = false, filling_ = false;
    std::vector<reports::Summary> list_;
    reports::Conversation talk_;
    std::vector<std::vector<std::string>> chatShots_;  // the chat's items' shots (names of the conversation's)
    std::vector<size_t> shownItems_;  // the list's rows: list_'s reports of the tab
    bool closedTab_ = false;
    unsigned generation_ = 0;  // the chat's filling, for the thumbnails that come late
    HBRUSH barBrush_ = nullptr, cardsBrush_ = nullptr;
    COLORREF barGround_ = RGB(255, 244, 232), barWords_ = RGB(70, 40, 0);
    std::vector<std::pair<std::string, std::string>> attached_;  // the reply's: name, JPEG
    std::vector<Pick> picks_;
    int pasted_ = 0;
};

HWND ReportsWindow::make(const wchar_t* cls, const std::wstring& text, DWORD style, int id, DWORD ex) {
    HWND c = CreateWindowExW(ex, cls, text.c_str(), WS_CHILD | style, 0, 0, 10, 10, hwnd_,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
    SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(id == kNewHeading ? bold_ : font_), TRUE);
    return c;
}

HWND ReportsWindow::create(HWND owner) {
    font_ = makeFont(dpi_, false);
    bold_ = makeFont(dpi_, true, 115);
    strong_ = makeFont(dpi_, true);
    cardsBrush_ = CreateSolidBrush(RGB(241, 244, 249));
    key_ = computerKey(model_.paths().dir);
    RECT o;
    GetWindowRect(owner, &o);
    const int w = at(1040), h = at(700);
    hwnd_ = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT, kClass, t(Text::ReportsTitle).c_str(),
                            WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MAXIMIZEBOX,
                            o.left + (o.right - o.left - w) / 2, o.top + (o.bottom - o.top - h) / 2, w, h, owner,
                            nullptr, GetModuleHandleW(nullptr), this);
    if (!hwnd_) return nullptr;
    const DWORD tab = WS_TABSTOP;
    make(L"BUTTON", t(Text::ReportsNew), WS_VISIBLE | tab | BS_PUSHBUTTON, kNew);
    make(L"BUTTON", t(Text::ReportsRefresh), WS_VISIBLE | tab | BS_PUSHBUTTON, kRefresh);
    // 3.0.9-beta.5: «Відкриті» / «Закриті», the list as cards (drawn here: drawCard)
    make(L"BUTTON", tf(Text::TabOpen, {"0"}), WS_VISIBLE | tab | BS_AUTORADIOBUTTON | BS_PUSHLIKE | WS_GROUP, kTabOpen);
    make(L"BUTTON", tf(Text::TabClosed, {"0"}), WS_VISIBLE | BS_AUTORADIOBUTTON | BS_PUSHLIKE, kTabClosed);
    SendMessageW(GetDlgItem(hwnd_, kTabOpen), BM_SETCHECK, BST_CHECKED, 0);
    HWND list = make(L"LISTBOX", L"",
                     WS_VISIBLE | tab | WS_GROUP | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | LBS_OWNERDRAWFIXED,
                     kList);
    SendMessageW(list, LB_SETITEMHEIGHT, 0, at(86));
    make(L"STATIC", t(Text::ReportsPick), WS_VISIBLE | SS_LEFT | SS_NOPREFIX, kHint);
    // the conversation: its head, the chat, the bar of what is awaited, the answer
    makeTalkHead(hwnd_, kHead, font_, strong_, dpi_);
    makeChatView(hwnd_, kChat, font_, strong_, dpi_);
    make(L"STATIC", L"", SS_LEFT | SS_NOPREFIX | SS_EDITCONTROL, kNote);
    make(L"BUTTON", t(Text::ReportConfirm), tab | BS_PUSHBUTTON, kConfirm);
    make(L"BUTTON", t(Text::ReportStillThere), tab | BS_PUSHBUTTON, kStill);
    make(L"STATIC", t(Text::ReportReplyLabel), SS_LEFT | SS_NOPREFIX, kReplyLabel);
    HWND reply = make(L"EDIT", L"", tab | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN, kReply,
                      WS_EX_CLIENTEDGE);
    SendMessageW(reply, EM_SETLIMITTEXT, 4000, 0);
    make(L"BUTTON", t(Text::ReportAddShot), tab | BS_PUSHBUTTON, kAddShot);
    make(L"BUTTON", t(Text::TalkPaste), tab | BS_PUSHBUTTON, kPasteReply);
    make(L"STATIC", L"", SS_LEFT | SS_NOPREFIX | SS_CENTERIMAGE | SS_ENDELLIPSIS, kAttached);
    markPrimary(make(L"BUTTON", t(Text::TalkSend), tab | BS_PUSHBUTTON, kSend));  // 3.0.2: blue
    // the new report
    make(L"STATIC", t(Text::ReportNewHeading), SS_LEFT | SS_NOPREFIX, kNewHeading);
    make(L"STATIC", t(Text::ReportComment) + L" (" + t(Text::ReportCommentCue) + L")", SS_LEFT | SS_NOPREFIX,
         kWhatLabel);
    HWND what = make(L"EDIT", L"", tab | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN, kWhat,
                     WS_EX_CLIENTEDGE);
    SendMessageW(what, EM_SETLIMITTEXT, 2000, 0);  // 4000 bytes of UTF-8 (report::kMostComment) in any language
    make(L"STATIC", t(Text::ReportKindLabel), SS_LEFT | SS_NOPREFIX, kKindLabel);
    const Text kinds[] = {Text::ReportKindGraphics, Text::ReportKindCrash, Text::ReportKindSlow,
                          Text::ReportKindOther};
    for (int i = 0; i < 4; ++i)
        make(L"BUTTON", t(kinds[i]), tab | BS_AUTORADIOBUTTON | (i ? 0 : WS_GROUP), kKind0 + i);
    make(L"STATIC", t(Text::ReportShotsPick), SS_LEFT | SS_NOPREFIX | WS_GROUP, kPickLabel);
    HWND pick = make(WC_LISTVIEWW, L"", tab | LVS_ICON | LVS_SHOWSELALWAYS | LVS_AUTOARRANGE, kPick,
                     WS_EX_CLIENTEDGE);
    SendMessageW(pick, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_CHECKBOXES | LVS_EX_DOUBLEBUFFER);
    thumbs_ = ImageList_Create(at(160), at(90), ILC_COLOR32, 8, 8);
    SendMessageW(pick, LVM_SETIMAGELIST, LVSIL_NORMAL, reinterpret_cast<LPARAM>(thumbs_));
    make(L"BUTTON", t(Text::ReportAddFile), tab | BS_PUSHBUTTON, kAddFile);
    make(L"BUTTON", t(Text::ReportPaste), tab | BS_PUSHBUTTON, kPaste);
    make(L"STATIC", L"", SS_LEFT | SS_NOPREFIX, kGoes);
    make(L"BUTTON", t(Text::ReportCancel), tab | BS_PUSHBUTTON, kCancel);
    markPrimary(make(L"BUTTON", t(Text::ReportSendButton), tab | BS_PUSHBUTTON, kSendNew));
    // the bottom row
    make(L"STATIC", L"", WS_VISIBLE | SS_LEFT | SS_NOPREFIX | SS_CENTERIMAGE | SS_ENDELLIPSIS, kStatus);
    make(L"BUTTON", t(Text::Close), WS_VISIBLE | tab | BS_PUSHBUTTON, kClose);
    show(Pane::Hint);
    if (start_.fresh) startNew();
    return hwnd_;
}

void ReportsWindow::layout() {
    if (!GetDlgItem(hwnd_, kClose)) return;  // still being made
    RECT c;
    GetClientRect(hwnd_, &c);
    const int w = c.right, h = c.bottom, pad = at(12), bh = at(30), gap = at(8), line = at(20);
    const auto move = [&](int id, int x, int y, int cw, int ch) {
        MoveWindow(GetDlgItem(hwnd_, id), x, y, std::max(0, cw), std::max(0, ch), TRUE);
    };
    const int leftW = std::min(at(340), (w - 3 * pad) * 2 / 5);
    move(kNew, pad, pad, leftW - at(110) - gap, bh);
    move(kRefresh, pad + leftW - at(110), pad, at(110), bh);
    const int tabsY = pad + bh + gap, half = (leftW - gap / 2) / 2;
    move(kTabOpen, pad, tabsY, half, at(28));
    move(kTabClosed, pad + leftW - half, tabsY, half, at(28));
    const int bottom = h - pad - bh, listY = tabsY + at(28) + gap;
    move(kList, pad, listY, leftW, bottom - gap - listY);
    move(kStatus, pad, bottom, w - 3 * pad - at(120), bh);
    move(kClose, w - pad - at(120), bottom, at(120), bh);
    const int x = 2 * pad + leftW, rw = w - x - pad, top = pad, end = bottom - gap;
    move(kHint, x, top + at(4), rw, at(80));
    if (pane_ == Pane::Talk) {
        // 3.0.9-beta.5: the head, the chat, the bar of what is awaited (with «Підтверджую» / «Ще є»), the answer
        const int headH = at(86);
        move(kHead, x, top, rw, headH);
        const bool bar = shown(kNote), confirm = shown(kConfirm);
        const int replyH = line + at(64) + gap + bh;
        const int barH = bar ? at(46) + (confirm ? bh + gap / 2 : 0) + gap : 0;
        const int chatY = top + headH + gap / 2;
        const int chatH = std::max(at(80), end - chatY - barH - replyH - gap);
        move(kChat, x, chatY, rw, chatH);
        int y = chatY + chatH + gap;
        if (bar) {
            move(kNote, x, y, rw, at(46));
            y += at(46);
            if (confirm) {
                y += gap / 2;
                move(kConfirm, x, y, at(150), bh);
                move(kStill, x + at(150) + gap, y, at(150), bh);
                y += bh;
            }
            y += gap;
        }
        move(kReplyLabel, x, y, rw, line);
        move(kReply, x, y + line, rw, at(64));
        y += line + at(64) + gap;
        move(kAddShot, x, y, at(160), bh);
        move(kPasteReply, x + at(160) + gap, y, at(160), bh);
        const int sendW = at(200);
        move(kAttached, x + 2 * (at(160) + gap), y, rw - 2 * (at(160) + gap) - sendW - gap, bh);
        move(kSend, x + rw - sendW, y, sendW, bh);
    } else if (pane_ == Pane::New) {
        int y = top;
        move(kNewHeading, x, y, rw, at(24));
        y += at(24) + gap / 2;
        move(kWhatLabel, x, y, rw, line);
        move(kWhat, x, y + line, rw, at(92));
        y += line + at(92) + gap;
        move(kKindLabel, x, y, rw, line);
        y += line;
        for (int i = 0; i < 4; ++i) move(kKind0 + i, x + (i % 2) * (rw / 2), y + (i / 2) * at(24), rw / 2, at(24));
        y += 2 * at(24) + gap;
        move(kPickLabel, x, y, rw, line);
        y += line;
        const int goesH = at(54), tail = bh + gap + goesH + gap + bh;
        const int pickH = std::max(at(110), end - y - tail);
        move(kPick, x, y, rw, pickH);
        y += pickH + gap;
        move(kAddFile, x, y, at(170), bh);
        move(kPaste, x + at(170) + gap, y, at(190), bh);
        y += bh + gap;
        move(kGoes, x, y, rw, goesH);
        move(kCancel, x + rw - 2 * at(140) - gap, end - bh, at(140), bh);
        move(kSendNew, x + rw - at(140), end - bh, at(140), bh);
    }
}

void ReportsWindow::show(Pane p) {
    pane_ = p;
    const int talk[] = {kHead, kChat, kReplyLabel, kReply, kAddShot, kPasteReply, kAttached, kSend};
    const int fresh[] = {kNewHeading, kWhatLabel, kWhat, kKindLabel, kKind0, kKind1, kKind2, kKind3, kPickLabel, kPick,
                         kAddFile, kPaste, kGoes, kCancel, kSendNew};
    for (int id : talk) ShowWindow(GetDlgItem(hwnd_, id), p == Pane::Talk ? SW_SHOW : SW_HIDE);
    for (int id : fresh) ShowWindow(GetDlgItem(hwnd_, id), p == Pane::New ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(hwnd_, kHint), p == Pane::Hint ? SW_SHOW : SW_HIDE);
    if (p != Pane::Talk)
        for (int id : {kNote, kConfirm, kStill}) ShowWindow(GetDlgItem(hwnd_, id), SW_HIDE);
    layout();
    InvalidateRect(hwnd_, nullptr, TRUE);
}

void ReportsWindow::load() {
    HCURSOR old = SetCursor(LoadCursorW(nullptr, IDC_WAIT));
    status(t(Text::ReportsLoading));
    UpdateWindow(hwnd_);
    const std::wstring dir = model_.paths().dir;
    sendQueued(dir);
    std::vector<reports::Summary> list;
    std::string error;
    if (listReports(key_, list, error)) {
        list_ = list;
        loaded_ = true;
        status(L"");
    } else {
        status(tf(Text::ReportsOffline, {error}));
    }
    const int waiting = queuedReports(dir);
    if (waiting) status(tf(Text::ReportsWaiting, {std::to_string(waiting)}));
    fillList();
    if (pane_ == Pane::Hint)
        SetWindowTextW(GetDlgItem(hwnd_, kHint), t(list_.empty() ? Text::ReportsEmpty : Text::ReportsPick).c_str());
    SetCursor(old);
    if (!start_.code.empty()) {  // 3.0.9-beta.4: the game's answer card - its conversation at once
        const std::string code = start_.code;
        start_.code.clear();
        openCode(code);
    }
}

void ReportsWindow::fillList() {
    // 3.0.9-beta.5: the tab's reports as cards (drawCard); the tabs' numbers
    HWND list = GetDlgItem(hwnd_, kList);
    filling_ = true;
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    shownItems_.clear();
    size_t open = 0, closed = 0;
    for (size_t i = 0; i < list_.size(); ++i) {
        const bool inClosed = talk::closedTab(list_[i].status);
        ++(inClosed ? closed : open);
        if (inClosed != closedTab_) continue;
        const LRESULT row = SendMessageW(list, LB_ADDSTRING, 0, LPARAM(i));
        shownItems_.push_back(i);
        if (pane_ == Pane::Talk && list_[i].code == talk_.code) SendMessageW(list, LB_SETCURSEL, WPARAM(row), 0);
    }
    SetWindowTextW(GetDlgItem(hwnd_, kTabOpen), tf(Text::TabOpen, {std::to_string(open)}).c_str());
    SetWindowTextW(GetDlgItem(hwnd_, kTabClosed), tf(Text::TabClosed, {std::to_string(closed)}).c_str());
    filling_ = false;
}

void ReportsWindow::openSelected() {
    const LRESULT row = SendMessageW(GetDlgItem(hwnd_, kList), LB_GETCURSEL, 0, 0);
    if (row < 0 || size_t(row) >= shownItems_.size()) return;
    const size_t i = shownItems_[size_t(row)];
    if (i >= list_.size()) return;
    if (pane_ == Pane::New && newTouched() && MessageBoxW(hwnd_, t(Text::ReportDiscard).c_str(),
                                                         t(Text::ReportsTitle).c_str(), MB_YESNO) != IDYES)
        return;
    openCode(list_[i].code);
}

void ReportsWindow::openCode(const std::string& code) {
    HCURSOR old = SetCursor(LoadCursorW(nullptr, IDC_WAIT));
    reports::Conversation c;
    std::string error;
    const bool ok = openReport(key_, code, c, error);
    SetCursor(old);
    if (!ok) {
        status(tf(Text::ReportsOffline, {error}));
        return;
    }
    talk_ = c;
    const bool closed = talk::closedTab(c.status);  // 3.0.9-beta.5: its tab, so its card is in the list
    if (closed != closedTab_) {
        closedTab_ = closed;
        SendMessageW(GetDlgItem(hwnd_, kTabOpen), BM_SETCHECK, closed ? BST_UNCHECKED : BST_CHECKED, 0);
        SendMessageW(GetDlgItem(hwnd_, kTabClosed), BM_SETCHECK, closed ? BST_CHECKED : BST_UNCHECKED, 0);
    }
    for (reports::Summary& s : list_)
        if (s.code == code) s.unread = 0;  // the site marked it read
    if (loaded_) noteAnswersOf(model_.paths().dir, list_);  // 3.0.9-beta.4: a running game's card goes
    attached_.clear();
    SetWindowTextW(GetDlgItem(hwnd_, kReply), L"");
    show(Pane::Talk);
    fillTalk();
    fillList();
}

void ReportsWindow::fillTalk() {
    // 3.0.9-beta.5 (the support canvas' screen 1): the head (title, status, facts, path), the chat, the bar
    const reports::Conversation& c = talk_;
    TalkHead head;
    head.title = wide(c.code);
    for (const reports::Summary& s : list_)
        if (s.code == c.code) head.title = titleOf(s);
    head.chip = statusText(c.status, c.fixedIn);
    const Tone tone = toneOf(c.status);
    head.chipGround = tone.ground;
    head.chipWords = tone.words;
    std::wstring what = wide(c.code);
    if (c.kind != reports::Kind::None) what += L" · " + kindText(c.kind);
    head.facts = tf(Text::TalkFacts, {utf8(what), c.version.empty() ? "?" : c.version, localTime(c.at)});
    for (const talk::Step& s : talk::steps(c)) {
        const std::wstring name = s.status == reports::Status::Fixed
                                      ? (c.fixedIn.empty() ? t(Text::StepFixed) : statusText(s.status, c.fixedIn))
                                      : statusText(s.status, c.fixedIn);
        head.steps.push_back(TalkStep{name, s.done, s.current});
    }
    setTalkHead(GetDlgItem(hwnd_, kHead), head);
    // the chat: its items, and the shots fetched on a thread
    std::vector<ChatItem> items;
    chatShots_.clear();
    auto* job = new ThumbJob;
    job->view = GetDlgItem(hwnd_, kChat);
    job->generation = ++generation_;
    job->key = key_;
    job->code = c.code;
    job->size = chatThumbSize(dpi_);
    for (const talk::Item& it : talk::timeline(c)) {
        ChatItem item;
        std::vector<std::string> shots;
        if (it.kind == talk::Item::AutoSent) {
            item.text = tf(Text::ReportAutoSent, {utf8(titleOfReason(c.reason))});
        } else if (it.kind == talk::Item::DevRead) {
            item.text = tf(Text::TalkDevRead, {localTime(c.devSeenAt)});
        } else if (it.kind == talk::Item::Status) {
            const reports::Message& m = c.messages[it.message];
            item.text = tf(Text::ReportEventStatus, {utf8(statusText(m.status, c.fixedIn))});
            item.tone = m.status == reports::Status::NeedInfo ? RGB(138, 75, 0)
                        : m.status == reports::Status::Fixed  ? RGB(29, 107, 58)
                                                              : RGB(93, 104, 128);
        } else {
            const reports::Message& m = c.messages[it.message];
            item.kind = m.fromDev ? ChatItem::Theirs : ChatItem::Mine;
            item.head = (m.fromDev ? t(Text::ReportDev) : t(Text::ReportYou)) + L" · " + wide(localTime(m.at));
            item.text = wide(m.text);
            item.shots = m.shots.size();
            shots = m.shots;
            for (size_t k = 0; k < m.shots.size(); ++k) job->shots.emplace_back(items.size(), k, m.shots[k]);
        }
        items.push_back(item);
        chatShots_.push_back(shots);
    }
    setChat(job->view, items, job->generation);
    HANDLE thread = job->shots.empty() ? nullptr : CreateThread(nullptr, 0, fetchThumbs, job, 0, nullptr);
    if (thread)
        CloseHandle(thread);
    else
        delete job;
    // the bar: what the developer waits for, or the fix to confirm
    const bool confirm = reports::canConfirm(c, kVersion);
    const bool fixed = c.status == reports::Status::Fixed && !c.fixedIn.empty();
    std::wstring bar;
    if (c.status == reports::Status::NeedInfo) {
        std::string question;
        for (const reports::Message& m : c.messages)
            if (m.fromDev && !m.text.empty()) question = m.text;
        for (char& ch : question)
            if (ch == '\n' || ch == '\r') ch = ' ';
        bar = question.empty() ? t(Text::CardWaits) : tf(Text::TalkWaitsBar, {question});
        barGround_ = RGB(255, 244, 232);
        barWords_ = RGB(90, 50, 0);
    } else if (confirm || fixed) {
        bar = confirm ? tf(Text::ReportConfirmNote, {c.fixedIn}) : tf(Text::ReportFixedWait, {c.fixedIn});
        barGround_ = RGB(223, 243, 230);
        barWords_ = RGB(20, 80, 40);
    }
    if (barBrush_) DeleteObject(barBrush_);
    barBrush_ = CreateSolidBrush(barGround_);
    SetWindowTextW(GetDlgItem(hwnd_, kNote), bar.c_str());
    ShowWindow(GetDlgItem(hwnd_, kNote), bar.empty() ? SW_HIDE : SW_SHOW);
    for (int id : {kConfirm, kStill}) ShowWindow(GetDlgItem(hwnd_, id), confirm ? SW_SHOW : SW_HIDE);
    SetWindowTextW(GetDlgItem(hwnd_, kAttached), L"");
    layout();
}

void ReportsWindow::sendReply() {
    const std::wstring text = textOf(GetDlgItem(hwnd_, kReply));
    if (text.empty() && attached_.empty()) {
        status(t(Text::ReportNeedWords));
        return;
    }
    HCURSOR old = SetCursor(LoadCursorW(nullptr, IDC_WAIT));
    std::string error;
    const bool ok = answerReport(key_, talk_.code, utf8(text), attached_, error);
    SetCursor(old);
    if (!ok) {
        status(tf(Text::ReportNotSent, {error}));
        return;
    }
    status(t(Text::ReportSentOk));
    openCode(talk_.code);
}

void ReportsWindow::verdict(bool fixed) {
    HCURSOR old = SetCursor(LoadCursorW(nullptr, IDC_WAIT));
    std::string error;
    const bool ok = fixed ? sendVerdict(key_, talk_.code, true, "", error)
                          : sendStillThere(key_, talk_.code, utf8(textOf(GetDlgItem(hwnd_, kReply))),
                                           utf8(t(Text::ReportStillThere)), attached_, error);
    SetCursor(old);
    if (!ok) {
        status(tf(Text::ReportNotSent, {error}));
        return;
    }
    status(t(Text::ReportSentOk));
    for (reports::Summary& s : list_)
        if (s.code == talk_.code) s.status = fixed ? reports::Status::Closed : reports::Status::New;
    openCode(talk_.code);
}

void ReportsWindow::openShot(size_t item, size_t shot) {
    if (item >= chatShots_.size() || shot >= chatShots_[item].size()) return;
    const std::string& name = chatShots_[item][shot];
    HCURSOR old = SetCursor(LoadCursorW(nullptr, IDC_WAIT));
    std::string bytes, error;
    const bool ok = fetchShot(key_, talk_.code, name, bytes, error);
    SetCursor(old);
    wchar_t temp[MAX_PATH] = {};
    if (!ok || !GetTempPathW(MAX_PATH, temp)) {
        status(tf(Text::ReportsOffline, {error}));
        return;
    }
    const std::wstring path = std::wstring(temp) + L"uaro-" + wide(talk_.code) + L"-" + wide(name);
    if (writeFileAtomic(path, bytes)) openInShell(path);
}

// 3.0.9-beta.5: a report's card in the list - the unread dot, its title, the time; its status in a chip; the last
// message's start
void ReportsWindow::drawCard(const DRAWITEMSTRUCT* d) {
    if (d->itemID == UINT(-1) || size_t(d->itemData) >= list_.size()) return;
    const reports::Summary& s = list_[size_t(d->itemData)];
    HDC dc = d->hDC;
    FillRect(dc, &d->rcItem, cardsBrush_);
    const RECT card{d->rcItem.left + at(6), d->rcItem.top + at(4), d->rcItem.right - at(6), d->rcItem.bottom - at(4)};
    const bool chosen = (d->itemState & ODS_SELECTED) != 0;
    HBRUSH white = CreateSolidBrush(RGB(255, 255, 255));
    HPEN edge = CreatePen(PS_SOLID, chosen ? 2 : 1, chosen ? RGB(159, 187, 232) : RGB(227, 231, 239));
    HGDIOBJ oldBrush = SelectObject(dc, white), oldPen = SelectObject(dc, edge);
    RoundRect(dc, card.left, card.top, card.right, card.bottom, at(12), at(12));
    SetBkMode(dc, TRANSPARENT);
    int x = card.left + at(10);
    const int y1 = card.top + at(8);
    if (s.unread > 0) {  // a new answer: the orange dot
        HBRUSH dot = CreateSolidBrush(RGB(217, 115, 13));
        HPEN none = CreatePen(PS_NULL, 0, 0);
        SelectObject(dc, dot);
        SelectObject(dc, none);
        Ellipse(dc, x, y1 + at(5), x + at(9), y1 + at(14));
        SelectObject(dc, white);
        SelectObject(dc, edge);
        DeleteObject(dot);
        DeleteObject(none);
        x += at(15);
    }
    HGDIOBJ oldFont = SelectObject(dc, font_);
    const std::wstring when = shortTime(s.updated.empty() ? s.at : s.updated);
    SIZE whenSize{};
    GetTextExtentPoint32W(dc, when.c_str(), int(when.size()), &whenSize);
    RECT time{card.right - at(10) - whenSize.cx, y1, card.right - at(10), y1 + at(18)};
    SetTextColor(dc, RGB(93, 104, 128));
    DrawTextW(dc, when.c_str(), int(when.size()), &time, DT_RIGHT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    SelectObject(dc, strong_);
    RECT title{x, y1, time.left - at(8), y1 + at(18)};
    const std::wstring name = titleOf(s);
    SetTextColor(dc, RGB(27, 35, 51));
    DrawTextW(dc, name.c_str(), int(name.size()), &title,
              DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
    // the status's chip, «автоматичне» after it
    const int y2 = y1 + at(23);
    const std::wstring chipText = statusText(s.status, s.fixedIn);
    SIZE chipSize{};
    GetTextExtentPoint32W(dc, chipText.c_str(), int(chipText.size()), &chipSize);
    const Tone tone = toneOf(s.status);
    RECT chip{card.left + at(10), y2, card.left + at(10) + chipSize.cx + at(14), y2 + at(19)};
    HBRUSH chipBrush = CreateSolidBrush(tone.ground);
    HPEN chipPen = CreatePen(PS_SOLID, 1, tone.ground);
    SelectObject(dc, chipBrush);
    SelectObject(dc, chipPen);
    RoundRect(dc, chip.left, chip.top, chip.right, chip.bottom, at(8), at(8));
    SetTextColor(dc, tone.words);
    DrawTextW(dc, chipText.c_str(), int(chipText.size()), &chip, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    SelectObject(dc, font_);
    if (s.automatic) {
        RECT automatic{chip.right + at(8), y2, card.right - at(10), y2 + at(19)};
        SetTextColor(dc, RGB(93, 104, 128));
        const std::wstring word = t(Text::TalkAutomatic);
        DrawTextW(dc, word.c_str(), int(word.size()), &automatic, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    }
    // the last message's start
    RECT last{card.left + at(10), y2 + at(23), card.right - at(10), y2 + at(41)};
    const std::wstring line = wide(reporttexts::listLine(lang_, s));
    SetTextColor(dc, RGB(93, 104, 128));
    DrawTextW(dc, line.c_str(), int(line.size()), &last, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX |
                                                             DT_END_ELLIPSIS);
    SelectObject(dc, oldFont);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(white);
    DeleteObject(edge);
    DeleteObject(chipBrush);
    DeleteObject(chipPen);
}

// The list's time: "00:31" today, else "26.10"
std::wstring ReportsWindow::shortTime(const std::string& iso) const {
    const std::string shown = localTime(iso);  // "22.10.2026 21:57"
    if (shown.size() < 16) return wide(shown);
    SYSTEMTIME now{};
    GetLocalTime(&now);
    char today[16];
    std::snprintf(today, sizeof today, "%02u.%02u.%04u", unsigned(now.wDay), unsigned(now.wMonth),
                  unsigned(now.wYear));
    return wide(shown.compare(0, 10, today) == 0 ? shown.substr(11, 5) : shown.substr(0, 5));
}

void ReportsWindow::startNew() {
    if (pane_ != Pane::New) {
        SetWindowTextW(GetDlgItem(hwnd_, kWhat), L"");
        for (int i = 0; i < 4; ++i) SendMessageW(GetDlgItem(hwnd_, kKind0 + i), BM_SETCHECK, i == 0, 0);
        picks_.clear();
        SendMessageW(GetDlgItem(hwnd_, kPick), LVM_DELETEALLITEMS, 0, 0);
        ImageList_RemoveAll(thumbs_);
        if (!start_.shot.empty()) addPick(Pick{start_.shot, "", fileNameOf(start_.shot), ""}, true);
        for (const std::wstring& path : gameShots(model_.paths().dir, kGameShots))
            if (path != start_.shot) addPick(Pick{path, "", fileNameOf(path), ""}, false);
        std::wstring goes = t(Text::ReportNote);
        if (!start_.map.empty()) goes += L"\r\n" + tf(Text::ReportMapLine, {start_.map});
        SetWindowTextW(GetDlgItem(hwnd_, kGoes), goes.c_str());
    }
    show(Pane::New);
    SetFocus(GetDlgItem(hwnd_, kWhat));
}

void ReportsWindow::addPick(const Pick& p, bool ticked) {
    HBITMAP bmp = p.path.empty() ? jpegThumb(p.jpeg, at(160), at(90)) : shotThumb(p.path, at(160), at(90));
    const int image = bmp ? ImageList_Add(thumbs_, bmp, nullptr) : -1;
    if (bmp) DeleteObject(bmp);
    HWND pick = GetDlgItem(hwnd_, kPick);
    LVITEMW item{};
    item.mask = LVIF_TEXT | LVIF_IMAGE;
    item.iItem = int(picks_.size());
    item.iImage = image;
    item.pszText = const_cast<wchar_t*>(p.name.c_str());
    picks_.push_back(p);
    const LRESULT index = SendMessageW(pick, LVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&item));
    if (ticked) {
        filling_ = true;
        ListView_SetCheckState(pick, int(index), TRUE);
        filling_ = false;
    }
}

void ReportsWindow::addFile(bool toReply) {
    wchar_t file[MAX_PATH] = {};
    std::wstring filter = t(Text::ReportPictures) + L" (*.jpg, *.png, *.bmp)";
    filter.push_back(L'\0');
    filter += L"*.jpg;*.jpeg;*.png;*.bmp";
    filter.push_back(L'\0');
    filter.push_back(L'\0');
    OPENFILENAMEW o{};
    o.lStructSize = sizeof o;
    o.hwndOwner = hwnd_;
    o.lpstrFilter = filter.c_str();
    o.lpstrFile = file;
    o.nMaxFile = MAX_PATH;
    o.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR | OFN_HIDEREADONLY;
    if (!GetOpenFileNameW(&o)) return;
    if (toReply) {
        const std::string jpeg = shotJpeg(file);
        if (jpeg.empty())
            status(tf(Text::ReportShotFailed, {utf8(fileNameOf(file))}));
        else
            attach(jpeg);
        return;
    }
    std::vector<size_t> ticked;
    picksTicked(ticked);
    addPick(Pick{file, "", fileNameOf(file), ""}, ticked.size() < kMostPicked);
}

void ReportsWindow::paste(bool toReply) {
    const std::string jpeg = clipboardJpeg(hwnd_);
    if (jpeg.empty()) {
        status(t(Text::ReportPasteNone));
        return;
    }
    if (toReply) {
        attach(jpeg);
        return;
    }
    std::vector<size_t> ticked;
    picksTicked(ticked);
    addPick(Pick{L"", jpeg, L"Ctrl+V " + std::to_wstring(++pasted_), clipboardPng(hwnd_)},
            ticked.size() < kMostPicked);
}

void ReportsWindow::attach(const std::string& jpeg) {
    if (attached_.size() >= kMostPicked) {
        status(t(Text::ReportTooMany));
        return;
    }
    attached_.emplace_back("shot" + std::to_string(attached_.size() + 1) + ".jpg", jpeg);
    const std::wstring text = tf(Text::ReportAttached, {std::to_string(attached_.size())});
    SetWindowTextW(GetDlgItem(hwnd_, kAttached), text.c_str());
}

bool ReportsWindow::picksTicked(std::vector<size_t>& ticked) const {
    ticked.clear();
    HWND pick = GetDlgItem(hwnd_, kPick);
    for (size_t i = 0; i < picks_.size(); ++i)
        if (ListView_GetCheckState(pick, int(i))) ticked.push_back(i);
    return !ticked.empty();
}

bool ReportsWindow::newTouched() const {
    std::vector<size_t> ticked;
    picksTicked(ticked);
    return GetWindowTextLengthW(GetDlgItem(hwnd_, kWhat)) > 0 || ticked.size() > (start_.shot.empty() ? 0u : 1u);
}

void ReportsWindow::sendNewReport() {
    const std::wstring what = textOf(GetDlgItem(hwnd_, kWhat));
    std::vector<size_t> ticked;
    picksTicked(ticked);
    if (ticked.size() > kMostPicked) {
        status(t(Text::ReportTooMany));
        return;
    }
    if (what.empty() && ticked.empty()) {
        status(t(Text::ReportNeedWords));
        return;
    }
    HCURSOR old = SetCursor(LoadCursorW(nullptr, IDC_WAIT));
    std::vector<std::pair<std::string, std::string>> shots;
    std::vector<std::pair<std::string, std::string>> originals;  // 3.0.5-beta.5: the same in full, sent after it
    for (size_t i : ticked) {
        const Pick& p = picks_[i];
        std::string jpeg = p.path.empty() ? p.jpeg : shotJpeg(p.path);
        if (jpeg.empty()) {
            SetCursor(old);
            status(tf(Text::ReportShotFailed, {utf8(p.name)}));
            return;
        }
        const std::string shot = "shot" + std::to_string(shots.size() + 1);
        shots.emplace_back(shot + ".jpg", std::move(jpeg));
        std::string png = p.path.empty() ? p.png : shotPng(p.path);
        if (!png.empty()) originals.emplace_back(shot + ".png", std::move(png));
    }
    std::string comment = utf8(what);
    if (!start_.map.empty()) comment += (comment.empty() ? "" : "\n\n") + utf8(tf(Text::ReportMapLine, {start_.map}));
    report::Upload u = makeUpload(model_, "manual", false, comment, {}, start_.logSlot);
    for (int i = 0; i < 4; ++i)
        if (SendMessageW(GetDlgItem(hwnd_, kKind0 + i), BM_GETCHECK, 0, 0) == BST_CHECKED)
            u.kind = reports::kindToken(kKinds[i]);
    u.shots = shots;
    const NewReport r = sendNew(model_.paths().dir, u);
    if (!r.code.empty()) {
        start_ = ReportStart{};
        // 3.0.5-beta.5: the shots in full now that the report has its code - the site shows them in the JPEGs' place
        std::string why;
        size_t sentFull = 0;
        if (!originals.empty()) status(t(Text::ReportOriginalsSending));
        for (const auto& o : originals) {
            if (!sendOriginal(key_, r.code, o.first, o.second, why)) break;
            ++sentFull;
        }
        SetCursor(old);
        status(sentFull == originals.size() ? tf(Text::ReportNewSent, {r.code})
                                            : tf(Text::ReportOriginalsFailed, {r.code, why}));
        std::vector<reports::Summary> list;
        std::string error;
        if (listReports(key_, list, error)) {
            list_ = list;
            loaded_ = true;
        }
        openCode(r.code);
    } else if (r.queued) {
        SetCursor(old);
        start_ = ReportStart{};
        status(t(Text::ReportNewQueued));
        show(Pane::Hint);
    } else {
        SetCursor(old);
        status(tf(Text::ReportNewRefused, {r.error}));
    }
}

bool ReportsWindow::keys(const MSG& msg) {
    if (msg.message != WM_KEYDOWN || !(GetKeyState(VK_CONTROL) & 0x8000)) return false;
    // 3.0.9-beta.5: Ctrl+Enter in the answer - it goes
    if (msg.wParam == VK_RETURN && pane_ == Pane::Talk && msg.hwnd == GetDlgItem(hwnd_, kReply)) {
        sendReply();
        return true;
    }
    if (msg.wParam != 'V') return false;
    if (pane_ == Pane::Hint || !IsClipboardFormatAvailable(CF_BITMAP) || IsClipboardFormatAvailable(CF_UNICODETEXT))
        return false;  // text goes into the field as always
    paste(pane_ == Pane::Talk);
    return true;
}

LRESULT ReportsWindow::proc(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_SIZE:
            layout();
            return 0;
        case WM_GETMINMAXINFO: {
            MINMAXINFO* mm = reinterpret_cast<MINMAXINFO*>(lp);
            mm->ptMinTrackSize.x = at(760);
            mm->ptMinTrackSize.y = at(560);
            return 0;
        }
        case kLoad:
            load();
            return 0;
        case WM_NOTIFY: {
            const NMHDR* n = reinterpret_cast<const NMHDR*>(lp);
            LRESULT drawn = 0;
            if (drawPrimary(n, drawn)) return drawn;  // 3.0.2: «Надіслати» blue
            if (n->idFrom == kPick && n->code == LVN_GETEMPTYMARKUP) {  // no game shots: what makes them
                NMLVEMPTYMARKUP* m = reinterpret_cast<NMLVEMPTYMARKUP*>(lp);
                m->dwFlags = EMF_CENTERED;
                const std::wstring text = t(Text::ReportNoGameShots);
                const size_t n = std::min(text.size(), size_t(L_MAX_URL_LENGTH - 1));
                std::copy(text.begin(), text.begin() + std::ptrdiff_t(n), m->szMarkup);
                m->szMarkup[n] = L'\0';
                return TRUE;
            } else if (n->idFrom == kPick && n->code == LVN_ITEMCHANGED && !filling_) {
                const NMLISTVIEW* v = reinterpret_cast<const NMLISTVIEW*>(lp);
                const bool nowTicked = ((v->uNewState & LVIS_STATEIMAGEMASK) >> 12) == 2;
                std::vector<size_t> ticked;
                if (nowTicked && picksTicked(ticked) && ticked.size() > kMostPicked) {
                    filling_ = true;
                    ListView_SetCheckState(n->hwndFrom, v->iItem, FALSE);  // a seventh: not taken
                    filling_ = false;
                    status(t(Text::ReportTooMany));
                }
            }
            return 0;
        }
        case WM_DRAWITEM:  // 3.0.9-beta.5: the list's cards
            if (wp == WPARAM(kList)) {
                drawCard(reinterpret_cast<const DRAWITEMSTRUCT*>(lp));
                return TRUE;
            }
            break;
        case WM_CTLCOLORSTATIC:  // 3.0.9-beta.5: the bar of what is awaited on its own ground
            if (reinterpret_cast<HWND>(lp) == GetDlgItem(hwnd_, kNote) && barBrush_) {
                HDC dc = reinterpret_cast<HDC>(wp);
                SetTextColor(dc, barWords_);
                SetBkColor(dc, barGround_);
                return reinterpret_cast<LRESULT>(barBrush_);
            }
            break;
        case kChatShot:  // a thumbnail of the chat clicked: the shot itself
            openShot(size_t(wp), size_t(lp));
            return 0;
        case WM_COMMAND: {
            const int id = LOWORD(wp), code = HIWORD(wp);
            if (id == kList && code == LBN_SELCHANGE && !filling_) openSelected();
            if (code != BN_CLICKED) return 0;
            switch (id) {
                case kTabOpen:
                case kTabClosed:
                    if (closedTab_ != (id == kTabClosed)) {
                        closedTab_ = id == kTabClosed;
                        fillList();
                    }
                    break;
                case kPasteReply: paste(true); break;
                case kNew: startNew(); break;
                case kRefresh: load(); break;
                case kSend: sendReply(); break;
                case kConfirm: verdict(true); break;
                case kStill: verdict(false); break;
                case kAddShot: addFile(true); break;
                case kAddFile: addFile(false); break;
                case kPaste: paste(false); break;
                case kSendNew: sendNewReport(); break;
                case kCancel:
                    start_ = ReportStart{};
                    show(Pane::Hint);
                    break;
                case kClose:
                case IDCANCEL: SendMessageW(hwnd_, WM_CLOSE, 0, 0); break;
                default: break;
            }
            return 0;
        }
        case WM_CLOSE:
            if (pane_ == Pane::New && newTouched() && MessageBoxW(hwnd_, t(Text::ReportDiscard).c_str(),
                                                                 t(Text::ReportsTitle).c_str(), MB_YESNO) != IDYES)
                return 0;
            DestroyWindow(hwnd_);
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd_, msg, wp, lp);
}

std::wstring ReportsWindow::statusText(reports::Status s, const std::string& fixedIn) const {
    switch (s) {
        case reports::Status::Read: return t(Text::ReportStatusRead);
        case reports::Status::NeedInfo: return t(Text::ReportStatusNeedInfo);
        case reports::Status::Fixing: return t(Text::ReportStatusFixing);
        case reports::Status::Fixed: return tf(Text::ReportStatusFixed, {fixedIn.empty() ? "?" : fixedIn});
        case reports::Status::Closed: return t(Text::ReportStatusClosed);
        case reports::Status::New: break;
    }
    return t(Text::ReportStatusNew);
}

std::wstring ReportsWindow::kindText(reports::Kind k) const {
    switch (k) {
        case reports::Kind::Graphics: return t(Text::ReportKindGraphics);
        case reports::Kind::Crash: return t(Text::ReportKindCrash);
        case reports::Kind::Slow: return t(Text::ReportKindSlow);
        case reports::Kind::Other: return t(Text::ReportKindOther);
        case reports::Kind::Test: return t(Text::ReportKindTest);  // 3.0.5-beta.6
        case reports::Kind::None: break;
    }
    return L"";
}

std::wstring ReportsWindow::titleOfReason(const std::string& reason) const {
    if (reason == "crash") return t(Text::ReportTitleCrash);
    if (reason == "start-failed") return t(Text::ReportTitleStartFailed);
    if (reason == "gpu-lost") return t(Text::ReportTitleGpuLost);
    if (reason == "test") return t(Text::TestTitle);  // 3.0.5-beta.6
    return t(Text::ReportTitleNone);
}

std::wstring ReportsWindow::titleOf(const reports::Summary& s) const {
    return s.title.empty() ? titleOfReason(s.reason) : wide(s.title);
}

LRESULT CALLBACK reportsProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        const CREATESTRUCTW* cs = reinterpret_cast<const CREATESTRUCTW*>(lp);
        SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        static_cast<ReportsWindow*>(cs->lpCreateParams)->attach(h);
    }
    ReportsWindow* w = reinterpret_cast<ReportsWindow*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    if (!w || msg == WM_NCCREATE || msg == WM_CREATE) return DefWindowProcW(h, msg, wp, lp);
    return w->proc(msg, wp, lp);
}

}  // namespace

int showReports(HWND owner, const Model& m, Lang l, int dpi, const ReportStart& start,
                std::vector<reports::Answer>* answers) {
    HINSTANCE inst = GetModuleHandleW(nullptr);
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof wc;
        wc.lpfnWndProc = reportsProc;
        wc.hInstance = inst;
        wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));  // IDC_ARROW
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
        wc.lpszClassName = kClass;
        registered = RegisterClassExW(&wc) != 0;
    }
    ReportsWindow w(m, l, dpi, start);
    HWND win = w.create(owner);
    if (!win) return -1;
    EnableWindow(owner, FALSE);
    ShowWindow(win, SW_SHOW);
    PostMessageW(win, kLoad, 0, 0);
    MSG msg;
    while (IsWindow(win)) {
        const BOOL got = GetMessageW(&msg, nullptr, 0, 0);
        if (got <= 0) {  // the app is ending: its quit for the main loop
            if (got == 0) PostQuitMessage(int(msg.wParam));
            break;
        }
        if (w.keys(msg)) continue;
        if (IsDialogMessageW(win, &msg)) continue;  // Tab, Esc
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
    if (answers && w.unread() >= 0) *answers = w.answers();
    return w.unread();
}

}  // namespace uagfx::app

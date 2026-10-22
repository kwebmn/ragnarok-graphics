#include "app/reports_window.hpp"

#include <commctrl.h>
#include <commdlg.h>

#include <algorithm>

#include "app/files.hpp"
#include "app/report_send.hpp"
#include "app/reports.hpp"
#include "app/shots.hpp"
#include "app/text.hpp"
#include "app/ui.hpp"
#include "core/version.hpp"

namespace uagfx::app {
namespace {

using settings::Lang;
using settings::Text;

const wchar_t kClass[] = L"uaroGraphicsReports";
constexpr UINT kLoad = WM_APP + 1;  // the site asked once the window is on the screen
constexpr size_t kMostPicked = 6;
constexpr size_t kGameShots = 12;

enum Id {
    kNew = 100, kRefresh, kList, kHint, kHeader, kNote, kConfirm, kStill, kTalk, kShotsLabel, kShots, kOpenShot,
    kReplyLabel, kReply, kAddShot, kAttached, kSend, kNewHeading, kWhatLabel, kWhat, kKindLabel, kKind0, kKind1,
    kKind2, kKind3, kPickLabel, kPick, kAddFile, kPaste, kGoes, kCancel, kSendNew, kStatus, kClose,
};

const reports::Kind kKinds[] = {reports::Kind::Graphics, reports::Kind::Crash, reports::Kind::Slow,
                                reports::Kind::Other};

std::wstring crlf(const std::wstring& text) {  // a multi-line edit breaks at "\r\n" only
    std::wstring out;
    for (wchar_t ch : text) {
        if (ch == L'\r') continue;
        out += ch == L'\n' ? std::wstring(L"\r\n") : std::wstring(1, ch);
    }
    return out;
}

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
};

class ReportsWindow {
public:
    ReportsWindow(const Model& m, Lang l, int dpi, const ReportStart& start)
        : model_(m), lang_(l), dpi_(dpi), start_(start) {}
    ~ReportsWindow() {
        if (thumbs_) ImageList_Destroy(thumbs_);
        if (font_) DeleteObject(font_);
        if (bold_) DeleteObject(bold_);
    }

    HWND create(HWND owner);
    void attach(HWND h) { hwnd_ = h; }  // at WM_NCCREATE: the messages of the creation have the window
    LRESULT proc(UINT msg, WPARAM wp, LPARAM lp);
    bool pasteKey(const MSG& msg);  // Ctrl+V with a picture in the clipboard: taken here
    int unread() const { return loaded_ ? reports::unreadOf(list_) : -1; }

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
    void openShot();
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
    HFONT font_ = nullptr, bold_ = nullptr;
    HIMAGELIST thumbs_ = nullptr;
    Pane pane_ = Pane::Hint;
    std::string key_;
    bool loaded_ = false, filling_ = false;
    std::vector<reports::Summary> list_;
    reports::Conversation talk_;
    std::vector<std::pair<std::string, std::string>> shotRefs_;  // the conversation's shots: code, name
    std::vector<std::pair<std::string, std::string>> attached_;  // the reply's: name, JPEG
    std::vector<Pick> picks_;
    int pasted_ = 0;
};

HWND ReportsWindow::make(const wchar_t* cls, const std::wstring& text, DWORD style, int id, DWORD ex) {
    HWND c = CreateWindowExW(ex, cls, text.c_str(), WS_CHILD | style, 0, 0, 10, 10, hwnd_,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
    SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(id == kHeader || id == kNewHeading ? bold_ : font_), TRUE);
    return c;
}

HWND ReportsWindow::create(HWND owner) {
    font_ = makeFont(dpi_, false);
    bold_ = makeFont(dpi_, true, 115);
    key_ = computerKey(model_.paths().dir);
    RECT o;
    GetWindowRect(owner, &o);
    const int w = at(980), h = at(660);
    hwnd_ = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT, kClass, t(Text::ReportsTitle).c_str(),
                            WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MAXIMIZEBOX,
                            o.left + (o.right - o.left - w) / 2, o.top + (o.bottom - o.top - h) / 2, w, h, owner,
                            nullptr, GetModuleHandleW(nullptr), this);
    if (!hwnd_) return nullptr;
    const DWORD tab = WS_TABSTOP;
    make(L"BUTTON", t(Text::ReportsNew), WS_VISIBLE | tab | BS_PUSHBUTTON, kNew);
    make(L"BUTTON", t(Text::ReportsRefresh), WS_VISIBLE | tab | BS_PUSHBUTTON, kRefresh);
    HWND list = make(WC_LISTVIEWW, L"",
                     WS_VISIBLE | tab | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER, kList,
                     WS_EX_CLIENTEDGE);
    SendMessageW(list, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    const std::pair<Text, int> columns[] = {{Text::ReportsColStatus, 150}, {Text::ReportsColTitle, 170},
                                            {Text::ReportsColDate, 112}};
    for (int i = 0; i < 3; ++i) {
        const std::wstring name = t(columns[i].first);
        LVCOLUMNW c{};
        c.mask = LVCF_TEXT | LVCF_WIDTH;
        c.cx = at(columns[i].second);
        c.pszText = const_cast<wchar_t*>(name.c_str());
        SendMessageW(list, LVM_INSERTCOLUMNW, i, reinterpret_cast<LPARAM>(&c));
    }
    make(L"STATIC", t(Text::ReportsPick), WS_VISIBLE | SS_LEFT | SS_NOPREFIX, kHint);
    // the conversation
    make(L"STATIC", L"", SS_LEFT | SS_NOPREFIX | SS_ENDELLIPSIS, kHeader);
    make(L"STATIC", L"", SS_LEFT | SS_NOPREFIX, kNote);
    make(L"BUTTON", t(Text::ReportConfirm), tab | BS_PUSHBUTTON, kConfirm);
    make(L"BUTTON", t(Text::ReportStillThere), tab | BS_PUSHBUTTON, kStill);
    make(L"EDIT", L"", tab | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, kTalk, WS_EX_CLIENTEDGE);
    make(L"STATIC", t(Text::ReportShotsHeading), SS_LEFT | SS_NOPREFIX, kShotsLabel);
    make(L"LISTBOX", L"", tab | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT, kShots, WS_EX_CLIENTEDGE);
    make(L"BUTTON", t(Text::ReportOpenShot), tab | BS_PUSHBUTTON, kOpenShot);
    make(L"STATIC", t(Text::ReportReplyLabel), SS_LEFT | SS_NOPREFIX, kReplyLabel);
    HWND reply = make(L"EDIT", L"", tab | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN, kReply,
                      WS_EX_CLIENTEDGE);
    SendMessageW(reply, EM_SETLIMITTEXT, 4000, 0);
    make(L"BUTTON", t(Text::ReportAddShot), tab | BS_PUSHBUTTON, kAddShot);
    make(L"STATIC", L"", SS_LEFT | SS_NOPREFIX | SS_CENTERIMAGE, kAttached);
    make(L"BUTTON", t(Text::ReportSendButton), tab | BS_PUSHBUTTON, kSend);
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
    make(L"BUTTON", t(Text::ReportSendButton), tab | BS_PUSHBUTTON, kSendNew);
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
    const int leftW = std::min(at(440), (w - 3 * pad) * 2 / 5);
    move(kNew, pad, pad, at(200), bh);
    move(kRefresh, pad + at(200) + gap, pad, at(110), bh);
    const int bottom = h - pad - bh;
    move(kList, pad, pad + bh + gap, leftW, bottom - gap - (pad + bh + gap));
    move(kStatus, pad, bottom, w - 3 * pad - at(120), bh);
    move(kClose, w - pad - at(120), bottom, at(120), bh);
    const int x = 2 * pad + leftW, rw = w - x - pad, top = pad, end = bottom - gap;
    move(kHint, x, top + at(4), rw, at(80));
    if (pane_ == Pane::Talk) {
        int y = top;
        move(kHeader, x, y, rw, at(24));
        y += at(24) + gap / 2;
        if (shown(kNote)) {
            move(kNote, x, y, rw, at(38));
            y += at(38);
            if (shown(kConfirm)) {
                move(kConfirm, x, y, at(150), bh);
                move(kStill, x + at(150) + gap, y, at(150), bh);
                y += bh;
            }
            y += gap;
        }
        const bool shots = shown(kShots);
        const int replyH = line + at(76) + gap + bh, shotsH = shots ? line + at(64) + gap : 0;
        const int talkH = std::max(at(80), end - y - replyH - shotsH - gap);
        move(kTalk, x, y, rw, talkH);
        y += talkH + gap;
        if (shots) {
            move(kShotsLabel, x, y, rw, line);
            move(kShots, x, y + line, rw - at(180) - gap, at(64));
            move(kOpenShot, x + rw - at(180), y + line, at(180), bh);
            y += line + at(64) + gap;
        }
        move(kReplyLabel, x, y, rw, line);
        move(kReply, x, y + line, rw, at(76));
        y += line + at(76) + gap;
        move(kAddShot, x, y, at(170), bh);
        move(kAttached, x + at(170) + gap, y, rw - at(170) - at(140) - 2 * gap, bh);
        move(kSend, x + rw - at(140), y, at(140), bh);
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
    const int talk[] = {kHeader, kTalk, kReplyLabel, kReply, kAddShot, kAttached, kSend};
    const int fresh[] = {kNewHeading, kWhatLabel, kWhat, kKindLabel, kKind0, kKind1, kKind2, kKind3, kPickLabel, kPick,
                         kAddFile, kPaste, kGoes, kCancel, kSendNew};
    for (int id : talk) ShowWindow(GetDlgItem(hwnd_, id), p == Pane::Talk ? SW_SHOW : SW_HIDE);
    for (int id : fresh) ShowWindow(GetDlgItem(hwnd_, id), p == Pane::New ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(hwnd_, kHint), p == Pane::Hint ? SW_SHOW : SW_HIDE);
    if (p != Pane::Talk)
        for (int id : {kNote, kConfirm, kStill, kShotsLabel, kShots, kOpenShot})
            ShowWindow(GetDlgItem(hwnd_, id), SW_HIDE);
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
}

void ReportsWindow::fillList() {
    HWND list = GetDlgItem(hwnd_, kList);
    filling_ = true;
    SendMessageW(list, LVM_DELETEALLITEMS, 0, 0);
    for (size_t i = 0; i < list_.size(); ++i) {
        const reports::Summary& s = list_[i];
        const std::wstring texts[] = {statusText(s.status, s.fixedIn),
                                      (s.unread > 0 ? L"● " : L"") + titleOf(s),
                                      wide(localTime(s.updated.empty() ? s.at : s.updated))};
        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = int(i);
        item.pszText = const_cast<wchar_t*>(texts[0].c_str());
        SendMessageW(list, LVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&item));
        for (int col = 1; col < 3; ++col) {
            item.iSubItem = col;
            item.pszText = const_cast<wchar_t*>(texts[col].c_str());
            SendMessageW(list, LVM_SETITEMTEXTW, i, reinterpret_cast<LPARAM>(&item));
        }
        if (pane_ == Pane::Talk && s.code == talk_.code) {
            item.mask = LVIF_STATE;
            item.iSubItem = 0;
            item.state = item.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
            SendMessageW(list, LVM_SETITEMSTATE, i, reinterpret_cast<LPARAM>(&item));
        }
    }
    filling_ = false;
}

void ReportsWindow::openSelected() {
    const LRESULT i = SendMessageW(GetDlgItem(hwnd_, kList), LVM_GETNEXTITEM, WPARAM(-1), LVNI_SELECTED);
    if (i < 0 || size_t(i) >= list_.size()) return;
    if (pane_ == Pane::New && newTouched() && MessageBoxW(hwnd_, t(Text::ReportDiscard).c_str(),
                                                         t(Text::ReportsTitle).c_str(), MB_YESNO) != IDYES)
        return;
    openCode(list_[size_t(i)].code);
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
    for (reports::Summary& s : list_)
        if (s.code == code) s.unread = 0;  // the site marked it read
    attached_.clear();
    SetWindowTextW(GetDlgItem(hwnd_, kReply), L"");
    show(Pane::Talk);
    fillTalk();
    fillList();
}

void ReportsWindow::fillTalk() {
    const reports::Conversation& c = talk_;
    std::wstring head = wide(c.code);
    if (c.kind != reports::Kind::None) head += L" · " + kindText(c.kind);
    head += L" · " + statusText(c.status, c.fixedIn);
    SetWindowTextW(GetDlgItem(hwnd_, kHeader), head.c_str());
    std::wstring text;
    if (c.automatic) text += tf(Text::ReportAutoSent, {utf8(titleOfReason(c.reason))}) + L"\r\n\r\n";
    shotRefs_.clear();
    HWND shots = GetDlgItem(hwnd_, kShots);
    SendMessageW(shots, LB_RESETCONTENT, 0, 0);
    for (const reports::Message& msg : c.messages) {
        const std::wstring who = msg.fromDev ? t(Text::ReportDev) : t(Text::ReportYou);
        const std::wstring when = wide(localTime(msg.at));
        text += who + L" · " + when + L"\r\n";
        if (msg.hasStatus) text += tf(Text::ReportEventStatus, {utf8(statusText(msg.status, c.fixedIn))}) + L"\r\n";
        if (!msg.text.empty()) text += crlf(wide(msg.text)) + L"\r\n";
        if (!msg.shots.empty()) text += tf(Text::ReportShotsCount, {std::to_string(msg.shots.size())}) + L"\r\n";
        text += L"\r\n";
        for (const std::string& name : msg.shots) {
            const std::wstring row = who + L" · " + when + L" · " + wide(name);
            SendMessageW(shots, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(row.c_str()));
            shotRefs_.emplace_back(c.code, name);
        }
    }
    HWND talk = GetDlgItem(hwnd_, kTalk);
    SetWindowTextW(talk, text.c_str());
    SendMessageW(talk, EM_LINESCROLL, 0, 100000);  // the newest at the bottom, in view
    const bool hasShots = !shotRefs_.empty();
    for (int id : {kShotsLabel, kShots, kOpenShot}) ShowWindow(GetDlgItem(hwnd_, id), hasShots ? SW_SHOW : SW_HIDE);
    if (hasShots) SendMessageW(shots, LB_SETCURSEL, 0, 0);
    const bool confirm = reports::canConfirm(c, kVersion);
    const bool fixed = c.status == reports::Status::Fixed && !c.fixedIn.empty();
    SetWindowTextW(GetDlgItem(hwnd_, kNote), confirm ? tf(Text::ReportConfirmNote, {c.fixedIn}).c_str()
                                             : fixed ? tf(Text::ReportFixedWait, {c.fixedIn}).c_str()
                                                     : L"");
    ShowWindow(GetDlgItem(hwnd_, kNote), confirm || fixed ? SW_SHOW : SW_HIDE);
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

void ReportsWindow::openShot() {
    const LRESULT i = SendMessageW(GetDlgItem(hwnd_, kShots), LB_GETCURSEL, 0, 0);
    if (i < 0 || size_t(i) >= shotRefs_.size()) return;
    const auto& ref = shotRefs_[size_t(i)];
    HCURSOR old = SetCursor(LoadCursorW(nullptr, IDC_WAIT));
    std::string bytes, error;
    const bool ok = fetchShot(key_, ref.first, ref.second, bytes, error);
    SetCursor(old);
    wchar_t temp[MAX_PATH] = {};
    if (!ok || !GetTempPathW(MAX_PATH, temp)) {
        status(tf(Text::ReportsOffline, {error}));
        return;
    }
    const std::wstring path = std::wstring(temp) + L"uaro-" + wide(ref.first) + L"-" + wide(ref.second);
    if (writeFileAtomic(path, bytes)) openInShell(path);
}

void ReportsWindow::startNew() {
    if (pane_ != Pane::New) {
        SetWindowTextW(GetDlgItem(hwnd_, kWhat), L"");
        for (int i = 0; i < 4; ++i) SendMessageW(GetDlgItem(hwnd_, kKind0 + i), BM_SETCHECK, i == 0, 0);
        picks_.clear();
        SendMessageW(GetDlgItem(hwnd_, kPick), LVM_DELETEALLITEMS, 0, 0);
        ImageList_RemoveAll(thumbs_);
        if (!start_.shot.empty()) addPick(Pick{start_.shot, "", fileNameOf(start_.shot)}, true);
        for (const std::wstring& path : gameShots(model_.paths().dir, kGameShots))
            if (path != start_.shot) addPick(Pick{path, "", fileNameOf(path)}, false);
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
    addPick(Pick{file, "", fileNameOf(file)}, ticked.size() < kMostPicked);
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
    addPick(Pick{L"", jpeg, L"Ctrl+V " + std::to_wstring(++pasted_)}, ticked.size() < kMostPicked);
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
    for (size_t i : ticked) {
        const Pick& p = picks_[i];
        std::string jpeg = p.path.empty() ? p.jpeg : shotJpeg(p.path);
        if (jpeg.empty()) {
            SetCursor(old);
            status(tf(Text::ReportShotFailed, {utf8(p.name)}));
            return;
        }
        shots.emplace_back("shot" + std::to_string(shots.size() + 1) + ".jpg", std::move(jpeg));
    }
    std::string comment = utf8(what);
    if (!start_.map.empty()) comment += (comment.empty() ? "" : "\n\n") + utf8(tf(Text::ReportMapLine, {start_.map}));
    report::Upload u = makeUpload(model_, "manual", false, comment, {});
    for (int i = 0; i < 4; ++i)
        if (SendMessageW(GetDlgItem(hwnd_, kKind0 + i), BM_GETCHECK, 0, 0) == BST_CHECKED)
            u.kind = reports::kindToken(kKinds[i]);
    u.shots = shots;
    const NewReport r = sendNew(model_.paths().dir, u);
    SetCursor(old);
    if (!r.code.empty()) {
        start_ = ReportStart{};
        status(tf(Text::ReportNewSent, {r.code}));
        std::vector<reports::Summary> list;
        std::string error;
        if (listReports(key_, list, error)) {
            list_ = list;
            loaded_ = true;
        }
        openCode(r.code);
    } else if (r.queued) {
        start_ = ReportStart{};
        status(t(Text::ReportNewQueued));
        show(Pane::Hint);
    } else {
        status(tf(Text::ReportNewRefused, {r.error}));
    }
}

bool ReportsWindow::pasteKey(const MSG& msg) {
    if (msg.message != WM_KEYDOWN || msg.wParam != 'V' || !(GetKeyState(VK_CONTROL) & 0x8000)) return false;
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
            if (n->idFrom == kList && n->code == LVN_ITEMCHANGED && !filling_) {
                const NMLISTVIEW* v = reinterpret_cast<const NMLISTVIEW*>(lp);
                if ((v->uNewState & LVIS_SELECTED) && !(v->uOldState & LVIS_SELECTED)) openSelected();
            } else if (n->idFrom == kPick && n->code == LVN_GETEMPTYMARKUP) {  // no game shots: what makes them
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
        case WM_COMMAND: {
            const int id = LOWORD(wp), code = HIWORD(wp);
            if (id == kShots && code == LBN_DBLCLK) openShot();
            if (code != BN_CLICKED) return 0;
            switch (id) {
                case kNew: startNew(); break;
                case kRefresh: load(); break;
                case kSend: sendReply(); break;
                case kConfirm: verdict(true); break;
                case kStill: verdict(false); break;
                case kOpenShot: openShot(); break;
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
        case reports::Kind::None: break;
    }
    return L"";
}

std::wstring ReportsWindow::titleOfReason(const std::string& reason) const {
    if (reason == "crash") return t(Text::ReportTitleCrash);
    if (reason == "start-failed") return t(Text::ReportTitleStartFailed);
    if (reason == "gpu-lost") return t(Text::ReportTitleGpuLost);
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

int showReports(HWND owner, const Model& m, Lang l, int dpi, const ReportStart& start) {
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
        if (w.pasteKey(msg)) continue;
        if (IsDialogMessageW(win, &msg)) continue;  // Tab, Esc
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
    return w.unread();
}

}  // namespace uagfx::app

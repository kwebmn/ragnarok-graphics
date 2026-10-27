#pragma once
#include <windows.h>

#include <string>
#include <vector>

#include "app/model.hpp"
#include "core/reports.hpp"

namespace uagfx::app {

// 2.13: «Звернення» - the tester's reports (app/reports): the list with statuses on the left, the conversation on the
// right (the developer's answers, a reply with screenshots, «Підтверджую» / «Ще є» once the fix is installed) and
// «+ Нове звернення» with its form (what happened, what it is, the game's screenshots to tick, a file, Ctrl+V).
struct ReportStart {
    bool fresh = false;  // the new report's form at once
    std::wstring shot;   // a picture ticked in it (the game's Ctrl+Shift+F10)
    std::string map;     // the game's map then, "" unknown
    int logSlot = 1;     // 3.0.3-beta.1: that game window's logs (core/log_files slotted)
    std::string code;    // 3.0.9-beta.4: --answer (the game's answer card) - that conversation at once
};
// A window over `owner` until it is closed (as the notes' window); the number of answers not read yet after it, -1
// when the site could not be asked. 3.0.9-beta.4: `answers` - those answers then (the main window's strip)
int showReports(HWND owner, const Model& m, settings::Lang l, int dpi, const ReportStart& start,
                std::vector<reports::Answer>* answers = nullptr);

// The header's «Звернення (N)»: at the app's start, on a thread of its own - the waiting reports sent, the list asked.
// `key`: computerKey's, made before the thread (two first-time makers would part with different keys).
void startReportsCheck(const std::wstring& dir, const std::string& key);
int reportsUnread();  // -1 while asking or when the site did not answer
// 3.0.9-beta.4: the answers not read yet, newest first, as the start's check found them (none while asking) - the
// main window's strip «Є відповідь на звернення»; the check also keeps them for the game (answers.json)
std::vector<reports::Answer> reportsAnswers();

}  // namespace uagfx::app

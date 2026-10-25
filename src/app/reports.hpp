#pragma once
#include <string>
#include <utility>
#include <vector>

#include "core/report_upload.hpp"
#include "core/reports.hpp"

namespace uagfx::app {

// 2.13: «Звернення» - the tester's reports on uaro.kiev.ua (core/reports reads the site's answers). No login: the
// reports belong to the computer's random key. A report the site did not take waits in uaro-graphics\reports\ and
// goes at the app's next start. Errors in English.
std::string computerKey(const std::wstring& dir);  // uaro-graphics\reports\computer.txt, made once
bool listReports(const std::string& key, std::vector<reports::Summary>& out, std::string& error);
bool openReport(const std::string& key, const std::string& code, reports::Conversation& out, std::string& error);
// The player's answer: the text and the shots they picked (a name -> a JPEG's bytes)
bool answerReport(const std::string& key, const std::string& code, const std::string& text,
                  const std::vector<std::pair<std::string, std::string>>& shots, std::string& error);
// «Підтверджую» (fixed) or «Ще є» (`text`; the site writes «Ще є» when it is empty)
bool sendVerdict(const std::string& key, const std::string& code, bool fixed, const std::string& text,
                 std::string& error);
// «Ще є» with the player's words and screenshots: with screenshots it goes as their message (`stillThere`, the
// player's language, when no words) - the site makes the report «Нове» again as for the verdict, which takes none
bool sendStillThere(const std::string& key, const std::string& code, const std::string& text,
                    const std::string& stillThere, const std::vector<std::pair<std::string, std::string>>& shots,
                    std::string& error);
// 3.0.5-beta.5 (the user: «сделай так чтобы снимки делались без уменьшения качества»): a shot of a report just sent
// in full - shotN.png beside its shotN.jpg (app/shots shotPng) - in parts of 1 MB in their order; the site shows it in
// the JPEG's place from then on
bool sendOriginal(const std::string& key, const std::string& code, const std::string& name, const std::string& png,
                  std::string& error);
// A shot of a conversation (the player's or the developer's), for the app to open
bool fetchShot(const std::string& key, const std::string& code, const std::string& name, std::string& bytes,
               std::string& error);

struct NewReport {
    std::string code;     // the site's, "" when it did not take it
    bool queued = false;  // kept for later: no answer, the hour's limit, the site's trouble
    std::string error;
};
// A new report (the window's form, the game's Ctrl+Shift+F10, the watcher's after a bad end); the waiting ones go
// with it when the site answers.
NewReport sendNew(const std::wstring& dir, const report::Upload& u);
int sendQueued(const std::wstring& dir);  // how many went
int queuedReports(const std::wstring& dir);

// uaro-graphics\reports.txt (0.51): the codes sent from this computer, newest first; when the last automatic one
// went (seconds since 1970, 0 never) - the watcher sends one in 6 hours at most.
void noteSentIn(const std::wstring& dir, const std::string& code, const std::string& reason, bool automatic);
std::vector<std::string> sentCodesIn(const std::wstring& dir, size_t most);
long long lastAutomaticIn(const std::wstring& dir);

}  // namespace uagfx::app

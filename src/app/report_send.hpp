#pragma once
#include <string>
#include <vector>

#include "app/model.hpp"
#include "core/report_upload.hpp"
#include "core/settings/report.hpp"

namespace uagfx::app {

// 0.51: reports to the developers (core/report_upload.hpp). The Diagnostics page's facts, its red and yellow lines
// (problems) among them.
settings::ReportFacts reportFacts(const Model& m, const std::vector<std::string>& problems);
// What goes to uaro.kiev.ua: the report, uaro-graphics.ini and the logs' ends, the Windows user name hidden.
report::Upload makeUpload(const Model& m, const std::string& reason, bool automatic, const std::string& comment,
                          const std::vector<std::string>& problems, bool shots = false);  // 1.8.1: and screenshots
// POST to uaro.kiev.ua: true and the report's code, or false and why (in English).
bool sendUpload(const report::Upload& u, std::string& code, std::string& error);
// uaro-graphics\reports.txt: the codes sent from this computer, newest first.
void noteSent(const Model& m, const std::string& code, const std::string& reason, bool automatic);
std::vector<std::string> sentCodes(const Model& m, size_t most = 5);
// uaro-graphics.exe --watch <pid>, started by the driver with [report] auto = 1: waits for that game to end, and
// when its log says it went badly (report::reasonOfLog), sends a report - at most one in 6 hours. No window.
int watchGame(unsigned long pid);
// 2.2.1: uaro-graphics\usage.jsonl (core/usage.hpp) to uaro.kiev.ua, with the player's leave ([stats] send) - once a
// day (force: now); what was sent leaves the file. false: not sent (error says why, "" no leave).
bool sendUsage(const std::wstring& dir, bool force, std::string& error);

}  // namespace uagfx::app

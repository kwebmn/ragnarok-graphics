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
// 2.13: sent by app/reports (sendNew), the screenshots the player ticked added by the window «Звернення».
// 3.0.3-beta.1: the logs of the game window `logSlot` (core/log_files slotted) under the names the site knows, the
// other windows' game logs of the last 12 hours beside them while the site takes more (report::kMostFiles).
report::Upload makeUpload(const Model& m, const std::string& reason, bool automatic, const std::string& comment,
                          const std::vector<std::string>& problems, int logSlot = 1);
// uaro-graphics.exe --watch <pid> [--log-slot N], started by the driver with [report] auto = 1: waits for that game
// to end, and when its log says it went badly (report::reasonOfLog), sends a report - at most one in 6 hours. No
// window.
int watchGame(unsigned long pid, int logSlot = 1);
// 2.2.1: uaro-graphics\usage.jsonl (core/usage.hpp) to uaro.kiev.ua, with the player's leave ([stats] send) - once a
// day (force: now). 2.2.2: the driver's file is taken whole first (renamed usage-sending.jsonl: a running game starts
// a new one, nothing it writes meanwhile is lost), sent in parts, then deleted - a part not sent stays for the next
// time; one sender at a time (the app and a game's watcher). false: not sent (error says why, "" no leave);
// `lines`: how many went.
bool sendUsage(const std::wstring& dir, bool force, std::string& error, size_t* lines = nullptr);
// 2.2.2: when the statistics last went ("" never) and how many bytes of them wait - the Diagnostics page.
void usageState(const std::wstring& dir, std::string& lastSent, unsigned long long& waiting);

}  // namespace uagfx::app

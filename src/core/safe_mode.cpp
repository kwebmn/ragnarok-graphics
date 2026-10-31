#include "core/safe_mode.hpp"

#include <string>

#include "core/log_files.hpp"
#include "core/report_upload.hpp"

namespace uagfx::safe {

namespace {

// When the game ended badly: the crash's, the card's loss or the renderer's line; a game not closed - its last line
int badEndAt(const std::string& log) {
    for (const char* mark : {"crash: exception 0x", "the GPU device was removed", "the graphics card was lost",
                             "the uaRO renderer could not start"})
        if (const size_t at = log.find(mark); at != std::string::npos) return logfiles::secondsAt(log, at);
    size_t end = log.size();
    while (end > 0 && (log[end - 1] == '\n' || log[end - 1] == '\r')) --end;
    return end == 0 ? -1 : logfiles::secondsAt(log, end - 1);
}

// 3.0.11-beta.106 (R-N6M9DY: the system's DINPUT.dll crashed 1 ms after «DirectDraw released, renderer closed», a
// minute into the game): a crash after the renderer's last opening was closed - the game's own exit, the start fine
bool exitCrash(const std::string& log) {
    const size_t opened = log.rfind("(uaRO renderer)");
    if (opened == std::string::npos) return false;
    const size_t closed = log.find("DirectDraw released, renderer closed", opened);
    const size_t crash = log.find("crash: exception 0x");
    return closed != std::string::npos && crash != std::string::npos && crash > closed;
}

}  // namespace

Start judge(const std::string& previousLog) {
    const bool bad = !report::reasonOfLog(previousLog).empty() && !exitCrash(previousLog);
    const size_t picture = previousLog.find(kFirstPicture);
    if (picture == std::string::npos) return bad ? Start::Failed : Start::Unknown;
    if (!bad) return Start::Fine;
    const int shown = logfiles::secondsAt(previousLog, picture), ended = badEndAt(previousLog);
    if (shown < 0 || ended < 0) return Start::Fine;  // no times: as before
    const int seconds = ended >= shown ? ended - shown : ended + 86400 - shown;
    return seconds <= kEarlySeconds ? Start::FailedEarly : Start::Fine;
}

bool startFailed(const std::string& previousLog) {
    const Start s = judge(previousLog);
    return s == Start::Failed || s == Start::FailedEarly;
}

int failedInRow(int before, Start last) {
    return last == Start::Failed || last == Start::FailedEarly ? before + 1 : last == Start::Fine ? 0 : before;
}

std::string noteText(int inRow, const std::string& version) {
    return "version " + version + "\n" + std::to_string(inRow) +
           " starts in a row ended badly - before their first picture or within 5 minutes of it\n";
}

std::string noteVersion(const std::string& note) {
    if (note.rfind("version ", 0) != 0) return "";
    const size_t end = note.find_first_of("\r\n");
    std::string v = note.substr(8, end == std::string::npos ? std::string::npos : end - 8);
    while (!v.empty() && v.back() == ' ') v.pop_back();
    return v;
}

bool needed(int failedInRow, bool asked) { return asked || failedInRow >= 2; }

void apply(Config& c) {
    if (c.api != Api::System) c.api = Api::D3d11;
    c.bridge = c.remix = c.hdr = c.renderThread = false;
    c.dlss = "off";
    c.frameGeneration = 0;
    c.ai.enabled = c.ai.generate = false;
    c.effects = effectsOff();
}

}  // namespace uagfx::safe

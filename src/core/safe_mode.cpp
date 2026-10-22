#include "core/safe_mode.hpp"

#include "core/report_upload.hpp"

namespace uagfx::safe {

Start judge(const std::string& previousLog) {
    if (previousLog.find(kFirstPicture) != std::string::npos) return Start::Fine;
    return report::reasonOfLog(previousLog).empty() ? Start::Unknown : Start::Failed;
}

bool startFailed(const std::string& previousLog) { return judge(previousLog) == Start::Failed; }

int failedInRow(int before, Start last) {
    return last == Start::Failed ? before + 1 : last == Start::Fine ? 0 : before;
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

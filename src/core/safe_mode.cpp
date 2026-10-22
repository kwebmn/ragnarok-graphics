#include "core/safe_mode.hpp"

#include "core/report_upload.hpp"

namespace uagfx::safe {

bool startFailed(const std::string& previousLog) {
    if (previousLog.empty() || previousLog.find(kFirstPicture) != std::string::npos) return false;
    return !report::reasonOfLog(previousLog).empty();
}

int failedInRow(int before, bool lastFailed) { return lastFailed ? before + 1 : 0; }

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

#pragma once
#include <cstddef>
#include <string>
#include <vector>

namespace uagfx::settings {

// The main facts of uaro-graphics.log for the Diagnostics page and the report (stage 2 spec §7). Its lines are
// "HH:MM:SS.mmm LEVEL message".
struct LogDigest {
    bool found = false;
    std::string version;              // "0.6.0" from "uaRO Graphics 0.6.0 in ..."
    std::string renderer;             // what follows "renderer: "
    std::string fps;                  // "129.4 fps" from the last stats line
    // Crisp text at the last start (spec §7): "on" or "off (<why>)", and the last "k 2.625, 74 windows redrawn".
    std::string crisp;
    std::string crispScale;
    std::vector<std::string> errors;  // the last 10 ERROR messages
    size_t errorCount = 0;
    std::vector<std::string> tail;    // the last lines, whole, for the report
    std::string map;                  // 2.2.5: the last map the game showed ("map: cmd_fild02")
};
LogDigest digestLog(const std::string& text, size_t keep = 40);

}  // namespace uagfx::settings

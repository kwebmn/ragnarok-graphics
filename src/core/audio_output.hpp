#pragma once
#include <string>
#include <vector>

namespace uagfx::audio_output {

// 3.0.4-beta.1 (the user: the game played on the monitor's HDMI output with Bluetooth headphones on; «выбирать на
// каком устройстве выводить звук»; then: the headphones had not connected): [sound] output - the Windows output
// device the game's music and sounds play on (its endpoint id, "{0.0.0.00000000}.{...}"; "" Windows' default, as
// before). The chosen one away (headphones off): Windows' default until it is back.

// Where to open: `id` ("" Windows' default); `away` - the chosen one is not connected now.
struct Target {
    std::string id;
    bool away = false;
};
// `wanted` the setting's, `active` the ids of the output devices connected now
Target pick(const std::string& wanted, const std::vector<std::string>& active);

// Whether to open again: its device lost, or another target than `openedId` ("" Windows' default)
bool reopen(const std::string& openedId, const Target& target, bool lost);

// Endpoint ids alike in any case (Windows writes them in capitals in places)
bool sameId(const std::string& a, const std::string& b);

}  // namespace uagfx::audio_output

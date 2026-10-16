#include <string>

#include "core/settings/pad_files.hpp"
#include "test.hpp"

using namespace uagfx::settings;

TEST(pad_files_tell_the_gamepad_from_other_dinput) {
    const std::string ours = std::string("MZ...uaro-pad.log...uaRO Pad 0.6.0", 34);
    CHECK(classifyPad("", ours) == PadFile::None);
    CHECK(classifyPad(ours, ours) == PadFile::Same);
    CHECK(classifyPad("MZ...uaro-pad.log...uaRO Pad 0.5.1", ours) == PadFile::Pad);
    CHECK(classifyPad("MZ...ROExt...", ours) == PadFile::Other);
    CHECK(classifyPad("MZ...ROExt...", "") == PadFile::Other);
}

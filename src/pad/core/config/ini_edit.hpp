#pragma once
#include <string>
#include <vector>

namespace uapad {

struct IniEdit {
    std::string section;
    std::string key;
    std::string value;
};

// `text` (uaro-pad.ini) with the edits put in, read the way parseConfig reads it. A key already in its
// section gets the new value and keeps the rest of its line, comment included (every copy of it, should it
// appear twice). A missing key goes after the last line of its section, a missing section to the end of the
// file. Every other byte stays as it was; new lines end the way the file's lines do (CRLF for a new file).
std::string applyIniEdits(const std::string& text, const std::vector<IniEdit>& edits);

}  // namespace uapad

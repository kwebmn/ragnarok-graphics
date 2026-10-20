#pragma once
#include <map>
#include <string>
#include <vector>

namespace uagfx::settings {

struct IniEdit {
    std::string section;
    std::string key;
    std::string value;
    bool remove = false;  // 2.6.22: the key's lines taken out (every copy in its section), nothing added
};

// `text` with the edits put in, read the way parseConfig reads it (from uaRO Pad; sections and keys match without
// regard to case, as Windows reads ROExt's ini). A key already in its section gets the new value and keeps the rest
// of its line, comment included - every copy of it, also in a repeated section (install.bat appends sections). A
// missing key goes after the last line of the last copy of its section, a missing section to the end of the file.
// Every other byte stays as it was; new lines end the way the file's lines do (CRLF for a new file). An edit with
// `remove` takes its key's lines out of its section instead.
std::string applyIniEdits(const std::string& text, const std::vector<IniEdit>& edits);

// What parseConfig sees: "section.key" (both lower case) -> the value as written (trimmed, the comment cut off). The
// last copy of a key wins, as in parseConfig.
std::map<std::string, std::string> readIni(const std::string& text);

}  // namespace uagfx::settings

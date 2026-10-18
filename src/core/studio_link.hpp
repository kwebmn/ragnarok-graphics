#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "core/json.hpp"

namespace uagfx::studio {

// 2.3, uaRO Studio: what the studio, its engine (uaro-render64.exe --studio) and the game's driver say to each other.

// The pipe to the engine: one JSON object a line ({"id":..,"op":..,"body":..}). Requests (the studio's, an id each):
// hello, enlarge, material, build, stop, quit. Replies (the engine's, the request's id): progress, result, error.
// Events (id 0): log.
struct Message {
    int64_t id = 0;
    std::string op;
    json::Value body;
};
std::string lineOf(const Message& m);  // ends in '\n'
class LineReader {  // bytes as they come from the pipe, whole lines out
public:
    void feed(const char* p, size_t n);
    bool next(Message& out);  // false: no whole line yet; a line that is no message is skipped (and counted)
    size_t skipped() const { return skipped_; }

private:
    std::string buffer_;
    size_t skipped_ = 0;
};

// The link with a running game: files in the studio's folder (%LOCALAPPDATA%\uaroGraphics\studio\link) and two named
// events - the driver's part is a file and an event (ddraw.dll imports kernel32, user32, gdi32 and msvcrt alone).
constexpr const char* kFolder = "uaroGraphics\\studio";       // under %LOCALAPPDATA%
constexpr const char* kPickEvent = "Local\\uaro-studio-pick";  // the game picked a texture (pick.txt)
constexpr const char* kChangedEvent = "Local\\uaro-studio-changed";  // the studio changed the pack (changed.txt)
struct Pick {  // the texture under the cursor (Ctrl+Shift+F8)
    std::string key, kind, how, map, picture;
    int x = 0, y = 0;  // the cursor in the game's picture
};
std::string pickText(const Pick& p);  // "key=..." lines
bool parsePick(const std::string& text, Pick& out);  // false: no key
std::string changedText(const std::vector<std::string>& keys);  // a key a line
std::vector<std::string> parseChanged(const std::string& text);

// What the game drew on a map (uaro-graphics\seen\<map>.txt): each texture, how it was shown and how often.
enum class How : uint8_t { Pack, Made, Missing, Soft, Alpha, Size, KindOff, Marked, Budget, Hidden };
const char* howName(How h);  // pack, made, missing, soft, alpha, size, kind-off, marked, budget, hidden
bool parseHow(const std::string& word, How& out);
struct Seen {
    std::string key;
    uint8_t kind = 0;  // ai::Kind
    How how = How::Missing;
    uint32_t count = 0;  // frames it was drawn in
};
std::string seenText(const std::string& map, const std::vector<Seen>& seen);  // "map=..." then key, kind, how, count
bool parseSeen(const std::string& text, std::string& map, std::vector<Seen>& out);
// Of the reports there (name, time), the ones past the newest `keep`.
std::vector<std::string> seenToDelete(std::vector<std::pair<std::string, int64_t>> files, size_t keep = 20);

// A picture the engine is given or gives back: "URAW", width, height (four bytes each), then ARGB values.
std::string rawOf(int w, int h, const std::vector<uint32_t>& argb);  // "" when argb is short of w x h
bool parseRaw(const std::string& bytes, int& w, int& h, std::vector<uint32_t>& argb);  // its size checked

}  // namespace uagfx::studio

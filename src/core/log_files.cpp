#include "core/log_files.hpp"

namespace uagfx::logfiles {
namespace {

// Where a name's ".log" starts; npos for a name without one
size_t logDot(const std::string& path) {
    const size_t slash = path.find_last_of('\\');
    const size_t dot = path.find_last_of('.');
    const bool log = dot != std::string::npos && (slash == std::string::npos || dot > slash) &&
                     path.compare(dot, std::string::npos, ".log") == 0;
    return log ? dot : std::string::npos;
}

// The name with a word before its ".log" ("a.log" -> "a.<word>.log"); a name without one gets ".<word>" at the end.
std::string withWord(const std::string& path, const char* word) {
    const size_t dot = logDot(path);
    return dot != std::string::npos ? path.substr(0, dot) + "." + word + ".log" : path + "." + word;
}

}  // namespace

std::string slotted(const std::string& path, int slot) {
    if (slot <= 1) return path;
    const std::string tail = "-" + std::to_string(slot);
    const size_t dot = logDot(path);
    return dot != std::string::npos ? path.substr(0, dot) + tail + ".log" : path + tail;
}

std::string previous(const std::string& path) { return withWord(path, "prev"); }

std::string part(const std::string& path) { return withWord(path, "1"); }

std::string folderOf(const std::string& path) {
    const size_t slash = path.find_last_of('\\');
    return slash == std::string::npos ? std::string() : path.substr(0, slash);
}

}  // namespace uagfx::logfiles

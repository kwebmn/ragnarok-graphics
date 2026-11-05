#include "win/homun_ai_files.hpp"

#include <windows.h>

#include <string>

#include "core/layout.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"

namespace uagfx::win::homunaifiles {
namespace {

struct State {
    bool done = false, wasOn = false;
    homunai::Choices last;
};

bool same(const homunai::Choices& a, const homunai::Choices& b) {
    return a.aggressive == b.aggressive && a.defend == b.defend && a.skills == b.skills && a.follow == b.follow &&
           a.healAt == b.healAt && a.skillLevel == b.skillLevel;
}

bool put(const std::string& path, const std::string& text) {
    std::string have;
    if (readTextFile(path, have)) {
        if (have == text) return true;
        if (!homunai::ours(have)) {
            LOG_INFO("homun ai: %s is another AI - left as it is", path.c_str());
            return false;
        }
    }
    const bool ok = writeFileAtomic(path, text);
    LOG_INFO("homun ai: %s %s", path.c_str(), ok ? "written" : "cannot be written");
    return ok;
}

}  // namespace

void apply(bool on, const homunai::Choices& c) {
    static State s;
    if (s.done && on == s.wasOn && same(c, s.last)) return;
    s.done = true;
    s.wasOn = on;
    s.last = c;
    if (!on) return;
    const std::string ai = layout::join(gameDir(), "AI"), user = layout::join(ai, "USER_AI");
    CreateDirectoryA(ai.c_str(), nullptr);
    CreateDirectoryA(user.c_str(), nullptr);
    const bool mine = put(layout::join(user, "AI.lua"), homunai::aiLua());
    put(layout::join(user, "AI_M.lua"), homunai::aiMLua());
    if (mine) put(layout::join(user, "uaro_config.lua"), homunai::config(c));
}

}  // namespace uagfx::win::homunaifiles

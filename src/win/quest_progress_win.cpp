#include "win/quest_progress_win.hpp"

#include <windows.h>

#include <string>
#include <utility>

#include "core/site_api.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/site_http.hpp"
#include "win/site_key.hpp"

namespace uagfx::win::questprogresswin {
namespace {

constexpr ULONGLONG kAgainMs = 60 * 1000;

struct State {
    std::string name;   // the character now (UTF-8)
    std::string of;     // whose the states are
    std::vector<questprogress::State> states;
    bool asking = false;
    ULONGLONG askedAt = 0;
    int told = 0;
    int generation = 0;
};

State& state() {
    static State s;
    return s;
}

}  // namespace

const std::vector<questprogress::State>* states() {
    State& s = state();
    if (sitekey::token().empty() || s.of.empty() || s.of != s.name) return nullptr;
    return &s.states;
}

int generation() { return state().generation; }

void frame(bool open, unsigned codePage) {
    State& s = state();
    if (!open || sitekey::token().empty() || s.asking) return;
    const std::string own = readOwnName();
    const std::string name = own.empty() ? std::string() : utf8Of(widened(own, codePage));
    if (name.empty()) return;
    if (name != s.name) {  // another character: at once
        s.name = name;
        s.askedAt = 0;
    }
    const ULONGLONG now = GetTickCount64();
    if (s.askedAt && now - s.askedAt < kAgainMs) return;
    s.asking = true;
    s.askedAt = now;
    sitehttp::send(siteapi::questProgress(name), [name](int status, const std::string& body) {
        State& st = state();
        st.asking = false;
        std::vector<questprogress::State> got;
        if (status == 200 && questprogress::parse(body, got)) {
            st.states = std::move(got);
            st.of = name;
            ++st.generation;
            if (st.told++ < 10)
                LOG_INFO("quests: %zu quests' steps of the character from the site (its variables on the server)",
                         st.states.size());
            return;
        }
        if (status == 404 && !st.of.empty()) {  // the site: not this account's character
            st.of.clear();
            ++st.generation;
        }
        if (st.told++ < 10) LOG_INFO("quests: the site's steps not read (HTTP %d)", status);
    });
}

}  // namespace uagfx::win::questprogresswin

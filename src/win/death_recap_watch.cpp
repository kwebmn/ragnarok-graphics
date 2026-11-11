#include "win/death_recap_watch.hpp"

#include <windows.h>

#include <unordered_map>

#include "core/death_recap.hpp"
#include "core/reference_index.hpp"
#include "core/settings/strings.hpp"
#include "core/skill_info.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/real_ddraw.hpp"
#include "win/reference_load.hpp"

namespace uagfx::win::deathrecapwin {
namespace {

constexpr uint32_t kOwnAccount = 0x771F7C, kActorClass = 0x20C;

struct State {
    deathrecap::Log log;
    std::unordered_map<uint32_t, std::string> names;  // told by 0x0095
    int told = 0;
    std::string lastTitle;  // 3.0.18-beta.120: the last death's
    std::vector<std::string> lastLines;
};

State& state() {
    static State s;
    return s;
}

std::string say(settings::Text t) { return settings::tr(language(), t); }

std::string nameOf(State& s, uint32_t id) {
    const uint32_t actor = actorAddress(id);
    uint32_t cls = 0;
    if (actor && readGame(actor + kActorClass, &cls, 4)) {
        if (const auto index = reference::index()) {
            const auto m = index->mobs.find(int(cls));
            if (m != index->mobs.end()) return m->second.name;
        }
    }
    const auto n = s.names.find(id);
    return n == s.names.end() ? std::string() : n->second;
}

}  // namespace

void packet(const packets::Event& e) {
    State& s = state();
    if (const auto* n = std::get_if<packets::ActorName>(&e)) {
        if (s.names.size() > 2000) s.names.clear();
        s.names[n->id] = n->name;
        return;
    }
    const auto* h = std::get_if<packets::Hit>(&e);
    uint32_t own = 0;
    if (!h || !readGame(kOwnAccount, &own, 4) || h->target != own || h->source == own) return;
    s.log.hit(deathrecap::Hit{h->source, nameOf(s, h->source), h->skill, h->damage, GetTickCount64()});
}

std::vector<std::string> lines(std::string& title) {
    State& s = state();
    const uint64_t now = GetTickCount64();
    const std::vector<deathrecap::Hit> hits = s.log.before(now);
    std::vector<std::string> out;
    if (hits.empty()) return out;
    const deathrecap::Texts t{say(settings::Text::RecapAttack), say(settings::Text::RecapAgo)};
    const char decimal = language() == settings::Lang::En ? '.' : ',';
    for (size_t i = 0; i < hits.size() && i < 6; ++i)  // a skill by the server's own name for it
        out.push_back(deathrecap::line(hits[i], now, hits[i].skill ? skillinfo::name(hits[i].skill) : "", t, decimal));
    title = say(settings::Text::RecapTitle);
    const size_t at = title.find("{1}");
    if (at != std::string::npos) title.replace(at, 3, std::to_string(deathrecap::total(hits)));
    if (s.told++ < 20) LOG_INFO("death: %zu hits in its last 10 s (%lld damage) on the death screen", hits.size(),
                                deathrecap::total(hits));
    s.log.clear();
    s.lastTitle = title;  // 3.0.18-beta.120: kept for «Що мене вбило» on its own
    s.lastLines = out;
    return out;
}

bool last(std::string& title, std::vector<std::string>& lines) {
    const State& s = state();
    if (s.lastLines.empty()) return false;
    title = s.lastTitle;
    lines = s.lastLines;
    return true;
}

}  // namespace uagfx::win::deathrecapwin

#include "win/upkeep_watch.hpp"

#include <vector>

#include "core/hotbar_panel.hpp"
#include "core/ui_chime.hpp"
#include "core/upkeep.hpp"
#include "win/client_tables.hpp"
#include "win/code_page.hpp"
#include "win/log.hpp"
#include "win/sound_hook.hpp"

namespace uagfx::win::upkeepwatch {
namespace {

constexpr size_t kReadable = 2048;  // as win/packet_tap reads them

struct State {
    upkeep::Watch watch;
    bool brokenOn = false, hungerOn = false;
    int petLevel = 0, homunLevel = 0, told = 0;
};

State& state() {
    static State s;
    return s;
}

void chime(bool out) {
    static const std::vector<uint8_t> low = chime::wav(chime::Kind::AmmoLow), none = chime::wav(chime::Kind::AmmoOut);
    playOwn(out ? none.data() : low.data());
}

// A fall to hungry (1) or very (2) chimes once; fed again quiet
void hunger(State& s, int fullness, int& was, const char* who) {
    const int level = upkeep::hungerLevel(fullness);
    if (level > was && s.hungerOn) {
        chime(level == 2);
        if (s.told++ < 20) LOG_INFO("upkeep: the %s is %s (%d)", who, level == 2 ? "very hungry" : "hungry", fullness);
    }
    was = level;
}

}  // namespace

void packet(uint16_t id, const uint8_t* p) {
    State& s = state();
    if (!s.watch.packet(id, p, kReadable)) return;
    if (id == 0x00a4) {
        const std::vector<int> fresh = s.watch.takeFresh();
        if (!fresh.empty() && s.brokenOn) chime(true);
        if (s.told++ < 20)
            LOG_INFO("upkeep: %zu broken piece(s) in the equipment list, %zu just broken", s.watch.broken().size(),
                     fresh.size());
        return;
    }
    hunger(s, s.watch.pet(), s.petLevel, "pet");
    hunger(s, s.watch.homun(), s.homunLevel, "homunculus");
}

void setOn(bool brokenGear, bool petHunger) {
    State& s = state();
    if (brokenGear != s.brokenOn) LOG_INFO("upkeep: [interface] broken_gear = %d", brokenGear ? 1 : 0);
    if (petHunger != s.hungerOn) LOG_INFO("upkeep: [interface] pet_hunger = %d", petHunger ? 1 : 0);
    s.brokenOn = brokenGear;
    s.hungerOn = petHunger;
}

std::string broken(unsigned codePage) {
    const State& s = state();
    if (!s.brokenOn || s.watch.broken().empty()) return {};
    if (s.watch.broken().size() > 1) return std::to_string(s.watch.broken().size());
    const std::string id = std::to_string(*s.watch.broken().begin());
    const auto& names = clienttables::itemNames();
    const auto n = names.find(id);
    return n != names.end() ? utf8Of(widened(hotbar::spaced(n->second), codePage)) : "#" + id;
}

int pet() { return state().hungerOn ? state().watch.pet() : -1; }
int homun() { return state().hungerOn ? state().watch.homun() : -1; }

}  // namespace uagfx::win::upkeepwatch

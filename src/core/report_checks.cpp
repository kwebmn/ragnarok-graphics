#include "core/report_checks.hpp"

#include "core/effect_groups.hpp"

namespace uagfx::reportchecks {

const std::vector<Variant>& variants() {
    static const std::vector<Variant> kVariants = {
        {"own-ground", "own ground off", {"effects.own_ground"}},
        {"ground", "the ground's group off", {"effects.group_ground"}},
        {"lights", "the lights, the fire and the skills' light off",
         {"effects.group_lights", "effects.group_fire", "effects.fire_light"}},
        {"screen", "the screen's group off (grade, bloom, the regions' looks)", {"effects.group_screen"}},
        {"pause", "every effect off (as Pause)", {}, true},
    };
    return kVariants;
}

EffectSettings effectsOf(const Variant& v, EffectSettings file, int cardLevel) {
    if (v.pause) return effectsOff();
    for (const std::string& k : v.off)
        for (int g = 0; g < kEffectGroups; ++g)
            if (k == std::string("effects.group_") + effect_groups::groupWord(g)) file.groups[g] = false;
    EffectSettings fx = effect_groups::looksStyled(effect_groups::effective(file, cardLevel));
    for (const std::string& k : v.off)
        if (const effect_groups::Member* m = effect_groups::memberOf(k)) fx.*(m->on) = false;
    return fx;
}

std::string baseOf(const std::string& mainShot) {
    const size_t slash = mainShot.find_last_of("\\/");
    std::string base = slash == std::string::npos ? mainShot : mainShot.substr(slash + 1);
    if (base.size() > 4) {
        std::string tail = base.substr(base.size() - 4);
        for (char& c : tail) c = char(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
        if (tail == ".png") base.resize(base.size() - 4);
    }
    return base;
}

std::string shotPath(const std::string& folder, const std::string& mainShot, const char* name) {
    std::string path = folder;
    if (!path.empty() && path.back() != '\\' && path.back() != '/') path += '\\';
    return path + baseOf(mainShot) + "-" + name + ".png";
}

void Run::start(const std::string& folder, const std::string& mainShot) {
    folder_ = folder;
    main_ = mainShot;
    held_ = -1;  // the report's own frame first: its picture as the player sees it
    index_ = mainShot.empty() || variants().empty() ? -1 : 0;
}

int Run::frame(std::string& shot) {
    shot.clear();
    if (index_ < 0) return -1;
    if (held_ < 0) {
        held_ = 0;
        return -1;
    }
    const int now = index_;
    if (++held_ >= kHoldFrames) {
        shot = shotPath(folder_, main_, variants()[size_t(now)].name);
        held_ = 0;
        if (++index_ >= int(variants().size())) index_ = -1;
    }
    return now;
}

}  // namespace uagfx::reportchecks

#include "core/region_look.hpp"

#include <algorithm>

namespace uagfx::region_look {
namespace {

struct Region {
    const char* names[4];  // a map is of it when its name starts with one of these
    const char* region;
    float tint[3];
    float saturation;
};

// The longer prefixes first where one starts another ("gef" and "gl_").
const Region kRegions[] = {
    {{"niflheim", "nif_"}, "Niflheim", {0.90f, 0.95f, 1.06f}, 0.65f},
    {{"gl_", "glast"}, "Glast Heim", {0.93f, 0.95f, 1.02f}, 0.80f},
    {{"xmas"}, "Lutie", {0.96f, 1.00f, 1.06f}, 1.00f},
    {{"pay", "payon"}, "Payon", {1.04f, 1.02f, 0.94f}, 1.08f},
    {{"moc", "morocc", "in_moc"}, "Morroc", {1.07f, 1.00f, 0.88f}, 0.95f},
    {{"prt", "prontera", "izlude", "iz_"}, "Prontera", {1.02f, 1.00f, 0.98f}, 1.05f},
    {{"gef", "geffen"}, "Geffen", {0.97f, 0.98f, 1.05f}, 1.05f},
    {{"alb", "alberta"}, "Alberta", {0.98f, 1.01f, 1.03f}, 1.08f},
    {{"ein", "einbroch", "einbech"}, "Einbroch", {1.00f, 0.97f, 0.90f}, 0.75f},
    {{"lhz", "lighthalzen"}, "Lighthalzen", {0.97f, 1.00f, 1.04f}, 0.95f},
    {{"hu_", "hugel"}, "Hugel", {1.00f, 1.03f, 0.97f}, 1.10f},
    {{"ra_", "rachel"}, "Rachel", {1.04f, 1.00f, 0.93f}, 1.00f},
    {{"ve_", "veins"}, "Veins", {1.06f, 0.99f, 0.88f}, 0.95f},
    {{"ama", "amatsu"}, "Amatsu", {1.04f, 0.99f, 1.00f}, 1.08f},
    {{"gon", "gonryun"}, "Kunlun", {1.00f, 1.02f, 1.00f}, 1.10f},
    {{"um_", "umbala"}, "Umbala", {0.97f, 1.04f, 0.95f}, 1.10f},
    {{"ayo", "ayothaya"}, "Ayothaya", {1.02f, 1.03f, 0.94f}, 1.08f},
    {{"yuno", "juno"}, "Juno", {1.00f, 1.00f, 1.04f}, 1.05f},
    {{"cmd", "comodo"}, "Comodo", {1.03f, 0.99f, 1.02f}, 1.10f},
    {{"lou", "louyang"}, "Louyang", {1.04f, 1.00f, 0.95f}, 1.06f},
};

bool startsWith(const std::string& s, const char* p) { return s.compare(0, std::char_traits<char>::length(p), p) == 0; }

}  // namespace

Look of(const std::string& map, int strength) {
    Look l;
    const float k = float(std::clamp(strength, 0, 100)) / 100.0f;
    for (const Region& r : kRegions)
        for (const char* name : r.names) {
            if (!name || !startsWith(map, name)) continue;
            for (int c = 0; c < 3; ++c) l.tint[c] = 1.0f + (r.tint[c] - 1.0f) * k;
            l.saturation = 1.0f + (r.saturation - 1.0f) * k;
            l.region = r.region;
            return l;
        }
    return l;
}

}  // namespace uagfx::region_look

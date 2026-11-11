#include "core/supplies.hpp"

#include <algorithm>

#include "core/potion_warn.hpp"

namespace uagfx::supplies {
namespace {

// The gems: 715 Yellow, 716 Red, 717 Blue; 523 Holy Water
struct Need {
    uint16_t skill;
    std::vector<int> items;
};
const std::vector<Need>& table() {
    static const std::vector<Need> t = {
        {12, {717}},           // MG_SAFETYWALL
        {16, {716}},           // MG_STONECURSE
        {27, {717}},           // AL_WARP
        {54, {717}},           // ALL_RESURRECTION
        {68, {523}},           // PR_ASPERSIO
        {70, {717}},           // PR_SANCTUARY
        {79, {717}},           // PR_MAGNUS
        {80, {717}},           // WZ_FIREPILLAR (levels 6-10)
        {140, {716}},          // AS_VENOMDUST
        {141, {716}},          // AS_SPLASHER
        {152, {7049}},         // TF_THROWSTONE
        {220, {716}},          // RG_GRAFFITI
        {228, {7134}},         // AM_PHARMACY
        {229, {7135}},         // AM_DEMONSTRATION
        {230, {7136}},         // AM_ACIDTERROR
        {232, {7137}},         // AM_CANNIBALIZE
        {233, {7138}},         // AM_SPHEREMINE
        {234, {7139}},         // AM_CP_WEAPON
        {235, {7139}},         // AM_CP_SHIELD
        {236, {7139}},         // AM_CP_ARMOR
        {237, {7139}},         // AM_CP_HELM
        {280, {990}},          // SA_FLAMELAUNCHER
        {281, {991}},          // SA_FROSTWEAPON
        {282, {992}},          // SA_LIGHTNINGLOADER
        {283, {993}},          // SA_SEISMICWEAPON
        {285, {715}},          // SA_VOLCANO
        {286, {715}},          // SA_DELUGE
        {287, {715}},          // SA_VIOLENTGALE
        {288, {717, 715}},     // SA_LANDPROTECTOR
        {289, {715}},          // SA_DISPELL
        {290, {715}},          // SA_ABRACADABRA
        {362, {715, 716, 717, 523}},  // HP_BASILICA
        {378, {678}},          // ASC_EDP
        {405, {1025}},         // PF_SPIDERWEB
        {446, {657}},          // AM_BERSERKPITCHER
        {479, {7139}},         // CR_FULLPROTECTION
        {483, {715, 717}},     // HW_GANBANTEIN
        {484, {717}},          // HW_GRAVITATION
        {490, {7135, 7136}},   // CR_ACIDDEMONSTRATION
        {496, {7134}},         // AM_TWILIGHT1
        {497, {7134}},         // AM_TWILIGHT2
        {498, {7134}},         // AM_TWILIGHT3
        {532, {7524}},         // NJ_BUNSINJYUTSU
        {535, {7521}},         // NJ_KAENSIN
        {536, {7521}},         // NJ_BAKUENRYU
        {538, {7522}},         // NJ_SUITON
        {539, {7522}},         // NJ_HYOUSYOURAKU
        {541, {7523}},         // NJ_RAIGEKISAI
        {542, {7523}},         // NJ_KAMAITACHI
        {1008, {12115}},       // SA_ELEMENTWATER
        {1017, {12116}},       // SA_ELEMENTGROUND
        {1018, {12114}},       // SA_ELEMENTFIRE
        {1019, {12117}},       // SA_ELEMENTWIND
    };
    return t;
}

// Spend (skill_require_db's SpiritSphereCost) or make (Summon Spirit Sphere, Absorb, Zen, Flip the Coin) the spheres
constexpr uint16_t kMonk[] = {261, 262, 264, 266, 267, 268, 269, 270, 271, 273, 371, 372, 401, 1015};
constexpr uint16_t kCoins[] = {500, 501, 502, 503, 504, 505, 506, 507, 508};

}  // namespace

std::vector<int> itemsOf(uint16_t skill) {
    for (const Need& n : table())
        if (n.skill == skill) return n.items;
    return {};
}

bool coins(uint16_t skill) { return std::find(std::begin(kCoins), std::end(kCoins), skill) != std::end(kCoins); }

bool usesSpheres(uint16_t skill) {
    return coins(skill) || std::find(std::begin(kMonk), std::end(kMonk), skill) != std::end(kMonk);
}

Pick pick(const std::vector<uint16_t>& barSkills, int spheresNow) {
    Pick p;
    bool any = false, gun = false;
    for (uint16_t skill : barSkills) {
        for (int item : itemsOf(skill))
            if (int(p.items.size()) < kMostItems && std::find(p.items.begin(), p.items.end(), item) == p.items.end())
                p.items.push_back(item);
        if (usesSpheres(skill)) any = true;
        if (coins(skill)) gun = true;
    }
    p.spheres = any || spheresNow > 0;
    p.sphereMost = gun || spheresNow > 5 ? 10 : 5;
    return p;
}

int spheresFor(uint16_t skill, int level) {
    switch (skill) {
        case 264: case 266: case 269: case 273: case 371: case 1015:  // Snap, Occult, Blade Stop, Finish, Tiger, Ki
        case 502: case 503: case 507: case 508: return 1;            // Triple Action, Bull's Eye, Bullet, Cracker
        case 267: return std::clamp(level, 1, 5);                     // Finger Offensive: its level
        case 372: case 505: return 2;                                 // Chain Crush, Adjustment
        case 504: case 506: return 4;                                 // Madness Canceller, Increasing Accuracy
        case 268: case 270: case 271: case 501: return 5;             // Steel Body, Fury, Asura, Fling
        default: return 0;
    }
}

std::vector<int> itemsAt(uint16_t skill, int level) {
    if (skill == 80 && level <= 5) return {};  // WZ_FIREPILLAR: no gems at levels 1-5 (skill.c)
    return itemsOf(skill);
}

}  // namespace uagfx::supplies

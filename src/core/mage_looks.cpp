#include "core/mage_looks.hpp"

namespace uagfx::magelook {
namespace {

struct Entry {
    int id;
    Branch branch;
    Element tone;
};
using B = Branch;
using E = Element;
// The skills' effects (uAthena's db/const.txt EF_*), by their branch and tone
constexpr Entry kLooks[] = {
    // the Mage, Wizard, Sage (and their transcendent classes)
    {24, B::Mage, E::Fire}, {25, B::Mage, E::Fire}, {31, B::Mage, E::Fire}, {49, B::Mage, E::Fire},
    {50, B::Mage, E::Fire}, {62, B::Mage, E::Fire}, {92, B::Mage, E::Fire}, {96, B::Mage, E::Fire},
    {97, B::Mage, E::Fire}, {118, B::Mage, E::Fire}, {138, B::Mage, E::Fire}, {173, B::Mage, E::Fire},
    {176, B::Mage, E::Fire}, {225, B::Mage, E::Fire}, {255, B::Mage, E::Fire},
    {26, B::Mage, E::Water}, {27, B::Mage, E::Water}, {28, B::Mage, E::Water}, {51, B::Mage, E::Water},
    {74, B::Mage, E::Water}, {89, B::Mage, E::Water}, {116, B::Mage, E::Water}, {117, B::Mage, E::Water},
    {135, B::Mage, E::Water}, {174, B::Mage, E::Water}, {236, B::Mage, E::Water}, {256, B::Mage, E::Water},
    {29, B::Mage, E::Wind}, {30, B::Mage, E::Wind}, {52, B::Mage, E::Wind}, {90, B::Mage, E::Wind},
    {93, B::Mage, E::Wind}, {94, B::Mage, E::Wind}, {175, B::Mage, E::Wind}, {237, B::Mage, E::Wind},
    {257, B::Mage, E::Wind}, {432, B::Mage, E::Wind}, {433, B::Mage, E::Wind}, {452, B::Mage, E::Wind},
    {23, B::Mage, E::Earth}, {79, B::Mage, E::Earth}, {95, B::Mage, E::Earth}, {102, B::Mage, E::Earth},
    {142, B::Mage, E::Earth}, {147, B::Mage, E::Earth}, {177, B::Mage, E::Earth}, {238, B::Mage, E::Earth},
    {258, B::Mage, E::Earth},
    {13, B::Mage, E::Spirit}, {15, B::Mage, E::Spirit}, {32, B::Mage, E::Spirit},  // not Sight (22): its orb lights
    {169, B::Mage, E::Spirit}, {182, B::Mage, E::Spirit}, {234, B::Mage, E::Spirit}, {235, B::Mage, E::Spirit},
    {244, B::Mage, E::Spirit}, {315, B::Mage, E::Spirit}, {380, B::Mage, E::Spirit}, {401, B::Mage, E::Spirit},
    {403, B::Mage, E::Spirit}, {406, B::Mage, E::Spirit}, {451, B::Mage, E::Spirit}, {522, B::Mage, E::Spirit},
    // the Swordsman, Knight, Crusader
    {11, B::Sword, E::Steel}, {16, B::Sword, E::Steel}, {17, B::Sword, E::Fire}, {67, B::Sword, E::Fire},
    {70, B::Sword, E::Steel}, {73, B::Sword, E::Steel}, {80, B::Sword, E::Steel}, {81, B::Sword, E::Steel},
    {130, B::Sword, E::Steel}, {131, B::Sword, E::Steel}, {144, B::Sword, E::Steel}, {148, B::Sword, E::Steel},
    {149, B::Sword, E::Steel}, {150, B::Sword, E::Steel}, {151, B::Sword, E::Steel}, {153, B::Sword, E::Steel},
    {213, B::Sword, E::Holy}, {222, B::Sword, E::Holy}, {226, B::Sword, E::Holy}, {245, B::Sword, E::Holy},
    {246, B::Sword, E::Steel}, {248, B::Sword, E::Holy}, {249, B::Sword, E::Steel}, {250, B::Sword, E::Steel},
    {251, B::Sword, E::Holy}, {252, B::Sword, E::Holy}, {336, B::Sword, E::Holy}, {339, B::Sword, E::Fire},
    {366, B::Sword, E::Steel}, {367, B::Sword, E::Steel}, {369, B::Sword, E::Steel}, {370, B::Sword, E::Holy},
    {377, B::Sword, E::Steel}, {392, B::Sword, E::Steel}, {399, B::Sword, E::Steel}, {400, B::Sword, E::Steel},
    {450, B::Sword, E::Holy}, {474, B::Sword, E::Steel}, {494, B::Sword, E::Steel}, {496, B::Sword, E::Holy},
    {520, B::Sword, E::Steel}, {628, B::Sword, E::Steel},
    // the Acolyte, Priest, Monk
    {37, B::Acolyte, E::Holy}, {38, B::Acolyte, E::Dark}, {39, B::Acolyte, E::Water}, {40, B::Acolyte, E::Holy},
    {41, B::Acolyte, E::Holy}, {42, B::Acolyte, E::Holy}, {43, B::Acolyte, E::Holy}, {66, B::Acolyte, E::Life},
    {75, B::Acolyte, E::Holy}, {76, B::Acolyte, E::Holy}, {77, B::Acolyte, E::Holy}, {78, B::Acolyte, E::Life},
    {82, B::Acolyte, E::Holy}, {83, B::Acolyte, E::Holy}, {84, B::Acolyte, E::Holy}, {85, B::Acolyte, E::Holy},
    {86, B::Acolyte, E::Holy}, {87, B::Acolyte, E::Dark}, {88, B::Acolyte, E::Holy}, {91, B::Acolyte, E::Holy},
    {112, B::Acolyte, E::Holy}, {113, B::Acolyte, E::Holy}, {136, B::Acolyte, E::Life}, {141, B::Acolyte, E::Wind},
    {152, B::Acolyte, E::Holy}, {203, B::Acolyte, E::Spirit}, {228, B::Acolyte, E::Spirit},
    {253, B::Acolyte, E::Spirit}, {254, B::Acolyte, E::Steel}, {261, B::Acolyte, E::Spirit},
    {262, B::Acolyte, E::Steel}, {263, B::Acolyte, E::Spirit}, {264, B::Acolyte, E::Steel},
    {265, B::Acolyte, E::Spirit}, {266, B::Acolyte, E::Steel}, {267, B::Acolyte, E::Steel},
    {273, B::Acolyte, E::Steel}, {276, B::Acolyte, E::Steel}, {312, B::Acolyte, E::Life},
    {313, B::Acolyte, E::Life}, {318, B::Acolyte, E::Holy}, {319, B::Acolyte, E::Holy}, {320, B::Acolyte, E::Life},
    {325, B::Acolyte, E::Life}, {328, B::Acolyte, E::Spirit}, {374, B::Acolyte, E::Holy},
    {375, B::Acolyte, E::Holy}, {412, B::Acolyte, E::Spirit}, {418, B::Acolyte, E::Spirit},
    {424, B::Acolyte, E::Spirit}, {440, B::Acolyte, E::Holy}, {467, B::Acolyte, E::Spirit},
    {468, B::Acolyte, E::Spirit}, {469, B::Acolyte, E::Spirit}, {470, B::Acolyte, E::Spirit},
    {471, B::Acolyte, E::Spirit}, {472, B::Acolyte, E::Spirit}, {473, B::Acolyte, E::Spirit},
    {485, B::Acolyte, E::Spirit}, {486, B::Acolyte, E::Spirit}, {504, B::Acolyte, E::Spirit},
    {510, B::Acolyte, E::Spirit}, {629, B::Acolyte, E::Spirit},
    // the Archer, Hunter, Bard, Dancer
    {64, B::Archer, E::Steel}, {69, B::Archer, E::Steel}, {99, B::Archer, E::Holy}, {100, B::Archer, E::Steel},
    {105, B::Archer, E::Fire}, {106, B::Archer, E::Fire}, {107, B::Archer, E::Fire}, {108, B::Archer, E::Water},
    {111, B::Archer, E::Steel}, {114, B::Archer, E::Gold}, {115, B::Archer, E::Wind}, {119, B::Archer, E::Steel},
    {137, B::Archer, E::Gold}, {139, B::Archer, E::Dark}, {277, B::Archer, E::Gold}, {278, B::Archer, E::Gold},
    {279, B::Archer, E::Gold}, {280, B::Archer, E::Gold}, {281, B::Archer, E::Gold}, {282, B::Archer, E::Gold},
    {283, B::Archer, E::Gold}, {284, B::Archer, E::Gold}, {285, B::Archer, E::Gold}, {286, B::Archer, E::Gold},
    {287, B::Archer, E::Gold}, {288, B::Archer, E::Gold}, {289, B::Archer, E::Gold}, {290, B::Archer, E::Gold},
    {291, B::Archer, E::Gold}, {292, B::Archer, E::Gold}, {293, B::Archer, E::Gold}, {294, B::Archer, E::Gold},
    {386, B::Archer, E::Steel}, {387, B::Archer, E::Wind}, {523, B::Archer, E::Gold}, {524, B::Archer, E::Gold},
    {525, B::Archer, E::Gold}, {526, B::Archer, E::Gold}, {527, B::Archer, E::Gold}, {528, B::Archer, E::Gold},
    {529, B::Archer, E::Gold}, {530, B::Archer, E::Gold}, {531, B::Archer, E::Gold}, {532, B::Archer, E::Gold},
    {533, B::Archer, E::Gold}, {534, B::Archer, E::Gold}, {535, B::Archer, E::Gold}, {536, B::Archer, E::Gold},
    // the Thief, Assassin, Rogue
    {18, B::Thief, E::Gold}, {19, B::Thief, E::Dark}, {53, B::Thief, E::Poison}, {65, B::Thief, E::Poison},
    {120, B::Thief, E::Dark}, {121, B::Thief, E::Steel}, {122, B::Thief, E::Steel}, {123, B::Thief, E::Steel},
    {124, B::Thief, E::Poison}, {125, B::Thief, E::Poison}, {126, B::Thief, E::Poison}, {127, B::Thief, E::Poison},
    {129, B::Thief, E::Poison}, {132, B::Thief, E::Steel}, {143, B::Thief, E::Steel}, {171, B::Thief, E::Poison},
    {268, B::Thief, E::Gold}, {269, B::Thief, E::Steel}, {270, B::Thief, E::Steel}, {271, B::Thief, E::Steel},
    {272, B::Thief, E::Steel}, {274, B::Thief, E::Gold}, {275, B::Thief, E::Steel}, {493, B::Thief, E::Poison},
    {495, B::Thief, E::Gold}, {627, B::Thief, E::Gold},
    // the Merchant, Blacksmith, Alchemist
    {10, B::Merchant, E::Gold}, {101, B::Merchant, E::Steel}, {103, B::Merchant, E::Steel},
    {104, B::Merchant, E::Fire}, {128, B::Merchant, E::Fire}, {170, B::Merchant, E::Steel},
    {300, B::Merchant, E::Gold}, {302, B::Merchant, E::Fire}, {303, B::Merchant, E::Gold},
    {390, B::Merchant, E::Fire}, {391, B::Merchant, E::Steel}, {439, B::Merchant, E::Gold},
    {518, B::Merchant, E::Steel}, {537, B::Merchant, E::Poison}, {623, B::Merchant, E::Gold},
    // the Taekwon, Star Gladiator, Soul Linker, Ninja, Gunslinger
    {435, B::Other, E::Steel}, {449, B::Other, E::Steel}, {457, B::Other, E::Steel}, {459, B::Other, E::Steel},
    {460, B::Other, E::Steel}, {461, B::Other, E::Steel}, {462, B::Other, E::Steel}, {463, B::Other, E::Steel},
    {464, B::Other, E::Steel}, {465, B::Other, E::Steel}, {503, B::Other, E::Spirit}, {505, B::Other, E::Spirit},
    {506, B::Other, E::Spirit}, {546, B::Other, E::Steel}, {547, B::Other, E::Steel}, {551, B::Other, E::Steel},
    {552, B::Other, E::Steel}, {553, B::Other, E::Steel}, {555, B::Other, E::Steel}, {556, B::Other, E::Steel},
    {612, B::Other, E::Steel}, {617, B::Other, E::Dark}, {618, B::Other, E::Fire}, {619, B::Other, E::Water},
    {620, B::Other, E::Water}, {621, B::Other, E::Steel}, {622, B::Other, E::Wind}, {624, B::Other, E::Steel},
    // 3.0.16-beta.1: the weapons' hits
    {0, B::Hits, E::Steel}, {1, B::Hits, E::Steel}, {2, B::Hits, E::Steel}, {3, B::Hits, E::Steel},
    {4, B::Hits, E::Steel}, {5, B::Hits, E::Steel}, {180, B::Hits, E::Dark},
    {184, B::Hits, E::Dark}, {186, B::Hits, E::Steel}, {187, B::Hits, E::Steel}, {188, B::Hits, E::Steel},
    {189, B::Hits, E::Steel}, {190, B::Hits, E::Steel}, {191, B::Hits, E::Steel}, {198, B::Hits, E::Spirit},
    {330, B::Hits, E::Steel}, {376, B::Hits, E::Steel}, {417, B::Hits, E::Steel}, {428, B::Hits, E::Steel},
    {429, B::Hits, E::Steel}, {430, B::Hits, E::Steel}, {431, B::Hits, E::Steel}, {434, B::Hits, E::Steel},
    {548, B::Hits, E::Fire}, {549, B::Hits, E::Water}, {554, B::Hits, E::Steel}, {603, B::Hits, E::Fire},
    // the potions, buffs, food and items
    {14, B::Items, E::Water}, {48, B::Items, E::Water}, {204, B::Items, E::Life}, {205, B::Items, E::Life},
    {206, B::Items, E::Life}, {207, B::Items, E::Life}, {208, B::Items, E::Life}, {209, B::Items, E::Life},
    {210, B::Items, E::Life}, {211, B::Items, E::Life}, {218, B::Items, E::Gold}, {219, B::Items, E::Gold},
    {220, B::Items, E::Fire}, {221, B::Items, E::Life}, {298, B::Items, E::Steel}, {299, B::Items, E::Steel},
    {305, B::Items, E::Gold}, {306, B::Items, E::Dark}, {308, B::Items, E::Steel}, {309, B::Items, E::Life},
    {497, B::Items, E::Life}, {498, B::Items, E::Life}, {499, B::Items, E::Life}, {519, B::Items, E::Gold},
    {539, B::Items, E::Steel}, {541, B::Items, E::Steel}, {576, B::Items, E::Wind}, {577, B::Items, E::Spirit},
    {578, B::Items, E::Dark}, {579, B::Items, E::Spirit}, {580, B::Items, E::Water}, {581, B::Items, E::Holy},
    {590, B::Items, E::Holy}, {591, B::Items, E::Life}, {593, B::Items, E::Gold}, {594, B::Items, E::Gold},
    {595, B::Items, E::Gold}, {596, B::Items, E::Gold}, {597, B::Items, E::Gold}, {598, B::Items, E::Gold},
    {600, B::Items, E::Steel}, {608, B::Items, E::Gold}, {609, B::Items, E::Dark}, {613, B::Items, E::Steel},
    {614, B::Items, E::Steel}, {615, B::Items, E::Steel}, {616, B::Items, E::Steel},
    // the states
    {133, B::Status, E::Water}, {134, B::Status, E::Water}, {164, B::Status, E::Spirit}, {192, B::Status, E::Poison},
    {193, B::Status, E::Spirit}, {194, B::Status, E::Gold}, {195, B::Status, E::Earth}, {196, B::Status, E::Dark},
    {197, B::Status, E::Spirit}, {334, B::Status, E::Dark}, {335, B::Status, E::Poison}, {368, B::Status, E::Fire},
    {379, B::Status, E::Water}, {396, B::Status, E::Spirit}, {538, B::Status, E::Poison}, {542, B::Status, E::Water},
    {543, B::Status, E::Dark}, {544, B::Status, E::Fire}, {572, B::Status, E::Dark}, {599, B::Status, E::Steel},
    {625, B::Status, E::Spirit}, {626, B::Status, E::Fire},
    // the travel: teleport, coming and going, levels, the job, refining
    {6, B::Travel, E::Water}, {7, B::Travel, E::Water}, {34, B::Travel, E::Water}, {68, B::Travel, E::Gold},
    {154, B::Travel, E::Gold}, {155, B::Travel, E::Dark}, {156, B::Travel, E::Holy}, {157, B::Travel, E::Holy},
    {158, B::Travel, E::Holy}, {159, B::Travel, E::Gold}, {304, B::Travel, E::Water}, {314, B::Travel, E::Water},
    {337, B::Travel, E::Holy}, {338, B::Travel, E::Holy}, {344, B::Travel, E::Water}, {371, B::Travel, E::Holy},
    {372, B::Travel, E::Dark}, {582, B::Travel, E::Holy},
    // the monsters' and the homunculi's
    {212, B::Monster, E::Dark}, {215, B::Monster, E::Dark}, {216, B::Monster, E::Dark}, {217, B::Monster, E::Spirit},
    {378, B::Monster, E::Spirit}, {383, B::Monster, E::Spirit}, {427, B::Monster, E::Spirit},
    {455, B::Monster, E::Spirit}, {475, B::Monster, E::Dark}, {476, B::Monster, E::Dark}, {477, B::Monster, E::Dark},
    {478, B::Monster, E::Dark}, {479, B::Monster, E::Dark}, {480, B::Monster, E::Dark}, {481, B::Monster, E::Dark},
    {482, B::Monster, E::Dark}, {483, B::Monster, E::Dark}, {484, B::Monster, E::Dark}, {487, B::Monster, E::Dark},
    {564, B::Monster, E::Spirit}, {565, B::Monster, E::Spirit}, {566, B::Monster, E::Spirit},
    {567, B::Monster, E::Spirit}, {568, B::Monster, E::Spirit}, {569, B::Monster, E::Steel},
    {570, B::Monster, E::Spirit}, {571, B::Monster, E::Fire},
};

}  // namespace

Look lookOf(int id) {
    for (const Entry& e : kLooks)
        if (e.id == id) return Look{e.branch, e.tone};
    return Look{};
}

Element elementOf(int id) {
    const Look l = lookOf(id);
    return l.branch == Branch::Mage ? l.tone : Element::None;
}

Palette paletteOf(Element e) {
    switch (e) {
        case Element::Fire: return {{1.0f, 0.93f, 0.72f}, {1.0f, 0.45f, 0.12f}, {0.55f, 0.16f, 0.05f}};
        case Element::Water: return {{1.0f, 1.0f, 1.0f}, {0.62f, 0.80f, 1.0f}, {0.28f, 0.52f, 0.84f}};
        case Element::Wind: return {{1.0f, 1.0f, 0.88f}, {0.78f, 0.62f, 1.0f}, {0.36f, 0.26f, 0.66f}};
        case Element::Earth: return {{1.0f, 0.95f, 0.82f}, {0.78f, 0.60f, 0.36f}, {0.36f, 0.26f, 0.16f}};
        case Element::Spirit: return {{1.0f, 1.0f, 1.0f}, {0.84f, 0.78f, 1.0f}, {0.42f, 0.36f, 0.62f}};
        case Element::Holy: return {{1.0f, 1.0f, 0.94f}, {1.0f, 0.86f, 0.52f}, {0.62f, 0.48f, 0.22f}};
        case Element::Dark: return {{0.86f, 0.78f, 1.0f}, {0.46f, 0.28f, 0.66f}, {0.16f, 0.08f, 0.24f}};
        case Element::Poison: return {{0.92f, 1.0f, 0.82f}, {0.48f, 0.82f, 0.30f}, {0.18f, 0.36f, 0.12f}};
        case Element::Steel: return {{1.0f, 1.0f, 1.0f}, {0.80f, 0.86f, 0.94f}, {0.36f, 0.42f, 0.52f}};
        case Element::Life: return {{1.0f, 1.0f, 0.96f}, {0.62f, 0.94f, 0.66f}, {0.22f, 0.50f, 0.30f}};
        case Element::Gold: return {{1.0f, 0.98f, 0.86f}, {1.0f, 0.80f, 0.36f}, {0.52f, 0.36f, 0.10f}};
        default: return {{1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {0.5f, 0.5f, 0.5f}};
    }
}

Part partOf(bool blend, uint32_t srcBlend, uint32_t dstBlend, uint32_t vertexCount, float width, float screenWidth) {
    if (!blend) return Part::None;
    if (dstBlend == 6 && vertexCount == 3) return Part::Solid;  // INVSRCALPHA: a solid's faces (Storm Gust's spikes)
    if ((srcBlend == 5 || srcBlend == 2) && dstBlend == 2)       // SRCALPHA or ONE, ONE: added on
        return width >= kHazeWidth * screenWidth ? Part::Haze : Part::Layer;
    return Part::None;
}

Part partFor(int effectId, Part p) {
    if (p != Part::Layer && p != Part::Haze) return p;
    if (effectId == kSpheresEffect) return Part::Orb;
    if (effectId == kFireWallEffect) return Part::Flame;  // 3.0.16-beta.24
    return p;
}

}  // namespace uagfx::magelook

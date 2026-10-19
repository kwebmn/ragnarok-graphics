#pragma once
#include "core/ai/groups.hpp"
#include "core/settings/strings.hpp"

namespace uagfx::app {

// 2.5.28: the AI groups' names (core/ai/groups.hpp's order) and the headings of their sections (ai::sectionOf) - the
// AI page's rows and the Texture pack page's line about the models.
using settings::Text;
inline constexpr Text kAiGroupLabels[ai::kGroups] = {
    Text::AiGroupGroundTown,    Text::AiGroupGroundField,   Text::AiGroupGroundDungeon, Text::AiGroupGroundIndoor,
    Text::AiGroupGroundOther,   Text::AiGroupBuildingsTown, Text::AiGroupModelsField,   Text::AiGroupModelsDungeon,
    Text::AiGroupFurniture,     Text::AiGroupModelsOther,   Text::AiGroupTrees,         Text::AiGroupPlants,
    Text::AiGroupWater,         Text::AiGroupBodies,        Text::AiGroupMounted,       Text::AiGroupHeads,
    Text::AiGroupWeapons,       Text::AiGroupShields,       Text::AiGroupHeadgear,      Text::AiGroupGarments,
    Text::AiGroupMonsters,      Text::AiGroupBosses,        Text::AiGroupHomunculi,     Text::AiGroupNpcs,
    Text::AiGroupItems,         Text::AiGroupItemIcons,     Text::AiGroupItemPictures,  Text::AiGroupCardPictures,
    Text::AiGroupSkillAnimations, Text::AiGroupEffectSprites, Text::AiGroupEmotions,    Text::AiGroupCursor,
    Text::AiGroupWindows,       Text::AiGroupMenuIcons,     Text::AiGroupPortraits,     Text::AiGroupMaps,
    Text::AiGroupLoading,       Text::AiGroupLogin};
inline constexpr Text kAiSectionLabels[ai::kSections] = {
    Text::AiSectionGround,   Text::AiSectionModels, Text::AiSectionCharacters, Text::AiSectionMonsters,
    Text::AiSectionItems,    Text::AiSectionEffects, Text::AiSectionInterface};

}  // namespace uagfx::app

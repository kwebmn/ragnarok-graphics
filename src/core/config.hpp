#pragma once
#include <array>
#include <string>

#include "core/ai/groups.hpp"
#include "core/closed_windows.hpp"
#include "core/login_view.hpp"
#include "core/roext.hpp"

namespace uagfx {

enum class LogLevel { Error = 0, Info = 1, Debug = 2 };
// [renderer] api (0.46): Direct3D 12, Direct3D 11 (graphics cards without Direct3D 12), or 12 and else 11.
// 2.12: System - uaRO Graphics switched off: the game draws with the system's DirectDraw (its own look)
enum class Api { D3d12, D3d11, Auto, System };
// [display] mode. Auto follows the game: a windowed game keeps its window (ROExt sizes it), full screen becomes
// borderless. Window and Borderless override what the game asks for.
enum class DisplayMode { Auto, Window, Borderless };
enum class UiFilter { Sharp, Smooth };  // [display] ui_filter: how magnified interface pictures are sampled
// 3.0.3-beta.6: [windows] switch_keys - Ctrl+Tab and Ctrl+Alt+1-8 (all), Ctrl+Tab alone (back) or none (off)
enum class WindowKeys : uint8_t { All, Back, Off };
enum class PanelLayout : uint8_t { Row, Column, Icons };  // 3.0.3-beta.8: [panel] layout; 3.0.3-beta.11 icons
// 3.0.11-beta.33: [interface] minimap_way - the way followed on the ground: footprints (steps), a strip (line), none
enum class WayOnGround : uint8_t { Steps, Line, Off };
// 3.0.11-beta.120: [interface] minimap_style - the maps' picture: the game's own (picture) or drawn from the walkable
// cells (scheme)
enum class MapStyle : uint8_t { Picture, Scheme };
// 3.0.11-beta.87 (the concept «Контроллер в интерфейсе uaRO», stage 3): the target's ring on the ground - off, while
// played with the controller, or always (the mouse's fight too)
enum class TargetRing : uint8_t { Off, Pad, Always };
// 3.0.11-beta.95: [interface] mob_hp - the monsters' HP bars: none, the target's, the target's and the wounded's, all
enum class MobHp : uint8_t { Off, Target, Wounded, All };
// 3.0.11-beta.88 (stage 4): how the controller's left stick walks - holding the game's cursor out (as before), or the
// pad asking for the cell itself with an arrow on the ground
enum class PadWalk : uint8_t { Mouse, Arrow };
// 3.0.11-beta.103 (the owner: «можешь сделать настройку, чтобы можно было выбрать эту стрелку в конфигураторе»):
// [interface] walk_arrow - the look of the stick's arrow on the ground, the variants of the doc «Стрелка хода с
// контроллера - варианты»: chevrons (its advice, B), a slim tapered arrow (A), beta.102's arrow of the concept, a ring
// round the feet with a pointer (C), dots of the way (D), the cell alone (E), none
enum class WalkLook : uint8_t { Off, Chevrons, Slim, Concept, Ring, Dots, Cell };
// 3.0.11-beta.198 (the tester: «курсор миші стандартний не дуже підходить в загальний стиль»; the owner: «да, сделай
// возможность курсор менять»): [interface] cursor - the game's own cursor, or the interface's: dark glass in a gold
// edge, or white in a dark one (core/cursor_art)
enum class CursorLook : uint8_t { Game, Dark, Light };
// 3.0.11-beta.35: [interface] buffs - the character's buffs: the game's own icons (off), the driver's column under the
// minimap, a row under it
enum class BuffsLook : uint8_t { Off, Column, Row };
// 3.0.11-beta.63: [interface] buffs_overflow - what the buffs' column does not hold: under «ще N» (more), in a second
// column on its left - the row's in a second row (column)
enum class BuffsOverflow : uint8_t { More, Column };
// 3.0.11-beta.36: [interface] character - the game's own Basic Info window (off), the character block with bars, with
// rings
enum class CharacterLook : uint8_t { Off, Bars, Rings };
// [display] window_art (0.41): how crisp text enlarges the windows' buttons, frames and labels - xBR (core/xbr.hpp),
// the AI (with [ai] ui_pieces) or as the game's pixels.
enum class WindowArt { Xbr, Ai, Pixels };

// [ai] (0.12; 0.11's [quality] ai_textures, ai_budget and ai_denoise are read when [ai] has no such key): the world's
// textures enlarged by a neural network (Real-ESRGAN): 0.48 - the pictures of the texture pack uaro-textures.zip next
// to uaRO.exe (upscaled.grf until 0.47), its builder on the Texture pack page.
struct AiSettings {
    // 1.5.8: the texture pack's pictures shown; generate (below) is a switch of its own, either one turns the AI
    // textures on.
    bool enabled = false;
    bool on() const { return enabled || generate; }
    // What it works on, the kinds the driver tells apart by how the game draws them (core/ai/kinds.hpp).
    bool ground = true;
    bool objects = true;   // buildings, trees, water
    // The sprites (pixel art the network may change too much), told apart by their files' folders (0.16; 0.13-0.15's
    // one "sprites" key sets all four where none of them is in the file).
    bool monsters = false;  // and homunculi, mercenaries
    bool players = false;   // bodies, heads, headgear, weapons, shields
    bool npcs = false;
    bool items = false;     // on the ground
    bool sprites() const { return monsters || players || npcs || items; }
    // 2.5.23: a sprite frame the game draws in a dye (the clothes' or the hair's colours) - the pack's picture of the
    // frame in its own colours, coloured as the dye (core/ai/recolour.hpp)
    bool dyes = true;
    bool effects = false;  // light and sparks
    bool icons = true;     // 0.14: item and skill icons in the game's windows (crisp text's twins)
    bool uiPieces = true;  // 0.15: with icons, the windows' small pieces too - buttons, tabs, slots (baked-in text)
    std::string model = "general";      // ground and objects: general (realistic) or anime (drawn)
    std::string spriteModel = "anime";  // sprites and effects
    // 1.6: a model for each group of pictures ([ai] model_<group>); "" - its family's. 2.5.28: 38 groups, the eight
    // of before their families ([ai] model_world, model_players ...; model_monsters and model_npcs are their head
    // group's own), and a family of none as model (the world) or sprite_model (the rest) say.
    std::array<std::string, ai::kGroups> models{};
    std::array<std::string, ai::kFamilies> familyModels{};
    const std::string& modelFor(ai::Group g) const {
        const std::string& own = models[size_t(g)];
        if (!own.empty()) return own;
        const ai::Family f = ai::familyOf(g);
        ai::Group head = g;
        if (ai::headOf(f, head) && !models[size_t(head)].empty()) return models[size_t(head)];
        const std::string& family = familyModels[size_t(f)];
        if (!family.empty()) return family;
        return f == ai::Family::World ? model : spriteModel;
    }
    // 1.6.3: pictures smaller than small_side a side (both sides, the game's pixels) by small_model whatever their
    // group's - anime 6B redraws a 24x24 icon its own way.
    bool smallPictures = false;
    int smallSide = 48;  // 16-256
    std::string smallModel = "anime";
    const std::string& modelForPicture(ai::Group g, int width, int height) const {
        return smallPictures && width < smallSide && height < smallSide ? smallModel : modelFor(g);
    }
    int denoise = 25;  // % of the grain the general model smooths away (Real-ESRGAN's denoise strength)
    int scale = 4;     // the result 4 or 2 times the game's texture (2: a quarter of the video memory)
    int budget = 2;    // ms of GPU time a frame, 1-20
    int minSize = 16;  // the textures it takes: both sides from min_size to max_size, 16-1024 (0.17: 16, was 64)
    int maxSize = 512;
    // 0.42: the network makes in the game what the texture pack (uaro-textures.zip) does not have (0.48: and writes it
    // into the pack) - a debugging aid of the pack's maker. Off: no model is even loaded; the players' textures come
    // from the pack. 1.5.8: with the pack off (enabled 0) it makes every picture anew and keeps none - the models
    // tried without touching the pack.
    bool generate = false;
    bool ahead = true;  // a map's textures the pack's index knows are taken at once, not when they come into view
    // 0.57: Direct3D 11 keeps the pictures compressed in video memory (BC1/BC3): "on", "off", "auto" (a card under
    // 3 GB)
    std::string compress = "auto";
    bool operator==(const AiSettings& o) const;
    bool operator!=(const AiSettings& o) const { return !(*this == o); }
};
// The page's sets: Recommended (the defaults: ground and objects), All (every kind), Custom (anything else).
enum class AiPreset { Recommended, All, Custom };
AiPreset aiPresetOf(const AiSettings& a);  // whatever `enabled` is
AiSettings aiPresetSettings(AiPreset p, bool enabled);  // Custom: the defaults
bool aiKind(const AiSettings& a, int kind);  // ai::Kind order: ground, objects, sprites, effects, icons
// The settings as one word for uaro-render64.exe's command line ("enabled=1;ground=1;...;ahead=1") and back.
std::string aiArgument(const AiSettings& a);
AiSettings parseAiArgument(const std::string& word);

// [effects] (0.18): the picture made more modern by the driver, drawn on top of what the game draws. They apply while
// the game runs; Pause switches them off with the other improvements.
// 2.2: the Effects page's set (core/effect_groups.hpp) - the card's size picks which effects are on, its groups
// (light, water...) each on or off with a strength; custom: every switch as written below.
enum class EffectPreset { Auto, Off, Weak, Medium, Strong, Custom };
// 2.2.1: the picture's fixes a group of their own; 2.6.22: fourteen, by what the eye is on (the user: «непонятно что
// где искать»), the ten before read from an old file (legacyGroups) until written into the effects
constexpr int kEffectGroups = 14;
constexpr int kOldEffectGroups = 10;

// 3.0.18-beta.183: [effects] skill_looks (EffectSettings::skillLooks)
enum SkillLooks : int { kSkillOriginal = 0, kSkillImproved = 1, kSkillRedrawn = 2, kSkillSoft = 3 };

struct EffectSettings {
    EffectPreset preset = EffectPreset::Custom;  // an ini without it keeps its switches
    bool groups[kEffectGroups] = {true, true, true, true, true, true, true, true, true, true, true, true, true, true};
    int groupStrength[kEffectGroups] = {100, 100, 100, 100, 100, 100, 100,
                                        100, 100, 100, 100, 100, 100, 100};  // %, 0-200
    // 2.6.22: an old file's group_<word> of the ten groups before (effect_groups::hasLegacy, settleLegacy)
    bool legacyGroups[kOldEffectGroups] = {true, true, true, true, true, true, true, true, true, true};
    int legacyStrength[kOldEffectGroups] = {100, 100, 100, 100, 100, 100, 100, 100, 100, 100};
    // 0.22.6: the map's baked light - torches' and lamps' pools, the shadows of houses and trees on the ground -
    // smooth instead of blotches and steps (the ground's coarse lightmap blurred within its cells). On by default.
    bool smoothLight = true;
    // (2.7.32's soft_map_light - the map's strong light laid over its ground and walls softly - gone in 2.7.33 at the
    // user's word: by the sewers' torch it changed nothing to be seen.)
    // 0.23: the maps' torches drawn anew (core/torch.hpp) - a flame that burns and sparks instead of the game's
    // magnified drop, a soft glow in the air around it instead of its red blotch, and its warm flickering light on
    // the ground and the walls. On by default.
    bool torch = true;
    bool torchFlame = true;     // 0.23.2: the new flame; else the game's own picture of it (the rest stays new)
    int torchBrightness = 100;  // %, 0-200 (0.23.2): how bright the new flame burns
    int torchGlow = 100;    // %, 0-200: the glow around the flame
    int torchLight = 100;   // %, 0-200: the light it throws around
    int torchFlicker = 50;  // %, 0-100: how much it flickers
    // 3.0.16-beta.16 (the owner: «тени от факелов и ламп» - «начинай»): torch_shadows - the torches' and the map's
    // lamps' light stopped by walls, models and the ground between (core/point_shadows.hpp, Direct3D 12); off at first.
    // torch_shadows_strength %, 0-100: how dark a shadow is (100 none of that light)
    bool torchShadows = false;
    int torchShadowsStrength = 80;
    // 3.0.16-beta.30 (the owner: «делай» - Korangar's light through the leaves): torch_leaf_light - a tree's leaves
    // between a lamp and the ground let a part of its light through, green, in spots (their own atlas rows,
    // torch_shadows.cpp); a part of torch_shadows, off at first
    bool torchLeafLight = false;
    // 3.0.16-beta.25 (the owner: «давай» - the body's stamp, from nostalRE): body_stamp - after the game's characters'
    // sprites (drawn writing no depth) each one's shape put into the depth a little behind its feet: an effect drawn
    // after them falls behind the body where it lies behind it (a ring's far half), in front where it lies in front
    // (win/gpu/body_stamp.cpp, Direct3D 12); off at first
    bool bodyStamp = false;
    // 3.0.16-beta.29 (the owner: «делай» - the sharp pixels, Korangar's idea): sharp_sprites - the characters', the
    // monsters' and the NPCs' sprites close up drawn as clean squares of their pixels with a soft edge one screen
    // pixel wide (no blur, no flicker); far off as before (shaders.cpp sharpTexel); off at first
    bool sharpSprites = false;
    // 0.24: the chimneys' smoke - each puff the game sends up drawn as billowing smoke, its wisps changing as it rises,
    // instead of the same small picture blown up. On by default.
    bool smoke = true;
    int smokeDensity = 100;  // %, 0-200: how thick it is
    // 0.25: the lamps' light - the lights the maps' makers placed (only baked into the ground until now) falls on the
    // characters and the walls too, and the fire-coloured ones flicker. On by default.
    bool lamps = true;
    int lampsLight = 60;  // %, 0-200: how bright
    // 0.26: the fire of skills - fire walls, bolts, meteors: effects whose pictures are fire-coloured - lights the
    // ground, the walls and the characters around it a little, as a torch does. On by default.
    bool fireLight = true;
    int fireLightStrength = 100;  // %, 0-200
    bool skillColours = true;  // 0.36: the other skills too, each in its own colour (ice blue, lightning white...)
    bool lightOrbs = true;     // 2.7.25: Ruwach's orb a small ball of light at the head of its trail (core/glint.hpp)
    // 0.37: the skills a little softer - their pictures' colour and alpha, as the aura's. On by default.
    bool skillsSoft = true;
    int skillBrightness = 80;  // %, 0-100 of the game's
    // 0.27: the level 99 aura - its ring, bubbles and circle - softer than the game draws it. On by default.
    bool aura = true;
    int auraBrightness = 50;  // %, 0-100 of the game's
    // 3.0.13-beta.15 (the owner's pick 8 of «50 идей»: «Аура не закрывает порталы»): a level 99 aura standing on a
    // warp portal drawn faint (core/aura_portal) - off as new looks are
    bool auraPortals = false;
    // 3.0.13-beta.30 (the owner: «давай попробуем просто силует», pick 6 of «50 идей»): the character's parts hidden
    // behind a house, a tree or a wall drawn over them as a light silhouette - off as new looks are
    bool silhouette = false;
    // 3.0.13-beta.20 (the owner's pick 9 of «50 идей»): others' skill effects - on actors neither the character nor a
    // party member here - at 30 % of the game's (a guild war, the market); off as new looks are
    bool crowdEffects = false;
    // 3.0.15-beta.11 (the owner: all the mages' skills in one go, «удобно переключать»): [effects] mage_effects - the
    // Mage's, Wizard's and Sage's skill effects in their own look (core/mage_looks), off as new looks are; under it
    // each part on its own: the casting (core/cast_look; beta.2's new_casting), the fire, water (beta.8's Storm Gust),
    // wind, earth and ghost skills
    bool mageEffects = false;
    bool mageCasting = true, mageFire = true, mageWater = true, mageWind = true, mageEarth = true, mageSpirit = true;
    // 3.0.16-beta.27 (the owner: Safety Wall's and the portals' look «слишком уж жесткие ... приглушить по типу как мы
    // сделали с фаерволом» - «2», one choice for all): mage_soft - every class's new skills' effects soft: the game's
    // own added light, tinted a little in the element's colours and brighter (Part::Soft), not the crisp layer
    bool mageSoft = false;
    // 3.0.18-beta.181 (the owner: Storm Gust «как в игре, только чётко»): crisp_storm_gust - Storm Gust as the game
    // draws it (its colours, its added light; the mages' look aside for it), its snow drawn again at real resolution
    // (core/sharp_specks, win/sharp_specks); off as new looks are
    bool crispStormGust = false;
    // 3.0.18-beta.183 (the owner: «оригинал, улушеный орининал и перерисованый ... для всех скилов магов», «и других
    // професий», «лучше одним пунктом»): [effects] skill_looks - every class's skills, one choice over what the set and
    // the groups leave on (effect_groups::looksStyled): the game's own, the game's made sharp (Storm Gust's snow so
    // far), redrawn (the classes' new looks), redrawn soft (mage_soft). The player's own, as the clock is.
    int skillLooks = 3;  // SkillLooks - 3.0.18-beta.186: the owner's «redraw soft поставь по умолчанию»
    // 3.0.18-beta.184 (the owner: «сделай все без остановки»): every skill's sheets of specks (the snow, the sparkles)
    // drawn anew at real resolution (core/sharp_specks) - made by looksStyled: any look but the original, while the
    // set and the groups leave the skills' looks on. The driver's own, no file's.
    bool sharpSpecks = false;
    // 3.0.15-beta.12 (the owner: «делай еффекты по всем остальным персонажам»): every other class's skills likewise
    // (core/mage_looks.hpp lookOf), a switch a branch, off as new looks are
    bool swordEffects = false, acolyteEffects = false, archerEffects = false, thiefEffects = false;
    bool merchantEffects = false, otherEffects = false;
    // 3.0.16-beta.1 (the owner: «делай все»): the hits, items, states, travel, monsters likewise
    bool hitEffects = false, itemEffects = false, statusEffects = false, travelEffects = false, monsterEffects = false;
    // 0.28: the maps' fireflies - those the game shows and those the maps' makers placed that it does not - glow,
    // flash now and then and light a little around them. On by default.
    bool fireflies = true;
    int firefliesBrightness = 100;  // %, 0-200
    // 3.0.7-beta.3 (the user: «сделать как лампы, чтобы от них исходил свет и освещал карту дополнительно. а их
    // убрать, разным светом»): with fireflies on, each lights the map from its place as a lamp, its own colour; the
    // fireflies themselves drawn in the air (the game's anew) only with this on, the game's left out with it off. Off
    // by default.
    bool fireflyDots = false;
    // 0.29: the maps' hidden lights - glows, pools of light, the towers' dust the maps' makers placed and the game
    // never shows - drawn from the map's file. On by default.
    bool mapGlows = true;
    int mapGlowsBrightness = 100;  // %, 0-200
    // 3.0.7-beta.1 (the user: «чтобы подсвечивали но не рисовались сами шары»): their orbs drawn in the air; off -
    // the light they throw alone. Off by default.
    bool mapGlowOrbs = false;
    // 0.30: the forest's beams of light (Umbala, Geffenia, Glast Heim) - soft ends, light shimmering as through
    // moving leaves, dust in them, light where they fall. On by default.
    bool rays = true;
    int raysBrightness = 100;  // %, 0-200
    // 0.31: lava (the Magma Dungeon, tak_run) - its crust drifting, its cracks glowing, its heat lighting the walls
    // and the characters around it. On by default.
    bool lava = true;
    int lavaBrightness = 100;  // %, 0-200
    // 0.32: the characters' shadows - each layer of their sprites laid on the ground from their feet, away from the
    // light - instead of the game's round ones. On by default.
    bool actorShadows = true;
    int actorShadowsStrength = 55;  // %, 0-100: how dark
    // 2.6.15: the game's round spot under the characters with their own shadows too; off - no two shadows: only where
    // none of theirs falls (core/actor_shadows.hpp spotShares). Off by default, at the player's word.
    bool actorShadowsSpot = false;
    // 0.35: heat haze - the picture shimmers over the torches' flames, burning skills, lava and far off over the
    // desert. On by default.
    bool heat = true;
    int heatStrength = 100;  // %, 0-200
    // 0.38: the portals glow around (2.7.54: the map warps; the air's swirl gone). On by default.
    bool portals = true;
    int portalsStrength = 100;  // %, 0-200
    // 2.7.48: portals - the game's warp zone drawn anew, sharper. 2.7.53: off while its draws were recorded (git
    // eea888f). 2.7.54: the game's own draws of the map warps shaded anew where it draws them (core/portal.hpp
    // partOf, portal_draw_ps) - on by default
    bool portalNew = true;
    // 0.39: the driver's own weather (core/weather.hpp): snow in Lutie, leaves in Payon, sand about Morroc, rain over
    // Niflheim and Einbroch. On by default.
    bool weather = true;
    int weatherAmount = 100;  // %, 0-200: how much of it
    // The game's light and sparks - what it adds to the picture: skills, portals, lamps - shine with a soft glow
    // (bloom from those draws alone; the rest of the picture keeps its brightness).
    bool glow = false;
    int glowStrength = 100;  // %, 0-200: how bright the glow is
    int glowSize = 50;       // %, 0-100: how far it spreads
    int glowLight = 50;      // %, 0-100 (0.20): how much the light falls on what is around, in its colour
    // 0.20: soft shadows where surfaces meet - at the walls, in the corners of buildings, under trees - on the map
    // and its models (screen-space ambient occlusion); the characters and the effects keep their light.
    bool ao = false;
    int aoStrength = 60;  // %, 0-100: how dark
    int aoSize = 50;      // %, 0-100: how far from a corner
    // 0.19: the maps' water drawn anew - waves, the picture above it reflected, the sun's glints, the bed seen
    // through it and darker with depth, foam along the shore.
    // 0.21: the picture's colours (not the interface's): saturation and brightness 100 as the game, contrast too;
    // warmth 50 neutral, less colder, more warmer; a vignette darkens the corners.
    bool grade = false;
    int gradeSaturation = 115;  // %, 0-200
    int gradeContrast = 108;    // %, 50-150
    int gradeBrightness = 100;  // %, 50-150
    int gradeWarmth = 50;       // %, 0-100
    int gradeVignette = 25;     // %, 0-100
    // 0.22: depth of field - sharp around the character in the middle of the screen, the rest softer with the
    // distance in depth from it (tilt-shift: the map looks like a model).
    // 0.63: night - the world darker and colder, the torches, lamps and skills shining out of it; by the clock: only
    // from 20:00 to 6:00 (dusk and dawn an hour each), the computer's time. nightNow: how much now (0-100) - the
    // driver's, each frame, not the file's.
    // 0.64: the leaves sway in the wind (trees, bushes: green textures with holes)
    bool foliage = false;
    int foliageStrength = 100;  // %, 0-200
    int nightStrength = 60;  // %, 0-100; 2.6.19: the night always the clock's (its switches gone)
    // 2.7.18 (the user: «очень темно ночью ... луна светит»): moon_light - the moon lights the night silver-blue
    // (night_ps); 2.7.19: the night's main light, the map bright under it. On by default. (2.7.19: hero_light - a
    // warm pool about the player's character - gone: «так себе, его можно убрать».)
    bool moonLight = true;
    int moonStrength = 100;  // %, 0-200
    int nightNow = 0;
    // 3.0.16-beta.37 (the owner: «давай дальше» - the backlog's Hallucination wave, nostalro's idea; uaRO.exe draws
    // nothing for the status, only its icon 34 and its chat line): hallucination_wave - while the character is under
    // Hallucination the world's picture sways in slow waves (heat_ps, Direct3D 12 and 11); off at first.
    // hallucinatingNow: the status on now - the driver's, each frame, not the file's
    bool hallucinationWave = false;
    bool hallucinatingNow = false;
    bool water = false;
    int waterWaves = 50;        // %, 0-100: 0 a still mirror
    int waterReflections = 50;  // %, 0-100: how much the water reflects
    int waterClarity = 50;      // %, 0-100: how deep one sees into it
    // 0.67: soft particles - the effects (skills, sparks, portals) fade where they meet the ground and the walls
    // instead of being cut along a line. On by default.
    bool softParticles = true;
    // 0.68: rim light - the torches, lamps and burning skills light the edge of a character (and of a model)
    // that faces them more: the flat sprites look round. On by default.
    bool rimLight = true;
    int rimStrength = 100;  // %, 0-200
    // (2.7.17: sky - the sky past the map's edge, its clouds and stars (0.69), and edge_land - the land past it
    // (1.18) - gone at the user's word.)
    // 0.70: motes in the air where the map has no weather - pollen drifting over the fields and the towns, dust
    // in the dungeons and the houses. On by default.
    bool motes = true;
    int motesAmount = 200;  // %, 0-200 (2.6.8: the tester's choice - at 100 all but unseen in Prontera, R-UT7AH3)
    // 0.71: the ground's relief - its picture's cracks and stones lit from one side (bump mapping from the
    // texture's brightness; the AI textures' detail the most). On by default.
    bool relief = true;
    int reliefStrength = 30;  // %, 0-200 (2.6.8: the tester's choice - at 100 the stones' tops turned gritty)
    // 0.72: reflections on the floor - indoors (polished and wet floors) and in the rain what stands on the
    // floor is reflected in it (screen-space). On by default.
    bool floorReflections = true;
    int floorReflectStrength = 35;  // %, 0-100 (in the rain 1.6 times)
    // 0.73: light in the air - the air glows about the torches, lamps and burning skills (hidden by the walls in
    // front), and on the fields rays stream from the sky past the map's edge. 0.84: off by default (too much).
    bool shafts = false;
    int shaftsStrength = 100;  // %, 0-200
    // 3.0.7-beta.5 (R-PB9EME «посмотри на скрине остались шарики»): the air glowing about the map's lamps, its hidden
    // lights and its fireflies too - balls of light in the air where no lamp is seen. Off by default: the fire's alone.
    bool shaftsLamps = false;
    // 0.74: the day going round (core/daylight.hpp): by the computer's clock the sun's shadows turn and lengthen
    // and its light warms in the morning and the evening. dayNow: the minute of the day now - the driver's, each
    // frame, not the file's (-1 off).
    bool dayCycle = false;
    int dayNow = -1;
    int clock = 0;  // 2.6.11: daylight::Clock - the computer's clock (0), or a moment the player chose
    int season = 0;  // 2.6.22: seasons::Choice - the computer's calendar (0), or a season the player chose
    int weatherKind = 0;  // 2.6.22: weather::Choice - the map's own (0), clear, or a weather the player chose
    // 0.78: bloom - the bright parts of the picture (sunlit tiles, snow, windows, fire) glow softly. 0.84: off by
    // default. (2.6.11: the filmic curve with it gone at the player's word.)
    bool bloom = false;
    int bloomStrength = 60;  // %, 0-200
    // 0.79: each region its colours (core/region_look.hpp): Payon warm and green, Niflheim cold and pale, Morroc
    // sun-bleached... 0.84: off by default.
    bool regionLooks = false;
    int regionLooksStrength = 100;  // %, 0-100
    // 0.80: footprints in the snow and the sand, dust under the feet on dry ground; a thunderstorm's lightning
    // where it rains. 0.84: both off by default.
    bool footprints = false;
    bool storms = false;
    // 0.81: grass - blades on the green ground of the fields, swaying in the wind and trodden down where the
    // characters walk. Off by default: it costs the card the most of the effects.
    bool grass = false;
    int grassAmount = 70;  // %, 0-100
    // 3.0.4-beta.35 (the user: «у людей отключается трава на слабых видеокартах»): the driver's own, in no file - the
    // grass thinned, its roots on every second row (half its work): a card weak by its memory's, and keep_fps's first
    // step before it turns anything off (effect_costs::pickToDrop)
    bool grassThin = false;
    // 0.82: the skills' sparks - embers rise from burning skills, glints of their colour spread from the others.
    // 0.84: off by default.
    bool skillSparks = false;
    int skillSparksAmount = 100;  // %, 0-200
    // 0.83: the world coming in from black after a new map; 0.84: off by default. (2.6.11: the film grain with it
    // gone at the player's word.)
    bool mapFade = false;
    // 2.6.10: the game's own fog (its fogparametertable: prt_fild08's light green, prontera's white) left out on the
    // fields and in the towns, where it lay as a coloured veil, thicker as the camera went back (R-P6KRQF); the
    // dungeons keep theirs (dark, hiding the void past the map). A fix, in every set.
    bool noGameFog = true;
    // 2.6.13: the server's night (its status SI_NIGHT) does not darken the picture: the game's half-dark blue light
    // left out, the time of day the clock's and the night's (win/server_night.hpp). A fix, in every set.
    bool noServerNight = true;
    // 3.0.18-beta.229 (R-H2CQCJ «негатив», the washed ground on moc_pryd05 and aldebaran): an effect's inverted texture
    // (the game's preset 6) kept to its own draw - the game never set it back, so the world after it came out a
    // negative or its ground washed white (core/states DeviceStates::dropComplement). A fix, in every set.
    bool fixNegative = true;
    // 0.84: round characters - the torches' light across a character as if it were round (0.79 with the rim light;
    // now its own switch). Off by default.
    bool roundLight = false;
    // (2.7.10: appear - monsters, characters, NPCs and items fading in where they came into view - gone at the
    // user's word.)
    // 0.94: ripples - rings on the modern water where characters and monsters step (Direct3D 12). Off by default.
    bool ripples = false;
    // 0.95: glint - a card lying on the ground sparkles now and then (core/glint.hpp). Off by default.
    bool glint = false;
    // 0.96: hit_flash - a monster hit flashes white for a moment (core/hit_flash.hpp, win/hit_hook.cpp). Off by
    // default.
    bool hitFlash = false;
    // 3.0.16-beta.7 (the owner: «делай все» - a death's flash): death_flash - a monster dying (its «_die» sound)
    // flashes brighter and longer than a hit (core/hit_flash.hpp). Off by default.
    bool deathFlash = false;
    // 3.0.16-beta.13 (the owner's pick 15 of «20 идей»): body_tints - Fury a warm red, Steel Body cool steel added
    // over its body (core/afterimage, Device7). (.13's afterimages behind a fast hero gone in .14: «убери».)
    bool bodyTints = false;
    // 0.97: wet_ground - where it rains the ground is darker and puddles mirror the world (floor_reflect_ps, Direct3D
    // 12). Off by default.
    bool wetGround = false;
    // (2.7.10: flare - the sun's flare where the sky showed towards the sun - gone: the camera looks down at the
    // ground, the sky is hardly ever in the picture.)
    // 1.7: the living world, trees: tree_volume - a crown lit as a round shape (its side to the sun lighter);
    // leaf_light - the leaves on the crown's sunward edge lit through; tree_tints - each tree a shade of its
    // own, autumn colours in autumn, rime in Lutie. Off by default. (2.6.22: dapple - the sun's spots in the trees'
    // shadows - gone: it drew nothing since the sun's shadows went in 2.1.4. 2.7.10: gusts - a band of wind across the
    // grass and the woods - gone: the tester could not see it.)
    bool treeVolume = false;
    bool leafLight = false;
    bool treeTints = false;
    // 1.7.1: the living world, the ground: flowers in the grass, dew on it in the morning (with grass); a low morning
    // fog (2.6.8: on its own, the haze gone); sand drifting over the desert, snow glittering, damp caves' walls
    // glistening. Off by default.
    bool flowers = false;
    bool dew = false;
    bool morningFog = false;
    bool sandDrift = false;
    // (2.7.45: snow_sparkle gone - after three rounds in Lutie its glints were hardly seen; the user: «может уберем»)
    bool wetWalls = false;
    // 1.7.2: the living world, the water (with water): swamp - a swamp's bubbles and mist. Off by default. (2.7.2: the
    // fish under the surface and the surf - its swash and the wet sand above it - gone at the user's word: too faint
    // to be seen; the foam along the shore stays the water's own, water_foam.)
    bool swamp = false;
    // 1.7.3: the living world, creatures and particles (core/critters.hpp, Direct3D 12): butterflies and dragonflies,
    // birds, leaves falling from the crowns, embers from the fires, splashes, the caves' drips, bubbles and light under
    // the sea. Off by default. (2.7.45: breath in the cold gone with the snow's glints - hardly seen, the user's word.)
    bool butterflies = false;
    bool birds = false;
    bool nightFireflies = false;  // 2.6.11: fireflies over the grass from dusk to dawn
    bool firefliesLight = true;   // 2.7.18: night_fireflies - each lights the ground under it a little
    bool moonBeams = true;        // 2.7.20: moon_light - its beams on the ground, drifting
    bool leafFall = false;
    bool embers = false;
    bool splashes = false;
    bool caveDrips = false;
    bool underwater = false;
    // 1.8: the remaster's first step, the materials: each texture of the ground and the models gets its relief, height
    // and gloss, made on the card from its picture; the sun lights them, smooth stone glints, the ground's
    // repeats hidden under large patches; with the remaster (tier) also its depth (parallax). Off by default.
    int tier = 0;  // 1.8: [effects] tier - 0 auto (by the card), 1 classic+ (any card), 2 remaster (a strong one)
    bool materials = false;
    int materialsStrength = 50;  // %, 0-200 (2.6.6: the tester's choice, 100 looked pressed into the stone)
    // 1.9: the remaster's ground: ground_seams - where two cells of the ground with different textures meet, the line
    // between them wanders over the world instead of running along the cells (core/seams.hpp). Off by default.
    bool groundSeams = false;

    // 1.15: the remaster's smooth hills (core/hills.hpp, shaders.cpp hillLight): the ground lit by its corners'
    // averaged normals, not cell by cell - the sun lights the slopes smoothly. Off by default.
    bool smoothHills = false;
    int smoothHillsStrength = 60;  // %, 0-200
    // 3.0.4-beta.10 (the user: «сделай 1»), own_ground: the own world's prototype - the ground drawn from the map's
    // .gnd (core/terrain.hpp: smooth, its textures in full colour, no cut at the far plane) in place of the game's
    // cells (gpu/own_ground.cpp, Direct3D 12)
    bool ownGround = false;
    // 3.0.4-beta.14 (the user chose «Дома и деревья вдали»), a part of own_ground: the map's houses, trees and
    // rocks from its files (core/own_models.hpp) past the game's far plane, where it cuts them off
    bool ownFarModels = true;
    // 1.16: the remaster's world reflections (mirror.cpp): the last frame's opaque world turned over the water's
    // level, read by water_ps where the picture has nothing to reflect. Remaster mode. Off by default.
    bool worldReflections = false;
    // 1.17: living characters (shaders.cpp spriteLight): a character's sprite lit by the sun from its silhouette's
    // sides, a warm rim towards it; with Real sun shadows darker in a house's or a crown's shadow. Off by default.
    bool spriteLight = false;
    int spriteLightStrength = 100;  // %, 0-200 (2.6.4: the body's light, seen at 100; the old rim was 60)
    // 1.20: the year on the whole map (core/seasons.hpp) and the rain's wet on the walls and roofs
    // (ground_life_ps). Off by default.
    bool seasons = false;
    bool rainWet = false;
    // 1.21: windows at night (material_ps finds the panes, shade() lights them): the houses' windows glow warm as
    // the night comes. Needs the materials. Off by default.
    bool nightWindows = false;
    // 1.22: one wind for the whole world (core/wind.hpp): the grass, the leaves, the sand, the rain, the snow and the
    // clouds go its way, which turns slowly, its strength coming and going. Off by default.
    bool sharedWind = false;
    // 1.23: dust in the light (weather_ps): indoors the motes show where light falls - the beams, the torches - and
    // hardly in the dark. Needs the motes. Off by default.
    bool litDust = false;
    // 2.4.1: 1.10's and 2.0's new trees (new_trees, anime_trees) are gone - the player keeps the game's own trees.
    // 2.4: the parts of what an effect draws, each a box of its own under it (the Effects page, the game's console) -
    // on, as each was drawn before; its effect off, it is off too. 1.9.8's pebbles (ground_clutter) are gone: specks
    // a few pixels across over every paved square, as were the seasons' leaves on the ground.
    bool waterFoam = true;      // water: the foam along the shore
    bool waterGlints = true;    // water: the sun's glints on the waves
    bool caustics = true;       // water: the light the waves focus on a shallow bed
    bool waterfalls = true;     // water: the waterfalls the maps' makers placed (0.65)
    bool stepDust = true;       // footprints: the dust a step lifts on dry ground
    bool seasonSnow = true;     // seasons: winter's snow on what faces up
    bool seasonGrass = true;    // seasons: the green ground drier in autumn, greener in spring
    bool pollen = true;         // motes: under the open sky
    bool indoorDust = true;     // motes: in the dungeons and the houses
    bool rainSplashes = true;   // weather: the rain's rings on the ground
    bool desertHeat = true;     // heat: far off in the desert
    bool skyRays = true;        // shafts: the rays from the sky over the fields
    bool treeSeasons = true;    // tree_tints: autumn's golden crowns, Lutie's rime
    bool dragonflies = true;    // butterflies: the dragonflies over the water
    bool operator==(const EffectSettings& o) const;
    bool operator!=(const EffectSettings& o) const { return !(*this == o); }
};
// Pause (the compare key): the game's own picture - every effect off.
EffectSettings effectsOff();
// 0.46.1: one of the effects the Direct3D 12 renderer draws itself is on (all but the aura's and the skills'
// softness, which the game's draws get on their way).
bool drawnByRenderer(const EffectSettings& e);
// 0.63: how much night now (0-100, EffectSettings::nightNow) at this time of day: its strength from 20:00 to 6:00 -
// rising from 20:00 to 21:00, fading from 5:00 to 6:00 (2.6.19: always by the clock, «Часы» the master).
int nightAmount(const EffectSettings& e, int hour, int minute);
// 2.6.11: dayNow and nightNow by the clock the player chose ([effects] clock) or the computer's (localMinute);
// 2.6.19: «От сервера» the server clock's minute (daylight::ServerClock; below 0 nothing heard)
void applyClock(EffectSettings& e, int localMinute, int serverMinute = -1);

// uaro-graphics.ini. Unknown keys and values that do not parse leave the default in place.
// 0.91: [sound] - the game's sound through the driver (core/sound_mix.hpp, win/sound_hook.cpp)
struct SoundSettings {
    int rate = 44100;     // rate: 0 the game's (22050 Hz at most), 44100 or 48000
    bool voices = true;   // voices: the most sounds at once the game holds (48)
    int background = 30;  // background: % of the volume while the game is not in front (100 = as it is)
    bool limiter = true;  // limiter: a crowd's sounds made quieter together
    int stereo = 60;        // 0.92, stereo: % of the way to the left or right a sound beside the player goes
    bool stereoFlip = false;  // stereo_flip: left and right the other way round (3.0.11-beta.301: on the page)
    bool steal = true;      // steal: all samples busy - the quietest stopped for a new sound
    bool crossfade = true;  // crossfade: the music's pieces change over 1.5 s
    bool engine = false;    // 1.1, engine: the sounds at a place by the driver's engine (XAudio2): around the player
    int reverb = 60;        // reverb: the place's reverberation with the engine, 0-100
    bool headphones = true;  // 1.2, headphones: Windows' spatial sound (Sonic, Atmos) when it is on in Windows
    bool duck = true;        // 1.3, duck: the music quieter (to 65 %) while many sounds play at once
    // 3.0.4-beta.1, output: the Windows output device it plays on (its endpoint id; "" - auto: as in Windows;
    // core/audio_output.hpp)
    std::string output;
    // 3.0.4-beta.7, ambience: the maps' own sounds - birds, brooks, waves, the wind (core/ambience.hpp); off as new
    // things ship; ambience_volume - their share of the game's effects' volume (%)
    bool ambience = false;
    int ambienceVolume = 60;
};

// 3.0.4-beta.3, [notify] (core/notify.hpp): a game window behind others tells of these - on, then each kind
struct NotifySettings {
    bool on = true;
    bool whisper = true, party = true, trade = true, guild = true, friendAsk = true, death = true;
    // 3.0.7-beta.6 (the user chose 1, 5, 6, 7, 8, 9, 10): an attack on the character (once, again after a calm), a
    // buff ending (buffSeconds before), one of the party near dead or under partyHp per cent, the party's and the
    // guild's chat, the character's name said near, the server gone; sound - Windows' own with each notification
    bool attack = true, buffEnding = true, partyDeath = true, partyLowHp = true, partyChat = true, guildChat = true,
         mention = true, disconnect = true, sound = true;
    int buffSeconds = 10, partyHp = 30;
    // 3.0.7-beta.7 (the user: «надо сделать чтобы когда хп и сп восстановились»): HP and SP both back to
    // recoveredPercent of their most
    bool recovered = true;
    int recoveredPercent = 100;
    bool reportAnswer = true;  // 3.0.9-beta.4: the developer answered a report (the game's card when in front)
};
// [notify]'s boxes by their ini words (3.0.7-beta.6: one list for the file, the configurator and the live update)
struct NotifyBox {
    const char* key;
    bool NotifySettings::*field;
};
extern const NotifyBox kNotifyBoxes[];
extern const size_t kNotifyBoxCount;
constexpr int kBuffSecondsLeast = 3, kBuffSecondsMost = 60, kPartyHpLeast = 10, kPartyHpMost = 90;
constexpr int kRecoveredLeast = 50, kRecoveredMost = 100;
// 2.6.17: [connection] - the legacy client's way onto a map (core/map_entry.hpp, win/map_entry_hook.hpp)
struct ConnectionSettings {
    bool patientEntry = true;  // patient_entry: wait for the map up to a minute, a failed connect made again
    bool entryPause = true;    // entry_pause: after a failed entry the next selection waits till the map forgets it
};

struct Config {
    Api api = Api::D3d12;
    // Part 3 (test): [renderer] bridge = 1 draws in the 64-bit process uaro-render64.exe; bridge_present = auto,
    // window or composition (how its picture reaches the game's window); bridge_dpi_scale = 1 scales a composition
    // surface for Windows scaling of the game's window.
    bool bridge = false;
    std::string bridgePresent = "auto";
    // 3.0.4-beta.8: [renderer] bridge_wine = 1 - the 64-bit renderer under Wine too (core/platform.hpp; a try)
    bool bridgeWine = false;
    // 2.5.12 (test): [renderer] render_thread = 1 - Direct3D 11 draws on a thread of its own in the game's process
    // (gpu/local_host.cpp): the game's thread only writes the frame down, as for uaro-render64.exe
    bool renderThread = false;
    // 0.50.2: [renderer] gpu - the name of the graphics card to draw on (Windows' own, as the Renderer page lists
    // them); "" (auto): the fast one of several (core/gpu_order.hpp).
    std::string gpu;
    // 3.0.19-beta.9 (R-T8FGMM): [renderer] cross_present = 1 - Direct3D 12's frames shown through the card the game's
    // monitor hangs off when it is another one (gpu/cross_present.cpp); the present watch turns it on by itself
    bool crossPresent = false;
    // 0.51: [report] auto - a report to the developers (uaro.kiev.ua) when a game ended badly: the settings app's
    // watcher, started by the driver, sends it after the game (core/report_upload.hpp).
    bool reportAuto = false;
    // 2.2.1: [stats] send - the anonymous statistics of the settings played with (core/usage.hpp), asked once by the
    // settings app; id - this installation's random number there (no name, no login)
    bool statsSend = false;
    std::string statsId;
    // 2.9: [pad] enabled - uaRO Pad, a part of the driver now, mounted at the game's start (src/pad); on by default.
    // Its own [general] enabled in uaro-pad.ini is the in-game switch (Back).
    bool padEnabled = true;
    // 2.11: [launcher] direct_start - a direct start of uaRO.exe plays at once instead of opening «Гра»
    bool directStart = false;
    // 3.0.1-beta.5: [client] cps, bink - our own libraries in place of the game's old ones (core/client_libs.hpp),
    // each on by default; off puts the game's own back (the settings app, with the game closed)
    bool clientCps = true, clientBink = true, clientIjl = true;  // 3.0.1-beta.6: ijl
    bool clientMss = true;  // 3.0.1-beta.7: mss32.dll, put in only where XAudio2 2.9 is
    // 3.0.1-beta.8: [client] roext - ROExt's work (the client's dinput8.dll) in the driver, the DLL moved aside;
    // [roext] its settings (core/roext.hpp), dinput8.ini's brought over by the installer
    bool clientRoext = true;
    roext::Settings roext;
    // 3.0.3-beta.2 (the user, several game windows at once): [windows] title_name - the character's name in the game
    // window's title; remember_place - each character's window (a framed one) where it was put (core/window_places)
    bool windowTitleName = true, windowRememberPlace = true;
    // 3.0.3-beta.3: others_light - a game window while another one is the main (in front, or the last in front) drawn
    // light (core/light_mode); others_music - its music too (off: the main window's alone)
    bool windowOthersLight = true, windowOthersMusic = false;
    bool windowOthersSounds = false;  // 3.0.3-beta.9: others_sounds - their sounds too (off: the main window's alone)
    int windowStartCount = 1;  // 3.0.4-beta.4: start_count - the game windows the settings app's «Грати» starts (1-8)
    // 3.0.3-beta.6: switch_keys - the keys that switch the game windows (core/game_windows)
    WindowKeys windowKeys = WindowKeys::All;
    // 3.0.3-beta.8: [panel] - the game windows' panel in the game's picture (core/window_panel, win/window_panel):
    // show; layout - its cards in a row or a column; size - % over the game's interface scale (75-150); opacity (40-100
    // %); hidden (Ctrl+Alt+0, its cross) and place ("x y", core/window_panel's Place; "" its first place) written by
    // the driver
    bool panelShow = true, panelHidden = false;
    // 3.0.11-beta.1: [interface] minimap - the new interface's minimap prototype (stage 1): the game's own hidden, the
    // driver's panel in its corner with the player's cell (core/minimap_proto)
    bool newMinimap = true;  // 3.0.11 (the owner before the release: «Включить всем») - the new interface for all
    // 3.0.11-beta.6: [interface] minimap_npcs / minimap_warps / minimap_mobs - its hints from the game's reference
    // (core/reference_index): the NPCs as dots, the warps to other maps as rings, the map's monsters under it
    bool minimapNpcs = true, minimapWarps = true, minimapMobs = true;
    // 3.0.11-beta.119 (R-5Z6J2S): [interface] minimap_kafra / minimap_party - the Kafras and the party's faces, each a
    // layer of its own (core/minimap_proto Layer), switched on the maps themselves too
    bool minimapKafra = true, minimapParty = true;
    // 3.0.11-beta.187 (the tester took it for the character's sight): [interface] minimap_view - the camera's fan round
    // the minimap's arrow (core/minimap_proto Scene::viewFan)
    bool minimapView = true;
    // 3.0.11-beta.222 (the tester: «togle в налаштуваннях старий \ новий стиль»): [interface] minimap_classic - the
    // minimap's marks as round dots in the game's colours (core/minimap_proto Scene::classicMarks)
    bool minimapClassic = false;
    // 3.0.11-beta.236 (the concept «Блок персонажа»: «внизу экрана показывать опыт… включить»): [interface]
    // exp_bars - the base's and the job's experience as thin bars along the screen's bottom (win/exp_bars)
    bool expBars = true;
    // 3.0.11-beta.237 (the owner: «над мобом было типа + столько то опыта»): [interface] exp_gain - a kill's experience
    // rising over the monster, and its label by the bars (win/exp_gain)
    bool expGain = true;
    // 3.0.11-beta.240 (the chat concept, stage 1): [interface] chat_fade - idle seconds before the chat's ground fades
    // (12, 30; 0 never)
    int chatFade = 12;
    // 3.0.13-beta.3 (the owner's pick 24 of «50 идей»): [interface] chat_party, chat_guild, chat_own, chat_others -
    // the chat's colours by name (core/chat_panel chatColour), "default" the chat's own
    // 3.0.13-beta.3 (the owner's pick 28): [interface] chat_log - the chat's lines into uaro-graphics\chat-logs, a
    // file a day; off unless asked (whispers stay on the disk)
    bool chatLog = false;
    // 3.0.13-beta.12 (the owner's pick 21): [interface] long_chat - the field takes 234 letters, sent in pieces
    bool longChat = true;
    std::string chatParty = "default", chatGuild = "default", chatOwn = "default", chatOthers = "default";
    // 3.0.11-beta.120 (the owner: «карта ... какаято не красивая»): [interface] minimap_style - the minimap's and the
    // big map's picture: the game's own, or «Схема» drawn from the server's walkable cells (core/minimap_proto
    // schemePixels); switched on the maps themselves too
    MapStyle minimapStyle = MapStyle::Picture;
    // 3.0.11-beta.122 (the network ideas, 7): [interface] net_badge - the round trip to the map server on the
    // minimap's bar (win/net_watch): a dot green, amber or red, a stall in red
    bool netBadge = true;
    // 3.0.11-beta.123 (the network plan's step 3): [interface] smooth_fixes - the server's position fixes (0x0088) of
    // 1-3 cells glided to instead of jumped (win/net_smooth)
    bool smoothFixes = true;
    // 3.0.11-beta.130 (the network plan's step 5; the owner «делаем»): [interface] predict_walk - the character's walk
    // told at the click and the server's answer joined to it (core/net_predict, win/net_predict); an experiment, off
    bool predictWalk = false;
    // 3.0.11-beta.33: [interface] minimap_way - the way followed drawn on the ground (win/ground_way): as footprints
    // (the owner: «такими типа следами типа шажков»), as the strip of 3.0.11-beta.31, or not
    WayOnGround minimapWay = WayOnGround::Steps;
    // 3.0.11-beta.65: [interface] minimap_reach - the concept's «Подсветка, куда можно дойти», an experiment (off): the
    // minimap and its big map dim what the player cannot reach on foot (walkgrid parts)
    bool minimapReach = false;
    // 3.0.11-beta.72 (the owner: «сами картинки карт маленькие и не аккуратные»): [interface] minimap_hd - the map's
    // picture four times bigger when the game has one (the AI pack's, the old client's texture_x4.zip); on
    bool minimapHd = true;
    // 3.0.11-beta.74 (the owner: «иконки скилов, предметов, и всего остального также по возможности грузить в текстур
    // которые обработаны»): [interface] pack_icons - the panels' icons (skills, items, status icons) from the AI
    // texture pack when it has them (win/pack_pictures); on
    bool packIcons = true;
    // 3.0.11-beta.77 (the owner: «чтобы без быстрых кнопок можно было двигать, чтобы сразу все мышкой»): [interface]
    // lock_layout - the new interface's parts kept where they are: no dragging, no sizing grip (win/hud_edit); off
    bool lockLayout = false;
    // 3.0.11-beta.78 (the owner: «на панели скилов не те скилы появляются, с других аккаунтов, плюс появляются те
    // которых на чаре нету»): [interface] hotbar_per_character - each character's own hotkeys bar, kept by the driver
    // (the game keeps one by the selection screen's place, every account's alike - win/hotbar); on
    bool hotbarPerCharacter = true;
    // 3.0.11-beta.125 (the owner: «когда играю арчером, есть стрелы, и не видно сколько осталось ... чтобы словить
    // момент когда они заканчиваются»): [interface] hotbar_ammo - the ammo worn and how many left on a pill over the
    // hotbar (core/hotbar_panel Ammo, win/ammo_watch); on
    bool hotbarAmmo = true;
    // 3.0.16-beta.8 (the owner: «для магов и пристов показывать количество гемов, а для монков ... сферы»):
    // [interface] hotbar_supplies - what the bar's skills spend (gems, Holy Water, stones, bottles) and the spirit
    // spheres on pills after the ammo's (core/supplies, win/sphere_watch); off
    bool hotbarSupplies = false;
    // 3.0.16-beta.13 (the owner's picks 1, 5, 6, 19, 20 of «20 идей»): [interface] skill_needs (a skill short of its
    // gem or spheres a red plate), combo_window (the Monk's chain gold, Asura ready blue), broken_gear, pet_hunger
    // (win/upkeep_watch); all off
    bool skillNeeds = false, comboWindow = false, brokenGear = false, petHunger = false;
    // 3.0.16-beta.21 (the owner's pick 7 of «20 идей»): ground_timers - the own walls' and traps' time rings on the
    // ground (win/ground_marks); off at first
    bool groundTimers = false;
    // 3.0.11-beta.129 (the owner «делай»): [interface] ammo_sound - a short soft chime when 10 arrows are left and
    // when none are (core/ui_chime, win/ammo_sound through the game's own sound player); on
    bool ammoSound = true;
    // 3.0.18-beta.144 (the owner: element things such as a wind weapon in their own colour): [interface]
    // element_colours - a thing with an element (core/item_elements: the server's table, a forged one's, an armour
    // card's) in its colour - its name in the lists and the tip, a dot in its cell; on
    bool elementColours = true;
    // 3.0.11-beta.86 (the concept «Контроллер в интерфейсе uaRO», stage 2): [interface] pad_hints - played with the
    // controller (core/input_mode), its buttons on the interface instead of the keys' names (core/pad_glyphs); on
    bool padHints = true;
    // 3.0.11-beta.87 (stage 3): [interface] target_ring - the pad's target in a gold ring on the ground
    // (win/ground_marks)
    TargetRing targetRing = TargetRing::Pad;
    // 3.0.11-beta.95 (the owner: «Полоски HP мобов - всем показывай»): [interface] mob_hp - HP bars over the monsters'
    // heads from their name lines' per cent (core/mob_hp, win/mob_hp); the target's and the wounded's
    MobHp mobHp = MobHp::All;  // 3.0.11-beta.100 (the owner: «а можно чтобы над мобами всегда хп висело?»)
    // 3.0.11-beta.293 (R-9R6G7A, the owner: «это окошко возле моба не надо»): [interface] mob_hp_percent - the per
    // cent's plate by the target's bar
    bool mobHpPercent = false;
    // 3.0.13-beta.3 (the owner's pick 10 of «50 идей»): [interface] boss_bars - an MVP's bar wider and gold, another
    // boss's wider and violet; off as new looks are
    bool bossBars = false;
    // 3.0.13-beta.13 (the owner's pick 44 of «50 идей»): [interface] who_hits - the monsters hitting the character
    bool whoHits = false;
    // 3.0.13-beta.7 (the owner's pick 18 of «50 идей»): [interface] quick_teleport - a teleport on the same map without
    // the game's 255 ms of darkening (win/teleport_fade)
    bool quickTeleport = false;
    // 3.0.13-beta.8 (the owner's pick 2 of «50 идей»): [interface] screen_shake - Earthquake and the like shake the
    // camera (the game's); 0 never (win/teleport_fade stillScreen)
    bool screenShake = true;
    // 3.0.13-beta.8 (the owner's pick 23 of «50 идей»): [interface] skill_jokes - Frost Joke and Scream say their line
    // in the chat (the game's); 0 not
    bool skillJokes = true;
    // 3.0.13-beta.9 (the owner's pick 25 of «50 идей»): [interface] swear_filter - the game refuses texts with words of
    // manner.txt (the game's); 0 not
    bool swearFilter = true;
    // 3.0.13-beta.10 (the owner's pick 29 of «50 идей»): [interface] esc_cutin - Esc closes the NPC's picture too
    bool escCutin = true;
    // 3.0.13-beta.11 (pick 42 of «50 идей»): [interface] follow - Shift+click on a player follows it for good
    bool follow = true;
    // 3.0.11-beta.88 (stage 4): [interface] pad_walk - the left stick walks without the mouse, an arrow on the ground
    // (a prototype for the owner: off till checked)
    PadWalk padWalk = PadWalk::Arrow;  // 3.0.11-beta.102: by the map's cells by default (core/stick_walk)
    // 3.0.11-beta.103: [interface] walk_arrow - how the arrow of pad_walk = arrow looks (core/ground_way walkShape)
    WalkLook walkArrow = WalkLook::Chevrons;
    // 3.0.18-beta.125 (the concept «Ігровий інтерфейс»: «Клавіші»): [interface] key_hunt,
    // key_mvp, key_tracker, key_party, key_recap - our windows' Ctrl letters (by closedwin::Own; 0 none), one of
    // closedwin::kFreeLetters
    closedwin::Keys ownKeys = closedwin::kDefaultKeys;
    CursorLook cursorLook = CursorLook::Game;  // 3.0.11-beta.198: [interface] cursor
    // 3.0.11-beta.105 (the owner: «а можно клетку куда идти над стрелкой не рисовать?»): [interface] walk_mark - the
    // mark at the arrow's end where the character goes (a diamond, a cell's corners or frame, a small ring)
    bool walkMark = false;
    // 3.0.18-beta.207 (the keyboard concept, stage 1): [interface] arrows_walk - the arrows walk while the chat line is
    // empty (win/key_walk)
    bool arrowsWalk = true;
    // 3.0.18-beta.208 (stage 2): [interface] walk_mode - the walk mode on (` switches it in the game, written here)
    bool walkMode = false;
    // 3.0.18-beta.209 (stage 3): [interface] space_action - Space with the chat line empty acts (the pad's A)
    bool spaceAction = true;
    // 3.0.11-beta.90 (stage 6): [interface] pad_tiles - Start opens the windows' tile (win/pad_tiles); off: the pad's
    // own menu, as before
    bool padTiles = true;
    // 3.0.11-beta.35: [interface] buffs - stage 7 of the new interface (the HUD, the buffs first; the owner's approved
    // column, the row a choice): the game's own status icons hidden, the driver's under the minimap (core/buff_panel)
    BuffsLook buffs = BuffsLook::Off;
    // 3.0.11-beta.63: [interface] buffs_overflow - the concept's question 17: «ще N» by default (variant 1), a second
    // column a choice (variant 2); the debuffs that do not fit open columns of their own either way
    BuffsOverflow buffsOverflow = BuffsOverflow::More;
    // 3.0.11-beta.36: [interface] character - the character block at the top left (core/char_block), the game's Basic
    // Info window hidden; its compact look bars or rings (the owner: «кольца или полоски на выбор»)
    CharacterLook character = CharacterLook::Off;
    // 3.0.11-beta.38: [interface] target - the target's card at the top middle (core/target_card): the monster the
    // character fights, its element and what hurts it; its card in «Довідник» by a click
    bool targetCard = false;
    // 3.0.11-beta.39: [interface] party - the party's panel at the top left (core/party_panel): each member's HP per
    // cent with its bar, its map when elsewhere
    bool partyPanel = false;
    // 3.0.11-beta.70: [interface] tracker - the concept's «Відстеження» left of the buffs (core/tracker_panel): the
    // quests, the way and the items the player pinned
    bool tracker = false;
    // 3.0.11-beta.40: [interface] hotbar - stage 8 of the new interface: the hotkeys' bar at the bottom middle
    // (core/hotbar_panel), the game's own hidden
    bool hotbar = false;
    // 3.0.11-beta.41: [interface] chat - stage 8: the chat at the bottom left with its tabs (core/chat_panel), the
    // game's own out of sight
    bool chat = false;
    // 3.0.11-beta.42: [interface] items and status - stage 8's windows: «Речі» (core/item_window) and
    // «Характеристики» (core/stat_window) in place of the game's own while they are open
    bool itemsWindow = false, statusWindow = false;
    // 3.0.11-beta.186 (the tester: «тут дублюючий інтерфейс»): [interface] item_toast - «+N …» of a thing picked up
    // over the bag (win/item_window drawToast), the game's own box at the top (its window 0x3A) hidden for it
    bool itemToast = true;
    // 3.0.11-beta.96 (the owner: «доделаем окна интерфейсов»): [interface] skills - «Навички» (core/skill_window) in
    // place of the game's own skill tree while it is open; on with the new minimap
    bool skillsWindow = false;
    // 3.0.11-beta.97: [interface] equip - «Спорядження» (core/equip_window) in place of the game's equipment window
    bool equipWindow = false;
    // 3.0.11-beta.98: [interface] npc_talk - the NPC talk (core/npc_talk) in place of the game's text and menu windows
    bool npcWindow = false;
    // 3.0.11-beta.255 (the NPC concept, question 6): [interface] npc_remember - the option chosen last time at the
    // same NPC's menu chosen again when it opens (each character's own, win/npc_talk)
    bool npcRemember = true;
    // 3.0.11-beta.273 (the owner's feature 1): [interface] portraits - the character's picture as it stands in the
    // game, taken now and then while it plays (win/portrait), on its character select's place
    bool portraits = true;
    // 3.0.11-beta.275 (the owner's feature 2): [interface] char_carousel - the character select's places round the
    // chosen one, two neighbours each side (core/char_select carouselLayout)
    bool charCarousel = true;
    // 3.0.11-beta.276 (the owner's feature 7): [interface] delay_ring - the delay after a skill (the server's, cut by
    // DEX) swept over the hotbar's skills (win/skill_delay_watch)
    bool delayRing = true;
    // 3.0.11-beta.277 (the owner's feature 8): [interface] death_recap - the last hits on the death screen
    bool deathRecap = true;
    // 3.0.11-beta.278 (the owner's features 6 and 10): [interface] hunt_panel - «Полювання»: DPS, exp/h, zeny/h, kills
    bool huntPanel = false;
    // 3.0.11-beta.279 (the owner's feature 5): [interface] mob_card - the monster's card by the cursor
    bool mobCard = false;  // 3.0.11-beta.294 (the owner: «выключить по умолчанию, с геймпада это не удобно»)
    // 3.0.11-beta.280 (the owner's feature 9): [interface] mvp_timers - the bosses seen dying and their return
    bool mvpTimers = true;
    // 3.0.11-beta.281 (the owner's feature 16): [interface] potion_warn - the hotbar's healing items running low
    bool potionWarn = true;
    // 3.0.11-beta.282 (the owner's feature 18): [interface] party_alarm - the healer's alarm on the party's panel
    bool partyAlarm = true;
    // 3.0.11-beta.283 (the owner's features 12 and 11): [interface] drop_names - the items on the ground named while
    // Alt is held; [interface] drop_highlight - the valuable ones (cards, gear, dear) always
    bool dropNames = true, dropHighlight = true;
    // 3.0.11-beta.288 (the owner's feature 20): [interface] layout_per_char - each character's own interface layout
    bool layoutPerChar = false;
    // 3.0.11-beta.290 (the owner's feature 22): [interface] homun_ai - our homunculus and mercenary AI written into
    // AI\USER_AI (/hoai, /merai); homun_aggressive, homun_defend, homun_skills - its choices
    bool homunAi = true, homunAggressive = false, homunDefend = true, homunSkills = true;
    // 3.0.11-beta.100: [interface] shop - the NPCs' shops (core/shop_window) in place of the game's deal windows
    bool shopWindow = false;
    // 3.0.11-beta.115: [interface] storage - the Kafra storage (win/storage_window: «Склад», «Речі» beside it) in
    // place of the game's storage window
    bool storageWindow = false;
    // 3.0.11-beta.116: [interface] trade - the trade (win/trade_window: «Обмін», «Речі» beside it) in place of the
    // game's trade window
    bool tradeWindow = false;
    // 3.0.11-beta.140 (the owner «давай»): [interface] menu - the game's menu (Esc) and its death screen
    // (win/menu_window, core/menu_window) in place of the game's windows 0x30, 0x13 and 0x88
    bool menuWindow = false;
    // 3.0.11-beta.149 (the owner: «сделай все без остановки»): [interface] cards - the game's item card and skill card
    // (win/info_card, core/info_card) in place of its windows 0xC and 0x2E
    bool cardWindows = false;
    // 3.0.11-beta.150: [interface] asks - a trade's, a party's, a guild's, a friend's request and an NPC's number or
    // words to type (win/ask_card, core/ask_card) in place of the game's windows 0x20, 0x23, 0x46, 0x6D, 0x38, 0x64
    bool askWindows = false;
    // 3.0.11-beta.151: [interface] messages - the game's message boxes and its waits (win/msg_box) in the new look
    bool messageBoxes = false;
    // 3.0.11-beta.151: [interface] login - the game's login window (win/login_card) in the new look
    bool loginWindow = false;
    // 3.0.11-beta.173: [interface] login_status - the server's status line under the login window (on)
    bool loginStatus = true;
    // 3.0.11-beta.174: [interface] login_news - the site's event or news now above the status line (on)
    bool loginNews = true;
    // 3.0.11-beta.177: [interface] login_view - the login screen's view: «Полиця» (shelf), «Класичний» (classic),
    // «Продовжити» (continue)
    loginview::View loginView = loginview::View::Shelf;
    // 3.0.11-beta.174: [utility] language - the driver's words' language ("uk", "ru", "en"; "" Windows'), at once
    std::string language;
    // 3.0.11-beta.152: [interface] char_select - the game's character select (win/char_select) in the new look
    bool charSelectWindow = false;
    // 3.0.11-beta.153: [interface] char_make - the game's character make (win/char_make) in the new look
    bool charMakeWindow = false;
    // 3.0.11-beta.154: [interface] cart - the merchant's cart (win/cart_window) as «Візок»
    bool cartWindow = false;
    // 3.0.11-beta.155: [interface] counts - the game's «how many» window (win/ask_card) as a count card
    bool countWindow = false;
    // 3.0.11-beta.156: [interface] vending - another's shop (win/vend_shop) as «Лавка»
    bool vendWindow = false;
    // 3.0.11-beta.157: [interface] picks - the magnifier's and a card's choice of things (win/pick_card)
    bool pickWindows = false;
    // 3.0.11-beta.158: [interface] party_setup - the party's settings (Alt+P, win/party_setup) as a card
    bool partySetupWindow = false;
    // 3.0.11-beta.159: [interface] pet - the pet's window (Alt+J, win/pet_card) as a card
    bool petWindow = false;
    // 3.0.11-beta.160: [interface] homun - the homunculus' window (Alt+R, win/homun_card) as a card
    bool homunWindow = false;
    // 3.0.11-beta.161: [interface] merc - the mercenary's window (Ctrl+R, win/merc_card) as a card
    bool mercWindow = false;
    // 3.0.11-beta.162: [interface] options - the game's options window (Alt+O, win/options_card) as a card
    bool optionsWindow = false;
    // 3.0.11-beta.163: [interface] social - the friends and party window (win/social_card) as a card
    bool socialWindow = false;
    // 3.0.11-beta.164: [interface] servers - the game's server select (win/server_card) as a card
    bool serverWindow = false;
    // 3.0.11-beta.165: [interface] guild - the guild's window (Alt+G, win/guild_card) as a card
    bool guildWindow = false;
    // 3.0.11-beta.166: [interface] my_shop - one's own open shop (win/my_shop) as a card
    bool myShopWindow = false;
    // 3.0.16-beta.33: [interface] vend_setup - opening one's shop (the game's vending windows 0x29 and 0x2A) as one
    // card (win/vend_setup); off at first
    bool vendSetupWindow = false;
    // 3.0.11-beta.168: [interface] whisper_setup - the whisper's settings (win/whisper_setup) as a card of switches
    bool whisperSetupWindow = false;
    // 3.0.11-beta.169: [interface] macros - the chat's macros (Alt+M, win/macro_card) as a card
    bool macroWindow = false;
    // 3.0.11-beta.204: [interface] tips - the game's «Tip of the Day» (win/tip_window) as a card
    bool tipWindow = false;
    // 3.0.18-beta.180: [interface] loading_bar - the game's map loading bar (win/loading_bar) in the new look
    bool loadingBar = false;
    // 3.0.11-beta.205: [interface] emotes - the game's emotion list (Alt+L, win/emote_window) as a card
    bool emoteWindow = false;
    // 3.0.11-beta.206: [interface] commands - the game's command list (Alt+Y, win/command_window) as a card
    bool commandWindow = false;
    // 3.0.11-beta.207: [interface] chat_rooms - the game's chat room windows (Alt+C, win/chatroom_window) as cards
    bool chatRoomWindows = false;
    // 3.0.11-beta.209: [interface] detail_console - Alt+D opens the driver's console in place of the game's «Arrange
    // Detail» window (win/detail_window)
    bool detailConsole = false;
    // 3.0.11-beta.211: [interface] popups - the game's popup menus (win/popup_window) as cards
    bool popupMenus = false;
    // 3.0.11-beta.212: [interface] whispers - the game's 1:1 whisper windows (win/whisper_window) as cards
    bool whisperWindows = false;
    // 3.0.11-beta.142 (the owner's concept «маг, прист и хант на геймпаде», answer 9 «дать»): [interface] skill_cells -
    // a ground skill's cells, as the server makes them (core/skill_area), shown on the ground after a cast by the pad
    // or the mouse (win/ground_marks); 0: none
    bool skillCells = true;
    // 3.0.16-beta.7 (the owner: «делай все» - ground cells): [interface] mouse_cell - the game's cell under the mouse
    // (grid.tga, uaRO.exe 0x4FAB0E) drawn anew as a ground skill's cell is (win/ground_marks); off: the game's own
    bool mouseCell = false;
    // 3.0.16-beta.7 (the owner: «делай все» - shop signs, chat bubbles): [interface] signs - the shops' and the chat
    // rooms' signs over the characters (the game's world windows 0x69673C, 0x6966A4) as the new look's plates
    // (win/world_signs); off: the game's own
    bool worldSigns = false;
    // 3.0.11-beta.143: [interface] ground_counts - on a ground skill's hotbar slot its walls standing («2/3»: Fire
    // Wall, Quagmire, Fire Pillar - core/ground_units) or a trap's Traps in the bag («×37»), its edge flashing amber
    // when a wall burnt out; 0: none
    bool groundCounts = true;
    // 3.0.11-beta.46: [interface] animations - the new interface's motion (core/ui_motion): the target's card, the
    // character block's views, a new level's flare, the low HP warning sliding in; 0: all at once
    bool animations = true;
    // 3.0.11-beta.47: [interface] panel_opacity and window_opacity - the new interface's grounds (core/ui_style): the
    // HUD's panels 50-100 % (80), the windows 80-100 % (90)
    int hudOpacity = 80, windowOpacity = 90;
    // 3.0.11-beta.48: [interface] cast - the character's cast over the hero in the new look (core/cast_bar), the
    // game's own gauge hidden while it shows
    bool castLabel = false;
    // 3.0.11-beta.50: [interface] low_hp_edge 0-100 - the screen's edges red at low HP, its strength (0: none)
    int lowHpEdge = 0;
    // 3.0.18-beta.7: [interface] uaui - the RmlUi spike (Claude Doc «Концепция: единая библиотека интерфейса»); since
    // 3.0.18-beta.11 the new interface's parts moved to uaUI drawn by it (win/uaui), its cost in the log; off unless
    // asked for; 3.0.18-beta.68 (the owner's stage 6: «сделай 1,2,3,4,5,6») on by default
    bool uaui = true;
    // 3.0.18-beta.11: [interface] uaui_showcase - uaUI's components in one window, for the developers (with uaui on)
    bool uauiShowcase = false;
    // 3.0.18-beta.64: [interface] uaui_glass (uaUI)
    bool uauiGlass = false;
    // 3.0.18-beta.61: [interface] uaui_effects (uaUI)
    bool uauiEffects = false;
    // 3.0.18-beta.58: [interface] uaui_motion (uaUI)
    bool uauiMotion = false;
    // 3.0.18-beta.13: [interface] theme = standard | classic | contrast - uaUI's parts' colours and corners
    // (core/ui_theme), uaro-graphics\ui\palette.txt over it
    std::string uiTheme = "standard";
    // 3.0.11-beta.54: [interface] size - the new interface's size against the concept's: 0 auto (smaller on a small
    // game, core/ui_style autoSize), else 50-150 per cent
    int uiSize = 0;
    // 3.0.11-beta.67: [interface] text_size - the concept's «Масштаб текста 80–150 %»: the windows one reads in (the
    // chat, «Довідник», «Речі», «Характеристики») larger or smaller as a whole, per cent of [interface] size's
    int textSize = 100;
    // 3.0.15-beta.9 (a player: «а можно якось зробити щоб окна складу і ГІ складу зменшити»): [interface]
    // item_windows - «Речі», the storage (the guild's too) and the cart smaller, per cent of the reading windows' size
    int itemWindows = 100;
    // 3.0.11-beta.7: [interface] reference - «Довідник» (win/reference_window): Ctrl+F in the game
    bool referenceWindow = true;  // 3.0.11 (the owner: the new interface for all) - «Довідник» with it
    PanelLayout panelLayout = PanelLayout::Row;
    int panelSize = 100, panelOpacity = 90;
    std::string panelPlace;
    NotifySettings notify;      // 3.0.4-beta.3
    // 3.0.4-beta.5, [camera] indoor: building maps (data\indoorrswtable.txt) - the camera turns, tilts and zooms out
    // as outdoors (win/game_camera keepCameraFree)
    bool cameraIndoor = true;
    // 3.0.4-beta.17, [camera] zoom_limit: the camera out no further than the character stays this side of the game's
    // far plane (win/game_camera limitZoom, camera::kZoomLimit; R-35N5QN)
    bool cameraZoomLimit = true;
    // 3.0.11-beta.184, [camera] remember: the camera's zoom and tilt as the player left them, outdoors and in buildings
    // apart, kept between the games in uaro-graphics\camera.txt (win/game_camera rememberCamera; R-ANMTEJ)
    bool cameraRemember = true;
    // 3.0.13-beta.5 (the owner's pick 15 of «50 идей»): [camera] tilt - game, lower or ground (core/game_camera
    // tiltRoom)
    std::string cameraTilt = "game";
    bool buffTimers = true;  // 3.0.4-beta.6, [buffs] timers: the time left on the buff icons (win/buff_overlay)
    bool clientUpdates = true;  // 3.0.2-beta.5: [client] updates - the GRFs in updates\ over the game's archives
    // 2.12: [renderer] safe_mode - Direct3D 11 without the risky parts (core/safe_mode), asked by the player
    bool safeMode = false;
    // 2.15: [utility] mode = developer - the settings app's Developer mode: uaRO Studio's keys work in the game
    bool developer = false;
    bool bridgeDpiScale = true;
    // Part 4 (test): [renderer] remix = 1 draws through Direct3D 9's fixed function, with the world in world space:
    // RTX Remix when its d3d9.dll is next to uaRO.exe, else the system's Direct3D 9.
    bool remix = false;
    DisplayMode mode = DisplayMode::Auto;
    int monitor = 0;       // 0 = the main monitor
    int windowWidth = 0;   // window mode: inner size in pixels; 0 = the size the game asks for
    int windowHeight = 0;
    bool highResolution = true;  // draw at the window's real pixels under Windows scaling (stage 3); false = 0.2
    // 3.0.11-beta.292: the game's window made per-monitor DPI aware (its creation only; the game's thread stays
    // unaware): a whole pixel window, no DWM stretch at 175 % (R-PSFZ8K 3839x2159). After the game's restart
    bool dpiAware = false;
    UiFilter uiFilter = UiFilter::Sharp;
    bool crispText = true;       // window text and lines at the window's real pixels when k > 1 (stage 6)
    // 3.0.18-beta.178: the damage digits, Miss, Guard, Critical!!, Lucky and the crits' stars from sharp copies of
    // the game's own frames (core/sharp_frames.hpp) - as the game has them, xBR four times larger
    bool crispNumbers = true;
    WindowArt windowArt = WindowArt::Xbr;  // 0.41
    int renderScale = 100;       // %, 100-200: the frame is drawn this much larger and shrunk when shown
    int sharpen = 50;            // 0-100: contrast adaptive sharpening of the picture on screen; 0 = off
    int msaa = 4;                // stage 4: samples per pixel at polygon edges - 0 (off), 2, 4 or 8
    bool alphaToCoverage = true; // stage 4, with MSAA: soft edges where alpha testing cuts textures (leaves, fences)
    int anisotropy = 16;         // stage 4: anisotropic filtering of the world's textures - 0 (off), 2, 4, 8 or 16
    AiSettings ai;
    // 2.2.1: [ai] as the file has it now - ai stays as the game started (the networks and the pack start with it);
    // what is shown follows this one at once (aiKindsShown): a kind off is hidden in a running game
    AiSettings aiNow;
    EffectSettings effects;
    ConnectionSettings connection;  // 2.6.17
    // Part 3: [quality] dlss = off, dlaa, quality, balanced, performance, ultra or (2.4.2) custom - NVIDIA DLSS in the
    // 64-bit renderer (it starts that renderer; MSAA is off then).
    std::string dlss = "off";
    // 2.4.2: dlss = custom - DLSS at the player's own size (core/dlss_size.hpp): dlss_scale % of the screen (33-100),
    // or dlss_width x dlss_height when written (0 = by the share; one of them alone: the screen's proportions);
    // dlss_model - its network: auto (NVIDIA's for the mode), k, j, l or m (Streamline's presets), any mode.
    int dlssScale = 75;
    int dlssWidth = 0;
    int dlssHeight = 0;
    std::string dlssModel = "auto";
    // 0.75: [quality] taa - the renderer's own temporal anti-aliasing (Direct3D 12, any card; DLSS goes first)
    bool taa = false;
    // Test switches (not on a page): dlss_jitter, dlss_motion = normal or inverted - the sign conventions DLSS
    // expects, should the picture come out blurred (jitter) or smeared (motion).
    bool dlssJitterInverted = false;
    bool dlssMotionInverted = false;
    bool vsync = true;
    // 0 = no limit; 0.85: 60 by default (was none: 150 frames of a game that looks the same at 60)
    int fpsLimit = 60;
    bool savePower = true;  // 0.85: [frames] save_power - 15 frames a second in the background, 30 with no map
    // 2.5.20: [frames] battery_saver - while a laptop runs on its battery: battery_fps frames at most, the effects at
    // most the weak set's, the world at 75 % (core/light_mode.hpp)
    bool batterySaver = false;
    int batteryFps = 30;
    // 0.76: [frames] keep_fps - below this for ten seconds, the costliest effect goes off for the rest of the game
    // (core/effect_costs.hpp); 0 = never
    int keepFps = 0;
    // 3.0.18-beta.186 (the owner: «делай 2 без остановки»): [frames] light_skills - the game's own light skill effects
    // (its /mineffect's) while the frames run short of keep_fps in a crowd of skills (core/light_skills)
    bool lightSkills = true;
    bool fpsCounter = false;
    // 2.5.16: [quality] world_scale - Direct3D 11: the world drawn at this share (50-100 %) of the picture's size and
    // made larger with sharpening before the interface, which stays sharp (an HD 6570: the world was the frame)
    int worldScale = 100;
    // 2.5.18: [quality] full_colour - the textures in their pictures' own colours, not the 16-bit ones the game makes
    // (core/full_colour.hpp)
    bool fullColour = true;
    // 2.5.20: [quality] super_light - the weakest cards' mode (core/light_mode.hpp): no effect, no AI picture, the
    // world at half size, its textures as their colour alone, 16-bit, in a running game and given back when it ends
    bool superLight = false;
    // 0.62: [display] ui_native - with render_scale over 100 % on Direct3D 12 (no DLSS): the world drawn larger and
    // made smaller, the interface drawn at the screen's own pixels (not drawn larger and made smaller with it)
    bool uiNative = false;
    // 0.65: [display] hdr - Direct3D 12 on a monitor with Windows' HDR on: an HDR10 picture, the brightest a little
    // over white
    bool hdr = false;  // 0.54: [frames] fps_counter - the frame counter at the start (win/fps_overlay.hpp)
    int frameGeneration = 0;  // part 3: [frames] frame_generation = off, 2x, 3x, 4x - DLSS frame generation (DLSS on)
    // 2.5.26: [frames] reflex - NVIDIA Reflex with DLSS (the 64-bit renderer): each frame held back until the card is
    // about to be free and the game's next one started only then, so that it takes the mouse late; frame generation
    // turns it on anyway
    bool reflex = true;
    bool allowTearing = false;  // let G-SYNC / FreeSync monitors show frames as soon as they are ready
    LogLevel logLevel = LogLevel::Info;
    bool profile = false;  // 0.87: [debug] profile - where the game's thread spends its time, into the log
    // 2.3: [debug] xray - uaRO Studio's X-ray (Ctrl+Shift+F9 too): the textures tinted by how they are shown
    bool xray = false;
    // 2.7.8: [debug] seams_view - the ground seams' strips coloured (1) or coloured over everything (2): what blinks
    // on the hills; 0 the picture as usual
    int seamsView = 0;
    // 3.0.4-beta.35: [debug] weak_card - the card taken as weak by its memory, to try the judge of its speed
    // (core/card_speed.hpp) on a strong one
    bool weakCard = false;
    // 0.88: [debug] state_cache - Direct3D 11 calls only for what changed between the game's draws; 0 calls everything
    // each draw as before (should a picture come out wrong with it)
    bool stateCache = true;
    // 0.89: [debug] fast_pick - the cell under the mouse found along the mouse's ray (win/pick_hook.hpp); 0 leaves the
    // game's own, which tests every cell of the map's pieces the ray passes over
    bool fastPick = true;
    bool fastFtol = true;  // 2.5.20: [debug] fast_ftol - the game's __ftol by fisttp (win/ftol_patch.hpp)
    bool fastTextures = true;  // 2.5.20: [debug] fast_textures - CTexture::Update's rows by SSE2 (texture_hook)
    // 3.0.11-beta.128 (the network plan's step 5): [debug] quick_acks - the game's socket acknowledges each of the
    // server's packets at once (SIO_TCP_SET_ACK_FREQUENCY 1, win/net_watch); off: Windows' delayed ACKs (2)
    bool quickAcks = true;
    // 0.90: [debug] merge_draws - Direct3D 11 sends the game's draws that follow one another with the same state as
    // one draw call (core/draw_batch.hpp); 0 sends each by itself as before
    bool mergeDraws = true;
    // 2.5.16: [debug] shader_variants - Direct3D 11 draws each state of the game's draws with a shader made for it
    // (core/shader_variants.hpp); 0 draws all with the one shader for every state, as before
    bool shaderVariants = true;
    // 3.0.17-beta.7: [debug] record_scene - the driver's messages to the renderer recorded (core/scene_record.hpp)
    bool recordScene = false;
    SoundSettings sound;  // 0.91
};

Config parseConfig(const std::string& text);
// 2.2.1: the kinds of AI pictures shown (bits as ai::Kind: 0 ground, 1 objects, 2 sprites, 3 effects, 4 icons); 0 none
uint8_t aiKindsShown(const AiSettings& a);
// 2.2.2: the sprites' kinds shown, a bit each by sprite_exe::Source (1 monsters, 2 players, 3 NPCs, 4 items); 0 none
uint8_t aiSpritesShown(const AiSettings& a);
// 2.2.2: whether a cell of the game's sprite cache holding `source` (sprite_exe::Source) is drawn from its AI copy -
// monsters, players, NPCs and items by their own bits (`sprites`, aiSpritesShown), an effect's frame by the effects'
// and the cursor by the icons' (`kinds`, aiKindsShown), a frame of no known file while any sprite kind is shown.
bool aiCellShown(uint8_t source, uint8_t kinds, uint8_t sprites);
// 2.2.1 (the console's ↻), 2.2.2 in the core: whether an [ai] kind's switch ("ai.ground", "ai.monsters"...) changed
// from `started` (the game's start) to `now` waits for the next start - a kind turned on that the game started
// without (its pictures were never read). Every other change of a kind shows at once (2.2.2: a kind of sprites alone
// too - the cells of the sprite cache that hold it).
bool aiWaits(const std::string& key, const AiSettings& started, const AiSettings& now);
const char* apiToken(Api a);
const char* displayModeToken(DisplayMode m);
const char* uiFilterToken(UiFilter f);
const char* windowKeysToken(WindowKeys k);  // 3.0.3-beta.6
const char* wayOnGroundToken(WayOnGround w);  // 3.0.11-beta.33
const char* mapStyleToken(MapStyle m);         // 3.0.11-beta.120
const char* targetRingToken(TargetRing r);    // 3.0.11-beta.87
const char* mobHpToken(MobHp m);              // 3.0.11-beta.95
const char* padWalkToken(PadWalk w);          // 3.0.11-beta.88
const char* walkLookToken(WalkLook w);        // 3.0.11-beta.103
const char* cursorLookToken(CursorLook l);    // 3.0.11-beta.198
const char* buffsLookToken(BuffsLook b);       // 3.0.11-beta.35
const char* buffsOverflowToken(BuffsOverflow b);  // 3.0.11-beta.63
const char* characterLookToken(CharacterLook c);  // 3.0.11-beta.36
const char* panelLayoutToken(PanelLayout l);  // 3.0.3-beta.7
const char* windowArtToken(WindowArt a);

}  // namespace uagfx

#pragma once
#include <array>
#include <string>

#include "core/ai/groups.hpp"
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
    // 0.28: the maps' fireflies - those the game shows and those the maps' makers placed that it does not - glow,
    // flash now and then and light a little around them. On by default.
    bool fireflies = true;
    int firefliesBrightness = 100;  // %, 0-200
    // 0.29: the maps' hidden lights - glows, pools of light, the towers' dust the maps' makers placed and the game
    // never shows - drawn from the map's file. On by default.
    bool mapGlows = true;
    int mapGlowsBrightness = 100;  // %, 0-200
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
    bool stereoFlip = false;  // stereo_flip (not on the page): left and right the other way round
    bool steal = true;      // steal: all samples busy - the quietest stopped for a new sound
    bool crossfade = true;  // crossfade: the music's pieces change over 1.5 s
    bool engine = false;    // 1.1, engine: the sounds at a place by the driver's engine (XAudio2): around the player
    int reverb = 60;        // reverb: the place's reverberation with the engine, 0-100
    bool headphones = true;  // 1.2, headphones: Windows' spatial sound (Sonic, Atmos) when it is on in Windows
    bool duck = true;        // 1.3, duck: the music quieter (to 65 %) while many sounds play at once
    // 3.0.4-beta.1, output: the Windows output device it plays on (its endpoint id; "" - auto: as in Windows;
    // core/audio_output.hpp)
    std::string output;
};

// 3.0.4-beta.3, [notify] (core/notify.hpp): a game window behind others tells of these - on, then each kind
struct NotifySettings {
    bool on = true;
    bool whisper = true, party = true, trade = true, guild = true, friendAsk = true, death = true;
};

struct Config {
    Api api = Api::D3d12;
    // Part 3 (test): [renderer] bridge = 1 draws in the 64-bit process uaro-render64.exe; bridge_present = auto,
    // window or composition (how its picture reaches the game's window); bridge_dpi_scale = 1 scales a composition
    // surface for Windows scaling of the game's window.
    bool bridge = false;
    std::string bridgePresent = "auto";
    // 2.5.12 (test): [renderer] render_thread = 1 - Direct3D 11 draws on a thread of its own in the game's process
    // (gpu/local_host.cpp): the game's thread only writes the frame down, as for uaro-render64.exe
    bool renderThread = false;
    // 0.50.2: [renderer] gpu - the name of the graphics card to draw on (Windows' own, as the Renderer page lists
    // them); "" (auto): the fast one of several (core/gpu_order.hpp).
    std::string gpu;
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
    // 3.0.3-beta.6: switch_keys - the keys that switch the game windows (core/game_windows)
    WindowKeys windowKeys = WindowKeys::All;
    // 3.0.3-beta.8: [panel] - the game windows' panel in the game's picture (core/window_panel, win/window_panel):
    // show; layout - its cards in a row or a column; size - % over the game's interface scale (75-150); opacity (40-100
    // %); hidden (Ctrl+Alt+0, its cross) and place ("x y", core/window_panel's Place; "" its first place) written by
    // the driver
    bool panelShow = true, panelHidden = false;
    PanelLayout panelLayout = PanelLayout::Row;
    int panelSize = 100, panelOpacity = 90;
    std::string panelPlace;
    NotifySettings notify;      // 3.0.4-beta.3
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
    UiFilter uiFilter = UiFilter::Sharp;
    bool crispText = true;       // window text and lines at the window's real pixels when k > 1 (stage 6)
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
    // 0.88: [debug] state_cache - Direct3D 11 calls only for what changed between the game's draws; 0 calls everything
    // each draw as before (should a picture come out wrong with it)
    bool stateCache = true;
    // 0.89: [debug] fast_pick - the cell under the mouse found along the mouse's ray (win/pick_hook.hpp); 0 leaves the
    // game's own, which tests every cell of the map's pieces the ray passes over
    bool fastPick = true;
    bool fastFtol = true;  // 2.5.20: [debug] fast_ftol - the game's __ftol by fisttp (win/ftol_patch.hpp)
    bool fastTextures = true;  // 2.5.20: [debug] fast_textures - CTexture::Update's rows by SSE2 (texture_hook)
    // 0.90: [debug] merge_draws - Direct3D 11 sends the game's draws that follow one another with the same state as
    // one draw call (core/draw_batch.hpp); 0 sends each by itself as before
    bool mergeDraws = true;
    // 2.5.16: [debug] shader_variants - Direct3D 11 draws each state of the game's draws with a shader made for it
    // (core/shader_variants.hpp); 0 draws all with the one shader for every state, as before
    bool shaderVariants = true;
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
const char* panelLayoutToken(PanelLayout l);  // 3.0.3-beta.7
const char* windowArtToken(WindowArt a);

}  // namespace uagfx

#include "core/ini_upgrade.hpp"

#include <cctype>
#include <vector>

namespace uagfx {
namespace {

// A block of keys and their notes, added when the file lacks its marker (install.bat 2.9: findstr /c: is
// case-sensitive, findstr /i /l is not).
struct Block {
    const char* marker;
    bool anyCase;
    std::vector<const char*> lines;
};

const Block kSharpen = {"sharpen", false, {
    "[quality]",
    "; sharpening of the picture on screen, 0-100: 0 = off, more = crisper detail",
    "sharpen = 50",
    "; in the game, Pause or Scroll Lock switches all improvements off and on, to compare",
}};

const Block kCrispText = {"crisp_text", false, {
    "[display]",
    "; 1 = the game's text, window frames and fills at the real pixels of the window when the game is enlarged; "
    "0 = as version 0.4",
    "crisp_text = 1",
}};

// 0.18-0.39: the effects pages' keys, each block with the values that version showed. 2.10: the blocks of
// the removed depth of field, clouds and haze (dof_strength, clouds_strength, haze_strength) are gone.
const std::vector<Block> kEffects = {
    {"[effects]", true, {
        "[effects]",
        "; modern effects over the game's picture with the uaro renderer, also in a running game",
        "; 1 = the light of skills, portals and lamps shines with a soft glow - bloom;",
        ";     the rest of the picture keeps its brightness",
        "glow = 0",
        "; how bright the glow is, 0-200 %; how far it spreads, 0-100 %",
        "glow_strength = 100",
        "glow_size = 50",
        "; 0-100 %: how much the effects' light falls on what is around them",
        "glow_light = 50",
        "; 1 = soft shadows where surfaces meet: at walls, in corners, under trees",
        "ao = 0",
        "; 0-100 %: how dark the shadows are, how far they reach from a corner",
        "ao_strength = 60",
        "ao_size = 50",
        "; 1 = the maps' water drawn anew: waves, reflections, the sun's glints,",
        ";     the bed seen through it, foam",
        "water = 0",
        "; 0-100 %: how rough it is - 0 = a still mirror, how much it reflects,",
        ";     how deep one sees into it",
        "water_waves = 50",
        "water_reflections = 50",
        "water_clarity = 50",
        "; 1 = the picture's colours to taste: saturation 0-200 %, contrast and brightness",
        ";     50-150 % - 100 as the game, warmth 0-100 % - 50 as the game, vignette 0-100 %",
        "grade = 0",
        "grade_saturation = 115",
        "grade_contrast = 108",
        "grade_brightness = 100",
        "grade_warmth = 50",
        "grade_vignette = 25",
        "; 1 = depth of field: sharp around your character, the farther and the nearer softer;",
        ";     0-100 %: how soft, how deep the sharp zone is",
        "dof = 0",
        "dof_strength = 60",
        "dof_range = 40",
    }},
    {"water_waves", true, {
        "[effects]",
        "; 1 = the maps' water drawn anew: waves, reflections, the sun's glints,",
        ";     the bed seen through it, foam",
        "water = 0",
        "; 0-100 %: how rough it is - 0 = a still mirror, how much it reflects,",
        ";     how deep one sees into it",
        "water_waves = 50",
        "water_reflections = 50",
        "water_clarity = 50",
    }},
    {"ao_strength", true, {
        "[effects]",
        "; 1 = soft shadows where surfaces meet: at walls, in corners, under trees",
        "ao = 0",
        "; 0-100 %: how dark the shadows are, how far they reach from a corner",
        "ao_strength = 60",
        "ao_size = 50",
        "; 0-100 %: how much the effects' light falls on what is around them",
        "glow_light = 50",
    }},
    {"grade_saturation", true, {
        "[effects]",
        "; 1 = the picture's colours to taste: saturation 0-200 %, contrast and brightness",
        ";     50-150 % - 100 as the game, warmth 0-100 % - 50 as the game, vignette 0-100 %",
        "grade = 0",
        "grade_saturation = 115",
        "grade_contrast = 108",
        "grade_brightness = 100",
        "grade_warmth = 50",
        "grade_vignette = 25",
    }},
    {"smooth_light", true, {
        "[effects]",
        "; 1 = the map's baked light - torch pools, shadows on the ground -",
        ";     smooth instead of blotches",
        "smooth_light = 1",
    }},
    {"torch_glow", true, {
        "[effects]",
        "; 1 = the maps' torches drawn anew: a burning flame with sparks, a soft glow around it,",
        ";     warm flickering light on the ground and the walls",
        "torch = 1",
        "; 0-200 %: the glow around the fire, the light it throws around; 0-100 %: its flicker",
        "torch_glow = 100",
        "torch_light = 100",
        "torch_flicker = 50",
    }},
    {"torch_brightness", true, {
        "[effects]",
        "; 1 = the torches' new flame, 0 = the game's own flame with the new glow and light",
        "torch_flame = 1",
        "; 0-200 %: how bright the new flame burns",
        "torch_brightness = 100",
    }},
    {"smoke_density", true, {
        "[effects]",
        "; 1 = the chimneys' smoke billowing - wisps and gaps that rise - instead of one picture",
        "smoke = 1",
        "; 0-200 %: how thick the smoke is",
        "smoke_density = 100",
    }},
    {"lamps_light", true, {
        "[effects]",
        "; 1 = the maps' lamps light the characters and walls too, the fire-coloured ones flicker",
        "lamps = 1",
        "; 0-200 %: how bright the lamps' light is",
        "lamps_light = 60",
    }},
    {"fire_light_strength", true, {
        "[effects]",
        "; 1 = fire walls, fire bolts and other fire-coloured skills light around them a little",
        "fire_light = 1",
        "; 0-200 %: how much",
        "fire_light_strength = 100",
    }},
    {"skill_colours", true, {
        "[effects]",
        "; 1 = the other skills light too, each in its colour: ice blue, lightning white, holy gold",
        "skill_colours = 1",
    }},
    {"aura_brightness", true, {
        "[effects]",
        "; 1 = the level 99 aura - its ring, bubbles and circle - shines softer",
        "aura = 1",
        "; 0-100 %: how bright the aura is, of what the game draws",
        "aura_brightness = 50",
    }},
    {"skill_brightness", true, {
        "[effects]",
        "; 1 = skills and the other effects of the game shine a little softer",
        "skills_soft = 1",
        "; 0-100 %: how bright, of what the game draws",
        "skill_brightness = 80",
    }},
    {"fireflies_brightness", true, {
        "[effects]",
        "; 1 = the maps' fireflies glow, flash now and then and light a little around them",
        "fireflies = 1",
        "; 0-200 %: how bright the fireflies and their light are",
        "fireflies_brightness = 100",
    }},
    {"map_glows_brightness", true, {
        "[effects]",
        "; 1 = glows, pools of light and dust the maps' makers placed but the game never showed",
        "map_glows = 1",
        "; 0-200 %: how bright they and their light are",
        "map_glows_brightness = 100",
    }},
    {"rays_brightness", true, {
        "[effects]",
        "; 1 = the forest's beams of light soft, shimmering, with dust and light where they fall",
        "rays = 1",
        "; 0-200 %: how bright the beams and their light are",
        "rays_brightness = 100",
    }},
    {"lava_brightness", true, {
        "[effects]",
        "; 1 = lava flows: its crust drifts, its cracks glow, its heat lights what is around it",
        "lava = 1",
        "; 0-200 %: how brightly the lava glows and lights around it",
        "lava_brightness = 100",
    }},
    {"portals_strength", true, {
        "[effects]",
        "; 1 = the warps and the skill's portals glow around in soft blue, the air turns in them",
        "portals = 1",
        "; 0-200 %: how bright the glow, how strong the swirl",
        "portals_strength = 100",
    }},
    {"actor_shadows_strength", true, {
        "[effects]",
        "; 1 = characters and monsters cast soft shadows the shape of their sprites",
        "actor_shadows = 1",
        "; 0-100 %: how dark",
        "actor_shadows_strength = 55",
    }},
    {"heat_strength", true, {
        "[effects]",
        "; 1 = the picture shimmers over flames, burning skills, lava and far off in the desert",
        "heat = 1",
        "; 0-200 %: how much",
        "heat_strength = 100",
    }},
    {"weather_amount", true, {
        "[effects]",
        "; 1 = snow over Lutie, leaves in Payon, sand about Morroc, rain over Niflheim and Einbroch",
        "weather = 1",
        "; 0-200 %: how much of it",
        "weather_amount = 100",
    }},
};

std::string lower(std::string s) {
    for (char& c : s) c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool has(const std::string& text, const Block& b) {
    return b.anyCase ? lower(text).find(lower(b.marker)) != std::string::npos
                     : text.find(b.marker) != std::string::npos;
}

// As `>>file echo.` and `>>file echo ...`: an empty line (it ends a last line without one), then the lines.
void add(std::string& text, const std::vector<const char*>& lines) {
    text += "\r\n";
    for (const char* line : lines) text += std::string(line) + "\r\n";
}

std::vector<std::string> linesOf(const std::string& text) {  // without their CR and LF
    std::vector<std::string> out;
    for (size_t at = 0; at < text.size();) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(at, end - at);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        out.push_back(line);
        at = end + 1;
    }
    return out;
}

// A line "  key = 0" then its end or no digit (install.bat's findstr /i /r "^ *key *= *0 *$" "^ *key *= *0[^0-9]").
bool isZero(const std::string& text, const std::string& key) {
    for (const std::string& line : linesOf(text)) {
        const std::string l = lower(line);
        size_t i = l.find_first_not_of(' ');
        if (i == std::string::npos || l.compare(i, key.size(), key) != 0) continue;
        i = l.find_first_not_of(' ', i + key.size());
        if (i == std::string::npos || l[i] != '=') continue;
        i = l.find_first_not_of(' ', i + 1);
        if (i == std::string::npos || l[i] != '0') continue;
        if (i + 1 == l.size() || !std::isdigit(static_cast<unsigned char>(l[i + 1]))) return true;
    }
    return false;
}

// 0.7: the smoothing as the preset the file holds has it - Speed (no sharpening, no crisp text) and No
// improvements (also no high resolution) get less, so an update never makes a weak card work harder.
void addSmoothing(std::string& text) {
    if (text.find("msaa") != std::string::npos) return;
    std::string msaa = "4", a2c = "1", aniso = "16";
    if (isZero(text, "sharpen") && isZero(text, "crisp_text")) {
        msaa = "0";
        aniso = "4";
        if (isZero(text, "high_resolution")) {
            a2c = "0";
            aniso = "0";
        }
    }
    text += "\r\n";
    text += "[quality]\r\n";
    text += "; edge smoothing (MSAA): samples per pixel - 0 = off, 2, 4 or 8\r\n";
    text += "msaa = " + msaa + "\r\n";
    text += "; 1 = with msaa, soft edges where textures are cut out (leaves, fences)\r\n";
    text += "alpha_to_coverage = " + a2c + "\r\n";
    text += "; anisotropic filtering of the world: 0 = off, 2, 4, 8 or 16\r\n";
    text += "anisotropy = " + aniso + "\r\n";
}

// 0.17: the AI textures take textures of 16 pixels and more, not 64 - the file 0.12-0.16 put in place says
// min_size = 64 under that version's note: both change, once.
void textures017(std::string& text) {
    const std::string note = "; the textures it takes, by their sides in pixels: from min_size to max_size, 16-1024";
    const std::vector<std::string> lines = linesOf(text);
    bool noted = false, old = false;
    for (const std::string& l : lines) {
        noted = noted || l == note;
        old = old || l == "min_size = 64";
    }
    if (!noted || !old) return;
    std::string out;
    for (size_t at = 0; at < text.size();) {
        size_t end = text.find('\n', at);
        end = end == std::string::npos ? text.size() : end + 1;
        std::string line = text.substr(at, end - at);
        const size_t body = line.find_first_of("\r\n");
        const std::string bare = line.substr(0, body), tail = body == std::string::npos ? "" : line.substr(body);
        if (bare == note)
            line = "; the textures of the world it takes, by their sides in pixels (16 since 0.17)" + tail;
        if (bare == "min_size = 64") line = "min_size = 16" + tail;
        out += line;
        at = end;
    }
    text = out;
}

}  // namespace

std::string upgradeIni(const std::string& original) {
    std::string text = original;
    for (const Block* b : {&kSharpen, &kCrispText})
        if (!has(text, *b)) add(text, b->lines);
    addSmoothing(text);
    textures017(text);
    for (const Block& b : kEffects)
        if (!has(text, b)) add(text, b.lines);
    return text;
}

}  // namespace uagfx

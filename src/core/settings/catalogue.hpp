#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/config.hpp"
#include "core/settings/strings.hpp"

namespace uagfx::settings::catalogue {

// 3.0.21-beta.1 (the concept «Настройки uaRO Graphics», Claude Doc JzcE4xZDus82grUxsQjgy7; the owner: «Вариант 4.
// Каждому месту своё дело»): the one catalogue of uaro-graphics.ini's rows. The settings app, the game's console and
// its «Налаштування» (Alt+O) each kept a list of their own, made by hand: a new switch was edits in 8 files, the three
// never agreed, and a trial looked like a setting for good. Here a row is said once - its area and group (one address
// in every place), its level, its status and what it needs to do anything; where it lives follows from its nature
// (homeOf). A new row of the schema with no line here fails the tests (test_catalogue).
enum class Area : uint8_t { Picture, View3d, Interface, Controls, Sound, Speed, Game, Textures, Diagnostics };
constexpr int kAreaCount = 9;
Text areaName(Area a);

// Basic - what many change (seven a group at most); Detail - an effect's parts, the sliders, each window's own
// switch; Expert - the technique
enum class Level : uint8_t { Basic, Detail, Expert };
// A trial lives in the console's laboratory until a release decides it: kept as a setting, built in or removed
enum class Status : uint8_t { Permanent, Trial, Diagnostic };
// Game - applies at once: the game's «Налаштування» (Alt+O); Start - waits for the game's start: the settings app;
// Lab - a trial or a diagnostic: the console; File - only in uaro-graphics.ini (the effects hidden behind a choice)
enum class Home : uint8_t { Game, Start, Lab, File };

// What a row needs to do anything: another row on (a check on, a list not at its first word), Direct3D 12, the 3D view
enum class NeedKind : uint8_t { Row, Direct3D12, View3d };
struct Need {
    NeedKind kind;
    const char* key = nullptr;  // Row: "effects.grass"
};

struct Row {
    std::string key;                     // "interface.minimap"
    Level level = Level::Basic;
    Status status = Status::Permanent;
    const char* since = "";              // a trial: the version it came with ("3.0.20-beta.73"), "" not known
    std::string under;                   // the row it is a part of (shown under it), "" none
    std::vector<Need> needs;
    bool nextMap = false;                // a change shows from the next map
    bool atStart = false;                // read before the game starts though the driver takes it at once
};
struct Group {
    Area area;
    Text name;
    std::vector<Row> rows;
};
// The groups in the order every place shows them: an area's one after another
const std::vector<Group>& groups();
const Row* find(const std::string& sectionKey);      // none: not a row of the catalogue
const Group* groupOf(const std::string& sectionKey);
Home homeOf(const Row& r);
// A need met by the settings as they are shown (an effects' set decides its switches), the renderer and the view
bool met(const Need& n, const Config& shown, bool direct3d12, bool view3d);

}  // namespace uagfx::settings::catalogue

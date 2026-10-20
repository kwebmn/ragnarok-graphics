#pragma once
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "app/registry.hpp"
#include "app/system.hpp"
#include "core/config.hpp"
#include "core/settings/game_registry.hpp"
#include "core/settings/game_settings.hpp"
#include "core/settings/input_module.hpp"
#include "core/settings/auto_setup.hpp"
#include "core/settings/log_digest.hpp"
#include "core/settings/navigation.hpp"
#include "core/settings/presets.hpp"
#include "core/settings/schema.hpp"
#include "core/settings/screen.hpp"
#include "core/settings/strings.hpp"
#include "core/settings/window_place.hpp"

namespace uagfx::app {

struct Paths {
    std::wstring dir;  // the client folder
    std::wstring ini, log, game, version, backups, report, ddraw;
};

// A dinput DLL in the client folder (stage 2 spec §2.3).
struct ModuleFile {
    std::wstring file;
    bool loaded = false;  // the game loads it
    settings::ModuleInfo info;
    std::wstring iniPath;
    std::string iniText;
    bool iniExists = false;
};

struct CompatFlag {
    bool allUsers = false;
    std::string flag;
};

enum class Ddraw { Ours, Other, None };

// Everything the pages show, read from the client folder and the registry.
struct Snapshot {
    std::string iniText;
    bool iniExists = false;
    Config config;
    settings::GameRegistry registry;
    std::vector<ModuleFile> modules;
    settings::RoextWindow roext;  // of the ROExt the game loads; inactive when there is none
    int roextModule = -1;         // its index in modules
    std::vector<CompatFlag> compat;
    Ddraw ddraw = Ddraw::None;
    std::string installedVersion;  // uaro-graphics\version.txt
    bool gameRunning = false;
    std::vector<std::wstring> backups;  // folder names, newest first
    settings::LogDigest log;
    bool folderWritable = true;
    bool programFiles = false;
};

// The settings app's state: what is on disk, what the player changed and how it is written (spec §4).
class Model {
public:
    Model();
    void load();             // everything, again
    bool refreshRunning();   // whether the game runs; true when that changed
    const Snapshot& now() const { return now_; }
    const SystemInfo& system() const { return system_; }
    // 2.2: the card's level for the automatic set, as the driver finds it (effect_groups::cardLevel)
    int cardLevel() const;
    const Paths& paths() const { return paths_; }

    // uaro-graphics.ini keys: the player's change, else the file's value.
    std::string token(const settings::IniSetting& s) const;
    void setToken(const settings::IniSetting& s, const std::string& token);
    Config pendingConfig() const;   // the file's settings with the player's changes

    settings::ScreenChoice screenChoice() const;  // the player's, else what the game gets now
    void setScreen(const settings::ScreenChoice& c);
    settings::ScreenNow screenNow() const;
    settings::ScreenPlan screenPlan() const;
    // The plan needs ROExt's ini, written only while the game is closed (spec §4.1): the Screen page greys what changes
    // the window. The full screen never needs it (0.6.1 minor 6).
    bool screenWaits() const;

    // The game's key on the pages (0.6.2): the player's word, else what the game reads now (game_settings).
    std::string regWord(const settings::RegSetting& s) const;
    void setRegWord(const settings::RegSetting& s, const std::string& word);
    // Whether a page may change it now: what the game writes itself waits for it to close (spec §4.1).
    bool regOpen(const settings::RegSetting& s) const { return !s.gameWrites || !now_.gameRunning; }
    std::vector<std::wstring> skins() const;  // the folders of skin\ next to uaRO.exe (spec §6.1)
    bool hasIntro() const;                     // openning.bik next to uaRO.exe

    void applyPreset(settings::Preset p);
    std::optional<settings::Preset> preset() const;  // the one the fields hold now, with the game's quality (spec §8)
    size_t pendingCount() const;  // the player's changes not written yet
    // Apply has something to write: the player's changes, or the game's key on a first run (spec §4.3).
    bool canApply() const { return pendingCount() > 0 || !settings::gameKeyExists(now_.registry); }
    bool canRevert() const { return pendingCount() > 0 || !wroteNow_.empty(); }

    // Writing (spec §4). Each returns "" or what went wrong, in the language given.
    std::wstring apply(settings::Lang l);
    // While the game runs, one key of uaro-graphics.ini now (a slider let go, spec §6); "Undo" brings it back.
    std::wstring writeNow(const settings::IniSetting& s, const std::string& token, settings::Lang l);
    // 2.2: several keys in one write (a set made the player's own: every switch at once)
    std::wstring writeNow(const std::vector<std::pair<const settings::IniSetting*, std::string>>& changes,
                          settings::Lang l);
    std::wstring revert(settings::Lang l);
    std::wstring restore(const std::wstring& backup, settings::Lang l);
    void keepNewestBackups(size_t count);
    std::wstring resetDefaults(settings::Lang l);
    // 0.55: profiles - every setting of uaro-graphics.ini (the player's changes in) under a name, in
    // uaro-graphics\profiles\<name>.ini; loading one makes it uaro-graphics.ini (a backup first, the language kept).
    std::vector<std::wstring> profiles() const;  // names, sorted
    static bool profileNameOk(const std::wstring& name);  // letters, digits, spaces, - and _; up to 40
    std::wstring saveProfile(const std::wstring& name, settings::Lang l);
    std::wstring loadProfile(const std::wstring& name, settings::Lang l);
    std::wstring deleteProfile(const std::wstring& name, settings::Lang l);
    std::wstring removeCompatFlag(const CompatFlag& f, settings::Lang l);
    // 2.4.2: the game's key this app's as it starts - setup.exe out of it: with none in either copy, Apply's first
    // run (the game would start setup.exe and wait for it); with the game closed, every value setup.exe's copy alone
    // holds taken into the personal one. An error, else "".
    std::wstring ownGameKey(settings::Lang l);
    std::wstring forgetLogin(settings::Lang l);  // the saved login goes; only while the game is closed (§4.1)
    // Spec §8: the whole game key as the game reads it, to a .reg file under the personal copy's path; from one, it
    // replaces the personal copy (only while the game is closed, a copy of the present one made first).
    std::wstring saveGameSettings(const std::wstring& file, settings::Lang l) const;
    std::wstring loadGameSettings(const std::wstring& file, settings::Lang l);
    std::wstring resetWindowLayout(settings::Lang l);  // spec §8; only while the game is closed
    std::wstring launchGame(settings::Lang l);
    std::optional<settings::Lang> savedLanguage() const;
    void saveLanguage(settings::Lang l);  // the app's own key: no backup for it
    // 2.2.1: the anonymous statistics (core/usage.hpp) - whether the player was asked ([stats] send in the file), and
    // the answer written at once (with this computer's random number the first time)
    bool statsAsked() const;
    std::wstring setStats(bool send, settings::Lang l);

    // 2.6: the app's own keys of [utility], written at once without a backup like the language and kept across a
    // reset and a profile: the mode (a new install starts Simple, one the app ran in before Detailed), the window's
    // place, the version whose notes were shown.
    bool usedBefore() const;  // the file holds the language or the statistics' answer
    settings::Mode startMode() const;
    void saveMode(settings::Mode m);
    std::optional<settings::WindowPlace> savedPlace() const;
    void savePlace(const settings::WindowPlace& p);
    std::string seenVersion() const;
    void saveSeenVersion(const std::string& version);
    // This computer as the auto setup sees it (0.50; 2.6 from the Render page): the card the Render page's list
    // names, else the fast one.
    settings::Machine machine() const;
    // 2.6: the word recommended for a row on this computer - the auto setup's, else the shipped default.
    std::string recommended(const settings::IniSetting& s) const;

private:
    // The ini as it is on disk now - what Notepad or use-*.bat wrote since load() stays - or the shipped defaults
    // when there is none.
    std::string currentIni() const;
    std::wstring writeIni(const std::vector<settings::IniEdit>& edits, settings::Lang l);
    // What a reset or a profile loaded keeps of the ini now: the language, (2.2.2) the statistics' answer and number.
    std::vector<settings::IniEdit> keptAcross() const;
    std::wstring backupOnce(settings::Lang l, bool prune = true);  // prune: only the 20 newest stay (spec §4.6)
    // 2.6.22: an old file's ten groups (fixes, light...) written into the effects, the old lines taken out - at once,
    // after a backup (effect_groups::settleLegacy)
    void settleOldGroups();
    // Each value on its own (spec §4.5): "" or the ones that failed, "NAME (error N), ..."; the others are written.
    std::string writeValues(const std::vector<settings::RegWrite>& writes);
    std::vector<settings::IniEdit> pendingIniEdits() const;

    Paths paths_;
    SystemInfo system_;
    Snapshot now_;
    std::map<std::string, std::string> ini_;  // "section.key" -> the player's word
    std::map<std::string, std::string> reg_;  // regKey -> the player's word
    std::optional<settings::ScreenChoice> screen_;
    std::map<std::string, std::string> wroteNow_;  // "section.key" -> the word before the first writeNow
    bool backedUp_ = false;
    mutable std::map<std::string, std::string> recommended_;  // 2.6: recommendedTokens, made at the first ask
};

// What the Screen page says about a plan it cannot write.
settings::Text problemText(settings::ScreenProblem p);

}  // namespace uagfx::app

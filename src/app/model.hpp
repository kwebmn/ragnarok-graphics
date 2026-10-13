#pragma once
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "app/registry.hpp"
#include "app/system.hpp"
#include "core/config.hpp"
#include "core/settings/game_registry.hpp"
#include "core/settings/game_settings.hpp"
#include "core/settings/input_module.hpp"
#include "core/settings/log_digest.hpp"
#include "core/settings/presets.hpp"
#include "core/settings/schema.hpp"
#include "core/settings/screen.hpp"
#include "core/settings/strings.hpp"

namespace uagfx::app {

struct Paths {
    std::wstring dir;  // the client folder
    std::wstring ini, log, game, version, backups, report, ddraw, dgvoodoo;
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
    bool dgvoodoo = false;         // dgvoodoo\DDraw.dll
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
    const Paths& paths() const { return paths_; }

    // uaro-graphics.ini keys: the player's change, else the file's value.
    std::string token(const settings::IniSetting& s) const;
    void setToken(const settings::IniSetting& s, const std::string& token);
    Backend backend() const;        // with the player's change
    Config pendingConfig() const;   // the file's settings with the player's changes

    settings::ScreenChoice screenChoice() const;  // the player's, else what the game gets now
    void setScreen(const settings::ScreenChoice& c);
    settings::ScreenNow screenNow() const;
    settings::ScreenPlan screenPlan() const;
    void screenAsSetup();          // "as in setup.exe": the personal screen values go
    bool screenAsSetupPending() const { return screenAsSetup_; }
    std::optional<settings::Size> setupScreen(bool& fullscreen) const;  // setup.exe's, when it differs from the game's

    // The game's key on the pages (0.6.2): the player's word, else what the game reads now (game_settings).
    std::string regWord(const settings::RegSetting& s) const;
    void setRegWord(const settings::RegSetting& s, const std::string& word);
    // Whether a page may change it now: what the game writes itself waits for it to close (spec §4.1).
    bool regOpen(const settings::RegSetting& s) const { return !s.gameWrites || !now_.gameRunning; }
    std::vector<std::wstring> skins() const;  // the folders of skin\ next to uaRO.exe (spec §6.1)
    bool hasIntro() const;                     // openning.bik next to uaRO.exe

    void applyPreset(settings::Preset p);
    size_t pendingCount() const;  // the player's changes not written yet
    // Apply has something to write: the player's changes, or the game's key on a first run (spec §4.3).
    bool canApply() const { return pendingCount() > 0 || !settings::gameKeyExists(now_.registry); }
    bool canRevert() const { return pendingCount() > 0 || !wroteNow_.empty(); }

    // Writing (spec §4). Each returns "" or what went wrong, in the language given.
    std::wstring apply(settings::Lang l);
    // While the game runs, one key of uaro-graphics.ini now (a slider let go, spec §6); "Undo" brings it back.
    std::wstring writeNow(const settings::IniSetting& s, const std::string& token, settings::Lang l);
    std::wstring revert(settings::Lang l);
    std::wstring restore(const std::wstring& backup, settings::Lang l);
    void keepNewestBackups(size_t count);
    std::wstring resetDefaults(settings::Lang l);
    std::wstring removeCompatFlag(const CompatFlag& f, settings::Lang l);
    std::wstring resetRegistryValue(const std::string& name, settings::Lang l);
    std::wstring forgetLogin(settings::Lang l);  // the saved login goes; only while the game is closed (§4.1)
    // Spec §8: the whole game key as the game reads it, to a .reg file under the personal copy's path; from one, it
    // replaces the personal copy (only while the game is closed, a copy of the present one made first).
    std::wstring saveGameSettings(const std::wstring& file, settings::Lang l) const;
    std::wstring loadGameSettings(const std::wstring& file, settings::Lang l);
    std::wstring resetWindowLayout(settings::Lang l);  // spec §8; only while the game is closed
    std::wstring launchGame(settings::Lang l);
    std::optional<settings::Lang> savedLanguage() const;
    void saveLanguage(settings::Lang l);  // the app's own key: no backup for it

private:
    // The ini as it is on disk now - what Notepad or use-*.bat wrote since load() stays - or the shipped defaults
    // when there is none.
    std::string currentIni() const;
    std::wstring writeIni(const std::vector<settings::IniEdit>& edits, settings::Lang l);
    std::wstring backupOnce(settings::Lang l, bool prune = true);  // prune: only the 20 newest stay (spec §4.6)
    // Each value on its own (spec §4.5): "" or the ones that failed, "NAME (error N), ..."; the others are written.
    std::string writeValues(const std::vector<settings::RegWrite>& writes);
    std::vector<settings::IniEdit> pendingIniEdits() const;

    Paths paths_;
    SystemInfo system_;
    Snapshot now_;
    std::map<std::string, std::string> ini_;  // "section.key" -> the player's word
    std::map<std::string, std::string> reg_;  // regKey -> the player's word
    std::optional<settings::ScreenChoice> screen_;
    bool screenAsSetup_ = false;
    std::map<std::string, std::string> wroteNow_;  // "section.key" -> the word before the first writeNow
    bool backedUp_ = false;
};

// What the Screen page says about a plan it cannot write.
settings::Text problemText(settings::ScreenProblem p);

}  // namespace uagfx::app

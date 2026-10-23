#include "app/model.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdint>
#include <ctime>
#include <set>

#include "app/app_disk.hpp"
#include "app/files.hpp"
#include "app/installer.hpp"
#include "app/text.hpp"
#include "app/update_run.hpp"
#include "core/effect_groups.hpp"
#include "core/settings/compat_flags.hpp"
#include "core/settings/pad_files.hpp"
#include "core/settings/ini_edit.hpp"
#include "core/settings/recommended.hpp"
#include "core/settings/reg_file.hpp"
#include "core/settings/report.hpp"

namespace uagfx::app {

using namespace settings;

namespace {

std::string firstLine(const std::string& s) {
    const size_t nl = s.find_first_of("\r\n");
    return nl == std::string::npos ? s : s.substr(0, nl);
}

IniEdit editOf(const std::string& sectionKey, const std::string& value) {
    const size_t dot = sectionKey.find('.');
    return IniEdit{sectionKey.substr(0, dot), sectionKey.substr(dot + 1), value};
}

}  // namespace

Text problemText(ScreenProblem p) {
    switch (p) {
        case ScreenProblem::TooBig: return Text::ScreenTooBig;
        case ScreenProblem::TooSmall: return Text::ScreenTooSmall;
        case ScreenProblem::BadNumber:
        case ScreenProblem::None: break;
    }
    return Text::ScreenBadNumber;
}

namespace {

}  // namespace

Model::Model() {
    paths_.dir = exeFolder();
    {
        AppDisk disk;  // 2.8: a 2.x install's files into uaro-graphics\ before anything reads them
        layout::migrate(utf8(paths_.dir), disk);
    }
    for (const char* folder : layout::kMadeAtStart) makeFolder(ourPlace(paths_.dir, folder));  // each one level
    paths_.ini = ourFile(paths_.dir, layout::File::Ini);
    paths_.log = ourPlace(paths_.dir, layout::now(layout::File::DriverLog));
    paths_.game = paths_.dir + L"\\uaRO.exe";
    paths_.version = paths_.dir + L"\\uaro-graphics\\version.txt";
    paths_.backups = ourFile(paths_.dir, layout::File::Backups);
    paths_.report = ourPlace(paths_.dir, layout::now(layout::File::Report));
    paths_.ddraw = paths_.dir + L"\\ddraw.dll";
    paths_.pack = ourFile(paths_.dir, layout::File::Pack);
    if (!gameRunning(paths_.game)) {
        installer::removeOldPad(paths_.dir);      // 2.9: the pad is inside the driver
        installer::removeOldScripts(paths_.dir);  // 2.10: no .bat of ours any more (an old update.bat's too)
    }
    system_ = probeSystem();
    load();
}

void Model::load() {
    Snapshot s;
    s.iniExists = readFile(paths_.ini, s.iniText);
    s.config = parseConfig(s.iniText);
    s.registry.personal = readPersonal();
    s.registry.common = readCommon();
    for (const wchar_t* name : {L"dinput.dll", L"dinput8.dll", L"dinput.dll.bak", L"dinput8.dll.bak"}) {
        std::string bytes;
        if (!readFile(paths_.dir + L"\\" + name, bytes)) continue;
        ModuleFile m;
        m.file = name;
        m.loaded = loadedByGame(utf8(name));
        m.info = identifyModule(utf8(name), std::vector<uint8_t>(bytes.begin(), bytes.end()));
        if (m.info.kind != ModuleKind::Other) {
            m.iniPath = paths_.dir + L"\\" + wide(m.info.ini);
            m.iniExists = readFile(m.iniPath, m.iniText);
        }
        if (m.loaded && m.info.kind == ModuleKind::Roext && s.roextModule < 0) {
            s.roext = roextWindow(readIni(m.iniText));
            s.roextModule = int(s.modules.size());
        }
        s.modules.push_back(m);
    }
    // 3.0.1-beta.8: ROExt's work in the driver - its dinput8.dll moved aside by the installer (a client that never
    // had ROExt keeps the game's own window), none loaded from the folder, [client] roext on
    std::string aside;
    const bool roextAside =
        readFile(paths_.dir + L"\\" + wide(layout::kOriginals) + L"\\dinput8.dll", aside) &&
        identifyModule("dinput8.dll", std::vector<uint8_t>(aside.begin(), aside.end())).kind == ModuleKind::Roext;
    if (s.roextModule < 0 && s.config.clientRoext && roextAside) {
        s.roextInDriver = true;
        s.roext.active = true;
        s.roext.lock = s.config.roext.windowLock;
        if (s.config.roext.windowWidth > 0 && s.config.roext.windowHeight > 0)
            s.roext.size = {s.config.roext.windowWidth, s.config.roext.windowHeight};
    }
    for (const CompatEntry& e : readCompat(paths_.game))
        for (const std::string& f : parseLayers(e.flags).flags) s.compat.push_back(CompatFlag{e.allUsers, f});
    std::string bytes;
    if (readFile(paths_.ddraw, bytes))
        s.ddraw = bytes.find("uaRO Graphics") != std::string::npos ? Ddraw::Ours : Ddraw::Other;
    if (readFile(paths_.version, bytes)) s.installedVersion = firstLine(bytes);
    s.gameRunning = gameRunning(paths_.game);
    s.backups = listFolder(paths_.backups, true);
    std::reverse(s.backups.begin(), s.backups.end());
    if (readFile(paths_.log, bytes)) s.log = digestLog(bytes);
    s.folderWritable = folderWritable(paths_.dir);
    s.programFiles = underProgramFiles(paths_.dir);
    now_ = s;
    settleOldGroups();
}

int Model::cardLevel() const {
    const bool d3d12 = system_.d3d12Probed && system_.d3d12.ok &&
                       token(*findSetting("renderer.api")) != "d3d11";
    return effect_groups::cardLevel(d3d12, system_.d3d12.vramMb);
}

void Model::settleOldGroups() {
    const auto ini = readIni(now_.iniText);
    bool old = false;
    for (int g = 0; g < kOldEffectGroups; ++g) {
        const std::string key = std::string("effects.group_") + effect_groups::oldGroupWord(g);
        old = old || ini.count(key) || ini.count(key + "_strength");
    }
    if (!old) return;
    std::vector<IniEdit> edits = effect_groups::settleLegacy(now_.config.effects, cardLevel());
    if (edits.empty()) edits = effect_groups::oldGroupLinesOut();  // only as they always were: on, 100 %
    if (!backupOnce(Lang::En).empty()) return;  // the file left as it is: the driver holds them all the same
    const std::string text = applyIniEdits(now_.iniText, edits);
    if (!writeFileAtomic(paths_.ini, text)) return;
    now_.iniText = text;
    now_.config = parseConfig(text);
}

bool Model::refreshRunning() {
    const bool running = gameRunning(paths_.game);
    if (running == now_.gameRunning) return false;
    if (running)
        now_.gameRunning = true;
    else
        load();  // it wrote its own values as it closed - Option, UIRectInfo, its quality, the login (0.6.2 review I1)
    return true;
}

std::string Model::token(const IniSetting& s) const {
    const auto it = ini_.find(sectionKey(s));
    return it != ini_.end() ? it->second : tokenOf(s, now_.config);
}

void Model::setToken(const IniSetting& s, const std::string& token) {
    if (!validToken(s, token)) return;
    if (token == tokenOf(s, now_.config))
        ini_.erase(sectionKey(s));
    else
        ini_[sectionKey(s)] = token;
}

std::vector<IniEdit> Model::pendingIniEdits() const {
    std::vector<IniEdit> edits;
    for (const auto& kv : ini_) edits.push_back(editOf(kv.first, kv.second));
    return edits;
}

Config Model::pendingConfig() const { return parseConfig(applyIniEdits(now_.iniText, pendingIniEdits())); }

ScreenNow Model::screenNow() const {
    const auto fs = effectiveValue(now_.registry, "", "ISFULLSCREENMODE");
    const bool full = fs && fs->asDword() && *fs->asDword() != 0;
    const auto w = effectiveValue(now_.registry, "", "WIDTH");
    const auto h = effectiveValue(now_.registry, "", "HEIGHT");
    const Size size{w && w->asDword() ? int(*w->asDword()) : 0, h && h->asDword() ? int(*h->asDword()) : 0};
    return currentScreen(full, size, now_.config, now_.roext, system_.monitors);
}

ScreenChoice Model::screenChoice() const {
    if (screen_) return *screen_;
    ScreenNow n = screenNow();
    if ((n.game.width <= 0 || n.game.height <= 0) && !system_.monitors.empty()) {  // no settings yet (spec §4.3)
        const MonitorInfo& m = system_.monitors[0];
        n.choice.show = Show::Window;
        n.choice.monitor = 0;
        n.choice.ui = m.scale;
        n.choice.window = defaultWindow(m, system_.frame);
    }
    return n.choice;
}

void Model::setScreen(const ScreenChoice& c) {
    screen_ = c;
}

ScreenPlan Model::screenPlan() const {
    return planScreen(screenChoice(), system_.monitors, now_.roext, system_.frame);
}

bool Model::screenWaits() const {
    return now_.gameRunning && (now_.roextModule >= 0 || now_.roextInDriver) && screenPlan().writesRoext;
}

std::string Model::regWord(const RegSetting& s) const {
    const auto it = reg_.find(regKey(s));
    return it != reg_.end() ? it->second : regWordOf(s, now_.registry);
}

void Model::setRegWord(const RegSetting& s, const std::string& word) {
    if (!validRegWord(s, word)) return;
    if (word == regWordOf(s, now_.registry))
        reg_.erase(regKey(s));
    else
        reg_[regKey(s)] = word;
}

std::vector<std::wstring> Model::skins() const { return listFolder(paths_.dir + L"\\skin", true); }

bool Model::hasIntro() const { return fileExists(paths_.dir + L"\\openning.bik"); }

void Model::applyPreset(Preset p) {
    for (const IniEdit& e : presetEdits(p))
        if (const IniSetting* s = findSetting(e.section + "." + e.key)) setToken(*s, e.value);
    for (const auto& w : presetRegWords(p))
        if (const RegSetting* s = findRegSetting(w.first)) setRegWord(*s, w.second);
}

std::optional<Preset> Model::preset() const {
    std::map<std::string, std::string> words;
    for (int i = 0; i < kPresetCount; ++i)
        for (const auto& w : presetRegWords(Preset(i)))
            if (const RegSetting* s = findRegSetting(w.first)) words[w.first] = regWord(*s);
    return presetOf(pendingConfig(), words);
}

size_t Model::pendingCount() const {
    const bool screen = screen_ && !(*screen_ == screenNow().choice);
    return ini_.size() + reg_.size() + (screen ? 1 : 0);
}

std::string Model::currentIni() const {
    std::string text;
    return readFile(paths_.ini, text) ? text : defaultIni();
}

std::wstring Model::writeIni(const std::vector<IniEdit>& edits, Lang l) {
    const std::string text = applyIniEdits(currentIni(), edits);
    if (!writeFileAtomic(paths_.ini, text)) return wtrf(l, Text::ErrWriteFile, {"uaro-graphics.ini"});
    now_.iniText = text;
    now_.iniExists = true;
    now_.config = parseConfig(text);
    return L"";
}

std::wstring Model::backupOnce(Lang l, bool prune) {
    if (backedUp_) return L"";
    const std::wstring dir = paths_.backups + L"\\" + timestampName();
    if (!makeFolder(paths_.backups) || !makeFolder(dir)) return wtrf(l, Text::ErrBackup, {utf8(dir)});
    std::string list;  // files.txt: "<name in the backup>\t<path in the client folder>"
    std::set<std::wstring> kept;
    auto keep = [&](const std::wstring& relative, const std::wstring& saved) {
        std::string bytes;
        if (kept.count(saved) || !readFile(paths_.dir + L"\\" + relative, bytes)) return true;  // nothing to keep
        kept.insert(saved);
        if (!writeFileAtomic(dir + L"\\" + saved, bytes)) return false;
        list += utf8(saved) + "\t" + utf8(relative) + "\r\n";
        return true;
    };
    // 2.2: dgVoodoo's own settings no longer; 2.8: the ini by where it is now (uaro-graphics\)
    bool ok = keep(paths_.ini.substr(paths_.dir.size() + 1), L"uaro-graphics.ini");
    for (const ModuleFile& m : now_.modules)
        if (ok && m.iniExists) ok = keep(wide(m.info.ini), L"module-" + wide(m.info.ini));
    const GameKeyPaths k = gameKeyPaths();
    // The registry as it is now, not as it was read: the game writes its hotbars at every map change (0.6.0 M2).
    const RegCopy personal = readPersonal();
    const RegCopy common = readCommon();
    ok = ok && writeFileAtomic(dir + L"\\personal.reg", utf8ToRegFile(toRegFile(personal, k.personalRoot))) &&
         writeFileAtomic(dir + L"\\common.reg", utf8ToRegFile(toRegFile(common, k.commonRoot))) &&
         writeFileAtomic(dir + L"\\files.txt", list);
    if (!ok) return wtrf(l, Text::ErrBackup, {utf8(dir)});
    backedUp_ = true;
    if (prune) keepNewestBackups(20);
    return L"";
}

std::string Model::writeValues(const std::vector<RegWrite>& writes) {
    std::string failed;
    for (const RegWrite& w : writes) {
        const std::string e = writePersonalValue(w);
        if (!e.empty()) failed += (failed.empty() ? "" : ", ") + e;
    }
    return failed;
}

std::wstring Model::apply(Lang l) {
    refreshRunning();
    std::vector<IniEdit> ini = pendingIniEdits();
    std::vector<RegWrite> reg;
    std::vector<IniEdit> roext;
    const bool firstRun = !gameKeyExists(now_.registry);
    const bool screen = screen_ && !(*screen_ == screenNow().choice);
    bool waits = false;  // the screen needs ROExt's ini, written only while the game is closed (spec §4.1)
    ScreenChoice chosen;
    if (screen || firstRun) {
        chosen = screenChoice();
        const ScreenPlan p = screenPlan();
        if (p.problem != ScreenProblem::None) return wtrf(l, Text::ErrScreen, {utf8(wtr(l, problemText(p.problem)))});
        waits = p.writesRoext && (now_.roextModule >= 0 || now_.roextInDriver) && now_.gameRunning;
        // A first run's key goes even then: the game waits for it in its setup.exe loop (spec §4.3; 0.6.1 review I1).
        if (firstRun) reg = firstRunWrites(p.game.width, p.game.height, p.fullscreen);
        if (!waits) {  // the screen waits for the game to close; the rest goes now (0.6.0 M4)
            if (!firstRun) {
                auto add = [&](const char* name, uint32_t v) {
                    if (const auto w = planWrite(now_.registry, "", name, RegValue::dword(v))) reg.push_back(*w);
                };
                add("ISFULLSCREENMODE", p.fullscreen ? 1 : 0);
                add("WIDTH", uint32_t(p.game.width));
                add("HEIGHT", uint32_t(p.game.height));
                add("BITPERPIXEL", 32);
            }
            if (p.writesIni) {
                ini.push_back({"display", "mode", displayModeToken(p.mode)});
                ini.push_back({"display", "monitor", num(p.monitor)});
                ini.push_back({"display", "window_width", num(p.iniWindow.width)});
                ini.push_back({"display", "window_height", num(p.iniWindow.height)});
            }
            if (p.writesRoext && now_.roextModule >= 0)
                roext = {{"ROExt", "WindowWidth", num(p.game.width)}, {"ROExt", "WindowHeight", num(p.game.height)}};
            if (p.writesRoext && now_.roextInDriver) {  // 3.0.1-beta.8: the driver's [roext]
                ini.push_back({"roext", "window_width", num(p.game.width)});
                ini.push_back({"roext", "window_height", num(p.game.height)});
            }
        }
    }
    std::map<std::string, std::string> held;  // what the game writes itself waits for it to close (spec §4.1)
    for (const auto& kv : reg_) {  // after the screen's: the player's word wins over a first run's
        const RegSetting* s = findRegSetting(kv.first);
        if (!s) continue;
        if (!regOpen(*s))
            held.insert(kv);
        else if (const auto w = planWrite(now_.registry, s->subkey, s->name, regValueOf(*s, kv.second)))
            reg.push_back(*w);
    }
    // 2.4.2: with the game closed, every value setup.exe's copy alone holds goes into the personal one too - first,
    // so that a write of the player's to the same value comes after it
    if (!firstRun && !now_.gameRunning) {
        const std::vector<RegWrite> pins = pinWrites(now_.registry);
        reg.insert(reg.begin(), pins.begin(), pins.end());
    }
    if (!ini.empty() || !reg.empty() || !roext.empty()) {
        std::wstring err = backupOnce(l);
        if (!err.empty()) return err;
        if (!reg.empty()) {
            const std::string failed = writeValues(reg);
            if (!failed.empty()) {
                load();
                return wtrf(l, Text::ErrWriteRegistry, {failed});
            }
        }
        if (!roext.empty()) {
            const ModuleFile& m = now_.modules[size_t(now_.roextModule)];
            std::string text;  // as it is now, like the ini: what the player wrote there meanwhile stays
            readFile(m.iniPath, text);
            if (!writeFileAtomic(m.iniPath, applyIniEdits(text, roext))) {
                load();
                return wtrf(l, Text::ErrWriteFile, {m.info.ini});
            }
        }
        if (!ini.empty()) {
            err = writeIni(ini, l);
            if (!err.empty()) {
                load();
                return err;
            }
        }
    }
    // What was written is no longer pending; a screen that waits stays.
    ini_.clear();
    reg_ = held;
    if (waits)
        screen_ = chosen;  // the next Apply with the game closed writes the rest of it
    else
        screen_.reset();
    wroteNow_.clear();
    load();
    return waits || !held.empty() ? wtr(l, Text::ApplyWaits) : L"";
}

std::wstring Model::writeNow(const IniSetting& s, const std::string& token, Lang l) {
    if (!validToken(s, token)) return L"";
    const std::string key = sectionKey(s);
    if (!wroteNow_.count(key)) wroteNow_[key] = tokenOf(s, now_.config);
    std::wstring err = backupOnce(l);
    if (!err.empty()) return err;
    ini_.erase(key);
    return writeIni({{s.section, s.key, token}}, l);
}

std::wstring Model::writeNow(const std::vector<std::pair<const IniSetting*, std::string>>& changes, Lang l) {
    std::wstring err = backupOnce(l);
    if (!err.empty()) return err;
    std::vector<IniEdit> edits;
    for (const auto& [s, token] : changes) {
        if (!s || !validToken(*s, token)) continue;
        const std::string key = sectionKey(*s);
        if (!wroteNow_.count(key)) wroteNow_[key] = tokenOf(*s, now_.config);
        ini_.erase(key);
        edits.push_back({s->section, s->key, token});
    }
    return edits.empty() ? L"" : writeIni(edits, l);
}

std::wstring Model::revert(Lang l) {
    ini_.clear();
    reg_.clear();
    screen_.reset();
    if (wroteNow_.empty()) return L"";
    std::vector<IniEdit> back;
    for (const auto& kv : wroteNow_) back.push_back(editOf(kv.first, kv.second));
    wroteNow_.clear();
    return writeIni(back, l);
}

std::wstring Model::restore(const std::wstring& backup, Lang l) {
    refreshRunning();
    if (now_.gameRunning) return wtr(l, Text::ErrGameRunning);
    const std::wstring dir = paths_.backups + L"\\" + backup;
    std::string list, reg;
    RegCopy personal;
    if (!readFile(dir + L"\\files.txt", list) || !readFile(dir + L"\\personal.reg", reg) ||
        !fromRegFile(regFileToUtf8(reg), gameKeyPaths().personalRoot, personal))
        return wtrf(l, Text::ErrRestore, {utf8(backup)});
    backedUp_ = false;  // what is there now goes into a copy of its own first
    std::wstring err = backupOnce(l, false);  // the copy restored may be the oldest: the next run prunes
    if (!err.empty()) return err;
    size_t pos = 0;
    while (pos < list.size()) {
        size_t end = list.find("\r\n", pos);
        if (end == std::string::npos) end = list.size();
        const std::string line = list.substr(pos, end - pos);
        pos = end + 2;
        const size_t tab = line.find('\t');
        if (tab == std::string::npos) continue;
        std::string bytes;
        // 2.8: a 2.x backup names the root ini - it goes where the settings are now
        const std::string now = layout::relativeNow(line.substr(tab + 1));
        const std::wstring to = now == layout::now(layout::File::Ini) ? paths_.ini : paths_.dir + L"\\" + wide(now);
        if (!readFile(dir + L"\\" + wide(line.substr(0, tab)), bytes) || !writeFileAtomic(to, bytes)) {
            load();
            return wtrf(l, Text::ErrRestore, {line.substr(tab + 1)});
        }
    }
    const std::string failed = replacePersonal(personal);
    ini_.clear();
    reg_.clear();
    screen_.reset();
    wroteNow_.clear();
    load();
    return failed.empty() ? L"" : wtrf(l, Text::ErrWriteRegistry, {failed});
}

void Model::keepNewestBackups(size_t count) {
    const std::vector<std::wstring> all = listFolder(paths_.backups, true);  // oldest first: names are timestamps
    for (size_t i = 0; i + count < all.size(); ++i) removeTree(paths_.backups + L"\\" + all[i]);
    now_.backups = listFolder(paths_.backups, true);
    std::reverse(now_.backups.begin(), now_.backups.end());
}

std::vector<IniEdit> Model::keptAcross() const {
    std::vector<IniEdit> keep;
    if (const auto lang = savedLanguage()) keep.push_back({"utility", "language", langToken(*lang)});
    // 2.2.2: the statistics' answer and this computer's number - else asked again, and counted as a new player
    const auto now = readIni(currentIni());
    for (const char* key : {"send", "id"})
        if (const auto it = now.find(std::string("stats.") + key); it != now.end())
            keep.push_back({"stats", key, it->second});
    for (const char* key : {"mode", "window", "seen", "report_asked"})  // 2.6: the app's own
        if (const auto it = now.find(std::string("utility.") + key); it != now.end())
            keep.push_back({"utility", key, it->second});
    // 3.0.0-beta.1: the answer about the automatic reports, once given - else a reset would take it back unasked
    if (const auto it = now.find("report.auto"); it != now.end() && now.count("utility.report_asked"))
        keep.push_back({"report", "auto", it->second});
    // 3.0.0-beta.2: the update channel - a reset or a profile never takes a tester off «Бета»
    if (const auto it = now.find("update.channel"); it != now.end()) keep.push_back({"update", "channel", it->second});
    return keep;
}

std::wstring Model::resetDefaults(Lang l) {
    std::wstring err = backupOnce(l);
    if (!err.empty()) return err;
    const std::string text = applyIniEdits(defaultIni(), keptAcross());
    if (!writeFileAtomic(paths_.ini, text)) return wtrf(l, Text::ErrWriteFile, {"uaro-graphics.ini"});
    ini_.clear();
    wroteNow_.clear();
    load();
    return L"";
}

std::vector<std::wstring> Model::profiles() const {
    std::vector<std::wstring> out;
    for (const std::wstring& f : listFolder(paths_.dir + L"\\uaro-graphics\\profiles", false))
        if (f.size() > 4 && f.compare(f.size() - 4, 4, L".ini") == 0) out.push_back(f.substr(0, f.size() - 4));
    return out;
}

bool Model::profileNameOk(const std::wstring& name) {
    if (name.empty() || name.size() > 40 || name.front() == L' ' || name.back() == L' ') return false;
    // Letters of any script (a Ukrainian name too, whatever the C library's locale), digits, spaces, - and _.
    for (const wchar_t c : name) {
        const bool ascii = (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9') ||
                           c == L' ' || c == L'-' || c == L'_';
        if (!ascii && c < 0xC0) return false;  // no punctuation, no control characters, nothing Windows forbids
    }
    return true;
}

std::wstring Model::saveProfile(const std::wstring& name, Lang l) {
    if (!profileNameOk(name)) return wtr(l, Text::ProfileBadName);
    const std::wstring folder = paths_.dir + L"\\uaro-graphics\\profiles";
    makeFolder(paths_.dir + L"\\uaro-graphics");
    makeFolder(folder);
    const std::string text = applyIniEdits(currentIni(), pendingIniEdits());
    if (!writeFileAtomic(folder + L"\\" + name + L".ini", text))
        return wtrf(l, Text::ErrWriteFile, {utf8(name) + ".ini"});
    return L"";
}

std::wstring Model::loadProfile(const std::wstring& name, Lang l) {
    std::string text;
    if (!profileNameOk(name) || !readFile(paths_.dir + L"\\uaro-graphics\\profiles\\" + name + L".ini", text))
        return wtrf(l, Text::ErrWriteFile, {utf8(name) + ".ini"});
    std::wstring err = backupOnce(l);
    if (!err.empty()) return err;
    if (!writeFileAtomic(paths_.ini, applyIniEdits(text, keptAcross())))
        return wtrf(l, Text::ErrWriteFile, {"uaro-graphics.ini"});
    ini_.clear();
    wroteNow_.clear();
    load();
    return L"";
}

std::wstring Model::deleteProfile(const std::wstring& name, Lang l) {
    if (!profileNameOk(name) || !deleteFile(paths_.dir + L"\\uaro-graphics\\profiles\\" + name + L".ini"))
        return wtrf(l, Text::ErrWriteFile, {utf8(name) + ".ini"});
    return L"";
}

std::wstring Model::removeCompatFlag(const CompatFlag& f, Lang l) {
    if (f.allUsers) return L"";  // only from uaRO.exe's properties, as administrator
    for (const CompatEntry& e : readCompat(paths_.game)) {
        if (e.allUsers) continue;
        CompatLayers layers = parseLayers(e.flags);
        if (removeLayer(layers, f.flag) && !writeUserCompat(paths_.game, formatLayers(layers)))
            return wtrf(l, Text::ErrWriteRegistry, {"AppCompatFlags"});
    }
    load();
    return L"";
}

std::wstring Model::ownGameKey(Lang l) {
    refreshRunning();
    if (!gameKeyExists(now_.registry)) {  // nothing of the player's yet: Apply's first run, ROExt's part waiting
        const std::wstring said = apply(l);
        return said == wtr(l, Text::ApplyWaits) ? L"" : said;
    }
    if (now_.gameRunning) return L"";  // it reads its key at its start: taken over once it is closed
    const std::vector<RegWrite> pins = pinWrites(now_.registry);
    if (pins.empty()) return L"";
    std::wstring err = backupOnce(l);
    if (!err.empty()) return err;
    const std::string failed = writeValues(pins);
    load();
    return failed.empty() ? L"" : wtrf(l, Text::ErrWriteRegistry, {failed});
}

std::wstring Model::forgetLogin(Lang l) {
    refreshRunning();
    if (now_.gameRunning) return wtr(l, Text::ErrGameRunning);  // the game writes it from its login window
    const auto w = planWrite(now_.registry, "", "ID", RegValue::sz(""));
    if (!w) return L"";
    std::wstring err = backupOnce(l);
    if (!err.empty()) return err;
    const std::string failed = writePersonalValue(*w);
    load();
    return failed.empty() ? L"" : wtrf(l, Text::ErrWriteRegistry, {failed});
}

std::wstring Model::saveGameSettings(const std::wstring& file, Lang l) const {
    GameRegistry r;  // as it is now: the game wrote its hotbars at its last map change
    r.personal = readPersonal();
    r.common = readCommon();
    RegCopy all = effectiveCopy(r);
    // The client never writes PASSWORD and drops it when it reads it (registry.md): an old one stays out of a file a
    // player may pass on (0.6.2 review).
    all.keys[""].erase("PASSWORD");
    const std::string text = utf8ToRegFile(toRegFile(all, gameKeyPaths().personalRoot));
    return writeFileAtomic(file, text) ? L"" : wtrf(l, Text::ErrWriteFile, {utf8(file)});
}

std::wstring Model::loadGameSettings(const std::wstring& file, Lang l) {
    refreshRunning();
    if (now_.gameRunning) return wtr(l, Text::ErrGameRunning);  // the game writes most of it at exit
    std::string bytes;
    RegCopy copy;
    if (!readFile(file, bytes) || !fromRegFileAny(regFileToUtf8(bytes), gameKeyRoots(), copy))
        return wtrf(l, Text::ErrReadFile, {utf8(file)});
    // Only the whole key replaces the personal copy: one folder of it - regedit's export of the chat macros, say -
    // would take the hotbars and the windows away (0.6.2 review I2).
    if (copy.keys.find("") == copy.keys.end()) return wtrf(l, Text::ErrPartialFile, {utf8(file)});
    std::wstring err = backupOnce(l);
    if (!err.empty()) return err;
    const std::string failed = replacePersonal(copy);
    reg_.clear();  // the game's key is another one now: what the pages changed was for the old one
    screen_.reset();
    load();
    return failed.empty() ? L"" : wtrf(l, Text::ErrWriteRegistry, {failed});
}

std::wstring Model::resetWindowLayout(Lang l) {
    refreshRunning();
    if (now_.gameRunning) return wtr(l, Text::ErrGameRunning);  // the game writes UIRectInfo at exit
    GameRegistry r;
    r.personal = readPersonal();
    r.common = readCommon();
    const std::vector<RegWrite> writes = windowLayoutReset(r);
    if (writes.empty()) return L"";
    std::wstring err = backupOnce(l);
    if (!err.empty()) return err;
    const std::string failed = writeValues(writes);
    load();
    return failed.empty() ? L"" : wtrf(l, Text::ErrWriteRegistry, {failed});
}

std::wstring Model::launchGame(Lang l) {
    markOurStart();  // 2.11: the driver plays at once (no launcher «Гра» again)
    const unsigned long e = launch(paths_.game, paths_.dir);
    refreshRunning();
    return e ? wtrf(l, Text::ErrLaunch, {num(static_cast<long long>(e))}) : L"";
}

AutoSetup Model::pendAutoSetup(bool& uiChanged) {
    const AutoSetup a = autoSetup(machine());
    applyPreset(a.preset);  // the game's own texture quality too
    for (const IniEdit& e : a.edits)
        if (const IniSetting* s = findSetting(e.section + "." + e.key)) setToken(*s, e.value);
    // 1.3.3: the interface a whole number of times the game's pixels, where it is a fraction now (167 %)
    uiChanged = false;
    if (!integerScale(screenPlan().s)) {
        const ScreenChoice before = screenChoice();
        ScreenChoice c = before;
        if (c.ui != a.ui) {
            c.ui = a.ui;
            setScreen(c);
            uiChanged = screenPlan().problem == ScreenProblem::None;
            if (!uiChanged) setScreen(before);  // too small a game at that size: as it was
        }
    }
    return a;
}

std::string Model::machineKept() const {
    const auto v = readIni(currentIni());
    const auto it = v.find("utility.machine");
    return it == v.end() ? "" : it->second;
}

std::wstring Model::recordMachine(Lang l) { return writeIni({{"utility", "machine", machineKey(machine())}}, l); }

FirstStart Model::startSetup(Lang l) {
    const FirstStart f = firstStart(usedBefore(), machineKept(), machineKey(machine()));
    if (f == FirstStart::Apply) {
        bool uiChanged = false;
        lastAuto_ = pendAutoSetup(uiChanged);
        apply(l);
    }
    if (f == FirstStart::Apply || f == FirstStart::Record) recordMachine(l);
    return f;
}

std::wstring Model::setSafeMode(bool on, Lang l) {
    if (!on) {
        deleteFile(ourPlace(paths_.dir, layout::kSafeModeNote));
        deleteFile(ourPlace(paths_.dir, layout::kFailedStarts));
    }
    setToken(*findSetting("renderer.safe_mode"), on ? "1" : "0");
    return apply(l);
}

bool Model::safeModeNoted() const { return fileExists(ourPlace(paths_.dir, layout::kSafeModeNote)); }

std::wstring Model::setSwitchedOff(bool off, Lang l) {
    setToken(*findSetting("renderer.api"), off ? "system" : "auto");
    return apply(l);
}

std::optional<Lang> Model::savedLanguage() const {
    const auto v = readIni(now_.iniText);
    const auto it = v.find("utility.language");
    return it == v.end() ? std::nullopt : parseLang(it->second);
}

void Model::saveLanguage(Lang l) { writeIni({{"utility", "language", langToken(l)}}, l); }

bool Model::statsAsked() const { return readIni(currentIni()).count("stats.send") > 0; }

bool Model::reportAsked() const { return readIni(currentIni()).count("utility.report_asked") > 0; }

std::wstring Model::setReportAuto(bool send, Lang l) {
    const std::wstring err = writeIni({{"report", "auto", send ? "1" : "0"}, {"utility", "report_asked", "1"}}, l);
    if (err.empty()) load();
    return err;
}

update::Channel Model::channel() const {
    const auto v = readIni(currentIni());
    const auto it = v.find("update.channel");
    return update::channelOf(it == v.end() ? "" : it->second, installedVersion(paths_.dir));
}

std::wstring Model::setChannel(update::Channel c, Lang l) {
    const bool other = c != channel();
    const std::wstring err = writeIni({{"update", "channel", update::channelToken(c)}}, l);
    if (err.empty()) {
        if (other) forgetBranch(paths_.dir);  // the left channel's offer never asked at the game's start
        load();
    }
    return err;
}

void Model::recordChannel(Lang l) {
    if (!readIni(currentIni()).count("update.channel")) setChannel(channel(), l);
}

bool Model::usedBefore() const {
    const auto v = readIni(now_.iniText);
    return v.count("utility.language") > 0 || v.count("stats.send") > 0;
}

Mode Model::startMode() const {
    const auto v = readIni(now_.iniText);
    const auto it = v.find("utility.mode");
    return settings::startMode(it == v.end() ? std::nullopt : parseMode(it->second), usedBefore());
}

void Model::saveMode(Mode m) { writeIni({{"utility", "mode", modeToken(m)}}, Lang::En); }

std::optional<WindowPlace> Model::savedPlace() const {
    const auto v = readIni(now_.iniText);
    const auto it = v.find("utility.window");
    return it == v.end() ? std::nullopt : parsePlace(it->second);
}

void Model::savePlace(const WindowPlace& p) { writeIni({{"utility", "window", placeToken(p)}}, Lang::En); }

std::string Model::seenVersion() const {
    const auto v = readIni(now_.iniText);
    const auto it = v.find("utility.seen");
    return it == v.end() ? std::string() : it->second;
}

void Model::saveSeenVersion(const std::string& version) { writeIni({{"utility", "seen", version}}, Lang::En); }

Machine Model::machine() const {
    const SystemInfo& sys = system_;
    Machine x;
    x.windows10 = sys.windows.major >= 10;
    x.d3d12 = sys.d3d12Probed && sys.d3d12.ok;
    x.d3d11 = sys.d3d11.ok;
    x.d3d11Level = sys.d3d11.level;
    // The Direct3D 12 probe reads the main adapter even where it makes no device (an old Radeon).
    x.vendor = sys.d3d12.vendor ? sys.d3d12.vendor : sys.d3d11.vendor;
    x.vramMb = sys.d3d12.vramMb ? sys.d3d12.vramMb : sys.d3d11.vramMb;
    x.adapter = !sys.d3d12.adapter.empty() ? sys.d3d12.adapter : sys.d3d11.adapter;
    unsigned long long shared = sys.d3d12.sharedMb ? sys.d3d12.sharedMb : sys.d3d11.sharedMb;  // 2.12
    // 0.50.2: the card the game will draw on - the one the Render page's list names, else the fast one
    const std::string word = token(*findSetting("renderer.gpu"));
    const win::AdapterProbe* card = sys.adapters.empty() ? nullptr : &sys.adapters.front();
    for (const win::AdapterProbe& c : sys.adapters)
        if (c.name.size() == word.size() &&
            std::equal(c.name.begin(), c.name.end(), word.begin(), [](char a, char b) {
                return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
            }))
            card = &c;
    if (card) {
        x.d3d12 = card->d3d12;
        x.d3d11 = card->d3d11Level != 0;
        x.d3d11Level = card->d3d11Level;
        x.vendor = card->vendor;
        x.vramMb = card->vramMb;
        x.adapter = card->name;
        shared = card->sharedMb;
    }
    x.integrated = integratedByMemory(x.vramMb, shared);  // 2.12: integrated graphics' own tiers
    x.battery = sys.computer.battery == 1;
    x.vramCapped = x.vramMb == kVramSeenAtMost;  // 0.51.1: no better number from the driver's registry
    if (!sys.monitors.empty()) {
        x.screenWidth = sys.monitors[0].pixels.width;
        x.screenHeight = sys.monitors[0].pixels.height;
        x.hdr = sys.monitors[0].hdr;  // 1.3.3
    }
    x.pack = fileExists(paths_.pack);
    return x;
}

std::string Model::recommended(const IniSetting& s) const {
    if (recommended_.empty()) recommended_ = recommendedTokens(parseConfig(defaultIni()), autoSetup(machine()));
    const auto it = recommended_.find(sectionKey(s));
    return it == recommended_.end() ? std::string() : it->second;
}

std::wstring Model::setStats(bool send, Lang l) {
    std::vector<IniEdit> edits{{"stats", "send", send ? "1" : "0"}};
    if (send && parseConfig(currentIni()).statsId.empty()) {
        // the clocks (seconds since 1970, the processor's time of this app) and a stack address, mixed: a number for
        // this computer, not a secret (no std::chrono: its clocks bring GetTimeZoneInformation into the imports)
        int here = 0;
        uint64_t x = uint64_t(std::time(nullptr)) ^ (uint64_t(std::clock()) << 32) ^
                     (uint64_t(reinterpret_cast<uintptr_t>(&here)) << 7);
        for (int i = 0; i < 4; ++i) {  // splitmix64: the bits spread
            x += 0x9e3779b97f4a7c15ull;
            x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
            x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
            x ^= x >> 31;
        }
        char id[24];
        std::snprintf(id, sizeof id, "%016llx", static_cast<unsigned long long>(x));
        edits.push_back({"stats", "id", id});
    }
    const std::wstring err = writeIni(edits, l);
    if (err.empty()) load();
    return err;
}

}  // namespace uagfx::app

#include "app/model.hpp"

#include <algorithm>
#include <set>

#include "app/files.hpp"
#include "app/text.hpp"
#include "core/settings/compat_flags.hpp"
#include "core/settings/ini_edit.hpp"
#include "core/settings/reg_file.hpp"

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

int dword(const RegCopy& c, const char* name) {
    const RegValue* v = c.find("", name);
    return v && v->asDword() ? int(*v->asDword()) : 0;
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

Model::Model() {
    paths_.dir = exeFolder();
    paths_.ini = paths_.dir + L"\\uaro-graphics.ini";
    paths_.log = paths_.dir + L"\\uaro-graphics.log";
    paths_.game = paths_.dir + L"\\uaRO.exe";
    paths_.version = paths_.dir + L"\\uaro-graphics\\version.txt";
    paths_.backups = paths_.dir + L"\\uaro-graphics-backup";
    paths_.report = paths_.dir + L"\\uaro-graphics-report.txt";
    paths_.ddraw = paths_.dir + L"\\ddraw.dll";
    paths_.dgvoodoo = paths_.dir + L"\\dgvoodoo\\DDraw.dll";
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
    for (const CompatEntry& e : readCompat(paths_.game))
        for (const std::string& f : parseLayers(e.flags).flags) s.compat.push_back(CompatFlag{e.allUsers, f});
    std::string bytes;
    if (readFile(paths_.ddraw, bytes))
        s.ddraw = bytes.find("uaRO Graphics") != std::string::npos ? Ddraw::Ours : Ddraw::Other;
    if (readFile(paths_.version, bytes)) s.installedVersion = firstLine(bytes);
    s.dgvoodoo = fileExists(paths_.dgvoodoo);
    s.gameRunning = gameRunning(paths_.game);
    s.backups = listFolder(paths_.backups, true);
    std::reverse(s.backups.begin(), s.backups.end());
    if (readFile(paths_.log, bytes)) s.log = digestLog(bytes);
    s.folderWritable = folderWritable(paths_.dir);
    s.programFiles = underProgramFiles(paths_.dir);
    now_ = s;
}

bool Model::refreshRunning() {
    const bool running = gameRunning(paths_.game);
    if (running == now_.gameRunning) return false;
    now_.gameRunning = running;
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

Backend Model::backend() const {
    return token(*findSetting("renderer.backend")) == "uaro" ? Backend::Uaro : Backend::DgVoodoo;
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
    return currentScreen(full, size, now_.config, now_.roext, system_.monitors, now_.config.backend);
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
    screenAsSetup_ = false;
}

ScreenPlan Model::screenPlan() const {
    return planScreen(screenChoice(), system_.monitors, backend(), now_.roext, system_.frame);
}

bool Model::screenWaits() const {
    return now_.gameRunning && now_.roextModule >= 0 && screenPlan().writesRoext;
}

void Model::screenAsSetup() {
    screen_.reset();
    screenAsSetup_ = true;
}

std::optional<Size> Model::setupScreen(bool& fullscreen) const {
    bool differs = false;
    for (const char* name : {"ISFULLSCREENMODE", "WIDTH", "HEIGHT"}) {
        const RegValue* mine = now_.registry.personal.find("", name);
        const RegValue* common = now_.registry.common.find("", name);
        differs = differs || (mine && common && *mine != *common);
    }
    if (!differs) return std::nullopt;
    fullscreen = dword(now_.registry.common, "ISFULLSCREENMODE") != 0;
    return Size{dword(now_.registry.common, "WIDTH"), dword(now_.registry.common, "HEIGHT")};
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

std::vector<std::pair<const RegSetting*, std::string>> Model::regSetupDiffers() const {
    GameRegistry setup = now_.registry;
    setup.personal = RegCopy{};
    const std::vector<std::string>& names = setupValues();
    std::vector<std::pair<const RegSetting*, std::string>> out;
    for (const RegSetting& s : regSettings()) {
        if (*s.subkey || std::find(names.begin(), names.end(), s.name) == names.end()) continue;
        if (!now_.registry.personal.find("", s.name) || !now_.registry.common.find("", s.name)) continue;
        const std::string theirs = regWordOf(s, setup);
        if (regWord(s) != theirs) out.push_back({&s, theirs});
    }
    return out;
}

void Model::regAsSetup() {
    for (const auto& d : regSetupDiffers()) setRegWord(*d.first, d.second);
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
    return ini_.size() + reg_.size() + (screen ? 1 : 0) + (screenAsSetup_ ? 1 : 0);
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
    bool ok = keep(L"uaro-graphics.ini", L"uaro-graphics.ini") && keep(L"dgVoodoo.conf", L"dgVoodoo.conf") &&
              keep(L"dgvoodoo\\dgVoodoo.conf", L"dgvoodoo-dgVoodoo.conf");
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
    // The Screen page shows the plan for the renderer chosen: a switch of the renderer alone writes it too.
    const bool screen = (screen_ && !(*screen_ == screenNow().choice)) || backend() != now_.config.backend;
    bool waits = false;  // the screen needs ROExt's ini, written only while the game is closed (spec §4.1)
    ScreenChoice chosen;
    if (screen || firstRun) {
        chosen = screenChoice();
        const ScreenPlan p = screenPlan();
        if (p.problem != ScreenProblem::None) return wtrf(l, Text::ErrScreen, {utf8(wtr(l, problemText(p.problem)))});
        waits = p.writesRoext && now_.roextModule >= 0 && now_.gameRunning;
        // A first run's key goes even then: the game waits for it in its setup.exe loop (spec §4.3; 0.6.1 review I1).
        if (firstRun) reg = firstRunWrites(p.game.width, p.game.height, p.fullscreen);
        if (waits) {  // the screen and its renderer switch wait for the game to close; the rest goes now (0.6.0 M4)
            ini.erase(std::remove_if(ini.begin(), ini.end(),
                                     [](const IniEdit& e) { return e.section == "renderer" && e.key == "backend"; }),
                      ini.end());
        } else {
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
        }
    } else if (screenAsSetup_) {
        for (const char* name : {"ISFULLSCREENMODE", "WIDTH", "HEIGHT", "BITPERPIXEL"})
            if (const auto w = planReset(now_.registry, "", name)) reg.push_back(*w);
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
    // What was written is no longer pending; a screen that waits stays, with its renderer switch.
    const auto backendWord = ini_.find("renderer.backend");
    const std::string kept = waits && backendWord != ini_.end() ? backendWord->second : "";
    ini_.clear();
    if (!kept.empty()) ini_["renderer.backend"] = kept;
    reg_ = held;
    if (waits)
        screen_ = chosen;  // the next Apply with the game closed writes the rest of it
    else
        screen_.reset();
    screenAsSetup_ = false;
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

std::wstring Model::revert(Lang l) {
    ini_.clear();
    reg_.clear();
    screen_.reset();
    screenAsSetup_ = false;
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
        if (!readFile(dir + L"\\" + wide(line.substr(0, tab)), bytes) ||
            !writeFileAtomic(paths_.dir + L"\\" + wide(line.substr(tab + 1)), bytes)) {
            load();
            return wtrf(l, Text::ErrRestore, {line.substr(tab + 1)});
        }
    }
    const std::string failed = replacePersonal(personal);
    ini_.clear();
    reg_.clear();
    screen_.reset();
    screenAsSetup_ = false;
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

std::wstring Model::resetDefaults(Lang l) {
    std::wstring err = backupOnce(l);
    if (!err.empty()) return err;
    std::vector<IniEdit> keep{{"renderer", "backend", backendToken(parseConfig(currentIni()).backend)}};
    if (const auto lang = savedLanguage()) keep.push_back({"utility", "language", langToken(*lang)});
    const std::string text = applyIniEdits(defaultIni(), keep);
    if (!writeFileAtomic(paths_.ini, text)) return wtrf(l, Text::ErrWriteFile, {"uaro-graphics.ini"});
    ini_.clear();
    wroteNow_.clear();
    load();
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

std::wstring Model::resetRegistryValue(const std::string& name, Lang l) {
    const auto w = planReset(now_.registry, "", name);
    if (!w) return L"";
    std::wstring err = backupOnce(l);
    if (!err.empty()) return err;
    const std::string failed = writePersonalValue(*w);
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
    const std::string text = utf8ToRegFile(toRegFile(effectiveCopy(r), gameKeyPaths().personalRoot));
    return writeFileAtomic(file, text) ? L"" : wtrf(l, Text::ErrWriteFile, {utf8(file)});
}

std::wstring Model::loadGameSettings(const std::wstring& file, Lang l) {
    refreshRunning();
    if (now_.gameRunning) return wtr(l, Text::ErrGameRunning);  // the game writes most of it at exit
    std::string bytes;
    RegCopy copy;
    if (!readFile(file, bytes) || !fromRegFileAny(regFileToUtf8(bytes), gameKeyRoots(), copy))
        return wtrf(l, Text::ErrReadFile, {utf8(file)});
    std::wstring err = backupOnce(l);
    if (!err.empty()) return err;
    const std::string failed = replacePersonal(copy);
    reg_.clear();  // the game's key is another one now: what the pages changed was for the old one
    screen_.reset();
    screenAsSetup_ = false;
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
    const unsigned long e = launch(paths_.game, paths_.dir);
    refreshRunning();
    return e ? wtrf(l, Text::ErrLaunch, {num(static_cast<long long>(e))}) : L"";
}

std::optional<Lang> Model::savedLanguage() const {
    const auto v = readIni(now_.iniText);
    const auto it = v.find("utility.language");
    return it == v.end() ? std::nullopt : parseLang(it->second);
}

void Model::saveLanguage(Lang l) { writeIni({{"utility", "language", langToken(l)}}, l); }

}  // namespace uagfx::app

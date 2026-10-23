#include <cstdio>

#include "app/files.hpp"
#include "app/installer.hpp"
#include "app/modes.hpp"
#include "app/page.hpp"
#include "app/palettes.hpp"
#include "app/setting_ids.hpp"
#include "app/update.hpp"
#include "core/settings/presets.hpp"
#include "core/update_check.hpp"
#include "core/version.hpp"

namespace uagfx::app {
namespace {

using namespace settings;

constexpr int kPreset = 1401;  // + Preset
constexpr int kBackups = 1410;
constexpr int kRestore = 1411;
constexpr int kKeepFive = 1412;
constexpr int kReset = 1420;
constexpr int kGameSave = 1430;
constexpr int kGameLoad = 1431;
constexpr int kLayoutReset = 1432;
constexpr int kUpdateCheck = 1440;  // 0.49
constexpr int kUpdateNow = 1441;
constexpr int kRollback = 1442;  // 2.10
constexpr int kRepair = 1443;
constexpr int kUninstall = 1444;
constexpr int kSafeToggle = 1445;    // 2.12: the safe mode on or off
constexpr int kSwitchToggle = 1446;  // uaRO Graphics off (api = system) or on
constexpr int kChannel = 1447;  // 3.0.0-beta.2: «Реліз» or «Бета»
constexpr int kProfiles = 1450;  // 0.55
constexpr int kProfileName = 1451;
constexpr int kProfileSave = 1452;
constexpr int kProfileLoad = 1453;
constexpr int kProfileDelete = 1454;
constexpr int kPalettesFile = 1460;  // 2.5.19
constexpr int kPalettesFolder = 1461;

Text presetName(Preset p) {
    switch (p) {
        case Preset::Maximum: return Text::PresetMaximum;
        case Preset::Balance: return Text::PresetBalance;
        case Preset::Speed: return Text::PresetSpeed;
        case Preset::Plain: break;
    }
    return Text::PresetPlain;
}

// "2026-10-13-192033" -> "2026-10-13 19:20:33"
std::wstring backupLabel(const std::wstring& name) {
    if (name.size() != 17) return name;
    return name.substr(0, 10) + L" " + name.substr(11, 2) + L":" + name.substr(13, 2) + L":" + name.substr(15, 2);
}

// The Service page (stage 2 spec §8): presets, backups, the game's settings to a file and back, the window layout
// reset, uaRO Graphics' own reset and versions. While the game runs, what it writes itself waits (spec §4.1): those
// buttons are grey and their group's note says why.
class ServicePage : public Page {
public:
    explicit ServicePage(PageHost& host) : Page(host), rows_(makeSettingsPage(host, {})) {}

    void build(Panel& p) override {
        Model& m = host_.model();
        const bool running = m.now().gameRunning;
        const bool backups = !m.now().backups.empty();
        p.heading(t(Text::ServicePresets));
        std::vector<ButtonItem> presets;
        for (int i = 0; i < kPresetCount; ++i) presets.push_back({kPreset + i, t(presetName(Preset(i)))});
        p.buttons(presets);
        const auto now = m.preset();
        p.note(tf(Text::PresetNote, {utf8(now ? t(presetName(*now)) : t(Text::PresetCustom))}));
        // 0.55: profiles - every setting under a name
        p.gap();
        p.heading(t(Text::ServiceProfiles));
        p.note(t(Text::ProfileNote));
        const std::vector<std::wstring> profiles = m.profiles();
        profiles_ = p.listBox(kProfiles, 3);
        for (const std::wstring& name : profiles)
            SendMessageW(profiles_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name.c_str()));
        if (profiles.empty())
            SendMessageW(profiles_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(t(Text::ProfileNone).c_str()));
        p.buttons({{kProfileLoad, t(Text::ProfileLoad), !profiles.empty()},
                   {kProfileDelete, t(Text::ProfileDelete), !profiles.empty()}});
        p.line(kProfileName, t(Text::ProfileName), t(Text::ProfileNameCue), 40, t(Text::ProfileBadName));
        p.buttons({{kProfileSave, t(Text::ProfileSave)}});
        p.gap();
        p.heading(t(Text::ServiceBackups));
        list_ = p.listBox(kBackups, 3);
        for (const std::wstring& b : m.now().backups)
            SendMessageW(list_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(backupLabel(b).c_str()));
        if (!backups) SendMessageW(list_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(t(Text::BackupNone).c_str()));
        p.buttons({{kRestore, t(Text::BackupRestore), backups && !running},  // the game writes it too (spec §4.1)
                   {kKeepFive, t(Text::BackupPrune), backups}});
        if (running && backups)
            p.note(t(Text::ErrGameRunning), Tone::Warn);
        else
            p.note(t(Text::BackupNote));
        p.gap();
        p.heading(t(Text::ServiceGameSettings));
        p.buttons({{kGameSave, t(Text::GameSave)},
                   {kGameLoad, t(Text::GameLoad), !running},
                   {kLayoutReset, t(Text::LayoutReset), !running}});
        if (running)
            p.note(t(Text::GameSettingsRunning), Tone::Warn);
        else
            p.note(t(Text::GameSettingsNote));
        // 2.5.19: Palettes.grf with the palettes the server lets be worn
        const PalettesState made = palettesState();
        making_ = made.kind == PalettesState::Working;
        p.gap();
        // 2.6: the Developer mode's
        if (host_.mode() == settings::Mode::Developer) {
            p.heading(t(Text::ServicePalettes));
            p.note(tf(Text::PalettesNote, {std::to_string(palettes::kMaxHairStyle),
                                           std::to_string(palettes::kMaxClothColour)}));
            p.buttons({{kPalettesFile, t(Text::PalettesFromFile), !making_},
                       {kPalettesFolder, t(Text::PalettesFromFolder), !making_}});
            if (making_) {
                p.note(t(Text::PalettesWorking));
            } else if (made.kind == PalettesState::Failed) {
                p.note(tf(Text::PalettesFailed, {made.error}), Tone::Bad);
            } else if (made.kind == PalettesState::Done) {
                char mb[32], s[32];
                std::snprintf(mb, sizeof mb, "%.1f", double(made.bytes) / 1e6);
                std::snprintf(s, sizeof s, "%.1f", made.seconds);
                p.note(tf(Text::PalettesDone,
                          {utf8(made.out), std::to_string(made.kept), std::to_string(made.blocks), mb, s}),
                       Tone::Good);
                std::vector<std::string> left;
                for (uint32_t n : made.left) left.push_back(std::to_string(n));
                p.note(tf(Text::PalettesLeft, left));
            }
            p.gap();
        }
        p.heading(t(Text::ServiceUaroGraphics));
        p.buttons({{kReset, t(Text::ResetButton)}});
        p.note(tf(Text::VersionsValue,
                  {kVersion, m.now().installedVersion.empty() ? utf8(t(Text::Unknown)) : m.now().installedVersion}),
               Tone::Normal);
        // 0.49: the newest version; 2.10: from uaro.kiev.ua, put in by this app's own modes (app/modes) - and the
        // previous version back, the files repaired, uaRO Graphics removed.
        p.gap();
        p.heading(t(Text::ServiceUpdate));
        // 3.0.0-beta.2: the channel - the testers' builds or the releases (not in the Simple mode: «Гра» has it)
        if (host_.mode() != settings::Mode::Simple) buildChannel(p, host_, kChannel);
        const UpdateState u = channelUpdate(m);
        checking_ = u.kind == UpdateState::Checking;
        const bool offered = u.kind == UpdateState::Done && !u.offer.empty();
        std::vector<ButtonItem> buttons{{kUpdateCheck, t(Text::UpdateCheck), !checking_}};
        if (offered) buttons.push_back({kUpdateNow, t(Text::UpdateNow)});
        p.buttons(buttons);
        if (u.kind == UpdateState::Checking)
            p.note(t(Text::UpdateChecking));
        else if (u.kind == UpdateState::Failed)
            p.note(tf(Text::UpdateFailed, {u.error}), Tone::Bad);
        else if (offered)
            p.note(tf(Text::UpdateNewer, {u.offer, kVersion}), Tone::Warn);
        else if (u.kind == UpdateState::Done)
            p.note(tf(Text::UpdateLatest, {kVersion}), Tone::Good);
        if (running && offered) p.note(t(Text::UpdateGameRunning), Tone::Warn);
        p.note(t(Text::UpdateNote));
        std::vector<ButtonItem> more;
        if (installer::canRollBack(m.paths().dir))
            more.push_back({kRollback, tf(Text::RollbackButton, {installer::previousVersion(m.paths().dir)})});
        more.push_back({kRepair, t(Text::RepairButton)});
        more.push_back({kUninstall, t(Text::UninstallButton)});
        p.buttons(more);
        // 3.0.1-beta.5: our own libraries in place of the game's old ones, each switchable (core/client_libs.hpp)
        if (host_.mode() != settings::Mode::Simple) {
            p.gap();
            p.heading(t(Text::ServiceClientLibs));
            p.note(t(Text::ClientLibsNote));
            for (const char* key : {"client.cps", "client.bink", "client.ijl"}) rows_->buildRow(p, key);
        }
        // 2.12: when the game does not start - the safe mode, or uaRO Graphics switched off (the game's own look)
        p.gap();
        p.heading(t(Text::TroubleHeading));
        const bool safe = m.safeModeNoted() || m.now().config.safeMode;
        const bool off = m.now().config.api == Api::System;
        p.buttons({{kSafeToggle, t(safe ? Text::SafeModeOff : Text::RenderSafeMode)},
                   {kSwitchToggle, t(off ? Text::SwitchOn : Text::SwitchOff)}});
        p.note(t(Text::RenderSafeModeHint));
        p.note(t(Text::SwitchOffNote));
    }

    bool ticks() const override {
        return updateState().kind == UpdateState::Checking || palettesState().kind == PalettesState::Working;
    }
    void tick() override {
        const bool checking = updateState().kind == UpdateState::Checking;
        const bool making = palettesState().kind == PalettesState::Working;
        if ((checking_ && !checking) || (making_ && !making)) host_.rebuild();  // the answer came, or the file
        checking_ = checking;
        making_ = making;
    }

    void command(int id, int code) override {
        if (settingOfId(id)) {  // 3.0.1-beta.5: the libraries' switches
            rows_->command(id, code);
            return;
        }
        if (channelCommand(host_, id, code, kChannel)) return;  // 3.0.0-beta.2
        if (code != BN_CLICKED) return;
        Model& m = host_.model();
        const Lang l = host_.lang();
        if (id >= kPreset && id < kPreset + kPresetCount) {
            m.applyPreset(Preset(id - kPreset));
            host_.changed();
            host_.rebuild();
        } else if (id == kRestore) {
            const LRESULT sel = SendMessageW(list_, LB_GETCURSEL, 0, 0);
            if (sel < 0 || size_t(sel) >= m.now().backups.size()) return;
            const std::wstring name = m.now().backups[size_t(sel)];
            if (!host_.ask(tf(Text::BackupAsk, {utf8(backupLabel(name))}))) return;
            const std::wstring err = m.restore(name, l);
            if (!err.empty()) host_.message(err, true);
            host_.reload();
        } else if (id == kProfileSave) {
            const std::wstring name = wide(controlText(GetDlgItem(host_.window(), kProfileName)));
            const std::wstring err = m.saveProfile(name, l);
            host_.message(err.empty() ? tf(Text::ProfileSaved, {utf8(name)}) : err, !err.empty());
            if (err.empty()) host_.rebuild();
        } else if (id == kProfileLoad || id == kProfileDelete) {
            const std::vector<std::wstring> profiles = m.profiles();
            const LRESULT sel = SendMessageW(profiles_, LB_GETCURSEL, 0, 0);
            if (sel < 0 || size_t(sel) >= profiles.size()) return;
            const std::wstring name = profiles[size_t(sel)];
            if (id == kProfileDelete) {
                if (!host_.ask(tf(Text::ProfileAskDelete, {utf8(name)}))) return;
                const std::wstring err = m.deleteProfile(name, l);
                if (!err.empty()) host_.message(err, true);
                host_.rebuild();
                return;
            }
            if (!host_.ask(tf(Text::ProfileAskLoad, {utf8(name)}))) return;
            const std::wstring err = m.loadProfile(name, l);
            host_.message(err.empty() ? tf(Text::ProfileLoaded, {utf8(name)}) : err, !err.empty());
            host_.reload();
        } else if (id == kKeepFive) {
            m.keepNewestBackups(5);
            host_.reload();
        } else if (id == kUpdateCheck) {
            startUpdateCheck();
            host_.rebuild();
        } else if (id == kUpdateNow) {
            updateAndReopen(host_, true);
        } else if (id == kRollback) {
            startAndClose(Args::Mode::Rollback, tf(Text::RollbackAsk, {installer::previousVersion(m.paths().dir)}));
        } else if (id == kRepair) {
            startAndClose(Args::Mode::Repair, t(Text::RepairAsk));
        } else if (id == kUninstall) {
            startAndClose(Args::Mode::Uninstall, L"");  // its own window asks
        } else if (id == kSafeToggle || id == kSwitchToggle) {
            const bool safe = m.safeModeNoted() || m.now().config.safeMode;
            const bool off = m.now().config.api == Api::System;
            const std::wstring err = id == kSafeToggle ? m.setSafeMode(!safe, l) : m.setSwitchedOff(!off, l);
            if (!err.empty()) host_.message(err, true);
            host_.reload();
        } else if (id == kReset) {
            if (!host_.ask(t(Text::ResetAsk))) return;
            const std::wstring err = m.resetDefaults(l);
            if (!err.empty()) host_.message(err, true);
            host_.reload();
        } else if (id == kGameSave) {
            const std::wstring file = host_.pickFile(true);
            if (file.empty()) return;
            const std::wstring err = m.saveGameSettings(file, l);
            host_.message(err.empty() ? tf(Text::GameSaved, {utf8(file)}) : err, !err.empty());
        } else if (id == kGameLoad) {
            const std::wstring file = host_.pickFile(false);
            if (file.empty() || !host_.ask(tf(Text::GameLoadAsk, {utf8(file)}))) return;
            const std::wstring err = m.loadGameSettings(file, l);
            host_.message(err.empty() ? t(Text::GameLoaded) : err, !err.empty());
            host_.reload();
        } else if (id == kPalettesFile || id == kPalettesFolder) {  // 2.5.19
            std::wstring source;
            if (id == kPalettesFolder) {
                source = host_.pickFolder(t(Text::PalettesFolderTitle));
            } else {
                const std::wstring own = m.paths().dir + L"\\Palettes.grf";  // the game's, offered first
                source = host_.pickPath(false, t(Text::FileFilterGrfZip), L"*.grf;*.gpf;*.zip",
                                        fileExists(own) ? own : L"");
            }
            if (source.empty()) return;
            const std::wstring out = host_.pickPath(true, t(Text::FileFilterGrf), L"*.grf", L"Palettes.grf");
            if (out.empty()) return;
            if (samePath(source, out)) {
                host_.message(t(Text::PalettesSameFile), true);
                return;
            }
            startPalettes(source, id == kPalettesFolder, out);
            host_.rebuild();
        } else if (id == kLayoutReset) {
            if (!host_.ask(t(Text::LayoutResetAsk))) return;
            const std::wstring err = m.resetWindowLayout(l);
            host_.message(err.empty() ? t(Text::LayoutResetDone) : err, !err.empty());
            host_.reload();
        }
    }

private:
    // 2.10: a mode of this exe as a process of its own (app/modes); the app closes: the files it runs from change.
    void startAndClose(Args::Mode mode, const std::wstring& ask) {
        if (!ask.empty() && !host_.ask(ask)) return;
        Args a;
        a.mode = mode;
        a.then = mode == Args::Mode::Uninstall ? Args::Then::Nothing : Args::Then::App;
        const std::string err = startMode(a);
        if (!err.empty()) {
            host_.message(tf(Text::UpdateError, {err}), true);
            return;
        }
        PostMessageW(GetAncestor(host_.window(), GA_ROOT), WM_CLOSE, 0, 0);
    }

    std::unique_ptr<Page> rows_;  // 3.0.1-beta.5: the schema's rows (the libraries' switches)
    HWND list_ = nullptr;
    HWND profiles_ = nullptr;
    bool checking_ = false, making_ = false;  // at the last look: what ended since is shown anew
};

}  // namespace

std::unique_ptr<Page> makeServicePage(PageHost& host) { return std::make_unique<ServicePage>(host); }

void buildChannel(Panel& p, PageHost& host, int id) {
    using settings::Text;
    const settings::Lang l = host.lang();
    p.choice(id, wtr(l, Text::UpdateChannel), {wtr(l, Text::UpdateChannelRelease), wtr(l, Text::UpdateChannelBeta)},
             host.model().channel() == update::Channel::Beta ? 1 : 0, wtr(l, Text::UpdateChannelHint));
}

bool channelCommand(PageHost& host, int id, int code, int channelId) {
    if (id != channelId || (code != CBN_SELCHANGE && code != CBN_CLOSEUP)) return false;
    HWND ctl = GetDlgItem(host.window(), id);
    const LRESULT sel = SendMessageW(ctl, CB_GETCURSEL, 0, 0);
    const update::Channel c = sel == 1 ? update::Channel::Beta : update::Channel::Release;
    if (code == CBN_SELCHANGE && sel >= 0 && c != host.model().channel()) {
        const std::wstring err = host.model().setChannel(c, host.lang());
        if (!err.empty()) host.message(err, true);
        startUpdateCheck();
    }
    if (code == CBN_CLOSEUP || !SendMessageW(ctl, CB_GETDROPPEDSTATE, 0, 0)) host.rebuild();
    return true;
}

void updateAndReopen(PageHost& host, bool ask) {
    if (ask && !host.ask(wtr(host.lang(), settings::Text::UpdateAsk))) return;
    Args a;
    a.mode = Args::Mode::Update;
    a.then = Args::Then::App;
    const std::string err = startMode(a);
    if (!err.empty()) {
        host.message(wtrf(host.lang(), settings::Text::UpdateError, {err}), true);
        return;
    }
    PostMessageW(GetAncestor(host.window(), GA_ROOT), WM_CLOSE, 0, 0);
}

UpdateState channelUpdate(const Model& m) {
    const UpdateState u = updateState();
    if ((u.kind != UpdateState::Done && u.kind != UpdateState::Failed) || u.channel == m.channel()) return u;
    startUpdateCheck();  // the last check asked the other channel's branch
    return updateState();
}

}  // namespace uagfx::app

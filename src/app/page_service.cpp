#include "app/page.hpp"
#include "core/settings/presets.hpp"
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
    explicit ServicePage(PageHost& host) : Page(host) {}

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
        p.gap();
        p.heading(t(Text::ServiceUaroGraphics));
        p.buttons({{kReset, t(Text::ResetButton)}});
        p.note(tf(Text::VersionsValue,
                  {kVersion, m.now().installedVersion.empty() ? utf8(t(Text::Unknown)) : m.now().installedVersion}),
               Tone::Normal);
    }

    void command(int id, int code) override {
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
        } else if (id == kKeepFive) {
            m.keepNewestBackups(5);
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
        } else if (id == kLayoutReset) {
            if (!host_.ask(t(Text::LayoutResetAsk))) return;
            const std::wstring err = m.resetWindowLayout(l);
            host_.message(err.empty() ? t(Text::LayoutResetDone) : err, !err.empty());
            host_.reload();
        }
    }

private:
    HWND list_ = nullptr;
};

}  // namespace

std::unique_ptr<Page> makeServicePage(PageHost& host) { return std::make_unique<ServicePage>(host); }

}  // namespace uagfx::app

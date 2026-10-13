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

// The Service page (stage 2 spec §8, 0.6.0 part): presets, backups, reset, versions.
class ServicePage : public Page {
public:
    explicit ServicePage(PageHost& host) : Page(host) {}

    void build(Panel& p) override {
        Model& m = host_.model();
        p.heading(t(Text::ServicePresets));
        std::vector<std::pair<int, std::wstring>> presets;
        for (int i = 0; i < kPresetCount; ++i) presets.push_back({kPreset + i, t(presetName(Preset(i)))});
        p.buttons(presets);
        const auto now = presetOf(m.pendingConfig());
        p.note(tf(Text::PresetNote, {utf8(now ? t(presetName(*now)) : t(Text::PresetCustom))}));
        p.gap();
        p.heading(t(Text::ServiceBackups));
        list_ = p.listBox(kBackups, 4);
        for (const std::wstring& b : m.now().backups)
            SendMessageW(list_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(backupLabel(b).c_str()));
        if (m.now().backups.empty()) p.note(t(Text::BackupNone));
        p.buttons({{kRestore, t(Text::BackupRestore)}, {kKeepFive, t(Text::BackupPrune)}}, !m.now().backups.empty());
        p.note(t(Text::BackupNote));
        p.gap();
        p.heading(t(Text::ServiceReset));
        p.buttons({{kReset, t(Text::ResetButton)}});
        p.gap();
        p.heading(t(Text::ServiceVersions));
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
            const std::wstring ask = tf(Text::BackupAsk, {utf8(backupLabel(name))});
            if (MessageBoxW(host_.window(), ask.c_str(), t(Text::AppTitle).c_str(), MB_YESNO | MB_ICONQUESTION) != IDYES)
                return;
            const std::wstring err = m.restore(name, l);
            if (!err.empty()) host_.message(err, true);
            host_.reload();
        } else if (id == kKeepFive) {
            m.keepNewestBackups(5);
            host_.reload();
        } else if (id == kReset) {
            if (MessageBoxW(host_.window(), t(Text::ResetAsk).c_str(), t(Text::AppTitle).c_str(),
                            MB_YESNO | MB_ICONQUESTION) != IDYES)
                return;
            const std::wstring err = m.resetDefaults(l);
            if (!err.empty()) host_.message(err, true);
            host_.reload();
        }
    }

private:
    HWND list_ = nullptr;
};

}  // namespace

std::unique_ptr<Page> makeServicePage(PageHost& host) { return std::make_unique<ServicePage>(host); }

}  // namespace uagfx::app

#include <cstdio>

#include "app/page.hpp"
#include "app/setting_ids.hpp"
#include "app/status_info.hpp"
#include "app/update.hpp"
#include "core/settings/game_settings.hpp"
#include "core/settings/presets.hpp"
#include "core/update_check.hpp"
#include "core/version.hpp"

namespace uagfx::app {
namespace {

using namespace settings;

constexpr int kQuality = 2001;
constexpr int kRecommended = 2003;
constexpr int kWhatsNew = 2004;
constexpr int kUpdateCheck = 2005;
constexpr int kUpdateNow = 2006;
constexpr int kMakePack = 2007;
constexpr int kAiMore = 2008;

const Text kQualities[kMainQualityCount] = {Text::MainQualityLow, Text::MainQualityMedium, Text::MainQualityHigh,
                                            Text::MainQualitySuperLight};

std::string firstWord(const std::string& s) { return s.substr(0, s.find(' ')); }  // "2.5.28 build ..." -> "2.5.28"

// 2.6: the Simple mode's Main page - the picture's quality in one choice and the rows a player looks for first (made
// by a page of rows of the schema inside), the state of the driver, the card and the game, the HD textures.
class MainPage : public Page {
public:
    explicit MainPage(PageHost& host) : Page(host), rows_(makeSettingsPage(host, {})) {}

    void build(Panel& p) override {
        Model& m = host_.model();
        p.heading(t(Text::ViewMain));
        p.note(t(Text::MainIntro), Tone::Normal);
        const bool two = p.split();
        p.heading(t(Text::MainPicture));
        const auto quality = mainQualityOf(m.pendingConfig());
        std::vector<std::wstring> items;
        for (Text q : kQualities) items.push_back(t(q));
        if (!quality) items.push_back(t(Text::MainQualityOwn));  // none of them: the player's own
        p.choice(kQuality, t(Text::PageQuality), items, quality ? int(*quality) : kMainQualityCount,
                 t(Text::MainQualityHint));
        for (const char* key : {"quality.dlss", "frames.frame_generation", "frames.fps_limit", "frames.vsync",
                                "display.hdr"})
            rows_->buildRow(p, key);
        p.note(t(Text::AfterRestart));
        p.gap();
        p.heading(t(Text::MainOther));
        rows_->buildRow(p, "sound.engine");
        rows_->buildRow(p, "pad.enabled");  // 2.9: uaRO Pad inside the driver, on by default
        rows_->buildRow(p, "frames.battery_saver");
        p.buttons({{kRecommended, t(Text::MainRecommended)}, {kWhatsNew, tf(Text::MainWhatsNew, {kVersion})}});
        if (two)
            p.nextColumn();
        else
            p.gap(16);
        buildState(p);
        p.gap();
        buildTextures(p);
        p.join();
    }

    bool ticks() const override { return checking_; }
    void tick() override {
        if (updateState().kind != UpdateState::Checking) host_.rebuild();  // the check's answer
    }

    void command(int id, int code) override {
        if (settingOfId(id)) {
            rows_->command(id, code);
            return;
        }
        Model& m = host_.model();
        HWND ctl = GetDlgItem(host_.window(), id);
        if (id == kQuality && (code == CBN_SELCHANGE || code == CBN_CLOSEUP)) {
            const LRESULT sel = SendMessageW(ctl, CB_GETCURSEL, 0, 0);
            if (code == CBN_SELCHANGE && sel >= 0 && sel < kMainQualityCount) setQuality(MainQuality(sel));
            if (code == CBN_CLOSEUP || !SendMessageW(ctl, CB_GETDROPPEDSTATE, 0, 0)) host_.rebuild();
            return;
        }
        if (code != BN_CLICKED) return;
        switch (id) {
            case kRecommended: runAutoSetup(host_); return;
            case kWhatsNew: {
                const std::wstring notes = notesText(m.paths().dir, host_.lang());
                if (notes.empty())
                    host_.message(tf(Text::NotesNone, {kVersion}), false);
                else
                    host_.showNotes(notes);
                return;
            }
            case kUpdateCheck:
                startUpdateCheck();
                host_.rebuild();
                return;
            case kUpdateNow: {  // as the Service page's
                if (m.now().gameRunning) {
                    host_.message(t(Text::UpdateGameRunning), true);
                    return;
                }
                if (!host_.ask(t(Text::UpdateAsk))) return;
                const std::string err = startUpdate(m.paths().dir);
                if (!err.empty()) {
                    host_.message(tf(Text::UpdateError, {err}), true);
                    return;
                }
                PostMessageW(GetAncestor(host_.window(), GA_ROOT), WM_CLOSE, 0, 0);  // update.bat replaces this exe
                return;
            }
            case kMakePack: host_.showView(View::Ai, 2); return;
            case kAiMore: host_.showView(View::Ai, 0); return;
            default: return;
        }
    }

    void scroll(int id, int code, int pos) override { rows_->scroll(id, code, pos); }

private:
    // The presets' values (or the super-light mode) as the player's changes; the game's own quality with a preset.
    void setQuality(MainQuality q) {
        Model& m = host_.model();
        for (const IniEdit& e : mainQualityEdits(q))
            if (const IniSetting* s = findSetting(e.section + "." + e.key)) m.setToken(*s, e.value);
        if (q != MainQuality::SuperLight) {
            const Preset p = q == MainQuality::Low ? Preset::Speed : q == MainQuality::Medium ? Preset::Balance
                                                                                               : Preset::Maximum;
            for (const auto& [name, word] : presetRegWords(p))
                if (const RegSetting* r = findRegSetting(name)) m.setRegWord(*r, word);
        }
        host_.changed();
    }

    void buildState(Panel& p) {
        Model& m = host_.model();
        p.heading(t(Text::MainState));
        const std::string installed =
            m.now().installedVersion.empty() ? std::string(kVersion) : firstWord(m.now().installedVersion);
        const UpdateState u = updateState();
        checking_ = u.kind == UpdateState::Checking;
        const bool newer = u.kind == UpdateState::Done && update::newer(u.latest, kVersion);
        std::wstring line = tf(Text::MainDriver, {installed});
        if (checking_) line += L" · " + t(Text::UpdateChecking);
        if (newer) line += L" · " + tf(Text::MainUpdateNew, {u.latest});
        if (u.kind == UpdateState::Done && !newer) line += L" · " + t(Text::MainUpdateNone);
        p.note(line, newer ? Tone::Warn : Tone::Normal);
        if (u.kind == UpdateState::Failed) p.note(tf(Text::UpdateFailed, {u.error}), Tone::Bad);
        std::vector<ButtonItem> buttons{{kUpdateCheck, t(Text::UpdateCheck), !checking_}};
        if (newer) buttons.push_back({kUpdateNow, t(Text::UpdateNow), !m.now().gameRunning});
        p.buttons(buttons);
        const Machine x = m.machine();
        const std::wstring api = x.d3d12 ? L"Direct3D 12" : x.d3d11 ? L"Direct3D 11" : t(Text::MainNoD3d);
        p.note(api + (x.adapter.empty() ? L"" : L" · " + wide(x.adapter)), x.d3d11 ? Tone::Normal : Tone::Bad);
        if (x.vendor == 0x10de) {  // NVIDIA: DLSS and its frame generation
            const bool dlss = dlssInstalled(m.paths().dir);
            p.note(t(dlss ? Text::MainDlss : Text::MainNoDlss), dlss ? Tone::Good : Tone::Muted);
        }
        p.note(t(m.now().gameRunning ? Text::StatusRunning : Text::StatusClosed));
    }

    void buildTextures(Panel& p) {
        Model& m = host_.model();
        p.heading(t(Text::MainHd));
        rows_->buildRow(p, "ai.enabled");
        const PackSummary s = packSummary(m.paths().dir);
        if (!s.exists)
            p.note(t(Text::MainNoPack));
        else if (s.counted)
            p.note(tf(Text::MainPack, {num((long long)s.pictures), sizeText(s.bytes, host_.lang())}));
        else
            p.note(tf(Text::PackWriting, {sizeText(s.bytes, host_.lang())}));
        p.buttons({{kMakePack, t(Text::MainMakePack)}, {kAiMore, t(Text::MainMore)}});
    }

    std::unique_ptr<Page> rows_;  // the schema's rows, built and changed as their pages do
    bool checking_ = false;      // this build saw the update check under way
};

}  // namespace

std::unique_ptr<Page> makeMainPage(PageHost& host) { return std::make_unique<MainPage>(host); }

}  // namespace uagfx::app

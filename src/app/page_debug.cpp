#include "app/files.hpp"
#include "app/page.hpp"
#include "app/setting_ids.hpp"

namespace uagfx::app {
namespace {

using namespace settings;

// Over the game key's rows (1500 + 4 x 41) and under the schema's (4000), the Main page's (2001-2008) apart.
constexpr int kOpenLog = 2101;
constexpr int kOpenFolder = 2102;
constexpr int kStudio = 2103;
constexpr int kCompact = 2104;
constexpr int kPalettes = 2105;
constexpr int kPreview = 2106;

// 2.6: the Developer mode's page - the switches for checks and reports (the Debug rows of the schema, a page of rows
// of their own inside), the log, and the tools: uaRO Studio, and where Compact, Palettes.grf and the models' preview
// are.
class DebugPage : public Page {
public:
    explicit DebugPage(PageHost& host) : Page(host), rows_(makeSettingsPage(host, {settings::Page::Debug})) {}

    void build(Panel& p) override {
        rows_->build(p);
        const Model& m = host_.model();
        p.buttons({{kOpenLog, t(Text::DiagOpenLog), m.now().log.found}, {kOpenFolder, t(Text::DiagOpenFolder)}});
        p.gap(16);
        p.heading(t(Text::DebugTools));
        p.buttons({{kStudio, t(Text::DebugStudio), fileExists(studio())},
                   {kCompact, t(Text::DebugCompact)},
                   {kPalettes, t(Text::DebugPalettes)},
                   {kPreview, t(Text::DebugPreview)}});
        p.note(t(Text::DebugKeys));
    }

    void command(int id, int code) override {
        if (settingOfId(id)) {
            rows_->command(id, code);
            return;
        }
        if (code != BN_CLICKED) return;
        const Model& m = host_.model();
        switch (id) {
            case kOpenLog: openInShell(m.paths().log); break;
            case kOpenFolder: openInShell(m.paths().dir + L"\\uaro-graphics"); break;
            case kStudio: openInShell(studio()); break;
            case kCompact: host_.showView(View::Ai, 2); break;  // the pack's tab
            case kPalettes: host_.showView(View::Service, 0); break;
            case kPreview: host_.showView(View::Ai, 1); break;  // the models' tab
            default: break;
        }
    }

    void scroll(int id, int code, int pos) override { rows_->scroll(id, code, pos); }

private:
    std::wstring studio() const { return host_.model().paths().dir + L"\\uaro-graphics\\uaro-studio.exe"; }

    std::unique_ptr<Page> rows_;
};

}  // namespace

std::unique_ptr<Page> makeDebugPage(PageHost& host) { return std::make_unique<DebugPage>(host); }

}  // namespace uagfx::app

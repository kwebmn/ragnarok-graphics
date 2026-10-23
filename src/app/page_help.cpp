#include <string>

#include "app/page.hpp"
#include "core/settings/help.hpp"

namespace uagfx::app {
namespace {

using namespace settings;

// 3.0.2-beta.6 (the user: a description of every function, so that it is clear what each is): «Довідка» - what the
// program does, the game's keys, then every view of the mode with its functions and their hints (the same ones the
// rows show under the mouse on their pages, core/settings/help.hpp)
class HelpPage : public Page {
public:
    explicit HelpPage(PageHost& host) : Page(host) {}

    void build(Panel& p) override {
        const Mode mode = host_.mode();
        p.heading(t(Text::ViewHelp));
        p.note(t(Text::HelpIntro), Tone::Normal);
        p.gap();
        p.heading(t(Text::HelpFeatures));
        p.note(t(Text::HelpFeaturesList), Tone::Normal);
        p.gap();
        p.heading(t(Text::MainKeys));
        p.note(t(Text::MainKeysList), Tone::Normal);
        for (View v : viewsOf(mode)) {
            if (v == View::Help) continue;
            std::wstring rows;
            for (const HelpRow& r : helpRows(v, mode, host_.model().machine()))
                rows += (rows.empty() ? L"• " : L"\n• ") + t(r.label) + (r.hint == r.label ? L"" : L" — " + t(r.hint));
            p.gap();
            p.heading(t(viewName(v)));
            p.note(t(helpOf(v)), Tone::Normal);
            if (v == View::Effects) p.note(t(Text::HelpEffectsMore));
            if (!rows.empty()) p.note(rows, Tone::Normal);
        }
    }
};

}  // namespace

std::unique_ptr<Page> makeHelpPage(PageHost& host) { return std::make_unique<HelpPage>(host); }

}  // namespace uagfx::app

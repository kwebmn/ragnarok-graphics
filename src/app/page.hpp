#pragma once
#include <windows.h>

#include <commctrl.h>

#include <memory>
#include <string>

#include "app/model.hpp"
#include "app/text.hpp"
#include "app/ui.hpp"
#include "app/update.hpp"
#include "core/settings/navigation.hpp"
#include "core/settings/schema.hpp"

namespace uagfx::app {

// What a page asks of the main window.
class PageHost {
public:
    virtual ~PageHost() = default;
    virtual Model& model() = 0;
    virtual settings::Lang lang() const = 0;
    virtual HWND window() const = 0;  // the window the page's controls are children of (GetDlgItem)
    virtual void changed() = 0;  // the player's changes changed: the status line and the buttons
    virtual void rebuild() = 0;  // the page again: it shows values that depend on others
    virtual void reload() = 0;   // the stores were written: everything is read again and every page rebuilt
    // A message box. The page stays until it is closed: its code still runs under the box.
    virtual void message(const std::wstring& text, bool error) = 0;
    virtual bool ask(const std::wstring& text) = 0;  // Yes or No
    // A .reg file to save the game's settings to (save) or to load them from; "" when the player cancels.
    virtual std::wstring pickFile(bool save) = 0;
    // 2.5.19: a file of another kind - pattern "*.grf;*.zip" under filterName, offered: the name (or path) the
    // dialog starts with - and a folder; "" when the player cancels.
    virtual std::wstring pickPath(bool save, const std::wstring& filterName, const std::wstring& pattern,
                                  const std::wstring& offered) = 0;
    virtual std::wstring pickFolder(const std::wstring& title) = 0;
    // 1.6: another page, once the page's code has returned (the AI page's "Make again" opens the Texture pack page).
    // 2.6: where the page is now (the pack's tab of the AI view); showView a view and its tab.
    virtual void showPage(settings::Page which) = 0;
    virtual void showView(settings::View view, int tab) = 0;
    // 2.6: the level of the rows the pages show - the mode, the Detailed one for a page opened off the mode's list
    virtual settings::Mode mode() const = 0;
    // 2.6: a long text in a window of its own that scrolls ("What's new"); the page stays under it like a box's
    virtual void showNotes(const std::wstring& text) = 0;
    // 2.11: the one way to the game (the bottom row's «Застосувати й грати», «Гра»'s «Грати»): the changes applied,
    // then an offered update first, then the game
    virtual void play() = 0;
    // 2.13: the window «Звернення» (app/reports_window), with the new report's form at once (fresh)
    virtual void openReports(bool fresh) = 0;
};

// One page of the list on the left (stage 2 spec §6.1). Its controls' ids start at 1000.
class Page {
public:
    explicit Page(PageHost& host) : host_(host) {}
    virtual ~Page() = default;
    virtual void build(Panel& p) = 0;
    virtual void command(int id, int code) {
        (void)id;
        (void)code;
    }
    virtual void scroll(int id, int code, int pos) {
        (void)id;
        (void)code;
        (void)pos;
    }
    virtual LRESULT notify(const NMHDR* n) {
        (void)n;
        return 0;
    }
    // 2.6: the control of the setting `key` ("section.key" of the ini) as this page makes it, alone - for the Main
    // page and the search; false: not one of this page's.
    virtual bool buildRow(Panel& p, const std::string& key) {
        (void)p;
        (void)key;
        return false;
    }
    // 0.43: a page that shows work going on elsewhere (the texture pack's builder) - tick() twice a second.
    virtual bool ticks() const { return false; }
    virtual void tick() {}

protected:
    std::wstring t(settings::Text x) const { return wtr(host_.lang(), x); }
    std::wstring tf(settings::Text x, const std::vector<std::string>& args) const {
        return wtrf(host_.lang(), x, args);
    }
    PageHost& host_;
};

// The pages of the schema's rows and the game's key (Render, Quality, Frames...). 2.6: several on one page, the right
// ones in a second column when the page is wide; the rows above the host's mode left out.
std::unique_ptr<Page> makeSettingsPage(PageHost& host, std::vector<settings::Page> left,
                                       std::vector<settings::Page> right = {});
// 2.6: pages one under another; a control's command goes to the page that made it.
std::unique_ptr<Page> makePartsPage(PageHost& host, std::vector<std::unique_ptr<Page>> parts);
std::unique_ptr<Page> makeScreenPage(PageHost& host);
std::unique_ptr<Page> makeDiagnosticsPage(PageHost& host);
std::unique_ptr<Page> makeServicePage(PageHost& host);
// 0.12: the [ai] section; 2.6: on two tabs - what the network improves, and the models by group
enum class AiPart { What, Models };
std::unique_ptr<Page> makeAiPage(PageHost& host, AiPart part);
std::unique_ptr<Page> makeAiCachePage(PageHost& host);  // 0.12: what the texture pack holds (0.48)
std::unique_ptr<Page> makeEffectsPage(PageHost& host);  // 0.18: the [effects] section
std::unique_ptr<Page> makePackPage(PageHost& host);     // 0.43: the texture pack's builder
std::unique_ptr<Page> makePadPage(PageHost& host);      // 1.4: uaRO Pad, the gamepad
std::unique_ptr<Page> makeDebugPage(PageHost& host);    // 2.6: the Developer mode's switches and tools
std::unique_ptr<Page> makeMainPage(PageHost& host);     // 2.6: the Simple mode's Main page
std::unique_ptr<Page> makePlayPage(PageHost& host);     // 2.11: the launcher «Гра»
// 2.12: what the auto setup picked, in words (page_settings.cpp) - the Main page's question, «Гра»'s card
std::wstring autoSetupText(const settings::Machine& x, const settings::AutoSetup& a, bool uiChanged,
                           settings::Lang l);

// 2.6: "Recommended settings" of the Main page - the Render page's auto setup for this computer.
void runAutoSetup(PageHost& host);
// 3.0.0-beta.2: the update channel's choice («Реліз», «Бета») as control `id` - the Service page's, and 3.0.1-beta.2
// «Гра»'s (the user: on the first page); channelCommand takes its commands: a change is recorded and its branch asked
// at once (true: it was the choice's). channelUpdate: the check's state, asked again when the last one asked the
// other channel's branch.
void buildChannel(Panel& p, PageHost& host, int id);
bool channelCommand(PageHost& host, int id, int code, int channelId);
UpdateState channelUpdate(const Model& m);
// 2.6: the search's results - the rows changed in place, each with a button to its page.
std::unique_ptr<Page> makeSearchPage(PageHost& host, const std::string& query);
settings::Text effectGroupName(int g);  // the Effects page's name of a group

// 1.6: the Texture pack page's "make again what another model made", ticked from the AI page.
bool& packRedo();

// The window text of a control, as UTF-8 (number fields).
std::string controlText(HWND ctl);

}  // namespace uagfx::app

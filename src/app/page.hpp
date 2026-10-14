#pragma once
#include <windows.h>

#include <commctrl.h>

#include <memory>
#include <string>

#include "app/model.hpp"
#include "app/text.hpp"
#include "app/ui.hpp"
#include "core/settings/schema.hpp"

namespace uagfx::app {

// What a page asks of the main window.
class PageHost {
public:
    virtual ~PageHost() = default;
    virtual Model& model() = 0;
    virtual settings::Lang lang() const = 0;
    virtual HWND window() const = 0;
    virtual void changed() = 0;  // the player's changes changed: the status line and the buttons
    virtual void rebuild() = 0;  // the page again: it shows values that depend on others
    virtual void reload() = 0;   // the stores were written: everything is read again and every page rebuilt
    // A message box. The page stays until it is closed: its code still runs under the box.
    virtual void message(const std::wstring& text, bool error) = 0;
    virtual bool ask(const std::wstring& text) = 0;  // Yes or No
    // A .reg file to save the game's settings to (save) or to load them from; "" when the player cancels.
    virtual std::wstring pickFile(bool save) = 0;
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

protected:
    std::wstring t(settings::Text x) const { return wtr(host_.lang(), x); }
    std::wstring tf(settings::Text x, const std::vector<std::string>& args) const {
        return wtrf(host_.lang(), x, args);
    }
    PageHost& host_;
};

std::unique_ptr<Page> makeSettingsPage(PageHost& host, settings::Page which);  // Render, Quality, Frames
std::unique_ptr<Page> makeScreenPage(PageHost& host);
std::unique_ptr<Page> makeDiagnosticsPage(PageHost& host);
std::unique_ptr<Page> makeServicePage(PageHost& host);
std::unique_ptr<Page> makeAiPage(PageHost& host);       // 0.12: the [ai] section
std::unique_ptr<Page> makeAiCachePage(PageHost& host);  // 0.12: what upscaled.grf holds

// The window text of a control, as UTF-8 (number fields).
std::string controlText(HWND ctl);

}  // namespace uagfx::app

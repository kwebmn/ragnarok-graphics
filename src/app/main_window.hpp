#pragma once
#include <windows.h>

#include <memory>
#include <string>
#include <vector>

#include "app/model.hpp"
#include "app/page.hpp"
#include "app/rebuilds.hpp"
#include "app/reports_window.hpp"
#include "app/ui.hpp"

namespace uagfx::app {

// The settings app's window (stage 2 spec §6): the list of pages on the left, the page, the language at the top
// right, the status line and the buttons at the bottom.
class MainWindow : public PageHost {
public:
    int run(HINSTANCE inst, int show);

    Model& model() override { return *model_; }
    settings::Lang lang() const override { return lang_; }
    HWND window() const override { return view_; }  // the page view: the page's controls are its children
    void changed() override;
    void rebuild() override;
    void reload() override;
    void message(const std::wstring& text, bool error) override;
    bool ask(const std::wstring& text) override;
    std::wstring pickFile(bool save) override;
    std::wstring pickPath(bool save, const std::wstring& filterName, const std::wstring& pattern,
                          const std::wstring& offered) override;
    std::wstring pickFolder(const std::wstring& title) override;
    void showPage(settings::Page which) override;
    void showView(settings::View view, int tab) override;
    settings::Mode mode() const override;
    void showNotes(const std::wstring& text) override;
    void play() override { doApply(true); }
    void openReports(bool fresh) override;
    // 2.13: uaro-graphics.exe --report (the game's Ctrl+Shift+F10): «Нове звернення» with its shot, the window up
    void reportAtStart(const ReportStart& s) {
        reportStart_ = s;
        startReport_ = true;
    }

private:
    static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT handle(UINT msg, WPARAM wp, LPARAM lp);
    // 0.22.1, the page view: the page in a child window that scrolls it when it is taller than the window.
    static LRESULT CALLBACK viewProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT handleView(UINT msg, WPARAM wp, LPARAM lp);
    void scrollTo(int y);  // the view's top at y of the page
    void updateScrollBar();
    void showFocused();             // after Tab or Alt+letter: the focused control of the page in view
    bool wheelToPage(const MSG& m);  // a wheel turned over the page scrolls the page, not the slider under it
    void create();
    void makeFonts();
    void layout();
    void retext();
    void buildPage();
    void updateStatus();
    int box(const std::wstring& text, UINT type);
    bool confirmClose();
    void savePlace();       // 2.6
    int viewWidth() const;  // the page view's, in pixels
    // 2.6: the list of the mode's views (the "Developer only" heading before the Debug view), the mode switched
    void fillList();
    void setMode(settings::Mode m);
    bool listed(settings::View v) const;  // in the mode's list
    int contentTop() const;               // under the header (and the administrator's banner)
    void drawListItem(const DRAWITEMSTRUCT& d);
    void paint();
    int rowTop() const;  // the buttons' row
    std::unique_ptr<Page> makeView();  // the shown view's page (its tab's)
    void clearSearch();
    void updateStrip();  // 2.6: the status bar
    void resetView();
    std::wstring searchText() const;
    std::wstring windowLetters() const;  // Alt+letter of the window's own buttons
    void doApply(bool play);
    RECT pageArea() const;

    std::unique_ptr<Model> model_;
    HWND hwnd_ = nullptr;
    settings::Lang lang_ = settings::Lang::En;
    int dpi_ = 96;
    HFONT font_ = nullptr;
    HFONT heading_ = nullptr;
    HFONT pageFont_ = nullptr;  // the fonts the page was made with, after a DPI change: they go when it is made again
    HFONT pageHeading_ = nullptr;
    HWND view_ = nullptr;  // the page view (0.22.1)
    int scroll_ = 0;        // its top, in pixels of the page; kept while the page is made again
    int pageHeight_ = 0;
    HWND sidebar_ = nullptr;
    HWND langBox_ = nullptr;
    HWND banner_ = nullptr;
    HWND status_ = nullptr;
    HWND tooltip_ = nullptr;
    HWND apply_ = nullptr;
    HWND play_ = nullptr;
    HWND undo_ = nullptr;
    HWND close_ = nullptr;
    // 2.6: what is shown - the mode, its view (or one off its list) and the view's tab
    settings::Mode mode_ = settings::Mode::Detailed;
    settings::View shown_ = settings::View::Graphics;
    int tab_ = 0;
    std::vector<int> listItems_;  // the list's entries: a View, or -1 for the "Developer only" heading
    HWND modes_[settings::kModeCount] = {};
    HWND report_ = nullptr;     // 2.13: «Звернення» and the number of new answers
    HWND search_ = nullptr;     // the search's field; its text, once taken, shows the search's page
    std::wstring query_;
    int unread_ = -1;               // 2.13: the developers' answers not read yet (-1 unknown)
    bool unreadFromWindow_ = false;  // the window's count is newer than the start's check
    bool startReport_ = false;
    ReportStart reportStart_;
    void retextReports();
    HFONT bold_ = nullptr;      // the list's chosen entry
    int listSel_ = -1;          // the list's entry chosen before (past the heading the way the arrow went)
    HBRUSH listBrush_ = nullptr;
    HBRUSH lineBrush_ = nullptr;
    HBRUSH chosenBrush_ = nullptr;
    HBRUSH accentBrush_ = nullptr;
    HWND stripLeft_ = nullptr;  // 2.6: the status bar's two sides
    HWND stripRight_ = nullptr;
    HWND stripUpdate_ = nullptr;  // 2.10: "Update" in the status bar while a newer version is offered
    HBRUSH stripBrush_ = nullptr;
    bool newInstall_ = false;  // no notes of "What's new" on the first start
    std::unique_ptr<Panel> panel_;
    std::unique_ptr<Page> current_;
    Rebuilds rebuilds_;
    bool typing_ = false;  // a number field holds text it has not taken yet: Apply takes it first (0.6.0 review M5)
    std::wstring flash_;          // the last result in the status line, until the next change
    HANDLE statsThread_ = nullptr;  // 2.2.2: the day's statistics going - waited for (a while) at the end
    bool sizing_ = false;           // 2.6: the frame is being dragged
    int builtWidth_ = 0;            // the page view's width the page was made at
};

}  // namespace uagfx::app

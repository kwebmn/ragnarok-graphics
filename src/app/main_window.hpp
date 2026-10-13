#pragma once
#include <windows.h>

#include <memory>
#include <string>

#include "app/model.hpp"
#include "app/page.hpp"
#include "app/rebuilds.hpp"
#include "app/ui.hpp"

namespace uagfx::app {

// The settings app's window (stage 2 spec §6): the list of pages on the left, the page, the language at the top
// right, the status line and the buttons at the bottom.
class MainWindow : public PageHost {
public:
    int run(HINSTANCE inst, int show);

    Model& model() override { return *model_; }
    settings::Lang lang() const override { return lang_; }
    HWND window() const override { return hwnd_; }
    void changed() override;
    void rebuild() override;
    void reload() override;
    void message(const std::wstring& text, bool error) override;
    bool ask(const std::wstring& text) override;

private:
    static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT handle(UINT msg, WPARAM wp, LPARAM lp);
    void create();
    void makeFonts();
    void layout();
    void retext();
    void buildPage();
    void updateStatus();
    int box(const std::wstring& text, UINT type);
    bool confirmClose();
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
    HWND sidebar_ = nullptr;
    HWND langBox_ = nullptr;
    HWND banner_ = nullptr;
    HWND status_ = nullptr;
    HWND tooltip_ = nullptr;
    HWND apply_ = nullptr;
    HWND play_ = nullptr;
    HWND undo_ = nullptr;
    HWND close_ = nullptr;
    int page_ = 0;
    std::unique_ptr<Panel> panel_;
    std::unique_ptr<Page> current_;
    Rebuilds rebuilds_;
    bool typing_ = false;  // a number field holds text it has not taken yet: Apply takes it first (0.6.0 review M5)
    std::wstring flash_;          // the last result in the status line, until the next change
};

}  // namespace uagfx::app

#pragma once
#include <windows.h>

#include <memory>
#include <string>

#include "app/model.hpp"
#include "app/page.hpp"
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

private:
    static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT handle(UINT msg, WPARAM wp, LPARAM lp);
    void create();
    void makeFonts();
    void layout();
    void retext();
    void buildPage();
    void updateStatus();
    bool confirmClose();
    void doApply(bool play);
    RECT pageArea() const;

    std::unique_ptr<Model> model_;
    HWND hwnd_ = nullptr;
    settings::Lang lang_ = settings::Lang::En;
    int dpi_ = 96;
    HFONT font_ = nullptr;
    HFONT heading_ = nullptr;
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
    bool quiet_ = false;          // the page is being made or taken down: its notifications are not the player's
    bool rebuildPosted_ = false;
    std::wstring flash_;          // the last result in the status line, until the next change
};

}  // namespace uagfx::app

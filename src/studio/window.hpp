#pragma once
#include <windows.h>

#include <memory>
#include <string>

#include "core/settings/strings.hpp"
#include "core/studio_state.hpp"
#include "studio/catalog_work.hpp"
#include "studio/jobs.hpp"
#include "studio/textures_tab.hpp"
#include "studio/thumbs.hpp"

namespace uagfx::studio {

constexpr const wchar_t* kWindowClass = L"uaroStudio";  // a second studio finds the first by it

// 2.3, uaRO Studio's main window (the spec's "The window"): its tabs over one jobs bar - Pause or Resume, the engine,
// the job running with its progress and Stop.
class StudioWindow {
public:
    StudioWindow(std::wstring gameDir, settings::Lang lang) : gameDir_(std::move(gameDir)), lang_(lang) {}
    bool create(HINSTANCE instance, int show);
    HWND hwnd() const { return hwnd_; }

private:
    enum Id : int { kTabs = 100, kPauseButton, kEngineLabel, kJobLabel, kJobBar, kStopButton };
    static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT handle(UINT msg, WPARAM wp, LPARAM lp);
    std::wstring t(settings::Text x) const;
    void build();          // the controls
    void layout();         // their places at the window's size
    void showBar();        // the bar as the jobs and the engine are now
    void togglePause();
    void showTab(int tab);
    void loadSession();
    void saveSession();

    std::wstring gameDir_;
    settings::Lang lang_;
    HWND hwnd_ = nullptr;
    HWND tabs_ = nullptr, pause_ = nullptr, engineLabel_ = nullptr, jobLabel_ = nullptr, jobBar_ = nullptr,
         stop_ = nullptr;
    HFONT font_ = nullptr;
    int dpi_ = 96;
    Session session_;
    std::unique_ptr<Jobs> jobs_;
    std::unique_ptr<CatalogWork> catalog_;
    std::unique_ptr<Thumbs> thumbs_;
    std::unique_ptr<TexturesTab> textures_;
    int tab_ = 0;
};

}  // namespace uagfx::studio

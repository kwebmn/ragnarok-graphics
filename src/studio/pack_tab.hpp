#pragma once
#include <windows.h>

#include <atomic>
#include <memory>
#include <string>

#include "core/settings/strings.hpp"

namespace uagfx::studio {

class CatalogWork;
class Jobs;

// 2.3 (P1 in part), the Pack tab: the pack in numbers (pictures by group, model and kind; the pictures no texture
// names and the textures without one; its size and the space replaced or taken-out pictures hold) read on a thread,
// and Compact as a job.
class PackTab {
public:
    PackTab(HWND parent, settings::Lang lang, HFONT font, int dpi, CatalogWork& catalog, Jobs& jobs);
    ~PackTab();  // its count done first: it reads the catalog
    void show(bool on);
    void layout(const RECT& area);
    void refresh();  // the numbers again (on a thread: kPackReady)
    void ready(std::wstring* text);
    bool command(int id, int code);

private:
    enum Id : int { kText = 600, kRead, kCompact, kNote };
    std::wstring t(settings::Text x) const;

    HWND parent_;
    settings::Lang lang_;
    HFONT font_;
    int dpi_;
    CatalogWork& catalog_;
    Jobs& jobs_;
    HWND text_ = nullptr, read_ = nullptr, compact_ = nullptr, note_ = nullptr;
    bool busy_ = false;
    std::atomic<int> working_{0};
};

// 2.3, the Log tab: the engine's lines and what the jobs did, the newest last.
class LogTab {
public:
    LogTab(HWND parent, HFONT font);
    void show(bool on);
    void layout(const RECT& area);
    void add(const std::wstring& line);

private:
    HWND edit_ = nullptr;
    size_t lines_ = 0;
};

}  // namespace uagfx::studio

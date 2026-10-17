#pragma once
#include <windows.h>

#include <string>
#include <vector>

#include "core/config.hpp"
#include "core/gpu_timing.hpp"
#include "core/settings/strings.hpp"

namespace uagfx::win {

// 1.9.3: the game's console - ~ (the key left of 1) over the game opens a panel of the Effects page's switches and
// sliders, drawn in a layered window of its own (as the frame counter: the same with every renderer). Up and down
// choose, Space or Enter switches, left and right move a slider, Tab held turns the chosen effect off while it is held
// (to compare), Esc or ~ closes. A change goes into uaro-graphics.ini, which the driver reads within its next present.
// While it is open the game's window gets no keys.
class Console {
public:
    ~Console();
    // Every present of the game: its keys and its panel. True: uaro-graphics.ini was changed - read it again now.
    // 2.2.1: timing - the card's work (the title's GPU ms, each effect's own); nullptr: not measured
    bool present(HWND game, bool foreground, const std::string& iniPath, const Config& config,
                 const GpuTiming* timing = nullptr);
    // The effect held off for comparison ("effects.grass"), "" none.
    const std::string& comparing() const { return comparing_; }
    bool open() const { return open_; }

private:
    enum class Kind { Heading, Box, Check, Slider, Choice, Open };  // 2.2.1: Open - a group's effects shown or not
    struct Row {
        Kind kind = Kind::Heading;
        std::string key;  // "effects.grass"; a heading's: its text (UTF-8)
        int depth = 0;
    };
    struct Key {  // a key read by its state: once at a press, again while held
        bool down = false;
        ULONGLONG since = 0, last = 0;
    };

    void build(settings::Lang lang);
    bool pressed(int vk, Key& k, bool repeats, ULONGLONG now);
    bool change(const std::string& iniPath, const Config& config, int step);  // step 0: switch
    void draw(HWND game, const Config& config, const GpuTiming* timing);
    void hide();
    static LRESULT CALLBACK wndProc(HWND w, UINT msg, WPARAM wp, LPARAM lp);

    bool open_ = false;
    bool groupOpen_[kEffectGroups] = {};  // 2.2.1: the groups opened to their effects
    std::vector<Row> rows_;
    int at_ = 0, top_ = 0;
    settings::Lang lang_ = settings::Lang::En;
    std::string comparing_;
    Key toggle_, up_, down_, left_, right_, pageUp_, pageDown_, space_, enter_, escape_;
    HWND window_ = nullptr;
    HWND game_ = nullptr;
    bool dirty_ = true;
    std::string shownText_;
    // the game's windows whose procedure is wndProc now, each with its own before (2.2.2: a window made anew too)
    struct Subclassed {
        HWND window;
        WNDPROC proc;
    };
    static std::vector<Subclassed> subclassed_;
    static Console* active_;
};

}  // namespace uagfx::win

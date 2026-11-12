#pragma once
#include <windows.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "app/launcher.hpp"
#include "app/update.hpp"
#include "core/launcher_art.hpp"
#include "core/launcher_layout.hpp"
#include "core/launcher_picker.hpp"
#include "core/settings/changelog.hpp"
#include "core/settings/choice_presets.hpp"
#include "core/settings/strings.hpp"
#include "core/window_panel.hpp"

namespace uagfx::app {

// 3.0.18-beta.193 (the owner: «первый экран... очень сложный, новый пользователь может испугаться... учти, что это
// игра, а не приложение»; docs/superpowers/specs/2026-11-12-launcher-design.md): the launcher - the first screen of
// uaro-graphics.exe. One child window over the main window's client, painted by itself: the art of uaRO HD (the
// fountain at dawn, by day, at sunset, in winter - one fading into the next every 20 s while the window is in front and
// the game does not run), «Що нового» (the update's card, the site's news), four icons, the look line, the big button
// and the version. What it shows comes in LauncherData (MainWindow gathers it); what is pressed goes to LauncherHost.

struct LauncherData {
    settings::Lang lang = settings::Lang::En;
    launcher::Info info;
    bool siteAsking = false, siteFailed = false;  // the site's answer still coming / not got
    UpdateState update;
    bool gameRunning = false;
    std::wstring look, lookNote, version, plateText, plateButton;  // plateText "": no plate
    std::vector<settings::changelog::Version> news;  // the changelog's versions, the newest first
    int newFor = 0;  // how many of them the player has not seen (newer than cache\seen-version.txt)
    int unread = 0;  // answers to the player's reports not read
    // 3.0.18-beta.194: the look picker - each choice's variant now and its keys changed by hand, what fits the card
    struct Look {
        int variant = 0, changed = 0;
    };
    Look looks[settings::presets::kChoices];
    settings::presets::Suggestion card;
    std::wstring cardName;  // the adapter, "" not known
    bool fitsCard = false;  // «Картинка» and «Текстури» are the card's
    bool undoable = false;  // a pick of the last 5 s: «Змінено · Скасувати»
};

class LauncherHost {
public:
    virtual ~LauncherHost() = default;
    // A part clicked (or Enter on it): index - a news card's
    virtual void launcherAction(launcherview::Part part, int index) = 0;
    // 3.0.18-beta.194: the look picker's - an option (its choice and variant), the card's set, the test, undo, all
    virtual void lookAction(launcherpick::PickPart part, settings::presets::Choice choice, int variant) = 0;
};

class LauncherView {
public:
    LauncherView() = default;
    LauncherView(const LauncherView&) = delete;
    LauncherView& operator=(const LauncherView&) = delete;
    ~LauncherView();
    // artDir: the folder of the art (<the game>\uaro-graphics\launcher)
    bool create(HWND parent, LauncherHost& host, const std::wstring& artDir);
    HWND hwnd() const { return hwnd_; }
    void show(bool on);
    bool shown() const { return shown_; }
    void setData(const LauncherData& d);
    void setProgress(bool downloading, float share, double bytesPerSecond);
    // A part's box on the screen (a menu under or over it)
    RECT screenRect(launcherview::Part part, int index = -1) const;

private:
    // The launcher's faces: Exo 2 (its words) and Cinzel (the wordmark) from the app's resources, Segoe MDL2 Assets
    // (the icons), Segoe UI (what Exo 2 has no glyph for: arrows, triangles, stars)
    enum class Face : uint8_t { Medium, SemiBold, Bold, ExtraBold, Logo, Icons, Other, Count };
    struct Style {
        Face face = Face::Medium;
        float px = 13;  // at scale 1
        uint32_t rgb = 0xEEF2F8;
        float spacing = 0;  // between the letters, at scale 1
    };
    static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT handle(UINT msg, WPARAM wp, LPARAM lp);
    void relayout();
    void paint(HDC target);
    bool ensureFrame(int w, int h);
    void compose();      // the art, its shade, the cards and the button into pic_ (no words)
    void words(HDC dc);  // the words over it
    void shade(std::vector<uint32_t>& px) const;
    void coverArt();
    void tick();  // the art's timer
    void finishFade();
    void updateTimer();
    bool inFront() const;
    launcherview::Hit hitAt(int x, int y) const;
    void press(launcherview::Hit hit);
    std::vector<launcherview::Hit> focusOrder() const;
    launcherview::Box boxOf(launcherview::Hit hit) const;
    launcherview::PlayState playState() const;
    // 3.0.18-beta.194: the look picker over the launcher (the look line opens it; Esc, ✕ or a click outside close it)
    void openPicker();
    void closePicker();
    void composePicker();
    void wordsPicker(HDC dc);
    void pickerPress(launcherpick::PickHit hit);
    std::vector<launcherpick::PickHit> pickerOrder() const;
    launcherview::Box pickerBox(launcherpick::PickHit hit) const;
    bool hovered(launcherview::Part p, int index = -1) const;
    bool pressedOn(launcherview::Part p) const;
    std::wstring t(settings::Text x) const;
    std::wstring tf(settings::Text x, const std::vector<std::string>& args) const;
    // the words: GDI's fonts at the client's scale, a line's width and drawing (the runs Exo 2 lacks in Segoe UI),
    // lines wrapped to a width (the last one cut with «…»)
    HFONT font(Face f, float px);
    bool hasIcons();
    void dropFonts();
    int widthOf(HDC dc, const Style& st, const std::wstring& s);
    std::wstring fit(HDC dc, const Style& st, const std::wstring& s, int width);
    std::vector<std::wstring> wrap(HDC dc, const Style& st, const std::wstring& s, int width, int most);
    void line(HDC dc, const Style& st, const std::wstring& s, float x, float baseline, int align = 0);  // -1 0 1
    float ascentOf(HDC dc, const Style& st);

    HWND hwnd_ = nullptr;
    LauncherHost* host_ = nullptr;
    std::wstring artDir_;
    LauncherData data_;
    bool shown_ = false;
    bool downloading_ = false;
    float share_ = 0;
    double speed_ = 0;
    int dpi_ = 96;
    launcherview::Layout layout_;
    launcherview::Box pill_, plateButton_;  // the server's pill, the plate's button (composed, then written on)
    launcherview::Hit hover_, pressed_;
    int focus_ = -1;  // in focusOrder(); -1: no ring (until Tab)
    bool tracking_ = false;
    std::map<int, HFONT> fonts_;
    int icons_ = -1;  // Segoe MDL2 Assets there (1) or not (0, Windows 7 and 8: letters), -1 not looked
    // the frame: composed in pic_, copied into the DIB, the words drawn by GDI over it
    panel::Picture pic_;
    HBITMAP dib_ = nullptr;
    HGDIOBJ firstBitmap_ = nullptr;
    HDC memDc_ = nullptr;
    void* bits_ = nullptr;
    int frameW_ = 0, frameH_ = 0;
    // the art: the one shown and the one fading in, as read and at the client's size; base_: the frame's art shaded
    launcherart::Image art_[2];
    std::vector<uint32_t> artPx_, nextPx_, base_;
    int artIndex_ = 0, nextIndex_ = 0;
    bool baseValid_ = false, fading_ = false;
    int heldMs_ = 0, fadeMs_ = 0, timerMs_ = 0;
    unsigned long long lastTick_ = 0;
    // the look picker: shown, its places, the mouse's and the keys' part; the three pictures of «Картинка» as read and
    // at their boxes' size
    bool picking_ = false;
    launcherpick::Picker picker_;
    launcherpick::PickHit pickHover_, pickPressed_;
    int pickFocus_ = -1;
    bool looksRead_ = false;
    launcherart::Image looks_[3];
    std::vector<uint32_t> lookPx_[3];
    int lookW_ = 0, lookH_ = 0;
};

}  // namespace uagfx::app

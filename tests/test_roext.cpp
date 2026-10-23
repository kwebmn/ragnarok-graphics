// 3.0.1-beta.8: ROExt's work (the client's dinput8.dll) in the driver - core/roext: its settings as ROExt reads
// dinput8.ini, the game's window it makes, the messages it changes.
#include <string>
#include <vector>

#include "core/roext.hpp"
#include "core/settings/ini_edit.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

// The uaRO client's own dinput8.ini (uaRO.zip), its comments shortened
const char* kClientIni =
    "; Configuration defaults for ROExt.\r\n[ROExt]\r\n\r\n; Release CPU when RO window is inactive.\r\n"
    "AutoFreeCPU = 4\r\nMinimizeKey = 0\r\nRemapMouseButtons = 0\r\nRemapAltF4 = 0\r\nWindowOnTop = 0\r\n"
    "WindowLock = 1\r\nWindowPosX = 0\r\nWindowPosY = 0\r\nWindowWidth = 1600\r\nWindowHeight = 863\r\n"
    "; 65001 (UTF-8) makes client work with all languages at the same time.\r\nCodePage = -1\r\n";

roext::Message msg(uint32_t message, uint32_t wParam, int32_t lParam = 0) { return {message, wParam, lParam}; }

}  // namespace

TEST(roext_reads_dinput8_ini_as_roext) {
    const roext::Settings c = roext::fromDinput8Ini(kClientIni);
    CHECK(c.windowLock && !c.windowOnTop && !c.mouseButtons && !c.altF4 && !c.minimizeKey);
    CHECK(c.freeCpu == 4 && c.codePage == -1);
    CHECK(c.windowWidth == 1600 && c.windowHeight == 863 && c.windowX == 0 && c.windowY == 0);
    // a key not there: ROExt's own default (its mouse buttons on, 250 ms), case of no matter, the width up to 4
    const roext::Settings d = roext::fromDinput8Ini("[roext]\nwindowwidth = 1601\nWindowHeight=700 ; a note\n");
    CHECK(d.mouseButtons && d.windowLock && d.freeCpu == 250 && d.codePage == -1);
    CHECK(d.windowWidth == 1604 && d.windowHeight == 700);
    CHECK(roext::fromDinput8Ini("[ROExt]\nAutoFreeCPU = 0\n").freeCpu == 0);
    CHECK(roext::fromDinput8Ini("[ROExt]\nAutoFreeCPU = 1\n").freeCpu == 250);
    CHECK(roext::fromDinput8Ini("[ROExt]\nAutoFreeCPU = -5\n").freeCpu == 250);
    CHECK(roext::fromDinput8Ini("[ROExt]\nCodePage = 65001\nRemapAltF4 = 2\n").codePage == 65001);
    CHECK(roext::fromDinput8Ini("[ROExt]\nRemapAltF4 = 2\n").altF4);  // anything but 0 is on
    CHECK(roext::fromDinput8Ini("[Other]\nWindowLock = 0\n").windowLock);  // another section's: not ROExt's
    CHECK(roext::fromDinput8Ini("[ROExt]\nWindowLock =\n").windowLock);  // an empty value: the default
    CHECK(!roext::fromDinput8Ini("[ROExt]\nWindowLock = 0 ; off\n").windowLock);
    CHECK(roext::fromDinput8Ini("").mouseButtons);  // no file: ROExt's own defaults
}

TEST(roext_settings_go_into_uaro_graphics_ini) {
    roext::Settings s;
    s.windowLock = false;
    s.mouseButtons = true;
    s.freeCpu = 10;
    s.codePage = 1251;
    s.windowWidth = 1280;
    std::vector<settings::IniEdit> edits;
    for (const auto& kv : roext::iniValues(s)) edits.push_back({"roext", kv.first, kv.second});
    const std::string ini = settings::applyIniEdits("", edits);
    const auto keys = settings::readIni(ini);
    CHECK_EQ(keys.at("roext.window_lock"), std::string("0"));
    CHECK_EQ(keys.at("roext.mouse_buttons"), std::string("1"));
    CHECK_EQ(keys.at("roext.free_cpu"), std::string("10"));
    CHECK_EQ(keys.at("roext.code_page"), std::string("1251"));
    CHECK_EQ(keys.at("roext.window_width"), std::string("1280"));
    CHECK(keys.count("roext.alt_f4") && keys.count("roext.minimize_key") && keys.count("roext.window_on_top"));
    CHECK(keys.count("roext.window_x") && keys.count("roext.window_y") && keys.count("roext.window_height"));
}

// The game's main window as uaRO.exe makes it: windowed or full screen, nothing else
TEST(roext_knows_the_games_window) {
    CHECK(roext::isGameWindow(0, 0x02C20000, false, false, false));  // clip children, caption, minimize box
    CHECK(roext::isGameWindow(0, 0x02CA0000, false, false, false));  // and the system menu
    CHECK(roext::isGameWindow(0, 0x80020000, false, false, false));  // full screen: a popup
    CHECK(!roext::isGameWindow(0x8, 0x02C20000, false, false, false));
    CHECK(!roext::isGameWindow(0, 0x02C20000, true, false, false));
    CHECK(!roext::isGameWindow(0, 0x02C20000, false, true, false));
    CHECK(!roext::isGameWindow(0, 0x02C20000, false, false, true));
    CHECK(!roext::isGameWindow(0, 0x10CF0000, false, false, false));  // an overlapped window of another kind
}

TEST(roext_plans_the_window) {
    const int work[4] = {0, 0, 1921, 1040};  // the work area: all but the taskbar
    roext::Settings s;  // locked, no size of its own
    CHECK_EQ(roext::styleOf(s), 0x860A0000u);
    roext::WindowPlan p = roext::windowPlan(s, work, 16, 39);
    CHECK(p.style == 0x860A0000u && p.exStyle == 0 && p.place);
    CHECK(p.x == 0 && p.y == 0 && p.width == 1924 && p.height == 1040);  // the width up to 4, as ROExt
    s.windowWidth = 1600;  // the client's own dinput8.ini: both sizes set - they win over the work area
    s.windowHeight = 863;
    p = roext::windowPlan(s, work, 0, 0);  // a borderless window: no frame
    CHECK(p.place && p.x == 0 && p.y == 0 && p.width == 1600 && p.height == 863);
    s.windowWidth = s.windowHeight = 0;
    s.windowOnTop = true;
    CHECK_EQ(roext::windowPlan(s, work, 16, 39).exStyle, 0x8u);  // WS_EX_TOPMOST
    s.windowLock = false;
    s.windowOnTop = false;
    CHECK_EQ(roext::styleOf(s), 0x06CA0000u);
    p = roext::windowPlan(s, work, 16, 39);  // no size of its own: the game's
    CHECK(p.style == 0x06CA0000u && !p.place);
    s.windowX = 100;
    s.windowY = 50;
    s.windowWidth = 1600;
    s.windowHeight = 863;
    p = roext::windowPlan(s, work, 16, 39);  // the client area 1600 x 863, the frame around it
    CHECK(p.place && p.x == 100 && p.y == 50 && p.width == 1616 && p.height == 902);
    s.windowHeight = 0;
    CHECK(!roext::windowPlan(s, work, 16, 39).place);  // one size alone: ignored
}

TEST(roext_changes_the_messages) {
    roext::Settings s;  // the client's: nothing remapped
    roext::Message m = msg(0x207, 0x10);  // the middle button down
    CHECK(roext::transform(m, s, false) && m.message == 0x207u);
    m = msg(0x100, 0x91);  // Scroll Lock: dropped, always
    CHECK(!roext::transform(m, s, false));
    m = msg(0x101, 0x91);
    CHECK(!roext::transform(m, s, false));
    m = msg(0x100, 'W');
    CHECK(roext::transform(m, s, true) && m.message == 0x100u);  // Ctrl+W: as it is
    m = msg(0x104, 0x73, 0x20000000);
    CHECK(roext::transform(m, s, false) && m.message == 0x104u);  // Alt+F4: as it is
    s.mouseButtons = s.altF4 = s.minimizeKey = true;
    m = msg(0x207, 0x10, 0x00500040);  // middle down -> F24 down, the place kept
    CHECK(roext::transform(m, s, false) && m.message == 0x100u && m.wParam == 0x87u && m.lParam == 0x00500040);
    m = msg(0x208, 0);
    CHECK(roext::transform(m, s, false) && m.message == 0x101u && m.wParam == 0x87u);
    m = msg(0x20B, 0x00010020);  // X1 down -> F23
    CHECK(roext::transform(m, s, false) && m.message == 0x100u && m.wParam == 0x86u);
    m = msg(0x20C, 0x00010000);  // X1 up (its MK_XBUTTON1 gone) -> F23 up as well
    CHECK(roext::transform(m, s, false) && m.message == 0x101u && m.wParam == 0x86u);
    m = msg(0x20B, 0x00020040);  // X2 -> F22
    CHECK(roext::transform(m, s, false) && m.message == 0x100u && m.wParam == 0x85u);
    m = msg(0x209, 0x10);  // a double click: as it is
    CHECK(roext::transform(m, s, false) && m.message == 0x209u);
    m = msg(0x100, 'W');  // Ctrl+W: minimized
    CHECK(roext::transform(m, s, true) && m.message == 0x112u && m.wParam == 0xF020u && m.lParam == -1);
    m = msg(0x100, 'W');  // W alone: a W
    CHECK(roext::transform(m, s, false) && m.message == 0x100u && m.wParam == uint32_t('W'));
    m = msg(0x104, 0x73, 0x20000000);  // Alt+F4 -> F4
    CHECK(roext::transform(m, s, false) && m.message == 0x100u && m.wParam == 0x73u);
    m = msg(0x105, 0x73, 0x20000000);
    CHECK(roext::transform(m, s, false) && m.message == 0x101u);
    m = msg(0x105, 0x2C, 0x20000000);  // Alt+Print Screen -> Alt+F4: closes
    CHECK(roext::transform(m, s, false) && m.message == 0x104u && m.wParam == 0x73u);
    m = msg(0x104, 0x12, 0x20000000);  // Alt itself: as it is
    CHECK(roext::transform(m, s, false) && m.message == 0x104u && m.wParam == 0x12u);
}

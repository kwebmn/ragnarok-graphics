#include "app/check_notify.hpp"

namespace uagfx::app {
namespace {

HWND volatile g_window = nullptr;
UINT volatile g_message = 0;

}  // namespace

void setCheckWindow(HWND window, UINT message) {
    g_message = message;
    g_window = window;
}

void checkDone() {
    if (HWND w = g_window) PostMessageW(w, g_message, 0, 0);
}

}  // namespace uagfx::app

#pragma once

namespace uagfx::app {

// When the main window makes its page again (stage 2 spec §6). A rebuild comes from a posted message: never inside
// the notification of a control it takes down, never while a message box of the window waits - the page whose code
// opened the box still runs under it. One rebuild serves every request made before it, also those its own page makes
// while it is taken down and made again.
class Rebuilds {
public:
    bool request();  // a rebuild is wanted: true when the window posts its message now
    bool arrived();  // the posted message came: true when the page is made again now
    void begin();    // the page is taken down and made again: requests meanwhile are served by it
    void end();
    bool quiet() const { return building_; }  // the page's notifications now are not the player's
    void boxOpened();  // a message box: rebuilds wait until it is closed
    bool boxClosed();  // true when a rebuild waited for it: the window posts its message now

private:
    bool posted_ = false;   // a message is on its way
    bool waiting_ = false;  // wanted while a box was open
    bool building_ = false;
    int boxes_ = 0;
};

}  // namespace uagfx::app

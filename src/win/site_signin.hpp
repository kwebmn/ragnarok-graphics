#pragma once
#include <windows.h>

#include <string>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::sitesigninwin {

// 3.0.11-beta.180 (the login concept: the site account in the client): the site sign-in card (core/site_signin) -
// drawn by win/login_card in place of its card while open; typed into with the keyboard (Tab, Enter, Esc, Ctrl+V
// pastes), its requests through win/site_http; signed in: the token kept (win/site_key) and the card closed. The
// site's client API there or not (before its deploy: «Підтягнути всі логіни» not offered) - asked once, again every
// 10 minutes while not. On the game's thread.
void open();
// 3.0.11-beta.181: the signed-in site account's forms - a new game login, an existing one linked, a game password
// changed (`accountId`, `login` its); done: the login and its password on this computer's shelf, the site's asked again
void openForm(int way, int accountId, const std::string& login);  // way: sitesignin::Way's number
// 3.0.11-beta.181: the registration in the client - one form (the site account and its first game login, one
// password); done: the token kept, the account on the shelf, the login sent and the first character made
void openRegister();
// The registration done: its login and password once (win/login_card sends the login, then makes the character)
bool takeRegistered(std::string& login, std::string& password);
bool isOpen();
void close();
void probe();      // the login screen shown: is the site's client API there?
bool siteReady();  // it answered as the API (422 on an empty login)
bool siteDown();   // 3.0.18-beta.205: it did not answer at all
void frame(double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(UINT m, float tx, float ty);  // a button's message at the target's (tx, ty); true: the card's
bool key(UINT m, WPARAM wp);
bool box(int& x, int& y, int& w, int& h);
bool takeSignedIn();  // true once after a sign-in (the shelf to be filled from the site)

}  // namespace uagfx::win::sitesigninwin

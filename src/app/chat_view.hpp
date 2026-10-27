#pragma once
#include <windows.h>

#include <string>
#include <vector>

namespace uagfx::app {

// 3.0.9-beta.5 (the user: «делай все» - the support canvas' screen 1): «Звернення» as a messenger - two controls of
// the window's own. The chat: the player's bubbles on the right (blue), the developer's on the left (white, «Розробник»
// over it), screenshots as thumbnails under their words (a click opens one: kChatShot to the parent), a status change
// or «Розробник прочитав» as a thin line across; it scrolls, the newest at the bottom. The head: the report's title and
// its status in a chip, a line of facts, and the report's path (core/report_talk steps).
constexpr UINT kChatShot = WM_APP + 21;  // to the parent: wParam the item, lParam the shot in it

struct ChatItem {
    enum Kind : unsigned char { Line, Mine, Theirs };
    Kind kind = Line;
    std::wstring head, text;  // Line: the text alone
    size_t shots = 0;         // thumbnails under the words: their pictures come with setChatThumb
    COLORREF tone = RGB(93, 104, 128);  // a Line's words and rules
};
HWND makeChatView(HWND parent, int id, HFONT font, HFONT bold, int dpi);
// The items as the chat now (the old thumbnails go), scrolled to the newest; `generation` names this filling
void setChat(HWND view, const std::vector<ChatItem>& items, unsigned generation);
// A thumbnail fetched on another thread (posted): taken when its filling is still the chat's, else deleted
void postChatThumb(HWND view, unsigned generation, size_t item, size_t shot, HBITMAP bmp);
SIZE chatThumbSize(int dpi);  // the thumbnails' size

struct TalkStep {
    std::wstring text;
    bool done = false, current = false;
};
struct TalkHead {
    std::wstring title, chip, facts;
    COLORREF chipGround = RGB(236, 239, 244), chipWords = RGB(74, 85, 104);
    std::vector<TalkStep> steps;
};
HWND makeTalkHead(HWND parent, int id, HFONT font, HFONT bold, int dpi);
void setTalkHead(HWND head, const TalkHead& h);

}  // namespace uagfx::app

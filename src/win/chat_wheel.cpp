#include "win/chat_wheel.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "core/chat_wheel.hpp"
#include "core/settings/game_settings.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "pad/core/config/bindings.hpp"
#include "win/code_page.hpp"
#include "win/emote_window.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/osk_window.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"

namespace uagfx::win::chatwheelwin {
namespace {

namespace cw = uagfx::chatwheel;

// uaRO.exe's: the chat window (where the manager keeps it, its class, its SendMsg: 6 / 0x76 its Enter), its words'
// and whisper name's edits; an edit's words (thiscall) and its SetText (vtable + 0x98); the send mode (0 near, 1 the
// party, 2 the guild); the manager's SetFocus
constexpr uint32_t kWindowMgr = 0x727ED8, kChatAt = kWindowMgr + 0x84, kChatClass = 0x697D78, kChatSend = 0x45EF90;
constexpr uint32_t kWords = 0x68, kName = 0x6C, kGetText = 0x443300, kSetTextSlot = 0x98, kSendMode = 0x772D60;
constexpr uint32_t kSetFocus = 0x4CE510, kSendMsgSlot = 0x94;
constexpr int kButtonMsg = 6, kEnter = 0x76;
using SendMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);
using GetTextFn = const char*(__attribute__((thiscall)) *)(uint32_t edit);
using SetTextFn = void(__attribute__((thiscall)) *)(uint32_t edit, const char* text);
using SetFocusFn = void(__attribute__((thiscall)) *)(uint32_t manager, uint32_t control);

struct State {
    bool open = false;
    cw::Page page = cw::Page::Phrases;
    cw::Channel channel = cw::Channel::Near;
    int hover = -1;
    Setup setup;
    std::vector<std::string> phrases, emotes;
    unsigned codePage = 0;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    cw::Layout layout;
    std::vector<panel::Picture> icons;
    std::string key;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    int told = 0;
};

State& state() {
    static State s;
    return s;
}

std::string say(settings::Text t) { return settings::tr(language(), t); }

std::string fmt(std::string form, const std::vector<std::string>& v) {
    for (size_t i = 0; i < v.size(); ++i) {
        const std::string key = "{" + std::to_string(i + 1) + "}";
        for (size_t at = form.find(key); at != std::string::npos; at = form.find(key, at + v[i].size()))
            form.replace(at, key.size(), v[i]);
    }
    return form;
}

uint32_t read32(uint32_t at) {
    uint32_t v = 0;
    return readGame(at, &v, 4) ? v : 0;
}

bool write32(uint32_t at, uint32_t v) {
    SIZE_T n = 0;
    return WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(uintptr_t(at)), &v, 4, &n) && n == 4;
}

uint32_t gameChat() {
    const uint32_t w = read32(kChatAt);
    return w && read32(w) == kChatClass && read32(kChatClass + kSendMsgSlot) == kChatSend ? w : 0;
}

std::string editText(uint32_t edit) {
    if (!edit) return "";
    const char* t = reinterpret_cast<GetTextFn>(uintptr_t(kGetText))(edit);
    return t ? std::string(t, strnlen(t, 256)) : std::string();
}

void setText(uint32_t edit, const std::string& text) {
    if (edit) reinterpret_cast<SetTextFn>(uintptr_t(read32(read32(edit) + kSetTextSlot)))(edit, text.c_str());
}

// A line through the game's own chat line and its Enter, on `channel` (a command whatever the whisper's name); the
// player's own words, the whisper's name and the send mode put back after
bool sendLine(const std::string& gameText, bool command, cw::Channel channel) {
    const uint32_t chat = gameChat();
    if (!chat || gameText.empty()) return false;
    const uint32_t words = read32(chat + kWords), name = read32(chat + kName);
    if (!words) return false;
    const std::string keptWords = editText(words), keptName = editText(name);
    const uint32_t keptMode = read32(kSendMode);
    if (!command) {
        write32(kSendMode, uint32_t(channel));
        if (!keptName.empty()) setText(name, "");
    }
    setText(words, gameText);
    reinterpret_cast<SendMsgFn>(uintptr_t(kChatSend))(chat, 0, kButtonMsg, kEnter, 0, 0);
    setText(words, keptWords);
    if (!command) {
        if (!keptName.empty()) setText(name, keptName);
        write32(kSendMode, keptMode);
    }
    return true;
}

// Windows' voice typing into the chat's line (Win+H while the game's window is in front)
void voice(State& s) {
    const uint32_t chat = gameChat();
    const uint32_t words = chat ? read32(chat + kWords) : 0;
    if (words) reinterpret_cast<SetFocusFn>(uintptr_t(kSetFocus))(kWindowMgr, words);
    INPUT in[4] = {};
    for (INPUT& i : in) i.type = INPUT_KEYBOARD;
    in[0].ki.wVk = VK_LWIN;
    in[1].ki.wVk = 'H';
    in[2].ki.wVk = 'H';
    in[2].ki.dwFlags = KEYEVENTF_KEYUP;
    in[3].ki.wVk = VK_LWIN;
    in[3].ki.dwFlags = KEYEVENTF_KEYUP;
    const UINT sent = SendInput(4, in, sizeof(INPUT));
    if (s.told++ < 30) LOG_INFO("chat wheel: the voice typing asked (Win+H, %u of 4 keys)", sent);
}

const std::vector<std::string>& items(const State& s) {
    return s.page == cw::Page::Phrases ? s.phrases : s.emotes;
}

cw::Texts texts() {
    cw::Texts t;
    t.pages[0] = say(settings::Text::WheelPhrases);
    t.pages[1] = say(settings::Text::WheelEmotes);
    t.channels[0] = say(settings::Text::WheelNear);
    t.channels[1] = say(settings::Text::WheelParty);
    t.channels[2] = say(settings::Text::WheelGuild);
    t.keyboard = fmt(say(settings::Text::WheelKeyboard), {uapad::buttonLabel(0)});  // A
    t.voice = fmt(say(settings::Text::WheelVoice), {uapad::buttonLabel(3)});        // Y
    t.hint = fmt(say(settings::Text::WheelHint), {uapad::buttonLabel(4), uapad::buttonLabel(5),
                                                   uapad::buttonLabel(6), uapad::buttonLabel(7)});
    return t;
}

std::vector<Words> gdiWords(const std::vector<cw::Words>& in) {
    std::vector<Words> out;
    for (const cw::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == cw::Align::Right    ? DT_RIGHT
                                 : w.align == cw::Align::Center ? DT_CENTER
                                                                : DT_LEFT),
                            w.wrap});
    return out;
}

void render(State& s) {
    const uicost::Paint cost("the chat wheel", s.layout.w, s.layout.h);
    if (!panel::blank(s.picture, s.layout.w, s.layout.h)) return;
    const std::vector<std::string>& list = items(s);
    cw::paint(s.picture, s.layout, int(list.size()), s.hover, s.page, s.channel,
              s.page == cw::Page::Emotes ? &s.icons : nullptr);
    drawWords(s.picture, gdiWords(cw::words(s.layout, list, s.hover, s.page, s.channel, texts())));
    s.upload = true;
}

// The emotions' pictures for this page's commands (the game's list's, by their number in it)
void emotePictures(State& s) {
    s.icons.clear();
    const std::vector<panel::Picture>& all = emotewin::emotionPictures(int(std::lround(30 * s.scale)));
    for (const std::string& c : s.emotes) {
        const int n = emotewin::emotionNumber(c);
        s.icons.push_back(n >= 0 && size_t(n) < all.size() ? all[size_t(n)] : panel::Picture{});
    }
}

}  // namespace

void open(const Setup& setup) {
    State& s = state();
    if (s.open) return;
    s.open = true;
    s.setup = setup;
    s.hover = -1;
    std::array<std::string, cw::kPhrases> defaults{};
    const settings::Text phraseTexts[cw::kPhrases] = {
        settings::Text::WheelPhrase1, settings::Text::WheelPhrase2, settings::Text::WheelPhrase3,
        settings::Text::WheelPhrase4, settings::Text::WheelPhrase5, settings::Text::WheelPhrase6,
        settings::Text::WheelPhrase7, settings::Text::WheelPhrase8};
    for (int i = 0; i < cw::kPhrases; ++i) defaults[size_t(i)] = say(phraseTexts[i]);
    s.phrases = cw::phraseList(setup.phrases, defaults);
    std::vector<std::string> game;
    for (int i = 0; i < cw::kEmotes; ++i) game.push_back(settings::builtInMacro(i));
    s.emotes = cw::emoteList(setup.emotes, game);
    s.icons.clear();
    s.key.clear();
    LOG_INFO("chat wheel: open (%zu phrases, %zu emotions)", s.phrases.size(), s.emotes.size());
}

bool isOpen() { return state().open; }

void close() {
    State& s = state();
    s.open = false;
    s.hover = -1;
}

void pad(const Pad& p) {
    State& s = state();
    if (!s.open) return;
    const std::vector<std::string>& list = items(s);
    const int hover = cw::sectorAt(p.rx, p.ry, int(list.size()));
    if (hover != s.hover) {
        s.hover = hover;
        s.key.clear();
    }
    if (p.pageBack || p.pageOn) {
        s.page = s.page == cw::Page::Phrases ? cw::Page::Emotes : cw::Page::Phrases;
        s.hover = -1;
        s.key.clear();
    }
    if ((p.channelBack || p.channelOn) && s.page == cw::Page::Phrases) {
        const int by = p.channelOn ? 1 : cw::kChannels - 1;
        s.channel = cw::Channel((int(s.channel) + by) % cw::kChannels);
        s.key.clear();
    }
    if (p.cancel) {
        close();
        return;
    }
    if (p.voice) {
        close();
        voice(s);
        return;
    }
    if (p.keyboard) {  // the on-screen keyboard (win/osk_window) over the chat's line
        close();
        oskwin::open();
        return;
    }
    if (p.held) return;
    // the Talk button let go: the one pointed at sent
    const int at = s.hover;
    close();
    if (at < 0 || at >= int(list.size())) return;
    const std::string utf8 = list[size_t(at)];
    const bool command = s.page == cw::Page::Emotes;
    const bool sent = sendLine(narrowed(utf8Wide(utf8), s.codePage), command, s.channel);
    if (s.told++ < 30)
        LOG_INFO("chat wheel: %s %s%s", command ? "the emotion" : "a phrase", sent ? "sent" : "not sent (no chat line)",
                 command ? "" : (s.channel == cw::Channel::Party   ? " to the party"
                                 : s.channel == cw::Channel::Guild ? " to the guild"
                                                                   : " to everyone"));
}

void frame(HWND game, unsigned codePage, double k, int targetWidth, int targetHeight) {
    State& s = state();
    s.codePage = codePage;
    if (!s.open || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) return;
    if (!inGameWorld()) {
        close();
        return;
    }
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    const std::vector<std::string>& list = items(s);
    s.layout = cw::layout(int(list.size()), targetWidth, targetHeight, s.scale);
    if (s.page == cw::Page::Emotes && s.icons.size() != s.emotes.size()) emotePictures(s);
    std::string key = std::to_string(int(s.page)) + ',' + std::to_string(int(s.channel)) + ',' +
                      std::to_string(s.hover) + ';' + std::to_string(s.layout.x) + ',' + std::to_string(s.layout.y) +
                      ',' + std::to_string(s.layout.w) + '#' + std::to_string(uistyle::generation());
    if (key != s.key) {
        s.key = key;
        render(s);
    }
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.open || s.k <= 0 || s.picture.width <= 0 || s.picture.height <= 0) return;
    drawPart(record, renderer, s.picture, s.texture, s.texW, s.texH, s.upload, s.layout.x, s.layout.y, s.k);
}

}  // namespace uagfx::win::chatwheelwin

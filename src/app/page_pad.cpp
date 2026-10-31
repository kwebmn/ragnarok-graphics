#include <cmath>
#include <cstdio>

#include "app/app_disk.hpp"
#include "app/files.hpp"
#include "app/page.hpp"
#include "app/setting_ids.hpp"
#include "app/text.hpp"
#include "core/layout.hpp"
#include "core/settings/ini_edit.hpp"
#include "core/settings/pad_files.hpp"
#include "core/version.hpp"
#include "pad/core/config/bindings.hpp"
#include "pad/core/config/config.hpp"
#include "pad/core/i18n.hpp"

namespace uagfx::app {
namespace {

using namespace settings;

constexpr int kType = 1510;
constexpr int kLightbar = 1511;
constexpr int kTouchpad = 1512;
constexpr int kRumble = 1513;
constexpr int kStartsOn = 1514;
constexpr int kNpcHint = 1516;  // 0.7 of the pad
// 3.0.11-beta.89 (the concept «Контроллер в интерфейсе uaRO», stage 5): the page «Контролер» - the buttons of each
// character (uaro-pad-binds.ini), how far the stick walks, the old target mark
constexpr int kWho = 1517;
constexpr int kWalk = 1518;  // a slider: its number 1519
constexpr int kMark = 1521;
constexpr int kButton0 = 1530;  // the 14 buttons' choices, uapad::kBindableButtons' order

uint32_t g_who = 0;  // the character whose buttons the page shows (0 the default ones); pages are made anew

std::vector<uapad::Action> allActions() {  // the menu's ring: none, attack, the slots, pickup, sit...
    std::vector<uapad::Action> out;
    uapad::Action a{};
    do {
        out.push_back(a);
        a = uapad::nextAction(a, +1);
    } while (a != uapad::Action{} && out.size() < 64);
    return out;
}

const char* const kTypes[] = {"auto", "xbox", "sony"};
const char* const kColours[] = {"blue", "red", "green", "purple", "orange", "white", "off"};
const char* const kTouch[] = {"none", "back", "start"};

template <size_t N>
int indexOf(const char* const (&list)[N], const std::string& token, int otherwise) {
    for (size_t i = 0; i < N; ++i)
        if (token == list[i]) return int(i);
    return otherwise;
}

std::string fileText(const std::wstring& path) {
    std::string s;
    return readFile(path, s) ? s : std::string();
}

// The Gamepad page (1.4; 2.9: uaRO Pad inside the driver): its switch ([pad] enabled of uaro-graphics.ini, on by
// default, from the next start of the game) and the settings of uaro-pad.ini a player looks for first, PlayStation's
// light bar and touchpad among them. Those are written at once, not with Apply: the file is the pad's, the game reads
// it when it starts.
class PadPage : public Page {
public:
    explicit PadPage(PageHost& host) : Page(host), rows_(makeSettingsPage(host, {})) {}

    void build(Panel& p) override {
        Model& m = host_.model();
        p.heading(t(Text::PadHeading));
        p.note(t(Text::PadIntro), Tone::Normal);
        rows_->buildRow(p, "pad.enabled");
        if (m.now().config.padEnabled)
            p.note(tf(Text::PadStateOn, {kVersion}), Tone::Good);
        else
            p.note(t(Text::PadStateOff));
        // a 2.x pad dinput.dll the settings app could not take away (the game was running)
        if (classifyPad(fileText(m.paths().dir + L"\\dinput.dll"), "") == PadFile::Pad)
            p.note(t(Text::PadStateOld), Tone::Warn);

        const auto ini = readIni(fileText(ourFile(m.paths().dir, layout::File::PadIni)));
        auto value = [&](const char* key, const char* otherwise) {
            const auto it = ini.find(key);
            return it == ini.end() ? std::string(otherwise) : it->second;
        };
        p.note(seenNote(), Tone::Normal);
        p.gap();  // 3.0.11-beta.89: the interface's own rows for the controller, as «Інтерфейс» has them
        p.heading(t(Text::PadOnInterface));
        rows_->buildRow(p, "interface.pad_hints");
        rows_->buildRow(p, "interface.target_ring");
        rows_->buildRow(p, "interface.mob_hp");  // 3.0.11-beta.95
        rows_->buildRow(p, "interface.pad_walk");
        p.gap();
        buttons(p, value("pad.type", "auto") == "sony");
        p.gap();
        p.heading(t(Text::PadController));
        p.choice(kType, t(Text::PadType), {t(Text::PadTypeAuto), L"Xbox (XInput)", t(Text::PadTypeSony)},
                 indexOf(kTypes, value("pad.type", "auto"), 0), t(Text::PadTypeHint));
        std::vector<std::wstring> colours = {t(Text::ColourBlue),   t(Text::ColourRed),   t(Text::ColourGreen),
                                             t(Text::ColourPurple), t(Text::ColourOrange), t(Text::ColourWhite),
                                             t(Text::ColourOff)};
        const std::string colour = value("sony.lightbar", "blue");
        int colourAt = indexOf(kColours, colour, -1);
        if (colourAt < 0) {  // RRGGBB written by hand: kept as the last item
            colourAt = int(colours.size());
            colours.push_back(tf(Text::ColourOwn, {colour}));
        }
        p.choice(kLightbar, t(Text::PadLightbar), colours, colourAt, t(Text::PadLightbarHint));
        p.choice(kTouchpad, t(Text::PadTouchpad), {t(Text::TouchNothing), t(Text::TouchBack), t(Text::TouchStart)},
                 indexOf(kTouch, value("sony.touchpad", "none"), 0), t(Text::PadTouchpadHint));
        p.check(kRumble, t(Text::PadRumble), value("rumble.enabled", "1") != "0", t(Text::PadRumbleHint));
        p.check(kStartsOn, t(Text::PadStartsOn), value("general.enabled", "1") != "0", t(Text::PadStartsOnHint));
        const int walk = int(std::lround(std::atof(value("stick.walk_radius", "0.22").c_str()) * 100));
        p.slider(kWalk, t(Text::PadWalkRadius), 10, 40, 1, std::clamp(walk, 10, 40), std::to_wstring(walk) + L" %",
                 t(Text::PadWalkRadiusHint));
        const uapad::Lang lang = uapad::Lang(int(host_.lang()));
        std::vector<std::wstring> marks;
        for (int i = 0; i < uapad::kMarkStyles; ++i) {
            const uapad::MarkStyle style = uapad::MarkStyle(i);
            marks.push_back(style == uapad::MarkStyle::Off ? t(Text::PadMarkOff) : wide(markWords(style, lang)));
        }
        const auto mark = uapad::parseMarkStyle(value("aim.mark", "gasp"));
        p.choice(kMark, t(Text::PadMark), marks, mark ? int(*mark) : int(uapad::MarkStyle::Gasp), t(Text::PadMarkHint));
        p.check(kNpcHint, t(Text::PadNpcHint), value("npc.hint", "1") != "0", t(Text::PadNpcHintHint));
        p.note(t(Text::PadSettingsNote));
    }

    void command(int id, int code) override {
        if (settingOfId(id)) {  // the switch: a row of uaro-graphics.ini, applied as the other pages' rows
            rows_->command(id, code);
            return;
        }
        HWND ctl = GetDlgItem(host_.window(), id);
        const bool checked = SendMessageW(ctl, BM_GETCHECK, 0, 0) == BST_CHECKED;
        const LRESULT sel = SendMessageW(ctl, CB_GETCURSEL, 0, 0);
        if ((id == kRumble || id == kStartsOn) && code == BN_CLICKED) {
            write(id == kRumble ? "rumble" : "general", "enabled", checked ? "1" : "0");
        } else if (id == kNpcHint && code == BN_CLICKED) {
            write("npc", "hint", checked ? "1" : "0");
        } else if (code == CBN_SELCHANGE && sel >= 0) {
            if (id == kType && sel < 3) {
                write("pad", "type", kTypes[sel]);
                host_.rebuild();  // the buttons' names: PlayStation's or Xbox's
            }
            if (id == kLightbar && sel < 7) write("sony", "lightbar", kColours[sel]);
            if (id == kTouchpad && sel < 3) write("sony", "touchpad", kTouch[sel]);
            if (id == kMark && sel < uapad::kMarkStyles)
                write("aim", "mark", uapad::markStyleToken(uapad::MarkStyle(int(sel))));
            if (id == kWho) {
                const std::vector<uint32_t> ids = whoIds();
                g_who = size_t(sel) < ids.size() ? ids[size_t(sel)] : 0;
                host_.rebuild();
            }
            if (id >= kButton0 && id < kButton0 + int(uapad::kBindableCount)) setButton(size_t(id - kButton0), sel);
        }
    }

    void scroll(int id, int code, int pos) override {
        if (id == kWalk) {  // 3.0.11-beta.89: how far the stick walks
            const int v = std::clamp(pos, 10, 40);
            SetDlgItemTextW(host_.window(), kWalk + 1, (std::to_wstring(v) + L" %").c_str());
            char token[16];
            std::snprintf(token, sizeof token, "%.2f", double(v) / 100.0);
            write("stick", "walk_radius", token);
            return;
        }
        rows_->scroll(id, code, pos);
    }

private:
    static std::string markWords(uapad::MarkStyle style, uapad::Lang lang) {
        using uapad::Text;
        switch (style) {
            case uapad::MarkStyle::Poison: return uapad::tr(lang, Text::MarkPoison);
            case uapad::MarkStyle::DeadlyPoison: return uapad::tr(lang, Text::MarkDeadlyPoison);
            case uapad::MarkStyle::Curse: return uapad::tr(lang, Text::MarkCurse);
            case uapad::MarkStyle::Stun: return uapad::tr(lang, Text::MarkStun);
            case uapad::MarkStyle::Sleep: return uapad::tr(lang, Text::MarkSleep);
            case uapad::MarkStyle::Silence: return uapad::tr(lang, Text::MarkSilence);
            case uapad::MarkStyle::Sight: return uapad::tr(lang, Text::MarkSight);
            case uapad::MarkStyle::Ruwach: return uapad::tr(lang, Text::MarkRuwach);
            case uapad::MarkStyle::Gasp: return uapad::tr(lang, Text::MarkGasp);
            case uapad::MarkStyle::Off: break;
        }
        return "";
    }

    // The last controller the game saw (uaro-graphics\pad-seen.txt, its unix time and its name)
    std::wstring seenNote() {
        const std::string text = fileText(ourPlace(host_.model().paths().dir, layout::kPadSeen));
        const size_t tab = text.find('\t');
        if (tab == std::string::npos) return t(Text::PadSeenNone);
        unsigned long long when = 0;  // its unix time (no msvcrt's _atoi64 / localtime: kernel32's own)
        for (size_t i = 0; i < tab && text[i] >= '0' && text[i] <= '9'; ++i) when = when * 10 + unsigned(text[i] - '0');
        std::string name = text.substr(tab + 1);
        while (!name.empty() && (name.back() == '\n' || name.back() == '\r')) name.pop_back();
        const unsigned long long ticks = (when + 11644473600ULL) * 10000000ULL;  // the FILETIME of it
        FILETIME utc{DWORD(ticks & 0xFFFFFFFFULL), DWORD(ticks >> 32)}, local{};
        SYSTEMTIME st{};
        std::string at;
        if (when && FileTimeToLocalFileTime(&utc, &local) && FileTimeToSystemTime(&local, &st)) {
            const auto two = [](unsigned v) { return (v < 10 ? "0" : "") + std::to_string(v); };
            at = two(st.wDay) + "." + two(st.wMonth) + " " + two(st.wHour) + ":" + two(st.wMinute);
        }
        return tf(Text::PadSeen, {name, at});
    }

    std::wstring bindsPath() const { return ourFile(host_.model().paths().dir, layout::File::PadBinds); }

    // The characters with buttons of their own, the default ones first (0)
    std::vector<uint32_t> whoIds() {
        std::vector<uint32_t> ids{0};
        for (const auto& [id, c] : uapad::parseProfiles(fileText(bindsPath())).characters) ids.push_back(id);
        return ids;
    }

    // 3.0.11-beta.89: the buttons of the character chosen (the default ones for those without their own)
    void buttons(Panel& p, bool sony) {
        const uapad::Profiles profiles = uapad::parseProfiles(fileText(bindsPath()));
        if (g_who && !profiles.characters.count(g_who)) g_who = 0;
        std::vector<std::wstring> who{t(Text::PadWhoDefault)};
        int at = 0;
        for (const auto& [id, c] : profiles.characters) {
            if (id == g_who) at = int(who.size());
            who.push_back(c.name.empty() ? L"#" + std::to_wstring(id) : wide(c.name));
        }
        p.heading(t(Text::PadButtons));
        p.choice(kWho, t(Text::PadWho), who, at, t(Text::PadWhoHint));
        const uapad::Bindings b = g_who ? profiles.forCharacter(g_who) : profiles.fallback;
        const uapad::Lang lang = uapad::Lang(int(host_.lang()));
        const std::vector<uapad::Action> actions = allActions();
        std::vector<std::wstring> words;
        for (const uapad::Action& a : actions) words.push_back(wide(uapad::actionLabel(a, lang)));
        uapad::useSonyLabels(sony);
        const bool two = p.split(360);
        for (size_t i = 0; i < uapad::kBindableCount; ++i) {
            if (two && i == 7) p.nextColumn();
            int chosen = 0;
            for (size_t k = 0; k < actions.size(); ++k)
                if (actions[k] == b[i]) chosen = int(k);
            p.choice(kButton0 + int(i), wide(uapad::buttonLabel(i)), words, chosen, t(Text::PadButtonHint));
        }
        if (two) p.join();
        uapad::useSonyLabels(false);
    }

    void setButton(size_t i, LRESULT sel) {
        const std::vector<uapad::Action> actions = allActions();
        if (sel < 0 || size_t(sel) >= actions.size()) return;
        const std::wstring path = bindsPath();
        const uapad::Profiles onDisk = uapad::parseProfiles(fileText(path));  // read again: the game may have saved
        uapad::Bindings b = g_who ? onDisk.forCharacter(g_who) : onDisk.fallback;
        b[i] = actions[size_t(sel)];
        const auto it = onDisk.characters.find(g_who);
        const std::string name = it != onDisk.characters.end() ? it->second.name : std::string();
        if (!writeFileAtomic(path, uapad::formatProfiles(uapad::withLayout(onDisk, g_who, name, b))))
            host_.message(tf(Text::PadFailed, {"uaro-pad-binds.ini"}), true);
    }

    // uaro-pad.ini (uaro-graphics\, core/layout) with one key changed; a missing file starts from the package's
    // template, with every key and its note.
    void write(const char* section, const char* key, const char* token) {
        const std::wstring& dir = host_.model().paths().dir;
        const std::wstring path = ourFile(dir, layout::File::PadIni);
        std::string text = fileText(path);
        if (text.empty()) text = fileText(ourPlace(dir, layout::kPadTemplate));
        if (!writeFileAtomic(path, applyIniEdits(text, {{section, key, token}})))
            host_.message(tf(Text::PadFailed, {"uaro-pad.ini"}), true);
    }

    std::unique_ptr<Page> rows_;  // the switch's row (a page of the schema's rows, as the Main page's)
};

}  // namespace

std::unique_ptr<Page> makePadPage(PageHost& host) { return std::make_unique<PadPage>(host); }

}  // namespace uagfx::app

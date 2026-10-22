#include "app/app_disk.hpp"
#include "app/files.hpp"
#include "app/page.hpp"
#include "app/setting_ids.hpp"
#include "core/layout.hpp"
#include "core/settings/ini_edit.hpp"
#include "core/settings/pad_files.hpp"
#include "core/version.hpp"

namespace uagfx::app {
namespace {

using namespace settings;

constexpr int kType = 1510;
constexpr int kLightbar = 1511;
constexpr int kTouchpad = 1512;
constexpr int kRumble = 1513;
constexpr int kStartsOn = 1514;
constexpr int kLanguage = 1515;
constexpr int kNpcHint = 1516;  // 0.7 of the pad

const char* const kTypes[] = {"auto", "xbox", "sony"};
const char* const kColours[] = {"blue", "red", "green", "purple", "orange", "white", "off"};
const char* const kTouch[] = {"none", "back", "start"};
const char* const kLanguages[] = {"en", "uk", "ru"};

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
        p.choice(kLanguage, t(Text::PadMenuLanguage), {L"English", L"Українська", L"Русский"},
                 indexOf(kLanguages, value("general.language", "en"), 0), t(Text::PadMenuLanguageHint));
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
            if (id == kType && sel < 3) write("pad", "type", kTypes[sel]);
            if (id == kLightbar && sel < 7) write("sony", "lightbar", kColours[sel]);
            if (id == kTouchpad && sel < 3) write("sony", "touchpad", kTouch[sel]);
            if (id == kLanguage && sel < 3) write("general", "language", kLanguages[sel]);
        }
    }

    void scroll(int id, int code, int pos) override { rows_->scroll(id, code, pos); }

private:
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

#include "app/files.hpp"
#include "app/page.hpp"
#include "core/settings/ini_edit.hpp"
#include "core/settings/pad_files.hpp"

namespace uagfx::app {
namespace {

using namespace settings;

constexpr int kSwitch = 1500;  // on, off or update, by what the folder holds
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

std::wstring inFolder(const wchar_t* name) { return exeFolder() + L"\\" + name; }
std::string fileText(const std::wstring& path) {
    std::string s;
    return readFile(path, s) ? s : std::string();
}
std::string trimmed(const std::string& s) {
    const size_t a = s.find_first_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}

// The Gamepad page (1.4): uaRO Pad - the package's uaro-graphics\pad\dinput.dll - put into the game's folder or taken
// out of it, and the settings of uaro-pad.ini a player looks for first, PlayStation's light bar and touchpad among
// them. They are written at once, not with Apply: the file is the pad's, the game reads it when it starts.
class PadPage : public Page {
public:
    explicit PadPage(PageHost& host) : Page(host) {}

    // 2.6: the Main page's box - on or off as this page's buttons do it
    std::wstring setOn(bool on) {
        state_ = classifyPad(fileText(inFolder(L"dinput.dll")), fileText(inFolder(L"uaro-graphics\\pad\\dinput.dll")));
        return on ? switchOn() : switchOff();
    }

    void build(Panel& p) override {
        const bool running = host_.model().now().gameRunning;
        const std::string packaged = fileText(inFolder(L"uaro-graphics\\pad\\dinput.dll"));
        const std::string version = trimmed(fileText(inFolder(L"uaro-graphics\\pad\\version.txt")));
        state_ = classifyPad(fileText(inFolder(L"dinput.dll")), packaged);
        p.heading(t(Text::PadHeading));
        p.note(t(Text::PadIntro), Tone::Normal);
        switch (state_) {
            case PadFile::None: p.note(t(Text::PadStateOff)); break;
            case PadFile::Same: p.note(tf(Text::PadStateOn, {version}), Tone::Good); break;
            case PadFile::Pad: p.note(tf(Text::PadStateOld, {version}), Tone::Warn); break;
            case PadFile::Other: p.note(t(Text::PadStateOther)); break;
        }
        const bool on = state_ == PadFile::Same || state_ == PadFile::Pad;
        std::vector<ButtonItem> buttons;
        if (state_ == PadFile::Pad) buttons.push_back({kSwitch + 2, t(Text::PadUpdate), !running && !packaged.empty()});
        if (on)
            buttons.push_back({kSwitch + 1, t(Text::PadSwitchOff), !running});
        else
            buttons.push_back({kSwitch, t(Text::PadSwitchOn), !running && !packaged.empty()});
        p.buttons(buttons);
        if (packaged.empty()) p.note(t(Text::PadNoPackage), Tone::Bad);
        if (running) p.note(t(Text::PadRunning), Tone::Warn);

        const auto ini = readIni(fileText(inFolder(L"uaro-pad.ini")));
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
        HWND ctl = GetDlgItem(host_.window(), id);
        if (id >= kSwitch && id <= kSwitch + 2 && code == BN_CLICKED) {
            const std::wstring err = id == kSwitch + 1 ? switchOff() : switchOn();
            host_.message(err.empty() ? t(id == kSwitch + 1 ? Text::PadOffDone : Text::PadOnDone) : err, !err.empty());
            host_.reload();  // the Diagnostics page lists the dinput DLLs too
            return;
        }
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

private:
    // uaro-pad.ini with one key changed; a missing file starts from the package's, with every key and its note.
    void write(const char* section, const char* key, const char* token) {
        const std::wstring path = inFolder(L"uaro-pad.ini");
        std::string text = fileText(path);
        if (text.empty()) text = fileText(inFolder(L"uaro-graphics\\pad\\uaro-pad.ini"));
        if (!writeFileAtomic(path, applyIniEdits(text, {{section, key, token}})))
            host_.message(tf(Text::PadFailed, {"uaro-pad.ini"}), true);
    }

    std::wstring failed(const char* file) const { return tf(Text::PadFailed, {file}); }

    // The package's dinput.dll into the game's folder. Another tool's dinput.dll (ROExt) becomes dinput8.dll: the
    // system dinput.dll the pad loads loads it from there, so it goes on working. Where the folder has a dinput8.dll
    // already, it waits as dinput.dll.before-pad. uaro-pad.moved next to it remembers which, for switchOff.
    std::wstring switchOn() {
        std::string packaged;
        if (!readFile(inFolder(L"uaro-graphics\\pad\\dinput.dll"), packaged)) return t(Text::PadNoPackage);
        if (state_ == PadFile::Other) {
            const bool free8 = !fileExists(inFolder(L"dinput8.dll"));
            const wchar_t* to = free8 ? L"dinput8.dll" : L"dinput.dll.before-pad";
            if (!MoveFileExW(inFolder(L"dinput.dll").c_str(), inFolder(to).c_str(), MOVEFILE_REPLACE_EXISTING))
                return failed("dinput.dll");
            writeFileAtomic(inFolder(L"uaro-pad.moved"), free8 ? "dinput8.dll" : "dinput.dll.before-pad");
        }
        if (!writeFileAtomic(inFolder(L"dinput.dll"), packaged)) return failed("dinput.dll");
        if (!fileExists(inFolder(L"uaro-pad.ini"))) {
            std::string ini;
            if (readFile(inFolder(L"uaro-graphics\\pad\\uaro-pad.ini"), ini))
                writeFileAtomic(inFolder(L"uaro-pad.ini"), ini);
        }
        return L"";
    }

    // Our dinput.dll out, and the one switchOn moved back in its place. uaro-pad.ini and the layouts stay.
    std::wstring switchOff() {
        if (!deleteFile(inFolder(L"dinput.dll"))) return failed("dinput.dll");
        const std::wstring note = inFolder(L"uaro-pad.moved");
        const std::string moved = trimmed(fileText(note));
        if (moved == "dinput8.dll" || moved == "dinput.dll.before-pad") {
            const std::wstring from = inFolder(moved == "dinput8.dll" ? L"dinput8.dll" : L"dinput.dll.before-pad");
            if (fileExists(from) && !MoveFileExW(from.c_str(), inFolder(L"dinput.dll").c_str(), 0))
                return failed(moved.c_str());
        }
        deleteFile(note);
        return L"";
    }

    PadFile state_ = PadFile::None;
};

}  // namespace

bool padOn() {
    const PadFile s =
        classifyPad(fileText(inFolder(L"dinput.dll")), fileText(inFolder(L"uaro-graphics\\pad\\dinput.dll")));
    return s == PadFile::Same || s == PadFile::Pad;
}

std::wstring padVersion() { return wide(trimmed(fileText(inFolder(L"uaro-graphics\\pad\\version.txt")))); }

std::wstring switchPad(PageHost& host, bool on) { return PadPage(host).setOn(on); }

std::unique_ptr<Page> makePadPage(PageHost& host) { return std::make_unique<PadPage>(host); }

}  // namespace uagfx::app

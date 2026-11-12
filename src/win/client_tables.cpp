#include "win/client_tables.hpp"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "core/buff_panel.hpp"
#include "core/hotbar_panel.hpp"
#include "core/pack_source.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/game_files.hpp"
#include "win/log.hpp"
#include "win/pack_pictures.hpp"
#include "win/paths.hpp"
#include "win/updates_hook.hpp"

namespace uagfx::win::clienttables {
namespace {

struct Tables {
    std::map<std::string, std::string> skills, items, files, unknown;
};

// 3.0.18-beta.188 (as Wind Walk's icon, R-.187): a table not read (the game's files busy, a map loading) read again a
// while later, a few times - not empty for the rest of the game
constexpr ULONGLONG kReadAgainMs = 3000;
constexpr int kMostReads = 6;

const Tables& tables() {
    static Tables t;
    static int reads = 0;
    static ULONGLONG lastRead = 0;
    const bool whole = !t.skills.empty() && !t.items.empty() && !t.files.empty() && !t.unknown.empty();
    const ULONGLONG now = GetTickCount64();
    if (reads > 0 && (whole || reads >= kMostReads || now - lastRead < kReadAgainMs)) return t;
    ++reads;
    lastRead = now;
    std::string text;
    const auto readInto = [&](std::map<std::string, std::string>& table, const char* entry) {
        if (table.empty() && readClientFile(entry, text)) table = hotbar::nameTable(text);
    };
    readInto(t.skills, "data\\skillnametable.txt");
    readInto(t.items, "data\\idnum2itemdisplaynametable.txt");
    readInto(t.files, "data\\idnum2itemresnametable.txt");
    readInto(t.unknown, "data\\num2itemdisplaynametable.txt");
    LOG_INFO("client tables: %zu skills' names, %zu items' names (%zu unidentified), %zu items' icons%s",
             t.skills.size(), t.items.size(), t.unknown.size(), t.files.size(), reads > 1 ? " (read again)" : "");
    return t;
}

}  // namespace

bool readClientFile(const std::string& entry, std::string& out) {
    if (readGameFile(entry, out)) return true;
    static std::unique_ptr<gamefiles::Reader> files;
    if (!files) files = std::make_unique<gamefiles::Reader>(gameDir());
    std::vector<uint8_t> bytes;
    if (!files->read(entry, bytes, nullptr)) return false;
    out.assign(bytes.begin(), bytes.end());
    return true;
}

const std::map<std::string, std::string>& skillNames() { return tables().skills; }
const std::map<std::string, std::string>& itemNames() { return tables().items; }
const std::map<std::string, std::string>& itemFiles() { return tables().files; }
const std::map<std::string, std::string>& unknownNames() { return tables().unknown; }

bool iconPixels(const std::string& file, std::vector<uint32_t>& argb, int& width, int& height, std::string* from) {
    static const std::string kUi = "\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA";  // 유저인터페이스
    argb.clear();
    width = height = 0;
    std::string bytes;
    if (file.empty() || !readClientFile("data\\texture\\" + kUi + "\\item\\" + file + ".bmp", bytes)) {
        if (from) *from = "not found";
        return false;
    }
    const auto* data = reinterpret_cast<const uint8_t*>(bytes.data());
    std::string name;
    if (packpictures::iconsStamp() > 0 && packpictures::ofInterface(data, bytes.size(), argb, width, height, &name)) {
        if (from) *from = "the AI texture pack (" + name + ")";
        return true;
    }
    if (!pack::decodeBmp(data, bytes.size(), argb, width, height)) {
        argb.clear();
        if (from) *from = "not a .bmp the driver reads";
        return false;
    }
    for (uint32_t& c : argb) {  // the game's magenta see-through (as its icons are keyed), the rest opaque
        const uint32_t r = (c >> 16) & 0xFF, g = (c >> 8) & 0xFF, b = c & 0xFF;
        c = r > 0xF0 && g < 0x10 && b > 0xF0 ? 0u : (c | 0xFF000000u);
    }
    if (from) *from = "the game's .bmp";
    return true;
}

namespace {
// A picture of the game's interface folder (data\texture\유저인터페이스\<folder>\<file>.bmp), its magenta clear
bool uiPixels(const char* folder, const std::string& file, std::vector<uint32_t>& argb, int& width, int& height) {
    static const std::string kUi = "\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA";  // 유저인터페이스
    argb.clear();
    width = height = 0;
    std::string bytes;
    if (file.empty() ||
        !readClientFile("data\\texture\\" + kUi + "\\" + folder + "\\" + file + ".bmp", bytes))
        return false;
    if (!pack::decodeBmp(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size(), argb, width, height)) {
        argb.clear();
        return false;
    }
    for (uint32_t& c : argb) {  // the game's magenta see-through
        const uint32_t r = (c >> 16) & 0xFF, g = (c >> 8) & 0xFF, b = c & 0xFF;
        c = r > 0xF0 && g < 0x10 && b > 0xF0 ? 0u : (c | 0xFF000000u);
    }
    return true;
}
}  // namespace

const cardnames::Tables& cardWords() {
    static cardnames::Tables t;
    static int reads = 0;  // 3.0.18-beta.188: read again a while later while none came (the game's files busy)
    static ULONGLONG lastRead = 0;
    const ULONGLONG now = GetTickCount64();
    if (reads == 0 || (t.words.empty() && t.after.empty() && reads < kMostReads && now - lastRead >= kReadAgainMs)) {
        ++reads;
        lastRead = now;
        std::string prefix, postfix;
        readClientFile("data\\cardprefixnametable.txt", prefix);
        readClientFile("data\\cardpostfixnametable.txt", postfix);
        t = cardnames::read(prefix, postfix);
        LOG_INFO("cards: the cards' words in names %zu, after the name %zu", t.words.size(), t.after.size());
    }
    return t;
}

bool collectionPixels(const std::string& file, std::vector<uint32_t>& argb, int& width, int& height) {
    return uiPixels("collection", file, argb, width, height);
}

bool cardArtPixels(const std::string& file, std::vector<uint32_t>& argb, int& width, int& height) {
    return uiPixels("cardbmp", file, argb, width, height);
}

const panel::Picture* icon(const std::string& file, int side, float radius) {
    struct Sized {
        float radius = 0;
        panel::Picture picture;  // made again in its place: the panels keep its address a frame
    };
    struct Read {
        int madeFor = -2;  // 3.0.11-beta.74: packpictures::iconsStamp() it was read for (-2 never)
        std::vector<uint32_t> argb;
        int w = 0, h = 0;
        std::map<std::pair<int, int>, Sized> sized;  // by the side and the radius (tenths)
        ULONGLONG retryAt = 0;  // 3.0.18-beta.188: not found - read again then (a few times)
        int retries = 0;
    };
    static std::map<std::string, Read> icons;
    static int told = 0, toldPack = 0;
    if (file.empty() || side <= 0) return nullptr;
    Read& r = icons[file];
    // 3.0.11-beta.74 (the owner: «иконки скилов, предметов ... грузить в текстур которые обработаны»): the AI texture
    // pack's picture (4x the game's) when it has one and [interface] pack_icons is on; read again when that changes
    const ULONGLONG now = GetTickCount64();
    if (const int want = packpictures::iconsStamp(); want != r.madeFor || (r.retryAt && now >= r.retryAt)) {
        const bool first = r.madeFor == -2;
        r.madeFor = want;
        r.retryAt = 0;
        std::string from;
        const bool found = iconPixels(file, r.argb, r.w, r.h, &from);
        if (!found && r.retries < 5) {  // 3.0.18-beta.188: again 3 s later - the game's files may have been busy
            ++r.retries;
            r.retryAt = now + 3000;
        }
        if (found && from.rfind("the AI", 0) == 0 && toldPack++ < 5)
            LOG_INFO("client tables: the icon %s.bmp from %s, %dx%d", file.c_str(), from.c_str(), r.w, r.h);
        if (!found && first && told++ < 20) LOG_INFO("client tables: the icon %s.bmp %s", file.c_str(), from.c_str());
        for (auto& [key, made] : r.sized)
            made.picture = r.argb.empty() ? panel::Picture{}
                                          : buffpanel::iconPicture(r.argb, r.w, r.h, true, key.first, made.radius);
    }
    if (r.argb.empty()) return nullptr;
    const auto key = std::make_pair(side, int(radius * 10));
    auto it = r.sized.find(key);
    if (it == r.sized.end())
        it = r.sized.emplace(key, Sized{radius, buffpanel::iconPicture(r.argb, r.w, r.h, true, side, radius)}).first;
    return &it->second.picture;
}

std::string skillAegis(int id) {
    constexpr uint32_t kNames = 0x74B9AC;
    constexpr int kMostSkill = 0x3FD;
    const auto read32 = [](uint32_t at) {
        uint32_t v = 0;
        return readGame(at, &v, 4) ? v : 0u;
    };
    const auto text = [](uint32_t at) {
        char buf[48] = {};
        if (!at || !readGame(at, buf, sizeof buf - 1)) return std::string();
        buf[sizeof buf - 1] = 0;
        return std::string(buf);
    };
    static int known = -1;
    if (known < 0) {
        const uint32_t names = read32(kNames);
        known = names && text(read32(names + 4)) == "NV_BASIC" ? 1 : 0;
        if (!known) LOG_INFO("client tables: the game's skills' names are not where this build has them");
    }
    if (!known || id <= 0 || id >= kMostSkill) return "";
    return text(read32(read32(kNames) + 4 * uint32_t(id)));
}

std::string skillWords(int id, unsigned codePage) {
    const std::string aegis = skillAegis(id);
    if (aegis.empty()) return "";
    const auto& names = skillNames();
    const auto n = names.find(aegis);
    return n != names.end() ? utf8Of(widened(hotbar::spaced(n->second), codePage)) : aegis;
}

}  // namespace uagfx::win::clienttables

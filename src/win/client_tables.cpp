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

const Tables& tables() {
    static const Tables t = [] {
        Tables made;
        std::string text;
        if (readClientFile("data\\skillnametable.txt", text)) made.skills = hotbar::nameTable(text);
        if (readClientFile("data\\idnum2itemdisplaynametable.txt", text)) made.items = hotbar::nameTable(text);
        if (readClientFile("data\\idnum2itemresnametable.txt", text)) made.files = hotbar::nameTable(text);
        if (readClientFile("data\\num2itemdisplaynametable.txt", text)) made.unknown = hotbar::nameTable(text);
        LOG_INFO("client tables: %zu skills' names, %zu items' names (%zu unidentified), %zu items' icons",
                 made.skills.size(), made.items.size(), made.unknown.size(), made.files.size());
        return made;
    }();
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
    };
    static std::map<std::string, Read> icons;
    static int told = 0, toldPack = 0;
    if (file.empty() || side <= 0) return nullptr;
    Read& r = icons[file];
    // 3.0.11-beta.74 (the owner: «иконки скилов, предметов ... грузить в текстур которые обработаны»): the AI texture
    // pack's picture (4x the game's) when it has one and [interface] pack_icons is on; read again when that changes
    if (const int want = packpictures::iconsStamp(); want != r.madeFor) {
        const bool first = r.madeFor == -2;
        r.madeFor = want;
        std::string from;
        const bool found = iconPixels(file, r.argb, r.w, r.h, &from);
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

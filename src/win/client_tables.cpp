#include "win/client_tables.hpp"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "core/buff_panel.hpp"
#include "core/hotbar_panel.hpp"
#include "core/pack_source.hpp"
#include "win/game_files.hpp"
#include "win/log.hpp"
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

const panel::Picture* icon(const std::string& file, int side, float radius) {
    struct Read {
        bool looked = false;
        std::vector<uint32_t> argb;
        int w = 0, h = 0;
        std::map<std::pair<int, int>, panel::Picture> sized;  // by the side and the radius (tenths)
    };
    static std::map<std::string, Read> icons;
    static int told = 0;
    if (file.empty() || side <= 0) return nullptr;
    Read& r = icons[file];
    if (!r.looked) {
        r.looked = true;
        static const std::string kUi = "\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA";  // 유저인터페이스
        std::string bytes;
        const std::string entry = "data\\texture\\" + kUi + "\\item\\" + file + ".bmp";
        const bool read = readClientFile(entry, bytes);
        if (!read || !pack::decodeBmp(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size(), r.argb, r.w, r.h)) {
            r.argb.clear();
            if (told++ < 20)
                LOG_INFO("client tables: the icon %s.bmp %s", file.c_str(),
                         read ? "not a .bmp the driver reads" : "not found");
        }
    }
    if (r.argb.empty()) return nullptr;
    const auto key = std::make_pair(side, int(radius * 10));
    auto it = r.sized.find(key);
    if (it == r.sized.end())
        it = r.sized.emplace(key, buffpanel::iconPicture(r.argb, r.w, r.h, false, side, radius)).first;
    return &it->second;
}

}  // namespace uagfx::win::clienttables

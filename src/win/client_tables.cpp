#include "win/client_tables.hpp"

#include <cstdint>
#include <memory>
#include <vector>

#include "core/hotbar_panel.hpp"
#include "win/game_files.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"
#include "win/updates_hook.hpp"

namespace uagfx::win::clienttables {
namespace {

struct Tables {
    std::map<std::string, std::string> skills, items, files;
};

const Tables& tables() {
    static const Tables t = [] {
        Tables made;
        std::string text;
        if (readClientFile("data\\skillnametable.txt", text)) made.skills = hotbar::nameTable(text);
        if (readClientFile("data\\idnum2itemdisplaynametable.txt", text)) made.items = hotbar::nameTable(text);
        if (readClientFile("data\\idnum2itemresnametable.txt", text)) made.files = hotbar::nameTable(text);
        LOG_INFO("client tables: %zu skills' names, %zu items' names, %zu items' icons", made.skills.size(),
                 made.items.size(), made.files.size());
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

}  // namespace uagfx::win::clienttables

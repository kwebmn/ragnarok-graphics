#include "studio/pack_edit.hpp"

#include <windows.h>

#include <cstdio>

#include "app/pack_compact.hpp"
#include "app/text.hpp"
#include "core/studio_link.hpp"
#include "studio/places.hpp"

namespace uagfx::studio {

bool packEdit(const std::string& pack, const PackChange& change, std::string& error) {
    app::PackLock lock;
    if (!lock.take(error)) return false;
    pack::Index index;
    {
        pack::Reader r;  // its index, before the writer drops it to go on
        std::string why;
        std::vector<uint8_t> bytes;
        if (r.open(pack, why) && r.read(pack::kIndexName, bytes))
            index = pack::parseIndexFile(std::string(bytes.begin(), bytes.end()));
    }
    pack::Writer w;
    bool ok = w.open(pack, true, error);
    if (ok) {  // whatever the change did, the pack is whole again (its directory, its index)
        std::string why, closing;
        const bool changed = change(w, index, why);
        ok = w.close(pack::indexText(index), closing) && changed;
        error = !changed ? why : closing;
    }
    return ok;
}

void tellGame(const std::vector<std::string>& keys) {
    const std::wstring folder = linkFolder(), path = folder + L"\\changed.txt";
    makeFolder(folder);
    const std::string text = changedText(keys);
    if (std::FILE* f = _wfopen((path + L".part").c_str(), L"wb")) {
        const bool ok = std::fwrite(text.data(), 1, text.size(), f) == text.size();
        if (std::fclose(f) == 0 && ok) MoveFileExW((path + L".part").c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING);
    }
    // The game's driver made the event (it looks at it once a frame); no game running: no event, nothing to tell.
    if (HANDLE e = OpenEventW(EVENT_MODIFY_STATE, FALSE, app::wide(kChangedEvent).c_str())) {
        SetEvent(e);
        CloseHandle(e);
    }
}

}  // namespace uagfx::studio

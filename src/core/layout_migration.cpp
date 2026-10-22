#include "core/layout_migration.hpp"

namespace uagfx::layout {
namespace {

std::string parentOf(const std::string& relative) {
    const size_t slash = relative.find_last_of('\\');
    return slash == std::string::npos ? std::string() : relative.substr(0, slash);
}

class Move {
public:
    Move(const std::string& gameDir, Disk& disk) : dir_(gameDir), disk_(disk) {}

    std::vector<std::string>& lines() { return lines_; }

    // The folders of a relative one, each level from the game's folder down.
    void makeFolders(const std::string& relative) {
        size_t at = 0;
        while (at != std::string::npos) {
            at = relative.find('\\', at + 1);
            disk_.makeFolder(join(dir_, relative.substr(0, at)));
        }
    }

    bool there(const std::string& path) { return disk_.isFile(path) || disk_.isFolder(path); }

    // One rename; when it fails because the other program (the game's driver, the settings app) moved it meanwhile,
    // nothing to say.
    void rename(const std::string& from, const std::string& to, bool replace, const std::string& shown) {
        const std::string a = join(dir_, from), b = join(dir_, to);
        if (disk_.move(a, b, replace)) {
            lines_.push_back("layout: moved " + shown + " to " + to);
        } else if (!there(a) && there(b)) {
            // moved by the other program
        } else {
            lines_.push_back("layout: could not move " + shown + " - it is used where it is");
        }
    }

    void file(File f) {
        const std::string from = before(f), to = now(f);
        if (!disk_.isFile(join(dir_, from))) return;
        makeFolders(parentOf(to));
        // The texture pack is never replaced: two of them stay, the new place's used.
        if (f == File::Pack && disk_.isFile(join(dir_, to))) {
            lines_.push_back("layout: two texture packs - " + to + " is used, " + from + " is left");
            return;
        }
        // The settings: what is at the new place in a 2.x install is the package's template, the player's root
        // file replaces it. Logs and caches: the newer of the game's start.
        rename(from, to, f != File::Pack, from);
    }

    void folder(File f) {
        const std::string from = before(f), to = now(f);
        if (!disk_.isFolder(join(dir_, from))) return;
        makeFolders(parentOf(to));
        if (!there(join(dir_, to))) {
            rename(from, to, false, from);
            return;
        }
        for (const std::string& name : disk_.names(join(dir_, from))) {
            const std::string inFrom = from + "\\" + name, inTo = to + "\\" + name;
            if (!there(join(dir_, inTo))) rename(inFrom, inTo, false, inFrom);
        }
        if (disk_.removeEmptyFolder(join(dir_, from))) lines_.push_back("layout: moved " + from + " into " + to);
    }

    // 2.x put its notes for the tester into the root; they are in uaro-graphics\notes now. Only ours go.
    void notes() {
        static const char kOursStart[] = "# uaRO Graphics";
        for (const char* name : {"TEST.en.md", "TEST.uk.md", "TEST.ru.md"}) {
            const std::string path = join(dir_, name);
            std::string start;
            if (!disk_.isFile(path) || !disk_.readStart(path, sizeof kOursStart - 1, start)) continue;
            if (start == kOursStart && disk_.removeFile(path))
                lines_.push_back(std::string("layout: removed ") + name + " - the notes are in uaro-graphics\\notes");
        }
    }

private:
    std::string dir_;
    Disk& disk_;
    std::vector<std::string> lines_;
};

}  // namespace

std::vector<std::string> migrate(const std::string& gameDir, Disk& disk) {
    Move m(gameDir, disk);
    for (int i = 0; i < kFileCount; ++i) {
        const File f = File(i);
        if (isFolder(f))
            m.folder(f);
        else
            m.file(f);
    }
    m.notes();
    return m.lines();
}

}  // namespace uagfx::layout

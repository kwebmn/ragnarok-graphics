// 2.8: a 2.x install's files move into uaro-graphics\ (core/layout_migration) - over a file system in memory.
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "core/layout.hpp"
#include "core/layout_migration.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

std::string parentOf(const std::string& p) {
    const size_t slash = p.find_last_of('\\');
    return slash == std::string::npos ? std::string() : p.substr(0, slash);
}

bool under(const std::string& p, const std::string& folder) { return p.rfind(folder + "\\", 0) == 0; }

// Folders and files in memory, as Windows treats them: no file into a missing folder, a rename in one step.
struct MemoryDisk : layout::Disk {
    std::map<std::string, std::string> files;
    std::set<std::string> folders;
    std::set<std::string> locked;                        // held open: their moves fail
    std::function<void(const std::string&)> beforeMove;  // the other program, once, just before a move

    bool isFile(const std::string& p) override { return files.count(p) > 0; }
    bool isFolder(const std::string& p) override { return folders.count(p) > 0; }
    bool makeFolder(const std::string& p) override {
        if (folders.count(p)) return true;
        if (!folders.count(parentOf(p)) || files.count(p)) return false;
        folders.insert(p);
        return true;
    }
    bool move(const std::string& from, const std::string& to, bool replace) override {
        if (beforeMove) {
            const auto other = beforeMove;
            beforeMove = nullptr;
            other(from);
        }
        if (locked.count(from) || !folders.count(parentOf(to)) || folders.count(to)) return false;
        if (files.count(from)) {
            if (files.count(to) && !replace) return false;
            files[to] = files[from];
            files.erase(from);
            return true;
        }
        if (!folders.count(from) || files.count(to)) return false;
        std::map<std::string, std::string> keptFiles;
        for (const auto& f : files)
            keptFiles[under(f.first, from) ? to + f.first.substr(from.size()) : f.first] = f.second;
        std::set<std::string> keptFolders;
        for (const std::string& d : folders)
            keptFolders.insert(d == from || under(d, from) ? to + d.substr(from.size()) : d);
        files = keptFiles;
        folders = keptFolders;
        return true;
    }
    std::vector<std::string> names(const std::string& folder) override {
        std::set<std::string> out;
        for (const auto& f : files)
            if (parentOf(f.first) == folder) out.insert(f.first.substr(folder.size() + 1));
        for (const std::string& d : folders)
            if (parentOf(d) == folder) out.insert(d.substr(folder.size() + 1));
        return {out.begin(), out.end()};
    }
    bool removeFile(const std::string& p) override { return files.erase(p) > 0; }
    bool removeEmptyFolder(const std::string& p) override { return names(p).empty() && folders.erase(p) > 0; }
    bool readStart(const std::string& p, size_t most, std::string& out) override {
        const auto it = files.find(p);
        if (it == files.end()) return false;
        out = it->second.substr(0, most);
        return true;
    }
};

const std::string kGame = "C:\\uaRO";

std::string at(const std::string& relative) { return kGame + "\\" + relative; }

// The game's folder as 2.7.58 left it.
MemoryDisk twoX() {
    MemoryDisk d;
    d.folders = {"C:", kGame, at("uaro-graphics")};
    d.files[at("uaro-graphics.ini")] = "[quality]\r\nmsaa = 2\r\n";  // the player's
    d.files[at("uaro-graphics\\uaro-graphics.ini")] = "; template";  // the 2.x package's
    d.files[at("uaro-graphics.log")] = "log";
    d.files[at("uaro-render64.log")] = "log64";
    d.files[at("uaro-graphics-pipelines.txt")] = "pipelines";
    d.files[at("uaro-graphics-report.txt")] = "report";
    d.files[at("uaro-textures.zip")] = "pack";
    d.files[at("uaro-graphics\\shaders-32.bin")] = "shaders";
    return d;
}

bool anyLine(const std::vector<std::string>& lines, const std::string& part) {
    for (const std::string& l : lines)
        if (l.find(part) != std::string::npos) return true;
    return false;
}

}  // namespace

TEST(migration_moves_a_2x_install_into_uaro_graphics) {
    MemoryDisk d = twoX();
    CHECK(!layout::migrate(kGame, d).empty());
    CHECK_EQ(d.files[at("uaro-graphics\\uaro-graphics.ini")], std::string("[quality]\r\nmsaa = 2\r\n"));
    CHECK(d.files.count(at("uaro-graphics\\logs\\uaro-graphics.log")) > 0);
    CHECK(d.files.count(at("uaro-graphics\\logs\\uaro-render64.log")) > 0);
    CHECK(d.files.count(at("uaro-graphics\\logs\\uaro-graphics-report.txt")) > 0);
    CHECK(d.files.count(at("uaro-graphics\\cache\\pipelines.txt")) > 0);
    CHECK(d.files.count(at("uaro-graphics\\cache\\shaders-32.bin")) > 0);
    CHECK_EQ(d.files[at("uaro-graphics\\textures\\uaro-textures.zip")], std::string("pack"));
    for (const char* old : {"uaro-graphics.ini", "uaro-graphics.log", "uaro-render64.log", "uaro-textures.zip",
                            "uaro-graphics-pipelines.txt", "uaro-graphics-report.txt", "uaro-graphics\\shaders-32.bin"})
        CHECK(d.files.count(at(old)) == 0);
    CHECK(layout::migrate(kGame, d).empty());  // a second start finds nothing to do
}

TEST(migration_merges_the_backup_and_dgvoodoo_folders) {
    MemoryDisk d;
    d.folders = {"C:", kGame, at("uaro-graphics"), at("uaro-graphics-backup"),
                 at("uaro-graphics-backup\\2026-10-01-120000"), at("uaro-graphics\\backup"),
                 at("uaro-graphics\\backup\\2026-10-02-120000"), at("dgvoodoo")};
    d.files[at("uaro-graphics-backup\\2026-10-01-120000\\files.txt")] = "a";
    d.files[at("uaro-graphics\\backup\\2026-10-02-120000\\files.txt")] = "b";
    d.files[at("dgvoodoo\\DDraw.dll")] = "dg";
    layout::migrate(kGame, d);
    CHECK_EQ(d.files[at("uaro-graphics\\backup\\2026-10-01-120000\\files.txt")], std::string("a"));
    CHECK_EQ(d.files[at("uaro-graphics\\backup\\2026-10-02-120000\\files.txt")], std::string("b"));
    CHECK(d.folders.count(at("uaro-graphics-backup")) == 0);
    CHECK_EQ(d.files[at("uaro-graphics\\dgvoodoo\\DDraw.dll")], std::string("dg"));
    CHECK(d.folders.count(at("dgvoodoo")) == 0);
}

TEST(migration_keeps_both_texture_packs_and_says_so) {
    MemoryDisk d = twoX();
    d.folders.insert(at("uaro-graphics\\textures"));
    d.files[at("uaro-graphics\\textures\\uaro-textures.zip")] = "new pack";
    const auto lines = layout::migrate(kGame, d);
    CHECK_EQ(d.files[at("uaro-textures.zip")], std::string("pack"));
    CHECK_EQ(d.files[at("uaro-graphics\\textures\\uaro-textures.zip")], std::string("new pack"));
    CHECK(anyLine(lines, "two texture packs"));
}

TEST(migration_leaves_what_it_cannot_move_and_locate_keeps_it) {
    MemoryDisk d = twoX();
    d.locked.insert(at("uaro-textures.zip"));  // uaro-render64 makes the pack meanwhile
    const auto lines = layout::migrate(kGame, d);
    CHECK_EQ(d.files[at("uaro-textures.zip")], std::string("pack"));
    CHECK(anyLine(lines, "could not move uaro-textures.zip"));
    const auto exists = [&](const std::string& p) { return d.isFile(p) || d.isFolder(p); };
    CHECK_EQ(layout::locate(kGame, layout::File::Pack, exists), at("uaro-textures.zip"));
    CHECK_EQ(layout::locate(kGame, layout::File::Ini, exists), at("uaro-graphics\\uaro-graphics.ini"));
}

TEST(migration_running_at_once_in_two_programs_loses_nothing) {
    MemoryDisk d = twoX();
    d.beforeMove = [&](const std::string&) {  // the game's driver moves the ini while the settings app starts
        d.files[at("uaro-graphics\\uaro-graphics.ini")] = d.files[at("uaro-graphics.ini")];
        d.files.erase(at("uaro-graphics.ini"));
    };
    const auto lines = layout::migrate(kGame, d);
    CHECK_EQ(d.files[at("uaro-graphics\\uaro-graphics.ini")], std::string("[quality]\r\nmsaa = 2\r\n"));
    CHECK(!anyLine(lines, "could not move"));
}

TEST(migration_removes_only_our_test_notes) {
    MemoryDisk d;
    d.folders = {"C:", kGame};
    d.files[at("TEST.ru.md")] = "# uaRO Graphics 2.7.58 — проверка\n\n- ...";
    d.files[at("TEST.en.md")] = "my notes";
    const auto lines = layout::migrate(kGame, d);
    CHECK(d.files.count(at("TEST.ru.md")) == 0);
    CHECK_EQ(d.files[at("TEST.en.md")], std::string("my notes"));
    CHECK(anyLine(lines, "removed TEST.ru.md"));
}

TEST(migration_leaves_the_client_files_and_the_bats) {
    MemoryDisk d = twoX();
    for (const char* own : {"uaro-flags.grf", "update.bat", "dinput.dll", "uaro-pad.ini", "DATA.INI"})
        d.files[at(own)] = own;
    layout::migrate(kGame, d);
    for (const char* own : {"uaro-flags.grf", "update.bat", "dinput.dll", "uaro-pad.ini", "DATA.INI"})
        CHECK_EQ(d.files[at(own)], std::string(own));
}

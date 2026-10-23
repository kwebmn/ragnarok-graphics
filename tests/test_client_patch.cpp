// 2.14: the client patcher - the site's manifest (the client build's shared format 1) as core/client_patch reads it.
// 3.0.1: the client (uaro-client.json), the player's own files, the files withdrawn, the record of what was made.
#include <map>
#include <string>
#include <vector>

#include "core/client_patch.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

const std::string kSha(64, 'a');

std::string entry(const std::string& path, int size = 5) {
    return "{\"path\":\"" + path + "\",\"size\":" + std::to_string(size) + ",\"sha256\":\"" + kSha + "\"}";
}

client_patch::File file(const std::string& path, char sha, uint64_t size, bool made = false) {
    client_patch::File f;
    f.path = path;
    f.sha256 = std::string(64, sha);
    f.size = size;
    f.made = made;
    return f;
}

// The game's folder for `plan`: a path's file (its sha256 given only when `plan` asks at its size) and what was hashed
struct Disk {
    std::map<std::string, client_patch::Have> files;
    std::vector<std::string> hashed;
    client_patch::Look look() {
        return [this](const std::string& path, const std::vector<uint64_t>& sizes) {
            const auto it = files.find(path);
            if (it == files.end()) return client_patch::Have{};
            client_patch::Have h = it->second;
            bool asked = false;
            for (uint64_t size : sizes) asked = asked || size == h.size;
            if (asked)
                hashed.push_back(path);
            else
                h.sha256.clear();
            return h;
        };
    }
};

std::vector<std::string> paths(const std::vector<client_patch::File>& files) {
    std::vector<std::string> out;
    for (const client_patch::File& f : files) out.push_back(f.path);
    return out;
}

using Paths = std::vector<std::string>;

}  // namespace

TEST(client_patch_reads_the_shared_manifest) {
    const std::string json = "{\"format\":1,\"client\":\"uaRO\",\"version\":\"p0123456789ab\",\"built\":\"2026-10-22T"
                             "21:00:00Z\",\"files\":[" + entry("data\\/texture\\/scr_logo.bmp", 5278) + "," +
                             entry("uaro-flags.grf") + "]}";
    client_patch::Manifest m;
    std::string error;
    CHECK(client_patch::parse(json, m, error));
    CHECK_EQ(m.client, std::string("uaRO"));
    CHECK_EQ(m.version, std::string("p0123456789ab"));
    CHECK_EQ(m.files.size(), size_t(2));
    CHECK_EQ(m.files[0].path, std::string("data/texture/scr_logo.bmp"));
    CHECK(m.files[0].size == 5278 && m.files[0].sha256 == kSha);
    CHECK_EQ(client_patch::localPath(m.files[0].path), std::string("data\\texture\\scr_logo.bmp"));
    CHECK(!client_patch::parse("{\"format\":2,\"files\":[]}", m, error));  // a format this app does not know
    CHECK(!client_patch::parse("<html>", m, error));
    CHECK(client_patch::parse("{\"format\":1,\"version\":\"p1\",\"files\":[]}", m, error) && m.files.empty());
}

TEST(client_patch_refuses_unsafe_paths) {
    for (const char* bad : {"DATA.INI", "data.ini", "../x.bmp", "data/../../x.bmp", "ddraw.dll", "DDraw.dll",
                            "uaro-graphics/x.ini", "Uaro-Graphics/textures/a.zip", "dgvoodoo/DDraw.dll", "data/x.exe",
                            "data/X.DLL", "ScreenShot/a.jpg", "install.bat", "uaro-graphics.exe", "dinput.dll",
                            "TEST.uk.md", "uaro-pad.log", "", "C:/Windows/x.bmp", "/data/x.bmp", "data//x.bmp",
                            "data/x.bmp.", "data/x.bmp ", "data/\xD1\x82.bmp", "data\\x.bmp"})
        CHECK(!client_patch::safePath(bad));
    for (const char* good : {"data/texture/scr_logo.bmp", "uaro-flags.grf", "data/sprite/a (1).spr", "BGM/01.mp3"})
        CHECK(client_patch::safePath(good));
    // an unsafe entry in the site's list is dropped, the rest stays
    const std::string json = "{\"format\":1,\"version\":\"p2\",\"files\":[" + entry("..\\/evil.bmp") + "," +
                             entry("DATA.INI") + "," + entry("data\\/ok.bmp") + "]}";
    client_patch::Manifest m;
    std::string error;
    CHECK(client_patch::parse(json, m, error));
    CHECK(m.files.size() == 1 && m.files[0].path == "data/ok.bmp");
}

// 3.0.1: which client this is - the build's own manifest in its root, uaro-client.json; a name for files and keys
TEST(client_patch_reads_the_client) {
    client_patch::Manifest m;
    std::string error;
    const std::string json = "{\"format\":1,\"client\":\"uaRO HD\",\"version\":\"2026.10.6\",\"files\":[]}";
    CHECK(client_patch::parse(json, m, error));
    CHECK_EQ(m.client, std::string("uaRO HD"));
    CHECK_EQ(m.version, std::string("2026.10.6"));
    CHECK_EQ(client_patch::clientName("uaRO HD"), std::string("uaRO HD"));
    CHECK_EQ(client_patch::clientName("uaRO (test)"), std::string("uaRO (test)"));
    CHECK_EQ(client_patch::clientName(std::string(40, 'a')), std::string(40, 'a'));
    for (const std::string& bad : Paths{"", "a/b", "a\\b", "x.", " x", "x ", ".x", "uaRO:HD", "\xD1\x82", "CON", "nul",
                                        "Com1", std::string(41, 'a')})
        CHECK_EQ(client_patch::clientName(bad), std::string("uaRO"));
    CHECK(client_patch::sameClient("uaro hd", "uaRO HD"));
    CHECK(client_patch::sameClient("", "uaRO"));
    CHECK(!client_patch::sameClient("uaRO", "uaRO HD"));
}

TEST(client_patch_refuses_the_client_file) {
    CHECK(!client_patch::safePath("uaro-client.json"));
    CHECK(!client_patch::safePath("UARO-CLIENT.JSON"));
    CHECK(client_patch::safePath("data/uaro-client.json"));  // only the root's is the build's
}

TEST(client_patch_puts_what_differs) {
    client_patch::Manifest next;
    next.files = {file("data/a.bmp", 'a', 5), file("data/b.bmp", 'a', 5), file("data/c.bmp", 'a', 5),
                  file("data/d.bmp", 'a', 5), file("data/e.bmp", 'a', 5)};
    Disk disk;
    disk.files["data/a.bmp"] = {true, 5, kSha};                    // as listed
    disk.files["data/b.bmp"] = {true, 4, kSha};                    // another size: not hashed
    disk.files["data/c.bmp"] = {true, 5, std::string(64, 'f')};  // the same size, another file
    disk.files["data/e.bmp"] = {true, 5, ""};                      // unreadable: its size trusted
    const client_patch::Plan p = client_patch::plan(next, {}, {}, disk.look());  // d: missing
    CHECK((paths(p.put) == Paths{"data/b.bmp", "data/c.bmp", "data/d.bmp"}));
    CHECK(p.kept.empty() && p.remove.empty());
    CHECK((disk.hashed == Paths{"data/a.bmp", "data/c.bmp", "data/e.bmp"}));
}

// 3.0.1 (the client build's report): Palettes.grf and dinput8.ini are the player's to change - «Палітри гри» makes a
// 4 MB Palettes.grf of the 101 MB one; a patch replaces them only while they are as the build or the last patch left
TEST(the_players_own_files_stay) {
    CHECK(client_patch::playersOwn("Palettes.grf") && client_patch::playersOwn("palettes.GRF"));
    CHECK(client_patch::playersOwn("dinput8.ini"));
    CHECK(!client_patch::playersOwn("data/Palettes.grf") && !client_patch::playersOwn("data.grf"));
    client_patch::Manifest next, last, build;
    next.files = {file("Palettes.grf", 'n', 101)};
    Disk none;
    CHECK((paths(client_patch::plan(next, last, build, none.look()).put) == Paths{"Palettes.grf"}));  // missing
    build.files = {file("PALETTES.GRF", 'b', 100)};
    Disk asBuilt;
    asBuilt.files["Palettes.grf"] = {true, 100, std::string(64, 'b')};
    CHECK((paths(client_patch::plan(next, last, build, asBuilt.look()).put) == Paths{"Palettes.grf"}));
    last.files = {file("Palettes.grf", 'l', 99)};
    Disk asPatched;
    asPatched.files["Palettes.grf"] = {true, 99, std::string(64, 'l')};
    CHECK((paths(client_patch::plan(next, last, build, asPatched.look()).put) == Paths{"Palettes.grf"}));
    Disk own;  // the player's own: kept, and not even read
    own.files["Palettes.grf"] = {true, 4, std::string(64, 'p')};
    client_patch::Plan p = client_patch::plan(next, last, build, own.look());
    CHECK(p.put.empty() && (paths(p.kept) == Paths{"Palettes.grf"}) && own.hashed.empty());
    Disk ownOfAKnownSize;
    ownOfAKnownSize.files["Palettes.grf"] = {true, 100, std::string(64, 'x')};
    p = client_patch::plan(next, last, build, ownOfAKnownSize.look());
    CHECK(p.put.empty() && p.kept.size() == 1);
    Disk listed;  // as the list has it: nothing to do
    listed.files["Palettes.grf"] = {true, 101, std::string(64, 'n')};
    p = client_patch::plan(next, last, build, listed.look());
    CHECK(p.put.empty() && p.kept.empty());
}

// 3.0.1: a file the admin took off the list goes from the player's folder only when the patch made it (nothing was at
// its place), it is unchanged since, and the build does not list it - a patch over data/sclientinfo.xml never takes
// the client's file away
TEST(a_withdrawn_file_goes_only_when_the_patch_made_it) {
    client_patch::Manifest next, last, build;
    next.files = {file("data/kept.bmp", 'k', 5), file("data/moved.bmp", 'v', 5)};
    last.files = {file("data/kept.bmp", 'k', 5, true),     file("data/made.bmp", 'm', 5, true),
                  file("data/sclientinfo.xml", 'o', 5),     file("data/changed.bmp", 'c', 5, true),
                  file("data/built.bmp", 'b', 5, true),     file("data/Moved.bmp", 'v', 5, true),
                  file("data/gone.bmp", 'g', 5, true)};
    build.files = {file("data/built.bmp", 'b', 5)};
    Disk disk;
    for (const auto& f : std::map<std::string, char>{{"data/kept.bmp", 'k'}, {"data/made.bmp", 'm'},
                                                     {"data/sclientinfo.xml", 'o'}, {"data/changed.bmp", 'x'},
                                                     {"data/built.bmp", 'b'}, {"data/moved.bmp", 'v'}})
        disk.files[f.first] = {true, 5, std::string(64, f.second)};
    const client_patch::Plan p = client_patch::plan(next, last, build, disk.look());  // gone.bmp: not there
    CHECK((paths(p.remove) == Paths{"data/made.bmp"}));
    CHECK(p.put.empty());
}

TEST(client_patch_carries_what_it_made) {
    client_patch::Manifest last, next;
    last.files = {file("data/A.bmp", 'a', 5, true), file("data/b.bmp", 'b', 5)};
    next.files = {file("data/a.bmp", 'n', 6), file("data/b.bmp", 'b', 5), file("data/c.bmp", 'c', 5, true)};
    client_patch::carryMade(next, last);
    CHECK(next.files[0].made && !next.files[1].made && !next.files[2].made);
}

TEST(client_patch_record_round_trip) {
    client_patch::Manifest m;
    m.client = "uaRO HD";
    m.version = "p4";
    m.files = {file("data/texture/scr_logo.bmp", 'a', 5278, true), file("data/a b.bmp", 'b', 7),
               file("Palettes.grf", 'e', 101)};
    const std::string rec = client_patch::record(m);
    client_patch::Manifest back;
    CHECK(client_patch::readRecord(rec, back));
    CHECK(back.client == "uaRO HD" && back.version == "p4" && back.files.size() == 3);
    CHECK(back.files[0].made && !back.files[1].made);
    CHECK(back.files[1].path == "data/a b.bmp" && back.files[1].size == 7);
    CHECK(back.files[1].sha256 == std::string(64, 'b'));
    CHECK(client_patch::readRecord("version p1\n5278 " + kSha + " data/x y.bmp\n", back));  // 3.0.0's record
    CHECK(back.client.empty() && back.files.size() == 1 && back.files[0].path == "data/x y.bmp");
    CHECK(!back.files[0].made && back.files[0].size == 5278);
    CHECK(!client_patch::readRecord("garbage", back));
    const auto sizeOf = [](const std::string& path) -> long long {
        if (path == "data/texture/scr_logo.bmp") return 5278;
        if (path == "data/a b.bmp") return 7;
        return path == "Palettes.grf" ? 4 : -1;  // the player's own of another size: still up to date
    };
    CHECK(client_patch::upToDate(m, rec, sizeOf));
    client_patch::Manifest flags = m;  // what the patch made is no part of the list
    flags.files[0].made = false;
    CHECK(client_patch::upToDate(flags, rec, sizeOf));
    client_patch::Manifest other = m;
    other.client = "uaRO";
    CHECK(!client_patch::upToDate(other, rec, sizeOf));  // another client's list
    client_patch::Manifest newer = m;
    newer.version = "p5";
    CHECK(!client_patch::upToDate(newer, rec, sizeOf));  // another list: looked at again
    CHECK(!client_patch::upToDate(m, rec, [&](const std::string& p) { return p == "data/a b.bmp" ? 8 : sizeOf(p); }));
    CHECK(!client_patch::upToDate(m, rec, [&](const std::string& p) { return p == "Palettes.grf" ? -1 : sizeOf(p); }));
    CHECK(!client_patch::upToDate(m, "", sizeOf));
}

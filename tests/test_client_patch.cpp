// 2.14: the client patcher - the site's manifest (the client build's shared format 1) as core/client_patch reads it.
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

}  // namespace

TEST(client_patch_reads_the_shared_manifest) {
    const std::string json = "{\"format\":1,\"client\":\"uaRO\",\"version\":\"p0123456789ab\",\"built\":\"2026-10-22T"
                             "21:00:00Z\",\"files\":[" + entry("data\\/texture\\/scr_logo.bmp", 5278) + "," +
                             entry("uaro-flags.grf") + "]}";
    client_patch::Manifest m;
    std::string error;
    CHECK(client_patch::parse(json, m, error));
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

TEST(client_patch_fetches_only_what_differs) {
    client_patch::Manifest m;
    m.version = "p3";
    m.files = {{"data/a.bmp", kSha, 5}, {"data/b.bmp", kSha, 5}, {"data/c.bmp", kSha, 5}, {"data/d.bmp", kSha, 5}};
    const auto have = [](const client_patch::File& f) {
        client_patch::Have h;
        if (f.path == "data/a.bmp") h = {true, 5, ""};                 // there: hashed by `needed`'s caller
        if (f.path == "data/b.bmp") h = {true, 4, ""};                 // another size
        if (f.path == "data/c.bmp") h = {true, 5, std::string(64, 'f')};  // the same size, another file
        return h;                                                      // d: missing
    };
    const std::vector<client_patch::File> need = client_patch::needed(m, have);
    std::vector<std::string> paths;
    for (const auto& f : need) paths.push_back(f.path);
    CHECK((paths == std::vector<std::string>{"data/b.bmp", "data/c.bmp", "data/d.bmp"}));
}

TEST(client_patch_record_round_trip) {
    client_patch::Manifest m;
    m.version = "p4";
    m.files = {{"data/texture/scr_logo.bmp", kSha, 5278}};
    const std::string rec = client_patch::record(m);
    const auto sizeOf = [](const std::string& path) -> long long {
        return path == "data/texture/scr_logo.bmp" ? 5278 : -1;
    };
    CHECK(client_patch::upToDate(m, rec, sizeOf));
    client_patch::Manifest newer = m;
    newer.version = "p5";
    CHECK(!client_patch::upToDate(newer, rec, sizeOf));  // another list: looked at again
    CHECK(!client_patch::upToDate(m, rec, [](const std::string&) -> long long { return -1; }));  // the file gone
    CHECK(!client_patch::upToDate(m, rec, [](const std::string&) -> long long { return 7; }));   // changed
    CHECK(!client_patch::upToDate(m, "", sizeOf));
}

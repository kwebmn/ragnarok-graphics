// 2.14: the client patcher in the app (app/client_patch) over the fake site: the check, the fetch, the files put.
// 3.0.1: each client its own list, the player's own files, withdrawn files taken away, three programs closed first.
#include <cwctype>
#include <string>

#include "app/client_patch.hpp"
#include "app/files.hpp"
#include "core/sha256.hpp"
#include "fake_windows.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

const std::string kLogo = "BM-new-logo";
const std::string kFlags = "GRF-flags";

const std::string kList = "/api/public/client-patch?client=uaRO";  // the old client's: no uaro-client.json

std::string listed(const std::string& path, const std::string& bytes) {
    return "{\"path\":\"" + path + "\",\"size\":" + std::to_string(bytes.size()) + ",\"sha256\":\"" +
           sha256::hexOf(bytes) + "\"}";
}

std::string manifest(const std::string& version, const std::string& client = "uaRO") {
    return "{\"format\":1,\"client\":\"" + client + "\",\"version\":\"" + version + "\",\"files\":[" +
           listed("data/texture/scr_logo.bmp", kLogo) + "," + listed("uaro-flags.grf", kFlags) + "]}";
}

std::wstring lower(std::wstring s) {
    for (wchar_t& c : s) c = wchar_t(std::towlower(c));
    return s;
}

fake::Windows& site() {
    fake::Windows& w = fake::reset();
    w.web[kList] = manifest("p1");
    w.web["/files/client/" + sha256::hexOf(kLogo)] = kLogo;
    w.web["/files/client/" + sha256::hexOf(kFlags)] = kFlags;
    return w;
}

bool apply(const std::wstring& dir, const app::PatchCheck& c, std::string& error) {
    return app::applyClientPatch(dir, c, [](uint64_t, uint64_t) { return true; }, error);
}

}  // namespace

TEST(the_patch_puts_the_missing_files_and_records_them) {
    fake::Windows& w = site();
    const std::wstring dir = app::exeFolder();
    w.files[fake::path(L"uaro-flags.grf")] = "old";  // another file there: replaced
    const app::PatchCheck c = app::checkClientPatch(dir);
    CHECK(c.asked);
    CHECK_EQ(c.needed.size(), size_t(2));
    CHECK_EQ(c.bytes, uint64_t(kLogo.size() + kFlags.size()));
    std::string error;
    CHECK(apply(dir, c, error));
    CHECK_EQ(w.files[fake::path(L"data\\texture\\scr_logo.bmp")], kLogo);  // the folders made on the way
    CHECK_EQ(w.files[fake::path(L"uaro-flags.grf")], kFlags);
    CHECK(w.files.count(fake::path(L"uaro-graphics\\client-patch.txt")) == 1);
    CHECK(w.folders.count(fake::path(L"uaro-graphics\\update\\client")) == 0);  // the staging gone
}

TEST(a_patch_put_is_not_fetched_again) {
    fake::Windows& w = site();
    const std::wstring dir = app::exeFolder();
    std::string error;
    CHECK(apply(dir, app::checkClientPatch(dir), error));
    w.web.erase("/files/client/" + sha256::hexOf(kLogo));  // nothing to fetch any more
    app::PatchCheck again = app::checkClientPatch(dir);
    CHECK(again.asked && again.needed.empty());
    w.files.erase(fake::path(L"data\\texture\\scr_logo.bmp"));  // the player removed it: brought again
    w.web["/files/client/" + sha256::hexOf(kLogo)] = kLogo;
    again = app::checkClientPatch(dir);
    CHECK(again.needed.size() == 1 && again.needed[0].path == "data/texture/scr_logo.bmp");
    w.web[kList] = manifest("p2");  // the same files under a new list: hashed, not fetched
    CHECK(apply(dir, again, error));
    again = app::checkClientPatch(dir);
    CHECK(again.needed.empty());
}

TEST(a_corrupted_download_is_not_put) {
    fake::Windows& w = site();
    const std::wstring dir = app::exeFolder();
    w.files[fake::path(L"uaro-flags.grf")] = "old";
    w.web["/files/client/" + sha256::hexOf(kFlags)] = "GRF-flagz";  // the same size, other bytes
    const app::PatchCheck c = app::checkClientPatch(dir);
    std::string error;
    CHECK(!apply(dir, c, error));
    CHECK(!error.empty());
    CHECK_EQ(w.files[fake::path(L"uaro-flags.grf")], std::string("old"));  // the old file stays
    CHECK(w.files.count(fake::path(L"data\\texture\\scr_logo.bmp")) == 0);  // nothing put before all arrived
    CHECK(w.files.count(fake::path(L"uaro-graphics\\client-patch.txt")) == 0);
}

TEST(no_site_no_patch_the_game_starts) {
    fake::Windows& w = fake::reset();
    const app::PatchCheck c = app::checkClientPatch(app::exeFolder());
    CHECK(!c.asked && c.needed.empty() && !c.error.empty());
    std::string error;
    CHECK(apply(app::exeFolder(), c, error));  // nothing to put: fine
    w.gameRunning = true;  // the game open: nothing is written
    fake::Windows& s = site();
    s.gameRunning = true;
    const app::PatchCheck c2 = app::checkClientPatch(app::exeFolder());
    CHECK(!apply(app::exeFolder(), c2, error));
    CHECK(s.files.count(fake::path(L"uaro-flags.grf")) == 0);
}

TEST(the_same_file_at_two_places_is_put_twice) {
    // final review: one picture for two folders - one download, both places
    fake::Windows& w = fake::reset();
    const std::string sha = sha256::hexOf(kLogo), size = std::to_string(kLogo.size());
    const auto at = [&](const std::string& path) {
        return "{\"path\":\"" + path + "\",\"size\":" + size + ",\"sha256\":\"" + sha + "\"}";
    };
    w.web[kList] =
        "{\"format\":1,\"version\":\"p9\",\"files\":[" + at("data/a/logo.bmp") + "," + at("data/b/logo.bmp") + "]}";
    w.web["/files/client/" + sha] = kLogo;
    const std::wstring dir = app::exeFolder();
    std::string error;
    CHECK(apply(dir, app::checkClientPatch(dir), error));
    CHECK_EQ(w.files[fake::path(L"data\\a\\logo.bmp")], kLogo);
    CHECK_EQ(w.files[fake::path(L"data\\b\\logo.bmp")], kLogo);
}

// 3.0.1: «uaRO HD» stands next to the old client - its uaro-client.json names it, and the site's list is its own
TEST(the_patch_asks_for_its_clients_list) {
    fake::Windows& w = fake::reset();
    w.files[fake::path(L"uaro-client.json")] =
        "{\"format\":1,\"client\":\"uaRO HD\",\"version\":\"2026.10.6\",\"files\":[" + listed("data.grf", "GRF") + "]}";
    const std::string hd = "/api/public/client-patch?client=uaRO%20HD";
    w.web[hd] = manifest("p1", "uaRO HD");
    w.web[kList] = manifest("p1");  // the old client's list: never asked here
    CHECK_EQ(app::clientOf(app::exeFolder()).client, std::string("uaRO HD"));
    app::PatchCheck c = app::checkClientPatch(app::exeFolder());
    CHECK(c.asked && c.needed.size() == 2);
    w.web[hd] = manifest("p1");  // a list for another client: refused, nothing put
    c = app::checkClientPatch(app::exeFolder());
    CHECK(!c.asked && c.needed.empty() && !c.error.empty());
    w.files.erase(fake::path(L"uaro-client.json"));  // no file: the old client
    CHECK_EQ(app::clientOf(app::exeFolder()).client, std::string("uaRO"));
    w.files[fake::path(L"uaro-client.json")] = "{\"format\":1,\"client\":\"a/b\",\"files\":[]}";  // no fit name
    CHECK_EQ(app::clientOf(app::exeFolder()).client, std::string("uaRO"));
}

// 3.0.1: a file the admin withdrew goes when the patch made it; one the patch put over the client's own stays
TEST(the_patch_takes_away_what_it_made_and_was_withdrawn) {
    fake::Windows& w = site();
    const std::wstring dir = app::exeFolder();
    w.files[fake::path(L"uaro-flags.grf")] = "old";  // the client's own: the patch puts its file over it
    std::string error;
    CHECK(apply(dir, app::checkClientPatch(dir), error));
    w.web[kList] =
        "{\"format\":1,\"client\":\"uaRO\",\"version\":\"p2\",\"files\":[" + listed("uaro-flags.grf", kFlags) + "]}";
    app::PatchCheck c = app::checkClientPatch(dir);
    CHECK(c.asked && c.needed.empty() && c.remove.size() == 1 && c.remove[0].path == "data/texture/scr_logo.bmp");
    CHECK(c.work());
    CHECK(apply(dir, c, error));
    CHECK(w.files.count(fake::path(L"data\\texture\\scr_logo.bmp")) == 0);
    w.web[kList] = "{\"format\":1,\"client\":\"uaRO\",\"version\":\"p3\",\"files\":[]}";
    c = app::checkClientPatch(dir);
    CHECK(c.asked && c.remove.empty() && !c.work());  // put over the client's file: it stays
    CHECK_EQ(w.files[fake::path(L"uaro-flags.grf")], kFlags);
}

// 3.0.1: the player's own Palettes.grf (the app's «Палітри гри» made it) is not the list's - it stays, nothing comes
TEST(the_players_palettes_stay) {
    fake::Windows& w = fake::reset();
    const std::string official(101, 'P');
    w.web[kList] =
        "{\"format\":1,\"client\":\"uaRO\",\"version\":\"p1\",\"files\":[" + listed("Palettes.grf", official) + "]}";
    w.files[fake::path(L"Palettes.grf")] = "my own 4 MB one";
    const std::wstring dir = app::exeFolder();
    const app::PatchCheck c = app::checkClientPatch(dir);
    CHECK(c.asked && c.needed.empty() && c.kept.size() == 1 && c.bytes == 0 && !c.work());
    CHECK_EQ(w.files[fake::path(L"Palettes.grf")], std::string("my own 4 MB one"));
}

// 3.0.1: the other client's game (its own folder) does not hold this one's files; this folder's Studio does
TEST(another_folders_game_does_not_stop_the_patch) {
    fake::Windows& w = site();
    const std::wstring dir = app::exeFolder();
    w.running.insert(lower(L"C:\\Games\\uaRO-HD\\uaRO.exe"));
    std::string error;
    CHECK(apply(dir, app::checkClientPatch(dir), error));
    CHECK_EQ(w.files[fake::path(L"uaro-flags.grf")], kFlags);
    for (const wchar_t* program : {L"uaro-graphics\\uaro-studio.exe", L"uaro-graphics\\uaro-render64.exe"}) {
        fake::Windows& again = site();
        again.running.insert(lower(fake::path(program)));
        error.clear();
        CHECK(!apply(dir, app::checkClientPatch(dir), error));
        CHECK(!error.empty());
        CHECK(again.files.count(fake::path(L"uaro-flags.grf")) == 0);
        CHECK(again.files.count(fake::path(L"uaro-graphics\\client-patch.txt")) == 0);
    }
}

// 3.0.1: a list the folder has already (the build brought the same files): recorded by the check - not hashed again
TEST(a_list_with_nothing_to_do_is_recorded) {
    fake::Windows& w = site();
    w.folders.insert(fake::path(L"uaro-graphics"));  // an installed uaRO Graphics
    w.files[fake::path(L"data\\texture\\scr_logo.bmp")] = kLogo;
    w.files[fake::path(L"uaro-flags.grf")] = kFlags;
    const app::PatchCheck c = app::checkClientPatch(app::exeFolder());
    CHECK(c.asked && !c.work());
    CHECK(w.files.count(fake::path(L"uaro-graphics\\client-patch.txt")) == 1);
    CHECK(w.files[fake::path(L"uaro-graphics\\client-patch.txt")].find("over data/texture/scr_logo.bmp") !=
          std::string::npos);  // the build's file, not the patch's: never taken away
}

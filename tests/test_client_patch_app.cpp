// 2.14: the client patcher in the app (app/client_patch) over the fake site: the check, the fetch, the files put.
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

std::string manifest(const std::string& version) {
    return "{\"format\":1,\"client\":\"uaRO\",\"version\":\"" + version + "\",\"files\":["
           "{\"path\":\"data/texture/scr_logo.bmp\",\"size\":" + std::to_string(kLogo.size()) + ",\"sha256\":\"" +
           sha256::hexOf(kLogo) + "\"},{\"path\":\"uaro-flags.grf\",\"size\":" + std::to_string(kFlags.size()) +
           ",\"sha256\":\"" + sha256::hexOf(kFlags) + "\"}]}";
}

fake::Windows& site() {
    fake::Windows& w = fake::reset();
    w.web["/api/public/client-patch"] = manifest("p1");
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
    w.web["/api/public/client-patch"] = manifest("p2");  // the same files under a new list: hashed, not fetched
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
    w.web["/api/public/client-patch"] =
        "{\"format\":1,\"version\":\"p9\",\"files\":[" + at("data/a/logo.bmp") + "," + at("data/b/logo.bmp") + "]}";
    w.web["/files/client/" + sha] = kLogo;
    const std::wstring dir = app::exeFolder();
    std::string error;
    CHECK(apply(dir, app::checkClientPatch(dir), error));
    CHECK_EQ(w.files[fake::path(L"data\\a\\logo.bmp")], kLogo);
    CHECK_EQ(w.files[fake::path(L"data\\b\\logo.bmp")], kLogo);
}

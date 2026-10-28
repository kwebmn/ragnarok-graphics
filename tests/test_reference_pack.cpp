// 3.0.11-beta.5: the game's reference pack - the site's manifest read and kept, the pack fetched only when the site
// has another one, checked by its SHA-256, the copy there kept while the site is down (tests/fake_windows: web, files).
#include <string>

#include "app/files.hpp"
#include "app/reference_pack.hpp"
#include "core/reference_pack.hpp"
#include "core/sha256.hpp"
#include "fake_windows.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

// What uaro.kiev.ua answered on 2026-10-29 after its first import (the deploy's check)
const std::string kLive =
    R"({"v":1,"data":1793227520,"commit":"d68ea17ea5b8","date":"2026-10-23T19:20:17+03:00","size":270164,)"
    R"("sha256":"f3a179a8e0abab4a693017394b930ea1d3eb78bc257257c3f4b270977c971820",)"
    R"("url":"https://uaro.kiev.ua/api/public/game/pack-1793227520.gz"})";

const std::wstring kFolder = L"uaro-graphics\\reference\\";

std::string manifestFor(long long data, const std::string& bytes, const std::string& sha = "") {
    return R"({"v":1,"data":)" + std::to_string(data) +
           R"(,"commit":"d68ea17ea5b8","date":"2026-10-23T19:20:17+03:00","size":)" + std::to_string(bytes.size()) +
           R"(,"sha256":")" + (sha.empty() ? sha256::hexOf(bytes) : sha) +
           R"(","url":"https://elsewhere.example/pack.gz"})";  // never followed
}

void site(fake::Windows& w, long long data, const std::string& bytes) {
    w.web["/api/public/game/pack.json"] = manifestFor(data, bytes);
    w.web["/api/public/game/pack-" + std::to_string(data) + ".gz"] = bytes;
}

std::string packThere(fake::Windows& w, long long data) {
    const auto it = w.files.find(fake::path(kFolder + L"pack-" + std::to_wstring(data) + L".json.gz"));
    return it == w.files.end() ? std::string("(none)") : it->second;
}

}  // namespace

TEST(reference_manifest_of_the_site_is_read) {
    refpack::Manifest m;
    CHECK(refpack::parse(kLive, m));
    CHECK_EQ(m.format, 1);
    CHECK_EQ(m.data, 1793227520LL);
    CHECK_EQ(m.commit, std::string("d68ea17ea5b8"));
    CHECK_EQ(m.size, uint64_t(270164));
    CHECK_EQ(m.sha256, std::string("f3a179a8e0abab4a693017394b930ea1d3eb78bc257257c3f4b270977c971820"));
    CHECK_EQ(refpack::dateOf(m), std::string("2026-10-23"));
    CHECK_EQ(refpack::fileName(m), std::string("pack-1793227520.json.gz"));
    CHECK_EQ(refpack::sitePath(m), std::string("/api/public/game/pack-1793227520.gz"));  // from the stamp, not "url"
    refpack::Manifest back;
    CHECK(refpack::parse(refpack::write(m), back));  // what pack.json keeps reads back the same
    CHECK(back.data == m.data && back.size == m.size && back.sha256 == m.sha256 && back.commit == m.commit &&
          back.date == m.date && back.format == 1);
}

TEST(reference_manifest_refuses_what_it_does_not_read) {
    refpack::Manifest m;
    const std::string bytes = "PACK";
    CHECK(refpack::parse(manifestFor(5, bytes), m));
    std::string other = manifestFor(5, bytes);
    other.replace(other.find("\"v\":1"), 5, "\"v\":2");
    CHECK(!refpack::parse(other, m));  // another format
    CHECK(!refpack::parse(manifestFor(0, bytes), m));  // no stamp
    CHECK(!refpack::parse(manifestFor(5, ""), m));  // nothing to fetch
    CHECK(!refpack::parse(manifestFor(5, bytes, std::string(64, 'A')), m));  // not lower-case hex
    CHECK(!refpack::parse(manifestFor(5, bytes, "abc"), m));
    std::string huge = manifestFor(5, bytes);
    huge.replace(huge.find("\"size\":4"), 8, "\"size\":99999999999");
    CHECK(!refpack::parse(huge, m));  // past kMostSize
    CHECK(!refpack::parse("<html>502 Bad Gateway</html>", m));
}

TEST(reference_needs_another_or_a_missing_pack) {
    refpack::Manifest a;
    CHECK(refpack::parse(manifestFor(10, "AAAA"), a));
    refpack::Manifest b;
    CHECK(refpack::parse(manifestFor(11, "BBBB"), b));
    CHECK(refpack::needs(a, std::nullopt, -1));      // none there
    CHECK(!refpack::needs(a, a, 4));                 // the same, whole
    CHECK(refpack::needs(a, a, -1));                 // its pack gone
    CHECK(refpack::needs(a, a, 3));                  // cut short
    CHECK(refpack::needs(b, a, 4));                  // the site has another
    refpack::Manifest sameStamp = a;
    sameStamp.sha256 = sha256::hexOf("CCCC");
    CHECK(refpack::needs(sameStamp, a, 4));          // the same stamp, other bytes
}

TEST(reference_verify_checks_size_and_sha256) {
    refpack::Manifest m;
    CHECK(refpack::parse(manifestFor(7, "PACK-BYTES"), m));
    CHECK(refpack::verify("PACK-BYTES", m));
    CHECK(!refpack::verify("PACK-BYTEZ", m));
    CHECK(!refpack::verify("PACK-BYTE", m));
}

TEST(reference_pack_is_fetched_once_and_kept) {
    fake::Windows& w = fake::reset();
    const std::wstring dir = app::exeFolder();
    CHECK(!app::referenceThere(dir));  // nothing yet: «Інтерфейс» says it comes with the game
    site(w, 100, "PACK-A");
    std::string error;
    CHECK(app::noteReference(dir, error));
    CHECK_EQ(packThere(w, 100), std::string("PACK-A"));
    const auto there = app::referenceThere(dir);
    CHECK(there && there->data == 100 && there->size == 6);
    w.web["/api/public/game/pack-100.gz"] = "GARBAG";  // the same manifest: the pack is not fetched again
    CHECK(app::noteReference(dir, error));
    CHECK_EQ(packThere(w, 100), std::string("PACK-A"));
}

TEST(reference_pack_is_replaced_by_a_newer_one) {
    fake::Windows& w = fake::reset();
    const std::wstring dir = app::exeFolder();
    std::string error;
    site(w, 100, "PACK-A");
    CHECK(app::noteReference(dir, error));
    site(w, 200, "PACK-B");
    CHECK(app::noteReference(dir, error));
    CHECK_EQ(packThere(w, 200), std::string("PACK-B"));
    CHECK_EQ(packThere(w, 100), std::string("(none)"));  // the older one goes
    const auto there = app::referenceThere(dir);
    CHECK(there && there->data == 200);
}

TEST(reference_pack_of_other_bytes_is_not_kept) {
    fake::Windows& w = fake::reset();
    const std::wstring dir = app::exeFolder();
    std::string error;
    site(w, 100, "PACK-A");
    CHECK(app::noteReference(dir, error));
    w.web["/api/public/game/pack.json"] = manifestFor(300, "PACK-C");
    w.web["/api/public/game/pack-300.gz"] = "PACK-X";  // not what the manifest says
    CHECK(!app::noteReference(dir, error));
    CHECK(error.find("SHA-256") != std::string::npos);
    CHECK_EQ(packThere(w, 300), std::string("(none)"));
    const auto there = app::referenceThere(dir);  // the one before stays
    CHECK(there && there->data == 100);
    CHECK_EQ(packThere(w, 100), std::string("PACK-A"));
}

TEST(reference_copy_stays_while_the_site_is_down) {
    fake::Windows& w = fake::reset();
    const std::wstring dir = app::exeFolder();
    std::string error;
    site(w, 100, "PACK-A");
    CHECK(app::noteReference(dir, error));
    w.web.clear();  // uaro.kiev.ua down
    CHECK(!app::noteReference(dir, error));
    CHECK(!error.empty());
    const auto there = app::referenceThere(dir);
    CHECK(there && there->data == 100);
    w.web["/api/public/game/pack.json"] = "<html>502</html>";  // a proxy's page instead of the manifest
    CHECK(!app::noteReference(dir, error));
    CHECK(app::referenceThere(dir).has_value());
}

TEST(reference_pack_comes_back_when_its_file_is_gone) {
    fake::Windows& w = fake::reset();
    const std::wstring dir = app::exeFolder();
    std::string error;
    site(w, 100, "PACK-A");
    CHECK(app::noteReference(dir, error));
    w.files.erase(fake::path(kFolder + L"pack-100.json.gz"));
    CHECK(!app::referenceThere(dir));
    CHECK(app::noteReference(dir, error));
    CHECK_EQ(packThere(w, 100), std::string("PACK-A"));
}

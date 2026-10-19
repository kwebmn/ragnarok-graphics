#include <unistd.h>

#include <cstdio>
#include <string>
#include <vector>

#include "core/grf/grf.hpp"
#include "core/palette_grf.hpp"
#include "test.hpp"

using namespace uagfx;
using palettes::Left;

namespace {

const std::string kHair = "\xB8\xD3\xB8\xAE";  // 머리
const std::string kBody = "\xB8\xF6";          // 몸
const std::string kMale = "\xB3\xB2";          // 남
const std::string kFemale = "\xBF\xA9";        // 여
const std::string kSword = "\xB0\xCB\xBB\xE7";  // 검사

std::string hair(int style, int colour) {
    return "data\\palette\\" + kHair + "\\" + kHair + std::to_string(style) + "_" + kMale + "_" +
           std::to_string(colour) + ".pal";
}
std::string clothes(const std::string& job, int colour) {
    return "data\\palette\\" + kBody + "\\" + job + "_" + kFemale + "_" + std::to_string(colour) + ".pal";
}

std::vector<uint8_t> palette(uint8_t seed) {
    std::vector<uint8_t> p(1024);
    for (size_t i = 0; i < p.size(); ++i) p[i] = uint8_t(seed + i * 3);
    return p;
}

}  // namespace

TEST(palettes_client_names_from_every_source) {
    const std::string want = "data\\palette\\" + kHair + "\\" + kHair + "2_" + kMale + "_5.pal";
    CHECK_EQ(palettes::clientName("palette/" + kHair + "/" + kHair + "2_" + kMale + "_5.pal"), want);  // a zip's
    CHECK_EQ(palettes::clientName(want), want);                                                         // a GRF's
    CHECK_EQ(palettes::clientName("D:\\ro\\Palette\\" + kHair + "\\" + kHair + "2_" + kMale + "_5.pal"), want);
    CHECK_EQ(palettes::clientName("data\\sprite\\a.spr"), std::string());
    CHECK_EQ(palettes::clientName("palette"), std::string());
}

TEST(palettes_keep_what_the_server_lets_be_worn) {
    CHECK(!palettes::verdict(hair(23, 20)));
    CHECK(!palettes::verdict(hair(0, 0)));
    CHECK(palettes::verdict(hair(24, 1)) == Left::HairPast);
    // 2.5.24: every hair colour - the server clamps at its max_hair_color (8 now), raised there the client has them
    CHECK(!palettes::verdict(hair(5, 21)));
    CHECK(!palettes::verdict(hair(5, 127)));
    CHECK(!palettes::verdict(clothes(kSword, 77)));
    CHECK(palettes::verdict(clothes(kSword, 78)) == Left::ClothesPast);
    CHECK(!palettes::verdict(clothes("meister_riding", 3)));  // a job's name with '_' in it
    // Kamishi's costumes and the Doram race: unknown to the 2007 client
    CHECK(palettes::verdict("data\\palette\\" + kBody + "\\costume_1\\" + kSword + "_" + kMale + "_1.pal") ==
          Left::Unknown);
    CHECK(palettes::verdict("data\\palette\\\xB5\xB5\xB6\xF7\xC1\xB7\\body\\x_" + kMale + "_1.pal") == Left::Unknown);
    // names it never asks for: copies and slips of the pack
    CHECK(palettes::verdict("data\\palette\\" + kBody + "\\" + kSword + "_" + kFemale + "_.pal") == Left::OddName);
    CHECK(palettes::verdict("data\\palette\\" + kBody + "\\" + kSword + "_" + kFemale + "_ocument.pal") ==
          Left::OddName);
    CHECK(palettes::verdict("data\\palette\\" + kBody + "\\" + kSword + "_" + kFemale + "_3 (1).pal") ==
          Left::OddName);
    CHECK(palettes::verdict("data\\palette\\x.pal") == Left::OddName);
    CHECK(palettes::verdict("data\\palette\\readme.txt") == Left::NotPalette);
}

TEST(palettes_grf_holds_each_palette_once_and_reads_back) {
    palettes::Collector c;
    const std::vector<uint8_t> same = palette(1), other = palette(9);
    for (const std::string& n : {hair(1, 1), hair(2, 1)}) {  // the same colours under two names
        CHECK(c.wants(n));
        c.take(n, same);
    }
    CHECK(c.wants(clothes(kSword, 4)));
    c.take(clothes(kSword, 4), other);
    CHECK(!c.wants(hair(1, 1)));                    // a later source's: the first stays
    CHECK(!c.wants(hair(30, 1)));                   // past the styles
    CHECK(c.wants(clothes(kSword, 5)));
    c.take(clothes(kSword, 5), {});                 // an empty file
    CHECK(!c.wants("palette/127_Hairs_Human.png"));  // the pack's own picture
    CHECK_EQ(c.kept(), size_t(3));
    CHECK_EQ(c.hair(), size_t(2));
    CHECK_EQ(c.found(), 5u);
    CHECK_EQ(c.left()[size_t(Left::HairPast)], 1u);
    CHECK_EQ(c.left()[size_t(Left::Empty)], 1u);
    CHECK_EQ(c.left()[size_t(Left::NotPalette)], 1u);
    uint32_t blocks = 0;
    const std::vector<uint8_t> grf = c.grf(blocks);
    CHECK_EQ(blocks, 2u);
    char path[] = "/tmp/uagfx-palettes-XXXXXX";
    const int fd = mkstemp(path);
    CHECK(fd >= 0);
    if (fd < 0) return;
    CHECK(write(fd, grf.data(), grf.size()) == ssize_t(grf.size()));
    close(fd);
    grf::Archive a;
    std::string error;
    CHECK(a.openToRead(path, error));
    CHECK_EQ(a.names().size(), size_t(3));
    std::vector<uint8_t> back;
    CHECK(a.read(hair(2, 1), back, error) && back == same);
    CHECK(a.read(clothes(kSword, 4), back, error) && back == other);
    const grf::Entry* first = a.find(hair(1, 1));
    const grf::Entry* second = a.find(hair(2, 1));
    CHECK(first && second && first->offset == second->offset);  // one data for both
    CHECK(first && first->aligned % 8 == 0 && first->aligned >= first->compressed);
    std::remove(path);
}

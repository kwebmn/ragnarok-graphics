// 3.0.22-beta.42 (the owner's R-C8SA9Z: std::bad_alloc after two graphics tests, «no room» for the own ground): the
// game's address space told every 30 s - its free room, the largest free piece, what is committed
#include "core/address_room.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(address_room_sums_the_free_and_finds_the_largest) {
    constexpr uint64_t MB = 1u << 20;
    const std::vector<addressroom::Region> r = {
        {0x10000, 64 * MB, addressroom::Kind::Free},       {0x10000 + 64 * MB, 200 * MB, addressroom::Kind::Committed},
        {0x10000 + 264 * MB, 10 * MB, addressroom::Kind::Reserved}, {0x10000 + 274 * MB, 300 * MB, addressroom::Kind::Free},
    };
    const addressroom::Room room = addressroom::sum(r);
    CHECK_EQ(room.freeMb, 364u);
    CHECK_EQ(room.largestFreeMb, 300u);
    CHECK_EQ(room.committedMb, 200u);
    CHECK_EQ(room.reservedMb, 10u);
    CHECK_EQ(addressroom::sum({}).freeMb, 0u);
}

// 3.0.22-beta.43 (the owner: «делать» - who holds the room): the committed and reserved room by its kind - the
// modules (by file), the files mapped in (by file), the private (heaps, VirtualAlloc) - the largest owners first
TEST(address_room_tells_the_owners) {
    constexpr uint64_t MB = 1u << 20;
    using addressroom::Kind;
    using addressroom::Use;
    std::vector<addressroom::Region> r = {
        {0x100000, 300 * MB, Kind::Committed, Use::Mapped, "data.grf"},
        {0x200000, 100 * MB, Kind::Reserved, Use::Mapped, "data.grf"},
        {0x300000, 50 * MB, Kind::Committed, Use::Mapped, "uaro-textures.zip"},
        {0x400000, 20 * MB, Kind::Committed, Use::Image, "DDRAW.dll"},
        {0x500000, 600 * MB, Kind::Committed, Use::Private, ""},
        {0x600000, 64 * MB, Kind::Free, Use::Private, ""},
    };
    const addressroom::Room room = addressroom::sum(r);
    CHECK_EQ(room.mappedMb, 450u);
    CHECK_EQ(room.imageMb, 20u);
    CHECK_EQ(room.privateMb, 600u);
    CHECK_EQ(room.owners.size(), size_t(3));
    CHECK(room.owners[0].name == "data.grf" && room.owners[0].mb == 400u);  // its views together
    CHECK(room.owners[1].name == "uaro-textures.zip");
    CHECK_EQ(addressroom::ownersLine(room, 2), std::string("data.grf 400, uaro-textures.zip 50"));
}

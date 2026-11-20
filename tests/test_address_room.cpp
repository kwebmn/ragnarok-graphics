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

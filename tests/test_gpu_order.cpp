#include "core/gpu_order.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(gpu_order_takes_the_fast_card) {
    // 0.50.1: a laptop - the integrated Intel drives the screen and comes first; the NVIDIA one is the fast one.
    const std::vector<AdapterFacts> laptop = {{"Intel(R) UHD Graphics", 0x8086, 128, false},
                                              {"NVIDIA GeForce RTX 3060 Laptop GPU", 0x10de, 6144, false},
                                              {"Microsoft Basic Render Driver", 0x1414, 0, true}};
    CHECK(adapterOrder(laptop, false) == (std::vector<size_t>{1, 0}));  // by video memory, software left out
    // Windows' own order for high performance (and the player's choice in its graphics settings): as it is.
    CHECK(adapterOrder(laptop, true) == (std::vector<size_t>{0, 1}));
    const std::vector<AdapterFacts> same = {{"a", 0x1002, 8192, false}, {"b", 0x1002, 8192, false}};
    CHECK(adapterOrder(same, false) == (std::vector<size_t>{0, 1}));  // a tie: Windows' order
    CHECK(adapterOrder({}, false).empty());
    CHECK(adaptersText(laptop) == "Intel(R) UHD Graphics (128 MB), NVIDIA GeForce RTX 3060 Laptop GPU (6144 MB), "
                                  "Microsoft Basic Render Driver (0 MB, software)");
}

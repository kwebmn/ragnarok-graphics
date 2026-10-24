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
    // 0.50.2: the player's card ([renderer] gpu) first, whatever the order; a name no card has changes nothing.
    CHECK(adapterOrder(laptop, false, "intel(r) uhd graphics") == (std::vector<size_t>{0, 1}));  // any case
    CHECK(adapterOrder(laptop, true, "NVIDIA GeForce RTX 3060 Laptop GPU") == (std::vector<size_t>{1, 0}));
    CHECK(adapterOrder(laptop, false, "a card long gone") == (std::vector<size_t>{1, 0}));
    CHECK(adapterOrder(laptop, false, "Microsoft Basic Render Driver") == (std::vector<size_t>{1, 0}));
    CHECK(adaptersText(laptop) == "Intel(R) UHD Graphics (128 MB), NVIDIA GeForce RTX 3060 Laptop GPU (6144 MB), "
                                  "Microsoft Basic Render Driver (0 MB, software)");
}

TEST(gpu_row_shows_the_card_even_alone) {
    // 3.0.4-beta.2 (the user): the Renderer's graphics card row seen with a single card too - which card draws the
    // game; a list to choose from with several cards, or when one was chosen by name
    CHECK(!gpuRow(0, false).shown);  // none found: nothing to tell
    CHECK(gpuRow(1, false).shown && !gpuRow(1, false).choosable);
    CHECK(gpuRow(2, false).shown && gpuRow(2, false).choosable);
    CHECK(gpuRow(1, true).shown && gpuRow(1, true).choosable);  // chosen by name: back to Automatic
    CHECK(gpuRow(0, true).shown && gpuRow(0, true).choosable);  // a chosen card not found
}

#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_39_3) { CHECK(std::strcmp(uagfx::kVersion, "0.39.3") == 0); }

#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_25_3) { CHECK(std::strcmp(uagfx::kVersion, "0.25.3") == 0); }

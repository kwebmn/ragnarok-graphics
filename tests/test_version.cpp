#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_1_8_4) { CHECK(std::strcmp(uagfx::kVersion, "1.8.4") == 0); }

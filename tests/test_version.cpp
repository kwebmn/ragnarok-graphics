#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_2_8_0) { CHECK(std::strcmp(uagfx::kVersion, "2.8.0") == 0); }

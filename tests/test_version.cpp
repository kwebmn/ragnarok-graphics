#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_2_7_33) { CHECK(std::strcmp(uagfx::kVersion, "2.7.33") == 0); }

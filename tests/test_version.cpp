#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_2_5_20) { CHECK(std::strcmp(uagfx::kVersion, "2.5.20") == 0); }

#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_2_5_1) { CHECK(std::strcmp(uagfx::kVersion, "2.5.1") == 0); }

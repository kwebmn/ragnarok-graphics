#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_2_5_8) { CHECK(std::strcmp(uagfx::kVersion, "2.5.8") == 0); }

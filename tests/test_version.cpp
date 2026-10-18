#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_2_5_19) { CHECK(std::strcmp(uagfx::kVersion, "2.5.19") == 0); }

#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_2_7_19) { CHECK(std::strcmp(uagfx::kVersion, "2.7.19") == 0); }

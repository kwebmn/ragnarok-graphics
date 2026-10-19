#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_2_6_6) { CHECK(std::strcmp(uagfx::kVersion, "2.6.6") == 0); }

#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_1_6_6) { CHECK(std::strcmp(uagfx::kVersion, "1.6.6") == 0); }

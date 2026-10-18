#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_2_4_1) { CHECK(std::strcmp(uagfx::kVersion, "2.4.1") == 0); }

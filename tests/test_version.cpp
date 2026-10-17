#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_2_2_1) { CHECK(std::strcmp(uagfx::kVersion, "2.2.1") == 0); }

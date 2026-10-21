#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_2_7_32) { CHECK(std::strcmp(uagfx::kVersion, "2.7.32") == 0); }

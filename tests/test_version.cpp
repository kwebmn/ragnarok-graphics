#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_2_6_2) { CHECK(std::strcmp(uagfx::kVersion, "2.6.2") == 0); }

#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_6_2) { CHECK(std::strcmp(uagfx::kVersion, "0.6.2") == 0); }
